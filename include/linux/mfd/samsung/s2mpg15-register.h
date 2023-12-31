/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * s2mpg15-register.h
 *
 * Copyright (C) 2015 Samsung Electrnoics
 *
 * header file including register information of s2mpg15
 */

#ifndef __LINUX_MFD_S2MPG15_REGISTER_H
#define __LINUX_MFD_S2MPG15_REGISTER_H

#include <linux/i2c.h>
#include "s2mpg1415-register.h"

#define S2MPG15_REG_INVALID	(0xFF)

enum S2MPG15_pmic_rev {
	S2MPG15_EVT0,
};

#define I2C_ADDR_TOP 0x00
#define I2C_ADDR_PMIC 0x01
#define I2C_ADDR_RTC 0x02
#define I2C_ADDR_METER 0x0A
#define I2C_ADDR_WLWP 0x0B
#define I2C_ADDR_GPIO 0x0C
#define I2C_ADDR_MT_TRIM 0x0D
#define I2C_ADDR_PM_TRIM2 0x0E
#define I2C_ADDR_PM_TRIM1 0x0F

/* Common(0x0) Registers */
enum S2MPG15_COMMON_REG {
	S2MPG15_COMMON_VGPIO0 = 0x0,
	S2MPG15_COMMON_VGPIO1 = 0x1,
	S2MPG15_COMMON_VGPIO2 = 0x2,
	S2MPG15_COMMON_VGPIO3 = 0x3,
	S2MPG15_COMMON_I3C_DAA = 0x4,
	S2MPG15_COMMON_IBI0 = 0x5,
	S2MPG15_COMMON_IBI1 = 0x6,
	S2MPG15_COMMON_IBI2 = 0x7,
	S2MPG15_COMMON_IBI3 = 0x8,
	S2MPG15_COMMON_CHIPID = 0xB,
	S2MPG15_COMMON_I3C_CFG1 = 0xC,
	S2MPG15_COMMON_I3C_STA = 0xE,
	S2MPG15_COMMON_IBIM1 = 0xF,
	S2MPG15_COMMON_IBIM2 = 0x10,
};

