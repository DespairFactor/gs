// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/configfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/configfs.h>
#include <linux/iio/sysfs.h>

#include <linux/mfd/samsung/s2mpg1x-meter.h>
#include <linux/mfd/samsung/s2mpg10-meter.h>
#include <linux/mfd/samsung/s2mpg11-meter.h>
#include <linux/mfd/samsung/s2mpg10.h>
#include <linux/mfd/samsung/s2mpg11.h>

#define ODPM_PRINT_ESTIMATED_CLOCK_SKEW 0

/* Cache accumulated values to prevent too frequent updates,
 * allow a refresh only every 50 ms.
 * Note: s2mpg1x chips can take on average between 1-2 ms
 * for a refresh.
 */
#define ODPM_MIN_POLLING_TIME_MS 50 /* ms */

#define str(val) #val
#define xstr(s) str(s)
#define ODPM_RAIL_NAME_STR_LEN_MAX 49
#define ODPM_FREQ_DECIMAL_UHZ_STR_LEN_MAX 6

#define SWITCH_CHIP_FUNC(infop, func, args...)                                 \
	do {                                                                   \
		switch ((infop)->chip.id) {                                    \
		case ODPM_CHIP_S2MPG10:                                        \
			ret = s2mpg10_##func(args);                            \
			break;                                                 \
		case ODPM_CHIP_S2MPG11:                                        \
			ret = s2mpg11_##func(args);                            \
			break;                                                 \
		case ODPM_CHIP_COUNT:                                          \
			break;                                                 \
		}                                                              \
	} while (0)

#define SWITCH_METER_FUNC(infop, func, args...)                                \
	SWITCH_CHIP_FUNC(infop, func, info->meter, args)

/* At this moment, this driver supports a static 8 channels */
#define ODPM_CHANNEL_MAX S2MPG1X_METER_CHANNEL_MAX
#define ODPM_BUCK_EN_BYTES S2MPG1X_METER_BUCKEN_BUF

enum odpm_chip_id {
	ODPM_CHIP_S2MPG10,
	ODPM_CHIP_S2MPG11,
	ODPM_CHIP_COUNT,
};

enum odpm_rail_type {
	ODPM_RAIL_TYPE_REGULATOR_LDO,
	ODPM_RAIL_TYPE_REGULATOR_BUCK,
	ODPM_RAIL_TYPE_SHUNT,
};

struct odpm_rail_data {
	/* Config */
	const char *name;
	const char *schematic_name;
	const char *subsys_name;
	enum odpm_rail_type type;
	u32 mux_select;

	/* Buck specific */
	int channel_en_byte_offset;

	/* External rail specific config */
	u32 shunt_uohms;

	/* Bucks and external rails */
	u8 channel_en_index;

	/* Data */
	u64 acc_power_uW_sec_cached;
	u64 measurement_stop_ms;
	u64 measurement_start_ms_cached;
};

struct odpm_chip {
	/* Config */
	const char *name;
	enum odpm_chip_id id;
	enum s2mpg1x_id hw_id;
	int hw_rev;
	u32 max_refresh_time_ms;

	int num_rails;
	struct odpm_rail_data *rails;

	const u32 *sample_rate_int_uhz;
	int sample_rate_int_count;
	const u32 *sample_rate_ext_uhz;
	int sample_rate_ext_count;

	/* Data */
	u64 acc_timestamp_offset;
	u64 acc_timestamp;
	s2mpg1x_int_samp_rate int_sampling_rate_i;
	s2mpg1x_ext_samp_rate ext_sampling_rate_i;
};

struct odpm_channel_data {
	int rail_i;
	bool enabled;

	u64 measurement_start_ms;
	u64 acc_power_uW_sec;
};

/**
 * dynamic struct odpm_info
 */
struct odpm_info {
	struct odpm_chip chip;
	void *meter; /* Parent meter device data */
	struct i2c_client *i2c;
	struct mutex lock; /* Global HW lock */

	struct odpm_channel_data channels[ODPM_CHANNEL_MAX];

	struct workqueue_struct *work_queue;
	struct work_struct work_refresh;
	struct timer_list timer_refresh;
	unsigned long jiffies_last_poll;
};

/**
 * IIO driver specific channel configurations
 */
#define ODPM_ACC_CHANNEL(_index)                                               \
	{                                                                      \
		.type = IIO_ENERGY, .indexed = 1, .channel = (_index),         \
		.info_mask_separate = BIT(IIO_CHAN_INFO_AVERAGE_RAW),          \
		.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),       \
	}

static const struct iio_chan_spec s2mpg1x_single_channel[ODPM_CHANNEL_MAX] = {
	ODPM_ACC_CHANNEL(0), ODPM_ACC_CHANNEL(1), ODPM_ACC_CHANNEL(2),
	ODPM_ACC_CHANNEL(3), ODPM_ACC_CHANNEL(4), ODPM_ACC_CHANNEL(5),
	ODPM_ACC_CHANNEL(6), ODPM_ACC_CHANNEL(7),
};

static int odpm_take_snapshot(struct odpm_info *info);
static int odpm_take_snapshot_locked(struct odpm_info *info);

static u64 odpm_get_timestamp_now_ms(void)
{
	return ktime_get_boottime_ns() / 1000000;
}

static u64 odpm_get_timestamp_offset_ms(struct odpm_info *info)
{
	u64 time_ms_raw = odpm_get_timestamp_now_ms();

	if (time_ms_raw >= info->chip.acc_timestamp_offset)
		return time_ms_raw - info->chip.acc_timestamp_offset;
	else
		return 0;
}

static int odpm_io_set_channel(struct odpm_info *info, int channel)
{
	int ret = -1;
	int rail_i = info->channels[channel].rail_i;

	SWITCH_METER_FUNC(info, meter_set_muxsel, channel,
			  info->chip.rails[rail_i].mux_select);
	return ret;
}

static int odpm_io_set_int_sample_rate(struct odpm_info *info,
				       s2mpg1x_int_samp_rate hz)
{
	return s2mpg1x_meter_set_int_samp_rate(info->chip.hw_id, info->i2c, hz);
}

