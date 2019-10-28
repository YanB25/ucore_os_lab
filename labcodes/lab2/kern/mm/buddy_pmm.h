#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>
#include <memlayout.h>

#define BUDDY_NPG (KMEMSIZE / PGSIZE)

/**
 *  LM1 return the position(n-th bit) of the left most 1-bit 
 *  NOTICE: only work for an uint32_t
 */
#define LM1(u) ({                                              \
    __typeof__ (u) __lm1_u = u;                                \
    if (__lm1_u == 0) {                                        \
        panic("call __builtin_clz for 0\n");                   \
    }                                                          \
    (32 - __builtin_clz(__lm1_u));                             \
})

/* roll an uint32_t up to a power of two */
#define ROLLUP_POW2(u) ({                                      \
    __typeof__ (u) __rollup_pow2_u = u - 1;                    \
    uint32_t __rollup_pow2_res;                                \
    if (u == 1) {                                              \
        __rollup_pow2_res = 1;                                 \
    } else {                                                   \
        __typeof__ (u) __shft = LM1(__rollup_pow2_u);          \
        __rollup_pow2_res = (1 << __shft);                     \
    }                                                          \
    __rollup_pow2_res;                                         \
})

#define HEAD (1)
#define LEFT(idx) (idx * 2)
#define RIGHT(idx) (idx * 2 + 1)
#define PAR(idx) (idx / 2)
#define BRO(idx) (idx ^ 0x1)

#define USED(idx) (cont[idx] == 0)
#define NOT_USED(idx) (cont[idx] == idx2nodesize(idx))

/** 
 * idx2depth calculate the depth of idx-node.
 * the root has depth 1 and idx 1.
 */
#define idx2depth(idx) (LM1(idx))
#define depth2nodesize(depth) (buddy_rollup_nr_free / (1 << (depth - 1)))
#define idx2nodesize(idx) (depth2nodesize(idx2depth(idx)))

#define depth2fstidx(depth) (1 << (depth - 1))
#define max_depth (LM1(buddy_rollup_nr_free))
// TODO: critical bug below
#define nodesize2depth(nodesize) (max_depth + 1 - LM1(nodesize)) 
#define nodesize2fstidx(nodesize) (depth2fstidx(nodesize2depth(nodesize)))

/** 
 * idx2lgppn convert an idx to the logic page number.
 * the lgppn starts from 0
 */
#define idx2lgppn(idx) (idx * idx2nodesize(idx) - buddy_rollup_nr_free)
#define lgppn2ppn(lgppn) (lgppn + pmm_meta.pnn_offset)
#define idx2ppn(idx) (lgppn2ppn(idx2lgppn(idx)))
#define idx2page(idx) ((idx2ppn(idx) + pages))

#define page2lgppn(page) (ppn2lgppn(page2ppn(page)))
#define ppn2lgppn(ppn) (ppn - pmm_meta.pnn_offset)

#define first_layer(idx) (idx == HEAD)
#define last_layer(idx) (idx >= buddy_rollup_nr_free)
#define out_of_range(idx) (idx >= 2 * buddy_rollup_nr_free)


extern const struct pmm_manager buddy_pmm_manager;

struct buddy_pmm_meta{
    bool alloctree_init;
    uint32_t nr_free;
    uint32_t rollup_nr_free;
    uint32_t cont_log2[BUDDY_NPG + 1];
    uint32_t pnn_offset; /* lgppn + ppn_offset = ppn (phy ppn) */
};

#endif /* !	__KERN_MM_BUDDY_PMM_H__ */