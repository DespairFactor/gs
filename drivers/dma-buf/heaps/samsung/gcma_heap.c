// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF GCMA heap
 *
 */

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/kernel.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#include <linux/pfn.h>
#include <soc/google/gcma.h>

#include "samsung-dma-heap.h"
#include "gcma_heap.h"
#include "gcma_heap_sysfs.h"
#include "gcma_arbitrator.h"

#define BASE_GFP (GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP)
#define LIGHT_EFFORT_GFP  ((BASE_GFP | __GFP_NOWARN | __GFP_NORETRY) & ~__GFP_RECLAIM)
#define HARD_EFFORT_GFP BASE_GFP
/*
 * The selection of the orders used for allocation (2MB, 1MB, 64K, 4K) is designed
 * to match with the sizes often found in IOMMUs. Using high order pages instead
 * of order 0 pages can significantly improve the performance of many IOMMUs
 * by reducing TLB pressure and time spent updating page tables.
 *
 * Note: When the order is 0, the minimum allocation is PAGE_SIZE. The possible
 * page sizes for ARM devices could be 4K, 16K and 64K.
 */
#define ORDER_2M (21 - PAGE_SHIFT)
#define ORDER_1M (20 - PAGE_SHIFT)
#define ORDER_64K (16 - PAGE_SHIFT)
#define ORDER_FOR_PAGE_SIZE (0)

static const unsigned int buddy_pages_orders[] = {
	ORDER_2M, ORDER_1M, ORDER_64K, ORDER_FOR_PAGE_SIZE};
static const gfp_t buddy_pages_flags[] = {
	LIGHT_EFFORT_GFP, LIGHT_EFFORT_GFP, LIGHT_EFFORT_GFP, HARD_EFFORT_GFP};
static const unsigned int gcma_pages_orders[] = {
	ORDER_2M, ORDER_1M, ORDER_64K};

static int param_set_min_gcma_dmabuf_kb(const char *val, const struct kernel_param *kp)
{
	unsigned long threshold_kb;
	int ret;

	ret = kstrtoul(val, 0, &threshold_kb);
	if (ret)
		return ret;

	/* Validate input: limit threshold to 128MB (128 * 1024 KB) to prevent unreasonable settings. */
	if (threshold_kb > 128 * SZ_1K)
		return -EINVAL;

	*(unsigned long *)kp->arg = threshold_kb * SZ_1K;
	return 0;
}

static int param_get_min_gcma_dmabuf_kb(char *buffer, const struct kernel_param *kp)
{
	unsigned long bytes = *(unsigned long *)kp->arg;

	return sysfs_emit(buffer, "%lu\n", bytes / SZ_1K);
}

static const struct kernel_param_ops min_gcma_dmabuf_kb_ops = {
	.set = param_set_min_gcma_dmabuf_kb,
	.get = param_get_min_gcma_dmabuf_kb,
};

static unsigned long min_gcma_dmabuf_bytes = 512 * SZ_1K;
module_param_cb(min_gcma_dmabuf_kb, &min_gcma_dmabuf_kb_ops, &min_gcma_dmabuf_bytes, 0644);
MODULE_PARM_DESC(min_gcma_dmabuf_kb, "Minimum size in KB to attempt GCMA allocation");

#define MAX_SKIP_HEAPS 5
#define MAX_HEAP_NAME_LEN 32
static char skip_heaps[MAX_SKIP_HEAPS][MAX_HEAP_NAME_LEN];
static int num_skip_heaps;

static int param_set_skip_heaps(const char *val, const struct kernel_param *kp)
{
	char *str, *orig_str;
	char *p;
	int i = 0;

	if (!val)
		return -EINVAL;

	orig_str = kstrdup(val, GFP_KERNEL);
	if (!orig_str)
		return -ENOMEM;

	str = orig_str;
	num_skip_heaps = 0;

	while ((p = strsep(&str, ",")) != NULL && i < MAX_SKIP_HEAPS) {
		if (*p == '\0')
			continue;
		strscpy(skip_heaps[i], p, sizeof(skip_heaps[i]));
		i++;
	}
	num_skip_heaps = i;

	kfree(orig_str);
	return 0;
}

static int param_get_skip_heaps(char *buffer, const struct kernel_param *kp)
{
	char local_buf[256] = "";
	int i;

	for (i = 0; i < num_skip_heaps; i++) {
		strcat(local_buf, skip_heaps[i]);
		if (i < num_skip_heaps - 1)
			strcat(local_buf, ",");
	}

	return sysfs_emit(buffer, "%s\n", local_buf);
}

