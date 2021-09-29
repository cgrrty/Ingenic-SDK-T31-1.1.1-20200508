/*
 * sc3235.c
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
#include <txx-funcs.h>

#define SC3235_CHIP_ID_H	(0xcc)
#define SC3235_CHIP_ID_L	(0x05)
#define sc3235_REG_END		0xffff
#define sc3235_REG_DELAY	0xfffe
#define sc3235_SUPPORT_30FPS_SCLK (97200000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20190902a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_HIGH_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

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

struct again_lut sc3235_again_lut[] = {
	{0x320, 0},
	{0x321, 2886},
	{0x322, 5776},
	{0x323, 8494},
	{0x324, 11136},
	{0x325, 13706},
	{0x326, 16287},
	{0x327, 18723},
	{0x328, 21097},
	{0x329, 23414},
	{0x32a, 25746},
	{0x32b, 27953},
	{0x32c, 30109},
	{0x32d, 32217},
	{0x32e, 34345},
	{0x32f, 36361},
	{0x330, 38336},
	{0x331, 40270},
	{0x332, 42226},
	{0x333, 44082},
	{0x334, 45904},
	{0x335, 47690},
	{0x336, 49500},
	{0x337, 51220},
	{0x338, 52910},
	{0x339, 54571},
	{0x33a, 56254},
	{0x33b, 57857},
	{0x33c, 59433},
	{0x33d, 60984},
	{0x33e, 62558},
	{0x33f, 64059},
	{0x720, 65536},
	{0x721, 68468},
	{0x722, 71267},
	{0x723, 74030},
	{0x724, 76672},
	{0x725, 79283},
	{0x726, 81784},
	{0x727, 84259},
	{0x728, 86633},
	{0x729, 89538},
	{0x72a, 91246},
	{0x72b, 93489},
	{0x72c, 95645},
	{0x72d, 97686},
	{0x72e, 99848},
	{0x72f, 101961},
	{0x730, 103872},
	{0x731, 105744},
	{0x732, 107731},
	{0x733, 109678},
	{0x734, 111440},
	{0x735, 113169},
	{0x736, 115008},
	{0x737, 116811},
	{0x738, 118446},
	{0x739, 120053},
	{0x73a, 121764},
	{0x73b, 123444},
	{0x73c, 124969},
	{0x73d, 126470},
	{0x73e, 128070},
	{0x73f, 129643},
	{0xf20, 131072},
	{0xf21, 134095},
	{0xf22, 136803},
	{0xf23, 139652},
	{0xf24, 142208},
	{0xf25, 144900},
	{0xf26, 147320},
	{0xf27, 149873},
	{0xf28, 152169},
	{0xf29, 154596},
	{0xf2a, 156782},
	{0xf2b, 159095},
	{0xf2c, 161181},
	{0xf2d, 163390},
	{0xf2e, 165384},
	{0xf2f, 167497},
	{0xf30, 169408},
	{0xf31, 171434},
	{0xf32, 173267},
	{0xf33, 175214},
	{0xf34, 176976},
	{0xf35, 178848},
	{0xf36, 180544},
	{0xf37, 182347},
	{0xf38, 183982},
	{0xf39, 185722},
	{0xf3a, 187300},
	{0xf3b, 188980},
	{0xf3c, 190505},
	{0xf3d, 192130},
	{0xf3e, 193606},
	{0xf3f, 195179},
	{0x1f20, 196608},
	{0x1f21, 199517},
	{0x1f22, 202339},
	{0x1f23, 205080},
	{0x1f24, 207744},
	{0x1f25, 210334},
	{0x1f26, 212856},
	{0x1f27, 215312},
	{0x1f28, 217705},
	{0x1f29, 220040},
	{0x1f2a, 222318},
	{0x1f2b, 224543},
	{0x1f2c, 226717},
	{0x1f2d, 228842},
	{0x1f2e, 230920},
	{0x1f2f, 232953},
	{0x1f30, 234944},
	{0x1f31, 236893},
	{0x1f32, 238803},
	{0x1f33, 240676},
	{0x1f34, 242512},
	{0x1f35, 244313},
	{0x1f36, 246080},
	{0x1f37, 247815},
	{0x1f38, 249518},
	{0x1f39, 251192},
	{0x1f3a, 252836},
	{0x1f3b, 254452},
	{0x1f3c, 256041},
	{0x1f3d, 257604},
	{0x1f3e, 259142},
	{0x1f3f, 260655},
};

struct tx_isp_sensor_attribute sc3235_attr;

unsigned int sc3235_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc3235_again_lut;
	while(lut->gain <= sc3235_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc3235_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc3235_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sc3235_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 486,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 2304,
	.image_theight = 1296,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sc3235_attr={
	.name = "sc3235",
	.chip_id = 0x223820,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
		.dvp_hcomp_en = 0,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 260655,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1620 - 4,
	.integration_time_limit = 1620 - 4,
	.total_width = 2400,
	.total_height = 1620,
	.max_integration_time = 1620 - 4,
	.one_line_expr_in_us = 25,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc3235_alloc_again,
	.sensor_ctrl.alloc_dgain = sc3235_alloc_dgain,
};


static struct regval_list sc3235_init_regs_1920_1080_25fps_mipi[] = {
#if 0 //mclk = 27Mhz ok
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x301f, 0x01},
	{0x3038, 0x66},
	{0x3253, 0x08},
	{0x3301, 0x11},
	{0x3304, 0x30},
	{0x3306, 0x50},
	{0x330a, 0x00},
	{0x330b, 0xd0},
	{0x330e, 0x30},
	{0x3314, 0x94},
	{0x331c, 0x01},
	{0x331e, 0x29},
	{0x3320, 0x03},
	{0x3347, 0x05},
	{0x334c, 0x10},
	{0x3356, 0x01},
	{0x3364, 0x17},
	{0x3367, 0x10},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3390, 0x08},
	{0x3391, 0x38},
	{0x3392, 0x38},
	{0x3393, 0x1a},
	{0x3394, 0x88},
	{0x3395, 0x88},
	{0x360f, 0x05},
	{0x3614, 0x80},
	{0x3622, 0xf6},
	{0x3630, 0xc3},
	{0x3631, 0x8a},
	{0x3632, 0x18},
	{0x3633, 0x44},
	{0x3635, 0x20},
	{0x3637, 0x2c},
	{0x3638, 0x28},
	{0x363a, 0xa8},
	{0x363b, 0x20},
	{0x363c, 0x06},
	{0x3641, 0x00},
	{0x3670, 0x0a},
	{0x3671, 0xf6},
	{0x3672, 0x76},
	{0x3673, 0x16},
	{0x3674, 0xa0},
	{0x3675, 0x98},
	{0x3676, 0x6a},
	{0x367a, 0x08},
	{0x367b, 0x38},
	{0x367c, 0x08},
	{0x367d, 0x38},
	{0x3690, 0x64},
	{0x3691, 0x63},
	{0x3692, 0x64},
	{0x369c, 0x08},
	{0x369d, 0x38},
	{0x36e9, 0x07},
	{0x36eb, 0x05},
	{0x36ec, 0x15},
	{0x36ed, 0x04},
	{0x36f9, 0x33},
	{0x36fb, 0x23},
	{0x36fc, 0x01},
	{0x36fd, 0x14},
	{0x3900, 0x29},
	{0x3902, 0xc5},
	{0x3905, 0xd1},
	{0x3906, 0x62},
	{0x3908, 0x41},
	{0x3909, 0x00},
	{0x390a, 0x19},
	{0x390b, 0x00},
	{0x390c, 0x4c},
	{0x390d, 0x00},
	{0x390e, 0x19},
	{0x390f, 0x00},
	{0x3910, 0x4c},
	{0x391d, 0x04},
	{0x391e, 0x00},
	{0x3920, 0x00},
	{0x3921, 0x4c},
	{0x3922, 0x00},
	{0x3923, 0x19},
	{0x3924, 0x00},
	{0x3925, 0x4c},
	{0x3926, 0x00},
	{0x3927, 0x19},
	{0x3933, 0x0a},
	{0x3934, 0x28},
	{0x3935, 0x18},
	{0x3936, 0x08},
	{0x3937, 0x13},
	{0x3940, 0x68},
	{0x3942, 0x02},
	{0x3943, 0x33},
	{0x3e01, 0xa8},
	{0x3e02, 0x40},
	{0x3e09, 0x20},
	{0x3e1b, 0x35},
	{0x3e25, 0x03},
	{0x3e26, 0x20},
	{0x5781, 0x04},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x02},
	{0x5785, 0x40},
	{0x5786, 0x20},
	{0x5787, 0x18},
	{0x5788, 0x10},
	{0x5789, 0x10},
	{0x578a, 0x30},
	{0x57a4, 0xa0},
//	{sc3235_REG_DELAY, 0x0a},
	{0x0100, 0x01},
#endif

	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x301f, 0x26},
	{0x3038, 0x66},
	{0x3253, 0x08},
	{0x3301, 0x11},
	{0x3304, 0x30},
	{0x3306, 0x50},
	{0x330a, 0x00},
	{0x330b, 0xd0},
	{0x330e, 0x30},
	{0x3314, 0x94},
	{0x331c, 0x01},
	{0x331e, 0x29},
	{0x3320, 0x03},
	{0x3347, 0x05},
	{0x334c, 0x10},
	{0x3356, 0x01},
	{0x3364, 0x17},
	{0x3367, 0x10},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3390, 0x08},
	{0x3391, 0x38},
	{0x3392, 0x38},
	{0x3393, 0x1a},
	{0x3394, 0x88},
	{0x3395, 0x88},
	{0x360f, 0x05},
	{0x3614, 0x80},
	{0x3622, 0xf6},
	{0x3630, 0xc3},
	{0x3631, 0x8a},
	{0x3632, 0x18},
	{0x3633, 0x44},
	{0x3635, 0x20},
	{0x3637, 0x2c},
	{0x3638, 0x28},
	{0x363a, 0xa8},
	{0x363b, 0x20},
	{0x363c, 0x06},
	{0x3641, 0x00},
	{0x3670, 0x0a},
	{0x3671, 0xf6},
	{0x3672, 0x76},
	{0x3673, 0x16},
	{0x3674, 0xa0},
	{0x3675, 0x98},
	{0x3676, 0x6a},
	{0x367a, 0x08},
	{0x367b, 0x38},
	{0x367c, 0x08},
	{0x367d, 0x38},
	{0x3690, 0x64},
	{0x3691, 0x63},
	{0x3692, 0x64},
	{0x369c, 0x08},
	{0x369d, 0x38},
	{0x36e9, 0x23},
	{0x36ea, 0x25},
	{0x36eb, 0x05},
	{0x36ec, 0x15},
	{0x36ed, 0x04},
	{0x36f9, 0x33},
	{0x36fa, 0x25},
	{0x36fb, 0x23},
	{0x36fc, 0x01},
	{0x36fd, 0x04},
	{0x3900, 0x29},
	{0x3902, 0xc5},
	{0x3905, 0xd1},
	{0x3906, 0x62},
	{0x3908, 0x41},
	{0x3909, 0x00},
	{0x390a, 0x19},
	{0x390b, 0x00},
	{0x390c, 0x4c},
	{0x390d, 0x00},
	{0x390e, 0x19},
	{0x390f, 0x00},
	{0x3910, 0x4c},
	{0x391d, 0x04},
	{0x391e, 0x00},
	{0x3920, 0x00},
	{0x3921, 0x4c},
	{0x3922, 0x00},
	{0x3923, 0x19},
	{0x3924, 0x00},
	{0x3925, 0x4c},
	{0x3926, 0x00},
	{0x3927, 0x19},
	{0x3933, 0x0a},
	{0x3934, 0x28},
	{0x3935, 0x18},
	{0x3936, 0x08},
	{0x3937, 0x13},
	{0x3940, 0x68},
	{0x3942, 0x02},
	{0x3943, 0x33},
	{0x3e01, 0xa8},
	{0x3e02, 0x40},
	{0x3e09, 0x20},
	{0x3e1b, 0x35},
	{0x3e25, 0x03},
	{0x3e26, 0x20},
	{0x5781, 0x04},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x02},
	{0x5785, 0x40},
	{0x5786, 0x20},
	{0x5787, 0x18},
	{0x5788, 0x10},
	{0x5789, 0x10},
	{0x578a, 0x30},
	{0x57a4, 0xa0},
	{0x320e, 0x06},
	{0x320f, 0x54},
	{0x0100, 0x01},
	{sc3235_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3235_init_regs_1920_1080_25fps_dvp[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3641, 0x0c},
	{0x36e9, 0x07},
	{0x36eb, 0x05},
	{0x36ec, 0x15},
	{0x36ed, 0x04},
	{0x36f9, 0x33},
	{0x36fb, 0x23},
	{0x36fc, 0x01},
	{0x36fd, 0x14},
	{0x3641, 0x00},
	{0x3e09, 0x20},
	{0x3637, 0x2c},
	{0x3630, 0x83},
	{0x3635, 0x10},
	{0x363b, 0x10},
	{0x363c, 0x07},
	{0x3306, 0x58},
	{0x330a, 0x00},
	{0x330b, 0xd8},
	{0x3638, 0x28},
	{0x331c, 0x01},
	{0x3304, 0x30},
	{0x331e, 0x29},
	{0x3320, 0x03},
	{0x3356, 0x01},
	{0x57a4, 0xa0},
	{0x5781, 0x04},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x02},
	{0x5785, 0x40},
	{0x5786, 0x20},
	{0x5787, 0x18},
	{0x5788, 0x10},
	{0x5789, 0x10},
	{0x578a, 0x30},
	{0x3908, 0x41},
	{0x3622, 0xf6},
	{0x3e25, 0x03},
	{0x3e26, 0x20},
	{0x3902, 0xc5},
	{0x3905, 0x99},
	{0x3314, 0x94},
	{0x3347, 0x05},
	{0x3301, 0x80},
	{0x3630, 0xc3},
	{0x3633, 0x42},
	{0x363a, 0xa8},
	{0x3614, 0x80},
	{0x3632, 0x18},
	{0x3631, 0x8a},
	{0x3e01, 0x62},
	{0x363b, 0x20},
	{0x3635, 0x20},
	{0x3314, 0x94},
	{0x330e, 0x30},
	{0x3367, 0x10},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x334c, 0x10},
	{0x3633, 0x44},
	{0x3622, 0xf6},
	{0x3301, 0x11},
	{0x3630, 0xc3},
	{0x3632, 0x18},
	{0x3306, 0x50},
	{0x330b, 0xd0},
	{0x360f, 0x05},
	{0x367a, 0x08},
	{0x367b, 0x38},
	{0x3671, 0xf6},
	{0x3672, 0x76},
	{0x3673, 0x16},
	{0x3670, 0x08},
	{0x369c, 0x08},
	{0x369d, 0x38},
	{0x3690, 0x64},
	{0x3691, 0x63},
	{0x3692, 0x64},
	{0x3670, 0x0a},
	{0x367c, 0x08},
	{0x367d, 0x38},
	{0x3674, 0xa0},
	{0x3675, 0x98},
	{0x3676, 0x6a},
	{0x3364, 0x17},
	{0x3301, 0x11},
	{0x3393, 0x1a},
	{0x3394, 0x88},
	{0x3395, 0x88},
	{0x3390, 0x08},
	{0x3391, 0x38},
	{0x3392, 0x38},
	{0x301f, 0x22},
	{0x301f, 0x2a},
	{0x3933, 0x0a},
	{0x3934, 0x28},
	{0x3942, 0x02},
	{0x3943, 0x33},
	{0x3940, 0x68},
	{0x3e1b, 0x35},
	{0x3038, 0x66},
	{0x363c, 0x06},
	{0x3253, 0x08},
	{0x391d, 0x04},
	{0x391e, 0x00},
	{0x3905, 0xd1},
	{0x3905, 0xd1},
	{0x3909, 0x00},
	{0x390a, 0x19},
	{0x390d, 0x00},
	{0x390e, 0x19},
	{0x390b, 0x00},
	{0x390c, 0x4c},
	{0x390f, 0x00},
	{0x3910, 0x4c},
	{0x3920, 0x00},
	{0x3921, 0x4c},
	{0x3924, 0x00},
	{0x3925, 0x4c},
	{0x3922, 0x00},
	{0x3923, 0x19},
	{0x3926, 0x00},
	{0x3927, 0x19},
	{0x3900, 0x29},
	{0x3937, 0x13},
	{0x3906, 0x62},
	{0x3935, 0x18},
	{0x3936, 0x08},
	{0x391e, 0x00},
	{0x3e01, 0xa8},
	{0x3e02, 0x40},
	{0x3632, 0x18},
	{0x3306, 0x50},
	{0x330b, 0xd0},
	{0x3200, 0x00},
	{0x3201, 0x80},
	{0x3204, 0x08},
	{0x3205, 0x8f},
	{0x3208, 0x08},
	{0x3209, 0x00},
	{0x36e9, 0x07},
	{0x36ea, 0x3a},
	{0x36eb, 0x05},
	{0x36ec, 0x15},
	{0x36ed, 0x04},
	{0x36f9, 0x33},
	{0x36fa, 0x3a},
	{0x36fb, 0x23},
	{0x36fc, 0x01},
	{0x36fd, 0x14},
	{0x3000, 0x0f},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x301c, 0x94},
	{0x3018, 0x0f},
	{0x301a, 0xf8},
	{0x4603, 0x01},
	{0x303f, 0x81},
	{0x300a, 0x24},
	{0x3641, 0x02},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3204, 0x09},
	{0x3205, 0x0f},
	{0x3206, 0x05},
	{0x3207, 0x1f},
	{0x3208, 0x09},
	{0x3209, 0x00},
	{0x320a, 0x05},
	{0x320b, 0x10},
	{0x3210, 0x00},
	{0x3211, 0x08},
	{0x3212, 0x00},
	{0x3213, 0x08},
	{0x320c, 0x09},
	{0x320d, 0x60},
	{0x320e, 0x06},
	{0x320f, 0x54},
	{0x0100, 0x01}, // sclk 97.2M; count_clck 388.8M

	{sc3235_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc3235_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc3235_init_regs_1920_1080_25fps_mipi,
	},
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc3235_init_regs_1920_1080_25fps_dvp,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sc3235_win_sizes[0];

static enum v4l2_mbus_pixelcode sc3235_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sc3235_stream_on_dvp[] = {
	{0x0100, 0x01},
	{sc3235_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3235_stream_off_dvp[] = {
	{0x0100, 0x00},
	{sc3235_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3235_stream_on_mipi[] = {
	{0x0100, 0x01},
	{sc3235_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3235_stream_off_mipi[] = {
	{0x0100, 0x00},
	{sc3235_REG_END, 0x00},	/* END MARKER */
};

