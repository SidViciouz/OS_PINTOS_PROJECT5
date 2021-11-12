#ifndef FRAME_HEADER
#define FRAME_HEADER

#include "page.h"

struct frame_e{
	struct list_elem elem;
	struct thread *t;
	void *kaddr;
	struct spt_e *spte;
};

void init_frame_list(void);

void insert_frame_e(struct list_elem *e);

#endif
