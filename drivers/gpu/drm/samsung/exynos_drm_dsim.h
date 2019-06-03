/* SPDX-License-Identifier: GPL-2.0-only
 *
 * linux/drivers/gpu/drm/samsung/exynos_drm_dsim.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for Samsung MIPI DSI Master driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_DSI_H__
#define __EXYNOS_DRM_DSI_H__

/* Add header */
#include <drm/drm_encoder.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_property.h>
#include <video/videomode.h>

#include "cal_9820/dsim_cal.h"

enum dsim_state {
	DSIM_STATE_HSCLKEN,
	DSIM_STATE_ULPS,
	DSIM_STATE_SUSPEND
};

struct dsim_pll_params {
	unsigned int num_modes;
	struct dsim_pll_param **params;
};

struct dsim_resources {
	void __iomem *regs;
	void __iomem *phy_regs;
	void __iomem *phy_regs_ex;
	void __iomem *ss_reg_base;
	struct phy *phy;
	struct phy *phy_ex;
	struct clk *aclk;
};

struct dsim_device {
	struct drm_encoder encoder;
	struct drm_bridge *bridge;
	struct mipi_dsi_host dsi_host;
	struct drm_connector connector;
	struct device_node *panel_node;
	struct device_node *bridge_node;
	struct drm_panel *panel;
	struct display_timing *timings;
	struct drm_display_mode native_mode;
	struct videomode vm;
	struct device *dev;

	enum exynos_drm_output_type output_type;

	struct dsim_resources res;
	struct clk **clks;
	struct dsim_pll_params *pll_params;
	int irq;
	int id;
	spinlock_t slock;
	struct mutex cmd_lock;
	struct completion ph_wr_comp;
	struct completion rd_comp;
	struct timer_list cmd_timer;

	u32 lanes;
	u32 mode_flags;
	u32 format;

	enum dsim_state state;

	/* set bist mode by sysfs */
	unsigned int bist_mode;

	/* FIXME: dsim cal structure */
	struct dsim_reg_config config;
	struct dsim_clks clk_param;

	int idle_ip_index;

	struct {
		struct drm_property *max_luminance;
		struct drm_property *max_avg_luminance;
		struct drm_property *min_luminance;
		struct drm_property *hdr_formats;
	} props;
};

extern struct dsim_device *dsim_drvdata[MAX_DSI_CNT];

int dsim_write_data(struct dsim_device *dsi, u32 id, unsigned long d0, u32 d1);
#define to_dsi(nm)	container_of(nm, struct dsim_device, nm)

#define MIPI_WR_TIMEOUT				msecs_to_jiffies(50)

#endif /* __EXYNOS_DRM_DSI_H__ */