/* PM(0x1) Registers */
enum S2MPG15_PM_REG {
	S2MPG15_PM_INT1 = 0x0,
	S2MPG15_PM_INT2 = 0x1,
	S2MPG15_PM_INT3 = 0x2,
	S2MPG15_PM_INT4 = 0x3,
	S2MPG15_PM_INT1M = 0x4,
	S2MPG15_PM_INT2M = 0x5,
	S2MPG15_PM_INT3M = 0x6,
	S2MPG15_PM_INT4M = 0x7,
	S2MPG15_PM_OFFSRC1 = 0x9,
	S2MPG15_PM_OFFSRC2 = 0xA,
	S2MPG15_PM_CTRL1 = 0xB,
	S2MPG15_PM_CTRL2 = 0xC,
	S2MPG15_PM_CTRL3 = 0xD,
	S2MPG15_PM_B1S_CTRL = 0xE,
	S2MPG15_PM_B1S_OUT1 = 0xF,
	S2MPG15_PM_B2S_CTRL = 0x10,
	S2MPG15_PM_B2S_OUT1 = 0x11,
	S2MPG15_PM_B3S_CTRL = 0x12,
	S2MPG15_PM_B3S_OUT1 = 0x13,
	S2MPG15_PM_B4S_CTRL = 0x14,
	S2MPG15_PM_B4S_OUT1 = 0x15,
	S2MPG15_PM_B5S_CTRL = 0x16,
	S2MPG15_PM_B5S_OUT1 = 0x17,
	S2MPG15_PM_B6S_CTRL = 0x18,
	S2MPG15_PM_B6S_OUT1 = 0x19,
	S2MPG15_PM_B7S_CTRL = 0x1A,
	S2MPG15_PM_B7S_OUT1 = 0x1B,
	S2MPG15_PM_B8S_CTRL = 0x1C,
	S2MPG15_PM_B8S_OUT1 = 0x1D,
	S2MPG15_PM_B9S_CTRL = 0x1E,
	S2MPG15_PM_B9S_OUT0 = 0x1F,
	S2MPG15_PM_B9S_OUT3 = 0x20,
	S2MPG15_PM_B10S_CTRL = 0x21,
	S2MPG15_PM_B10S_OUT1 = 0x22,
	S2MPG15_PM_B11S_CTRL = 0x23,
	S2MPG15_PM_B11S_OUT1 = 0x24,
	S2MPG15_PM_B12S_CTRL = 0x25,
	S2MPG15_PM_B12S_OUT1 = 0x26,
	S2MPG15_PM_BUCKD_CTRL = 0x27,
	S2MPG15_PM_BUCKD_OUT = 0x28,
	S2MPG15_PM_BUCKA_CTRL = 0x29,
	S2MPG15_PM_BUCKA_OUT = 0x2A,
	S2MPG15_PM_BUCKC_CTRL = 0x2B,
	S2MPG15_PM_BUCKC_OUT = 0x2C,
	S2MPG15_PM_BB_CTRL = 0x2D,
	S2MPG15_PM_BB_OUT = 0x2E,
	S2MPG15_PM_L1S_CTRL = 0x2F,
	S2MPG15_PM_L2S_CTRL = 0x30,
	S2MPG15_PM_L3S_CTRL = 0x31,
	S2MPG15_PM_L4S_CTRL = 0x32,
	S2MPG15_PM_L5S_CTRL = 0x33,
	S2MPG15_PM_L6S_CTRL = 0x34,
	S2MPG15_PM_L7S_CTRL = 0x35,
	S2MPG15_PM_L8S_CTRL = 0x36,
	S2MPG15_PM_L9S_CTRL = 0x37,
	S2MPG15_PM_L10S_CTRL = 0x38,
	S2MPG15_PM_L11S_CTRL = 0x39,
	S2MPG15_PM_L12S_CTRL = 0x3A,
	S2MPG15_PM_L13S_CTRL = 0x3B,
	S2MPG15_PM_L14S_CTRL = 0x3C,
	S2MPG15_PM_L15S_CTRL = 0x3D,
	S2MPG15_PM_L16S_CTRL = 0x3E,
	S2MPG15_PM_L17S_CTRL = 0x3F,
	S2MPG15_PM_L18S_CTRL = 0x40,
	S2MPG15_PM_L19S_CTRL = 0x41,
	S2MPG15_PM_L20S_CTRL = 0x42,
	S2MPG15_PM_L21S_CTRL = 0x43,
	S2MPG15_PM_L22S_CTRL = 0x44,
	S2MPG15_PM_L23S_CTRL = 0x45,
	S2MPG15_PM_L24S_CTRL = 0x46,
	S2MPG15_PM_L25S_CTRL = 0x47,
	S2MPG15_PM_L26S_CTRL = 0x48,
	S2MPG15_PM_L27S_CTRL = 0x49,
	S2MPG15_PM_L28S_CTRL = 0x4A,
	S2MPG15_PM_L29S_CTRL = 0x4B,
	S2MPG15_PM_LDO_CTRL1 = 0x4C,
	S2MPG15_PM_LDO_CTRL2 = 0x4D,
	S2MPG15_PM_LDO_DSCH1 = 0x4E,
	S2MPG15_PM_LDO_DSCH2 = 0x4F,
	S2MPG15_PM_LDO_DSCH3 = 0x50,
	S2MPG15_PM_LDO_DSCH4 = 0x51,
	S2MPG15_PM_B11S_HLIMIT = 0x52,
	S2MPG15_PM_B11S_LLIMIT = 0x53,
	S2MPG15_PM_DVS_RAMP1 = 0x54,
	S2MPG15_PM_DVS_RAMP2 = 0x55,
	S2MPG15_PM_DVS_RAMP3 = 0x56,
	S2MPG15_PM_DVS_RAMP4 = 0x57,
	S2MPG15_PM_DVS_RAMP5 = 0x58,
	S2MPG15_PM_DVS_RAMP6 = 0x59,
	S2MPG15_PM_DVS_RAMP7 = 0x5A,
	S2MPG15_PM_DVS_RAMP8 = 0x5B,
	S2MPG15_PM_DVS_RAMP9 = 0x5C,
	S2MPG15_PM_DVS_SYNC_CTRL1 = 0x5D,
	S2MPG15_PM_DVS_SYNC_CTRL2 = 0x5E,
	S2MPG15_PM_DVS_SYNC_CTRL3 = 0x5F,
	S2MPG15_PM_DVS_OPTION = 0x60,
	S2MPG15_PM_OFF_CTRL1 = 0x61,
	S2MPG15_PM_OFF_CTRL2 = 0x62,
	S2MPG15_PM_OFF_CTRL3 = 0x63,
	S2MPG15_PM_OFF_CTRL4 = 0x64,
	S2MPG15_PM_OFF_CTRL5 = 0x65,
	S2MPG15_PM_OFF_CTRL6 = 0x66,
	/* ONSEQ1 ~ ONSEQ42 (0x67 ~ 0x90) */
	/* OFFSEQ1 ~ OFFSEQ21 (0x91 ~ 0xA5) */
	S2MPG15_PM_PCTRLSEL1 = 0xA6,
	S2MPG15_PM_PCTRLSEL2 = 0xA7,
	S2MPG15_PM_PCTRLSEL3 = 0xA8,
	S2MPG15_PM_PCTRLSEL4 = 0xA9,
	S2MPG15_PM_PCTRLSEL5 = 0xAA,
	S2MPG15_PM_PCTRLSEL6 = 0xAB,
	S2MPG15_PM_PCTRLSEL7 = 0xAC,
	S2MPG15_PM_PCTRLSEL8 = 0xAD,
	S2MPG15_PM_PCTRLSEL9 = 0xAE,
	S2MPG15_PM_PCTRLSEL10 = 0xAF,
	S2MPG15_PM_DCTRLSEL1 = 0xB0,
	S2MPG15_PM_DCTRLSEL2 = 0xB1,
	S2MPG15_PM_DCTRLSEL3 = 0xB2,
	S2MPG15_PM_DCTRLSEL4 = 0xB3,
	S2MPG15_PM_DCTRLSEL5 = 0xB4,
	S2MPG15_PM_BUCK_OCP_EN1 = 0xB5,
	S2MPG15_PM_BUCK_OCP_EN2 = 0xB6,
	S2MPG15_PM_BUCK_OCP_PD_EN1 = 0xB7,
	S2MPG15_PM_BUCK_OCP_PD_EN2 = 0xB8,
	S2MPG15_PM_BUCK_OCP_CTRL1 = 0xB9,
	S2MPG15_PM_BUCK_OCP_CTRL2 = 0xBA,
	S2MPG15_PM_BUCK_OCP_CTRL3 = 0xBB,
	S2MPG15_PM_BUCK_OCP_CTRL4 = 0xBC,
	S2MPG15_PM_BUCK_OCP_CTRL5 = 0xBD,
	S2MPG15_PM_BUCK_OCP_CTRL6 = 0xBE,
	S2MPG15_PM_BUCK_OCP_CTRL7 = 0xBF,
	S2MPG15_PM_BUCK_OCP_CTRL8 = 0xC0,
	S2MPG15_PM_PSI_CTRL1 = 0xC1,
	S2MPG15_PM_PSI_CTRL2 = 0xC2,
	S2MPG15_PM_PSI_CTRL3 = 0xC3,
	S2MPG15_PM_PSI_CTRL4 = 0xC4,
	S2MPG15_PM_SEL_HW_VGPIO = 0xC5,
	S2MPG15_PM_B2S_OCP_WARN_X = 0xC6,
	S2MPG15_PM_B2S_OCP_WARN = 0xC7,
	S2MPG15_PM_B2S_SOFT_OCP_WARN = 0xC8,
	S2MPG15_PM_B2S_OCP_WARN_DEBOUNCE = 0xC9,
	S2MPG15_PM_B1S_OUT2 = 0xCA,
	S2MPG15_PM_B2S_OUT2 = 0xCB,
	S2MPG15_PM_B3S_OUT2 = 0xCC,
	S2MPG15_PM_B8S_OUT2 = 0xCD,
	S2MPG15_PM_B9S_OUT1 = 0xCE,
	S2MPG15_PM_B9S_OUT2 = 0xCF,
	S2MPG15_PM_B10S_OUT2 = 0xD0,
	S2MPG15_PM_B11S_OUT2 = 0xD1,
	S2MPG15_PM_B12S_OUT2 = 0xD2,
	S2MPG15_PM_L1S_CTRL2 = 0xD3,
	S2MPG15_PM_L23S_CTRL2 = 0xD4,
	S2MPG15_PM_BUCK_HRMODE1 = 0xD5,
	S2MPG15_PM_BUCK_HRMODE2 = 0xD6,
	S2MPG15_PM_LDO_SENSE1 = 0xD7,
	S2MPG15_PM_LDO_SENSE2 = 0xD8,
	S2MPG15_PM_LDO_SENSE3 = 0xD9,
	S2MPG15_PM_LDO_SENSE4 = 0xDA,
	S2MPG15_PM_B1S_USONIC = 0xDB,
	S2MPG15_PM_B2S_USONIC = 0xDC,
	S2MPG15_PM_B3S_USONIC = 0xDD,
	S2MPG15_PM_B4S_USONIC = 0xDE,
	S2MPG15_PM_B5S_USONIC = 0xDF,
	S2MPG15_PM_B6S_USONIC = 0xE0,
	S2MPG15_PM_B7S_USONIC = 0xE1,
	S2MPG15_PM_B8S_USONIC = 0xE2,
	S2MPG15_PM_B9S_USONIC = 0xE3,
	S2MPG15_PM_B10S_USONIC = 0xE4,
	S2MPG15_PM_B11S_USONIC = 0xE5,
	S2MPG15_PM_B12S_USONIC = 0xE6,
	S2MPG15_PM_BD_USONIC = 0xE7,
	S2MPG15_PM_BA_USONIC = 0xE8,
	S2MPG15_PM_BC_USONIC = 0xE9,
	S2MPG15_PM_BB_USONIC = 0xEA,
};

