// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF GCMA GSA secure backend ops
 *
 * Uses a local_pool (gen_pool) for sub-allocation from a heap-wide
 * memory region that is protected via GSA IPC.
 */

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/pfn.h>
#include <linux/samsung-secure-iova.h>
#include <linux/gsa/gsa_ipc.h>
#include <soc/google/gcma.h>

#include "samsung-dma-heap.h"
#include "gcma_heap.h"
#include "gcma_heap_sysfs.h"
#include "gcma_arbitrator.h"

/**
 * struct gcma_gsa_priv - Private data for GSA secure backend
 * @local_pool:   Memory pool for GSA sub-allocations
 * @usage_count:  Active client count for on-demand protection
 * @gsa_mem_id:   The GSA handle for the protected memory region
 * @protect_lock: Serializes heap protection state transitions
 * @base:         Physical base address of the heap region
 * @size:         Size of the heap region in bytes
 * @iova_base:    Base IOVA mapped for the GSA device
 * @sg_table:     Scatterlist table representing the heap memory
 * @gsa_ipc_dev:  Device pointer for GSA IPC communications
 */
struct gcma_gsa_priv {
	struct gen_pool *local_pool;
	refcount_t usage_count;
	u64 gsa_mem_id;
	struct mutex protect_lock;
	phys_addr_t base;
	size_t size;
	u32 iova_base;
	struct sg_table sg_table;
	struct device *gsa_ipc_dev;
};

static atomic_t gsa_dma_buf_registered_count = ATOMIC_INIT(0);

/* Forward declarations — FFA callbacks used in heap_init */
static u64 gsa_dma_buf_get_ffa_tag(struct dma_buf *dma_buf);
static int gsa_dma_buf_get_shared_mem_id(struct dma_buf *dma_buf,
					 u64 *id, u64 *poff);

struct gcma_gsa_buffer_priv {
	struct buffer_prot_info protdesc;
	unsigned long mem_id_offset;
};

/*
 * The GSA backend protects the entire heap region at once via heap_protect().
 * These callbacks only set up and free per-buffer protection descriptors.
 */
static void *gcma_gsa_buffer_prot_desc_init(struct samsung_dma_buffer *buffer,
				     unsigned int chunk_size,
				     unsigned int nr_pages,
				     unsigned long paddr)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct gcma_heap *gcma_heap = heap->priv;
	struct gcma_gsa_priv *priv = gcma_heap->priv;
	struct gcma_gsa_buffer_priv *gsa_priv;
	struct buffer_prot_info *protdesc;
	unsigned int protalign = heap->alignment;
	unsigned long phys_offset;

	gsa_priv = kzalloc(sizeof(*gsa_priv), GFP_KERNEL);
	if (!gsa_priv)
		return ERR_PTR(-ENOMEM);

	protdesc = &gsa_priv->protdesc;
	protdesc->chunk_count = nr_pages;
	protdesc->flags = heap->protection_id;
	protdesc->chunk_size = ALIGN(chunk_size, protalign);
	protdesc->bus_address = paddr;

	phys_offset = paddr - priv->base;
	protdesc->dma_addr = priv->iova_base + phys_offset;
	buffer->mem_id = priv->gsa_mem_id;

	gsa_priv->mem_id_offset = phys_offset;

	return protdesc;
}

static int gcma_gsa_buffer_prot_desc_free(struct samsung_dma_buffer *buffer)
{
	struct buffer_prot_info *protdesc = buffer->priv;
	struct samsung_dma_heap *heap = buffer->heap;
	struct gcma_gsa_buffer_priv *gsa_priv;

	if (!protdesc || !heap)
		return 0;

	gsa_priv = container_of(protdesc, struct gcma_gsa_buffer_priv, protdesc);
	kfree(gsa_priv);
	return 0;
}

