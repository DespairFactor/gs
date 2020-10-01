/* SPDX-License-Identifier: GPL-2.0-only */
/* S2MPU offsets */
#define S2MPU_CTRL0_OFFSET				0x0
#define S2MPU_CTRL1_OFFSET				0x4
#define S2MPU_CFG_OFFSET				0x10
#define S2MPU_CFG_MPTW_USER_LOW_OFFSET			0x14

#define S2MPU_CFG_MPTW_USER_HIGH_OFFSET			0x18

#define S2MPU_INTERRUPT_ENABLE_PER_VID_SET_OFFSET	0x20
#define S2MPU_MPC_CTRL_OFFSET				0x40
#define S2MPU_PAGE_AWARE_DECODING_0_OFFSET		0x44
#define S2MPU_PAGE_AWARE_DECODING_1_OFFSET		0x48
#define S2MPU_PM_CFG_OFFSET				0x70
#define S2MPU_ALL_INVALIDATION_OFFSET			0x1000
#define S2MPU_RANGE_INVALIDATION_OFFSET			0x1020
#define S2MPU_RANGE_INVALIDATION_START_PPN_OFFSET	0x1024
#define S2MPU_RANGE_INVALIDATION_END_PPN_OFFSET		0x1028
#define S2MPU_L1ENTRY_L2TABLE_ADDR_OFFSET(n, m)		(0x4000 + ((n) * 0x200) + ((m) * 0x8))
#define S2MPU_L1ENTRY_ATTR_OFFSET(n, m)			(0x4004 + ((n) * 0x200) + ((m) * 0x8))

/* register values */
#define S2MPU_CTRL0(base)				((base) + S2MPU_CTRL0_OFFSET)
#define S2MPU_CTRL0_ENABLE_SHIFT			0
#define S2MPU_CTRL0_INTERRUPT_ENABLE_SHIFT		1
#define S2MPU_CTRL0_FAULT_RESP_TYPE_SHIFT		2

#define S2MPU_CTRL1(base)				((base) + S2MPU_CTRL1_OFFSET)
#define S2MPU_CTRL1_DISABLE_CHK_S1L1PTW_SHIFT		0
#define S2MPU_CTRL1_DISABLE_CHK_S1L2PTW_SHIFT		1
#define S2MPU_CTRL1_ENABLE_PAGE_SIZE_AWARENESS_SHIFT	2
#define S2MPU_CTRL1_DISABLE_CHK_USER_MATCHED_REQ_SHIFT	3

#define S2MPU_PM_CFG(base)				((base) + S2MPU_PM_CFG_OFFSET)
#define S2MPU_PM_CFG_CNT_EN_SHIFT			0

#define S2MPU_L1ENTRY_L2TABLE_ADDR(base, n, m) \
		((base) + (S2MPU_L1ENTRY_L2TABLE_ADDR_OFFSET(n, m)))

#define S2MPU_L1ENTRY_ATTR(base, n, m) \
		((base) + (S2MPU_L1ENTRY_ATTR_OFFSET(n, m)))
#define S2MPU_L1ENTRY_ATTR_L2TABLE_EN_SHIFT 0
#define S2MPU_L1ENTRY_ATTR_L2TABLE_EN_MASK \
		(0x1 << S2MPU_L1ENTRY_ATTR_L2TABLE_EN_SHIFT)
#define S2MPU_L1ENTRY_ATTR_L2TABLE_EN_VALUE(reg) \
		(((reg) & S2MPU_L1ENTRY_ATTR_L2TABLE_EN_MASK) >> \
		 S2MPU_L1ENTRY_ATTR_L2TABLE_EN_SHIFT)
#define S2MPU_L1ENTRY_ATTR_RD_ACCESS_SHIFT 1
#define S2MPU_L1ENTRY_ATTR_RD_ACCESS_MASK \
		(0x1 << S2MPU_L1ENTRY_ATTR_RD_ACCESS_SHIFT)
#define S2MPU_L1ENTRY_ATTR_RD_ACCESS_VALUE(reg) \
		(((reg) & S2MPU_L1ENTRY_ATTR_RD_ACCESS_MASK) >> \
		 S2MPU_L1ENTRY_ATTR_RD_ACCESS_SHIFT)
