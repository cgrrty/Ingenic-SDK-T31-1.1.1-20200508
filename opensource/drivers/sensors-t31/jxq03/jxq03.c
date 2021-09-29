/*
 * jxq03.c
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

#define JXQ03_CHIP_ID_H	(0x05)
#define JXQ03_CHIP_ID_L	(0x07)
#define JXQ03_REG_END		0xff
#define JXQ03_REG_DELAY	0xfe
#define JXQ03_SUPPORT_30FPS_SCLK (143910000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20200418a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int wdr_bufsize = 737280;//cache lines corrponding on VPB1
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

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

struct again_lut jxq03_again_lut[] = {
	{0x0,  0 },
	{0x1,  5731 },
	{0x2,  11136},
	{0x3,  16248},
	{0x4,  21097},
	{0x5,  25710},
	{0x6,  30109},
	{0x7,  34312},
	{0x8,  38336},
	{0x9,  42195},
	{0xa,  45904},
	{0xb,  49472},
	{0xc,  52910},
	{0xd,  56228},
	{0xe,  59433},
	{0xf,  62534},
	{0x10,  65536},
	{0x11,	71267},
	{0x12,	76672},
	{0x13,	81784},
	{0x14,	86633},
	{0x15,	91246},
	{0x16,	95645},
	{0x17,	99848},
	{0x18,  103872},
	{0x19,	107731},
	{0x1a,	111440},
	{0x1b,	115008},
	{0x1c,	118446},
	{0x1d,	121764},
	{0x1e,	124969},
	{0x1f,	128070},
	{0x20,	131072},
	{0x21,	136803},
	{0x22,	142208},
	{0x23,	147320},
	{0x24,	152169},
	{0x25,	156782},
	{0x26,	161181},
	{0x27,	165384},
	{0x28,	169408},
	{0x29,	173267},
	{0x2a,	176976},
	{0x2b,	180544},
	{0x2c,	183982},
	{0x2d,	187300},
	{0x2e,	190505},
	{0x2f,	193606},
	{0x30,	196608},
	{0x31,	202339},
	{0x32,	207744},
	{0x33,	212856},
	{0x34,	217705},
	{0x35,	222318},
	{0x36,	226717},
	{0x37,	230920},
	{0x38,	234944},
	{0x39,	238803},
	{0x3a,	242512},
	{0x3b,	246080},
	{0x3c,	249518},
	{0x3d,	252836},
	{0x3e,	256041},
	{0x3f,	259142},
	{0x40,	262144},
	{0x41,	267875},
	{0x42,	273280},
	{0x43,	278392},
	{0x44,	283241},
	{0x45,	287854},
	{0x46,	292253},
	{0x47,	296456},
	{0x48,	300480},
	{0x49,	304339},
	{0x4a,	308048},
	{0x4b,	311616},
	{0x4c,	315054},
	{0x4d,	318372},
	{0x4e,	321577},
	{0x4f,	324678},
};

struct tx_isp_sensor_attribute jxq03_attr;

unsigned int jxq03_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxq03_again_lut;
	while(lut->gain <= jxq03_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else{
			if((lut->gain == jxq03_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int jxq03_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	return 0;
}

unsigned int jxq03_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

static unsigned int long_it = 0;
unsigned int jxq03_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;

	isp_it = expo << shift;
	*sensor_it = expo;
	long_it = expo;

	return isp_it;
}

unsigned int jxq03_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;

	if (long_it % 2 == 0)
		expo = expo / 2 * 2;
	if (long_it % 2){
		expo = (expo % 2) ? expo : (expo - 1);
	}
	if(expo < jxq03_attr.min_integration_time)
		expo = (long_it % 2)?1:2;

	*sensor_it = expo;
	isp_it = expo << shift;

	return isp_it;
}

struct tx_isp_mipi_bus jxq03_mipi_dol={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 360,
	.lans = 2,
	.settle_time_apative_en = 1,
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
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_mipi_bus jxq03_mipi_linear={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 720,
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

struct tx_isp_sensor_attribute jxq03_attr={
	.name = "jxq03",
	.chip_id = 0x507,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x40,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 720,
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
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 259142,
	.max_again_short = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_short = 1,
	.max_integration_time_short = 156,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1599 - 4,
	.integration_time_limit = 1599 - 4,
	.total_width = 3600,
	.total_height = 1599,
	.max_integration_time = 1599 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.one_line_expr_in_us = 25,
	.sensor_ctrl.alloc_again_short = jxq03_alloc_again_short,
	.sensor_ctrl.alloc_again = jxq03_alloc_again,
	.sensor_ctrl.alloc_dgain = jxq03_alloc_dgain,
	.sensor_ctrl.alloc_integration_time = jxq03_alloc_integration_time,
	.sensor_ctrl.alloc_integration_time_short = jxq03_alloc_integration_time_short,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list jxq03_init_regs_3m_25fps_mipi_wdr[] = {
	{0x12, 0x48},
	{0x48, 0x96},
	{0x48, 0x16},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x3C},
	{0x11, 0x80},
	{0x0D, 0xF2},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x1A},
	{0x57, 0x60},
	{0x20, 0x84},
	{0x21, 0x03},
	{0x22, 0x6A},
	{0x23, 0x0A},
	{0x24, 0x40},
	{0x25, 0x10},
	{0x26, 0x52},
	{0x27, 0x54},
	{0x28, 0x25},
	{0x29, 0x03},
	{0x2A, 0x4E},
	{0x2B, 0x13},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x49},
	{0x2F, 0x44},
	{0x41, 0x84},
	{0x42, 0x34},
	{0x47, 0x42},
	{0x76, 0x40},
	{0x77, 0x0B},
	{0x80, 0x00},
	{0xAF, 0x22},
	{0xAB, 0x00},
	{0x46, 0x04},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x6E, 0x2C},
	{0x70, 0xDD},
	{0x71, 0xD3},
	{0x72, 0xD5},
	{0x73, 0x59},
	{0x74, 0x02},
	{0x78, 0x8B},
	{0x89, 0x81},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x31, 0x0A},
	{0x32, 0x21},
	{0x33, 0x5C},
	{0x34, 0x34},
	{0x35, 0x30},
	{0x3A, 0xA0},
	{0x3B, 0x00},
	{0x3C, 0x50},
	{0x3D, 0x5B},
	{0x3E, 0xFF},
	{0x3F, 0x48},
	{0x40, 0xFF},
	{0x56, 0xB2},
	{0x59, 0x34},
	{0x5A, 0x04},
	{0x85, 0x2C},
	{0x8A, 0x04},
	{0x91, 0x08},
	{0xA9, 0x08},
	{0x9C, 0xE1},
	{0x5B, 0xAC},
	{0x5C, 0x81},
	{0x5D, 0xEF},
	{0x5E, 0x14},
	{0x64, 0xE0},
	{0x66, 0x40},
	{0x67, 0x77},
	{0x68, 0x00},
	{0x69, 0x40},
	{0x7A, 0x70},
	{0x8F, 0x91},
	{0x9D, 0x70},
	{0xAE, 0x30},
	{0x13, 0x81},
	{0x96, 0x04},
	{0x4A, 0xE5},
	{0x7E, 0xCD},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7B, 0x4A},
	{0x7C, 0x0F},
	{0x7F, 0x56},
	{0x62, 0x21},
	{0x90, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0xBB, 0x11},
	{0x6A, 0x12},
	{0x65, 0x32},
	{0x82, 0x00},
	{0x81, 0x74},
	{0x19, 0x20},
	{0x07, 0x03},
	{0x1B, 0x4F},
	{0x06, 0x50},
	{0x03, 0xFF},
	{0x04, 0xFF},
	{0x12, 0x08},
	{0x48, 0x96},
	{0x48, 0x16},
	{JXQ03_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxq03_init_regs_3m_25fps_mipi[] = {
	{0x12, 0x40},
	{0x48, 0x96},
	{0x48, 0x16},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x3C},
	{0x11, 0x80},
	{0x0D, 0xF2},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x1A},
	{0x57, 0x60},
	{0x20, 0x84},
	{0x21, 0x03},
	{0x22, 0x3f},
	{0x23, 0x06},
	{0x24, 0x40},
	{0x25, 0x10},
	{0x26, 0x52},
	{0x27, 0x54},
	{0x28, 0x15},
	{0x29, 0x03},
	{0x2A, 0x4E},
	{0x2B, 0x13},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x4A},
	{0x2F, 0x44},
	{0x41, 0x84},
	{0x42, 0x14},
	{0x47, 0x42},
	{0x76, 0x40},
	{0x77, 0x0B},
	{0x80, 0x00},
	{0xAF, 0x22},
	{0xAB, 0x00},
	{0x46, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x6E, 0x2C},
	{0x70, 0xDD},
	{0x71, 0xD3},
	{0x72, 0xD5},
	{0x73, 0x59},
	{0x74, 0x02},
	{0x78, 0x8B},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x31, 0x0A},
	{0x32, 0x21},
	{0x33, 0x5C},
	{0x34, 0x34},
	{0x35, 0x30},
	{0x3A, 0xA0},
	{0x3B, 0x00},
	{0x3C, 0x50},
	{0x3D, 0x5B},
	{0x3E, 0xFF},
	{0x3F, 0x48},
	{0x40, 0xFF},
	{0x56, 0xB2},
	{0x59, 0x34},
	{0x5A, 0x04},
	{0x85, 0x2C},
	{0x8A, 0x04},
	{0x91, 0x08},
	{0xA9, 0x08},
	{0x9C, 0xE1},
	{0x5B, 0xAC},
	{0x5C, 0x81},
	{0x5D, 0xEF},
	{0x5E, 0x14},
	{0x64, 0xE0},
	{0x66, 0x04},
	{0x67, 0x77},
	{0x68, 0x00},
	{0x69, 0x41},
	{0x7A, 0xA0},
	{0x8F, 0x91},
	{0x9D, 0x70},
	{0xAE, 0x30},
	{0x13, 0x81},
	{0x96, 0x04},
	{0x4A, 0xE5},
	{0x7E, 0xCD},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7B, 0x4A},
	{0x7C, 0x0F},
	{0x7F, 0x56},
	{0x62, 0x21},
	{0x90, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0xBB, 0x11},
	{0x6A, 0x12},
	{0x65, 0x32},
	{0x82, 0x00},
	{0x81, 0x74},
	{0x19, 0x20},
	{0x12, 0x00},
	{0x48, 0x96},
	{0x48, 0x16},
	{JXQ03_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxq03_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxq03_win_sizes[] = {
	/* 2304*1296 */
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxq03_init_regs_3m_25fps_mipi,
	},
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxq03_init_regs_3m_25fps_mipi_wdr,
	}
};
struct tx_isp_sensor_win_setting *wsize = &jxq03_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list jxq03_stream_on_mipi[] = {