static int odpm_io_set_ext_sample_rate(struct odpm_info *info,
				       s2mpg1x_ext_samp_rate hz)
{
	return s2mpg1x_meter_set_ext_samp_rate(info->chip.hw_id, info->i2c, hz);
}

static int odpm_io_set_meter_on(struct odpm_info *info, bool is_on)
{
	int ret = -1;

	SWITCH_METER_FUNC(info, meter_onoff, is_on);
	return ret;
}

static int odpm_io_set_ext_meter_on(struct odpm_info *info, bool is_on)
{
	int ret = -1;

	SWITCH_METER_FUNC(info, ext_meter_onoff, is_on);
	return ret;
}

static int odpm_io_set_ext_channels_en(struct odpm_info *info, u8 channels)
{
	return s2mpg1x_meter_set_ext_channel_en(info->chip.hw_id, info->i2c,
						channels);
}

static int odpm_io_set_buck_channels_en(struct odpm_info *info, u8 *channels,
					int num_bytes)
{
	/* Disable BUCK5M if necessary (A0-specific) */
	if (info->chip.hw_id == ID_S2MPG10 &&
	    info->chip.hw_rev == S2MPG10_EVT0) {
		channels[0] &= ~0x10;
	}

	return s2mpg1x_meter_set_buck_channel_en(info->chip.hw_id, info->i2c,
						 channels, num_bytes);
}

static int odpm_io_send_blank_async(struct odpm_info *info,
				    unsigned long *jiffies)
{
	return s2mpg1x_meter_set_async_blocking(info->chip.hw_id, info->i2c,
						jiffies);
}

static int odpm_io_update_enable_bits(struct odpm_info *info)
{
	u8 ext_channels_en = 0;
	u8 buck_channels_en[ODPM_BUCK_EN_BYTES] = { 0 };
	int ch;
	int ret;

	for (ch = 0; ch < ODPM_CHANNEL_MAX; ch++) {
		if (info->channels[ch].enabled) {
			int rail_i = info->channels[ch].rail_i;
			struct odpm_rail_data *rail = &info->chip.rails[rail_i];

			u8 channel_en_i = rail->channel_en_index;
			int channel_offset_byte = rail->channel_en_byte_offset;

			switch (rail->type) {
			case ODPM_RAIL_TYPE_SHUNT:
				ext_channels_en |= channel_en_i;
				break;
			case ODPM_RAIL_TYPE_REGULATOR_BUCK:
				if (channel_offset_byte < ODPM_BUCK_EN_BYTES) {
					buck_channels_en[channel_offset_byte] |=
						channel_en_i;
				}
				break;
			case ODPM_RAIL_TYPE_REGULATOR_LDO:
			default:
				break;
			}
		}
	}

	ret = odpm_io_set_ext_channels_en(info, ext_channels_en);
	if (ret < 0)
		return ret;

	return odpm_io_set_buck_channels_en(info, buck_channels_en,
					    ODPM_BUCK_EN_BYTES);
}

int odpm_configure_chip(struct odpm_info *info)
{
	int ch;

	/* TODO(stayfan): b/156107234
	 * error conditions
	 */
	odpm_io_set_int_sample_rate(info, info->chip.int_sampling_rate_i);
	odpm_io_set_ext_sample_rate(info, info->chip.ext_sampling_rate_i);
	for (ch = 0; ch < ODPM_CHANNEL_MAX; ch++) {
		if (info->channels[ch].enabled)
			odpm_io_set_channel(info, ch);
	}

	odpm_io_update_enable_bits(info);

	odpm_io_set_meter_on(info, true);
	odpm_io_set_ext_meter_on(info, true);

	return 0;
}

int odpm_configure_start_measurement(struct odpm_info *info)
{
	unsigned long jiffies_capture = 0;
	int ch;

	/* For s2mpg1x chips, clear ACC registers */
	int ret = odpm_io_send_blank_async(info, &jiffies_capture);

	/* This timestamp becomes the offset (to match measurement start ms) */
	info->chip.acc_timestamp_offset = odpm_get_timestamp_now_ms();

	/* Initialize boot measurement time to 0 */
	for (ch = 0; ch < ODPM_CHANNEL_MAX; ch++) {
		if (info->channels[ch].enabled)
			info->channels[ch].measurement_start_ms = 0;
	}

	info->jiffies_last_poll = jiffies_capture;

	return ret;
}

void odpm_periodic_refresh_timeout(struct timer_list *t)
{
	int ret;
	struct odpm_info *info =
		container_of(t, struct odpm_info, timer_refresh);
	ret = mod_timer(&info->timer_refresh,
			jiffies +
			msecs_to_jiffies(info->chip.max_refresh_time_ms));
	if (ret < 0)
		pr_err("odpm: Refresh timer cannot be modified!\n");

	/* schedule the periodic reading from the chip */
	queue_work(info->work_queue, &info->work_refresh);
}

static void odpm_periodic_refresh_work(struct work_struct *work)
{
	struct odpm_info *info =
		container_of(work, struct odpm_info, work_refresh);


	if (odpm_take_snapshot(info) < 0)
		pr_err("odpm: Cannot refresh %s registers periodically!\n",
		       info->chip.name);
	else
		pr_info("odpm: Refreshed %s registers!\n", info->chip.name);
}

static void odpm_periodic_refresh_setup(struct odpm_info *info)
{
	info->work_queue = create_workqueue("wq_odpm");
	INIT_WORK(&info->work_refresh, odpm_periodic_refresh_work);

	/* setup the latest moment for reading the regs before saturation */
	/* register the timer */
	timer_setup(&info->timer_refresh, odpm_periodic_refresh_timeout, 0);
	info->timer_refresh.expires =
		jiffies + msecs_to_jiffies(info->chip.max_refresh_time_ms);
	add_timer(&info->timer_refresh);
}

