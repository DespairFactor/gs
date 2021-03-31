/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Samsung Electronics.
 *
 */

#ifndef __DIT_2_1_0_H__
#define __DIT_2_1_0_H__

#define DIT_REG_CLK_GT_OFF			0x0004 /* 20 bit */
#define DIT_REG_DMA_INIT_DATA			0x0008 /* 28 bit */

/* 0:16beat, 1:8beat, 2:4beat, 3:2beat, 4:1beat */
#define DIT_REG_TX_DESC_CTRL_SRC		0x000C /* 3 bit */
#define DIT_REG_TX_DESC_CTRL_DST		0x0010 /* 3 bit */
#define DIT_REG_TX_HEAD_CTRL			0x0014 /* 3 bit */
#define DIT_REG_TX_MOD_HD_CTRL			0x0018 /* 3 bit */
#define DIT_REG_TX_PKT_CTRL			0x001C /* 3 bit */
#define DIT_REG_TX_CHKSUM_CTRL			0x0020 /* 3 bit */

#define DIT_REG_RX_DESC_CTRL_SRC		0x0024 /* 3 bit */
#define DIT_REG_RX_DESC_CTRL_DST		0x0028 /* 3 bit */
#define DIT_REG_RX_HEAD_CTRL			0x002C /* 3 bit */
#define DIT_REG_RX_MOD_HD_CTRL			0x0030 /* 3 bit */
#define DIT_REG_RX_PKT_CTRL			0x0034 /* 3 bit */
#define DIT_REG_RX_CHKSUM_CTRL			0x0038 /* 3 bit */

#define DIT_REG_DMA_CHKSUM_OFF			0x003C /* 2 bit */

/* start address for Tx desc */
#define DIT_REG_TX_RING_START_ADDR_0_SRC	0x0044
#define DIT_REG_TX_RING_START_ADDR_1_SRC	0x0048
#define DIT_REG_TX_RING_START_ADDR_0_DST0	0x004C
#define DIT_REG_TX_RING_START_ADDR_1_DST0	0x0050
#define DIT_REG_TX_RING_START_ADDR_0_DST1	0x0054
#define DIT_REG_TX_RING_START_ADDR_1_DST1	0x0058
#define DIT_REG_TX_RING_START_ADDR_0_DST2	0x005C
#define DIT_REG_TX_RING_START_ADDR_1_DST2	0x0060

/* start address for Rx desc */
#define DIT_REG_RX_RING_START_ADDR_0_SRC	0x0064
#define DIT_REG_RX_RING_START_ADDR_1_SRC	0x0068
#define DIT_REG_RX_RING_START_ADDR_0_DST0	0x006C
#define DIT_REG_RX_RING_START_ADDR_1_DST0	0x0070
#define DIT_REG_RX_RING_START_ADDR_0_DST1	0x0074
#define DIT_REG_RX_RING_START_ADDR_1_DST1	0x0078
#define DIT_REG_RX_RING_START_ADDR_0_DST2	0x007C
#define DIT_REG_RX_RING_START_ADDR_1_DST2	0x0080

#define DIT_REG_INT_ENABLE			0x0084
#define DIT_REG_INT_MASK			0x0088
#define DIT_REG_INT_PENDING			0x008C
#define DIT_REG_STATUS				0x0090

/* address for Tx desc */
#define DIT_REG_NAT_TX_DESC_ADDR_0_SRC		0x4000	/* 32 bit */
#define DIT_REG_NAT_TX_DESC_ADDR_1_SRC		0x4004	/* 4 bit */
#define DIT_REG_NAT_TX_DESC_ADDR_EN_SRC		0x4008	/* 1 bit */
#define DIT_REG_NAT_TX_DESC_ADDR_0_DST0		0x4018
#define DIT_REG_NAT_TX_DESC_ADDR_1_DST0		0x401C
#define DIT_REG_NAT_TX_DESC_ADDR_0_DST1		0x4020
#define DIT_REG_NAT_TX_DESC_ADDR_1_DST1		0x4024
#define DIT_REG_NAT_TX_DESC_ADDR_0_DST2		0x4028
#define DIT_REG_NAT_TX_DESC_ADDR_1_DST2		0x402C

