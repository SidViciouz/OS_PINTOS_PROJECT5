#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format(void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init(bool format)
{
	fs_device = block_get_role(BLOCK_FILESYS);
	if (fs_device == NULL)
		PANIC("No file system device found, can't initialize file system.");

	inode_init();
	free_map_init();

	buffer_cache_init();

	if (format)
		do_format();

	free_map_open();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done(void)
{
	free_map_close();

	buffer_cache_terminate();
}

/* Creates a file or directory (set by `is_dir`) of
   full path `path` with the given `initial_size`.
   The path to file consists of two parts: path directory and filename.

   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create(const char *path, off_t initial_size, bool is_dir)
{
	block_sector_t inode_sector = 0;

	char d[strlen(path)];
	char f[strlen(path)];
	extract_directory_filename_from_path(path, d, f);
	struct dir *dir = dir_open_path(d);

	bool success = (dir != NULL
		&& free_map_allocate(1, &inode_sector)
		&& inode_create(inode_sector, initial_size, is_dir)
		&& dir_add(dir, f, inode_sector, is_dir));

	if (!success && inode_sector != 0)
		free_map_release(inode_sector, 1);
	dir_close(dir);

	return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
	filesys_open(const char *name)
{
	if (strlen(name) == 0){
		return NULL;
	}

	char d[strlen(name) + 1];
	char f[strlen(name) + 1];
	extract_directory_filename_from_path(name, d, f);
	struct dir *dir = dir_open_path(d);
	struct inode *inode = NULL;

	// removed directory handling
	if (dir == NULL){
		return NULL;
	}
	else if (strlen(f) > 0) {
		dir_lookup(dir, f, &inode);
		dir_close(dir);
	}
	else { // empty filename : just return the directory
		inode = dir_get_inode(dir);
	}

	// removed file handling
	if (inode == NULL)
		return NULL;
	else if(inode->removed)
		return NULL;

	return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove(const char *name)
{
	char d[strlen(name)];
	char f[strlen(name)];
	extract_directory_filename_from_path(name, d, f);
	struct dir *dir = dir_open_path(d);

	if(dir == NULL){
		dir_close(dir);
		return false;
	}
	if(!dir_remove(dir,f)){
		dir_close(dir);
		return false;
	}

	dir_close(dir);
	return true;
}

/* Formats the file system. */
static void
do_format(void)
{
	printf("Formatting file system...");
	free_map_create();
	if (!dir_create(ROOT_DIR_SECTOR, 16))
		PANIC("root directory creation failed");
	free_map_close();
	printf("done.\n");
}

bool filesys_chdir(const char *dir_path)
{
	struct dir *d = dir_open_path(dir_path);

	if (d == NULL) return false;

	dir_close(thread_current()->current_dir);

	thread_current()->current_dir = d;

	return true;
}
