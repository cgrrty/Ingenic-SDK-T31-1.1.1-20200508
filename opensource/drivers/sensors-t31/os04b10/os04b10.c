/*
 * os04b10.c
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

#define OS04B10_CHIP_ID_H	(0x43)
#define OS04B10_CHIP_ID_M	(0x08)
#define OS04B10_CHIP_ID_L	(0x01)
#define OS04B10_REG_END		0xff
#define OS04B10_REG_PAGE	0xfd
#define OS04B10_REG_DELAY	0xfe

#define OS04B10_SUPPORT_SCLK_FPS_25 (72000000)
#define OS04B10_SUPPORT_SCLK_FPS_20 (84000000)
#define OS04B10_VTS_25_FPS	0x71a
#define OS04B10_VTS_20_FPS	0xa5b
#define OS04B10_VB_25_FPS	0x149
#define OS04B10_VB_20_FPS	0x48a
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20200103a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_20;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};
struct again_lut os04b10_again_lut[] = {
	{0x10, 0},
	{0x11, 5731},
	{0x12, 11136},
	{0x13, 16248},
	{0x14, 21097},
	{0x15, 25710},
	{0x16, 30109},
	{0x17, 34312},
	{0x18, 38336},
	{0x19, 42195},
	{0x1a, 45904},
	{0x1b, 49472},
	{0x1c, 52910},
	{0x1d, 56228},
	{0x1e, 59433},
	{0x1f, 62534},
	{0x20, 65536},
	{0x22, 71267},
	{0x24, 76672},
	{0x26, 81784},
	{0x28, 86633},
	{0x2a, 91246},
	{0x2c, 95645},
	{0x2e, 99848},
	{0x30, 103872},
	{0x32, 107731},
	{0x34, 111440},
	{0x36, 115008},
	{0x38, 118446},
	{0x3a, 121764},
	{0x3c, 124969},
	{0x3e, 128070},
	{0x40, 131072},
	{0x44, 136803},
	{0x48, 142208},
	{0x4c, 147320},
	{0x50, 152169},
	{0x54, 156782},
	{0x58, 161181},
	{0x5c, 165384},
	{0x60, 169408},
	{0x64, 173267},
	{0x68, 176976},
	{0x6c, 180544},
	{0x70, 183982},
	{0x74, 187300},
	{0x78, 190505},
	{0x7c, 193606},
	{0x80, 196608},
	{0x88, 202339},
	{0x90, 207744},
	{0x98, 212856},
	{0xa0, 217705},
	{0xa8, 222318},
	{0xb0, 226717},
	{0xb8, 230920},
	{0xc0, 234944},
	{0xc8, 238803},
	{0xd0, 242512},
	{0xd8, 246080},
	{0xe0, 249518},
	{0xe8, 252836},
	{0xf0, 256041},
	{0xf8, 259142},
};

struct tx_isp_sensor_attribute os04b10_attr;

unsigned int os04b10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = os04b10_again_lut;

	while(lut->gain <= os04b10_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == os04b10_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int os04b10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute os04b10_attr={
	.name = "os04b10",
	.chip_id = 0x430801,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x3c,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 800,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.image_twidth = 2560,
		.image_theight = 1440,
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
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1818 - 21,
	.integration_time_limit = 1818 - 21,
	.total_width = 1584,
	.total_height = 1818,
	.max_integration_time = 1818 - 21,
	.one_line_expr_in_us = 22,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = os04b10_alloc_again,
	.sensor_ctrl.alloc_dgain = os04b10_alloc_dgain,
};

static struct regval_list os04b10_init_regs_2560_1440_25fps[] = {
#if 0
	{0xfd, 0x00},
	//{0x20, 0x00},

	{OS04B10_REG_DELAY, 0x10},
	{0xfd, 0x00},
	{0x34, 0x71},
	{0x32, 0x01},
	{0x33, 0x01},
	{0x2e, 0x0c},
	{0xfd, 0x01},
	{0x03, 0x01},
	{0x04, 0xc6},
	{0x06, 0x0b},
	{0x0a, 0x50},
	{0x38, 0x20},
	{0x39, 0x08},
	{0x31, 0x01},
	{0x24, 0xff},
	{0x01, 0x01},
	{0x11, 0x59},
	{0x13, 0xf4},
	{0x14, 0xff},
	{0x19, 0xf2},
	{0x16, 0x68},
	{0x1a, 0x5e},
	{0x1c, 0x1a},
	{0x1d, 0xd6},
	{0x1f, 0x17},
	{0x20, 0x99},
	{0x26, 0x76},
	{0x27, 0x0c},
	{0x29, 0x3b},
	{0x2a, 0x00},
	{0x2b, 0x8e},
	{0x2c, 0x0b},
	{0x2e, 0x02},
	{0x44, 0x03},
	{0x45, 0xbe},
	{0x50, 0x06},
	{0x51, 0x10},
	{0x52, 0x0d},
	{0x53, 0x08},
	{0x55, 0x15},
	{0x56, 0x00},
	{0x57, 0x09},
	{0x59, 0x00},
	{0x5a, 0x04},
	{0x5b, 0x00},
	{0x5c, 0xe0},
	{0x5d, 0x00},
	{0x65, 0x00},
	{0x67, 0x00},
	{0x66, 0x2a},
	{0x68, 0x2c},
	{0x69, 0x0c},
	{0x6a, 0x0a},
	{0x6b, 0x03},
	{0x6c, 0x18},
	{0x71, 0x42},
	{0x72, 0x04},
	{0x73, 0x30},
	{0x74, 0x03},
	{0x77, 0x28},
	{0x7b, 0x00},
	{0x7f, 0x18},
	{0x83, 0xf0},
	{0x85, 0x10},
	{0x86, 0xf0},
	{0x8a, 0x33},
	{0x8b, 0x33},
	{0x28, 0x04},
	{0x34, 0x00},
	{0x35, 0x08},
	{0x36, 0x0a},
	{0x37, 0x00},
	{0x4a, 0x00},
	{0x4b, 0x04},
	{0x4c, 0x05},
	{0x4d, 0xa0},
	{0x01, 0x01},
	{0x8e, 0x0a},
	{0x8f, 0x00},
	{0x90, 0x05},
	{0x91, 0xa0},
	{0xa1, 0x04},
	{0xc4, 0x80},
	{0xc5, 0x80},
	{0xc6, 0x80},
	{0xc7, 0x80},
	{0xfb, 0x00},

	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xb1, 0x01},
	{0xb6, 0x80},

	{0xfd, 0x00},
	{0x36, 0x01},
	{0x34, 0x72},
	{0x34, 0x71},
	{0x36, 0x00},
	{0xfd, 0x01},

	{0xfb, 0x03},
	{0xfd, 0x03},
	{0xc0, 0x01},

	{0xfd, 0x02},
	{0xa8, 0x01},
	{0xa9, 0x00},
	{0xaa, 0x08},
	{0xab, 0x00},
	{0xac, 0x08},
	{0xad, 0x05},
	{0xae, 0xa0},
	{0xaf, 0x0a},
	{0xb0, 0x00},
	{0x62, 0x09},
	{0x63, 0x00},

	{0xfd, 0x01},
	{0xb1, 0x03},
#else
	/*max 25fps*/
	{0xfd, 0x00},
	//{0x20, 0x00},
	{OS04B10_REG_DELAY, 0x10},

	{0xfd, 0x00},
	{0x34, 0x71},
	{0x32, 0x01},
	{0x33, 0x01},
	{0x2e, 0x0c},
	{0xfd, 0x01},
	{0x03, 0x01},
	{0x04, 0xc6},
	{0x05, 0x01},
	{0x06, 0x49},
	{0x0a, 0x50},
	{0x38, 0x20},
	{0x39, 0x08},
	{0x31, 0x01},
	{0x24, 0xff},
	{0x01, 0x01},
	{0x11, 0x59},
	{0x13, 0xf4},
	{0x14, 0xff},
	{0x19, 0xf2},
	{0x16, 0x68},
	{0x1a, 0x5e},
	{0x1c, 0x1a},
	{0x1d, 0xd6},
	{0x1f, 0x17},
	{0x20, 0x99},
	{0x26, 0x76},
	{0x27, 0x0c},
	{0x29, 0x3b},
	{0x2a, 0x00},
	{0x2b, 0x8e},
	{0x2c, 0x0b},
	{0x2e, 0x02},
	{0x44, 0x03},
	{0x45, 0xbe},
	{0x50, 0x06},
	{0x51, 0x10},
	{0x52, 0x0d},
	{0x53, 0x08},
	{0x55, 0x15},
	{0x56, 0x00},
	{0x57, 0x09},
	{0x59, 0x00},
	{0x5a, 0x04},
	{0x5b, 0x00},
	{0x5c, 0xe0},
	{0x5d, 0x00},
	{0x65, 0x00},
	{0x67, 0x00},
	{0x66, 0x2a},
	{0x68, 0x2c},
	{0x69, 0x0c},
	{0x6a, 0x0a},
	{0x6b, 0x03},
	{0x6c, 0x18},
	{0x71, 0x42},
	{0x72, 0x04},
	{0x73, 0x30},
	{0x74, 0x03},
	{0x77, 0x28},
	{0x7b, 0x00},
	{0x7f, 0x18},
	{0x83, 0xf0},
	{0x85, 0x10},
	{0x86, 0xf0},
	{0x8a, 0x33},
	{0x8b, 0x33},
	{0x28, 0x04},
	{0x34, 0x00},
	{0x35, 0x08},
	{0x36, 0x0a},
	{0x37, 0x00},
	{0x4a, 0x00},
	{0x4b, 0x04},
	{0x4c, 0x05},
	{0x4d, 0xa0},
	{0x01, 0x01},
	{0x8e, 0x0a},
	{0x8f, 0x00},
	{0x90, 0x05},
	{0x91, 0xa0},
	{0xa1, 0x04},
	{0xc4, 0x80},
	{0xc5, 0x80},
	{0xc6, 0x80},
	{0xc7, 0x80},
	{0xfb, 0x00},

	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xb1, 0x01},
	{0xb6, 0x80},

	{0xfd, 0x00},
	{0x36, 0x01},
	{0x34, 0x72},
	{0x34, 0x71},
	{0x36, 0x00},
	{0xfd, 0x01},

	{0xfb, 0x03},
	{0xfd, 0x03},
	{0xc0, 0x01},

	{0xfd, 0x02},
	{0xa8, 0x01},
	{0xa9, 0x00},
	{0xaa, 0x08},
	{0xab, 0x00},
	{0xac, 0x08},
	{0xad, 0x05},
	{0xae, 0xa0},
	{0xaf, 0x0a},
	{0xb0, 0x00},
	{0x62, 0x09},
	{0x63, 0x00},

	{0xfd, 0x01},
	{0xb1, 0x03},
