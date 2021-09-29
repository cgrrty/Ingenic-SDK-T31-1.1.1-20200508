/*
 * ov5648.c
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

#define OV5648_CHIP_ID_H	(0x56)
#define OV5648_CHIP_ID_L	(0x48)
#define OV5648_REG_END		0xffff
#define OV5648_REG_DELAY	0xfffe

#define OV5648_SUPPORT_SCLK (84000000)
#define SENSOR_OUTPUT_MAX_FPS 15
#define SENSOR_OUTPUT_MIN_FPS 10
#define SENSOR_VERSION	"H20180820a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_HIGH_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_15;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

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
struct again_lut ov5648_again_lut[] = {
	{0x10, 0},
	{0x11, 5731},
	{0x12, 11136},
	{0x13, 16247},
	{0x14, 21097},
	{0x15, 25710},
	{0x16, 30108},
	{0x17, 34311},
	{0x18, 38335},
	{0x19, 42195},
	{0x1a, 45903},
	{0x1b, 49471},
	{0x1c, 52910},
	{0x1d, 56227},
	{0x1e, 59433},
	{0x1f, 62533},
	{0x20, 65535},
	{0x21, 68444},
	{0x22, 71266},
	{0x23, 74007},
	{0x24, 76671},
	{0x25, 79261},
	{0x26, 81782},
	{0x27, 84238},
	{0x28, 86632},
	{0x29, 88967},
	{0x2a, 91245},
	{0x2b, 93470},
	{0x2c, 95643},
	{0x2d, 97768},
	{0x2e, 99846},
	{0x2f, 101879},
	{0x30, 103870},
	{0x31, 105820},
	{0x32, 107730},
	{0x33, 109602},
	{0x34, 111438},
	{0x35, 113239},
	{0x36, 115006},
	{0x37, 116741},
	{0x38, 118445},
	{0x39, 120118},
	{0x3a, 121762},
	{0x3b, 123379},
	{0x3c, 124968},
	{0x3d, 126530},
	{0x3e, 128068},
	{0x3f, 129581},
	{0x40, 131070},
	{0x41, 132535},
	{0x42, 133979},
	{0x43, 135401},
	{0x44, 136801},
	{0x45, 138182},
	{0x46, 139542},
	{0x47, 140883},
	{0x48, 142206},
	{0x49, 143510},
	{0x4a, 144796},
	{0x4b, 146065},
	{0x4c, 147317},
	{0x4d, 148553},
	{0x4e, 149773},
	{0x4f, 150978},
	{0x50, 152167},
	{0x51, 153342},
	{0x52, 154502},
	{0x53, 155648},
	{0x54, 156780},
	{0x55, 157899},
	{0x56, 159005},
	{0x57, 160098},
	{0x58, 161178},
	{0x59, 162247},
	{0x5a, 163303},
	{0x5b, 164348},
	{0x5c, 165381},
	{0x5d, 166403},
	{0x5e, 167414},
	{0x5f, 168415},
	{0x60, 169405},
	{0x61, 170385},
	{0x62, 171355},
	{0x63, 172314},
	{0x64, 173265},
	{0x65, 174205},
	{0x66, 175137},
	{0x67, 176059},
	{0x68, 176973},
	{0x69, 177878},
	{0x6a, 178774},
	{0x6b, 179662},
	{0x6c, 180541},
	{0x6d, 181412},
	{0x6e, 182276},
	{0x6f, 183132},
	{0x70, 183980},
	{0x71, 184820},
	{0x72, 185653},
	{0x73, 186479},
	{0x74, 187297},
	{0x75, 188109},
	{0x76, 188914},
	{0x77, 189711},
	{0x78, 190503},
	{0x79, 191287},
	{0x7a, 192065},
	{0x7b, 192837},
	{0x7c, 193603},
	{0x7d, 194362},
	{0x7e, 195116},
	{0x7f, 195863},
	{0x80, 196605},
	{0x81, 197340},
	{0x82, 198070},
	{0x83, 198795},
	{0x84, 199514},
	{0x85, 200227},
	{0x86, 200936},
	{0x87, 201639},
	{0x88, 202336},
	{0x89, 203029},
	{0x8a, 203717},
	{0x8b, 204399},
	{0x8c, 205077},
	{0x8d, 205750},
	{0x8e, 206418},
	{0x8f, 207082},
	{0x90, 207741},
	{0x91, 208395},
	{0x92, 209045},
	{0x93, 209690},
	{0x94, 210331},
	{0x95, 210968},
	{0x96, 211600},
	{0x97, 212228},
	{0x98, 212852},
	{0x99, 213472},
	{0x9a, 214088},
	{0x9b, 214700},
	{0x9c, 215308},
	{0x9d, 215912},
	{0x9e, 216513},
	{0x9f, 217109},
	{0xa0, 217702},
	{0xa1, 218291},
	{0xa2, 218877},
	{0xa3, 219458},
	{0xa4, 220037},
	{0xa5, 220611},
	{0xa6, 221183},
	{0xa7, 221751},
	{0xa8, 222315},
	{0xa9, 222876},
	{0xaa, 223434},
	{0xab, 223988},
	{0xac, 224540},
	{0xad, 225088},
	{0xae, 225633},
	{0xaf, 226175},
	{0xb0, 226713},
	{0xb1, 227249},
	{0xb2, 227782},
	{0xb3, 228311},
	{0xb4, 228838},
	{0xb5, 229362},
	{0xb6, 229883},
	{0xb7, 230401},
	{0xb8, 230916},
	{0xb9, 231429},
	{0xba, 231938},
	{0xbb, 232445},
	{0xbc, 232949},
	{0xbd, 233451},
	{0xbe, 233950},
	{0xbf, 234446},
	{0xc0, 234940},
	{0xc1, 235431},
	{0xc2, 235920},
	{0xc3, 236406},
	{0xc4, 236890},
	{0xc5, 237371},
	{0xc6, 237849},
	{0xc7, 238326},
	{0xc8, 238800},
	{0xc9, 239271},
	{0xca, 239740},
	{0xcb, 240207},
	{0xcc, 240672},
	{0xcd, 241134},
	{0xce, 241594},
	{0xcf, 242052},
	{0xd0, 242508},
	{0xd1, 242961},
	{0xd2, 243413},
	{0xd3, 243862},
	{0xd4, 244309},
	{0xd5, 244754},
	{0xd6, 245197},
	{0xd7, 245637},
	{0xd8, 246076},
	{0xd9, 246513},
	{0xda, 246947},
	{0xdb, 247380},
	{0xdc, 247811},
	{0xdd, 248240},
	{0xde, 248667},
	{0xdf, 249091},
	{0xe0, 249515},
	{0xe1, 249936},
	{0xe2, 250355},
	{0xe3, 250772},
	{0xe4, 251188},
	{0xe5, 251602},
	{0xe6, 252014},
	{0xe7, 252424},
	{0xe8, 252832},
	{0xe9, 253239},
	{0xea, 253644},
	{0xeb, 254047},
	{0xec, 254449},
	{0xed, 254848},
	{0xee, 255246},
	{0xef, 255643},
	{0xf0, 256038},
	{0xf1, 256431},
	{0xf2, 256822},
	{0xf3, 257212},
	{0xf4, 257600},
	{0xf5, 257987},
	{0xf6, 258372},
	{0xf7, 258756},
	{0xf8, 259138},
	{0xf9, 259518},
	{0xfa, 259897},
	{0xfb, 260275},
	{0xfc, 260651},
	{0xfd, 261025},
	{0xfe, 261398},
	{0xff, 261769}
};

struct tx_isp_sensor_attribute ov5648_attr;

unsigned int ov5648_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = ov5648_again_lut;

	while(lut->gain <= ov5648_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == ov5648_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int ov5648_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute ov5648_attr={
	.name = "ov5648",
	.chip_id = 0x5648,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.clk = 600,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.image_twidth = 2592,
		.image_theight = 1944,
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
	.max_again = 261769,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0x7c0 - 4,
	.integration_time_limit = 0x7c0 - 4,
	.total_width = 0xb00,
	.total_height = 0x7c0,
	.max_integration_time = 0x7c0 - 4,
	.integration_time_apply_delay = 1,
	.again_apply_delay = 1,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = ov5648_alloc_again,
	.sensor_ctrl.alloc_dgain = ov5648_alloc_dgain,
};

static struct regval_list ov5648_init_regs_2592_1944_15fps[] = {
	{0x0103, 0x01},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3011, 0x02},
	{0x3017, 0x05},
	{0x3018, 0x4c},
	{0x301c, 0xd2},
	{0x3022, 0x00},
	{0x3034, 0x1a},
	{0x3035, 0x21},
	{0x3036, 0x69},
	{0x3037, 0x03},
	{0x3038, 0x00},
	{0x3039, 0x00},
	{0x303a, 0x00},
	{0x303b, 0x19},
	{0x303c, 0x11},
	{0x303d, 0x30},
	{0x3105, 0x11},
	{0x3106, 0x05},
	{0x3304, 0x28},
	{0x3305, 0x41},
	{0x3306, 0x30},
	{0x3308, 0x00},
	{0x3309, 0xc8},
	{0x330a, 0x01},
	{0x330b, 0x90},
	{0x330c, 0x02},
	{0x330d, 0x58},
	{0x330e, 0x03},
	{0x330f, 0x20},
	{0x3300, 0x00},
	{0x3500, 0x00},
	{0x3501, 0x7b},
	{0x3502, 0x00},
	{0x3503, 0x07},
	{0x350a, 0x00},
	{0x350b, 0x40},
	{0x3601, 0x33},
	{0x3602, 0x00},
	{0x3611, 0x0e},
	{0x3612, 0x2b},
	{0x3614, 0x50},
	{0x3620, 0x33},
	{0x3622, 0x00},
	{0x3630, 0xad},
	{0x3631, 0x00},
	{0x3632, 0x94},
	{0x3633, 0x17},
	{0x3634, 0x14},
	{0x3704, 0xc0},
	{0x3705, 0x2a},
	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370b, 0x23},
	{0x370c, 0xcc},
	{0x370d, 0x00},
	{0x370e, 0x00},
	{0x371c, 0x07},
	{0x3739, 0xd2},
	{0x373c, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0xa3},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
	{0x380b, 0x98},
	{0x380c, 0x0b},
	{0x380d, 0x00},
	{0x380e, 0x07},
	{0x380f, 0xc0},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3817, 0x00},
	{0x3820, 0x40},
	{0x3821, 0x06},
	{0x3826, 0x03},
	{0x3829, 0x00},
	{0x382b, 0x0b},
	{0x3830, 0x00},
	{0x3836, 0x00},
	{0x3837, 0x00},
	{0x3838, 0x00},
	{0x3839, 0x04},
	{0x383a, 0x00},
	{0x383b, 0x01},
	{0x3b00, 0x00},
	{0x3b02, 0x08},
	{0x3b03, 0x00},
	{0x3b04, 0x04},
	{0x3b05, 0x00},
	{0x3b06, 0x04},
	{0x3b07, 0x08},
	{0x3b08, 0x00},
	{0x3b09, 0x02},
	{0x3b0a, 0x04},
	{0x3b0b, 0x00},
	{0x3b0c, 0x3d},
	{0x3f01, 0x0d},
	{0x3f0f, 0xf5},
	{0x4000, 0x89},
	{0x4001, 0x02},
	{0x4002, 0x45},
	{0x4004, 0x04},
	{0x4005, 0x18},
	{0x4006, 0x08},
	{0x4007, 0x10},
	{0x4008, 0x00},
	{0x4050, 0x6e},
	{0x4051, 0x8f},
	{0x4300, 0xf8},
	{0x4303, 0xff},
	{0x4304, 0x00},
	{0x4307, 0xff},
	{0x4520, 0x00},
	{0x4521, 0x00},
	{0x4511, 0x22},
	{0x4801, 0x0f},
	{0x4802, 0x84},//add 0817
	{0x4814, 0x2a},
	{0x4819, 0xa0},// add 0817
	{0x481f, 0x3c},
	{0x4823, 0x3c},
	{0x4826, 0x00},
	{0x481b, 0x3c},
	{0x4837, 0x18},
	{0x4b00, 0x06},
	{0x4b01, 0x0a},
	{0x4b04, 0x10},
	{0x5000, 0xff},
	{0x5001, 0x00},
	{0x5002, 0x41},
	{0x5003, 0x0a},
	{0x5004, 0x00},
	{0x5043, 0x00},
	{0x5013, 0x00},
	{0x501f, 0x03},
	{0x503d, 0x00},
	{0x5780, 0xfc},
	{0x5781, 0x1f},
	{0x5782, 0x03},
	{0x5786, 0x20},
	{0x5787, 0x40},
	{0x5788, 0x08},
	{0x5789, 0x08},
	{0x578a, 0x02},
	{0x578b, 0x01},
	{0x578c, 0x01},
	{0x578d, 0x0c},
	{0x578e, 0x02},
	{0x578f, 0x01},
	{0x5790, 0x01},
	{0x5a00, 0x08},
	{0x5b00, 0x01},
	{0x5b01, 0x40},
	{0x5b02, 0x00},
	{0x5b03, 0xf0},
	{0x0100, 0x00},

	{OV5648_REG_END, 0x00},/* END MARKER */
};
/*
 * the order of the ov5648_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting ov5648_win_sizes[] = {
	/* 1920*1080 @15fps*/
	{
		.width		= 2592,
		.height		= 1944,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov5648_init_regs_2592_1944_15fps,
	}
};