int sc3235_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
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

int sc3235_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sc3235_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != sc3235_REG_END) {
		if (vals->reg_num == sc3235_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc3235_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
static int sc3235_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != sc3235_REG_END) {
		if (vals->reg_num == sc3235_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc3235_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc3235_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc3235_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc3235_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC3235_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc3235_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC3235_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc3235_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	sc3235_write(sd,0x3812,0x00);
	ret += sc3235_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc3235_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc3235_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	if (it < 0x250) {
		ret += sc3235_write(sd, 0x3314, 0x14);
	} else if(it > 0x450){
		ret += sc3235_write(sd, 0x3314, 0x04);
	}
	if (again < 0x720) { //<2
		sc3235_write(sd,0x3632,0x18);
		sc3235_write(sd,0x3306,0x50);
		sc3235_write(sd,0x330b,0xd0);
	} else if (again >= 0x720 && again < 0x1f20){//>=2 <8
		sc3235_write(sd,0x3632,0x58);
		sc3235_write(sd,0x3306,0x58);
		sc3235_write(sd,0x330b,0xd8);
	} else { ////>=8x
		sc3235_write(sd,0x3632,0xd8);
		sc3235_write(sd,0x3306,0x5c);
		sc3235_write(sd,0x330b,0xdb);
	}

	ret = sc3235_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sc3235_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));
	sc3235_write(sd,0x3812,0x30);

	if (ret < 0)
		return ret;

	return 0;
}