static bool odpm_match_int_sample_rate(struct odpm_info *info, u32 sample_rate,
				       int *index)
{
	bool success = false;
	int i;

	for (i = 0; i < info->chip.sample_rate_int_count; i++) {
		if (sample_rate == info->chip.sample_rate_int_uhz[i]) {
			*index = i;
			success = true;
			break;
		}
	}

	return success;
}

static bool odpm_match_ext_sample_rate(struct odpm_info *info, u32 sample_rate,
				       int *index)
{
	bool success = false;
	int i;

	for (i = 0; i < info->chip.sample_rate_ext_count; i++) {
		if (sample_rate == info->chip.sample_rate_ext_uhz[i]) {
			*index = i;
			success = true;
			break;
		}
	}

	return success;
}

static int odpm_parse_dt_rail(struct odpm_rail_data *rail_data,
			      struct device_node *node)
{
	if (!node->name) {
		pr_err("odpm: cannot read node name\n");
		return -EINVAL;
	}
	rail_data->name = node->name;

	/* If the read fails, the pointers are not assigned; thus the pointers
	 * are left NULL if the values don't exist in the DT
	 */
	of_property_read_string(node, "schematic-name",
				&rail_data->schematic_name);
	of_property_read_string(node, "subsys-name", &rail_data->subsys_name);

	if (of_property_read_bool(node, "external_rail"))
		rail_data->type = ODPM_RAIL_TYPE_SHUNT;
	else if (of_property_read_bool(node, "buck_rail"))
		rail_data->type = ODPM_RAIL_TYPE_REGULATOR_BUCK;
	else
		rail_data->type = ODPM_RAIL_TYPE_REGULATOR_LDO;

	if (of_property_read_u32(node, "channel-mux-selection",
				 &rail_data->mux_select)) {
		pr_err("odpm: cannot read channel-mux-selection\n");
		return -EINVAL;
	}

	switch (rail_data->type) {
	case ODPM_RAIL_TYPE_SHUNT: {
		u32 channel_en_index;

		if (of_property_read_u32(node, "shunt-res-uohms",
					 &rail_data->shunt_uohms)) {
			pr_err("odpm: cannot read shunt-res-uohms\n");
			return -EINVAL;
		}

		if (of_property_read_u32(node, "channel-en-index",
					 &channel_en_index)) {
			pr_err("odpm: cannot read channel-en-index\n");
			return -EINVAL;
		}
		rail_data->channel_en_index = channel_en_index;
	} break;
	case ODPM_RAIL_TYPE_REGULATOR_BUCK: {
		u32 channel_en_index;
		int channel_en_byte_offset;

		if (of_property_read_u32(node, "channel-en-index",
					 &channel_en_index)) {
			pr_err("odpm: cannot read channel-en-index\n");
			return -EINVAL;
		}
		rail_data->channel_en_index = channel_en_index;

		if (of_property_read_u32(node, "channel-en-byte-offset",
					 &channel_en_byte_offset)) {
			pr_err("odpm: cannot read channel-en-byte-offset\n");
			return -EINVAL;
		}
		rail_data->channel_en_byte_offset = channel_en_byte_offset;
	} break;

	case ODPM_RAIL_TYPE_REGULATOR_LDO:
	default:
		break;
	}

	return 0;
}

static int odpm_parse_dt_rails(struct device *dev, struct odpm_info *info,
			       struct device_node *pmic_np)
{
	struct device_node *iter_np, *regulators_np;
	bool use_regulators_as_rails = false;
	struct odpm_rail_data *rail_data;
	int rail_i = 0, num_rails = 0;

	struct device_node *rails_np = of_find_node_by_name(pmic_np, "rails");

	if (!rails_np) {
		pr_err("odpm: cannot find rails DT node!\n");
		return -EINVAL;
	}

	/* Count rails */
	use_regulators_as_rails =
		of_property_read_bool(rails_np, "use-regulators-as-rails");
	if (use_regulators_as_rails) {
		regulators_np = of_find_node_by_name(pmic_np, "regulators");
		if (!regulators_np) {
			pr_err("odpm: Could not find regulators sub-node\n");
			return -EINVAL;
		}

		num_rails += of_get_child_count(regulators_np);
	}
	num_rails += of_get_child_count(rails_np);
	if (num_rails <= 0) {
		pr_err("odpm: Could not find any rails\n");
		return -EINVAL;
	}
	info->chip.num_rails = num_rails;

	/* Allocate/Initialize rails */
	rail_data =
		devm_kzalloc(dev, sizeof(*rail_data) * num_rails, GFP_KERNEL);
	if (!rail_data) {
		dev_err(dev, "odpm: could not allocate memory for rail data\n");
		return -ENOMEM;
	}
	info->chip.rails = rail_data;

	/* Populate rail data */
	if (use_regulators_as_rails) {
		for_each_child_of_node(regulators_np, iter_np) {
			int ret =
				odpm_parse_dt_rail(&rail_data[rail_i], iter_np);
			if (ret != 0)
				return ret;
			rail_i++;
		}
	}
	for_each_child_of_node(rails_np, iter_np) {
		int ret = odpm_parse_dt_rail(&rail_data[rail_i], iter_np);

		if (ret != 0)
			return ret;
		rail_i++;
	}

	/* Confidence check rail count */
	if (rail_i != num_rails) {
		pr_err("odpm: expected %d rails, got %d\n", num_rails, rail_i);
		return -EINVAL;
	}

	return 0;
}

static int odpm_parse_dt_channels(struct odpm_info *info,
				  struct device_node *channels_np)
{
	int rail_i = 0, channel_i = 0;
	int num_channels = of_get_child_count(channels_np);
	struct device_node *iter_np;

	/* Check channel count */
	if (num_channels != ODPM_CHANNEL_MAX) {
		pr_err("odpm: expected %d channels, got %d\n", ODPM_CHANNEL_MAX,
		       num_channels);
		return -EINVAL;
	}