/* Meter(0xA) Registers */
enum S2MPG15_METER_REG {
	S2MPG15_METER_INT1 = 0x0,
	S2MPG15_METER_INT2 = 0x1,
	S2MPG15_METER_INT3 = 0x2,
	S2MPG15_METER_INT4 = 0x3,
	S2MPG15_METER_INT1M = 0x4,
	S2MPG15_METER_INT2M = 0x5,
	S2MPG15_METER_INT3M = 0x6,
	S2MPG15_METER_INT4M = 0x7,
	S2MPG15_METER_CTRL1 = 0x8,
	S2MPG15_METER_CTRL2 = 0x9,
	S2MPG15_METER_CTRL3 = 0xA,
	S2MPG15_METER_CTRL4 = 0xB,
	S2MPG15_METER_CTRL5 = 0xC,
	S2MPG15_METER_CTRL6 = 0xD,
	S2MPG15_METER_CTRL7 = 0xE,
	S2MPG15_METER_BUCKEN1 = 0xF,
	S2MPG15_METER_BUCKEN2 = 0x10,
	S2MPG15_METER_MUXSEL0 = 0x11,
	S2MPG15_METER_MUXSEL1 = 0x12,
	S2MPG15_METER_MUXSEL2 = 0x13,
	S2MPG15_METER_MUXSEL3 = 0x14,
	S2MPG15_METER_MUXSEL4 = 0x15,
	S2MPG15_METER_MUXSEL5 = 0x16,
	S2MPG15_METER_MUXSEL6 = 0x17,
	S2MPG15_METER_MUXSEL7 = 0x18,
	S2MPG15_METER_MUXSEL8 = 0x19,
	S2MPG15_METER_MUXSEL9 = 0x1A,
	S2MPG15_METER_MUXSEL10 = 0x1B,
	S2MPG15_METER_MUXSEL11 = 0x1C,
	S2MPG15_METER_LPF_C0_0 = 0x1D,
	S2MPG15_METER_LPF_C0_1 = 0x1E,
	S2MPG15_METER_LPF_C0_2 = 0x1F,
	S2MPG15_METER_LPF_C0_3 = 0x20,
	S2MPG15_METER_LPF_C0_4 = 0x21,
	S2MPG15_METER_LPF_C0_5 = 0x22,
	S2MPG15_METER_LPF_C0_6 = 0x23,
	S2MPG15_METER_LPF_C0_7 = 0x24,
	S2MPG15_METER_LPF_C0_8 = 0x25,
	S2MPG15_METER_LPF_C0_9 = 0x26,
	S2MPG15_METER_LPF_C0_10 = 0x27,
	S2MPG15_METER_LPF_C0_11 = 0x28,
	S2MPG15_METER_NTC_LPF_C0_0 = 0x29,
	S2MPG15_METER_NTC_LPF_C0_1 = 0x2A,
	S2MPG15_METER_NTC_LPF_C0_2 = 0x2B,
	S2MPG15_METER_NTC_LPF_C0_3 = 0x2C,
	S2MPG15_METER_NTC_LPF_C0_4 = 0x2D,
	S2MPG15_METER_NTC_LPF_C0_5 = 0x2E,
	S2MPG15_METER_NTC_LPF_C0_6 = 0x2F,
	S2MPG15_METER_NTC_LPF_C0_7 = 0x30,
	S2MPG15_METER_PWR_WARN0 = 0x31,
	S2MPG15_METER_PWR_WARN1 = 0x32,
	S2MPG15_METER_PWR_WARN2 = 0x33,
	S2MPG15_METER_PWR_WARN3 = 0x34,
	S2MPG15_METER_PWR_WARN4 = 0x35,
	S2MPG15_METER_PWR_WARN5 = 0x36,
	S2MPG15_METER_PWR_WARN6 = 0x37,
	S2MPG15_METER_PWR_WARN7 = 0x38,
	S2MPG15_METER_PWR_WARN8 = 0x39,
	S2MPG15_METER_PWR_WARN9 = 0x3A,
	S2MPG15_METER_PWR_WARN10 = 0x3B,
	S2MPG15_METER_PWR_WARN11 = 0x3C,
	S2MPG15_METER_NTC_OT_WARN0 = 0x3D,
	S2MPG15_METER_NTC_OT_WARN1 = 0x3E,
	S2MPG15_METER_NTC_OT_WARN2 = 0x3F,
	S2MPG15_METER_NTC_OT_WARN3 = 0x40,
	S2MPG15_METER_NTC_OT_WARN4 = 0x41,
	S2MPG15_METER_NTC_OT_WARN5 = 0x42,
	S2MPG15_METER_NTC_OT_WARN6 = 0x43,
	S2MPG15_METER_NTC_OT_WARN7 = 0x44,
	S2MPG15_METER_NTC_OT_FAULT0 = 0x45,
	S2MPG15_METER_NTC_OT_FAULT1 = 0x46,
	S2MPG15_METER_NTC_OT_FAULT2 = 0x47,
	S2MPG15_METER_NTC_OT_FAULT3 = 0x48,
	S2MPG15_METER_NTC_OT_FAULT4 = 0x49,
	S2MPG15_METER_NTC_OT_FAULT5 = 0x4A,
	S2MPG15_METER_NTC_OT_FAULT6 = 0x4B,
	S2MPG15_METER_NTC_OT_FAULT7 = 0x4C,
	S2MPG15_METER_NTC_UT_WARN0 = 0x4D,
	S2MPG15_METER_NTC_UT_WARN1 = 0x4E,
	S2MPG15_METER_NTC_UT_WARN2 = 0x4F,
	S2MPG15_METER_NTC_UT_WARN3 = 0x50,
	S2MPG15_METER_NTC_UT_WARN4 = 0x51,
	S2MPG15_METER_NTC_UT_WARN5 = 0x52,
	S2MPG15_METER_NTC_UT_WARN6 = 0x53,
	S2MPG15_METER_NTC_UT_WARN7 = 0x54,
	S2MPG15_METER_ACC_DATA_CH0_1 = 0x63,
	S2MPG15_METER_ACC_DATA_CH0_2 = 0x64,
	S2MPG15_METER_ACC_DATA_CH0_3 = 0x65,
	S2MPG15_METER_ACC_DATA_CH0_4 = 0x66,
	S2MPG15_METER_ACC_DATA_CH0_5 = 0x67,
	S2MPG15_METER_ACC_DATA_CH0_6 = 0x68,
	S2MPG15_METER_ACC_DATA_CH1_1 = 0x69,
	S2MPG15_METER_ACC_DATA_CH1_2 = 0x6A,
	S2MPG15_METER_ACC_DATA_CH1_3 = 0x6B,
	S2MPG15_METER_ACC_DATA_CH1_4 = 0x6C,
	S2MPG15_METER_ACC_DATA_CH1_5 = 0x6D,
	S2MPG15_METER_ACC_DATA_CH1_6 = 0x6E,
	S2MPG15_METER_ACC_DATA_CH2_1 = 0x6F,
	S2MPG15_METER_ACC_DATA_CH2_2 = 0x70,
	S2MPG15_METER_ACC_DATA_CH2_3 = 0x71,
	S2MPG15_METER_ACC_DATA_CH2_4 = 0x72,
	S2MPG15_METER_ACC_DATA_CH2_5 = 0x73,
	S2MPG15_METER_ACC_DATA_CH2_6 = 0x74,
	S2MPG15_METER_ACC_DATA_CH3_1 = 0x75,
	S2MPG15_METER_ACC_DATA_CH3_2 = 0x76,
	S2MPG15_METER_ACC_DATA_CH3_3 = 0x77,
	S2MPG15_METER_ACC_DATA_CH3_4 = 0x78,
	S2MPG15_METER_ACC_DATA_CH3_5 = 0x79,
	S2MPG15_METER_ACC_DATA_CH3_6 = 0x7A,
	S2MPG15_METER_ACC_DATA_CH4_1 = 0x7B,
	S2MPG15_METER_ACC_DATA_CH4_2 = 0x7C,
	S2MPG15_METER_ACC_DATA_CH4_3 = 0x7D,
	S2MPG15_METER_ACC_DATA_CH4_4 = 0x7E,
	S2MPG15_METER_ACC_DATA_CH4_5 = 0x7F,
	S2MPG15_METER_ACC_DATA_CH4_6 = 0x80,
	S2MPG15_METER_ACC_DATA_CH5_1 = 0x81,
	S2MPG15_METER_ACC_DATA_CH5_2 = 0x82,
	S2MPG15_METER_ACC_DATA_CH5_3 = 0x83,
	S2MPG15_METER_ACC_DATA_CH5_4 = 0x84,
	S2MPG15_METER_ACC_DATA_CH5_5 = 0x85,
	S2MPG15_METER_ACC_DATA_CH5_6 = 0x86,
	S2MPG15_METER_ACC_DATA_CH6_1 = 0x87,
	S2MPG15_METER_ACC_DATA_CH6_2 = 0x88,
	S2MPG15_METER_ACC_DATA_CH6_3 = 0x89,
	S2MPG15_METER_ACC_DATA_CH6_4 = 0x8A,
	S2MPG15_METER_ACC_DATA_CH6_5 = 0x8B,
	S2MPG15_METER_ACC_DATA_CH6_6 = 0x8C,
	S2MPG15_METER_ACC_DATA_CH7_1 = 0x8D,
	S2MPG15_METER_ACC_DATA_CH7_2 = 0x8E,
	S2MPG15_METER_ACC_DATA_CH7_3 = 0x8F,
	S2MPG15_METER_ACC_DATA_CH7_4 = 0x90,
	S2MPG15_METER_ACC_DATA_CH7_5 = 0x91,
	S2MPG15_METER_ACC_DATA_CH7_6 = 0x92,
	S2MPG15_METER_ACC_DATA_CH8_1 = 0x93,
	S2MPG15_METER_ACC_DATA_CH8_2 = 0x94,
	S2MPG15_METER_ACC_DATA_CH8_3 = 0x95,
	S2MPG15_METER_ACC_DATA_CH8_4 = 0x96,
	S2MPG15_METER_ACC_DATA_CH8_5 = 0x97,
	S2MPG15_METER_ACC_DATA_CH8_6 = 0x98,
	S2MPG15_METER_ACC_DATA_CH9_1 = 0x99,
	S2MPG15_METER_ACC_DATA_CH9_2 = 0x9A,
	S2MPG15_METER_ACC_DATA_CH9_3 = 0x9B,
	S2MPG15_METER_ACC_DATA_CH9_4 = 0x9C,
	S2MPG15_METER_ACC_DATA_CH9_5 = 0x9D,
	S2MPG15_METER_ACC_DATA_CH9_6 = 0x9E,
	S2MPG15_METER_ACC_DATA_CH10_1 = 0x9F,
	S2MPG15_METER_ACC_DATA_CH10_2 = 0xA0,
	S2MPG15_METER_ACC_DATA_CH10_3 = 0xA1,
	S2MPG15_METER_ACC_DATA_CH10_4 = 0xA2,
	S2MPG15_METER_ACC_DATA_CH10_5 = 0xA3,
	S2MPG15_METER_ACC_DATA_CH10_6 = 0xA4,
	S2MPG15_METER_ACC_DATA_CH11_1 = 0xA5,
	S2MPG15_METER_ACC_DATA_CH11_2 = 0xA6,
	S2MPG15_METER_ACC_DATA_CH11_3 = 0xA7,
	S2MPG15_METER_ACC_DATA_CH11_4 = 0xA8,
	S2MPG15_METER_ACC_DATA_CH11_5 = 0xA9,
	S2MPG15_METER_ACC_DATA_CH11_6 = 0xAA,
	S2MPG15_METER_ACC_COUNT_1 = 0xAB,
	S2MPG15_METER_ACC_COUNT_2 = 0xAC,
	S2MPG15_METER_ACC_COUNT_3 = 0xAD,
	S2MPG15_METER_LPF_DATA_CH0_1 = 0xAE,
	S2MPG15_METER_LPF_DATA_CH0_2 = 0xAF,
	S2MPG15_METER_LPF_DATA_CH0_3 = 0xB0,
	S2MPG15_METER_LPF_DATA_CH1_1 = 0xB1,
	S2MPG15_METER_LPF_DATA_CH1_2 = 0xB2,
	S2MPG15_METER_LPF_DATA_CH1_3 = 0xB3,
	S2MPG15_METER_LPF_DATA_CH2_1 = 0xB4,
	S2MPG15_METER_LPF_DATA_CH2_2 = 0xB5,
	S2MPG15_METER_LPF_DATA_CH2_3 = 0xB6,
	S2MPG15_METER_LPF_DATA_CH3_1 = 0xB7,
	S2MPG15_METER_LPF_DATA_CH3_2 = 0xB8,
	S2MPG15_METER_LPF_DATA_CH3_3 = 0xB9,
	S2MPG15_METER_LPF_DATA_CH4_1 = 0xBA,
	S2MPG15_METER_LPF_DATA_CH4_2 = 0xBB,
	S2MPG15_METER_LPF_DATA_CH4_3 = 0xBC,
	S2MPG15_METER_LPF_DATA_CH5_1 = 0xBD,
	S2MPG15_METER_LPF_DATA_CH5_2 = 0xBE,
	S2MPG15_METER_LPF_DATA_CH5_3 = 0xBF,
	S2MPG15_METER_LPF_DATA_CH6_1 = 0xC0,
	S2MPG15_METER_LPF_DATA_CH6_2 = 0xC1,
	S2MPG15_METER_LPF_DATA_CH6_3 = 0xC2,
	S2MPG15_METER_LPF_DATA_CH7_1 = 0xC3,
	S2MPG15_METER_LPF_DATA_CH7_2 = 0xC4,
	S2MPG15_METER_LPF_DATA_CH7_3 = 0xC5,
	S2MPG15_METER_LPF_DATA_CH8_1 = 0xC6,
	S2MPG15_METER_LPF_DATA_CH8_2 = 0xC7,
	S2MPG15_METER_LPF_DATA_CH8_3 = 0xC8,
	S2MPG15_METER_LPF_DATA_CH9_1 = 0xC9,
	S2MPG15_METER_LPF_DATA_CH9_2 = 0xCA,
	S2MPG15_METER_LPF_DATA_CH9_3 = 0xCB,
	S2MPG15_METER_LPF_DATA_CH10_1 = 0xCC,
	S2MPG15_METER_LPF_DATA_CH10_2 = 0xCD,
	S2MPG15_METER_LPF_DATA_CH10_3 = 0xCE,
	S2MPG15_METER_LPF_DATA_CH11_1 = 0xCF,
	S2MPG15_METER_LPF_DATA_CH11_2 = 0xD0,
	S2MPG15_METER_LPF_DATA_CH11_3 = 0xD1,
	S2MPG15_METER_VBAT_DATA1 = 0xD2,
	S2MPG15_METER_VBAT_DATA2 = 0xD3,
	S2MPG15_METER_LPF_DATA_NTC0_1 = 0xD4,
	S2MPG15_METER_LPF_DATA_NTC0_2 = 0xD5,
	S2MPG15_METER_LPF_DATA_NTC1_1 = 0xD6,
	S2MPG15_METER_LPF_DATA_NTC1_2 = 0xD7,
	S2MPG15_METER_LPF_DATA_NTC2_1 = 0xD8,
	S2MPG15_METER_LPF_DATA_NTC2_2 = 0xD9,
	S2MPG15_METER_LPF_DATA_NTC3_1 = 0xDA,
	S2MPG15_METER_LPF_DATA_NTC3_2 = 0xDB,
	S2MPG15_METER_LPF_DATA_NTC4_1 = 0xDC,
	S2MPG15_METER_LPF_DATA_NTC4_2 = 0xDD,
	S2MPG15_METER_LPF_DATA_NTC5_1 = 0xDE,
	S2MPG15_METER_LPF_DATA_NTC5_2 = 0xDF,
	S2MPG15_METER_LPF_DATA_NTC6_1 = 0xE0,
	S2MPG15_METER_LPF_DATA_NTC6_2 = 0xE1,
	S2MPG15_METER_LPF_DATA_NTC7_1 = 0xE2,
	S2MPG15_METER_LPF_DATA_NTC7_2 = 0xE3,
	S2MPG15_METER_EXT_SIGNED_DATA1 = 0xE4,
	S2MPG15_METER_EXT_SIGNED_DATA2 = 0xE5,
};

