#include "swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"

static struct bitmap *swap_bitmap;

static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

void init_swap_bitmap()
{
	swap_bitmap = bitmap_create(1024);
}
void destroy_swap_bitmap()
{
	bitmap_destroy(swap_bitmap);
}

int find_swap_slot()
{
	int swap_idx = bitmap_scan(swap_bitmap,0,1,false);

	if(swap_idx != BITMAP_ERROR)
		return swap_idx;
	else
		return -1;
}

int swap_to_disk(void* kaddr){

	ASSERT(kaddr >= PHYS_BASE);
	struct block* b = block_get_role(BLOCK_SWAP);
	int swap_slot = find_swap_slot();

	if(swap_slot == -1){
		return -1;
	}

	//printf("swap to disk1\n");
	for(int i=0; i<SECTORS_PER_PAGE; i++){
		//printf("block write1\n");
		block_write(b,swap_slot*SECTORS_PER_PAGE + i,kaddr +(i*BLOCK_SECTOR_SIZE));
		//printf("block write2\n");
	}
	//printf("swap to disk2\n");
	bitmap_set(swap_bitmap,swap_slot,true);
	
	return swap_slot;
}

void swap_to_addr(int swap_slot,void * kaddr){
	
	struct block* b = block_get_role(BLOCK_SWAP);

	for(int i=0; i<SECTORS_PER_PAGE; i++)
		block_read(b,swap_slot*SECTORS_PER_PAGE+i,kaddr+(i*BLOCK_SECTOR_SIZE));	
	bitmap_set(swap_bitmap,swap_slot,false);
}

void swap_blank()
{

	struct block* b = block_get_role(BLOCK_SWAP);
	int swap_size = block_size(b)/SECTORS_PER_PAGE;
	printf("bitmap used slot:%d\n",bitmap_count(swap_bitmap,0,1024,true));
	printf("swap_size : %d\n",swap_size);
}
