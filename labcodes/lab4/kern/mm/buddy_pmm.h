#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <defs.h>
#include <pmm.h>
#include <string.h>
#include <x86.h>
#include <stdio.h>

#define BUDDY_NPG (KMEMSIZE / PGSIZE)

struct buddy_pmm_meta{
    bool alloctree_init;
    uint32_t nr_free;
    uint32_t rollup_nr_free;
    uint32_t cont_log2[BUDDY_NPG + 1];
    uint32_t pnn_offset; /* lgppn + ppn_offset = ppn (phy ppn) */
};

#endif /* !	__KERN_MM_BUDDY_PMM_H__ */