	/* Parse channels */
	for_each_child_of_node(channels_np, iter_np) {
		const char *rail_name;

		/* Explicitly set enabled to false until we find the
		 * associated rail
		 */
		info->channels[channel_i].enabled = false;

		/* Read rail name */
		if (of_property_read_string(iter_np, "rail-name", &rail_name)) {
			pr_err("odpm: invalid rail-name value on %s\n",
			       iter_np->full_name);
			return -EINVAL;
		}

		/* Match rail name */
		for (rail_i = 0; rail_i < info->chip.num_rails; rail_i++) {
			if (!strcmp(info->chip.rails[rail_i].name, rail_name)) {
				info->channels[channel_i].rail_i = rail_i;
				break;
			}
		}

		if (rail_i == info->chip.num_rails) {
			pr_err("odpm: Could not find rail-name %s\n",
			       rail_name);
			return -EINVAL;
		}

		/* Check if the channel is enabled or not */
		info->channels[channel_i].enabled =
			of_property_read_bool(iter_np, "channel_enabled");

		channel_i++;
	}

	return 0;
}

static int odpm_parse_dt(struct device *dev, struct odpm_info *info)
{
	struct device_node *pmic_np = dev->parent->parent->of_node;
	struct device_node *odpm_np, *channels_np;
	u32 sample_rate;
	int sample_rate_i;
	int ret;

	if (!pmic_np) {
		pr_err("odpm: cannot find parent DT node!\n");
		return -EINVAL;
	}
	odpm_np = of_find_node_by_name(pmic_np, "odpm");
	if (!odpm_np) {
		pr_err("odpm: cannot find main DT node!\n");
		return -EINVAL;
	}
	channels_np = of_find_node_by_name(pmic_np, "channels");
	if (!channels_np) {
		pr_err("odpm: cannot find channels DT node!\n");
		return -EINVAL;
	}

	/* Get main properties; sample-rate, chip-name, etc. */
	if (of_property_read_string(odpm_np, "chip-name", &info->chip.name)) {
		pr_err("odpm: cannot read sample rate value\n");
		return -EINVAL;
	}
	if (of_property_read_u32(odpm_np, "sample-rate-uhz", &sample_rate)) {
		pr_err("odpm: cannot read sample rate value\n");
		return -EINVAL;
	}
	if (!odpm_match_int_sample_rate(info, sample_rate, &sample_rate_i)) {
		pr_err("odpm: cannot parse sample rate value %d\n",
		       sample_rate);
		return -EINVAL;
	}
	info->chip.int_sampling_rate_i = sample_rate_i;
	if (of_property_read_u32(odpm_np, "sample-rate-external-uhz",
				 &sample_rate)) {
		pr_err("odpm: cannot read external sample rate value\n");
		return -EINVAL;
	}
	if (!odpm_match_ext_sample_rate(info, sample_rate, &sample_rate_i)) {
		pr_err("odpm: cannot parse external sample rate value %d\n",
		       sample_rate);
		return -EINVAL;
	}
	info->chip.ext_sampling_rate_i = sample_rate_i;
	if (of_property_read_u32(odpm_np, "max-refresh-time-ms",
				 &info->chip.max_refresh_time_ms)) {
		pr_err("odpm: cannot read max refresh time value\n");
		return -EINVAL;
	}

	ret = odpm_parse_dt_rails(dev, info, pmic_np);
	if (ret != 0)
		return ret;

	return odpm_parse_dt_channels(info, channels_np);
}

static u64 odpm_calculate_uW_sec(struct odpm_info *info, int rail_i,
				 u64 acc_data, u32 int_sampling_frequency_uhz)
{
	u64 sampling_period_ms_iq30;
	u32 sampling_period_ms_iq22;
	u32 resolution_mW_iq30 = INVALID_RESOLUTION;
	u32 ret = 0;
	__uint128_t power_acc_mW_iq30;
	__uint128_t power_acc_uW_s_iq52;

	switch (info->chip.rails[rail_i].type) {
	case ODPM_RAIL_TYPE_REGULATOR_BUCK:
	case ODPM_RAIL_TYPE_REGULATOR_LDO:
	default: {
		SWITCH_CHIP_FUNC(info, muxsel_to_power_resolution,
				 info->chip.rails[rail_i].mux_select);

	} break;
	case ODPM_RAIL_TYPE_SHUNT: {
		u64 resolution_W_iq60;

		/* Losing a fraction of resolution performing u64 divisions,
		 * as there is no support for 128 bit divisions
		 */
		resolution_W_iq60 = ((u64)EXTERNAL_RESOLUTION_VRAIL *
				     (u64)EXTERNAL_RESOLUTION_VSHUNT *
				     (u64)EXTERNAL_RESOLUTION_TRIM) /
				    info->chip.rails[rail_i].shunt_uohms;

		/* Scale back to iq30 (with conversion to mW) */
		ret = _IQ30_to_int(resolution_W_iq60 * 1000);

	} break;
	}
	resolution_mW_iq30 = ret;

	/* Maintain as much precision as possible computing period in ms */
	sampling_period_ms_iq30 =
		_IQ30(u64, (u64)(1000 * 1000000)) / int_sampling_frequency_uhz;

	/* Allocate 10-bits in u32 for sample rate in ms (max: 1023 ms) */
	sampling_period_ms_iq22 = _IQ30_to_IQ22(sampling_period_ms_iq30);

	/* Use 128 bit to prevent overflow on multiplied iq values */
	power_acc_mW_iq30 = (__uint128_t)acc_data * resolution_mW_iq30;
	power_acc_uW_s_iq52 = power_acc_mW_iq30 * sampling_period_ms_iq22;

	/* Scale back u128 from iq to value */
	return _IQ22_to_int(_IQ30_to_int(power_acc_uW_s_iq52));
}

#if ODPM_PRINT_ESTIMATED_CLOCK_SKEW
static void odpm_print_clock_skew(struct odpm_info *info, u64 elapsed_ms,
				  u32 acc_count)
{
	u64 uHz_estimated = ((u64)acc_count * 1000 * 1000000) / elapsed_ms;
	u64 uHz =
		info->chip.sample_rate_int_uhz[info->chip.int_sampling_rate_i];

	u64 ratio_u = (1000000 * uHz_estimated) / uHz;
	s64 pct_u = (((s64)ratio_u - (1 * 1000000)) * 100);

	pr_info("odpm: %s: internal clock skew: %d.%d %%\n", info->chip.name,
		pct_u / 1000000, abs(pct_u) % 1000000);
}
#endif

