#include <debug.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

#define CACHE_SIZE 64

struct cache_entry {
	bool valid;  
	bool dirty;     
	bool reference;
	block_sector_t disk_sector;
	uint8_t buffer[BLOCK_SECTOR_SIZE];

};

static size_t clock_idx;
static struct cache_entry cache[CACHE_SIZE];
struct lock cache_lock;

void buffer_cache_init(void)
{
	clock_idx = 0;
	lock_init(&cache_lock);
	for (size_t i = 0; i < CACHE_SIZE; ++i)
		cache[i].valid = false;
}

void buffer_cache_flush_entry(struct cache_entry *entry)
{
	if (entry->dirty == true) {
		block_write(fs_device, entry->disk_sector, entry->buffer);
		entry->dirty = false;
	}
}

void buffer_cache_terminate(void)
{
	lock_acquire(&cache_lock);

	for (size_t i = 0; i < CACHE_SIZE; ++i)
	{
		if (cache[i].valid == true)
			buffer_cache_flush_entry(&(cache[i]));
	}

	lock_release(&cache_lock);
}


struct cache_entry* buffer_cache_lookup(block_sector_t sector)
{
	for (size_t i = 0; i < CACHE_SIZE; ++i)
	{
		if (cache[i].valid == true){
			if (cache[i].disk_sector == sector) return &(cache[i]);
		}
	}
	return NULL;
}

struct cache_entry* buffer_cache_select_victim(void)
{
	while (1) {
		if (!cache[clock_idx].valid)
			return &cache[clock_idx];

		else if (cache[clock_idx].reference)
			cache[clock_idx].reference = false;

		else
			break;

		clock_idx = ++clock_idx % CACHE_SIZE;
	}

	struct cache_entry *slot = &cache[clock_idx];
	if (slot->dirty) {
		buffer_cache_flush_entry(slot);
	}

	slot->valid = false;
	return slot;
}


void buffer_cache_read(block_sector_t sector, void *buffer)
{
	lock_acquire(&cache_lock);

	struct cache_entry *e = buffer_cache_lookup(sector);
	if (e == NULL) {
		e = buffer_cache_select_victim();
		e->valid = true;
		e->dirty = false;
		e->disk_sector = sector;
		block_read(fs_device, sector, e->buffer);
	}
	memcpy(buffer, e->buffer, BLOCK_SECTOR_SIZE);
	e->reference = true;
	lock_release(&cache_lock);
}

void buffer_cache_write(block_sector_t sector, const void *buffer)
{
	lock_acquire(&cache_lock);

	struct cache_entry *e = buffer_cache_lookup(sector);
	if (e == NULL) {
		e = buffer_cache_select_victim();
		e->valid = true;
		e->dirty = false;
		e->disk_sector = sector;
		block_read(fs_device, sector, e->buffer);
	}

	memcpy(e->buffer, buffer, BLOCK_SECTOR_SIZE);
	e->reference = true;
	e->dirty = true;

	lock_release(&cache_lock);
}
