#include "cache.h"
#include "threads/malloc.h"

static struct buffer_cache_entry *cache;

void buffer_cache_init(){
	cache = (struct buffer_cache_entry*)malloc(sizeof(struct buffer_cache_entry)*NUM_CACHE);
	for(int i=0; i<NUM_CACHE; i++){
		cache[i].valid_bit = false;
		cache[i].reference_bit = false;
		cache[i].dirty_bit = false;
		cache[i].disk_sector = -1;
		memset(cache[i].buffer,0,512);
	}
}
void buffer_cache_terminate(){
	free(cache);
}
void buffer_cache_read(){
}
void buffer_cache_write(){
}
struct buffer_cache_entry *buffer_cache_lookup(block_sector_t a){
}
struct buffer_cache_entry *buffer_cache_select_victim(){
}
void buffer_cache_flush_entry(struct buffer_cache_entry * a){
}
void buffer_cache_flush_all(){
}
