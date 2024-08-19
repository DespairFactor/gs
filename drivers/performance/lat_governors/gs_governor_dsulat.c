// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Google LLC.
 *
 * Dsu Latency Governor Main Module.
 */
#define pr_fmt(fmt) "gs_governor_dsulat: " fmt

#include <dt-bindings/soc/google/zuma-devfreq.h>
#include <governor.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <performance/gs_perf_mon/gs_perf_mon.h>
#include <soc/google/exynos-devfreq.h>
#include <trace/events/power.h>

#include "gs_governor_utils.h"

/**
 * struct frequency_vote - Contains configs and voting data.
 * @vote_name:	The name of the device we will vote frequency for.
 * @min_freq_req:	The min vote we assert for the device frequency.
 */
struct frequency_vote {
	const char* vote_name;
	struct exynos_pm_qos_request min_freq_req;
};

/**
 * struct secondary_frequency_domain - Contains voting data for secondary domains.
 * @vote:	Frequency voting mechanism for the secondary device frequency.
 * @freq_map:	The mapping from primary freq vote to secondary vote.
 */
struct secondary_frequency_domain {
	struct frequency_vote target_freq_vote;
	struct gs_governor_core_dev_map *freq_map;
};

/**
 * struct dsulat_data - Node containing dsulat's global data.
 * @gov_is_on:			Governor's active state.
 * @attr_grp:			Tuneable governor parameters exposed to userspace.
 * @dev:			Reference to the governor's device.
 * @target_freq_vote:		Primary target domain.
 * @num_cpu_clusters:		Number of CPU clusters the governor will service.
 * @cpu_configs_arr:		Configurations for each cluster's latency vote.
 * @num_secondary_votes:	Number of secondary vote domains.
 * @secondary_vote_arr:		Array of secondary vote domains.
 */
struct dsulat_data {
	bool gov_is_on;
	struct attribute_group *attr_grp;
	struct device *dev;
	struct frequency_vote target_freq_vote;

	int num_cpu_clusters;
	struct cluster_config *cpu_configs_arr;

	int num_secondary_votes;
	struct secondary_frequency_domain *secondary_vote_arr;
};

static void update_dsulat_gov(struct gs_cpu_perf_data *data, void *private_data);

/* Global monitor client used to get callbacks when gs_perf_mon data is updated. */
static struct gs_perf_mon_client dsulat_perf_client = {
	.client_callback = update_dsulat_gov,
	.name = "dsulat"
};

/* Dsulat datastructure holding dsulat governor configurations and metadata. */
static struct dsulat_data dsulat_node;

/* Macro expansions for sysfs nodes.*/
MAKE_CLUSTER_ATTR(dsulat_node, stall_floor);
MAKE_CLUSTER_ATTR(dsulat_node, ratio_ceil);
MAKE_CLUSTER_ATTR(dsulat_node, cpuidle_state_depth_threshold);

SHOW_CLUSTER_FREQ_MAP_ATTR(dsulat_node, latency_freq_table);
SHOW_CLUSTER_FREQ_MAP_ATTR(dsulat_node, base_freq_table);

/* These sysfs emitters also depend on the macro expansions from above. */
static struct attribute *dsulat_dev_attr[] = {
	&dev_attr_dsulat_node_stall_floor.attr,
	&dev_attr_dsulat_node_ratio_ceil.attr,
	&dev_attr_dsulat_node_cpuidle_state_depth_threshold.attr,
	&dev_attr_dsulat_node_latency_freq_table.attr,
	&dev_attr_dsulat_node_base_freq_table.attr,
	NULL,
};

/* Sysfs files to expose. */
static struct attribute_group dsulat_dev_attr_group = {
	.name = "dsulat_attr",
	.attrs = dsulat_dev_attr,
};

/**
 * gs_governor_dsulat_update_target_freq_vote - Registers the vote for the dsulat governor.
 *
 * Inputs:
 * @vote:		The node to vote on.
 * @target_freq:	New target frequency.
 *
 * Outputs:
 *			Non-zero on error.
*/
static int gs_governor_dsulat_update_target_freq_vote(struct frequency_vote *vote,
					    unsigned long target_freq)
{
	if (!exynos_pm_qos_request_active(&vote->min_freq_req))
		return -ENODEV;

	exynos_pm_qos_update_request_async(&vote->min_freq_req, target_freq);
	trace_clock_set_rate(vote->vote_name, target_freq, raw_smp_processor_id());

	return 0;
}