/* gpio(0xC) Registers */
enum S2MPG15_GPIO_REG {
	S2MPG15_GPIO_INT1 = 0x0,
	S2MPG15_GPIO_INT2 = 0x1,
	S2MPG15_GPIO_INT3 = 0x2,
	S2MPG15_GPIO_INT1M = 0x3,
	S2MPG15_GPIO_INT2M = 0x4,
	S2MPG15_GPIO_INT3M = 0x5,
	S2MPG15_GPIO_STATUS1 = 0x6,
	S2MPG15_GPIO_STATUS2 = 0x7,
	S2MPG15_GPIO0_SET = 0x8,
	S2MPG15_GPIO1_SET = 0x9,
	S2MPG15_GPIO2_SET = 0xA,
	S2MPG15_GPIO3_SET = 0xB,
	S2MPG15_GPIO4_SET = 0xC,
	S2MPG15_GPIO5_SET = 0xD,
	S2MPG15_GPIO6_SET = 0xE,
	S2MPG15_GPIO7_SET = 0xF,
	S2MPG15_GPIO8_SET = 0x10,
	S2MPG15_GPIO9_SET = 0x11,
	S2MPG15_GPIO0_MONSEL = 0x12,
	S2MPG15_GPIO1_MONSEL = 0x13,
	S2MPG15_GPIO2_MONSEL = 0x14,
	S2MPG15_GPIO3_MONSEL = 0x15,
	S2MPG15_GPIO4_MONSEL = 0x16,
	S2MPG15_GPIO5_MONSEL = 0x17,
	S2MPG15_GPIO6_MONSEL = 0x18,
	S2MPG15_GPIO7_MONSEL = 0x19,
	S2MPG15_GPIO8_MONSEL = 0x1A,
	S2MPG15_GPIO9_MONSEL = 0x1B,
};

