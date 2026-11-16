// SPDX-License-Identifier: GPL-2.0-only
/* fork.c
 *
 * Android Vendor Hook Sched Fork Support
 *
 * Copyright 2026 Google LLC
 */
#include <linux/sched.h>
#include <linux/module.h>
#include <trace/hooks/dtask.h>

#include <kernel/sched/sched.h>
#include "sched_priv.h"

void vh_lock_task_fork_pixel_mod(void *data, struct task_struct *p)
{
	struct vendor_task_struct_h *vph;

	if (unlikely(!p))
		return;

	/* 
	 * OPTIMIZATION: Vendor Task Pointer Validation Guard
	 * Ensure the vendor task extension structure has been allocated and 
	 * linked before querying group attributes during fork events.
	 */
	vph = (struct vendor_task_struct_h *)p->android_vendor_data1;
	if (unlikely(!vph || !vph->body))
		return;

	queue_delayed_notification(p, VENDOR_SCHED_CMD_TASK_FORK, 0, get_vendor_group(p));
}