/* Heap-wide protect/unprotect */
static int heap_protect(struct gcma_heap *gcma_heap,
			struct samsung_dma_heap *samsung_heap)
{
	struct gcma_gsa_priv *priv = gcma_heap->priv;
	struct device *gsa_dev = priv->gsa_ipc_dev;
	u64 tag;
	struct page *base_page = NULL;
	int ret;
	unsigned int heap_align = max_t(u32, samsung_heap->alignment, PAGE_SIZE);
	unsigned long start_pfn, end_pfn, num_pfns;
	struct sg_table *sg_table = &priv->sg_table;
	phys_addr_t paddr;

	paddr = gcma_arbitrator_alloc(gcma_heap->arb, priv->size);
	if (!paddr) {
		perrfn("Heap %s: arbitrator alloc failed, size %zu",
		       samsung_heap->name, priv->size);
		return -ENOMEM;
	}

	/* Register the entire range with GCMA tracking */
	start_pfn = PFN_DOWN(priv->base);
	num_pfns = priv->size >> PAGE_SHIFT;
	end_pfn = start_pfn + num_pfns - 1;
	pixel_gcma_alloc_range(start_pfn, end_pfn);

	/* Allocate a single large IOVA range for the heap */
	priv->iova_base = secure_iova_alloc(priv->size, heap_align);
	if (!priv->iova_base) {
		perrfn("Heap %s: secure_iova_alloc failed size %zu",
		       samsung_heap->name, priv->size);
		ret = -ENOMEM;
		goto err_free_gcma_range;
	}

	/* Setup SG table for the entire heap range */
	ret = sg_alloc_table(sg_table, 1, GFP_KERNEL);
	if (ret)
		goto err_free_iova;

	base_page = phys_to_page(priv->base);
	sg_set_page(sg_table->sgl, base_page, priv->size, 0);

	/* Prepare the tag with the BASE IOVA and protection ID */
	tag = (u64)samsung_heap->protection_id | ((u64)(priv->iova_base) << 32);

	/* Lend the memory */
	ret = gsa_transfer_memory(gsa_dev, &priv->gsa_mem_id,
				  sg_table->sgl, sg_table->orig_nents,
				  PAGE_KERNEL, tag, true /* lend */);
	if (ret) {
		perrfn("Heap %s: gsa_transfer_memory failed: %d",
		       samsung_heap->name, ret);
		goto err_free_sg_table;
	}

	pr_info("GCMA heap %s: range [0x%pa - 0x%zx] protected, IOVA base 0x%x, gsa_handle 0x%llx\n",
		samsung_heap->name, &priv->base, priv->size,
		priv->iova_base, priv->gsa_mem_id);

	return 0;

err_free_sg_table:
	sg_free_table(sg_table);
err_free_iova:
	secure_iova_free(priv->iova_base, priv->size);
	priv->iova_base = 0;
err_free_gcma_range:
	pixel_gcma_free_range(start_pfn, end_pfn);
	gcma_arbitrator_free(gcma_heap->arb, paddr, priv->size);
	return ret;
}

static int heap_unprotect(struct gcma_heap *gcma_heap,
			  struct samsung_dma_heap *samsung_heap)
{
	struct gcma_gsa_priv *priv = gcma_heap->priv;
	struct device *gsa_dev = priv->gsa_ipc_dev;
	int ret;
	unsigned long start_pfn, end_pfn, num_pfns;
	struct sg_table *sg_table = &priv->sg_table;

	/* Reclaim memory from GSA */
	ret = gsa_reclaim_memory(gsa_dev, priv->gsa_mem_id, sg_table->sgl, 1);
	if (ret) {
		perrfn("Heap %s: gsa_reclaim_memory failed: %d, handle 0x%llx",
		       samsung_heap->name, ret, priv->gsa_mem_id);
		/*
		 * Intentionally leak the heap if unprotection fails.
		 * Returning the memory to the non-secure world while GSA
		 * might still hold it is a critical security/stability flaw.
		 */
	} else {
		pr_info("GCMA heap %s: range [0x%pa - 0x%zx] unprotected\n",
			samsung_heap->name, &priv->base, priv->size);
		if (priv->iova_base) {
			secure_iova_free(priv->iova_base, priv->size);
			priv->iova_base = 0;
		}

		/* Unregister from GCMA tracking */
		start_pfn = PFN_DOWN(priv->base);
		num_pfns = priv->size >> PAGE_SHIFT;
		end_pfn = start_pfn + num_pfns - 1;
		pixel_gcma_free_range(start_pfn, end_pfn);

		gcma_arbitrator_free(gcma_heap->arb, priv->base, priv->size);
	}
	sg_free_table(sg_table);
	priv->gsa_mem_id = 0;

