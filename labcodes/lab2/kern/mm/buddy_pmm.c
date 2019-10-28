#include <pmm.h>
#include <string.h>
#include <buddy_pmm.h>
#include <x86.h>
#include <stdio.h>

static struct buddy_pmm_meta pmm_meta;

#define buddy_nr_free (pmm_meta.nr_free)
#define buddy_rollup_nr_free (pmm_meta.rollup_nr_free)
#define cont (pmm_meta.cont_log2)
#define alloctree_init (pmm_meta.alloctree_init)


static inline void
report(struct Page* page, size_t n, char* str) {
    cprintf("[MMU] %s\n", str);
    if (page == NULL) {
        cprintf("Page Fault: NULL %u page(s)\n\n", n);
        return;
    }
    struct Page *beg = page, *end = page + n - 1;
    uint32_t nbeg = page2ppn(beg), nend = page2ppn(end);
    uintptr_t beg_addr = page2pa(beg), end_addr = page2pa(end);
    if (n == 1) {
        cprintf("%u page(%u) @0x%08x\n\n", n, nbeg, beg_addr);
    } else {
        cprintf("%u pages(%u - %u) @0x%08x - @0x%08x\n\n", n, nbeg, nend, beg_addr, end_addr);
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
    cprintf("[MMU] nr_free = %u, roll up nr_free = %u\n", buddy_nr_free, buddy_rollup_nr_free);
    do_init_alloctree(1, buddy_rollup_nr_free, buddy_nr_free);
    cprintf("[MMU] showing cont info ...\n");
    for (int i = 1; i < 20; ++i) {
        cprintf("\t %d:%u ", i, cont[i]);
    }
    cprintf("\n");
}

static struct Page*
do_alloc_pages(size_t idx, size_t n) {
    assert(idx > 0 && idx < BUDDY_NPG);
    cprintf("[debug] calling alloc pages, idx=%u, cont=%u\n", idx, cont[idx]);

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
    cprintf("\t backwords with idx=%u, cont=%u\n", idx, cont[idx]);
    if (!first_layer(idx)) {
        do_free_pages_backwords(PAR(idx));
    }
}


/**
 * buddy_free_pages REQUIRED allocs and frees in the EXACTLY
 * symmetrical way.
 * e.g. not allow to allc(2), alloc(2) then free(4).
 */
static void
buddy_free_pages(struct Page* base, size_t n) {
    assert(alloctree_init);

    if (base == NULL || n == 0) return;
    assert(n > 0);

    uint32_t rollup_n = ROLLUP_POW2(n);

    cprintf("\t nodesize=%u, depth=%u, debug=%u, fst=%u, buddy_rullup_nr_free=%u\n", rollup_n, nodesize2depth(rollup_n), buddy_rollup_nr_free/rollup_n, nodesize2fstidx(rollup_n), buddy_rollup_nr_free);

    bool found = 0;
    size_t cur_idx = HEAD;
    ppn_t bppn = page2ppn(base);
    while (!out_of_range(cur_idx)) {
        uint32_t nodesize = idx2nodesize(cur_idx);
        ppn_t ppn = idx2ppn(cur_idx);
        cprintf("\t searching idx=%u, n=%u, bppn=%u, nodesize=%u, ppn=%u\n", cur_idx, n, bppn, nodesize, ppn);
        if (nodesize == n && ppn == bppn) {
            /* parameter match */
            if (cont[cur_idx] == 0) {
                cprintf("\t freeing %u, backward...\n", cur_idx);
                cont[cur_idx] = n;
                do_free_pages_backwords(cur_idx);
                found = 1;
                break;
            } else {
                cprintf("\t WARN: cont[cur_idx]=%u not ZERO\n", cont[cur_idx]);
            }
        } else if (idx2ppn(RIGHT(cur_idx)) > bppn) {
            assert(idx2ppn(LEFT(cur_idx)) <= bppn);
            cur_idx = LEFT(cur_idx);
        } else {
            /* idx2ppn(RIGHT(idx)) <= ppn */
            cur_idx = RIGHT(cur_idx);
        }
    }

    // for (size_t idx = beg_idx; idx < end_idx; ++idx) {
    //     cprintf("cont[idx] = %u, idx2lgppn=%u, page2lgppn=%u\n", cont[idx], idx2lgppn(idx), page2lgppn(base));
    //     if (cont[idx] == 0 && idx2lgppn(idx) == page2lgppn(base)) {
    //         cont[idx] = n;
    //         do_free_pages_backwords(idx);
    //         found = 1;
    //         break;
    //     }
    // }
    cprintf("end\n");
    if (!found) {
        report(base, rollup_n, "WARN, fails to free pages.");
    } else {
        report(base, rollup_n, "buddy_free_pages.");
    }

}

static size_t
buddy_nr_free_pages(void) {
    return buddy_nr_free;
}

static void
one_page_check(void) {
    uint32_t nr_free_store = buddy_nr_free;
    buddy_nr_free = 16;
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);
    cprintf("now freeing ...\n");

    free_page(p1);
    free_page(p0);
    free_page(p2);

    struct Page* page[16];
    for (int i = 0; i < 16; ++i) {
        assert((page[i] = alloc_page()) != NULL);
    }
    assert(alloc_page() == NULL);
    for (int i = 0; i < 16; ++i) {
        free_page(page[i]);
    }

    cprintf("+++++++ MMU reset ++++++++\n");
    alloctree_init = 0;
    buddy_nr_free = nr_free_store;
}