#endif

	{OS04B10_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list os04b10_init_regs_2560_1440_20fps[] = {
	/*for T31N/L 1 buff*/
#if 0
	{0xfd, 0x00},
	//{0x20, 0x00},
	{0xfd, 0x00},
	{0x34, 0x71},
	{0x32, 0x01},
	{0x33, 0x01},
	{0x2e, 0x1c},
	{0x2f, 0x3d},
	{0xfd, 0x01},
	{0x03, 0x01},
	{0x04, 0xc6},
	{0x05, 0x04},
	{0x06, 0x81},
	{0x0a, 0x50},
	{0x38, 0x20},
	{0x39, 0x08},
	{0x31, 0x01},
	{0x24, 0xff},
	{0x01, 0x01},
	{0x11, 0x59},
	{0x13, 0xf4},
	{0x14, 0xff},
	{0x19, 0xf2},
	{0x16, 0x68},
	{0x1a, 0x5e},
	{0x1c, 0x1a},
	{0x1d, 0xd6},
	{0x1f, 0x17},
	{0x20, 0x99},
	{0x26, 0x76},
	{0x27, 0x0c},
	{0x29, 0x3b},
	{0x2a, 0x00},
	{0x2b, 0x8e},
	{0x2c, 0x0b},
	{0x2e, 0x02},
	{0x44, 0x03},
	{0x45, 0xbe},
	{0x50, 0x06},
	{0x51, 0x10},
	{0x52, 0x0d},
	{0x53, 0x08},
	{0x55, 0x15},
	{0x56, 0x00},
	{0x57, 0x09},
	{0x59, 0x00},
	{0x5a, 0x04},
	{0x5b, 0x00},
	{0x5c, 0xe0},
	{0x5d, 0x00},
	{0x65, 0x00},
	{0x67, 0x00},
	{0x66, 0x2a},
	{0x68, 0x2c},
	{0x69, 0x0c},
	{0x6a, 0x0a},
	{0x6b, 0x03},
	{0x6c, 0x18},
	{0x71, 0x42},
	{0x72, 0x04},
	{0x73, 0x30},
	{0x74, 0x03},
	{0x77, 0x28},
	{0x7b, 0x00},
	{0x7f, 0x18},
	{0x83, 0xf0},
	{0x85, 0x10},
	{0x86, 0xf0},
	{0x8a, 0x33},
	{0x8b, 0x33},
	{0x28, 0x04},
	{0x34, 0x00},
	{0x35, 0x08},
	{0x36, 0x0a},
	{0x37, 0x00},
	{0x4a, 0x00},
	{0x4b, 0x04},
	{0x4c, 0x05},
	{0x4d, 0xa0},
	{0x01, 0x01},
	{0x8e, 0x0a},
	{0x8f, 0x00},
	{0x90, 0x05},
	{0x91, 0xa0},
	{0xa1, 0x04},
	{0xc4, 0x80},
	{0xc5, 0x80},
	{0xc6, 0x80},
	{0xc7, 0x80},
	{0xfb, 0x00},
	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xb1, 0x01},
	{0xb6, 0x80},
	{0xfd, 0x00},
	{0x36, 0x01},
	{0x34, 0x72},
	{0x34, 0x71},
	{0x36, 0x00},
	{0xfd, 0x01},
	{0xfb, 0x03},
	{0xfd, 0x03},
	{0xc0, 0x01},
	{0xfd, 0x02},
	{0xa8, 0x01},
	{0xa9, 0x00},
	{0xaa, 0x08},
	{0xab, 0x00},
	{0xac, 0x08},
	{0xad, 0x05},
	{0xae, 0xa0},
	{0xaf, 0x0a},
	{0xb0, 0x00},
	{0x62, 0x09},
	{0x63, 0x00},
	{0xfd, 0x01},
	{0xb1, 0x03},
#else
	{0xfd, 0x00},
	//{0x20, 0x00},

	{OS04B10_REG_DELAY, 0x10},
	{0xfd, 0x00},
	{0x34, 0x71},
	{0x32, 0x01},
	{0x33, 0x01},
	{0x2e, 0x20},
	{0x2f, 0x3d},
	{0xfd, 0x01},
	{0x03, 0x01},
	{0x04, 0xc6},
	{0x05, 0x04},
	{0x06, 0x8a},/*0x48a for 20fps*/
	{0x0a, 0x50},
	{0x38, 0x20},
	{0x39, 0x08},
	{0x31, 0x01},
	{0x24, 0xff},
	{0x01, 0x01},
	{0x11, 0x59},
	{0x13, 0xf4},
	{0x14, 0xff},
	{0x19, 0xf2},
	{0x16, 0x68},
	{0x1a, 0x5e},
	{0x1c, 0x1a},
	{0x1d, 0xd6},
	{0x1f, 0x17},
	{0x20, 0x99},
	{0x26, 0x76},
	{0x27, 0x0c},
	{0x29, 0x3b},
	{0x2a, 0x00},
	{0x2b, 0x8e},
	{0x2c, 0x0b},
	{0x2e, 0x02},
	{0x44, 0x03},
	{0x45, 0xbe},
	{0x50, 0x06},
	{0x51, 0x10},
	{0x52, 0x0d},
	{0x53, 0x08},
	{0x55, 0x15},
	{0x56, 0x00},
	{0x57, 0x09},
	{0x59, 0x00},
	{0x5a, 0x04},
	{0x5b, 0x00},
	{0x5c, 0xe0},
	{0x5d, 0x00},
	{0x65, 0x00},
	{0x67, 0x00},
	{0x66, 0x2a},
	{0x68, 0x2c},
	{0x69, 0x0c},
	{0x6a, 0x0a},
	{0x6b, 0x03},
	{0x6c, 0x18},
	{0x71, 0x42},
	{0x72, 0x04},
	{0x73, 0x30},
	{0x74, 0x03},
	{0x77, 0x28},
	{0x7b, 0x00},
	{0x7f, 0x18},
	{0x83, 0xf0},
	{0x85, 0x10},
	{0x86, 0xf0},
	{0x8a, 0x33},
	{0x8b, 0x33},
	{0x28, 0x04},
	{0x34, 0x00},
	{0x35, 0x08},
	{0x36, 0x0a},
	{0x37, 0x00},
	{0x4a, 0x00},
	{0x4b, 0x04},
	{0x4c, 0x05},
	{0x4d, 0xa0},
	{0x01, 0x01},
	{0x8e, 0x0a},
	{0x8f, 0x00},
	{0x90, 0x05},
	{0x91, 0xa0},
	{0xa1, 0x04},
	{0xc4, 0x80},
	{0xc5, 0x80},
	{0xc6, 0x80},
	{0xc7, 0x80},
	{0xfb, 0x00},
	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xb1, 0x01},
	{0xb6, 0x80},
	{0xfd, 0x00},
	{0x36, 0x01},
	{0x34, 0x72},
	{0x34, 0x71},
	{0x36, 0x00},
	{0xfd, 0x01},
	{0xfb, 0x03},
	{0xfd, 0x03},
	{0xc0, 0x01},
	{0xfd, 0x02},
	{0xa8, 0x01},
	{0xa9, 0x00},
	{0xaa, 0x08},
	{0xab, 0x00},
	{0xac, 0x08},
	{0xad, 0x05},
	{0xae, 0xa0},
	{0xaf, 0x0a},
	{0xb0, 0x00},
	{0x62, 0x09},
	{0x63, 0x00},
	{0xfd, 0x01},
	{0xb1, 0x03},
