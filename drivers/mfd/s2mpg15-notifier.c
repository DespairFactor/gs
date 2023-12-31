// SPDX-License-Identifier: GPL-2.0+
/*
 * s2mpg15-notifier.c - Interrupt controller support for S2MPG15
 *
 * Copyright (C) 2022 Samsung Electronics Co.Ltd
 */

#include <linux/err.h>
#include <linux/mfd/samsung/s2mpg15.h>
#include <linux/mfd/samsung/s2mpg15-register.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/wakeup_reason.h>
#include <linux/interrupt.h>
#include <linux/mfd/samsung/s2mpg1415.h>

static struct notifier_block sub_pmic_notifier;
static struct s2mpg15_dev *s2mpg15_global;

static u8 irq_reg_sub[S2MPG15_IRQ_GROUP_NR] = {0};

static const u8 s2mpg15_mask_reg[] = {
	[S2MPG15_IRQS_PMIC_INT1] = S2MPG15_PM_INT1M,
	[S2MPG15_IRQS_PMIC_INT2] = S2MPG15_PM_INT2M,
	[S2MPG15_IRQS_PMIC_INT3] = S2MPG15_PM_INT3M,
	[S2MPG15_IRQS_PMIC_INT4] = S2MPG15_PM_INT4M,
	[S2MPG15_IRQS_METER_INT1] = S2MPG15_METER_INT1M,
	[S2MPG15_IRQS_METER_INT2] = S2MPG15_METER_INT2M,
	[S2MPG15_IRQS_METER_INT3] = S2MPG15_METER_INT3M,
	[S2MPG15_IRQS_METER_INT4] = S2MPG15_METER_INT4M,
};

