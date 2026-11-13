/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _WQ_H
#define _WQ_H

#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/cache.h>

#define WQ_NAME_LEN 32

/* Minimal definition of workqueue_struct to access name and unbound_attrs */
struct workqueue_struct {
	struct list_head	pwqs;
	struct list_head	list;
	struct mutex		mutex;
	int			work_color;
	int			flush_color;
	atomic_t		nr_pwqs_to_flush;
	void			*first_flusher;
	struct list_head	flusher_queue;
	struct list_head	flusher_overflow;
	struct list_head	maydays;
	void			*rescuer;
	int			nr_drainers;
	int			max_active;
	int			min_active;
	int			saved_max_active;
	int			saved_min_active;
	struct workqueue_attrs	*unbound_attrs;
	void			*dfl_pwq;
#ifdef CONFIG_SYSFS
	void			*wq_dev;
#endif
#ifdef CONFIG_LOCKDEP
	char			*lock_name;
	struct lock_class_key	key;
	struct lockdep_map	__lockdep_map;
	struct lockdep_map	*lockdep_map;
#endif
	char			name[WQ_NAME_LEN];
	struct rcu_head		rcu;
	unsigned int		flags ____cacheline_aligned;
	void			*cpu_pwq;
	void			*node_nr_active[];
};

void rvh_alloc_workqueue_handler(void *unused, struct workqueue_struct *wq,
				 unsigned int *flags, int *max_active);

#endif /* _WQ_H */