static void
pages_check() {
    cprintf("++++++++ starting pages check +++++++++\n");
    uint32_t nr_free_store = buddy_nr_free;
    buddy_nr_free = 16;

    struct Page *p0, *p1, *p2, *p3;
    assert((p0 = alloc_pages(16)) != NULL);
    assert(alloc_page() == NULL);
    free_pages(p0, 16);

    assert((p0 = alloc_pages(8)) != NULL);
    assert((p1 = alloc_pages(8)) != NULL);
    assert(alloc_page() == NULL);
    free_pages(p0, 8);
    free_pages(p1, 8);

    struct Page* page[8];
    for (int i = 0; i < 8; ++i) {
        assert((page[i] = alloc_pages(2)) != NULL);
    }
    assert(alloc_page() == NULL);
    for (int i = 0; i < 8; ++i) {
        free_pages(page[i], 2);
    }
    cprintf("--------- MMU reset --------\n");
    alloctree_init = 0;
    buddy_nr_free = nr_free_store;
}

static void
random_alloc_check(void) {
    cprintf("+++++++ starting random_alloc_check ++++++++\n");
    uint32_t nr_free_store = buddy_nr_free;
    buddy_nr_free = 16;

    struct Page *p0, *p1, *p2;
    assert((p0 = alloc_pages(7)) != NULL);
    assert((p1 = alloc_pages(3)) != NULL);
    struct Page* page[4];
    for (int i = 0; i < 4; ++i) {
        assert((page[i] = alloc_page()) != NULL);
    }
    assert(alloc_page() == NULL);

    free_pages(p1, 3);
    free_pages(p0, 7);
    for (int i = 0; i < 4; ++i) {
        free_page(page[i]);
    }
    cprintf("--------- MMU reset --------\n");
    alloctree_init = 0;
    buddy_nr_free = nr_free_store;
}

static void
rollup_alloc_check(void) {
    uint32_t nr_free_store = buddy_nr_free;
    buddy_nr_free = 9;
    cprintf("+++++++ starting rollup_alloc_check ++++++++\n");

    struct Page *p0, *p1, *p2;
    assert((p0 = alloc_pages(8)) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert(alloc_page() == NULL);
    free_page(p1);
    free_pages(p0, 8);

    assert((p0 = alloc_pages(4)) != NULL);
    assert((p1 = alloc_pages(4)) != NULL);
    assert((p2 = alloc_page()) != NULL);
    assert(alloc_page() == NULL);
    free_pages(p1, 4);
    free_page(p2);
    free_pages(p0, 4);

    struct Page* page[9];
    for (int i = 0; i < 9; ++i) {
        assert((page[i] = alloc_page()) != NULL);
    }
    assert(alloc_page() == NULL);

    for (int i = 8; i >= 0; --i) {
        free_page(page[i]);
    }

    cprintf("--------- MMU reset --------\n");
    alloctree_init = 0;
    buddy_nr_free = nr_free_store;
}

static void
buddy_pmm_check(void) {
    one_page_check();
    pages_check();
    random_alloc_check();
    rollup_alloc_check();
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_pmm_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_pmm_check,
};