#include <vm_tlb.h>

#include <types.h>
#include <mips/tlb.h>

int tlb_get_rr_victim(){
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}