static const struct kernel_param_ops gcma_skip_heaps_ops = {
	.set = param_set_skip_heaps,
	.get = param_get_skip_heaps,
};

module_param_cb(gcma_skip_heaps, &gcma_skip_heaps_ops, NULL, 0444);
MODULE_PARM_DESC(gcma_skip_heaps, "Comma-separated list of heap names to skip probing (max 5)");

struct heap_pages {
  struct list_head pages_list;
  unsigned int count;
};

unsigned long dma_heap_gcma_inuse_pages(void)
{
	return atomic64_read(&inuse_pages);
}

static inline unsigned long gcma_get_size(struct page *page)
{
	return page_private(page);
}

static inline void gcma_set_size(struct page *page, unsigned long size)
{
	return set_page_private(page, size);
}

static inline bool page_is_gcma(struct page *page)
{
	return page_private(page) ? true : false;
}

struct page *gcma_alloc(struct gcma_heap *gcma_heap, unsigned long size)
{
	phys_addr_t paddr;
	unsigned long pfn;
	struct page *page = NULL;

	paddr = gcma_arbitrator_alloc(gcma_heap->arb, size);
	if (!paddr)
		return NULL;

	pfn = PFN_DOWN(paddr);
	page = phys_to_page(paddr);
	pixel_gcma_alloc_range(pfn, pfn + (size >> PAGE_SHIFT) - 1);
	gcma_set_size(page, size);
	inc_gcma_heap_stat(gcma_heap, USAGE, size);
	/*
	 * zero out pages to align with the strategy in buddy allocator GFP flag
	 */
	if (BASE_GFP & __GFP_ZERO)
		heap_page_clean(page, size);

	return page;
}

void gcma_free(struct gcma_heap *gcma_heap, struct page *page)
{
	unsigned long size, pfn;

	size = gcma_get_size(page);
	pfn = page_to_pfn(page);
	pixel_gcma_free_range(pfn, pfn + (size >> PAGE_SHIFT) - 1);
	gcma_arbitrator_free(gcma_heap->arb, page_to_phys(page), size);
}

static void free_gcma_heap_page(struct gcma_heap *gcma_heap, struct page *page)
{
	if (unlikely(!page))
		return;

	if (page_is_gcma(page)) {
		gcma_free(gcma_heap, page);
		dec_gcma_heap_stat(gcma_heap, USAGE, gcma_get_size(page));
	} else {
		unsigned int order = compound_order(page);
		__free_pages(page, order);
		dma_heap_dec_inuse(1 << order);
		dec_gcma_heap_stat(gcma_heap, BUDDY, PAGE_SIZE << order);
	}
}

/*
 * 1. Try GCMA allocation
 * 2. Try Buddy allocator (light effort for high orders, hard for order-0)
 */
static struct page *alloc_largest_available(struct gcma_heap *gcma_heap,
					    unsigned long size,
					    unsigned int max_order)
{
	struct page *page = NULL;
	int i;

	if (size >= min_gcma_dmabuf_bytes) {
		for (i = 0; i < ARRAY_SIZE(gcma_pages_orders); i++) {
			unsigned long gcma_size = PAGE_SIZE << gcma_pages_orders[i];

			if (size < gcma_size)
				continue;

			page = gcma_alloc(gcma_heap, gcma_size);
			if (page)
				goto out;
		}
	}

	for (i = 0; i < ARRAY_SIZE(buddy_pages_orders); i++) {
		unsigned long buddy_size = PAGE_SIZE << buddy_pages_orders[i];
		gfp_t flags = buddy_pages_flags[i];

		if (size < buddy_size && i != ARRAY_SIZE(buddy_pages_orders) - 1)
			continue;
		if (max_order < buddy_pages_orders[i])
			continue;

		if (flags == HARD_EFFORT_GFP)
			inc_gcma_heap_stat(gcma_heap, ALLOCSTALL,
					   PAGE_SIZE << buddy_pages_orders[i]);

		page = alloc_pages(flags, buddy_pages_orders[i]);
		if (page) {
			inc_gcma_heap_stat(gcma_heap, BUDDY, PAGE_SIZE << buddy_pages_orders[i]);
			goto out;
		}
	}
out:
	if (page && !page_is_gcma(page))
		dma_heap_inc_inuse(1 << compound_order(page));
	return page;
}