/* PM_TRIM(0xF) Registers */
enum S2MPG15_PM_TRIM_REG {
	S2MPG15_PM_TRIM_BUCK_HR_MODE = 0xDC,
};

/* S2MPG15 regulator ids */
enum S2MPG15_REGULATOR {
	S2MPG15_LDO1,
	S2MPG15_LDO2,
	S2MPG15_LDO3,
	S2MPG15_LDO4,
	S2MPG15_LDO5,
	S2MPG15_LDO6,
	S2MPG15_LDO7,
	S2MPG15_LDO8,
	S2MPG15_LDO9,
	S2MPG15_LDO10,
	S2MPG15_LDO11,
	S2MPG15_LDO12,
	S2MPG15_LDO13,
	S2MPG15_LDO14,
	S2MPG15_LDO15,
	S2MPG15_LDO16,
	S2MPG15_LDO17,
	S2MPG15_LDO18,
	S2MPG15_LDO19,
	S2MPG15_LDO20,
	S2MPG15_LDO21,
	S2MPG15_LDO22,
	S2MPG15_LDO23,
	S2MPG15_LDO24,
	S2MPG15_LDO25,
	S2MPG15_LDO26,
	S2MPG15_LDO27,
	S2MPG15_LDO28,
	S2MPG15_LDO29,
	S2MPG15_BUCK1,
	S2MPG15_BUCK2,
	S2MPG15_BUCK3,
	S2MPG15_BUCK4,
	S2MPG15_BUCK5,
	S2MPG15_BUCK6,
	S2MPG15_BUCK7,
	S2MPG15_BUCK8,
	S2MPG15_BUCK9,
	S2MPG15_BUCK10,
	S2MPG15_BUCK11,
	S2MPG15_BUCK12,
	S2MPG15_BUCKD,
	S2MPG15_BUCKA,
	S2MPG15_BUCKC,
	S2MPG15_BUCKBOOST,
	S2MPG15_REGULATOR_MAX,
};

