/*
 * linux/drivers/gpu/exynos/g2d/g2d_task.c
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>

#include "g2d.h"
#include "g2d_task.h"
#include "g2d_uapi_process.h"
#include "g2d_command.h"
#include "g2d_fence.h"

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
#include <linux/smc.h>

#define G2D_SECURE_DMA_BASE	0x8000000
#define G2D_SEC_COMMAND_BUF 12
#define G2D_ALWAYS_S 37

static int g2d_map_cmd_data(struct g2d_task *task)
{
	struct g2d_buffer_prot_info *prot = &task->prot_info;
	int ret;

	prot->chunk_count = 1;
	prot->flags = G2D_SEC_COMMAND_BUF;
	prot->chunk_size = G2D_CMD_LIST_SIZE;
	prot->bus_address = page_to_phys(task->cmd_page);
	prot->dma_addr = G2D_SECURE_DMA_BASE + G2D_CMD_LIST_SIZE * task->job_id;

	__flush_dcache_area(prot, sizeof(struct g2d_buffer_prot_info));
	ret = exynos_smc(SMC_DRM_PPMP_PROT, virt_to_phys(prot), 0, 0);

	if (ret) {
		dev_err(task->g2d_dev->dev,
			"Failed to map secure page tbl (%d) %x %x %lx\n", ret,
			prot->dma_addr, prot->flags, prot->bus_address);
		return ret;
	}

	task->cmd_addr = prot->dma_addr;

	return 0;
}

static void g2d_secure_enable(void)
{
	exynos_smc(SMC_PROTECTION_SET, 0, G2D_ALWAYS_S, 1);
}

static void g2d_secure_disable(void)
{
	exynos_smc(SMC_PROTECTION_SET, 0, G2D_ALWAYS_S, 0);
}

static void g2d_flush_command_page(struct g2d_task *task)
{
	__dma_flush_area(page_address(task->cmd_page), G2D_CMD_LIST_SIZE);
}
#else
static int g2d_map_cmd_data(struct g2d_task *task)
{
	struct scatterlist sgl;

	/* mapping the command data */
	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, task->cmd_page, G2D_CMD_LIST_SIZE, 0);
	task->cmd_addr = iovmm_map(task->g2d_dev->dev, &sgl, 0,
				   G2D_CMD_LIST_SIZE, DMA_TO_DEVICE,
				   IOMMU_READ | IOMMU_CACHE);

	if (IS_ERR_VALUE(task->cmd_addr)) {
		dev_err(task->g2d_dev->dev,
			"%s: Unable to allocate IOVA for cmd data\n", __func__);
		return task->cmd_addr;
	}

	return 0;
}

#define g2d_secure_enable() do { } while (0)
#define g2d_secure_disable() do { } while (0)
#define g2d_flush_command_page(task) do { } while (0)
#endif

struct g2d_task *g2d_get_active_task_from_id(struct g2d_device *g2d_dev,
					     unsigned int id)
{
	struct g2d_task *task;

	list_for_each_entry(task, &g2d_dev->tasks_active, node) {
		if (task->job_id == id)
			return task;
	}

	dev_err(g2d_dev->dev,
		"%s: No active task entry is found for ID %d\n", __func__, id);

	return NULL;
}

/* TODO: consider separate workqueue to eliminate delay by scheduling work */
static void g2d_task_completion_work(struct work_struct *work)
{
	struct g2d_task *task = container_of(work, struct g2d_task, work);

	g2d_put_images(task->g2d_dev, task);

	g2d_put_free_task(task->g2d_dev, task);
}

static void __g2d_finish_task(struct g2d_task *task, bool success)
{
	change_task_state_finished(task);
	if (!success)
		mark_task_state_error(task);

	complete_all(&task->completion);

	if (!!(task->flags & G2D_FLAG_NONBLOCK)) {
		bool failed;

		INIT_WORK(&task->work, g2d_task_completion_work);
		failed = !queue_work(task->g2d_dev->schedule_workq,
					&task->work);
		BUG_ON(failed);
	}
}

static void g2d_finish_task(struct g2d_device *g2d_dev,
			    struct g2d_task *task, bool success)
{
	list_del_init(&task->node);

	del_timer(&task->timer);

	g2d_secure_disable();

	clk_disable(g2d_dev->clock);

	pm_runtime_put(g2d_dev->dev);

	__g2d_finish_task(task, success);
}

void g2d_finish_task_with_id(struct g2d_device *g2d_dev,
			     unsigned int job_id, bool success)
{
	struct g2d_task *task = NULL;

	task = g2d_get_active_task_from_id(g2d_dev, job_id);
	if (!task)
		return;

	if (is_task_state_killed(task)) {
		dev_err(g2d_dev->dev, "%s: Killed task ID %d is completed\n",
			__func__, job_id);
		success = false;
	}

	task->ktime_end = ktime_get();

	g2d_finish_task(g2d_dev, task, success);
}

