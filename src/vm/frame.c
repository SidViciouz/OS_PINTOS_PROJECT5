#include "frame.h"
#include "swap.h"
#include "threads/palloc.h"

static struct list frame_list;
static struct lock frame_lock;

void frame_print()
{
	int i = 0;
	struct list_elem *e = list_begin(&frame_list);
	while(e != list_end(&frame_list)){
		struct frame_e *fe = list_entry(e,struct frame_e,elem);
		if(fe->spte->swap_slot != -1)
			i++;
		e = list_next(e);
	}
	printf("frame_print : %d\n",i);
}

void init_frame_list(void)
{
	list_init(&frame_list);
	lock_init(&frame_lock);
}

void insert_frame_e(struct list_elem* e)
{
	lock_acquire(&frame_lock);
	list_push_back(&frame_list,e);
	lock_release(&frame_lock);
}

struct frame_e* free_frame()
{
	lock_acquire(&frame_lock);
	//dirty bit에 따라 swap slot에 swap in하는거 추가해야함
	struct list_elem *e;//= list_pop_back(&frame_list); //list_pop_back에서 lru로 바꿔야함.
	struct frame_e *fe = list_entry(e,struct frame_e,elem);

	e = list_begin(&frame_list);
	int j= list_size(&frame_list);
	for(int i=0; i<2*list_size(&frame_list); i++)
	{
		fe = list_entry(e,struct frame_e,elem);
		if(pagedir_is_accessed(thread_current()->pagedir,fe->spte->vaddr)){
			//printf("1\n");
			pagedir_set_accessed(thread_current()->pagedir,fe->spte->vaddr,false);	
		}
		
		else{
			//printf("2\n");
			break;
		}
	

		if( list_next(e) == list_end(&frame_list)){
			//printf("3\n");
			e = list_begin(&frame_list);
		}
		else{
			//printf("4\n");
			e = list_next(e);
		}
	}
	//int swap_slot = swap_to_disk(fe->kaddr);
	//fe->spte->swap_slot = swap_slot;
	list_remove(e);

	lock_release(&frame_lock);
	return fe;
}

void add_frame_e(struct spt_e* spte,void *kaddr){
      	struct frame_e *fe = (struct frame_e*)malloc(sizeof(struct frame_e));
	fe->kaddr = kaddr;
 	fe->t = thread_current();
	fe->spte = spte;
      	insert_frame_e(&fe->elem);	
}

void* frame_allocate(void* upage){

	void *frame = palloc_get_page(PAL_USER);

	if(frame == NULL){
		struct frame_e* evicted = free_frame();
		evicted->spte->swap_slot = swap_to_disk(evicted->kaddr);
		pagedir_clear_page(evicted->t->pagedir,evicted->spte->vaddr);
		palloc_free_page(evicted->kaddr);

		frame = palloc_get_page(PAL_USER);

		if(frame == NULL){
			printf("frame is NULL\n");
			exit(-1);
		}
	}
	
	struct spt_e find_e;
	find_e.vaddr = upage;
	struct hash_elem *e1 = hash_find(&thread_current()->spt,&find_e.elem);
	if(e1 == NULL){ //stack growth
		add_spte(upage,0,0,true,NULL,0);
		e1 = hash_find(&thread_current()->spt,&find_e.elem);
		if( e1 == NULL)
			printf("e1 is NULL\n");
	}
	struct spt_e* found1 = hash_entry(e1,struct spt_e,elem);
	add_frame_e(found1,frame);	

	return frame;
}