	{JXQ03_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxq03_stream_off_mipi[] = {
	{JXQ03_REG_END, 0x00},	/* END MARKER */
};

int jxq03_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int jxq03_write(struct tx_isp_subdev *sd, unsigned char reg,
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

static int jxq03_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != JXQ03_REG_END) {
		if (vals->reg_num == JXQ03_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxq03_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
static int jxq03_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != JXQ03_REG_END) {
		if (vals->reg_num == JXQ03_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxq03_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int jxq03_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int jxq03_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = jxq03_read(sd, 0x0a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != JXQ03_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxq03_read(sd, 0x0b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != JXQ03_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int jxq03_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = jxq03_write(sd,  0x01, (unsigned char)(value & 0xff));
	ret += jxq03_write(sd, 0x02, (unsigned char)((value >> 8) & 0xff));

	return 0;
}

static int jxq03_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = jxq03_write(sd,  0x05, (unsigned char)(value & 0xff));
	ret = jxq03_write(sd,  0x08, (unsigned char)((value >> 8) & 0x1));
	if (ret < 0)
		return ret;

	return 0;
}

static int jxq03_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned char val = 0;

	ret += jxq03_write(sd, 0x00, (unsigned char)(value & 0x7f));
	if(value < 0x10) {
		ret += jxq03_write(sd, 0xc0, 0x2f);
		ret += jxq03_write(sd, 0xc1, 0x64);
		ret += jxq03_write(sd, 0xc2, 0x0c);
		ret += jxq03_write(sd, 0xc3, 0x40);
		ret += jxq03_write(sd, 0xc4, 0x82);
		ret += jxq03_write(sd, 0xc5, 0x02);
	} else {
		ret += jxq03_write(sd, 0xc0, 0x2f);
		ret += jxq03_write(sd, 0xc1, 0x44);
		ret += jxq03_write(sd, 0xc2, 0x0c);
		ret += jxq03_write(sd, 0xc3, 0x00);
		ret += jxq03_write(sd, 0xc4, 0x82);
		ret += jxq03_write(sd, 0xc5, 0x00);
	}
	ret += jxq03_read(sd, 0x1f, &val);
	val |= (1 << 7);
	ret += jxq03_write(sd, 0x1f, val);

	if (ret < 0)
		return ret;

	return 0;
}

static int jxq03_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxq03_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxq03_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxq03_init(struct tx_isp_subdev *sd, int enable)
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

	ret = jxq03_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int jxq03_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = jxq03_write_array(sd, jxq03_stream_on_mipi);
		ISP_WARNING("jxq03 stream on\n");

	} else {
		ret = jxq03_write_array(sd, jxq03_stream_off_mipi);
		ISP_WARNING("jxq03 stream off\n");
	}

	return ret;
}

static int jxq03_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;