enum s2mpg15_irq {
	/* PMIC */
	S2MPG15_IRQ_PWR_OFF_INT1,
	S2MPG15_IRQ_PWR_ON_INT1,
	S2MPG15_IRQ_INT120C_INT1,
	S2MPG15_IRQ_INT140C_INT1,
	S2MPG15_IRQ_TSD_INT1,
	S2MPG15_IRQ_WRST_INT1,
	S2MPG15_IRQ_WTSR_INT1,

	S2MPG15_IRQ_SCL_SOFTRST_INT2,
	S2MPG15_IRQ_UV_BB_INT2,
	S2MPG15_IRQ_BB_NTR_DET_INT2,
	S2MPG15_IRQ_WLWP_ACC_INT2,

	S2MPG15_IRQ_OCP_B1S_INT3,
	S2MPG15_IRQ_OCP_B2S_INT3,
	S2MPG15_IRQ_OCP_B3S_INT3,
	S2MPG15_IRQ_OCP_B4S_INT3,
	S2MPG15_IRQ_OCP_B5S_INT3,
	S2MPG15_IRQ_OCP_B6S_INT3,
	S2MPG15_IRQ_OCP_B7S_INT3,
	S2MPG15_IRQ_OCP_B8S_INT3,

	S2MPG15_IRQ_OCP_B9S_INT4,
	S2MPG15_IRQ_OCP_B10S_INT4,
	S2MPG15_IRQ_OCP_B11S_INT4,
	S2MPG15_IRQ_OCP_B12S_INT4,
	S2MPG15_IRQ_OCP_BDS_INT4,
	S2MPG15_IRQ_OCP_BAS_INT4,
	S2MPG15_IRQ_OCP_BCS_INT4,
	S2MPG15_IRQ_OCP_BBS_INT4,

