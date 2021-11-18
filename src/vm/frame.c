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
	struct list_elem *e;
	struct frame_e *fe = list_entry(e,struct frame_e,elem);
	e = list_begin(&frame_list);
	int j= list_size(&frame_list);
	for(int i=0; i<=2*list_size(&frame_list); i++)
	{
		fe = list_entry(e,struct frame_e,elem);
		if(pagedir_is_accessed(thread_current()->pagedir,fe->spte->vaddr)){
			pagedir_set_accessed(thread_current()->pagedir,fe->spte->vaddr,false);	
		}
		
		else{
			break;
		}
	
		if( list_next(e) == list_end(&frame_list)){
			e = list_begin(&frame_list);
		}
		else{
			e = list_next(e);
		}
	}
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

void* frame_allocate(void* upage,enum palloc_flags flags){

	void *frame = palloc_get_page(PAL_USER|flags);

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
		add_spte(upage,frame,0,0,true,NULL,0);
		e1 = hash_find(&thread_current()->spt,&find_e.elem);
		if( e1 == NULL)
			printf("e1 is NULL\n");
	}
	struct spt_e* found1 = hash_entry(e1,struct spt_e,elem);
	add_frame_e(found1,frame);	

	return frame;
}

void frame_free(void* kpage){

	lock_acquire(&frame_lock);
	struct list_elem *e;
	for(e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
		struct frame_e *fe = list_entry(e,struct frame_e,elem);
		if(fe->kaddr == kpage){
			list_remove(&fe->elem);
			free(fe);
			break;
		}
	}
	palloc_free_page(kpage);	
	lock_release(&frame_lock);

}