static struct i2c_client *get_i2c(struct s2mpg15_dev *s2mpg15,
				  enum s2mpg15_irq_source src)
{
	switch (src) {
	case S2MPG15_IRQS_PMIC_INT1 ... S2MPG15_IRQS_PMIC_INT4:
		return s2mpg15->pmic;
	case S2MPG15_IRQS_METER_INT1 ... S2MPG15_IRQS_METER_INT4:
		return s2mpg15->meter;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mpg15_irq_data {
	u8 mask;
	enum s2mpg15_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask) \
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct s2mpg15_irq_data s2mpg15_irqs[] = {
	DECLARE_IRQ(S2MPG15_IRQ_PWR_OFF_INT1, S2MPG15_IRQS_PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_ON_INT1, S2MPG15_IRQS_PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPG15_IRQ_INT120C_INT1, S2MPG15_IRQS_PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPG15_IRQ_INT140C_INT1, S2MPG15_IRQS_PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPG15_IRQ_TSD_INT1, S2MPG15_IRQS_PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPG15_IRQ_WRST_INT1, S2MPG15_IRQS_PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPG15_IRQ_WTSR_INT1, S2MPG15_IRQS_PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPG15_IRQ_SCL_SOFTRST_INT2, S2MPG15_IRQS_PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPG15_IRQ_UV_BB_INT2, S2MPG15_IRQS_PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPG15_IRQ_BB_NTR_DET_INT2, S2MPG15_IRQS_PMIC_INT2, 1 << 6),
	DECLARE_IRQ(S2MPG15_IRQ_WLWP_ACC_INT2, S2MPG15_IRQS_PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPG15_IRQ_OCP_B1S_INT3, S2MPG15_IRQS_PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B2S_INT3, S2MPG15_IRQS_PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B3S_INT3, S2MPG15_IRQS_PMIC_INT3, 1 << 2),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B4S_INT3, S2MPG15_IRQS_PMIC_INT3, 1 << 3),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B5S_INT3, S2MPG15_IRQS_PMIC_INT3, 1 << 4),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B6S_INT3, S2MPG15_IRQS_PMIC_INT3, 1 << 5),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B7S_INT3, S2MPG15_IRQS_PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B8S_INT3, S2MPG15_IRQS_PMIC_INT3, 1 << 7),

	DECLARE_IRQ(S2MPG15_IRQ_OCP_B9S_INT4, S2MPG15_IRQS_PMIC_INT4, 1 << 0),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B10S_INT4, S2MPG15_IRQS_PMIC_INT4, 1 << 1),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B11S_INT4, S2MPG15_IRQS_PMIC_INT4, 1 << 2),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_B12S_INT4, S2MPG15_IRQS_PMIC_INT4, 1 << 3),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_BDS_INT4, S2MPG15_IRQS_PMIC_INT4, 1 << 4),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_BAS_INT4, S2MPG15_IRQS_PMIC_INT4, 1 << 5),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_BCS_INT4, S2MPG15_IRQS_PMIC_INT4, 1 << 6),
	DECLARE_IRQ(S2MPG15_IRQ_OCP_BBS_INT4, S2MPG15_IRQS_PMIC_INT4, 1 << 7),

	DECLARE_IRQ(S2MPG15_IRQ_PMETER_OVERF_INT5, S2MPG15_IRQS_METER_INT1, 1 << 0),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_CYCLE_DONE_INT5, S2MPG15_IRQS_METER_INT1, 1 << 1),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH0_INT5, S2MPG15_IRQS_METER_INT1, 1 << 2),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH1_INT5, S2MPG15_IRQS_METER_INT1, 1 << 3),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH2_INT5, S2MPG15_IRQS_METER_INT1, 1 << 4),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH3_INT5, S2MPG15_IRQS_METER_INT1, 1 << 5),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH4_INT5, S2MPG15_IRQS_METER_INT1, 1 << 6),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH5_INT5, S2MPG15_IRQS_METER_INT1, 1 << 7),

	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH6_INT6, S2MPG15_IRQS_METER_INT2, 1 << 0),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH7_INT6, S2MPG15_IRQS_METER_INT2, 1 << 1),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH8_INT6, S2MPG15_IRQS_METER_INT2, 1 << 2),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH9_INT6, S2MPG15_IRQS_METER_INT2, 1 << 3),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH10_INT6, S2MPG15_IRQS_METER_INT2, 1 << 4),
	DECLARE_IRQ(S2MPG15_IRQ_PWR_WARN_CH11_INT6, S2MPG15_IRQS_METER_INT2, 1 << 5),

	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_OT_CH1_INT7, S2MPG15_IRQS_METER_INT3, 1 << 0),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_OT_CH2_INT7, S2MPG15_IRQS_METER_INT3, 1 << 1),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_OT_CH3_INT7, S2MPG15_IRQS_METER_INT3, 1 << 2),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_OT_CH4_INT7, S2MPG15_IRQS_METER_INT3, 1 << 3),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_OT_CH5_INT7, S2MPG15_IRQS_METER_INT3, 1 << 4),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_OT_CH6_INT7, S2MPG15_IRQS_METER_INT3, 1 << 5),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_OT_CH7_INT7, S2MPG15_IRQS_METER_INT3, 1 << 6),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_OT_CH8_INT7, S2MPG15_IRQS_METER_INT3, 1 << 7),

	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_UT_CH1_INT8, S2MPG15_IRQS_METER_INT4, 1 << 0),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_UT_CH2_INT8, S2MPG15_IRQS_METER_INT4, 1 << 1),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_UT_CH3_INT8, S2MPG15_IRQS_METER_INT4, 1 << 2),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_UT_CH4_INT8, S2MPG15_IRQS_METER_INT4, 1 << 3),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_UT_CH5_INT8, S2MPG15_IRQS_METER_INT4, 1 << 4),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_UT_CH6_INT8, S2MPG15_IRQS_METER_INT4, 1 << 5),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_UT_CH7_INT8, S2MPG15_IRQS_METER_INT4, 1 << 6),
	DECLARE_IRQ(S2MPG15_IRQ_NTC_WARN_UT_CH8_INT8, S2MPG15_IRQS_METER_INT4, 1 << 7),

};

static int
s2mpg15_mask_ibi_region(struct s2mpg15_dev *s2mpg15, u8 reg, u8 region)
{
	int ret;
	u8 mask = (0x01 << region);

	ret = s2mpg15_update_reg(s2mpg15->i2c, reg, mask, mask);
	if (ret) {
		dev_err(s2mpg15->dev,
			"Failed to mask ibi region:0x%02x in reg:0x%02x", region, reg);
		return ret;
	}

	return 0;
}

static int
s2mpg15_unmask_ibi_region(struct s2mpg15_dev *s2mpg15, u8 reg, u8 region)
{
	int ret;
	u8 mask = (0x01 << region);

	ret = s2mpg15_update_reg(s2mpg15->i2c, reg, 0x00, mask);
	if (ret) {
		dev_err(s2mpg15->dev,
			"Failed to unmask ibi region:0x%02x in reg:0x%02x", region, reg);
		return ret;
	}

	return 0;
}