	return ret;
}

/* Heap usage tracking */
static int gcma_gsa_heap_get(struct gcma_heap *gcma_heap,
			     struct samsung_dma_heap *samsung_heap)
{
	struct gcma_gsa_priv *priv = gcma_heap->priv;
	int ret = 0;

	/* Fast path: heap is already protected */
	if (refcount_inc_not_zero(&priv->usage_count))
		return 0;

	/* Slow path: protect the heap (first user) */
	mutex_lock(&priv->protect_lock);

	if (refcount_inc_not_zero(&priv->usage_count)) {
		mutex_unlock(&priv->protect_lock);
		return 0;
	}

	ret = heap_protect(gcma_heap, samsung_heap);
	if (!ret)
		refcount_set(&priv->usage_count, 1);

	mutex_unlock(&priv->protect_lock);
	return ret;
}

static void gcma_gsa_heap_put(struct gcma_heap *gcma_heap,
			      struct samsung_dma_heap *samsung_heap)
{
	struct gcma_gsa_priv *priv = gcma_heap->priv;

	if (refcount_dec_and_mutex_lock(&priv->usage_count, &priv->protect_lock)) {
		heap_unprotect(gcma_heap, samsung_heap);
		mutex_unlock(&priv->protect_lock);
	}
}

/* Page allocation */
static int gcma_gsa_alloc(struct samsung_dma_heap *samsung_heap, unsigned long size,
			  struct heap_pages *pages)
{
	struct gcma_heap *heap = samsung_heap->priv;
	struct gcma_gsa_priv *priv = heap->priv;
	phys_addr_t paddr;
	struct page *page;
	int ret;

	/* On-demand protection: protect heap on first user */
	ret = gcma_gsa_heap_get(heap, samsung_heap);
	if (ret)
		return ret;

	if (!priv->local_pool) {
		ret = -ENOMEM;
		goto heap_put;
	}

	paddr = gen_pool_alloc(priv->local_pool, size);
	if (!paddr) {
		ret = -ENOMEM;
		goto heap_put;
	}

	page = phys_to_page(paddr);
	gcma_set_size(page, size);
	inc_gcma_heap_stat(heap, USAGE, size);

	list_add_tail(&page->lru, &pages->pages_list);
	pages->count = 1;
	return 0;

heap_put:
	gcma_gsa_heap_put(heap, samsung_heap);
	return ret;
}

static void gcma_gsa_free(struct samsung_dma_heap *samsung_heap, struct page *page)
{
	struct gcma_heap *heap = samsung_heap->priv;
	struct gcma_gsa_priv *priv = heap->priv;
	unsigned long size;

	if (unlikely(!page) || unlikely(!priv->local_pool))
		return;

	size = gcma_get_size(page);
	gen_pool_free(priv->local_pool, page_to_phys(page), size);
	dec_gcma_heap_stat(heap, USAGE, size);

	/* Release heap-wide protection on last user */
	gcma_gsa_heap_put(heap, samsung_heap);
}

