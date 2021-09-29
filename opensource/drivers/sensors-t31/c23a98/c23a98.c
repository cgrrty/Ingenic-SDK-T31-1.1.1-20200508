/*
 * c23a98.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>

#include <tx-isp-common.h>
#include <sensor-common.h>

#define C23A98_CHIP_ID_H	(0x23)
#define C23A98_CHIP_ID_L	(0x98)
#define C23A98_REG_END		0xffff
#define C23A98_REG_DELAY	0x0000
#define C23A98_SUPPORT_30FPS_SCLK (75182400)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20200216a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int wdr_bufsize = 4073600;//1451520;
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut c23a98_again_lut[] = {
	{0x0, 0},
	{0x2, 11136},
	{0x4, 21097},
	{0x6, 30109},
	{0x8, 38336},
	{0xa, 45904},
	{0xc, 52910},
	{0xe, 59433},
	{0x10, 65536},
	{0x14, 76672},
	{0x18, 86633},
	{0x1c, 95645},
	{0x20, 103872},
	{0x24, 111440},
	{0x28, 118446},
	{0x2c, 124969},
	{0x30, 131072},
	{0x38, 142208},
	{0x40, 152169},
	{0x48, 161181},
	{0x50, 169408},
	{0x58, 176976},
	{0x60, 183982},
	{0x68, 190505},
	{0x70, 196608},
	{0x80, 207744},
	{0x90, 217705},
	{0xa0, 226717},
	{0xb0, 234944},
	{0xc0, 242512},
	{0xd0, 249518},
	{0xe0, 256041},
};

struct tx_isp_sensor_attribute c23a98_attr;

unsigned int c23a98_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = c23a98_again_lut;
	while(lut->gain <= c23a98_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == c23a98_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int c23a98_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = c23a98_again_lut;
	while(lut->gain <= c23a98_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == c23a98_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int c23a98_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute c23a98_attr={
	.name = "c23a98",
	.chip_id = 0x0207,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 648,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.image_twidth = 1920,
		.image_theight = 1080,
		.mipi_sc.mipi_crop_start0x = 0,
		.mipi_sc.mipi_crop_start0y = 0,
		.mipi_sc.mipi_crop_start1x = 0,
		.mipi_sc.mipi_crop_start1y = 0,
		.mipi_sc.mipi_crop_start2x = 0,
		.mipi_sc.mipi_crop_start2y = 0,
		.mipi_sc.mipi_crop_start3x = 0,
		.mipi_sc.mipi_crop_start3y = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = 0,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 256041,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 1324 - 4,
	.integration_time_limit = 1324 - 4,
	.total_width = 2270,
	.total_height = 1324,
	.max_integration_time = 1324 - 4,
	.one_line_expr_in_us = 30,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = c23a98_alloc_again,
	.sensor_ctrl.alloc_again_short = c23a98_alloc_again_short,
	.sensor_ctrl.alloc_dgain = c23a98_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};


static struct regval_list c23a98_init_regs_1080p_30fps_mipi[] = {
	{0x0103,0x01},

	{0x0401,0x3b},
	{0x0403,0x00},

	{0x3180,0x20},
	{0x3c01,0x13},
	{0x022d,0x1f},
	{0x3f08,0x10},

	{0x0100,0x01}, /* stream on */

	{0x0304,0x05}, /* 05: 30fps, 00: 60fps */
	{0x3517,0x58},
	{0x3600,0xd8},
	{0x3602,0x01},
	{0x3583,0x10},
	{0x3584,0x02},
	{0xa000,0x0b},
	{0xa001,0x50},
	{0xa002,0x00},
	{0xa003,0x80},
	{0xa004,0x00},
	{0xa005,0x00},
	{0xa006,0x00},
	{0xa007,0x80},
	{0xa008,0xce},
	{0xa009,0x81},
	{0xa00a,0x03},
	{0xa00b,0x1c},
	{0xa00c,0x03},
	{0xa00d,0xa5},
	{0xa00e,0x8c},
	{0xa00f,0x81},
	{0xa010,0x0a},
	{0xa011,0x90},
	{0xa012,0x8d},
	{0xa013,0x81},
	{0xa014,0x8c},
	{0xa015,0x50},
	{0xa016,0xbc},
	{0xa017,0xd0},
	{0xa018,0x92},
	{0xa019,0xe0},
	{0xa01a,0x85},
	{0xa01b,0x01},
	{0xa01c,0x84},
	{0xa01d,0x60},
	{0xa01e,0x06},
	{0xa01f,0x81},
	{0xa020,0x08},
	{0xa021,0x10},
	{0xa022,0x0f},
	{0xa023,0x81},
	{0xa024,0x88},
	{0xa025,0x10},
	{0xa026,0x0f},
	{0xa027,0x01},
	{0xa028,0x8f},
	{0xa029,0x90},
	{0xa02a,0xa4},
	{0xa02b,0x7d},
	{0xa02c,0x0e},
	{0xa02d,0x81},
	{0xa02e,0x5c},
	{0xa02f,0xe0},
	{0xa030,0x8f},
	{0xa031,0x84},
	{0xa032,0x83},
	{0xa033,0x38},
	{0xa034,0x0e},
	{0xa035,0x03},
	{0xa036,0xf1},
	{0xa037,0x60},
	{0xa038,0x8f},
	{0xa039,0x84},
	{0xa03a,0x03},
	{0xa03b,0x38},
	{0xa03c,0xa2},
	{0xa03d,0x50},
	{0xa03e,0x94},
	{0xa03f,0x60},
	{0xa040,0x0e},
	{0xa041,0x81},
	{0xa042,0x03},
	{0xa043,0xa0},
	{0xa044,0x8e},
	{0xa045,0x99},
	{0xa046,0x68},
	{0xa047,0x61},
	{0xa048,0x8e},
	{0xa049,0x0f},
	{0xa04a,0x92},
	{0xa04b,0x60},
	{0xa04c,0x05},
	{0xa04d,0x01},
	{0xa04e,0xb1},
	{0xa04f,0xe0},
	{0xa050,0x06},
	{0xa051,0x81},
	{0xa052,0x8e},
	{0xa053,0x90},
	{0xa054,0x08},
	{0xa055,0x81},
	{0xa056,0x90},
	{0xa057,0xe0},
	{0xa058,0x05},
	{0xa059,0x81},
	{0xa05a,0xb6},
	{0xa05b,0x60},
	{0xa05c,0x06},
	{0xa05d,0x81},
	{0xa05e,0x08},
	{0xa05f,0x03},
	{0xa060,0x0d},
	{0xa061,0x10},
	{0xa062,0x8a},
	{0xa063,0x01},
	{0xa064,0x0c},
	{0xa065,0x9c},
	{0xa066,0x03},
	{0xa067,0x81},
	{0xa068,0xce},
	{0xa069,0x1d},
	{0xa06a,0xce},
	{0xa06b,0x9c},
	{0xa06c,0x89},
	{0xa06d,0x00},
	{0xa06e,0x83},
	{0xa06f,0x2d},
	{0xa070,0x01},
	{0xa071,0x2e},
	{0xa072,0x8b},
	{0xa073,0xac},
	{0xa074,0x0b},
	{0xa075,0x2f},
	{0xa076,0x3b},
	{0xa077,0x50},
	{0xa078,0x83},
	{0xa079,0x83},
	{0xa07a,0xb7},
	{0xa07b,0x50},
	{0x3583,0x00},
	{0x3584,0x22},

	{0x3009,0x03},
	{0x300b,0x0c},
	{0x0101,0x01},/*0x1 default mirror on*/
	{0x3806,0x06},/*hs prepare*/
	{0x034f,0x38},
	{0x0340,0x05},
	{0x0341,0x2c},/*vts for 25fps*/
	{0x0100,0x01}, /* stream on */

	{C23A98_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list c23a98_init_regs_1080p_2to1_30fps_mipi[] = {
	{0x0103,0x01},
	{0x0401,0x3b},
	{0x0403,0x00},

	{0x3180,0x20},
	{0x3c01,0x13},
	{0x022d,0x1f},
	{0x3087,0x70},
	{0x3f08,0x10},
	{0x3026,0x01},
	{0x3009,0x05},
	{0x0101,0x01},
	{0x0304,0x05},
	{0x3517,0x58},
	{0x0304,0x00},
	{0x3211,0x3b},
	{0x3216,0x28},
	{0x3217,0x28},
	{0x3219,0x52},
	{0x3226,0x00},
	{0x3227,0x80},
	{0x3089,0x40},
	{0x31a0,0x20},
	{0x3218,0x4c},
	{0x322b,0x06},
	{0x3c01,0x33},
	{0x330a,0x10},
	{0x330b,0x10},
	{0x330c,0x10},
	{0x330d,0x10},
	{0x323b,0x23},
	{0x3023,0x06},
	{0x3181,0xd2},
	{0x3186,0x03},
	{0x380c,0x20},
	{0x380d,0x10},
	{0x3817,0x10},
	{0x0202,0x04},
	{0x0203,0x0e},
	{0x308a,0x00},
	{0x308b,0x40},
	{0x3600,0xc8},
	{0x3602,0x08},
	{0x3583,0x10},
	{0x3584,0x02},
	{0xa000,0x8b},
	{0xa001,0xd0},
	{0xa002,0x00},
	{0xa003,0x80},
	{0xa004,0x80},
	{0xa005,0x00},
	{0xa006,0x80},
	{0xa007,0x00},
	{0xa008,0xce},
	{0xa009,0x01},
	{0xa00a,0x03},
	{0xa00b,0x1c},
	{0xa00c,0x83},
	{0xa00d,0xa5},
	{0xa00e,0x8c},
	{0xa00f,0x01},
	{0xa010,0x0a},
	{0xa011,0x10},
	{0xa012,0x8d},
	{0xa013,0x81},
	{0xa014,0x8c},
	{0xa015,0x50},
	{0xa016,0x85},
	{0xa017,0xd1},
	{0xa018,0x03},
	{0xa019,0x60},
	{0xa01a,0x94},
	{0xa01b,0x15},
	{0xa01c,0x14},
	{0xa01d,0x8b},
	{0xa01e,0x30},
	{0xa01f,0x60},
	{0xa020,0x85},
	{0xa021,0x81},
	{0xa022,0x06},
	{0xa023,0x01},
	{0xa024,0x08},
	{0xa025,0x10},
	{0xa026,0x12},
	{0xa027,0x81},
	{0xa028,0x08},
	{0xa029,0x90},
	{0xa02a,0x12},
	{0xa02b,0x01},
	{0xa02c,0x92},
	{0xa02d,0x91},
	{0xa02e,0x83},
	{0xa02f,0xb2},
	{0xa030,0x14},
	{0xa031,0x03},
	{0xa032,0x14},
	{0xa033,0x10},
	{0xa034,0x03},
	{0xa035,0x32},
	{0xa036,0xa0},
	{0xa037,0x50},
	{0xa038,0x14},
	{0xa039,0x90},
	{0xa03a,0x83},
	{0xa03b,0xf4},
	{0xa03c,0x83},
	{0xa03d,0x3a},
	{0xa03e,0x26},
	{0xa03f,0xd0},
	{0xa040,0xa2},
	{0xa041,0xe0},
	{0xa042,0x05},
	{0xa043,0x01},
	{0xa044,0x32},
	{0xa045,0x60},
	{0xa046,0x86},
	{0xa047,0x81},
	{0xa048,0x82},
	{0xa049,0x61},
	{0xa04a,0xab},
	{0xa04b,0xd0},
	{0xa04c,0x22},
	{0xa04d,0x60},
	{0xa04e,0x85},
	{0xa04f,0x81},
	{0xa050,0x32},
	{0xa051,0xe0},
	{0xa052,0x06},
	{0xa053,0x01},
	{0xa054,0x82},
	{0xa055,0x60},
	{0xa056,0x08},
	{0xa057,0x01},
	{0xa058,0xb1},
	{0xa059,0x60},
	{0xa05a,0x85},
	{0xa05b,0x01},
	{0xa05c,0xb0},
	{0xa05d,0x60},
	{0xa05e,0x86},
	{0xa05f,0x01},
	{0xa060,0x88},
	{0xa061,0x90},
	{0xa062,0x0e},
	{0xa063,0x81},
	{0xa064,0x88},
	{0xa065,0x10},
	{0xa066,0x0e},
	{0xa067,0x01},
	{0xa068,0x12},
	{0xa069,0x96},
	{0xa06a,0x74},
	{0xa06b,0x50},
	{0xa06c,0x8e},
	{0xa06d,0x91},
	{0xa06e,0x03},
	{0xa06f,0x3a},
	{0xa070,0x74},
	{0xa071,0xd0},
	{0xa072,0x8a},
	{0xa073,0x61},
	{0xa074,0x05},
	{0xa075,0x81},
	{0xa076,0x30},
	{0xa077,0xe0},
	{0xa078,0x86},
	{0xa079,0x01},
	{0xa07a,0x08},
	{0xa07b,0x10},
	{0xa07c,0x0f},
	{0xa07d,0x01},
	{0xa07e,0x88},
	{0xa07f,0x10},
	{0xa080,0x8f},
	{0xa081,0x01},
	{0xa082,0x8b},
	{0xa083,0xe1},
	{0xa084,0x85},
	{0xa085,0x01},
	{0xa086,0xb0},
	{0xa087,0xe0},
	{0xa088,0x06},
	{0xa089,0x81},
	{0xa08a,0x08},
	{0xa08b,0x90},
	{0xa08c,0x10},
	{0xa08d,0x01},
	{0xa08e,0x88},
	{0xa08f,0x90},
	{0xa090,0x90},
	{0xa091,0x81},
	{0xa092,0x20},
	{0xa093,0xe0},
	{0xa094,0x85},
	{0xa095,0x81},
	{0xa096,0x82},
	{0xa097,0xe0},
	{0xa098,0x86},
	{0xa099,0x81},
	{0xa09a,0x8f},
	{0xa09b,0x90},
	{0xa09c,0x88},
	{0xa09d,0x01},
	{0xa09e,0xa1},
	{0xa09f,0x60},
	{0xa0a0,0x85},
	{0xa0a1,0x81},
	{0xa0a2,0x82},
	{0xa0a3,0xe0},
	{0xa0a4,0x86},
	{0xa0a5,0x81},
	{0xa0a6,0x90},
	{0xa0a7,0x90},
	{0xa0a8,0x88},
	{0xa0a9,0x81},
	{0xa0aa,0x12},
	{0xa0ab,0xe0},
	{0xa0ac,0x85},
	{0xa0ad,0x01},
	{0xa0ae,0x04},
	{0xa0af,0x60},
	{0xa0b0,0x06},
	{0xa0b1,0x81},
	{0xa0b2,0x88},
	{0xa0b3,0x10},
	{0xa0b4,0x13},
	{0xa0b5,0x81},
	{0xa0b6,0x88},
	{0xa0b7,0x90},
	{0xa0b8,0x13},
	{0xa0b9,0x81},
	{0xa0ba,0x93},
	{0xa0bb,0x90},
	{0xa0bc,0x24},
	{0xa0bd,0xfd},
	{0xa0be,0x11},
	{0xa0bf,0x81},
	{0xa0c0,0xdc},
	{0xa0c1,0x60},
	{0xa0c2,0x93},
	{0xa0c3,0x04},
	{0xa0c4,0x03},
	{0xa0c5,0x38},
	{0xa0c6,0x91},
	{0xa0c7,0x03},
	{0xa0c8,0xf1},
	{0xa0c9,0x60},
	{0xa0ca,0x93},
	{0xa0cb,0x04},
	{0xa0cc,0x83},
	{0xa0cd,0xb8},
	{0xa0ce,0xeb},
	{0xa0cf,0x50},
	{0xa0d0,0x94},
	{0xa0d1,0x60},
	{0xa0d2,0x11},
	{0xa0d3,0x81},
	{0xa0d4,0x03},
	{0xa0d5,0x20},
	{0xa0d6,0x91},
	{0xa0d7,0x19},
	{0xa0d8,0xe8},
	{0xa0d9,0x61},
	{0xa0da,0x91},
	{0xa0db,0x0f},
	{0xa0dc,0x92},
	{0xa0dd,0xe0},
	{0xa0de,0x85},
	{0xa0df,0x81},
	{0xa0e0,0xb1},
	{0xa0e1,0xe0},
	{0xa0e2,0x06},
	{0xa0e3,0x01},
	{0xa0e4,0x11},
	{0xa0e5,0x90},
	{0xa0e6,0x88},
	{0xa0e7,0x81},
	{0xa0e8,0x90},
	{0xa0e9,0xe0},
	{0xa0ea,0x05},
	{0xa0eb,0x81},
	{0xa0ec,0xb6},
	{0xa0ed,0x60},
	{0xa0ee,0x06},
	{0xa0ef,0x81},
	{0xa0f0,0x08},
	{0xa0f1,0x83},
	{0xa0f2,0x8d},
	{0xa0f3,0x90},
	{0xa0f4,0x8a},
	{0xa0f5,0x81},
	{0xa0f6,0x0c},
	{0xa0f7,0x9c},
	{0xa0f8,0x03},
	{0xa0f9,0x01},
	{0xa0fa,0xce},
	{0xa0fb,0x1d},
	{0xa0fc,0xce},
	{0xa0fd,0x1c},
	{0xa0fe,0x09},
	{0xa0ff,0x00},
	{0xa100,0x83},
	{0xa101,0xad},
	{0xa102,0x01},
	{0xa103,0x2e},
	{0xa104,0x0b},
	{0xa105,0x2c},
	{0xa106,0x0b},
	{0xa107,0x2f},
	{0xa108,0x04},
	{0xa109,0x51},
	{0xa10a,0x14},
	{0xa10b,0x83},
	{0xa10c,0x83},
	{0xa10d,0x83},
	{0xa10e,0x80},
	{0xa10f,0xd1},
	{0x3583,0x00},
	{0x3584,0x22},

	{0x380f,0x08},
	{0x034f,0x38},
	{0x300b,0x02},
	{0x0100,0x01}, //stream on

	{C23A98_REG_END, 0x00},	/* END MARKER */
};
/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting c23a98_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= c23a98_init_regs_1080p_30fps_mipi,
	},
	/*wdr 1080p*/
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= c23a98_init_regs_1080p_2to1_30fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &c23a98_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list c23a98_stream_on[] = {
	{0x0100, 0x01},
	{C23A98_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list c23a98_stream_off[] = {
	{0x0100, 0x00},
	{C23A98_REG_END, 0x00},	/* END MARKER */
};

int c23a98_read(struct tx_isp_subdev *sd,  uint16_t reg,
	       unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int c23a98_write(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	int ret;
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};

	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int c23a98_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != C23A98_REG_END) {
		if (vals->reg_num == C23A98_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = c23a98_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}

	return 0;
}

static int c23a98_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != C23A98_REG_END) {
		if (vals->reg_num == C23A98_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = c23a98_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int c23a98_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int c23a98_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = c23a98_read(sd, 0x0000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != C23A98_CHIP_ID_H)
		return -ENODEV;
	ret = c23a98_read(sd, 0x0001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != C23A98_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int c23a98_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = c23a98_write(sd, 0x0203, value & 0xff);
	ret += c23a98_write(sd, 0x0202, value >> 8);
	if (ret < 0) {
		ISP_ERROR("c23a98_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int c23a98_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = c23a98_write(sd, 0x308b, value & 0xff);
	ret += c23a98_write(sd, 0x308a, value >> 8);
	if (ret < 0) {
		ISP_ERROR("c23a98_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int c23a98_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = c23a98_write(sd, 0x0205, value);
	if (ret < 0) {
		ISP_ERROR("c23a98_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int c23a98_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = c23a98_write(sd, 0x0206, value);
	if (ret < 0) {
		ISP_ERROR("c23a98_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int c23a98_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int c23a98_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int c23a98_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = c23a98_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int c23a98_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = c23a98_write_array(sd, c23a98_stream_on);
		pr_debug("c23a98 stream on\n");
	} else {
		ret = c23a98_write_array(sd, c23a98_stream_off);
		pr_debug("c23a98 stream off\n");
	}

	return ret;
}

static int c23a98_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	wpclk = C23A98_SUPPORT_30FPS_SCLK;
	max_fps = SENSOR_OUTPUT_MAX_FPS;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}
	ret += c23a98_read(sd, 0x0342, &tmp);
	hts = tmp;
	ret += c23a98_read(sd, 0x0343, &tmp);
	if(ret < 0)
		return -1;
	hts = (hts << 8) + tmp;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = c23a98_write(sd, 0x0340, (unsigned char)((vts & 0xff00) >> 8));
	ret += c23a98_write(sd, 0x0341, (unsigned char)(vts & 0xff));
	if(ret < 0)
		return -1;
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int c23a98_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

static int c23a98_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"c23a98_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"c23a98_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = c23a98_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an c23a98 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("c23a98 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "c23a98", sizeof("c23a98"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int c23a98_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = c23a98_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = c23a98_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = c23a98_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = c23a98_set_analog_gain_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = c23a98_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = c23a98_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = c23a98_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = c23a98_write_array(sd, c23a98_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = c23a98_write_array(sd, c23a98_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = c23a98_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int c23a98_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = c23a98_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int c23a98_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	c23a98_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops c23a98_core_ops = {
	.g_chip_ident = c23a98_g_chip_ident,
	.reset = c23a98_reset,
	.init = c23a98_init,
	/*.ioctl = c23a98_ops_ioctl,*/
	.g_register = c23a98_g_register,
	.s_register = c23a98_s_register,
};

static struct tx_isp_subdev_video_ops c23a98_video_ops = {
	.s_stream = c23a98_s_stream,
};

static struct tx_isp_subdev_sensor_ops	c23a98_sensor_ops = {
	.ioctl	= c23a98_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops c23a98_ops = {
	.core = &c23a98_core_ops,
	.video = &c23a98_video_ops,
	.sensor = &c23a98_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "c23a98",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int c23a98_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	/* ret = set_sensor_gpio_function(sensor_gpio_func); */
	/* if (ret < 0) */
	/* 	goto err_set_sensor_gpio; */

//	c23a98_attr.dvp.gpio = sensor_gpio_func;

	/*
	  convert sensor-gain into isp-gain,
	*/

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &c23a98_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &c23a98_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->c23a98\n");

	return 0;

err_get_mclk:
	kfree(sensor);

	return -1;
}

static int c23a98_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id c23a98_id[] = {
	{ "c23a98", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, c23a98_id);

static struct i2c_driver c23a98_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "c23a98",
	},
	.probe		= c23a98_probe,
	.remove		= c23a98_remove,
	.id_table	= c23a98_id,
};

static __init int init_c23a98(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init c23a98 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&c23a98_driver);
}

static __exit void exit_c23a98(void)
{
	private_i2c_del_driver(&c23a98_driver);
}

module_init(init_c23a98);
module_exit(exit_c23a98);

MODULE_DESCRIPTION("A low-level driver for CISTA sensors");
MODULE_LICENSE("GPL");
