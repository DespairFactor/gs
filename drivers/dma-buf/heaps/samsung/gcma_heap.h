// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF GCMA heap
 *
 */

#ifndef __GCMA_HEAP_H
#define __GCMA_HEAP_H

struct gcma_arbitrator;
struct device;

struct gcma_heap {
	struct device *dev;
	struct gcma_arbitrator *arb;
#ifdef CONFIG_SYSFS
        struct gcma_heap_stat *stat;
#endif
        bool flexible_alloc;
};


struct page *gcma_alloc(struct gcma_heap *gcma_heap, unsigned long size);
void gcma_free(struct gcma_heap *gcma_heap, struct page *page);

#endif
