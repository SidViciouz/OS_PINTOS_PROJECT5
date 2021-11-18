#ifndef PAGE_HEADER
#define PAGE_HEADER
#include <hash.h>
#include <threads/thread.h>

enum page_status{
	ALL_ZERO,
	ON_FRAME,
	ON_SWAP,
	FROM_FILESYS
};

struct spt_e{
	void *vaddr;
	void *kpage;
	size_t page_read_bytes;
	size_t page_zero_bytes;
	bool writable;
	struct file* file;
	struct hash_elem elem;
	size_t ofs;
	int swap_slot;
	enum page_status status;
};

unsigned hash_value(const struct hash_elem* e,void *aux);

bool hash_compare(const struct hash_elem *a,const struct hash_elem *b,void *aux);

void add_spte(void* upage,void* kpage,size_t page_read_bytes,size_t page_zero_bytes,bool writable,struct file* file,size_t ofs);

struct spt_e* spt_lookup(struct hash *spt,void *page);
bool spt_has_entry(struct hash *spt,void *page);
bool load_page(struct hash *spt,int *pagedir, void *upage);
static bool load_page_from_filesys(struct spt_e *spte,void *kpage);
#endif
