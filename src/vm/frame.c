#include "frame.h"
#include "swap.h"
#include "threads/palloc.h"

static struct list frame_list;
static struct lock frame_lock;

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
	struct list_elem *e;// = list_pop_back(&frame_list); //list_pop_back에서 lru로 바꿔야함.
	struct frame_e *fe = list_entry(e,struct frame_e,elem);

	e = list_begin(&frame_list);
	int j= list_size(&frame_list);
	//printf("list_size : %d\n",j);
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
	int swap_slot = swap_to_disk(fe->kaddr);
	fe->spte->swap_slot = swap_slot;
	//printf("swap_slot : %d\n",fe->spte->swap_slot);
	list_remove(e);

	//palloc_free_page(fe->kaddr);
	//pagedir_clear_page(fe->t->pagedir,fe->spte->vaddr);	
	lock_release(&frame_lock);
	return fe;
}

void add_frame_e(struct spt_e* spte){
      	struct frame_e *fe = (struct frame_e*)malloc(sizeof(struct frame_e));
	fe->kaddr = pagedir_get_page(thread_current()->pagedir,spte->vaddr);
 	fe->t = thread_current();
	fe->spte = spte;
      	insert_frame_e(&fe->elem);	
}

void* frame_allocate(void* upage){
	lock_acquire(&frame_lock);

	void *frame = palloc_get_page(PAL_USER);

	if(frame == NULL){
		struct frame_e* evicted = free_frame();
		pagedir_clear_page(evicted->t->pagedir,evicted->spte->vaddr);
		palloc_free_page(evicted->kaddr);

		frame = palloc_get_page(PAL_USER);
		ASSERT(frame != NULL);
	}
	
	struct spt_e find_e;
	find_e.vaddr = upage;
	struct hash_elem *e1 = hash_find(&thread_current()->spt,&find_e.elem);
	if(e1 == NULL)
		exit(-1);
	struct spt_e* found1 = hash_entry(e1,struct spt_e,elem);	
	add_frame_e(found1);	

	lock_release(&frame_lock);
}
