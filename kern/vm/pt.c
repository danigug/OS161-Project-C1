#include <types.h>
#include <lib.h>
#include <vm.h>
#include <coremap.h>
#include <spinlock.h>
#include <swapfile.h>

#include <novavm.h>
#include <machine/vm.h>

#include "pt.h"

struct spinlock free_pt = SPINLOCK_INITIALIZER;

/* Page Table */
struct pt_entry* pt = NULL;

/* **Queue for FIFO support** */
/* queue_fifo contains page numbers (index)*/
//static unsigned int queue_fifo[PT_SIZE];
unsigned int* queue_fifo = NULL;
static unsigned int queue_front = 0;
static unsigned int queue_rear = 0;


void pt_init(){
    pt = (struct pt_entry*) kmalloc(sizeof(struct pt_entry)*PT_SIZE);
    if (pt==NULL){
        return;
    }
    pt_clean_up();

    queue_fifo = kmalloc(sizeof(uint8_t)*PT_SIZE);
    if (queue_fifo==NULL){  return;  }

#if PRINT_IPT_DIM
    kprintf("Dimension of a single IPT entry: %d B\n",sizeof(struct pt_entry));
    kprintf("Current PT_SIZE: %d (# of frames/pages)\n",PT_SIZE);
    kprintf("Dimension of page table %d KB \n",(sizeof(struct pt_entry)*PT_SIZE)/1024);
    kprintf("Size of queue fifo %d KB \n\n",(sizeof(unsigned int)*PT_SIZE)/1024);
#endif

    for (unsigned long i=0; i<PT_SIZE; i++){
        queue_fifo[i]=0;
    }
}

/* Pop on queue_fifo */
/* Push on queue_fifo is done in pt_map */
static unsigned int pt_queue_fifo_pop() {
    KASSERT(queue_front != queue_rear);

    /* index of old page to pop */
    unsigned int old = queue_fifo[queue_front];
    //write on swapfile -> call a function in swapfile.c
    pt_swap_push(&pt[old]);
    pt_page_free(old);
    queue_front = (queue_front + 1) % PT_SIZE;
    
    return old;
}

/* L'ho scritto in una funzione a parte perché potrebbe tornarci utile per azzerare la pt*/
void pt_clean_up(){
    unsigned long i;
    for (i=0; i<PT_SIZE; i++){
        //pt[i].paddr=i*PAGE_SIZE;
        pt[i].status=ABSENT;
        pt[i].protection=PT_E_RW;
    }
}

void pt_page_free(unsigned int i){
    //pt[i].paddr=0;
    pt[i].status=ABSENT;
    pt[i].protection=PT_E_RW;
}

void pt_destroy(){
    kfree(pt);
}

/* p is the physical address of the frame (frame number)*/
/* so, some different virtual addresses have the same physical address (frame)*/
/* frame number != page number */
/* I obtain the page number dividing by PAGE_SIZE; I obtain the frame number from the page table */
void pt_map(paddr_t p, vaddr_t v){
    
    KASSERT(p!=0);
    KASSERT(v!=0);

    /* To be sure it is aligned */
    p &= PAGE_FRAME;
    v &= PAGE_FRAME;

    /* Index in IPT based on physical address p */
    int i = (int) p/PAGE_SIZE;

    spinlock_acquire(&free_pt);
    pt[i].vaddr=v;
    //pt[i].pid = something;
    if(pt[i].status==ABSENT){
        pt[i].status=PRESENT;
    }
    pt[i].protection=PT_E_RW;
    spinlock_release(&free_pt);

    /* TO DO: Should be checked for IPT */
    /* Push on queue_fifo and check on swapfile */
    spinlock_acquire(&free_pt);
    paddr_t res = pt_swap_pop(&pt[i]); /* pop on swapfile if paddr is written there */
    queue_fifo[queue_rear] = i; /* we write the index of page table */
    queue_rear = (queue_rear + 1) % PT_SIZE;  /* update of rear */
    spinlock_release(&free_pt);

    (void)res;
}

paddr_t pt_fault(uint32_t faulttype){
    unsigned int i;
    switch(faulttype){
        case INVALID_MAP:
        panic("Invalid input address for paging\n");
        
        case NOT_MAPPED:
        {
        /* Wr're trying to access to a not mapped page */
        /* Let's update it in memory and so in page table */
        
        paddr_t p = alloc_upage();

        if(p==0){
            /* there's not enough space -> substitute */    
            i = pt_queue_fifo_pop(); //Liberazione nella pt
            free_upage(i*PAGE_SIZE); //Liberazione nella "coremap"
            p=alloc_upage(); /* new address -> later, look if it is written in swapfile*/
        }

        /* Remember: p must be mapped -> look at pt_translate() */
        return p;

        }
        break;
        default:
        break;
    }
    return 0;
}

paddr_t pt_translate(vaddr_t v){
    paddr_t p; /* physical address of the frame (frame number) */
    
    unsigned i;
    bool found = false;

    /* Alignment of virtual address to page */
    v &= PAGE_FRAME;

    /* Search of the virtual address inside the IPT */
    spinlock_acquire(&free_pt);
    for(i=0;i<PT_SIZE;i++){
        if(pt[i].vaddr == v){
            found = true;
            break;
        }
    }
    spinlock_release(&free_pt);

    if(found){
        p = PAGE_SIZE*i;
    }
    else {
        p = pt_fault(NOT_MAPPED);
        pt_map(p,v);
    }

    return p;
}

void pt_swap_push(struct pt_entry* pt_e){
    (void)pt_e;
    //swap_in(pt_e->paddr);
}

paddr_t pt_swap_pop(struct pt_entry* pt_e){
    paddr_t res = 0;
    (void)pt_e;
    //res=swap_out(pt_e->paddr);

    return res;
}