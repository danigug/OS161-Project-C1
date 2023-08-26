#ifndef _NOVAVM_H_
#define _NOVAVM_H_

#include <vm.h> 

//#include <machine/vm.h>

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

// TO DO
/* under dumbvm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */
#define NOVAVM_STACKPAGES   18

void novavm_can_sleep(void);

/* Initialization function */
//void vm_bootstrap(void);



/* Fault handling function called by trap code */
//int vm_fault(int faulttype, vaddr_t faultaddress);

/* TLB shootdown handling called from interprocessor_interrupt */
//void vm_tlbshootdown(const struct tlbshootdown *);

/* Calls the print_stats function */
void vm_shutdown(void);


#endif