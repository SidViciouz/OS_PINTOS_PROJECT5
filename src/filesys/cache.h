#ifndef CACHE
#define CACHE

#include "devices/block.h"


//void buffer_cache_flush(struct buffer_cache_entry_t *entry);
void buffer_cache_init(void);
void buffer_cache_terminate(void);
void buffer_cache_read(block_sector_t sector, void *target);
void buffer_cache_write(block_sector_t sector, const void *source);

#endif