static int allocate_flexible_pages(struct gcma_heap *gcma_heap, unsigned long len,
					    struct heap_pages *heap_pages)
{
	struct page *page, *tmp_page;
	unsigned long size_remaining = len;
	unsigned int max_order = buddy_pages_orders[0];
	unsigned int count = 0;
	int ret = 0;

	while (size_remaining > 0) {
		unsigned long allocated_size;
		/*
		 * Avoid trying to allocate memory if the process
		 * has been killed by SIGKILL
		 */
		if (fatal_signal_pending(current)) {
			pr_err("Fatal signal pending pid #%d", current->pid);
			ret = -EINTR;
			goto free_flexible_pages;
		}

		page = alloc_largest_available(gcma_heap, size_remaining, max_order);
		if (!page) {
			ret = -ENOMEM;
			goto free_flexible_pages;
		}

		list_add_tail(&page->lru, &heap_pages->pages_list);
		allocated_size = page_is_gcma(page) ? gcma_get_size(page) : page_size(page);
		if (allocated_size > size_remaining)
			size_remaining = 0;
		else
			size_remaining -= allocated_size;
		if (!page_is_gcma(page))
			max_order = compound_order(page);
		count++;
	}
	heap_pages->count = count;
	goto out_flexible_alloc;

free_flexible_pages:
	list_for_each_entry_safe(page, tmp_page, &heap_pages->pages_list, lru) {
		list_del(&page->lru);
		free_gcma_heap_page(gcma_heap, page);
	}
out_flexible_alloc:
	return ret;
}

static int allocate_fixed_pages(struct gcma_heap *gcma_heap, unsigned long len,
					    struct heap_pages *heap_pages)
{
	struct page *page = gcma_alloc(gcma_heap, len);
	if (page) {
		list_add_tail(&page->lru, &heap_pages->pages_list);
		heap_pages->count = 1;
		return 0;
	}
	return -ENOMEM;
}