static int sc3235_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret += sc3235_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc3235_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc3235_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (value < 0x250) {
		ret += sc3235_write(sd, 0x3314, 0x14);
	} else if(value > 0x450){
		ret += sc3235_write(sd, 0x3314, 0x04);
	}

	if (ret < 0)
		return ret;

	return 0;
}

static int sc3235_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	/* denoise logic */
	sc3235_write(sd,0x3812,0x00);
	if (value < 0x720) { //<2
		sc3235_write(sd,0x3632,0x18);
		sc3235_write(sd,0x3306,0x50);
		sc3235_write(sd,0x330b,0xd0);
	} else if (value >= 0x720 && value < 0x1f20){//>=2 <8
		sc3235_write(sd,0x3632,0x58);
		sc3235_write(sd,0x3306,0x58);
		sc3235_write(sd,0x330b,0xd8);
	} else { ////>=8x
		sc3235_write(sd,0x3632,0xd8);
		sc3235_write(sd,0x3306,0x5c);
		sc3235_write(sd,0x330b,0xdb);
	}
	sc3235_write(sd,0x3812,0x30);

	ret = sc3235_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc3235_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc3235_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc3235_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc3235_init(struct tx_isp_subdev *sd, int enable)
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

	ret = sc3235_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc3235_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc3235_write_array(sd, sc3235_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3235_write_array(sd, sc3235_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc3235 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc3235_write_array(sd, sc3235_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3235_write_array(sd, sc3235_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc3235 stream off\n");
	}

	return ret;
}

static int sc3235_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	sclk = sc3235_SUPPORT_30FPS_SCLK;

	ret = sc3235_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc3235_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc3235 read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp);
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

//	ret = sc3235_write(sd,0x3812,0x00);
	ret += sc3235_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc3235_write(sd, 0x320e, (unsigned char)(vts >> 8));
//	ret += sc3235_write(sd,0x3812,0x30);
	if (0 != ret) {
		ISP_ERROR("err: sc3235_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc3235_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc3235_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc3235_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(35);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(35);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc3235_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc3235_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc3235 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc3235 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc3235", sizeof("sc3235"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sc3235_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc3235_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
//		if(arg)
//			ret = sc3235_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if(arg)
//			ret = sc3235_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc3235_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc3235_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc3235_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc3235_write_array(sd, sc3235_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3235_write_array(sd, sc3235_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc3235_write_array(sd, sc3235_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3235_write_array(sd, sc3235_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc3235_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sc3235_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc3235_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc3235_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc3235_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc3235_core_ops = {
	.g_chip_ident = sc3235_g_chip_ident,
	.reset = sc3235_reset,
	.init = sc3235_init,
	/*.ioctl = sc3235_ops_ioctl,*/
	.g_register = sc3235_g_register,
	.s_register = sc3235_s_register,
};

static struct tx_isp_subdev_video_ops sc3235_video_ops = {
	.s_stream = sc3235_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc3235_sensor_ops = {
	.ioctl	= sc3235_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc3235_ops = {
	.core = &sc3235_core_ops,
	.video = &sc3235_video_ops,
	.sensor = &sc3235_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc3235",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc3235_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	unsigned long rate = 0;
	int ret;

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

	if((data_interface == TX_SENSOR_DATA_INTERFACE_DVP)){
		rate = clk_get_rate(clk_get_parent(sensor->mclk));
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
		private_clk_set_rate(sensor->mclk, 27000000);
	} else {
		private_clk_set_rate(sensor->mclk, 24000000);
	}
	private_clk_enable(sensor->mclk);

	if((data_interface == TX_SENSOR_DATA_INTERFACE_DVP)){
		wsize = &sc3235_win_sizes[1];
		sc3235_attr.dvp.gpio = sensor_gpio_func;
		ret = set_sensor_gpio_function(sensor_gpio_func);
		if (ret < 0)
			goto err_set_sensor_gpio;
		/* sc3235_attr.max_integration_time_native = 1498; */
		/* sc3235_attr.integration_time_limit = 1498; */
		/* sc3235_attr.total_width = 2400; */
		/* sc3235_attr.total_height = 1500; */
		/* sc3235_attr.max_integration_time = 1498; */
	} else if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI)){
		wsize = &sc3235_win_sizes[0];
		memcpy((void*)(&(sc3235_attr.mipi)),(void*)(&sc3235_mipi),sizeof(sc3235_mipi));
	} else {
		ISP_ERROR("Can not support this data interface!!!\n");
		goto err_set_sensor_data_interface;
	}

	sc3235_attr.dbus_type = data_interface;

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sc3235_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc3235_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc3235\n");

	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sc3235_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc3235_id[] = {
	{ "sc3235", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc3235_id);

static struct i2c_driver sc3235_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc3235",
	},
	.probe		= sc3235_probe,
	.remove		= sc3235_remove,
	.id_table	= sc3235_id,
};

static __init int init_sc3235(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc3235 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc3235_driver);
}

static __exit void exit_sc3235(void)
{
	private_i2c_del_driver(&sc3235_driver);
}

module_init(init_sc3235);
module_exit(exit_sc3235);

MODULE_DESCRIPTION("A low-level driver for OmniVision sc3235 sensors");
MODULE_LICENSE("GPL");