static u32 odpm_estimate_sampling_frequency(struct odpm_info *info,
					    unsigned int elapsed_ms,
					    unsigned int elapsed_refresh_ms,
					    u32 acc_count)
{
	/* b/156680376
	 * Instead of using the configured sampling rate, we want to approximate
	 * the sampling rate based on acc_count
	 */

	u64 sampling_frequency_estimated_uhz;

	int sampling_rate_i = info->chip.int_sampling_rate_i;
	int sampling_frequency_table_uhz =
		info->chip.sample_rate_int_uhz[sampling_rate_i];
	u64 freq_lower_bound;
	u64 freq_upper_bound;

	if (elapsed_ms == 0) {
		pr_err("odpm: %s: elapsed time is 0 ms\n", info->chip.name);

		/* Fall back to configured frequency */
		return sampling_frequency_table_uhz;
	}

#if ODPM_PRINT_ESTIMATED_CLOCK_SKEW
	odpm_print_clock_skew(info, elapsed_ms, acc_count);
#endif

	/* Estimate sampling rate based on acc_count */
	sampling_frequency_estimated_uhz =
		((u64)acc_count * 1000 * 1000000) / elapsed_ms;

	/**
	 * 100 ms check on register refresh...
	 * We want to verify that the transaction time isn't too long, as the
	 * process may have been pre-empted between the command to refresh
	 * registers and when jiffies were captured (jiffies_after).
	 */
	if (elapsed_refresh_ms >= 100) {
		pr_err("odpm: %s: refresh registers took too long; %ld ms\n",
		       info->chip.name, elapsed_refresh_ms);

		/* Fall back to configured frequency */
		return sampling_frequency_table_uhz;
	}

	/* +-50% error bounds check */
	freq_lower_bound = sampling_frequency_table_uhz / 2;
	freq_upper_bound =
		sampling_frequency_table_uhz + sampling_frequency_table_uhz / 2;
	if (sampling_frequency_estimated_uhz < freq_lower_bound ||
	    sampling_frequency_estimated_uhz > freq_upper_bound) {
		pr_err("odpm: %s: clock error too large! fsel: %d, fest: %ld\n",
		       info->chip.name, sampling_frequency_table_uhz,
		       sampling_frequency_estimated_uhz);

		/* Fall back to configured frequency */
		return sampling_frequency_table_uhz;
	}

	return sampling_frequency_estimated_uhz;
}

static int odpm_refresh_registers(struct odpm_info *info)
{
	int ch;
	int ret = 0;

	u64 acc_data[ODPM_CHANNEL_MAX];
	u32 acc_count = 0;
	u64 measurement_timestamp_ms;
	u32 sampling_frequency_uhz;

	unsigned long jiffies_previous_sample;
	unsigned long jiffies_before_async;
	unsigned long jiffies_after_async;

	unsigned int elapsed_ms;
	unsigned int elapsed_refresh_ms;

	jiffies_before_async = jiffies;

	SWITCH_METER_FUNC(info, meter_load_measurement, S2MPG1X_METER_POWER,
			  acc_data, &acc_count,
			  &jiffies_after_async);
	measurement_timestamp_ms = odpm_get_timestamp_offset_ms(info);

	if (ret < 0) {
		pr_err("odpm: %s: i2c error; count not measure interval\n",
		       info->chip.name);
		goto exit_refresh;
	}

	/* Store timestamps - the rest of the function will succeed */
	jiffies_previous_sample = info->jiffies_last_poll;
	info->jiffies_last_poll = jiffies_after_async;
	info->chip.acc_timestamp = measurement_timestamp_ms;

	elapsed_ms = jiffies_to_msecs(jiffies_after_async -
				      jiffies_previous_sample);
	elapsed_refresh_ms = jiffies_to_msecs(jiffies_after_async -
					      jiffies_before_async);
	sampling_frequency_uhz =
		odpm_estimate_sampling_frequency(info,
						 elapsed_ms,
						 elapsed_refresh_ms,
						 acc_count);

	for (ch = 0; ch < ODPM_CHANNEL_MAX; ch++) {
		const int rail_i = info->channels[ch].rail_i;
		const u64 uW_sec =
			odpm_calculate_uW_sec(info, rail_i, acc_data[ch],
					      (u32)sampling_frequency_uhz);

		info->channels[ch].acc_power_uW_sec += uW_sec;
	}

exit_refresh:
	return ret;
}

static int odpm_take_snapshot(struct odpm_info *info)
{
	int ret;

	mutex_lock(&info->lock);
	ret = odpm_take_snapshot_locked(info);
	mutex_unlock(&info->lock);
	return ret;
}

static int odpm_take_snapshot_locked(struct odpm_info *info)
{
	int ret_refresh = 0;
	int ret_timer = 0;

	/* check if the minimal elapsed time has passed and if so,
	 * re-read the chip, otherwise the cached info is just fine
	 */
	unsigned long future_time = info->jiffies_last_poll +
				    msecs_to_jiffies(ODPM_MIN_POLLING_TIME_MS);
	if (time_after(jiffies, future_time)) {
		unsigned long future_timer =
			info->jiffies_last_poll +
			msecs_to_jiffies(info->chip.max_refresh_time_ms);

		/* we need to re-read the chip values
		 */
		ret_refresh = odpm_refresh_registers(info);
		if (ret_refresh < 0)
			pr_err("odpm: could not refresh registers!\n");

		/* re-schedule the work for the read registers timeout
		 * (to prevent chip regs saturation)
		 */
		ret_timer = mod_timer(&info->timer_refresh, future_timer);
		if (ret_timer < 0)
			pr_err("odpm: read timer can't be modified!\n");
	}

	if (ret_refresh < 0)
		return ret_refresh;
	else
		return ret_timer;
}