static struct dma_buf *gcma_heap_allocate(struct dma_heap *heap, unsigned long len,
					    u32 fd_flags, u64 heap_flaga)
{
	struct samsung_dma_heap *samsung_dma_heap = dma_heap_get_drvdata(heap);
	struct gcma_heap *gcma_heap = samsung_dma_heap->priv;
	bool is_secure_heap = dma_heap_flags_protected(samsung_dma_heap->flags);
	bool flexible_alloc = gcma_heap->flexible_alloc;
	struct samsung_dma_buffer *buffer;
	struct scatterlist *sg;
	struct dma_buf *dmabuf;
	unsigned int alignment = samsung_dma_heap->alignment;
	struct page *page, *tmp_page;
	struct heap_pages heap_pages;
	int ret = -ENOMEM;

	/* We don't support a secure heap with the flexible_alloc strategy */
	if (dma_heap_flags_video_aligned(samsung_dma_heap->flags))
		len = dma_heap_add_video_padding(len);

	if (len / PAGE_SIZE > totalram_pages() / 2) {
		pr_err("pid %d requested too large allocation of size %lu from %s heap\n",
		       current->pid, len, samsung_dma_heap->name);
		return ERR_PTR(ret);
	}

	INIT_LIST_HEAD(&heap_pages.pages_list);
	len = ALIGN(len, alignment);

	if (flexible_alloc) {
		ret = allocate_flexible_pages(gcma_heap, len, &heap_pages);
	} else {
		ret = allocate_fixed_pages(gcma_heap, len, &heap_pages);
	}

	if (ret)
		goto out;

	buffer = samsung_dma_buffer_alloc(samsung_dma_heap, len, heap_pages.count);
	if (IS_ERR(buffer)) {
		ret = PTR_ERR(buffer);
		goto free_buffer;
	}

	sg = buffer->sg_table.sgl;
	list_for_each_entry_safe(page, tmp_page, &heap_pages.pages_list, lru) {
		sg_set_page(sg, page,
			   page_is_gcma(page) ? gcma_get_size(page) : page_size(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	heap_cache_flush(buffer);

	if (is_secure_heap) {
		unsigned long paddr = page_to_phys(sg_page(buffer->sg_table.sgl));

		buffer->priv = samsung_dma_buffer_protect(
				buffer, len, heap_pages.count, paddr);
		if (IS_ERR(buffer->priv)) {
			ret = PTR_ERR(buffer->priv);
			buffer->priv = NULL;
			goto free_export;
		}
	}

	dmabuf = samsung_export_dmabuf(buffer, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_export;
	}

	return dmabuf;

free_export:
	if (is_secure_heap ? !samsung_dma_buffer_unprotect(buffer) : 1)
		for_each_sgtable_sg(&buffer->sg_table, sg, heap_pages.count)
			free_gcma_heap_page(gcma_heap, sg_page(sg));
free_buffer:
	list_for_each_entry_safe(page, tmp_page, &heap_pages.pages_list, lru) {
		list_del(&page->lru);
		free_gcma_heap_page(gcma_heap, page);
	}

	samsung_dma_buffer_free(buffer);
out:
	pr_err("failed to allocate from %s heap, size %lu ret %d",
		      samsung_dma_heap->name, len, ret);

	return ERR_PTR(ret);
}

static void gcma_heap_release(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *samsung_dma_heap = buffer->heap;
	struct gcma_heap *gcma_heap = samsung_dma_heap->priv;
	bool is_secure_heap = dma_heap_flags_protected(samsung_dma_heap->flags);
	int ret = 0;

	/* We don't support a secure heap with the flexible_alloc strategy */
	if (is_secure_heap)
		ret = samsung_dma_buffer_unprotect(buffer);

	if (!ret) {
		struct sg_table *table;
		struct scatterlist *sg;
		int i;

		gcma_heap = buffer->heap->priv;
		table = &buffer->sg_table;
		/*
		 * free @page directly without caching it to page pool now as after a
		 * long time use, we won't have high order pages anyway.
		 */
		for_each_sgtable_sg(table, sg, i)
			free_gcma_heap_page(gcma_heap, sg_page(sg));
	}
	samsung_dma_buffer_free(buffer);
}

static const struct dma_heap_ops gcma_heap_ops = {
	.allocate = gcma_heap_allocate,
};

static void gcma_arbitrator_destroy_action(void *data)
{
	gcma_arbitrator_destroy(data);
}

static int gcma_heap_probe(struct platform_device *pdev)
{
	struct gcma_heap *gcma_heap;
	const char *heap_name;
	bool is_secure_heap;
	int ret;
	int i;

	if (of_property_read_string(pdev->dev.of_node, "dma-heap,name", &heap_name))
		heap_name = pdev->name;

	for (i = 0; i < num_skip_heaps; i++) {
		if (strcmp(heap_name, skip_heaps[i]) == 0 ||
		    strcmp(pdev->name, skip_heaps[i]) == 0) {
			dev_info(&pdev->dev, "Skipping heap %s as requested\n", heap_name);
			return 0;
		}
	}

	gcma_heap = devm_kzalloc(&pdev->dev, sizeof(*gcma_heap), GFP_KERNEL);
	if (!gcma_heap)
		return -ENOMEM;

	gcma_heap->dev = &pdev->dev;
	gcma_heap->arb = gcma_arbitrator_create(gcma_heap);
	if (IS_ERR(gcma_heap->arb)) {
		perrdev(&pdev->dev, "Failed to create GCMA arbitrator\n");
		return PTR_ERR(gcma_heap->arb);
	}

	ret = devm_add_action_or_reset(&pdev->dev, gcma_arbitrator_destroy_action, gcma_heap->arb);
	if (ret)
		return ret;

	gcma_heap->flexible_alloc =
		of_property_read_bool(pdev->dev.of_node,"dma-heap-gcam,fleixble-alloc");

	is_secure_heap = of_property_read_bool(pdev->dev.of_node,"dma-heap,secure");

	if (is_secure_heap && gcma_heap->flexible_alloc) {
		perrfn("Don't support a secure heap with a flexible_alloc strategy");
		return -EPERM;
	}

	ret = samsung_heap_add(&pdev->dev, gcma_heap, gcma_heap_release,
			       &gcma_heap_ops);
	if (ret == -ENODEV)
		return 0;

	register_heap_sysfs(gcma_heap, pdev->name);

	return ret;
}

static const struct of_device_id gcma_heap_of_match[] = {
	{ .compatible = "google,dma-heap-gcma", },
	{ },
};
MODULE_DEVICE_TABLE(of, gcma_heap_of_match);

static struct platform_driver gcma_heap_driver = {
	.driver		= {
		.name	= "google,dma-heap-gcma",
		.of_match_table = gcma_heap_of_match,
	},
	.probe		= gcma_heap_probe,
};

int __init gcma_dma_heap_init(void)
{
	gcma_heap_sysfs_init();
	return platform_driver_register(&gcma_heap_driver);
}

void gcma_dma_heap_exit(void)
{
	platform_driver_unregister(&gcma_heap_driver);
}