#define S2MPU_L1ENTRY_ATTR_WR_ACCESS_SHIFT 2
#define S2MPU_L1ENTRY_ATTR_WR_ACCESS_MASK \
		(0x1 << S2MPU_L1ENTRY_ATTR_WR_ACCESS_SHIFT)
#define S2MPU_L1ENTRY_ATTR_WR_ACCESS_VALUE(reg) \
		(((reg) & S2MPU_L1ENTRY_ATTR_WR_ACCESS_MASK) >> \
		 S2MPU_L1ENTRY_ATTR_WR_ACCESS_SHIFT)
#define S2MPU_L1ENTRY_ATTR_L2TABLE_GRANULE_SHIFT 4
#define S2MPU_L1ENTRY_ATTR_L2TABLE_GRANULE_MASK \
		(0x3 << S2MPU_L1ENTRY_ATTR_L2TABLE_GRANULE_SHIFT)
#define S2MPU_L1ENTRY_ATTR_L2TABLE_GRANULE_VALUE(reg) \
		(((reg) & S2MPU_L1ENTRY_ATTR_L2TABLE_GRANULE_MASK) >> \
		 S2MPU_L1ENTRY_ATTR_L2TABLE_GRANULE_SHIFT)

#define S2MPU_ALL_INVALIDATION(base)			((base) + S2MPU_ALL_INVALIDATION_OFFSET)
#define S2MPU_ALL_INVALIDATION_INVALIDATE_SHIFT		0
#define S2MPU_ALL_INVALIDATION_VID_SPECIFIC_SHIFT	1
#define S2MPU_ALL_INVALIDATION_VID_SHIFT		16

#define S2MPU_RANGE_INVALIDATION(base)			((base) + S2MPU_RANGE_INVALIDATION_OFFSET)
#define S2MPU_RANGE_INVALIDATION_INVALIDATE_SHIFT	0
#define S2MPU_RANGE_INVALIDATION_VID_SPECIFIC_SHIFT	1
#define S2MPU_RANGE_INVALIDATION_VID_SHIFT		16

#define S2MPU_RANGE_INVALIDATION_START_PPN(base) ((base) + \
				S2MPU_RANGE_INVALIDATION_START_PPN_OFFSET)
#define S2MPU_RANGE_INVALIDATION_END_PPN(base) ((base) + \
				S2MPU_RANGE_INVALIDATION_END_PPN_OFFSET)

#define S2MPU_CFG(base)					((base) + S2MPU_CFG_OFFSET)
#define S2MPU_CFG_MPTW_CACHE_OVERRIDE_SHIFT		0
#define S2MPU_CFG_MPTW_CACHE_VALUE_SHIFT		4
#define S2MPU_CFG_MPTW_QOS_OVERRIDE_SHIFT		8
#define S2MPU_CFG_MPTW_SHAREABLE_SHIFT			16

#define S2MPU_CFG_MPTW_USER_LOW(base)			((base) + S2MPU_CFG_MPTW_USER_LOW_OFFSET)
#define S2MPU_CFG_MPTW_USER_LOW_PBHA_SHIFT		6
#define S2MPU_CFG_MPTW_USER_LOW_PID_SHIFT		10
#define S2MPU_CFG_MPTW_USER_LOW_VC_SHIFT		16

#define S2MPU_CFG_MPTW_USER_HIGH(base)			((base) + S2MPU_CFG_MPTW_USER_HIGH_OFFSET)
#define S2MPU_CFG_MPTW_USER_HIGH_EXT_DOMAIN_SHIFT		6

#define S2MPU_INTERRUPT_ENABLE_PER_VID_SET(base) ((base) + \
						  S2MPU_INTERRUPT_ENABLE_PER_VID_SET_OFFSET)

#define S2MPU_MPC_CTRL(base)			((base) + S2MPU_MPC_CTRL_OFFSET)
#define S2MPU_MPC_CTRL_RD_CH_TKN_SHIFT		0
#define S2MPU_MPC_CTRL_WR_CH_TKN_SHIFT		16

#define S2MPU_PAGE_AWARE_DECODING_0(base)	((base) + S2MPU_PAGE_AWARE_DECODING_0_OFFSET)
#define S2MPU_PAGE_AWARE_DECODING_1(base)	((base) + S2MPU_PAGE_AWARE_DECODING_1_OFFSET)