void g2d_flush_all_tasks(struct g2d_device *g2d_dev)
{
	struct g2d_task *task;

	dev_err(g2d_dev->dev, "%s: Flushing all active tasks\n", __func__);

	while (!list_empty(&g2d_dev->tasks_active)) {
		task = list_first_entry(&g2d_dev->tasks_active,
					struct g2d_task, node);

		dev_err(g2d_dev->dev, "%s: Flushed task of ID %d\n",
			__func__, task->job_id);

		mark_task_state_killed(task);

		g2d_finish_task(g2d_dev, task, false);
	}
}

static void g2d_execute_task(struct g2d_device *g2d_dev, struct g2d_task *task)
{
	list_move_tail(&task->node, &g2d_dev->tasks_active);
	change_task_state_active(task);

	task->ktime_begin = ktime_get();

	setup_timer(&task->timer,
		    g2d_hw_timeout_handler, (unsigned long)task);

	mod_timer(&task->timer,
		  jiffies + msecs_to_jiffies(G2D_HW_TIMEOUT_MSEC));

	g2d_flush_command_page(task);

	/*
	 * g2d_device_run() is not reentrant while g2d_schedule() is
	 * reentrant g2d_device_run() should be protected with
	 * g2d_dev->lock_task from race.
	 */
	if (g2d_device_run(g2d_dev, task) < 0)
		g2d_finish_task(g2d_dev, task, false);
}

void g2d_prepare_suspend(struct g2d_device *g2d_dev)
{
	spin_lock_irq(&g2d_dev->lock_task);
	set_bit(G2D_DEVICE_STATE_SUSPEND, &g2d_dev->state);
	spin_unlock_irq(&g2d_dev->lock_task);

	wait_event(g2d_dev->freeze_wait, list_empty(&g2d_dev->tasks_active));
}

void g2d_suspend_finish(struct g2d_device *g2d_dev)
{
	struct g2d_task *task;

	spin_lock_irq(&g2d_dev->lock_task);

	clear_bit(G2D_DEVICE_STATE_SUSPEND, &g2d_dev->state);

	while (!list_empty(&g2d_dev->tasks_prepared)) {

		task = list_first_entry(&g2d_dev->tasks_prepared,
					struct g2d_task, node);
		g2d_execute_task(g2d_dev, task);
	}

	spin_unlock_irq(&g2d_dev->lock_task);
}

static void g2d_schedule_task(struct g2d_task *task)
{
	struct g2d_device *g2d_dev = task->g2d_dev;
	unsigned long flags;
	int ret;

	del_timer(&task->timer);

	g2d_complete_commands(task);

	/*
	 * Unconditional invocation of pm_runtime_get_sync() has no side effect
	 * in g2d_schedule(). It just increases the usage count of RPM if this
	 * function skips calling g2d_device_run(). The skip only happens when
	 * there is no task to run in g2d_dev->tasks_prepared.
	 * If pm_runtime_get_sync() enabled power, there must be a task in
	 * g2d_dev->tasks_prepared.
	 */
	ret = pm_runtime_get_sync(g2d_dev->dev);
	if (ret < 0) {
		dev_err(g2d_dev->dev, "Failed to enable power (%d)\n", ret);
		goto err_pm;
	}

	ret = clk_prepare_enable(g2d_dev->clock);
	if (ret < 0) {
		dev_err(g2d_dev->dev, "Failed to enable clock (%d)\n", ret);
		goto err_clk;
	}

	g2d_secure_enable();

	spin_lock_irqsave(&g2d_dev->lock_task, flags);

	list_add_tail(&task->node, &g2d_dev->tasks_prepared);
	change_task_state_prepared(task);

	if (!!(g2d_dev->state & (1 << G2D_DEVICE_STATE_SUSPEND)))
		return;

	g2d_execute_task(g2d_dev, task);

	spin_unlock_irqrestore(&g2d_dev->lock_task, flags);
	return;
err_clk:
	pm_runtime_put(g2d_dev->dev);
err_pm:
	__g2d_finish_task(task, false);
}

/* TODO: consider separate workqueue to eliminate delay by completion work */
static void g2d_task_schedule_work(struct work_struct *work)
{
	g2d_schedule_task(container_of(work, struct g2d_task, work));
}

void g2d_queuework_task(struct kref *kref)
{
	struct g2d_task *task = container_of(kref, struct g2d_task, starter);
	struct g2d_device *g2d_dev = task->g2d_dev;
	bool failed;

	failed = !queue_work(g2d_dev->schedule_workq, &task->work);

	BUG_ON(failed);
}

