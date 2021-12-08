#include "cache.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys.h"

#define NUM_CACHE 64
static struct buffer_cache_entry cache[NUM_CACHE];
static struct lock cache_lock;
static int clock_number;

void buffer_cache_init(){
	for(int i=0; i<NUM_CACHE; i++){
		cache[i].valid_bit = false;
	}
	lock_init(&cache_lock);
	clock_number = 0;
}
void buffer_cache_terminate(){
	lock_acquire(&cache_lock);
	for(int i=0; i<NUM_CACHE; i++){
		if(cache[i].valid_bit)
			buffer_cache_flush_entry(&cache[i]);
	}
	lock_release(&cache_lock);
}
void buffer_cache_read(block_sector_t sector,uint8_t* buffer){
	lock_acquire(&cache_lock);

	struct buffer_cache_entry *entry = buffer_cache_lookup(sector);
	if(entry == NULL){
		entry = buffer_cache_select_victim();
		block_read(fs_device,sector,entry->buffer);
		entry->disk_sector = sector;
		entry->valid_bit = true;
		entry->dirty_bit = false;
	}
	memcpy(buffer,entry->buffer,BLOCK_SECTOR_SIZE);
	entry->reference_bit = true;

	lock_release(&cache_lock);
}
void buffer_cache_write(block_sector_t sector,uint8_t* buffer){
	lock_acquire(&cache_lock);

	struct buffer_cache_entry *entry = buffer_cache_lookup(sector);
	if(entry == NULL){
		entry = buffer_cache_select_victim();
		block_read(fs_device,sector,entry->buffer);
		entry->disk_sector = sector;
		entry->valid_bit = true;
		entry->dirty_bit = false;
	}
	memcpy(entry->buffer,buffer,BLOCK_SECTOR_SIZE);
	entry->dirty_bit = true;
	entry->reference_bit = true;

	lock_release(&cache_lock);
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
		if(temp->valid_bit == false){
			if(temp->dirty_bit)
				buffer_cache_flush_entry(temp);
			temp->valid_bit = false;
			return temp;
		}
		
		if(temp->reference_bit == true)
			temp->reference_bit = false;
		else{
			if(temp->dirty_bit)
				buffer_cache_flush_entry(temp);
			temp->valid_bit = false;
			return temp;	
		}
	}		
}
void buffer_cache_flush_entry(struct buffer_cache_entry * a){
	if(a->dirty_bit){
		a->dirty_bit = false;
		block_write(fs_device,a->disk_sector,a->buffer);
	}
}
void buffer_cache_flush_all(){
	for(int i=0; i<NUM_CACHE; i++){
		buffer_cache_flush_entry(&cache[i]);
	}
}

struct buffer_cache_entry* next_clock_number(){
	if(clock_number >= 63)
		clock_number = 0;
	else
		clock_number++;

	return &cache[clock_number];
}