/**
 * gs_governor_dsulat_update_all_freq_votes - Updates the vote for primary and secondary vote comonents.
 *
 * Inputs:
 * @primary_vote:	Primary frequency vote component.
 *
 * Outputs:
 *			Non-zero on error.
*/
static void gs_governor_dsulat_update_all_freq_votes(unsigned long primary_vote)
{
	unsigned long secondary_vote;
	struct secondary_frequency_domain *sub_vote;
	int secondary_idx;

	gs_governor_dsulat_update_target_freq_vote(&dsulat_node.target_freq_vote, primary_vote);

	for (secondary_idx = 0; secondary_idx < dsulat_node.num_secondary_votes;
	     secondary_idx++) {
		sub_vote = &dsulat_node.secondary_vote_arr[secondary_idx];
		secondary_vote =
			gs_governor_core_to_dev_freq(sub_vote->freq_map, primary_vote);
		gs_governor_dsulat_update_target_freq_vote(&sub_vote->target_freq_vote, secondary_vote);
	}
}

/**
 * gs_governor_dsulat_compute_freq - Calculates dsulat freq votes desired by each CPU cluster.
 *
 * This function determines the dsulat target frequency.
 *
 * Input:
 * @cpu_perf_data_arr:	CPU data to use as input.
 *
 * Returns:
 * @max_freq:		The computed target frequency.
*/
static unsigned long gs_governor_dsulat_compute_freq(struct gs_cpu_perf_data *cpu_perf_data_arr)
{
	int cpu;
	int cluster_idx;
	struct cluster_config *cluster;
	unsigned long max_freq = 0;
	char trace_name[] = { 'c', 'p', 'u', '0', 'd', 's', 'u', '\0' };

	/* For each cluster, we make a frequency decision. */
	for (cluster_idx = 0; cluster_idx < dsulat_node.num_cpu_clusters; cluster_idx++) {
		cluster = &dsulat_node.cpu_configs_arr[cluster_idx];
		for_each_cpu(cpu, &cluster->cpus) {
			struct gs_cpu_perf_data *cpu_data = &cpu_perf_data_arr[cpu];
			unsigned long ratio, mem_stall_pct, mem_stall_floor, ratio_ceil;
			bool dsulat_cpuidle_state_aware;
			enum gs_perf_cpu_idle_state dsulat_configured_idle_depth_threshold;
			unsigned long l2_cachemiss, mem_stall, cyc, last_delta_us, inst;
			unsigned long dsu_freq = 0, effective_cpu_freq_khz;
			trace_name[3] = '0' + cpu;

			/* Check if the cpu monitor is up. */
			if (!cpu_data->cpu_mon_on)
				goto early_exit;

			l2_cachemiss = cpu_data->perf_ev_last_delta[PERF_L2D_CACHE_REFILL_IDX];
			mem_stall = cpu_data->perf_ev_last_delta[PERF_STALL_BACKEND_MEM_IDX];
			cyc = cpu_data->perf_ev_last_delta[PERF_CYCLE_IDX];
			inst = cpu_data->perf_ev_last_delta[PERF_INST_IDX];
			last_delta_us = cpu_data->time_delta_us;

			ratio_ceil = cluster->ratio_ceil;
			mem_stall_floor = cluster->stall_floor;
			dsulat_cpuidle_state_aware = cluster->cpuidle_state_aware;
			dsulat_configured_idle_depth_threshold = cluster->cpuidle_state_depth_threshold;

			/* Compute threshold data. */
			if (l2_cachemiss != 0)
				ratio = inst / l2_cachemiss;
			else
				ratio = inst;

			mem_stall_pct = mult_frac(10000, mem_stall, cyc);
			effective_cpu_freq_khz = MHZ_TO_KHZ * cyc / last_delta_us;

			if (dsulat_cpuidle_state_aware && cpu_data->cpu_idle_state >= dsulat_configured_idle_depth_threshold)
   				goto early_exit; // Zeroing vote for sufficiently idle CPUs.

			/* If we pass the threshold, use the latency table. */
			if (ratio <= ratio_ceil && mem_stall_pct >= mem_stall_floor)
				dsu_freq = gs_governor_core_to_dev_freq(cluster->latency_freq_table,
									effective_cpu_freq_khz);
			else
				dsu_freq = gs_governor_core_to_dev_freq(cluster->base_freq_table,
									effective_cpu_freq_khz);

			/* Keep a running max of the DSU frequency. */
			if (dsu_freq > max_freq)
				max_freq = dsu_freq;
		early_exit:
			/* Leave a trace for the cluster desired DSU frequency. */
			trace_clock_set_rate(trace_name, dsu_freq, cpu);
		}
	}

	return max_freq;
}