	S2MPG15_IRQ_PMETER_OVERF_INT5,
	S2MPG15_IRQ_NTC_CYCLE_DONE_INT5,
	S2MPG15_IRQ_PWR_WARN_CH0_INT5,
	S2MPG15_IRQ_PWR_WARN_CH1_INT5,
	S2MPG15_IRQ_PWR_WARN_CH2_INT5,
	S2MPG15_IRQ_PWR_WARN_CH3_INT5,
	S2MPG15_IRQ_PWR_WARN_CH4_INT5,
	S2MPG15_IRQ_PWR_WARN_CH5_INT5,

	S2MPG15_IRQ_PWR_WARN_CH6_INT6,
	S2MPG15_IRQ_PWR_WARN_CH7_INT6,
	S2MPG15_IRQ_PWR_WARN_CH8_INT6,
	S2MPG15_IRQ_PWR_WARN_CH9_INT6,
	S2MPG15_IRQ_PWR_WARN_CH10_INT6,
	S2MPG15_IRQ_PWR_WARN_CH11_INT6,

	S2MPG15_IRQ_NTC_WARN_OT_CH1_INT7,
	S2MPG15_IRQ_NTC_WARN_OT_CH2_INT7,
	S2MPG15_IRQ_NTC_WARN_OT_CH3_INT7,
	S2MPG15_IRQ_NTC_WARN_OT_CH4_INT7,
	S2MPG15_IRQ_NTC_WARN_OT_CH5_INT7,
	S2MPG15_IRQ_NTC_WARN_OT_CH6_INT7,
	S2MPG15_IRQ_NTC_WARN_OT_CH7_INT7,
	S2MPG15_IRQ_NTC_WARN_OT_CH8_INT7,

	S2MPG15_IRQ_NTC_WARN_UT_CH1_INT8,
	S2MPG15_IRQ_NTC_WARN_UT_CH2_INT8,
	S2MPG15_IRQ_NTC_WARN_UT_CH3_INT8,
	S2MPG15_IRQ_NTC_WARN_UT_CH4_INT8,
	S2MPG15_IRQ_NTC_WARN_UT_CH5_INT8,
	S2MPG15_IRQ_NTC_WARN_UT_CH6_INT8,
	S2MPG15_IRQ_NTC_WARN_UT_CH7_INT8,
	S2MPG15_IRQ_NTC_WARN_UT_CH8_INT8,

	S2MPG15_IRQ_NR,
};

/* Common(0x0) MASK */
/* S2MPG15_COMMON_IBIM */
enum S2MPG15_IBIM1_REGION {
	S2MPG15_IBIM1_PM_REGION,
	S2MPG15_IBIM1_PMETER_REGION,
	S2MPG15_IBIM1_CCC_PARITY,
	S2MPG15_IBIM1_ADDR_PARITY,
	S2MPG15_IBIM1_DATA_PARITY,
	S2MPG15_IBIM1_REGION_MAX
};

enum S2MPG15_IBIM2_REGION {
	S2MPG15_IBIM2_OCP_WARN_B2S,
	S2MPG15_IBIM2_SOFT_OCP_WARN_B2S,
	S2MPG15_IBIM2_REGION_MAX
};

/* PM(0x1) MASK */
#define S2MPG15_BUCK_MAX	15
#define S2MPG15_LDO_MAX		29
#define S2MPG15_BB_MAX		1
#define S2MPG15_VGPIO_NUM	10
#define S2MPG15_VGPIO_MAX_VAL	(0xFF)

/* S2MPG15_PM_INT3M */
#define S2MPG15_IRQ_INT3M_SHIFT		0
#define S2MPG15_IRQ_INT3M_WIDTH		8
#define S2MPG15_IRQ_INT3M_MASK		MASK(S2MPG15_IRQ_INT3M_WIDTH, S2MPG15_IRQ_INT3M_SHIFT)