/**
 * Verify and return sampling rate
 *
 * @return Sampling rate if >= 0, else error number
 */
static int sampling_rate_verify(const char *buf)
{
	int hz;
	int i;
	char decimal_str[ODPM_FREQ_DECIMAL_UHZ_STR_LEN_MAX + 1];
	int decimal_len = 0;
	int decimal_uHz = 0;

	decimal_str[0] = 0;

	if (sscanf(buf, "%d.%" xstr(ODPM_FREQ_DECIMAL_UHZ_STR_LEN_MAX) "s", &hz,
		   decimal_str) != 2) {
		decimal_str[0] = 0;

		if (kstrtoint(buf, 10, &hz)) {
			pr_err("odpm: sampling rate is not a number\n");
			return -EINVAL;
		}
	}

	decimal_len = strlen(decimal_str);

	/* Got a decimal value */
	if (decimal_len > 0) {
		const int power_of_10 =
			(ODPM_FREQ_DECIMAL_UHZ_STR_LEN_MAX - decimal_len);

		/* Parse integer */
		if (kstrtoint(decimal_str, 10, &decimal_uHz)) {
			pr_err("odpm: sampling rate decimal is not a number\n");
			return -EINVAL;
		}

		/* Multiply by powers of 10 */
		for (i = 0; i < power_of_10; i++)
			decimal_uHz *= 10;
	}

	return 1000000 * hz + decimal_uHz;
}

static ssize_t sampling_rate_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);

	return scnprintf(buf, PAGE_SIZE, "%d.%d\n",
		info->chip.sample_rate_int_uhz[info->chip.int_sampling_rate_i] /
			1000000,
		info->chip.sample_rate_int_uhz[info->chip.int_sampling_rate_i] %
			1000000);
}

static ssize_t sampling_rate_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);

	int new_sampling_rate_i;
	int new_sampling_rate;

	int ret = sampling_rate_verify(buf);

	if (ret < 0)
		return ret;

	new_sampling_rate = ret;
	if (!odpm_match_int_sample_rate(info, new_sampling_rate,
					&new_sampling_rate_i)) {
		pr_err("odpm: cannot parse new sample rate value; %d uHz\n",
		       new_sampling_rate);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	/* Send a refresh and store values */
	if (odpm_take_snapshot_locked(info) < 0) {
		pr_err("odpm: cannot refresh to apply new sample rate");
		goto sampling_rate_store_exit;
	}

	info->chip.int_sampling_rate_i = new_sampling_rate_i;
	odpm_io_set_int_sample_rate(info, new_sampling_rate_i);

sampling_rate_store_exit:
	mutex_unlock(&info->lock);

	return count;
}

static ssize_t ext_sampling_rate_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);

	/* No locking needed until sampling rates are dynamic */
	return scnprintf(buf, PAGE_SIZE, "%d.%d\n",
		info->chip.sample_rate_ext_uhz[info->chip.ext_sampling_rate_i] /
			1000000,
		info->chip.sample_rate_ext_uhz[info->chip.ext_sampling_rate_i] %
			1000000);
}

static ssize_t ext_sampling_rate_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);

	int new_sampling_rate_i;
	int new_sampling_rate;

	int ret = sampling_rate_verify(buf);

	if (ret < 0)
		return ret;

	new_sampling_rate = ret;
	if (!odpm_match_ext_sample_rate(info, new_sampling_rate,
					&new_sampling_rate_i)) {
		pr_err("odpm: cannot match new sample rate value; %d uHz\n",
		       new_sampling_rate);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	/* Send a refresh and store values */
	if (odpm_take_snapshot_locked(info) < 0) {
		pr_err("odpm: cannot refresh to apply new ext sample rate");
		goto ext_sampling_rate_store_exit;
	}

	info->chip.ext_sampling_rate_i = new_sampling_rate_i;
	odpm_io_set_ext_sample_rate(info, new_sampling_rate_i);

ext_sampling_rate_store_exit:
	mutex_unlock(&info->lock);

	return count;
}

static ssize_t energy_value_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);
	ssize_t count = 0;
	int ch;

	/* take snapshot */
	mutex_lock(&info->lock);
	if (odpm_take_snapshot_locked(info) < 0) {
		pr_err("odpm: cannot retrieve energy values");
		goto energy_value_show_exit;
	}

	/**
	 * Output format:
	 * t=<Measurement timestamp, ms>
	 * CH<N>(T=<Duration, ms>)[<Schematic name>], <Accumulated Energy, uWs>
	 */
	count += scnprintf(buf + count, PAGE_SIZE - count, "t=%ld\n",
			   info->chip.acc_timestamp);

	for (ch = 0; ch < ODPM_CHANNEL_MAX; ch++) {
		int rail_i = info->channels[ch].rail_i;
		u64 start_ms = info->channels[ch].measurement_start_ms;
		u64 duration_ms = 0;

		if (info->chip.acc_timestamp >= start_ms)
			duration_ms = info->chip.acc_timestamp - start_ms;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "CH%d(T=%ld)[%s], %ld\n", ch,
				   duration_ms,
				   info->chip.rails[rail_i].schematic_name,
				   info->channels[ch].acc_power_uW_sec);
	}

energy_value_show_exit:
	mutex_unlock(&info->lock);

	return count;
}

static ssize_t available_rails_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);
	int rail_i;
	ssize_t count = 0;

	/* No locking needed for rail specific information */
	for (rail_i = 0; rail_i < info->chip.num_rails; rail_i++) {
		struct odpm_rail_data *rail = &info->chip.rails[rail_i];

		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "%s(%s):%s\n", rail->name,
				   rail->schematic_name, rail->subsys_name);
	}

	return count;
}

static ssize_t enabled_rails_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);
	ssize_t count = 0;
	int ch;

	mutex_lock(&info->lock);
	for (ch = 0; ch < ODPM_CHANNEL_MAX; ch++) {
		if (info->channels[ch].enabled) {
			size_t rail_i = info->channels[ch].rail_i;
			struct odpm_rail_data *rail = &info->chip.rails[rail_i];

			count += scnprintf(buf + count, PAGE_SIZE - count,
					   "CH%d[%s]:%s\n", ch,
					   rail->schematic_name,
					   rail->subsys_name);
		}
	}
	mutex_unlock(&info->lock);

	return count;
}

