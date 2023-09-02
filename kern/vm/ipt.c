#include <types.h>
#include <lib.h>
#include <vm.h>
#include <coremap.h>
#include <spinlock.h>
#include <swapfile.h>
#include <proc.h>

#include <novavm.h>
#include <machine/vm.h>

#include "ipt.h"

struct spinlock free_ipt = SPINLOCK_INITIALIZER;
struct spinlock free_queue = SPINLOCK_INITIALIZER;

/* Page Table */
struct ipt_entry *ipt = NULL;

/* **Queue for FIFO support** */
/* queue_fifo contains indexes (1,2,3...) of page table */
// scrivo in coda e prelevo in testa
unsigned int *queue_fifo = NULL;
static unsigned int queue_front = 0;
static unsigned int queue_rear = 0;

void ipt_init()
{
    ipt = (struct ipt_entry *)kmalloc(sizeof(struct ipt_entry) * IPT_SIZE);
    if (ipt == NULL)
    {
        return;
    }
    ipt_clean_up();

    queue_fifo = kmalloc(sizeof(uint8_t) * IPT_SIZE);
    if (queue_fifo == NULL)
    {
        return;
    }

#if PRINT_IPT_DIM
    kprintf("Dimension of a single IPT entry: %d B\n", sizeof(struct ipt_entry));
    kprintf("Current IPT_SIZE: %d (# of frames/pages)\n", IPT_SIZE);
    kprintf("Dimension of page table %d KB \n", (sizeof(struct ipt_entry) * IPT_SIZE) / 1024);
    kprintf("Size of queue fifo %d KB \n\n", (sizeof(unsigned int) * IPT_SIZE) / 1024);
#endif

    for (unsigned long i = 0; i < IPT_SIZE; i++)
    {
        queue_fifo[i] = 0;
    }
}

/* Pop on queue_fifo */
/* Push on queue_fifo is done in ipt_map */
static unsigned int ipt_queue_fifo_pop()
{
    KASSERT(queue_front != queue_rear);

    spinlock_acquire(&free_queue);

    /* index of old page to pop */
    unsigned int old = queue_fifo[queue_front];
    spinlock_release(&free_queue);
    // write on swapfile -> call a function in swapfile.c
    ipt_swap_push(&ipt[old]);
    spinlock_acquire(&free_queue);
    ipt_page_free(old);
    queue_front = (queue_front + 1) % IPT_SIZE;

    spinlock_release(&free_queue);

    return old;
}

static void ipt_queue_fifo_push(unsigned int el)
{
    KASSERT(el != 0);
    // TO DO: i defined where?
    unsigned int i = el;
    spinlock_acquire(&free_queue);
    queue_fifo[queue_rear] = i; /* we write the index of page table */
    queue_rear = (queue_rear + 1) % IPT_SIZE; /* update of rear */
    spinlock_release(&free_queue);
    
    /* pop on swapfile if paddr is written there */
    /* pop = invalidate the page entry in SWAPFILE */    
    ipt_swap_pop(&ipt[i]);

}

void ipt_clean_up()
{
    unsigned long i;
    for (i = 0; i < IPT_SIZE; i++)
    {
        ipt[i].p_pid = 0;
        ipt[i].page_number = 0;
        ipt[i].status = ABSENT;
        ipt[i].protection = IPT_E_RW;
    }
}

void ipt_page_free(unsigned int i)
{
    ipt[i].p_pid = 0;
    ipt[i].page_number = 0;
    ipt[i].status = ABSENT;
    ipt[i].protection = IPT_E_RW;
}

void ipt_destroy()
{
    kfree(ipt);
}

static unsigned int ipt_hash(vaddr_t v)
{   
    pid_t pid = proc_getpid();
    unsigned int h = (pid+v) % IPT_SIZE;

    return h;
}


