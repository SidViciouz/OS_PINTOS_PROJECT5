#ifndef FRAME_HEADER
#define FRAME_HEADER

#include "page.h"

struct frame_e{
	struct list_elem elem;
	struct thread *t;
	void *kaddr;
	struct spt_e *spte;
};

void frame_print();
void init_frame_list(void);

void insert_frame_e(struct list_elem *e);

struct frame_e* free_frame();

void add_frame_e(struct spt_e* spte,void *kaddr);

void* frame_allocate(void* upage);

void frame_free(void*);
#endif