static ssize_t enabled_rails_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);

	char rail_name[ODPM_RAIL_NAME_STR_LEN_MAX + 1];
	int channel = -1;
	bool channel_valid = false;
	int rail_i;
	int scan_result =
		sscanf(buf, "CH%d=%" xstr(ODPM_RAIL_NAME_STR_LEN_MAX) "s",
		       &channel, rail_name);

	if (scan_result == 2 && channel >= 0 && channel < ODPM_CHANNEL_MAX) {
		for (rail_i = 0; rail_i < info->chip.num_rails; rail_i++) {
			if (strcmp(info->chip.rails[rail_i].name, rail_name) ==
			    0) {
				channel_valid = true;
				break;
			}
		}
	}

	if (channel_valid) {
		int current_rail;
		int new_rail = rail_i;

		mutex_lock(&info->lock);

		/* Send a refresh and store values */
		if (odpm_take_snapshot_locked(info) < 0) {
			pr_err("odpm: cannot refresh values to swap rails");
			goto enabled_rails_store_exit;
		}

		current_rail = info->channels[channel].rail_i;

		/* Capture measurement time for current rail */
		info->chip.rails[current_rail].measurement_start_ms_cached =
			info->channels[channel].measurement_start_ms;
		info->chip.rails[current_rail].measurement_stop_ms =
			info->chip.acc_timestamp;

		/* Reset stored energy for channel */
		info->chip.rails[current_rail].acc_power_uW_sec_cached =
			info->channels[channel].acc_power_uW_sec;
		info->channels[channel].acc_power_uW_sec = 0;

		/* Assign new rail to channel */
		info->channels[channel].rail_i = new_rail;

		/* Rail is swapped, update en bits */
		odpm_io_update_enable_bits(info);

		/* Update rail muxsel */
		odpm_io_set_channel(info, channel);

		/* Record measurement start time / reset stop time */
		info->channels[channel].measurement_start_ms =
			odpm_get_timestamp_offset_ms(info);
		info->chip.rails[new_rail].measurement_stop_ms = 0;

enabled_rails_store_exit:
		mutex_unlock(&info->lock);

		return count;
	}

	/* The channel syntax was invalid if this is reached. */
	return -EINVAL;
}

static ssize_t measurement_start_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);
	int ch;
	ssize_t count = 0;

	mutex_lock(&info->lock);
	for (ch = 0; ch < ODPM_CHANNEL_MAX; ch++) {
		int rail_i = info->channels[ch].rail_i;

		if (info->channels[ch].enabled) {
			count += scnprintf(buf + count, PAGE_SIZE - count,
				"CH%d[%s], %ld\n", ch,
				info->chip.rails[rail_i].schematic_name,
				info->channels[ch].measurement_start_ms);
		}
	}
	mutex_unlock(&info->lock);

	return count;
}

static ssize_t measurement_stop_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct odpm_info *info = iio_priv(indio_dev);
	int rail_i;
	ssize_t count = 0;

	mutex_lock(&info->lock);
	for (rail_i = 0; rail_i < info->chip.num_rails; rail_i++) {
		struct odpm_rail_data *rail = &info->chip.rails[rail_i];

		if (rail->measurement_stop_ms != 0) {
			count += scnprintf(buf + count, PAGE_SIZE - count,
					   "%s(%s), %ld, %ld, %ld\n",
					   rail->name, rail->schematic_name,
					   rail->measurement_start_ms_cached,
					   rail->measurement_stop_ms,
					   rail->acc_power_uW_sec_cached);
		}
	}
	mutex_unlock(&info->lock);

	return count;
}

static IIO_DEVICE_ATTR_RW(ext_sampling_rate, 0);
static IIO_DEVICE_ATTR_RW(sampling_rate, 0);
static IIO_DEVICE_ATTR_RO(energy_value, 0);
static IIO_DEVICE_ATTR_RO(available_rails, 0);
static IIO_DEVICE_ATTR_RW(enabled_rails, 0);
static IIO_DEVICE_ATTR_RO(measurement_start, 0);
static IIO_DEVICE_ATTR_RO(measurement_stop, 0);

/**
 * TODO(stayfan): b/156109194
 * Add attributes that allow you to dynamically configure channels
 * You should be able to configure all 8 channels and initiate the config change
 * from a sysfs file, and the previous values from the static device tree config
 * should be buffered while the dynamic config is in place. There should also be
 * a sysfs file to revert back to the device tree config with the previous
 * accumulated values.
 */

#define ODPM_DEV_ATTR(name) (&iio_dev_attr_##name.dev_attr.attr)

static struct attribute *odpm_custom_attributes[] = {
	ODPM_DEV_ATTR(sampling_rate),	 ODPM_DEV_ATTR(ext_sampling_rate),
	ODPM_DEV_ATTR(energy_value),	 ODPM_DEV_ATTR(available_rails),
	ODPM_DEV_ATTR(enabled_rails),	 ODPM_DEV_ATTR(measurement_start),
	ODPM_DEV_ATTR(measurement_stop), NULL
};

static const struct attribute_group odpm_group = {
	.attrs = odpm_custom_attributes,
};

/* TODO(stayfan): b/156108825
 * Understand the requirements for read_raw/write_raw
 * Populate if these functions are useful to userland
 */
