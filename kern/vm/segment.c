#include <segment.h>
#include <lib.h>
#include <types.h> 

struct segment*
segment_create(){
    struct segment* s = kmalloc(sizeof(struct segment));
    if(s==NULL){
        return NULL;
    }
    segment_init(s);

    return s;
}

void
segment_init(struct segment *s){
    s->vaddr = (vaddr_t) 0;
    s->memsize = (size_t) 0;
    s->filesize = (size_t) 0;
    s->offset = (size_t) 0;
    s->is_loaded = NOT_LOADED;
    s->permission = S_RO;
    s->file_elf = NULL;
    s->as = NULL;
}

void
segment_destroy(struct segment *s)
{
    if (s==NULL){
        return;
    }
    kfree(s);
}

int
segment_copy(struct segment *old, struct segment **ret){
    struct segment *newsg;

	newsg = segment_create();
	if (newsg==NULL) {
		return -1;
	}

	newsg->vaddr=old->vaddr;
    newsg->memsize=old->memsize;
    newsg->npage=old->npage;
    newsg->filesize=old->filesize;
    newsg->offset=old->offset;
    newsg->is_loaded=NOT_LOADED; //LOADED?
    //Have to load?
    newsg->permission=old->permission;
    newsg->file_elf=old->file_elf;
    newsg->as=old->as;

	*ret = newsg; /* we pass the segment here */

	return 0;
}


