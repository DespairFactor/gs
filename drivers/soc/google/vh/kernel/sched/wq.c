// SPDX-License-Identifier: GPL-2.0-only
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/cache.h>
#include "wq.h"

void rvh_alloc_workqueue_handler(void *unused, struct workqueue_struct *wq,
				 unsigned int *flags, int *max_active)
{
	if (strcmp(wq->name, "kverityd") == 0) {
		if (!wq->unbound_attrs) {
			wq->unbound_attrs = alloc_workqueue_attrs();
			if (!wq->unbound_attrs) {
				pr_err("%s alloc_workqueue_attrs failed for: %s\n", __func__, wq->name);
				return;
			}
		}
		*flags |= (WQ_UNBOUND | WQ_HIGHPRI);
		if (*max_active == 1)
			*flags |= __WQ_ORDERED;
	}
}

