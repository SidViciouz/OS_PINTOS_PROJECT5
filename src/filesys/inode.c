#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT 123
#define INDIRECT 128


struct indirect_block {
	block_sector_t pointers[INDIRECT];
};

static bool allocate_block(block_sector_t* p_entry);
static bool inode_reserve(struct inode_disk *disk_inode, off_t length);
static bool inode_reserve_doubly_indirect(block_sector_t* p_entry, size_t num_sectors);
static bool inode_reserve_indirect(block_sector_t* p_entry, size_t num_sectors);
static void inode_deallocate_doubly_indirect(block_sector_t entry, size_t num_sectors);
static void inode_deallocate_indirect(block_sector_t entry, size_t num_sectors);
static bool inode_deallocate(struct inode *inode);

static block_sector_t
get_sector_number(const struct inode_disk *idisk, off_t n)
{
	block_sector_t ret;
	struct indirect_block *temp_block;

	if (n < DIRECT){
		return idisk->direct_blocks[n];
	}
	else if (n < DIRECT + INDIRECT) {
		temp_block = calloc(1, sizeof(struct indirect_block));
		buffer_cache_read(idisk->indirect_block, temp_block);
		ret = temp_block->pointers[n - DIRECT];
		free(temp_block);

		return ret;
	}
	else if (n < DIRECT + INDIRECT + INDIRECT*INDIRECT) {
		off_t first_level_block = (n - DIRECT - INDIRECT) / INDIRECT;
		off_t second_level_block = (n - DIRECT - INDIRECT) % INDIRECT;

		temp_block = calloc(1, sizeof(struct indirect_block));

		buffer_cache_read(idisk->doubly_indirect_block, temp_block);
		buffer_cache_read(temp_block->pointers[first_level_block], temp_block);
		ret = temp_block->pointers[second_level_block];

		free(temp_block);
		return ret;
	}

	return -1;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector(const struct inode *inode, off_t pos)
{
	ASSERT(inode != NULL);
	if (0 <= pos && pos < inode->data.length) {
		off_t n = pos / BLOCK_SECTOR_SIZE;
		return get_sector_number(&inode->data, n);
	}
	else
		return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init(void)
{
	list_init(&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create(block_sector_t sector, off_t length, bool is_dir)
{
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT(length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	   one sector in size, and you should fix that. */
	ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

	disk_inode = calloc(1, sizeof *disk_inode);
	if (disk_inode != NULL)
	{
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		disk_inode->is_dir = is_dir;
		if (inode_reserve(disk_inode,disk_inode->length))
		{
			buffer_cache_write(sector, disk_inode);
			success = true;
		}
		free(disk_inode);
	}
	return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
	inode_open(block_sector_t sector)
{
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
		e = list_next(e))
	{
		inode = list_entry(e, struct inode, elem);
		if (inode->sector == sector)
		{
			inode_reopen(inode);
			return inode;
		}
	}

	/* Allocate memory. */
	inode = malloc(sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front(&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;

	buffer_cache_read(inode->sector, &inode->data);
	return inode;
}

/* Reopens and returns INODE. */
struct inode *
	inode_reopen(struct inode *inode)
{
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber(const struct inode *inode)
{
	return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close(struct inode *inode)
{
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0)
	{
		/* Remove from inode list and release lock. */
		list_remove(&inode->elem);

		/* Deallocate blocks if removed. */
		if (inode->removed)
		{
			free_map_release(inode->sector, 1);
			inode_deallocate(inode);
		}

		free(inode);
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove(struct inode *inode)
{
	ASSERT(inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

	while (size > 0)
	{
		/* Disk sector to read, starting byte offset within sector. */
		block_sector_t sector_idx = byte_to_sector(inode, offset);
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length(inode) - offset;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
		{
			/* Read full sector directly into caller's buffer. */
			buffer_cache_read(sector_idx, buffer + bytes_read);
		}
		else
		{
			/* Read sector into bounce buffer, then partially copy
			   into caller's buffer. */
			if (bounce == NULL)
			{
				bounce = malloc(BLOCK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			buffer_cache_read(sector_idx, bounce);
			memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free(bounce);

	return bytes_read;
}

off_t
inode_write_at(struct inode *inode, const void *buffer_, off_t size,
	off_t offset)
{
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	if (inode->deny_write_cnt)
		return 0;

	// beyond the EOF: extend the file
	if (byte_to_sector(inode, offset + size - 1) == -1) {
		// extend and reserve up to [offset + size] bytes
		if(!inode_reserve(&inode->data, offset + size)) return 0;
		// write back the (extended) file size
		inode->data.length = offset + size;
		buffer_cache_write(inode->sector, &inode->data);
	}

	while (size > 0)
	{
		/* Sector to write, starting byte offset within sector. */
		block_sector_t sector_idx = byte_to_sector(inode, offset);
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length(inode) - offset;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
		{
			/* Write full sector directly to disk. */
			buffer_cache_write(sector_idx, buffer + bytes_written);
		}
		else
		{
			/* We need a bounce buffer. */
			if (bounce == NULL)
			{
				bounce = malloc(BLOCK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left)
				buffer_cache_read(sector_idx, bounce);
			else
				memset(bounce, 0, BLOCK_SECTOR_SIZE);
			memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
			buffer_cache_write(sector_idx, bounce);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}
	free(bounce);

	return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write(struct inode *inode)
{
	inode->deny_write_cnt++;
	ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write(struct inode *inode)
{
	ASSERT(inode->deny_write_cnt > 0);
	ASSERT(inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length(const struct inode *inode)
{
	return inode->data.length;
}

/* Returns whether the file is directory or not. */
bool
inode_dir(const struct inode *inode)
{
	return inode->data.is_dir;
}
static char zeros[BLOCK_SECTOR_SIZE];
static bool allocate_block(block_sector_t* p_entry){

	if(*p_entry == 0){
		if(!free_map_allocate(1,p_entry))
			return false;
		buffer_cache_write(*p_entry,zeros);
	}
	return true;
}

static bool
inode_reserve_doubly_indirect(block_sector_t* p_entry, size_t num_sectors)
{
	struct indirect_block indirect_block;
	if (*p_entry == 0) {
		free_map_allocate(1, p_entry);
		buffer_cache_write(*p_entry, zeros);
	}
	buffer_cache_read(*p_entry, &indirect_block);

	size_t l = DIV_ROUND_UP(num_sectors, INDIRECT);

	for (size_t i = 0; i < l; i++) {
		size_t subsize = num_sectors < INDIRECT ? num_sectors : INDIRECT;
		if (!inode_reserve_indirect(&indirect_block.pointers[i], subsize))
			return false;
		num_sectors -= subsize;
	}

	buffer_cache_write(*p_entry, &indirect_block);
	return true;
}

static bool
inode_reserve_indirect(block_sector_t* p_entry, size_t num_sectors)
{
	struct indirect_block indirect_block;
	if (*p_entry == 0) {
		free_map_allocate(1, p_entry);
		buffer_cache_write(*p_entry, zeros);
	}
	buffer_cache_read(*p_entry, &indirect_block);

	for(size_t i=0; i<num_sectors; i++){
		if(!allocate_block(&indirect_block.pointers[i]))
				return false;
	}
	buffer_cache_write(*p_entry, &indirect_block);
	return true;
}

static bool
inode_reserve(struct inode_disk *disk_inode, off_t length)
{
	if (length < 0) return false;

	// (remaining) number of sectors, occupied by this file.
	size_t num_sectors = DIV_ROUND_UP(length,BLOCK_SECTOR_SIZE);
	size_t i, l;

	// (1) direct blocks
	l = num_sectors < DIRECT ? num_sectors : DIRECT;
	for (i = 0; i < l; ++i) {
		if (disk_inode->direct_blocks[i] == 0) { // unoccupied
			if (!free_map_allocate(1, &disk_inode->direct_blocks[i]))
				return false;
			buffer_cache_write(disk_inode->direct_blocks[i], zeros);
		}
	}
	num_sectors -= l;
	if (num_sectors == 0) return true;

	// (2) a single indirect block
	l = num_sectors < INDIRECT ? num_sectors : INDIRECT;
	if (!inode_reserve_indirect(&disk_inode->indirect_block, l))
		return false;
	num_sectors -= l;
	if (num_sectors == 0) return true;

	// (3) a single doubly indirect block
	l = num_sectors < INDIRECT * INDIRECT ? num_sectors : INDIRECT * INDIRECT;
	if (!inode_reserve_doubly_indirect(&disk_inode->doubly_indirect_block, l))
		return false;
	num_sectors -= l;
	if (num_sectors == 0) return true;

	ASSERT(num_sectors == 0);
	return false;
}

static void
inode_deallocate_doubly_indirect(block_sector_t entry, size_t num_sectors)
{
	struct indirect_block indirect_block;
	buffer_cache_read(entry, &indirect_block);

	size_t l = DIV_ROUND_UP(num_sectors, INDIRECT);

	for (size_t i = 0; i < DIV_ROUND_UP(num_sectors,INDIRECT); ++i) {
		size_t subsize = num_sectors < INDIRECT ? num_sectors : INDIRECT;
		inode_deallocate_indirect(indirect_block.pointers[i], subsize);
		num_sectors -= subsize;
	}

	free_map_release(entry, 1);
}
static void
inode_deallocate_indirect(block_sector_t entry, size_t num_sectors)
{
	struct indirect_block indirect_block;
	buffer_cache_read(entry, &indirect_block);

	for(size_t i = 0; i<num_sectors; i++)
		free_map_release(indirect_block.pointers[i],1);

	free_map_release(entry, 1);
}

static
bool inode_deallocate(struct inode *inode)
{
	off_t file_length = inode->data.length; // bytes
	if (file_length < 0) return false;

	size_t num_sectors = DIV_ROUND_UP(file_length,BLOCK_SECTOR_SIZE);

	// (1) direct blocks
	size_t l = num_sectors < DIRECT ? num_sectors : DIRECT;
	for (size_t i = 0; i < l; ++i) {
		free_map_release(inode->data.direct_blocks[i], 1);
	}
	if(num_sectors <= DIRECT)
		return true;

	// (2) a single indirect block
	l = num_sectors < INDIRECT ? num_sectors : INDIRECT;
	if (num_sectors - DIRECT > 0) {
		inode_deallocate_indirect(inode->data.indirect_block, l);
	}
	if(num_sectors <= DIRECT + INDIRECT)
		return true;

	// (3) a single doubly indirect block
	
	l = num_sectors <  INDIRECT * INDIRECT ? num_sectors : INDIRECT * INDIRECT;
	if (num_sectors - DIRECT - INDIRECT > 0) {
		inode_deallocate_doubly_indirect(inode->data.doubly_indirect_block, l);
	}
	return true;
}