/* address for Rx desc */
#define DIT_REG_NAT_RX_DESC_ADDR_0_SRC		0x4030	/* 32 bit */
#define DIT_REG_NAT_RX_DESC_ADDR_1_SRC		0x4034	/* 4 bit */
#define DIT_REG_NAT_RX_DESC_ADDR_EN_SRC		0x4038	/* 1 bit */
#define DIT_REG_NAT_RX_DESC_ADDR_0_DST0		0x4048
#define DIT_REG_NAT_RX_DESC_ADDR_1_DST0		0x404C
#define DIT_REG_NAT_RX_DESC_ADDR_0_DST1		0x4050
#define DIT_REG_NAT_RX_DESC_ADDR_1_DST1		0x4054
#define DIT_REG_NAT_RX_DESC_ADDR_0_DST2		0x4058
#define DIT_REG_NAT_RX_DESC_ADDR_1_DST2		0x405C

struct dit_src_desc {
	u64	src_addr:36,
		_reserved_0:12,
		/* the below 16 bits are "private info" on the document */
		ch_id:8,		/* max ch value for rmnet is 17 */
		pre_csum:1,		/* checksum successful from pktproc */
		udp_csum_zero:1,	/* reset udp checksum 0 after NAT */
		_reserved_2:6;
	u64	length:16,
		_reserved_1:32,
		control:8,
		status:8;
} __packed;

struct dit_dst_desc {
	u64	dst_addr:36,
		packet_info:12,
		/* the below 16 bits are "private info" on the document */
		ch_id:8,
		pre_csum:1,
		udp_csum_zero:1,
		_reserved_2:6;
	u64	length:16,
		org_port:16,
		trans_port:16,
		control:8,		/* misspelled as "reserved" on the document */
		status:8;
} __packed;

/* DIT_INT_PENDING */
enum dit_int_pending_bits {
	TX_DST0_INT_PENDING_BIT = 0,
	TX_DST1_INT_PENDING_BIT,
	TX_DST2_INT_PENDING_BIT,
	RX_DST0_INT_PENDING_BIT = 3,
	RX_DST1_INT_PENDING_BIT,
	RX_DST2_INT_PENDING_BIT,
	ERR_INT_PENDING_BIT = 14,
};

#define DIT_TX_INT_PENDING_MASK \
	(BIT(TX_DST0_INT_PENDING_BIT) | BIT(TX_DST1_INT_PENDING_BIT) | \
		BIT(TX_DST2_INT_PENDING_BIT) | BIT(ERR_INT_PENDING_BIT))

#define DIT_RX_INT_PENDING_MASK \
	(BIT(RX_DST0_INT_PENDING_BIT) | BIT(RX_DST1_INT_PENDING_BIT) | \
		BIT(RX_DST2_INT_PENDING_BIT) | BIT(ERR_INT_PENDING_BIT))

enum dit_int_enable_bits {
	TX_DST0_INT_ENABLE_BIT = 0,
	TX_DST1_INT_ENABLE_BIT,
	TX_DST2_INT_ENABLE_BIT,
	RX_DST0_INT_ENABLE_BIT = 3,
	RX_DST1_INT_ENABLE_BIT,
	RX_DST2_INT_ENABLE_BIT,
	ERR_INT_ENABLE_BIT = 14,
};

#define DIT_INT_ENABLE_MASK \
	(BIT(TX_DST0_INT_ENABLE_BIT) | BIT(TX_DST1_INT_ENABLE_BIT) | \
		BIT(TX_DST2_INT_ENABLE_BIT) | \
		BIT(RX_DST0_INT_ENABLE_BIT) | BIT(RX_DST1_INT_ENABLE_BIT) | \
		BIT(RX_DST2_INT_ENABLE_BIT) | \
		BIT(ERR_INT_ENABLE_BIT))

#define DIT_INT_MASK_MASK	DIT_INT_ENABLE_MASK

#endif /* __DIT_2_1_0_H__ */