static int odpm_read_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan, int *val, int *val2,
			 long mask)
{
	int ret = 0;

	switch (mask) {
	case IIO_CHAN_INFO_AVERAGE_RAW:
		switch (chan->type) {
		case IIO_ENERGY:
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return ret;
}

static int odpm_write_raw(struct iio_dev *indio_dev,
			  struct iio_chan_spec const *chan, int val, int val2,
			  long mask)
{
	int ret = -EINVAL;

	if (iio_buffer_enabled(indio_dev))
		return -EBUSY;

	return ret;
}

static const struct iio_info odpm_iio_info = {
	.attrs = &odpm_group,
	.read_raw = odpm_read_raw,
	.write_raw = odpm_write_raw,
};

static int odpm_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(&pdev->dev);
	struct odpm_info *info = iio_priv(indio_dev);
	int ret;

	ret = try_to_del_timer_sync(&info->timer_refresh);
	if (ret < 0) {
		pr_err("odpm: cannot delete the refresh timer\n");
		return ret;
	}
	if (info->work_queue) {
		cancel_work_sync(&info->work_refresh);
		flush_workqueue(info->work_queue);
		destroy_workqueue(info->work_queue);
	}

	/* free the channel attributes memory */
	kfree(indio_dev->channels);

	iio_device_unregister(indio_dev);

	return ret;
}

static void odpm_probe_init_device_specific(struct odpm_info *info, int id)
{
	/* s2mpg1x specific data */
	switch (id) {
	case ODPM_CHIP_S2MPG10:
	case ODPM_CHIP_S2MPG11:
		info->chip.id = id;

		info->chip.sample_rate_int_uhz =
			s2mpg1x_meter_get_int_samping_rate_table();
		info->chip.sample_rate_int_count = S2MPG1X_INT_FREQ_COUNT;
		info->chip.sample_rate_ext_uhz =
			s2mpg1x_meter_get_ext_samping_rate_table();
		info->chip.sample_rate_ext_count = S2MPG1X_EXT_FREQ_COUNT;
		break;
	}

	switch (id) {
	case ODPM_CHIP_S2MPG10: {
		struct s2mpg10_meter *meter = info->meter;
		struct s2mpg10_dev *pmic = dev_get_drvdata(meter->dev->parent);

		info->chip.hw_id = ID_S2MPG10;
		info->chip.hw_rev = pmic->pmic_rev;
		info->i2c = meter->i2c;
	} break;
	case ODPM_CHIP_S2MPG11: {
		struct s2mpg11_meter *meter = info->meter;
		struct s2mpg11_dev *pmic = dev_get_drvdata(meter->dev->parent);

		info->chip.hw_id = ID_S2MPG11;
		info->chip.hw_rev = pmic->pmic_rev;
		info->i2c = meter->i2c;
	} break;
	}
}

static int odpm_probe(struct platform_device *pdev)
{
	struct odpm_info *odpm_info;
	struct iio_dev *indio_dev;
	int ret;
	void *iodev;

	pr_info("odpm: %s: init\n", pdev->name);

	/* Allocate the memory for our private structure
	 * related to the chip info structure
	 */
	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*odpm_info));
	if (!indio_dev) {
		pr_err("odpm: Could not allocate device!\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, indio_dev);

	/* Point our chip info structure towards
	 * the address freshly allocated
	 */
	odpm_info = iio_priv(indio_dev);
	/* Make the link between the parent driver and the IIO */
	iodev = dev_get_drvdata(pdev->dev.parent);
	if (!iodev) {
		pr_err("odpm: Could not get parent data!\n");
		return -ENODEV;
	}
	odpm_info->meter = iodev;

	if (!pdev->id_entry) {
		pr_err("odpm: Could not find id_entry!\n");
		odpm_remove(pdev);
		return -ENODEV;
	}

	if (pdev->id_entry->driver_data >= ODPM_CHIP_COUNT) {
		pr_err("odpm: Could not identify driver!\n");
		odpm_remove(pdev);
		return -ENODEV;
	}

	/* Init device-specific data */
	odpm_probe_init_device_specific(odpm_info, pdev->id_entry->driver_data);

	/* Read device tree data */
	if (!pdev->dev.parent->parent ||
	    (!of_get_next_child(pdev->dev.parent->parent->of_node, NULL))) {
		pr_err("odpm: DT does not exist!\n");
		odpm_remove(pdev);
		return -EINVAL;
	}
	/* Note: This function will call devm_kzalloc() in order to
	 * dynamically allocate memory for the rails
	 */
	ret = odpm_parse_dt(&pdev->dev, odpm_info);
	if (ret != 0) {
		pr_err("odpm: DT parsing error!\n");
		odpm_remove(pdev);
		return ret;
	}

	/* Initialize other data in odpm_info */

	/* Configure ODPM channels based on device tree input */
	ret = odpm_configure_chip(odpm_info);
	if (ret < 0) {
		odpm_remove(pdev);
		return ret;
	}

	/* Start measurement of default rails */
	odpm_configure_start_measurement(odpm_info);

	/* Configure work to kick off every XHz */
	odpm_periodic_refresh_setup(odpm_info);

	/* Setup IIO */
	indio_dev->channels = s2mpg1x_single_channel;
	indio_dev->num_channels = ARRAY_SIZE(s2mpg1x_single_channel);
	indio_dev->info = &odpm_iio_info;
	indio_dev->name = pdev->name;
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret < 0) {
		odpm_remove(pdev);
		return ret;
	}

	mutex_init(&odpm_info->lock);

	pr_info("odpm: %s: init completed\n", pdev->name);

	return ret;
}

static const struct platform_device_id odpm_id[] = {
	{ "s2mpg10-odpm", ODPM_CHIP_S2MPG10 },
	{ "s2mpg11-odpm", ODPM_CHIP_S2MPG11 },
	{},
};
MODULE_DEVICE_TABLE(platform, odpm_id);

static struct platform_driver odpm_driver = {
	.driver = {
		   .name = "odpm",
		   .owner = THIS_MODULE,
		   },
	.probe = odpm_probe,
	.remove = odpm_remove,
	.id_table = odpm_id,
};

static int __init odpm_init(void)
{
	return platform_driver_register(&odpm_driver);
}

subsys_initcall(odpm_init);

static void __exit odpm_exit(void)
{
	platform_driver_unregister(&odpm_driver);
}

module_exit(odpm_exit);

MODULE_DESCRIPTION("IIO ODPM Driver");
MODULE_AUTHOR("Stephane Lee <stayfan@google.com>");
MODULE_LICENSE("GPL");