/**
 * update_dsulat_gov  -	Callback function from the perf monitor to service
 * 			the dsulat governor.
 *
 * Input:
 * @data:		Performance data from the monitor.
 * @private_data:	Unused.
 *
*/
static void update_dsulat_gov(struct gs_cpu_perf_data *data, void *private_data)
{
	unsigned long next_frequency;

	/* If the dsulat governor is not active. Reset our vote to minimum. */
	if (!dsulat_node.gov_is_on || !data) {
		dev_dbg(dsulat_node.dev, "Dsulat governor is not active. Leaving vote unchanged.\n");
		return;
	}

	/* Step 1: compute the frequency. */
	next_frequency = gs_governor_dsulat_compute_freq(data);

	/* Step 2: process the frequency vote. */
	gs_governor_dsulat_update_all_freq_votes(next_frequency);
}

/**
 * gs_dsulat_governor_remove_all_votes - Initializes all the votes for dsulat governor.
*/
static void gs_dsulat_governor_remove_all_votes(void)
{
	struct frequency_vote *vote = &dsulat_node.target_freq_vote;
	int secondary_idx;

	/* Remove primary votes. */
	exynos_pm_qos_remove_request(&vote->min_freq_req);

	/* Remove secondary votes. */
	for (secondary_idx = 0; secondary_idx < dsulat_node.num_secondary_votes;
	     secondary_idx++) {
		vote = &dsulat_node.secondary_vote_arr[secondary_idx].target_freq_vote;
		exynos_pm_qos_remove_request(&vote->min_freq_req);
	}
}

/**
 * gov_start - Starts the governor.
*/
static int gov_start(void)
{
	int ret;
	if (dsulat_node.gov_is_on)
		return 0;

	/* Add clients. */
	ret = gs_perf_mon_add_client(&dsulat_perf_client);
	if (ret)
		return ret;

	dsulat_node.gov_is_on = true;

	return 0;
}

/**
 * gov_stop - Stops the governor.
*/
static void gov_stop(void)
{
	if (!dsulat_node.gov_is_on)
		return;

	dsulat_node.gov_is_on = false;

	/* Remove the client. */
	gs_perf_mon_remove_client(&dsulat_perf_client);

	/* Reset all the votes to minimum. */
	gs_governor_dsulat_update_all_freq_votes(0);
}

/**
 * gs_dsulat_governor_vote_parse - Initializes the votes for the dsulat.
 *
 * Input:
 * @vote_node:	Node containing the vote config.
 * @dev:	The device the governor is bound to.
 *
 * Output:	Non-zero on error.
*/
static int gs_dsulat_governor_vote_parse(struct device_node *vote_node,
					 struct frequency_vote *vote, struct device *dev)
{
	u32 pm_qos_class;

	if (of_property_read_u32(vote_node, "pm_qos_class", &pm_qos_class)) {
		dev_err(dev, "pm_qos_class undefined\n");
		return -ENODEV;
	}

	if (of_property_read_string(vote_node, "vote_name",
				    &vote->vote_name)) {
		dev_err(dev, "vote_name undefined\n");
		return -ENODEV;
	}

	exynos_pm_qos_add_request(&vote->min_freq_req, (int)pm_qos_class, 0);

	return 0;
}

/**
 * gs_dsulat_initialize_secondary_votes - Initializes the secondaty governors from a DT Node.
 *
 * Inputs:
 * @secondary_votes_node:	The tree node containing the list of secondary governor data.
 * @dev:			The device the governor is bound to.
 *
 * Returns:			Non-zero on error.
*/
static int gs_dsulat_initialize_secondary_votes(struct device_node *secondary_votes_node,
						  struct device *dev)
{
	struct device_node *sub_votes_node = NULL;
	struct secondary_frequency_domain *sub_vote;
	int sub_vote_idx;
	int ret = 0;

	dsulat_node.num_secondary_votes = of_get_child_count(secondary_votes_node);

	/* Allocate a container for secondary domains. */
	dsulat_node.secondary_vote_arr = devm_kzalloc(
		dev, sizeof(struct secondary_frequency_domain) * dsulat_node.num_secondary_votes,
		GFP_KERNEL);
	if (!dsulat_node.secondary_vote_arr) {
		dev_err(dev, "No memory for secondary_vote_arr.\n");
		return -ENOMEM;
	}

	/* Populate the Components. */
	sub_vote_idx = 0;
	while ((sub_votes_node = of_get_next_child(secondary_votes_node, sub_votes_node)) !=
	       NULL) {
		sub_vote = &dsulat_node.secondary_vote_arr[sub_vote_idx];

		/* Initialize the vote structure. */
		if ((ret = gs_dsulat_governor_vote_parse(sub_votes_node, &sub_vote->target_freq_vote,
							 dev)))
			return ret;

		/* Initialize the secondary translation map. */
		sub_vote->freq_map = gs_governor_init_core_dev_map(dev, sub_votes_node,
								     "core-dev-table-latency");
		if (!sub_vote->freq_map) {
			dev_err(dev, "Can't parse freq-table for sub-domain.");
			return -ENODEV;
		}

		/* Increment pointer. */
		sub_vote_idx += 1;
	}
	return 0;
}