#define S2MPG15_IRQ_OCP_B1S_MASK	BIT(0)
#define S2MPG15_IRQ_OCP_B2S_MASK	BIT(1)
#define S2MPG15_IRQ_OCP_B3S_MASK	BIT(2)
#define S2MPG15_IRQ_OCP_B4S_MASK	BIT(3)
#define S2MPG15_IRQ_OCP_B5S_MASK	BIT(4)
#define S2MPG15_IRQ_OCP_B6S_MASK	BIT(5)
#define S2MPG15_IRQ_OCP_B7S_MASK	BIT(6)
#define S2MPG15_IRQ_OCP_B8S_MASK	BIT(7)

/* S2MPG15_PM_INT4M */
#define S2MPG15_IRQ_INT4M_SHIFT		0
#define S2MPG15_IRQ_INT4M_WIDTH		8
#define S2MPG15_IRQ_INT4M_MASK		MASK(S2MPG15_IRQ_INT4M_WIDTH, S2MPG15_IRQ_INT4M_SHIFT)

#define S2MPG15_IRQ_OCP_B9S_MASK	BIT(0)
#define S2MPG15_IRQ_OCP_B10S_MASK	BIT(1)
#define S2MPG15_IRQ_OCP_B11S_MASK	BIT(2)
#define S2MPG15_IRQ_OCP_B12S_MASK	BIT(3)
#define S2MPG15_IRQ_OCP_BDS_MASK	BIT(4)
#define S2MPG15_IRQ_OCP_BAS_MASK	BIT(5)
#define S2MPG15_IRQ_OCP_BCS_MASK	BIT(6)
#define S2MPG15_IRQ_OCP_BBS_MASK	BIT(7)

/* S2MPG15_PM_CTRL1 (0x10B) */
#define S2MPG15_WTSREN_MASK	BIT(5)

/*
 * _MIN(group) S2MPG15_REG_MIN##group
 * _STEP(group) S2MPG15_REG_STEP##group
 */

/* BUCK 1S~6S, BUCK8S~12S, BUCKC ## group: 1 */
#define S2MPG15_REG_MIN1	200000
#define S2MPG15_REG_STEP1	6250
/* BUCK 7S,D,A ## group: 2 */
#define S2MPG15_REG_MIN2	600000
#define S2MPG15_REG_STEP2	12500
/* BUCKBOOST ## group: 3 */
#define S2MPG15_REG_MIN3	2600000
#define S2MPG15_REG_STEP3	12500
/* LDO 1S,2S,23S ## group: 4 */
#define S2MPG15_REG_MIN4	300000
#define S2MPG15_REG_STEP4	12500
/* LDO 3S,9S,21S,24S ## group: 5 */
#define S2MPG15_REG_MIN5	725000
#define S2MPG15_REG_STEP5	12500
/* LDO 4S,7S,10S~14S,17S,19S,20S,22S,27S,29S ## group: 6 */
#define S2MPG15_REG_MIN6	700000
#define S2MPG15_REG_STEP6	25000
/* LDO 5S,6S,8S,15S,16S,18S,25S,26S,28S ## group: 7 */
#define S2MPG15_REG_MIN7	1800000
#define S2MPG15_REG_STEP7	25000

/* _N_VOLTAGES(num) S2MPG15_REG_N_VOLTAGES_##num */
#define S2MPG15_REG_N_VOLTAGES_64	0x40
#define S2MPG15_REG_N_VOLTAGES_128	0x80
#define S2MPG15_REG_N_VOLTAGES_256	0x100

#define S2MPG15_REG_ENABLE_WIDTH		2
#define S2MPG15_REG_ENABLE_MASK_1_0		MASK(S2MPG15_REG_ENABLE_WIDTH, 0)
#define S2MPG15_REG_ENABLE_MASK_3_2		MASK(S2MPG15_REG_ENABLE_WIDTH, 2)
#define S2MPG15_REG_ENABLE_MASK_4_3		MASK(S2MPG15_REG_ENABLE_WIDTH, 3)
#define S2MPG15_REG_ENABLE_MASK_5_4		MASK(S2MPG15_REG_ENABLE_WIDTH, 4)
#define S2MPG15_REG_ENABLE_MASK_7_6		MASK(S2MPG15_REG_ENABLE_WIDTH, 6)
#define S2MPG15_REG_ENABLE_MASK_7		BIT(7)

/* _TIME(macro) S2MPG15_ENABLE_TIME##macro */
#define S2MPG15_ENABLE_TIME_LDO 128
#define S2MPG15_ENABLE_TIME_BUCK 130

/* S2MPG15_PM_DCTRLSEL1 ~ 5 */
#define DCTRLSEL_VOUT1			0x0
#define DCTRLSEL_PWREN			0x1
#define DCTRLSEL_PWREN_TRG		0x2
#define DCTRLSEL_PWREN_MIF		0x3
#define DCTRLSEL_PWREN_MIF_TRG		0x4
#define DCTRLSEL_AP_ACTIVE_N		0x5
#define DCTRLSEL_AP_ACTIVE_N_TRG	0x6
#define DCTRLSEL_PWREN_MIF_AP_ACTIVE_N	0x7
#define DCTRLSEL_G3D_EN			0x8
#define DCTRLSEL_PWREN_MIF_TRG_PWREN	0x9

/* S2MPG15_PM_OCP_WARN */
#define S2MPG15_OCP_WARN_EN_SHIFT	7
#define S2MPG15_OCP_WARN_CNT_SHIFT	6
#define S2MPG15_OCP_WARN_DVS_MASK_SHIFT	5
#define S2MPG15_OCP_WARN_LVL_SHIFT	0

#define S2MPG15_METER_NTC_BUF 2 /* 12-bit */

#define S2MPG15_METER_EN_MASK		BIT(0)

/* MT TRIM register for enabling NTC channels */
#define S2MPG15_MT_TRIM_NTC_EN_REG	0x34

#endif /* __LINUX_MFD_S2MPG15_REGISTER_H */