static int s2mpg15_update_ibi_regions(struct s2mpg15_dev *s2mpg15)
{
	/* Unmask PM region, if at least one PM interrupt is enabled */
	if ((s2mpg15->irq_masks_cur[S2MPG15_IRQS_PMIC_INT1] &
		s2mpg15->irq_masks_cur[S2MPG15_IRQS_PMIC_INT2] &
		s2mpg15->irq_masks_cur[S2MPG15_IRQS_PMIC_INT3] &
		s2mpg15->irq_masks_cur[S2MPG15_IRQS_PMIC_INT4]) != 0xff) {
		s2mpg15_unmask_ibi_region(s2mpg15, S2MPG15_COMMON_IBIM1,
					  S2MPG15_IBIM1_PM_REGION);
	} else {
		s2mpg15_mask_ibi_region(s2mpg15, S2MPG15_COMMON_IBIM1,
					S2MPG15_IBIM1_PM_REGION);
	}

	/* Unmask PMETER region, if at least one PMETER interrupt is enabled */
	if ((s2mpg15->irq_masks_cur[S2MPG15_IRQS_METER_INT1] &
		s2mpg15->irq_masks_cur[S2MPG15_IRQS_METER_INT2] &
		s2mpg15->irq_masks_cur[S2MPG15_IRQS_METER_INT3] &
		s2mpg15->irq_masks_cur[S2MPG15_IRQS_METER_INT4]) != 0xff) {
		s2mpg15_unmask_ibi_region(s2mpg15, S2MPG15_COMMON_IBIM1,
					  S2MPG15_IBIM1_PMETER_REGION);
	} else {
		s2mpg15_mask_ibi_region(s2mpg15, S2MPG15_COMMON_IBIM1,
					S2MPG15_IBIM1_PMETER_REGION);
	}

	return 0;
}

static void s2mpg15_irq_lock(struct irq_data *data)
{
	struct s2mpg15_dev *s2mpg15 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpg15->irqlock);
}

static void s2mpg15_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpg15_dev *s2mpg15 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPG15_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mpg15_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mpg15, i);

		if (mask_reg == S2MPG15_REG_INVALID || IS_ERR_OR_NULL(i2c))
			continue;
		s2mpg15->irq_masks_cache[i] = s2mpg15->irq_masks_cur[i];

		s2mpg15_write_reg(i2c, s2mpg15_mask_reg[i],
				  s2mpg15->irq_masks_cur[i]);
	}

	s2mpg15_update_ibi_regions(s2mpg15);

	mutex_unlock(&s2mpg15->irqlock);
}

static const inline struct s2mpg15_irq_data *
irq_to_s2mpg15_irq(struct s2mpg15_dev *s2mpg15, int irq)
{
	return &s2mpg15_irqs[irq - s2mpg15->irq_base];
}