/* Returns index where virtual address is, found is to be checked after calling */
static unsigned int ipt_search(pid_t pid, vaddr_t v, bool *found)
{
    KASSERT(pid != 0);
    //KASSERT(v != 0);

    unsigned int index = 0;
    unsigned int i = 0;
    *found = false;
    /* Page number, no displacement */
    v &= PAGE_FRAME;

    spinlock_acquire(&free_ipt);
    /* index to access to inverted page table */
    
    index = ipt_hash(v);

    for (i = index; i < IPT_SIZE; i++)
    {
        if (ipt[i].p_pid == pid && ipt[i].page_number == v)
        {
            *found = true;
            break;
        }
    }
    if (!found)
    {
        for (i = 0; i < index; i++)
        {
            if (ipt[i].p_pid == pid && ipt[i].page_number == v)
            {
                *found = true;
                break;
            }
        }
    }
    
    index = i;
    spinlock_release(&free_ipt);

    return index;
}

void ipt_map(pid_t pid, vaddr_t v, paddr_t p)
{

    KASSERT(pid != 0);
    KASSERT(v != 0);

    /* To be sure it is aligned */
    /* Page number */
    v &= PAGE_FRAME;
    /* To be sure it is aligned */
    p &= PAGE_FRAME;

    /* Index in IPT based on paddr */
    unsigned int i = (unsigned int)p / PAGE_SIZE;

    spinlock_acquire(&free_ipt);
    if (ipt[i].status == ABSENT)
    {
        ipt[i].p_pid = pid;
        ipt[i].page_number = v;
        ipt[i].status = PRESENT;
        ipt[i].protection = IPT_E_RW;
    }
    spinlock_release(&free_ipt);

    /* Push on queue_fifo and check on swapfile */
    ipt_queue_fifo_push(i);
}

paddr_t ipt_fault(uint32_t faulttype)
{
    unsigned int i;
    switch (faulttype)
    {
    case INVALID_MAP:
        panic("Invalid input address for paging\n");

    case NOT_MAPPED:
    {
        /* Wr're trying to access to a not mapped page */
        /* Let's update it in memory and so in page table */

        paddr_t p = alloc_upage();

        if (p == 0)
        {
            /* there's not enough space -> substitute */
            /* PAGE REPLACEMENT */
            i = ipt_queue_fifo_pop();
            free_upage(i * PAGE_SIZE);
            /* new address */
            p = alloc_upage();
        }

        /* p will be mapped in ipt_translate() */
        return p;
    }
    break;
    default:
        break;
    }
    return 0;
}

paddr_t ipt_translate(pid_t pid, vaddr_t v)
{
    unsigned i;
    KASSERT(pid != 0);

    /* Alignment of virtual address to page */
    /* Page number */
    v &= PAGE_FRAME;

    if (v >= USERSTACK)
    {
        ipt_fault(INVALID_MAP);
    }

    /* Search of the virtual address inside the IPT */
    bool found = false;
    i = ipt_search(pid, v, &found);

    /* Frame number */
    paddr_t p;

    if (found)
    {
        p = PAGE_SIZE * i;
    }
    else
    {
        p = ipt_fault(NOT_MAPPED);
        ipt_map(pid, v, p);
    }

    return p;
}

void ipt_swap_push(struct ipt_entry *ipt_e)
{
    KASSERT(ipt_e->p_pid!=0);
    KASSERT(ipt_e->page_number!=0);
    KASSERT(ipt_e->protection==IPT_E_RW);

    paddr_t p = ipt_translate(ipt_e->p_pid, ipt_e->page_number);

    swap_in(p);
}

/* This just invalidates the ipt_entry in SWAPFILE. */
/* If it is not present the ipt_entry, don't care. */
void ipt_swap_pop(struct ipt_entry *ipt_e)
{
    KASSERT(ipt_e->p_pid!=0);
    KASSERT(ipt_e->page_number!=0);
    KASSERT(ipt_e->protection==IPT_E_RW);

    paddr_t p = ipt_translate(ipt_e->p_pid, ipt_e->page_number);
    
    swap_out(p);
}