#endif

	{OS04B10_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the os04b10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os04b10_win_sizes[] = {
	/* 5M @25fps*/
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SGBRG10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= os04b10_init_regs_2560_1440_25fps,
	},
	/* 5M @20fps*/
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 20 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SGBRG10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= os04b10_init_regs_2560_1440_20fps,
	},

};

/*
 * the part of driver was fixed.
 */

static struct regval_list os04b10_stream_on[] = {
	{OS04B10_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list os04b10_stream_off[] = {
	{OS04B10_REG_END, 0x00},	/* END MARKER */
};

int os04b10_read(struct tx_isp_subdev *sd, unsigned char reg,
		 unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
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

int os04b10_write(struct tx_isp_subdev *sd, unsigned char reg,
		  unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int os04b10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OS04B10_REG_END) {
		if (vals->reg_num == OS04B10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os04b10_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
			if (vals->reg_num == OS04B10_REG_PAGE){
				val &= 0xf8;
				val |= (vals->value & 0x07);
				ret = os04b10_write(sd, vals->reg_num, val);
				ret = os04b10_read(sd, vals->reg_num, &val);
			}
		}
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
static int os04b10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != OS04B10_REG_END) {
		if (vals->reg_num == OS04B10_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = os04b10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int os04b10_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int os04b10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = os04b10_read(sd, 0x02, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04B10_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = os04b10_read(sd, 0x03, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04B10_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = os04b10_read(sd, 0x04, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04B10_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 16) | v;
	return 0;
}

static int os04b10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret = os04b10_write(sd, 0xfd, 0x01);
	ret += os04b10_write(sd, 0x04, (unsigned char)(expo & 0xff));
	ret += os04b10_write(sd, 0x03, (unsigned char)((expo >> 8) & 0xff));
	ret += os04b10_write(sd, 0x01, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}

static int os04b10_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os04b10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = os04b10_write(sd, 0xfd, 0x01);
	ret += os04b10_write(sd, 0x24, value);
	ret += os04b10_write(sd, 0x01, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}

static int os04b10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os04b10_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{

	return 0;
}

static int os04b10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os04b10_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &os04b10_win_sizes[0];
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		wsize = &os04b10_win_sizes[0];
		break;
	case TX_SENSOR_MAX_FPS_20:
		wsize = &os04b10_win_sizes[1];
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = os04b10_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int os04b10_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = os04b10_write_array(sd, os04b10_stream_on);
		pr_debug("os04b10 stream on\n");
	}
	else {
		ret = os04b10_write_array(sd, os04b10_stream_off);
		pr_debug("os04b10 stream off\n");
	}
	return ret;
}

static int os04b10_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int vb = 0;
	unsigned int vb_init = 0;
	unsigned int vts_init = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		sclk = OS04B10_SUPPORT_SCLK_FPS_25;
		max_fps = SENSOR_OUTPUT_MAX_FPS;
		vb_init = OS04B10_VB_25_FPS;
		vts_init = OS04B10_VTS_25_FPS;
		break;
	case TX_SENSOR_MAX_FPS_20:
		sclk = OS04B10_SUPPORT_SCLK_FPS_20;
		max_fps = TX_SENSOR_MAX_FPS_20;
		vb_init = OS04B10_VB_20_FPS;
		vts_init = OS04B10_VTS_20_FPS;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_WARNING("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	/* get hts */
	ret = os04b10_write(sd, 0xfd, 0x01);
	ret += os04b10_read(sd, 0x8c, &val);
	hts = val<<8;
	ret += os04b10_read(sd, 0x8d, &val);
	hts |= val;
#if 0
	/* get vb old */
	ret += os04b10_read(sd, 0x05, &val);
	vb = val<<8;
	ret += os04b10_read(sd, 0x06, &val);
	vb |= val;
	/* get vts old */
	ret += os04b10_read(sd, 0x4e, &val);
	vts = val<<8;
	ret += os04b10_read(sd, 0x4f, &val);
	vts |= val;
#endif
	if (0 != ret) {
		ISP_ERROR("err: os04b10 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	vb = vts - vts_init + vb_init;

	ret += os04b10_write(sd, 0xfd, 0x01);
	ret += os04b10_write(sd, 0x05, (vb >> 8) & 0xff);
	ret += os04b10_write(sd, 0x06, vb & 0xff);
	ret += os04b10_write(sd, 0x01, 0x01);
	if (0 != ret) {
		ISP_ERROR("err: %s os04b10_write err\n",__func__);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 21;
	sensor->video.attr->integration_time_limit = vts - 21;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 21;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int os04b10_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		if (sensor_max_fps == TX_SENSOR_MAX_FPS_25)
			wsize = &os04b10_win_sizes[0];
		else if (sensor_max_fps == TX_SENSOR_MAX_FPS_20)
			wsize = &os04b10_win_sizes[1];
		else
			ISP_ERROR("Now os04b10 Do not support this resolution.\n");
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		if (sensor_max_fps == TX_SENSOR_MAX_FPS_25)
			wsize = &os04b10_win_sizes[0];
		else if (sensor_max_fps == TX_SENSOR_MAX_FPS_20)
			wsize = &os04b10_win_sizes[1];
		else
			ISP_ERROR("Now os04b10 Do not support this resolution.\n");
	}

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

static int os04b10_g_chip_ident(struct tx_isp_subdev *sd,
				struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"os04b10_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"os04b10_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = os04b10_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an os04b10 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("os04b10 chip found @ 0x%02x (%s) version %s \n", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "os04b10", sizeof("os04b10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int os04b10_st(struct tx_isp_subdev *sd, int enable)
{
	return 0;
}

static int os04b10_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0x01;

	ret = os04b10_write(sd, 0xfd, 0x01);

	val &= 0xfc;
	val |= enable;
	if(enable & 0x1)
		val &= 0xfe;
	else
		val |= 0x1;

	switch(enable){
	case 0:
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGBRG10_1X10;
		break;
	case 1:
		sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		break;
	case 2:
		sensor->video.mbus.code = V4L2_MBUS_FMT_SRGGB10_1X10;
		break;
	case 3:
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGRBG10_1X10;
		break;
	default:
		ISP_ERROR("Sensor Can Not Support This HV flip mode!!!\n");
	}

	sensor->video.mbus_change = 1;
	ret += os04b10_write(sd, 0x3f, val);
	ret += os04b10_write(sd, 0x01, 0x01);

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int os04b10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = os04b10_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = os04b10_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = os04b10_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = os04b10_set_analog_gain_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = os04b10_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = os04b10_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = os04b10_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(arg)
			ret = os04b10_write_array(sd, os04b10_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(arg)
			ret = os04b10_write_array(sd, os04b10_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = os04b10_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = os04b10_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = os04b10_st(sd, *(int*)arg);
	default:
		break;
	}

	return 0;
}

static int os04b10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = os04b10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int os04b10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	os04b10_write(sd, reg->reg & 0xff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops os04b10_core_ops = {
	.g_chip_ident = os04b10_g_chip_ident,
	.reset = os04b10_reset,
	.init = os04b10_init,
	/*.ioctl = os04b10_ops_ioctl,*/
	.g_register = os04b10_g_register,
	.s_register = os04b10_s_register,
};

static struct tx_isp_subdev_video_ops os04b10_video_ops = {
	.s_stream = os04b10_s_stream,
};

static struct tx_isp_subdev_sensor_ops	os04b10_sensor_ops = {
	.ioctl	= os04b10_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops os04b10_ops = {
	.core = &os04b10_core_ops,
	.video = &os04b10_video_ops,
	.sensor = &os04b10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "os04b10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int os04b10_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &os04b10_win_sizes[0];
	unsigned long rate = 0;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	rate = clk_get_rate(clk_get_parent(sensor->mclk));
#if 0
	if (((rate / 1000) % 27000) != 0) {
		struct clk *vpll;
		vpll = clk_get(NULL,"vpll");
		if (IS_ERR(vpll)) {
			pr_err("get vpll failed\n");
		} else {
			rate = clk_get_rate(vpll);
			if (((rate / 1000) % 27000) != 0) {
				clk_set_rate(vpll,1080000000);
			}
			ret = clk_set_parent(sensor->mclk, vpll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}
#endif
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		wsize = &os04b10_win_sizes[0];
		break;
	case TX_SENSOR_MAX_FPS_20:
		wsize = &os04b10_win_sizes[1];
		os04b10_attr.max_integration_time_native = 0xa5b - 21;
		os04b10_attr.integration_time_limit = 0xa5b - 21;
		os04b10_attr.total_width = 1584;
		os04b10_attr.total_height = 0xa5b;
		os04b10_attr.max_integration_time = 0xa5b - 21;
		os04b10_attr.one_line_expr_in_us = 19;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	os04b10_attr.max_again = 259142;
	os04b10_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &os04b10_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &os04b10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->os04b10\n");
	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int os04b10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os04b10_id[] = {
	{ "os04b10", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, os04b10_id);

static struct i2c_driver os04b10_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "os04b10",
	},
	.probe		= os04b10_probe,
	.remove		= os04b10_remove,
	.id_table	= os04b10_id,
};

static __init int init_os04b10(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init os04b10 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&os04b10_driver);
}

static __exit void exit_os04b10(void)
{
	private_i2c_del_driver(&os04b10_driver);
}

module_init(init_os04b10);
module_exit(exit_os04b10);

MODULE_DESCRIPTION("A low-level driver for OmniVision os04b10 sensors");
MODULE_LICENSE("GPL");
