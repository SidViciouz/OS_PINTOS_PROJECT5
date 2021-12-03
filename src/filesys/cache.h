#ifndef CACHE
#define CACHE

#include "threads/vaddr.h"
#include "devices/block.h"

#define NUM_CACHE 64

struct buffer_cache_entry{
	bool valid_bit;
	bool reference_bit;
	bool dirty_bit;
	block_sector_t disk_sector;
	uint8_t buffer[BLOCK_SECTOR_SIZE];
};

void buffer_cache_init();
void buffer_cache_terminate();
void buffer_cache_read();
void buffer_cache_write();
struct buffer_cache_entry *buffer_cache_lookup(block_sector_t);
struct buffer_cache_entry *buffer_cache_select_victim();
void buffer_cache_flush_entry(struct buffer_cache_entry *);
void buffer_cache_flush_all();
struct buffer_cache_entry* next_clock_number();

#endif
