#include "page.h"

unsigned hash_value(const struct hash_elem* e,void *aux)
{
	const struct spt_e *entry = hash_entry(e,struct spt_e,elem);

	return hash_bytes(&entry->vaddr,sizeof(entry->vaddr));
}

bool hash_compare(const struct hash_elem *a,const struct hash_elem *b,void *aux)
{
	const struct spt_e *e1 = hash_entry(a,struct spt_e,elem);
	const struct spt_e *e2 = hash_entry(b,struct spt_e,elem);

	return e1->vaddr < e2->vaddr;
}
void add_spte(void* upage,size_t page_read_bytes,size_t page_zero_bytes,bool writable,struct file* file,size_t ofs){
	struct spt_e *spte = (struct spte *)malloc(sizeof(struct spt_e)); //insert spte
	spte->vaddr = upage;
	spte->page_read_bytes = page_read_bytes;
	spte->page_zero_bytes = page_zero_bytes;
 	spte->writable = writable;
 	spte->file = file;
 	spte->ofs = ofs;
	spte->swap_slot = -1;
	hash_insert(&thread_current()->spt,&(spte->elem));
}