	sclk = JXQ03_SUPPORT_30FPS_SCLK;
	max_fps = SENSOR_OUTPUT_MAX_FPS;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret += jxq03_read(sd, 0x21, &val);
	hts = val<<8;
	val = 0;
	ret += jxq03_read(sd, 0x20, &val);
	hts |= val;
	hts *= 4;
	if (0 != ret) {
		ISP_ERROR("err: jxq03 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	jxq03_write(sd, 0xc0, 0x22);
	jxq03_write(sd, 0xc1, (unsigned char)(vts & 0xff));
	jxq03_write(sd, 0xc2, 0x23);
	jxq03_write(sd, 0xc3, (unsigned char)(vts >> 8));
	ret = jxq03_read(sd, 0x1f, &val);
//	pr_debug("before register 0x1f value : 0x%02x\n", val);
	if(ret < 0)
		return -1;
	val |= (1 << 7); //set bit[7],  register group write function,  auto clean
	jxq03_write(sd, 0x1f, val);
//	pr_debug("after register 0x1f value : 0x%02x\n", val);

	if (0 != ret) {
		ISP_ERROR("err: jxq03_write err\n");
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

static int jxq03_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;

	ret = jxq03_write(sd, 0x12, 0x80);
	private_msleep(5);

	ret = jxq03_write_array(sd, wsize->regs);
	ret = jxq03_write_array(sd, jxq03_stream_on_mipi);
	ret = jxq03_write(sd, 0x00, 0x00);

	return 0;
}

static int jxq03_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;

	ret = jxq03_write(sd, 0x12, 0x40);
	if(wdr_en == 1){
		memcpy((void*)(&(jxq03_attr.mipi)),(void*)(&jxq03_mipi_dol),sizeof(jxq03_mipi_dol));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		wsize = &jxq03_win_sizes[1];
		jxq03_attr.one_line_expr_in_us = 25;
		jxq03_attr.data_type = data_type;
		jxq03_attr.wdr_cache = wdr_bufsize;
		jxq03_attr.min_integration_time_short = 1;
		jxq03_attr.max_integration_time_short = 156;
		jxq03_attr.min_integration_time = 2;
		jxq03_attr.min_integration_time_native = 2;
		jxq03_attr.max_integration_time_native = 2503;
		jxq03_attr.integration_time_limit = 2503;
		jxq03_attr.total_width = 3600;
		jxq03_attr.total_height = 2666;
		jxq03_attr.max_integration_time = 2503;

		sensor->video.attr = &jxq03_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0){
		memcpy((void*)(&(jxq03_attr.mipi)),(void*)(&jxq03_mipi_linear),sizeof(jxq03_mipi_linear));
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		jxq03_attr.data_type = data_type;
		wsize = &jxq03_win_sizes[0];

		jxq03_attr.one_line_expr_in_us = 25;
		jxq03_attr.min_integration_time = 2;
		jxq03_attr.min_integration_time_native = 2;
		jxq03_attr.max_integration_time_native = 1599 - 4;
		jxq03_attr.integration_time_limit = 1599 - 4;
		jxq03_attr.total_width = 3600;
		jxq03_attr.total_height = 1599;
		jxq03_attr.max_integration_time = 1599 - 4;

		sensor->video.attr = &jxq03_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int jxq03_set_mode(struct tx_isp_subdev *sd, int value)
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

static int jxq03_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"jxq03_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"jxq03_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = jxq03_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxq03 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("jxq03 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "jxq03", sizeof("jxq03"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int jxq03_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = jxq03_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = jxq03_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = jxq03_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = jxq03_set_analog_gain_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = jxq03_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = jxq03_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = jxq03_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = jxq03_write_array(sd, jxq03_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = jxq03_write_array(sd, jxq03_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = jxq03_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = jxq03_set_wdr(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = jxq03_set_wdr_stop(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int jxq03_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = jxq03_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int jxq03_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxq03_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops jxq03_core_ops = {
	.g_chip_ident = jxq03_g_chip_ident,
	.reset = jxq03_reset,
	.init = jxq03_init,
	/*.ioctl = jxq03_ops_ioctl,*/
	.g_register = jxq03_g_register,
	.s_register = jxq03_s_register,
};

static struct tx_isp_subdev_video_ops jxq03_video_ops = {
	.s_stream = jxq03_s_stream,
};

static struct tx_isp_subdev_sensor_ops	jxq03_sensor_ops = {
	.ioctl	= jxq03_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops jxq03_ops = {
	.core = &jxq03_core_ops,
	.video = &jxq03_video_ops,
	.sensor = &jxq03_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "jxq03",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int jxq03_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

	if(data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		wsize = &jxq03_win_sizes[0];
		memcpy((void*)(&(jxq03_attr.mipi)),(void*)(&jxq03_mipi_linear),sizeof(jxq03_mipi_linear));

		jxq03_attr.one_line_expr_in_us = 25;
		jxq03_attr.min_integration_time = 2;
		jxq03_attr.min_integration_time_native = 2;
		jxq03_attr.max_integration_time_native = 1599 - 4;
		jxq03_attr.integration_time_limit = 1599 - 4;
		jxq03_attr.total_width = 3600;
		jxq03_attr.total_height = 1599;
		jxq03_attr.max_integration_time = 1599 - 4;
	} else if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		wsize = &jxq03_win_sizes[1];
		memcpy((void*)(&(jxq03_attr.mipi)),(void*)(&jxq03_mipi_dol),sizeof(jxq03_mipi_dol));
		jxq03_attr.one_line_expr_in_us = 25;
		jxq03_attr.wdr_cache = wdr_bufsize;
		jxq03_attr.min_integration_time_short = 1;
		jxq03_attr.max_integration_time_short = 156;
		jxq03_attr.min_integration_time = 2;
		jxq03_attr.min_integration_time_native = 2;
		jxq03_attr.max_integration_time_native = 2503;
		jxq03_attr.integration_time_limit = 2503;
		jxq03_attr.total_width = 3600;
		jxq03_attr.total_height = 2666;
		jxq03_attr.max_integration_time = 2503;
	} else {
		ISP_ERROR("Can not support this data type!!!\n");
	}

	jxq03_attr.data_type = data_type;
	jxq03_attr.dbus_type = data_interface;

	/*
	  convert sensor-gain into isp-gain,
	*/
	jxq03_attr.max_again = 259142;
	jxq03_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &jxq03_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &jxq03_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->jxq03\n");

	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int jxq03_remove(struct i2c_client *client)
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

static const struct i2c_device_id jxq03_id[] = {
	{ "jxq03", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jxq03_id);

static struct i2c_driver jxq03_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "jxq03",
	},
	.probe		= jxq03_probe,
	.remove		= jxq03_remove,
	.id_table	= jxq03_id,
};

static __init int init_jxq03(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init jxq03 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&jxq03_driver);
}

static __exit void exit_jxq03(void)
{
	private_i2c_del_driver(&jxq03_driver);
}

module_init(init_jxq03);
module_exit(exit_jxq03);

MODULE_DESCRIPTION("A low-level driver for OmniVision jxq03 sensors");
MODULE_LICENSE("GPL");