/* Lifecycle */
static int gcma_gsa_heap_init(struct gcma_heap *heap, struct platform_device *pdev)
{
	struct gcma_gsa_priv *priv;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	struct platform_device *ipc_pdev;
	struct device_node *ipc_dev_np;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/* Read memory-region from DT */
	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!rmem_np)
		return -ENODEV;

	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		perrdev(&pdev->dev, "memory-region handle not found");
		return -ENODEV;
	}

	/* Find GSA IPC device */
	ipc_dev_np = of_parse_phandle(pdev->dev.of_node, "gsa-ipc-device", 0);
	if (!ipc_dev_np) {
		perrdev(&pdev->dev, "gsa-ipc-device handle not found");
		return -ENODEV;
	}

	ipc_pdev = of_find_device_by_node(ipc_dev_np);
	of_node_put(ipc_dev_np);
	if (!ipc_pdev) {
		perrdev(&pdev->dev, "gsa ipc device not found");
		return -ENODEV;
	}

	/* Create local_pool */
	priv->local_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!priv->local_pool) {
		perrdev(&pdev->dev, "local_pool creation failed");
		return -ENOMEM;
	}

	ret = gen_pool_add(priv->local_pool, rmem->base, rmem->size, -1);
	if (ret) {
		perrdev(&pdev->dev, "local_pool add failed");
		gen_pool_destroy(priv->local_pool);
		return ret;
	}

	refcount_set(&priv->usage_count, 0);
	mutex_init(&priv->protect_lock);
	priv->base = rmem->base;
	priv->size = rmem->size;
	priv->gsa_mem_id = 0;
	priv->iova_base = 0;
	priv->gsa_ipc_dev = &ipc_pdev->dev;

	heap->priv = priv;

	/* Register GSA DMA-buf callbacks only once for all GSA heaps */
	if (atomic_inc_return(&gsa_dma_buf_registered_count) == 1) {
		gsa_register_func_for_dma_buf(gsa_dma_buf_get_ffa_tag,
					      gsa_dma_buf_get_shared_mem_id);
	}

	return 0;
}

static void gcma_gsa_heap_exit(struct gcma_heap *heap)
{
	struct gcma_gsa_priv *priv = heap->priv;

	if (priv->local_pool)
		gen_pool_destroy(priv->local_pool);

	if (atomic_dec_and_test(&gsa_dma_buf_registered_count))
		gsa_register_func_for_dma_buf(NULL, NULL);
}

/* FFA callbacks */
static struct buffer_prot_info *gsa_dma_buf_prot_info(struct dma_buf *dma_buf)
{
	struct samsung_dma_buffer *buffer;

	if (dma_buf->ops != &samsung_dma_buf_ops)
		return NULL;

	buffer = dma_buf->priv;
	if (!dma_heap_flags_protected(buffer->flags))
		return NULL;

	return buffer->priv;
}

static u64 pack_tag(u32 prot_id, u32 dma_va)
{
	u64 tag = prot_id;

	tag |= ((u64)dma_va) << 32;
	return tag;
}

static u64 gsa_dma_buf_get_ffa_tag(struct dma_buf *dma_buf)
{
	struct buffer_prot_info *prot_info = gsa_dma_buf_prot_info(dma_buf);

	if (!prot_info)
		return 0;

	return pack_tag(prot_info->flags, prot_info->dma_addr);
}

static int gsa_dma_buf_get_shared_mem_id(struct dma_buf *dma_buf,
					 u64 *id, u64 *poff)
{
	struct samsung_dma_buffer *buffer = dma_buf->priv;
	struct buffer_prot_info *protdesc;
	struct gcma_gsa_buffer_priv *gsa_priv;

	if ((dma_buf->ops == &samsung_dma_buf_ops) &&
	    (dma_heap_flags_static_protected(buffer->flags))) {
		protdesc = buffer->priv;
		if (!protdesc)
			return -ENODATA;

		gsa_priv = container_of(protdesc, struct gcma_gsa_buffer_priv, protdesc);
		*id = buffer->mem_id;
		*poff = gsa_priv->mem_id_offset;
		return 0;
	}

	return -ENODATA;
}

const struct gcma_heap_ops gcma_heap_gsa_ops = {
	.heap_init        = gcma_gsa_heap_init,
	.heap_exit        = gcma_gsa_heap_exit,
	.alloc            = gcma_gsa_alloc,
	.free             = gcma_gsa_free,
	.buffer_protect   = gcma_gsa_buffer_prot_desc_init,
	.buffer_unprotect = gcma_gsa_buffer_prot_desc_free,
};
