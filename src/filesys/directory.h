#ifndef DIRECTORY
#define DIRECTORY

#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"

#define NAME_MAX 15

struct inode;

void split_path_filename(const char *path, char *directory, char *filename);

/* Opening and closing directories. */
bool dir_create(block_sector_t sector, size_t entry_cnt);
struct dir *dir_open(struct inode *);
struct dir *dir_open_root(void);
struct dir *dir_open_path(const char *);
struct dir *dir_reopen(struct dir *);
void dir_close(struct dir *);
struct inode *dir_get_inode(struct dir *);

/* Reading and writing. */
bool dir_is_empty(const struct dir *);
bool dir_lookup(const struct dir *, const char *name, struct inode **);
bool dir_add(struct dir *, const char *name, block_sector_t, bool is_dir);
bool dir_remove(struct dir *, const char *name);
bool dir_readdir(struct dir *, char name[NAME_MAX + 1]);

#endif /* filesys/directory.h */