static void s2mpg15_irq_mask(struct irq_data *data)
{
	struct s2mpg15_dev *s2mpg15 = irq_get_chip_data(data->irq);
	const struct s2mpg15_irq_data *irq_data =
		irq_to_s2mpg15_irq(s2mpg15, data->irq);

	if (irq_data->group >= S2MPG15_IRQ_GROUP_NR)
		return;

	s2mpg15->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mpg15_irq_unmask(struct irq_data *data)
{
	struct s2mpg15_dev *s2mpg15 = irq_get_chip_data(data->irq);
	const struct s2mpg15_irq_data *irq_data =
		irq_to_s2mpg15_irq(s2mpg15, data->irq);

	if (irq_data->group >= S2MPG15_IRQ_GROUP_NR)
		return;

	s2mpg15->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mpg15_irq_chip = {
	.name = S2MPG15_MFD_DEV_NAME,
	.irq_bus_lock = s2mpg15_irq_lock,
	.irq_bus_sync_unlock = s2mpg15_irq_sync_unlock,
	.irq_mask = s2mpg15_irq_mask,
	.irq_unmask = s2mpg15_irq_unmask,
};

/* BUCK 1~10, BUCK D, BUCK A, BUCK C */
static int s2mpg15_buck_ocp_cnt[S2MPG15_BUCK_MAX];

static void s2mpg15_buck_ocp_irq(struct s2mpg15_dev *s2mpg15, int buck)
{
	s2mpg15_buck_ocp_cnt[buck]++;

	dev_info(s2mpg15->dev, "%s: S2MPG15 BUCK[%d] OCP IRQ: %d\n",
		 __func__, buck + 1, s2mpg15_buck_ocp_cnt[buck]);
}

static const u8 s2mpg15_get_irq_mask[] = {
	/* OCP */
	[S2MPG15_IRQ_OCP_B1S_INT3] = S2MPG15_IRQ_OCP_B1S_MASK,
	[S2MPG15_IRQ_OCP_B2S_INT3] = S2MPG15_IRQ_OCP_B2S_MASK,
	[S2MPG15_IRQ_OCP_B3S_INT3] = S2MPG15_IRQ_OCP_B3S_MASK,
	[S2MPG15_IRQ_OCP_B4S_INT3] = S2MPG15_IRQ_OCP_B4S_MASK,
	[S2MPG15_IRQ_OCP_B5S_INT3] = S2MPG15_IRQ_OCP_B5S_MASK,
	[S2MPG15_IRQ_OCP_B6S_INT3] = S2MPG15_IRQ_OCP_B6S_MASK,
	[S2MPG15_IRQ_OCP_B7S_INT3] = S2MPG15_IRQ_OCP_B7S_MASK,
	[S2MPG15_IRQ_OCP_B8S_INT3] = S2MPG15_IRQ_OCP_B8S_MASK,
	[S2MPG15_IRQ_OCP_B9S_INT4] = S2MPG15_IRQ_OCP_B9S_MASK,
	[S2MPG15_IRQ_OCP_B10S_INT4] = S2MPG15_IRQ_OCP_B10S_MASK,
	[S2MPG15_IRQ_OCP_B11S_INT4] = S2MPG15_IRQ_OCP_B11S_MASK,
	[S2MPG15_IRQ_OCP_B12S_INT4] = S2MPG15_IRQ_OCP_B12S_MASK,
	[S2MPG15_IRQ_OCP_BDS_INT4] = S2MPG15_IRQ_OCP_BDS_MASK,
	[S2MPG15_IRQ_OCP_BAS_INT4] = S2MPG15_IRQ_OCP_BAS_MASK,
	[S2MPG15_IRQ_OCP_BCS_INT4] = S2MPG15_IRQ_OCP_BCS_MASK,
	[S2MPG15_IRQ_OCP_BBS_INT4] = S2MPG15_IRQ_OCP_BBS_MASK,
};

static void s2mpg15_call_interrupt(struct s2mpg15_dev *s2mpg15,
				   u8 pm_int1, u8 pm_int2, u8 pm_int3, u8 pm_int4)
{
	size_t i;
	u8 reg = 0;

	/* BUCK OCP interrupt */
	for (i = 0; i < S2MPG15_BUCK_MAX; i++) {
		reg = S2MPG15_IRQ_OCP_B1S_INT3 + i;

		if ((pm_int3 & s2mpg15_get_irq_mask[reg]) ||
		    (pm_int4 & s2mpg15_get_irq_mask[reg]))
			s2mpg15_buck_ocp_irq(s2mpg15, i);
	}
}

static int s2mpg15_notifier_handler(struct notifier_block *nb,
				    unsigned long action,
				    void *data)
{
	struct s2mpg15_dev *s2mpg15 = data;
	int i, ret;

	if (!s2mpg15)
		return NOTIFY_DONE;

	mutex_lock(&s2mpg15->irqlock);

	ret = s2mpg15_bulk_read(s2mpg15->pmic, S2MPG15_PM_INT1,
				S2MPG15_NUM_IRQ_PMIC_REGS,
				&irq_reg_sub[S2MPG15_IRQS_PMIC_INT1]);
	if (ret) {
		dev_err(s2mpg15->dev, "fail to read INT sources\n");
		mutex_unlock(&s2mpg15->irqlock);

		return NOTIFY_DONE;
	}

	ret = s2mpg15_bulk_read(s2mpg15->meter, S2MPG15_METER_INT1,
				S2MPG15_NUM_IRQ_METER_REGS,
				&irq_reg_sub[S2MPG15_IRQS_METER_INT1]);
	if (ret) {
		dev_err(s2mpg15->dev, "fail to read INT sources\n");
		mutex_unlock(&s2mpg15->irqlock);

		return NOTIFY_DONE;
	}


	s2mpg15_call_interrupt(s2mpg15, irq_reg_sub[S2MPG15_IRQS_PMIC_INT1],
			       irq_reg_sub[S2MPG15_IRQS_PMIC_INT2],
			       irq_reg_sub[S2MPG15_IRQS_PMIC_INT3],
			       irq_reg_sub[S2MPG15_IRQS_PMIC_INT4]);

	for (i = 0; i < S2MPG15_IRQ_GROUP_NR; i++)
		irq_reg_sub[i] &= ~s2mpg15->irq_masks_cur[i];

	for (i = 0; i < S2MPG15_IRQ_NR; i++) {
		if (irq_reg_sub[s2mpg15_irqs[i].group] & s2mpg15_irqs[i].mask) {
			handle_nested_irq(s2mpg15->irq_base + i);
			log_threaded_irq_wakeup_reason(s2mpg15->irq_base + i,
						       s2mpg15->irq);
		}
	}

	mutex_unlock(&s2mpg15->irqlock);

	return NOTIFY_DONE;
}

static BLOCKING_NOTIFIER_HEAD(s2mpg15_notifier);

static int s2mpg15_register_notifier(struct notifier_block *nb,
				     struct s2mpg15_dev *s2mpg15)
{
	int ret;

	ret = blocking_notifier_chain_register(&s2mpg15_notifier, nb);
	if (ret < 0)
		dev_err(s2mpg15->dev, "fail to register notifier\n");

	return ret;
}

void s2mpg15_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mpg15_notifier, 0, s2mpg15_global);
}
EXPORT_SYMBOL(s2mpg15_call_notifier);