/*
 * the part of driver was fixed.
 */

static struct regval_list ov5648_stream_on[] = {
	{0x0100, 0x01},
	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_stream_off[] = {
	{0x0100, 0x00},
	{OV5648_REG_END, 0x00},	/* END MARKER */
};

int ov5648_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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

int ov5648_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
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

static int ov5648_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OV5648_REG_END) {
		if (vals->reg_num == OV5648_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = ov5648_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
static int ov5648_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != OV5648_REG_END) {
		if (vals->reg_num == OV5648_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov5648_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int ov5648_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int ov5648_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = ov5648_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV5648_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = ov5648_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV5648_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int ov5648_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value << 4;

	ret = ov5648_write(sd, 0x3208, 0x00);
	ret += ov5648_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += ov5648_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	ret += ov5648_write(sd, 0x3500, (unsigned char)((expo >> 16) & 0xff));
	ret += ov5648_write(sd, 0x3208, 0x10);
	ret += ov5648_write(sd, 0x3208, 0xa0);
	if (ret < 0)
		return ret;
	return 0;
}

static int ov5648_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = ov5648_write(sd, 0x3208, 0x01);
	ret += ov5648_write(sd, 0x350b, (unsigned char)((value & 0xff)));
	ret += ov5648_write(sd, 0x350a, (unsigned char)((value >> 8) & 0x03));
	ret += ov5648_write(sd, 0x3208, 0x11);
	ret += ov5648_write(sd, 0x3208, 0xa1);
	if (ret < 0)
		return ret;

	return 0;
}

static int ov5648_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov5648_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov5648_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	wsize = &ov5648_win_sizes[0];
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = ov5648_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int ov5648_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = ov5648_write_array(sd, ov5648_stream_on);
		pr_debug("ov5648 stream on\n");
	}
	else {
		ret = ov5648_write_array(sd, ov5648_stream_off);
		pr_debug("ov5648 stream off\n");
	}
	return ret;
}

