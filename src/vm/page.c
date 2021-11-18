#include "page.h"
#include "frame.h"
#include "threads/vaddr.h"

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
void spte_destroy(struct hash_elem *elem,void *aux)
{
	struct spt_e *spte = hash_entry(elem,struct spt_e,elem);

	if(spte->kpage != NULL){
		frame_free_without_palloc(spte->kpage);	
	}
	free(spte);
}

void add_spte(void* upage,void* kpage,size_t page_read_bytes,size_t page_zero_bytes,bool writable,struct file* file,size_t ofs){
	struct spt_e *spte = (struct spte *)malloc(sizeof(struct spt_e)); //insert spte
	spte->vaddr = upage;
	spte->kpage = kpage;
	spte->page_read_bytes = page_read_bytes;
	spte->page_zero_bytes = page_zero_bytes;
 	spte->writable = writable;
 	spte->file = file;
 	spte->ofs = ofs;
	spte->swap_slot = -1;
	hash_insert(&thread_current()->spt,&(spte->elem));
}
/*
struct spt_e* spt_lookup(struct hash *spt,void *page){
	struct spt_e spte;
	spte.vaddr = page;

	struct hash_elem *elem = hash_find(spt,&spte.elem);
	if(elem == NULL) return NULL;
	return hash_entry(elem,struct spt_e,elem);			
}
bool spt_has_entry(struct hash *spt,void *page){
	struct spt_e *spte = spt_lookup(spt,page);
	if(spte == NULL) return false;
	return true;
}
bool load_page(struct hash *spt,int *pagedir, void *upage){

	struct spt_e *spte;
	spte = spt_lookup(spt,upage);
	if(spte == NULL)
		return false;
	
	if(spte->status == ON_FRAME)
		return true;

	void *frame_page;
	if(frame_page == NULL)
		return false;

	bool writable = true;
	switch(spte->status){
	case ALL_ZERO:
		memset(frame_page,0,PGSIZE);
		break;
	case ON_FRAME:
		break;
	case ON_SWAP:
		swap_to_addr(spte->swap_slot,frame_page);
		break;
	case FROM_FILESYS:
		if(load_page_from_filesys(spte,frame_page) == false){
			frame_free(frame_page);
			return false;
		}
		writable = spte->writable;
		break;
	default:
		PANIC("unreachable state");
	}

	if(!pagedir_set_page(pagedir,upage,frame_page,writable)){
		frame_free(frame_page);
		return false;
	}

	spte->kpage = frame_page;
	spte->status = ON_FRAME;

	pagedir_set_dirty(pagedir,frame_page,false);

	return true;
}
static bool load_page_from_filesys(struct spt_e *spte,void *kpage){
	file_seek(spte->file,spte->ofs);

	int n_read = file_read(spte->file,kpage,spte->page_read_bytes);
	if(n_read != (int)spte->page_read_bytes)
		return false;

	memset(kpage + n_read,0,spte->page_zero_bytes);
	return true;
}*/