void g2d_start_task(struct g2d_task *task)
{
	reinit_completion(&task->completion);

	setup_timer(&task->timer,
		    g2d_fence_timeout_handler, (unsigned long)task);

	if (atomic_read(&task->starter.refcount.refs) > 1)
		mod_timer(&task->timer,
			jiffies + msecs_to_jiffies(G2D_FENCE_TIMEOUT_MSEC));

	kref_put(&task->starter, g2d_queuework_task);
}

void g2d_fence_callback(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct g2d_layer *layer = container_of(cb, struct g2d_layer, fence_cb);
	unsigned long flags;

	spin_lock_irqsave(&layer->task->fence_timeout_lock, flags);
	/* @fence is released in g2d_put_image() */
	kref_put(&layer->task->starter, g2d_queuework_task);
	spin_unlock_irqrestore(&layer->task->fence_timeout_lock, flags);
}

struct g2d_task *g2d_get_free_task(struct g2d_device *g2d_dev)
{
	struct g2d_task *task;
	unsigned long flags;

	spin_lock_irqsave(&g2d_dev->lock_task, flags);

	if (list_empty(&g2d_dev->tasks_free)) {
		spin_unlock_irqrestore(&g2d_dev->lock_task, flags);
		return NULL;
	}

	task = list_first_entry(&g2d_dev->tasks_free, struct g2d_task, node);
	list_del_init(&task->node);
	INIT_WORK(&task->work, g2d_task_schedule_work);

	init_task_state(task);

	g2d_init_commands(task);

	spin_unlock_irqrestore(&g2d_dev->lock_task, flags);

	return task;
}

void g2d_put_free_task(struct g2d_device *g2d_dev, struct g2d_task *task)
{
	unsigned long flags;

	spin_lock_irqsave(&g2d_dev->lock_task, flags);

	clear_task_state(task);

	list_add(&task->node, &g2d_dev->tasks_free);

	spin_unlock_irqrestore(&g2d_dev->lock_task, flags);
}

void g2d_destroy_tasks(struct g2d_device *g2d_dev)
{
	struct g2d_task *task, *next;
	unsigned long flags;

	spin_lock_irqsave(&g2d_dev->lock_task, flags);

	task = g2d_dev->tasks;
	while (task != NULL) {
		next = task->next;

		list_del(&task->node);

		iovmm_unmap(g2d_dev->dev, task->cmd_addr);

		__free_pages(task->cmd_page, get_order(G2D_CMD_LIST_SIZE));

		kfree(task);

		task = next;
	}

	spin_unlock_irqrestore(&g2d_dev->lock_task, flags);

	destroy_workqueue(g2d_dev->schedule_workq);
}

static struct g2d_task *g2d_create_task(struct g2d_device *g2d_dev, int id)
{
	struct g2d_task *task;
	int i, ret;

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&task->node);

	task->cmd_page = alloc_pages(GFP_KERNEL, get_order(G2D_CMD_LIST_SIZE));
	if (!task->cmd_page) {
		ret = -ENOMEM;
		goto err_page;
	}

	task->job_id = id;
	task->g2d_dev = g2d_dev;

	ret = g2d_map_cmd_data(task);
	if (ret)
		goto err_map;

	for (i = 0; i < G2D_MAX_IMAGES; i++)
		task->source[i].task = task;
	task->target.task = task;

	init_completion(&task->completion);
	spin_lock_init(&task->fence_timeout_lock);

	return task;

err_map:
	__free_pages(task->cmd_page, get_order(G2D_CMD_LIST_SIZE));
err_page:
	kfree(task);

	return ERR_PTR(ret);
}

int g2d_create_tasks(struct g2d_device *g2d_dev)
{
	struct g2d_task *task;
	unsigned int i;

	g2d_dev->schedule_workq = create_singlethread_workqueue("g2dscheduler");
	if (!g2d_dev->schedule_workq)
		return -ENOMEM;

	for (i = 0; i < G2D_MAX_JOBS; i++) {
		task = g2d_create_task(g2d_dev, i);

		if (IS_ERR(task)) {
			g2d_destroy_tasks(g2d_dev);
			return PTR_ERR(task);
		}

		task->next = g2d_dev->tasks;
		g2d_dev->tasks = task;
		list_add(&task->node, &g2d_dev->tasks_free);
	}

	return 0;
}

void g2d_dump_task(struct g2d_device *g2d_dev, unsigned int job_id)
{
	struct g2d_task *task;
	unsigned long flags;
	unsigned int i;
	struct g2d_reg *regs;

	spin_lock_irqsave(&g2d_dev->lock_task, flags);

	list_for_each_entry(task, &g2d_dev->tasks_active, node) {
		if (task->job_id == job_id)
			break;
	}

	/* TODO: more dump task */

	regs = page_address(task->cmd_page);

	for (i = 0; i < task->cmd_count; i++)
		pr_info("G2D: CMD[%03d] %#06x, %#010x\n",
			i, regs[i].offset, regs[i].value);

	spin_unlock_irqrestore(&g2d_dev->lock_task, flags);
}