static int s2mpg15_set_interrupt(struct s2mpg15_dev *s2mpg15)
{
	int i;
	int ret = 0;

	/* mask individual interrupt sources */
	for (i = 0; i < S2MPG15_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mpg15_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mpg15, i);

		if (mask_reg == S2MPG15_REG_INVALID || IS_ERR_OR_NULL(i2c))
			return -ENODEV;

		ret = s2mpg15_write_reg(i2c, s2mpg15_mask_reg[i], 0xff);
		if (ret)
			return -ENODEV;

		s2mpg15->irq_masks_cur[i] = 0xff;
		s2mpg15->irq_masks_cache[i] = 0xff;
	}

	/* Mask all inband interrupts during init*/
	for (i = 0; i < S2MPG15_IBIM1_REGION_MAX; i++) {
		ret = s2mpg15_mask_ibi_region(s2mpg15, S2MPG15_COMMON_IBIM1, i);
		if (ret)
			return -ENODEV;
	}

	for (i = 0; i < S2MPG15_IBIM2_REGION_MAX; i++) {
		ret = s2mpg15_mask_ibi_region(s2mpg15, S2MPG15_COMMON_IBIM2, i);
		if (ret)
			return -ENODEV;
	}

	return 0;
}

static void s2mpg15_set_notifier(struct s2mpg15_dev *s2mpg15)
{
	sub_pmic_notifier.notifier_call = s2mpg15_notifier_handler;
	s2mpg15_register_notifier(&sub_pmic_notifier, s2mpg15);
}

int s2mpg15_notifier_init(struct s2mpg15_dev *s2mpg15)
{
	int i;
	int ret, cur_irq;

	if (!s2mpg15->irq_base) {
		dev_err(s2mpg15->dev, "No interrupt base specified.\n");
		return -ENODEV;
	}

	s2mpg15_global = s2mpg15;
	mutex_init(&s2mpg15->irqlock);

	/* Register notifier */
	s2mpg15_set_notifier(s2mpg15);

	/* Set interrupt */
	ret = s2mpg15_set_interrupt(s2mpg15);
	if (ret) {
		dev_err(s2mpg15->dev, "s2mpg15_set_interrupt fail\n");
		return ret;
	}

	/* Register with genirq */
	for (i = 0; i < S2MPG15_IRQ_NR; i++) {
		cur_irq = i + s2mpg15->irq_base;
		irq_set_chip_data(cur_irq, s2mpg15);
		irq_set_chip_and_handler(cur_irq, &s2mpg15_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpg15_notifier_init);
