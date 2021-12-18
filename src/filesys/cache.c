#include <debug.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

#define CACHE_SIZE 64

struct buffer_cache_entry_t {
	bool valid;  
	bool dirty;     
	bool reference;    //for clock algorithm
	block_sector_t disk_sector;
	uint8_t buffer[BLOCK_SECTOR_SIZE];

};

static size_t clock_idx;
static struct buffer_cache_entry_t cache[CACHE_SIZE];
static struct lock buffer_cache_lock;

void buffer_cache_init(void)
{
	clock_idx = 0;
	lock_init(&buffer_cache_lock);
	for (size_t i = 0; i < CACHE_SIZE; ++i)
		cache[i].valid = false;
}

void buffer_cache_flush_entry(struct buffer_cache_entry_t *entry)
{

	if (entry->dirty) {
		block_write(fs_device, entry->disk_sector, entry->buffer);
		entry->dirty = false;
	}
}

void buffer_cache_terminate(void)
{
	lock_acquire(&buffer_cache_lock);

	for (size_t i = 0; i < CACHE_SIZE; ++i)
	{
		if (cache[i].valid == false) continue;
		buffer_cache_flush_entry(&(cache[i]));
	}

	lock_release(&buffer_cache_lock);
}


struct buffer_cache_entry_t* buffer_cache_lookup(block_sector_t sector)
{
	for (size_t i = 0; i < CACHE_SIZE; ++i)
	{
		if (cache[i].valid == true){
			if (cache[i].disk_sector == sector) return &(cache[i]);
		}
	}
	return NULL;
}

struct buffer_cache_entry_t* buffer_cache_select_victim(void)
{

	// clock algorithm
	while (1) {
		if (!cache[clock_idx].valid)
			return &(cache[clock_idx]);

		if (cache[clock_idx].reference)
			cache[clock_idx].reference = false;
		else break;

		clock_idx++;
		clock_idx %= CACHE_SIZE;
	}

	struct buffer_cache_entry_t *slot = &cache[clock_idx];
	if (slot->dirty) {
		buffer_cache_flush_entry(slot);
	}

	slot->valid = false;
	return slot;
}


void buffer_cache_read(block_sector_t sector, void *target)
{
	lock_acquire(&buffer_cache_lock);

	struct buffer_cache_entry_t *slot = buffer_cache_lookup(sector);
	if (slot == NULL) {
		slot = buffer_cache_select_victim();
		slot->valid = true;
		slot->dirty = false;
		slot->disk_sector = sector;
		block_read(fs_device, sector, slot->buffer);
	}
	memcpy(target, slot->buffer, BLOCK_SECTOR_SIZE);
	slot->reference = true;
	lock_release(&buffer_cache_lock);
}

void buffer_cache_write(block_sector_t sector, const void *source)
{
	lock_acquire(&buffer_cache_lock);

	struct buffer_cache_entry_t *slot = buffer_cache_lookup(sector);
	if (slot == NULL) {
		slot = buffer_cache_select_victim();
		slot->valid = true;
		slot->disk_sector = sector;
		slot->dirty = false;
		block_read(fs_device, sector, slot->buffer);
	}

	memcpy(slot->buffer, source, BLOCK_SECTOR_SIZE);
	slot->reference = true;
	slot->dirty = true;

	lock_release(&buffer_cache_lock);
}
