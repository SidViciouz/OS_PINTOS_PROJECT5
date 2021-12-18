#ifndef CACHE
#define CACHE

#include "devices/block.h"


//void buffer_cache_flush(struct buffer_cache_entry_t *entry);
void buffer_cache_init(void);
void buffer_cache_terminate(void);
struct buffer_cache_entry_t* buffer_cache_select_victim(void);
struct buffer_cache_entry_t* buffer_cache_lookup(block_sector_t sector);
void buffer_cache_flush_entry(struct buffer_cache_entry_t *entry);
void buffer_cache_read(block_sector_t sector, void *target);
void buffer_cache_write(block_sector_t sector, const void *source);

#endif
