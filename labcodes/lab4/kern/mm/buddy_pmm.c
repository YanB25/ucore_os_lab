#include <pmm.h>
#include <string.h>
#include <buddy_pmm.h>
#include <x86.h>
#include <stdio.h>
#include <logging.h>

static struct buddy_pmm_meta pmm_meta;

#define buddy_nr_free (pmm_meta.nr_free)
#define buddy_rollup_nr_free (pmm_meta.rollup_nr_free)
#define cont (pmm_meta.cont_log2)
#define alloctree_init (pmm_meta.alloctree_init)

/* define logging macros */
#define mmu_debugf(fmt, ...) debugf(MMU, fmt, ##__VA_ARGS__)
#define mmu_warnf(fmt, ...) warnf(MMU, fmt, ##__VA_ARGS__)
#define mmu_infof(fmt, ...) infof(MMU, fmt, ##__VA_ARGS__)
#define mmu_errorf(fmt, ...) errorf(MMU, fmt, ##__VA_ARGS__)
#define mmu_panicf(fmt, ...) panicf(MMU, fmt, ##__VA_ARGS__)

static inline void
report(struct Page* page, size_t n, char* str) {
    if (page == NULL) {
        mmu_infof("%s\nPage Fault: NULL %u page(s)\n", str, n);
        return;
    }
    struct Page *beg = page, *end = page + n - 1;
    uint32_t nbeg = page2ppn(beg), nend = page2ppn(end);
    uintptr_t beg_addr = page2pa(beg), end_addr = page2pa(end);
    if (n == 1) {
        mmu_infof("%s\n%u page(%u) @0x%08x\n", str, n, nbeg, beg_addr);
    } else {
        mmu_infof("%s\n%u pages(%u - %u) @0x%08x - @0x%08x\n", str, n, nbeg, nend, beg_addr, end_addr);
    }
}

static void
buddy_pmm_init(void) {
    buddy_nr_free = 0;
    buddy_rollup_nr_free = 0;
    alloctree_init = 0; /* not init until first alloc. */
    cont[0] = 0;
}

static void 
buddy_init_memmap(struct Page* base, size_t n) {
    /* reporting */
    assert(n > 0);

    report(base, n, "buddy_init_memmap");
    pmm_meta.pnn_offset = page2ppn(base);

    for (struct Page* p = base; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    buddy_nr_free += n;
}

static void
do_init_alloctree(size_t idx, uint32_t ncap, uint32_t nfree) {
    if (ncap == 0) return;

    uint32_t next_cap = ncap / 2;
    uint32_t lfree = min(nfree, next_cap);
    uint32_t rfree = nfree - lfree;

    cont[idx] = nfree;
    do_init_alloctree(LEFT(idx), next_cap, lfree);
    do_init_alloctree(RIGHT(idx), next_cap, rfree);
}

static void
buddy_init_alloctree(void) {
    alloctree_init = 1;

    buddy_rollup_nr_free = ROLLUP_POW2(buddy_nr_free);
    mmu_debugf("nr_free = %u, roll up nr_free = %u\n", buddy_nr_free, buddy_rollup_nr_free);
    do_init_alloctree(1, buddy_rollup_nr_free, buddy_nr_free);

    RAW_LOGGING
    mmu_infof("showing cont info ...\n");
    for (int i = 1; i < 20; ++i) {
        mmu_infof("%d:%u ", i, cont[i]);
    }
    mmu_infof("\n");
    ENDR
}

static struct Page*
do_alloc_pages(size_t idx, size_t n) {
    assert(idx > 0 && idx < BUDDY_NPG);
    RAW_LOGGING mmu_debugf("calling alloc pages, idx=%u, cont=%u\n", idx, cont[idx]); ENDR

    /* quick match */
    if (idx2nodesize(idx) == n && !USED(idx)) {
        cont[idx] = 0;
        return idx2page(idx);
    }
    /* early stop */
    if (cont[idx] < n) {
        return NULL;
    }

    assert(cont[idx] >= n);

    struct Page* lpage = do_alloc_pages(LEFT(idx), n);
    if (lpage) {
        cont[idx] = max(cont[LEFT(idx)], cont[RIGHT(idx)]);
        return lpage;
    }
    struct Page* rpage = do_alloc_pages(RIGHT(idx), n);
    if (rpage) {
        cont[idx] = max(cont[LEFT(idx)], cont[RIGHT(idx)]);
        return rpage;
    }
    assert(0); /* never reach here */
    return NULL;
}


static struct Page *
buddy_alloc_pages(size_t n) {
    if (!alloctree_init) {
        buddy_init_alloctree();
    }
    uint32_t rollup_n = ROLLUP_POW2(n);
    mmu_debugf("start to do alloc page(s)\n");
    struct Page* p = do_alloc_pages(HEAD, rollup_n);
    report(p, rollup_n, "buddy_alloc_pages");
    return p;
}

static void
do_free_pages_backwords(size_t idx) {
    assert(idx > 0 && idx < BUDDY_NPG);
    if (!last_layer(idx)) {
        if (NOT_USED(LEFT(idx)) && NOT_USED(RIGHT(idx))) {
            cont[idx] = cont[LEFT(idx)] + cont[RIGHT(idx)];
        } else {
            cont[idx] = max(cont[LEFT(idx)], cont[RIGHT(idx)]);
        }
    }
    RAW_LOGGING mmu_debugf("backwords with idx=%u, cont=%u\n", idx, cont[idx]); ENDR
    if (!first_layer(idx)) {
        do_free_pages_backwords(PAR(idx));
    }
}


/**
 * buddy_free_pages REQUIRED allocs and frees in the EXACTLY
 * symmetrical way.
 * e.g. not allow to allc(2), alloc(2) then free(4).
 */