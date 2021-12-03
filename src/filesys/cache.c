#include "cache.h"
#include "threads/malloc.h"

static struct buffer_cache_entry *cache;
static int clock_number;

void buffer_cache_init(){
	cache = (struct buffer_cache_entry*)malloc(sizeof(struct buffer_cache_entry)*NUM_CACHE);
	for(int i=0; i<NUM_CACHE; i++){
		cache[i].valid_bit = false;
		cache[i].reference_bit = false;
		cache[i].dirty_bit = false;
		cache[i].disk_sector = -1;
		memset(cache[i].buffer,0,512);
	}
	clock_number = 0;
}
void buffer_cache_terminate(){
	free(cache);
}
void buffer_cache_read(block_sector_t sector,uint8_t* buffer,int size){

}
void buffer_cache_write(block_sector_t sector,uint8_t* buffer,int size){
}
struct buffer_cache_entry *buffer_cache_lookup(block_sector_t sector_to_find){
	for(int i=0; i<NUM_CACHE; i++){
		if(cache[i].valid_bit && cache[i].disk_sector == sector_to_find)
			return &cache[i];
	}
	return NULL;
}
struct buffer_cache_entry *buffer_cache_select_victim(){
	struct buffer_cache_entry* temp;

	for(int i=0; i<2*NUM_CACHE; i++){
		temp = next_clock_number();
		if(temp->reference_bit == true)
			temp->reference_bit = false;
		else
			return temp;	
	}		
}
void buffer_cache_flush_entry(struct buffer_cache_entry * a){
}
void buffer_cache_flush_all(){
}

struct buffer_cache_entry* next_clock_number(){
	if(clock_number >= 63)
		clock_number = 0;
	else
		clock_number++;

	return &cache[clock_number];
}
