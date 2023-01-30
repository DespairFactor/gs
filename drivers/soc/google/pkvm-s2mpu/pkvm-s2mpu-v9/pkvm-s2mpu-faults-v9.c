// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 - Google LLC
 */

#include "kvm_s2mpu.h"
#include <soc/google/pkvm-s2mpu.h>
#include <linux/io-64-nonatomic-hi-lo.h>

#define S2MPU_NR_WAYS	4

struct s2mpu_mptc_entry {
	bool valid;
	u32 vid;
	u32 ppn;
	u32 others;
	u32 data;
};

static const char *str_fault_direction(u32 fault_info)
{
	return (fault_info & FAULT_INFO_RW_BIT) ? "write" : "read";
}

static const char *str_fault_type(u32 fault_info)
{
	switch (FIELD_GET(FAULT_INFO_TYPE_MASK, fault_info)) {
	case FAULT_INFO_TYPE_MPTW:
		return "MPTW fault";
	case FAULT_INFO_TYPE_AP:
		return "access permission fault";
	case FAULT_INFO_TYPE_CONTEXT:
		return "context fault";
	default:
		return "unknown fault";
	}
}

static const char *str_l1entry_gran(u32 l1attr)
{
	if (!(l1attr & L1ENTRY_ATTR_L2TABLE_EN))
		return "1G";

	switch (FIELD_GET(L1ENTRY_ATTR_GRAN_MASK, l1attr)) {
	case L1ENTRY_ATTR_GRAN_4K:
		return "4K";
	case L1ENTRY_ATTR_GRAN_64K:
		return "64K";
	default:
		return "invalid";
	}
}

static const char *str_l1entry_prot(u32 l1attr)
{
	if (l1attr & L1ENTRY_ATTR_L2TABLE_EN)
		return "??";

	switch (FIELD_GET(L1ENTRY_ATTR_PROT_MASK, l1attr)) {
	case MPT_PROT_NONE:
		return "0";
	case MPT_PROT_R:
		return "R";
	case MPT_PROT_W:
		return "W";
	case MPT_PROT_RW:
		return "RW";
	default:
		return "invalid";
	}
}

static struct s2mpu_mptc_entry read_mptc(void __iomem *base, u32 set, u32 way)
{
	struct s2mpu_mptc_entry entry;
	u32 tag_ppn;

	writel_relaxed(V9_READ_MPTC(set, way), base + REG_NS_V9_READ_MPTC);

	tag_ppn = readl_relaxed(base + REG_NS_V9_READ_MPTC_TAG_PPN),
	entry.others = readl_relaxed(base + REG_NS_V9_READ_MPTC_TAG_OTHERS),
	entry.data = readl_relaxed(base + REG_NS_V9_READ_MPTC_DATA),

	entry.ppn = FIELD_GET(V9_READ_MPTC_TAG_PPN_TPN_PPN_MASK, tag_ppn);
	entry.valid = FIELD_GET(V9_READ_MPTC_TAG_PPN_VALID_MASK, tag_ppn);
	entry.vid = FIELD_GET(V9_READ_MPTC_TAG_OTHERS_VID_MASK, entry.others);
	return entry;
}

irqreturn_t s2mpu_fault_handler(struct s2mpu_data *data)
{
	struct device *dev = data->dev;
	unsigned int vid, gb;
	u32 vid_bmap, fmpt, smpt, nr_sets, set, way, invalid;
	/* Fault variables. */
	u32 fault_info, fault_info2, axi_id, pmmu_id, stream_id;
	phys_addr_t fault_pa;
	struct s2mpu_mptc_entry mptc;
	irqreturn_t ret = IRQ_NONE;

	while ((vid_bmap = readl_relaxed(data->base + REG_NS_FAULT_STATUS))) {
		WARN_ON_ONCE(vid_bmap & (~ALL_VIDS_BITMAP));
		vid = __ffs(vid_bmap);

		fault_pa = hi_lo_readq_relaxed(data->base + REG_NS_FAULT_PA_HIGH_LOW(vid));
		fault_info = readl_relaxed(data->base + REG_NS_FAULT_INFO(vid));
		axi_id = readl_relaxed(data->base + REG_NS_FAULT_INFO1(vid));
		fault_info2 = readl_relaxed(data->base + REG_NS_FAULT_INFO2(vid));
		WARN_ON(FIELD_GET(FAULT_INFO_VID_MASK, fault_info) != vid);
		pmmu_id = FIELD_GET(FAULT2_PMMU_ID_MASK, fault_info2);
		stream_id = FIELD_GET(FAULT2_STREAM_ID_MASK, fault_info2);

		dev_err(dev, "============== S2MPU FAULT DETECTED ==============\n");
		dev_err(dev, "  PA=%pap, FAULT_INFO=0x%08x\n",
			&fault_pa, fault_info);
		dev_err(dev, "  DIRECTION: %s, TYPE: %s\n",
			str_fault_direction(fault_info),
			str_fault_type(fault_info));
		dev_err(dev, "  VID=%u, REQ_LENGTH=%lu, REQ_AXI_ID=%u\n",
			vid, FIELD_GET(FAULT_INFO_LEN_MASK, fault_info), axi_id);
		dev_err(dev, "  PMMU_ID=%u, STREAM_ID=%u\n", pmmu_id, stream_id);

		for_each_gb(gb) {
			fmpt = readl_relaxed(data->base + REG_NS_L1ENTRY_ATTR(vid, gb));
			smpt = readl_relaxed(data->base + REG_NS_L1ENTRY_L2TABLE_ADDR(vid, gb));
			dev_err(dev, "  %uG: FMPT=%#x (%s, %s), SMPT=%#x\n",
				gb, fmpt, str_l1entry_gran(fmpt),
				str_l1entry_prot(fmpt), smpt);
		}

		dev_err(dev, "==================================================\n");

		writel_relaxed(BIT(vid), data->base + REG_NS_INTERRUPT_CLEAR);
		ret = IRQ_HANDLED;
	}

	dev_err(dev, "================== MPTC ENTRIES ==================\n");
	nr_sets = FIELD_GET(V9_READ_MPTC_INFO_NUM_MPTC_SET,
			    readl_relaxed(data->base + REG_NS_V9_MPTC_INFO));
	for (invalid = 0, set = 0; set < nr_sets; set++) {
		for (way = 0; way < S2MPU_NR_WAYS; way++) {
			mptc = read_mptc(data->base, set, way);
			if (!mptc.valid) {
				invalid++;
				continue;
			}

			dev_err(dev,
				"  MPTC[set=%u, way=%u]={VID=%u, PPN=%#x, OTHERS=%#x, DATA=%#x}\n",
				set, way, mptc.vid, mptc.ppn, mptc.others, mptc.data);
		}
	}
	dev_err(dev, "  invalid entries: %u\n", invalid);
	dev_err(dev, "==================================================\n");

	return ret;
}