/**
 * gs_dsulat_governor_initialize - Initializes the dsulat governor from a DT Node.
 *
 * Inputs:
 * @governor_node:	The tree node contanin governor data.
 * @data:		The devfreq data to update frequencies.
 *
 * Returns:		Non-zero on error.
*/
static int gs_dsulat_governor_initialize(struct device_node *governor_node, struct device *dev)
{
	int ret = 0;
	struct device_node *cluster_node = NULL;
	struct cluster_config *cluster;
	int cluster_idx;

	dsulat_node.num_cpu_clusters = of_get_child_count(governor_node);

	/* Allocate a container for clusters. */
	dsulat_node.cpu_configs_arr = devm_kzalloc(
		dev, sizeof(struct cluster_config) * dsulat_node.num_cpu_clusters, GFP_KERNEL);
	if (!dsulat_node.cpu_configs_arr) {
		dev_err(dev, "No memory for cluster_configs.\n");
		return -ENOMEM;
	}

	/* Populate the Components. */
	cluster_idx = 0;
	while ((cluster_node = of_get_next_child(governor_node, cluster_node)) != NULL) {
		cluster = &dsulat_node.cpu_configs_arr[cluster_idx];
		if ((ret = populate_cluster_config(dev, cluster_node, cluster)))
			return ret;

		/* Increment pointer. */
		cluster_idx += 1;
	}
	return 0;
}

static int gs_governor_dsulat_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *governor_config_node, *frequency_vote_node, *secondary_votes_node;
	int ret;

	dsulat_node.dev = &pdev->dev;
	dsulat_node.attr_grp = &dsulat_dev_attr_group;

	/* Find and intitialize frequency votes. */
	frequency_vote_node = of_get_child_by_name(dev->of_node, "primary_vote_config");
	if (!frequency_vote_node) {
		dev_err(dev, "Dsulat frequency_votes not defined.\n");
		return -ENODEV;
	}

	ret = gs_dsulat_governor_vote_parse(frequency_vote_node, &dsulat_node.target_freq_vote,
					    dev);
	if (ret) {
		dev_err(dev, "Failed to parse dsulat primary vote node data.\n");
		return ret;
	}

	/* Find and initialize secondary votes. */
	secondary_votes_node = of_get_child_by_name(dev->of_node, "secondary_frequency_votes");
	if (secondary_votes_node) {
		ret = gs_dsulat_initialize_secondary_votes(secondary_votes_node, dev);
		if (ret) {
			dev_err(dev, "Failed to parse secondary vote data.\n");
			goto err_out;
		}
	} else {
		dev_dbg(dev, "Dsulat secondary vote node not defined. Skipping\n");
	}

	/* Find and initialize governor. */
	governor_config_node = of_get_child_by_name(dev->of_node, "governor_config");
	if (!governor_config_node) {
		dev_err(dev, "Dsulat governor node not defined.\n");
		ret = -ENODEV;
		goto err_out;
	}

	ret = gs_dsulat_governor_initialize(governor_config_node, dev);
	if (ret) {
		dev_err(dev, "Failed to parse private governor data.\n");
		goto err_out;
	}

	/* Add sysfs nodes here. */
	ret = sysfs_create_group(&dev->kobj, dsulat_node.attr_grp);
	if (ret) {
		dev_err(dev, "Failed to initialize governor sysfs groups.\n");
		goto err_out;
	}

	/* Start the governor servicing. */
	ret = gov_start();
	if (ret) {
		dev_err(dev, "Failed to start dsulat governor.\n");
		goto err_gov_start;
	}

	return 0;

err_gov_start:
	sysfs_remove_group(&dsulat_node.dev->kobj, dsulat_node.attr_grp);
err_out:
	gs_dsulat_governor_remove_all_votes();

	return ret;
}

static int gs_governor_dsulat_driver_remove(struct platform_device *pdev)
{
	/* Stop governor servicing. */
	gov_stop();

	/* Remove Sysfs here. */
	sysfs_remove_group(&dsulat_node.dev->kobj, dsulat_node.attr_grp);

	/* Remove pm_qos vote here. */
	gs_dsulat_governor_remove_all_votes();

	return 0;
}

static const struct of_device_id gs_governor_dsulat_root_match[] = { {
	.compatible = "google,gs_governor_dsulat",
} };

static struct platform_driver gs_governor_dsulat_platform_driver = {
	.probe = gs_governor_dsulat_driver_probe,
	.remove = gs_governor_dsulat_driver_remove,
	.driver = {
		.name = "gs_governor_dsulat",
		.owner = THIS_MODULE,
		.of_match_table = gs_governor_dsulat_root_match,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(gs_governor_dsulat_platform_driver);
MODULE_AUTHOR("Will Song <jinpengsong@google.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Google Source Dsulat Governor");