static int ov5648_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = OV5648_SUPPORT_SCLK;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	ret += ov5648_read(sd, 0x380c, &val);
	hts = val<<8;
	val = 0;
	ret += ov5648_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		printk("err: ov5648 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = ov5648_write(sd, 0x3208, 0x02);
	ret += ov5648_write(sd, 0x380f, vts & 0xff);
	ret += ov5648_write(sd, 0x380e, (vts >> 8) & 0xff);
	ret += ov5648_write(sd, 0x3208, 0x12);
	ret += ov5648_write(sd, 0x3208, 0xa2);
	if (0 != ret) {
		printk("err: ov5648_write err\n");
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

static int ov5648_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &ov5648_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &ov5648_win_sizes[0];
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

static int ov5648_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	ret += ov5648_read(sd, 0x3820, &val);
	if (enable){
		val = val | 0x06;
	} else {
		val = val & 0xf9;
	}
	ret = ov5648_write(sd, 0x3208, 0x03);
	ret += ov5648_write(sd, 0x3820, val);
	ret += ov5648_write(sd, 0x3208, 0x13);
	ret += ov5648_write(sd, 0x3208, 0xa3);

	sensor->video.mbus_change = 0;
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int ov5648_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"ov5648_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(15);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"ov5648_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = ov5648_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an ov5648 chip.\n",
		       client->addr, client->adapter->name);
		return ret;
	}
	printk("ov5648 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "ov5648", sizeof("ov5648"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int ov5648_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = ov5648_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = ov5648_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = ov5648_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = ov5648_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = ov5648_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(arg)
			ret = ov5648_write_array(sd, ov5648_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(arg)
			ret = ov5648_write_array(sd, ov5648_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = ov5648_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = ov5648_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;;
	}
	return 0;
}

static int ov5648_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = ov5648_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov5648_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov5648_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops ov5648_core_ops = {
	.g_chip_ident = ov5648_g_chip_ident,
	.reset = ov5648_reset,
	.init = ov5648_init,
	/*.ioctl = ov5648_ops_ioctl,*/
	.g_register = ov5648_g_register,
	.s_register = ov5648_s_register,
};

static struct tx_isp_subdev_video_ops ov5648_video_ops = {
	.s_stream = ov5648_s_stream,
};

static struct tx_isp_subdev_sensor_ops	ov5648_sensor_ops = {
	.ioctl	= ov5648_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops ov5648_ops = {
	.core = &ov5648_core_ops,
	.video = &ov5648_video_ops,
	.sensor = &ov5648_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "ov5648",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int ov5648_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = NULL;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	wsize = &ov5648_win_sizes[0];
	ov5648_attr.max_again = 261769;
	ov5648_attr.max_dgain = 0;

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &ov5648_attr;
	sensor->video.mbus_change = 1;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &ov5648_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->ov5648\n");
	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int ov5648_remove(struct i2c_client *client)
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

static const struct i2c_device_id ov5648_id[] = {
	{ "ov5648", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5648_id);

static struct i2c_driver ov5648_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov5648",
	},
	.probe		= ov5648_probe,
	.remove		= ov5648_remove,
	.id_table	= ov5648_id,
};

static __init int init_ov5648(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to init ov5648 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&ov5648_driver);
}

static __exit void exit_ov5648(void)
{
	private_i2c_del_driver(&ov5648_driver);
}

module_init(init_ov5648);
module_exit(exit_ov5648);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov5648 sensors");
MODULE_LICENSE("GPL");
