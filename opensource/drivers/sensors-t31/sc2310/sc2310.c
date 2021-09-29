/*
 * sc2310.c
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

#define SC2310_CHIP_ID_H	(0x23)
#define SC2310_CHIP_ID_L	(0x11)
#define SC2310_REG_END		0xffff
#define SC2310_REG_DELAY	0xfffe
#define SC2310_SUPPORT_SCLK (81000000)
#define SC2310_SUPPORT_SCLK_15FPS (81000000)

#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20181031a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_12BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

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

struct again_lut sc2310_again_lut[] = {
	{0x340, 0},
	{0x342, 2886},
	{0x344, 5776},
	{0x346, 8494},
	{0x348, 11136},
	{0x34a, 13706},
	{0x34c, 16287},
	{0x34e, 18723},
	{0x350, 21097},
	{0x352, 23414},
	{0x354, 25746},
	{0x356, 27953},
	{0x358, 30109},
	{0x35a, 32217},
	{0x35c, 34345},
	{0x35e, 36361},
	{0x360, 38336},
	{0x362, 40270},
	{0x364, 42226},
	{0x366, 44082},
	{0x368, 45904},
	{0x36a, 47690},
	{0x36c, 49500},
	{0x36e, 51220},
	{0x370, 52910},
	{0x372, 54571},
	{0x374, 56254},
	{0x376, 57857},
	{0x378, 59433},
	{0x37a, 60984},
	{0x37c, 62558},
	{0x37e, 64059},
	{0x740, 65536},
	{0x742, 68468},
	{0x744, 71267},
	{0x746, 74030},
	{0x748, 76672},
	{0x74a, 79283},
	{0x74c, 81784},
	{0x74e, 84259},
	{0x750, 86633},
	{0x752, 88986},
	{0x754, 91246},
	{0x756, 93489},
	{0x2341, 96091},
	{0x2343, 98956},
	{0x2345, 101736},
	{0x2347, 104437},
	{0x2349, 107063},
	{0x234b, 109618},
	{0x234d, 112106},
	{0x234f, 114530},
	{0x2351, 116894},
	{0x2353, 119200},
	{0x2355, 121451},
	{0x2357, 123649},
	{0x2359, 125798},
	{0x235b, 127899},
	{0x235d, 129954},
	{0x235f, 131965},
	{0x2361, 133935},
	{0x2363, 135864},
	{0x2365, 137755},
	{0x2367, 139609},
	{0x2369, 141427},
	{0x236b, 143211},
	{0x236d, 144962},
	{0x236f, 146681},
	{0x2371, 148369},
	{0x2373, 150027},
	{0x2375, 151657},
	{0x2377, 153260},
	{0x2379, 154836},
	{0x237b, 156385},
	{0x237d, 157910},
	{0x237f, 159411},
	{0x2741, 161610},
	{0x2743, 164475},
	{0x2745, 167256},
	{0x2747, 169958},
	{0x2749, 172584},
	{0x274b, 175140},
	{0x274d, 177628},
	{0x274f, 180052},
	{0x2751, 182416},
	{0x2753, 184722},
	{0x2755, 186974},
	{0x2757, 189172},
	{0x2759, 191321},
	{0x275b, 193423},
	{0x275d, 195478},
	{0x275f, 197490},
	{0x2761, 199460},
	{0x2763, 201389},
	{0x2765, 203280},
	{0x2767, 205134},
	{0x2769, 206953},
	{0x276b, 208736},
	{0x276d, 210487},
	{0x276f, 212207},
	{0x2771, 213895},
	{0x2773, 215554},
	{0x2775, 217184},
	{0x2777, 218786},
	{0x2779, 220362},
	{0x277b, 221912},
	{0x277d, 223437},
	{0x277f, 224938},
	{0x2f41, 227146},
	{0x2f43, 230011},
	{0x2f45, 232792},
	{0x2f47, 235494},
	{0x2f49, 238120},
	{0x2f4b, 240676},
	{0x2f4d, 243164},
	{0x2f4f, 245588},
	{0x2f51, 247952},
	{0x2f53, 250258},
	{0x2f55, 252510},
	{0x2f57, 254708},
	{0x2f59, 256857},
	{0x2f5b, 258959},
	{0x2f5d, 261014},
	{0x2f5f, 263026},
	{0x2f61, 264996},
	{0x2f63, 266925},
	{0x2f65, 268816},
	{0x2f67, 270670},
	{0x2f69, 272489},
	{0x2f6b, 274273},
	{0x2f6d, 276023},
	{0x2f6f, 277743},
	{0x2f71, 279431},
	{0x2f73, 281090},
	{0x2f75, 282720},
	{0x2f77, 284322},
	{0x2f79, 285898},
	{0x2f7b, 287448},
	{0x2f7d, 288973},
	{0x2f7f, 290474},
	{0x3f41, 292682},
	{0x3f43, 295547},
	{0x3f45, 298328},
	{0x3f47, 301030},
	{0x3f49, 303656},
	{0x3f4b, 306212},
	{0x3f4d, 308700},
	{0x3f4f, 311124},
	{0x3f51, 313488},
	{0x3f53, 315794},
	{0x3f55, 318046},
	{0x3f57, 320244},
	{0x3f59, 322393},
	{0x3f5b, 324495},
	{0x3f5d, 326550},
	{0x3f5f, 328562},
	{0x3f61, 330532},
	{0x3f63, 332461},
	{0x3f65, 334352},
	{0x3f67, 336206},
	{0x3f69, 338025},
	{0x3f6b, 339809},
	{0x3f6d, 341559},
	{0x3f6f, 343279},
	{0x3f71, 344967},
	{0x3f73, 346626},
	{0x3f75, 348256},
	{0x3f77, 349858},
	{0x3f79, 351434},
	{0x3f7b, 352984},
	{0x3f7d, 354509},
	{0x3f7f, 356010},
};

struct tx_isp_sensor_attribute sc2310_attr;

unsigned int sc2310_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc2310_again_lut;
	while(lut->gain <= sc2310_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc2310_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc2310_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc2310_again_lut;
	while(lut->gain <= sc2310_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc2310_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc2310_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sc2310_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 400,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,//RAW12
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sc2310_mipi_wdr = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.settle_time_apative_en = 1,
	.clk = 400,
	.lans = 2,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,//RAW12
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_mipi_bus sc2310_mipi_wdr_25 = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.settle_time_apative_en = 1,
	.clk = 742,
	.lans = 2,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW12
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
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_dvp_bus sc2310_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};

struct tx_isp_sensor_attribute sc2310_attr={
	.name = "sc2310",
	.chip_id = 0x2311,
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
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 356010,
	.max_again_short = 0,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_short = 5,
	.max_integration_time_short = 0x5a0 - 3,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0x5a0 - 3,
	.integration_time_limit = 0x5a0 - 3,
	.total_width = 0x465 * 2,
	.total_height = 0x5a0,
	.max_integration_time = 0x5a0 - 3,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc2310_alloc_again,
	.sensor_ctrl.alloc_again_short = sc2310_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sc2310_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sc2310_init_regs_1920_1080_25fps_mipi_dol[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0xa6},
	{0x36f9, 0x85},
	{0x3001, 0xfe},
	{0x3018, 0x33},
	{0x301c, 0x78},
	{0x3031, 0x0a},
	{0x3037, 0x24},
	{0x3038, 0x44},
	{0x303f, 0x01},
	{0x3200, 0x00},
	{0x3201, 0x04},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3204, 0x07},
	{0x3205, 0x8b},
	{0x3206, 0x04},
	{0x3207, 0x3f},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x320c, 0x04},
	{0x320d, 0x4c},
	{0x320e, 0x08},//0x08
	{0x320f, 0xca},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3220, 0x51},
	{0x3222, 0x29},
	{0x3301, 0x10},
	{0x3302, 0x10},
	{0x3303, 0x30},
	{0x3306, 0x54},
	{0x3308, 0x10},
	{0x3309, 0x48},
	{0x330a, 0x00},
	{0x330b, 0xb4},
	{0x330e, 0x30},
	{0x3314, 0x04},
	{0x331b, 0x83},
	{0x331e, 0x21},
	{0x331f, 0x39},
	{0x3320, 0x01},
	{0x3324, 0x02},
	{0x3325, 0x02},
	{0x3326, 0x00},
	{0x3333, 0x30},
	{0x3334, 0x40},
	{0x333d, 0x08},
	{0x3341, 0x07},
	{0x3343, 0x03},
	{0x3364, 0x1d},
	{0x3366, 0xc0},
	{0x3367, 0x08},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x336c, 0x42},
	{0x337f, 0x03},
	{0x3380, 0x1b},
	{0x33aa, 0x00},
	{0x33b6, 0x07},
	{0x33b7, 0x07},
	{0x33b8, 0x10},
	{0x33b9, 0x10},
	{0x33ba, 0x10},
	{0x33bb, 0x07},
	{0x33bc, 0x07},
	{0x33bd, 0x18},
	{0x33be, 0x18},
	{0x33bf, 0x18},
	{0x33c0, 0x05},
	{0x360f, 0x05},
	{0x3621, 0xac},
	{0x3622, 0xf6},
	{0x3623, 0x18},
	{0x3624, 0x47},
	{0x3625, 0x09},
	{0x3630, 0xc8},
	{0x3631, 0x88},
	{0x3632, 0x18},
	{0x3633, 0x22},
	{0x3634, 0x44},
	{0x3635, 0x20},
	{0x3636, 0x62},
	{0x3637, 0x0c},
	{0x3638, 0x24},
	{0x363a, 0x83},
	{0x363b, 0x08},
	{0x363c, 0x05},
	{0x363d, 0x05},
	{0x3640, 0x00},
	{0x366e, 0x04},
	{0x3670, 0x6a},
	{0x3671, 0xf6},
	{0x3672, 0x16},
	{0x3673, 0x16},
	{0x3674, 0xc8},
	{0x3675, 0x54},
	{0x3676, 0x18},
	{0x3677, 0x22},
	{0x3678, 0x53},
	{0x3679, 0x55},
	{0x367a, 0x40},
	{0x367b, 0x40},
	{0x367c, 0x40},
	{0x367d, 0x58},
	{0x367e, 0x40},
	{0x367f, 0x58},
	{0x3693, 0x20},
	{0x3694, 0x40},
	{0x3695, 0x40},
	{0x3696, 0x9f},
	{0x3697, 0x9f},
	{0x3698, 0x9f},
	{0x369e, 0x40},
	{0x369f, 0x40},
	{0x36a0, 0x58},
	{0x36a1, 0x78},
	{0x36ea, 0x35},
	{0x36eb, 0x0a},
	{0x36ec, 0x0e},
	{0x36fa, 0xa8},
	{0x3802, 0x00},
	{0x3901, 0x02},
	{0x3902, 0xc5},
	{0x3905, 0xd8},
	{0x3907, 0x01},
	{0x3908, 0x01},
	{0x391d, 0x21},
	{0x391e, 0x00},
	{0x391f, 0xc0},
	{0x3933, 0x28},
	{0x3934, 0x0a},
	{0x3940, 0x1b},
	{0x3941, 0x40},
	{0x3942, 0x08},
	{0x3943, 0x0e},
	{0x3e00, 0x01},
	{0x3e01, 0x07},
	{0x3e02, 0xa0},
	{0x3e03, 0x0b},
	{0x3e04, 0x10},
	{0x3e05, 0x80},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e0e, 0x66},
	{0x3e14, 0xb0},
	{0x3e1e, 0x35},
	{0x3e23, 0x00},
	{0x3e24, 0x26},
	{0x3e25, 0x03},
	{0x3e26, 0x40},
	{0x3f00, 0x0d},
	{0x3f04, 0x02},
	{0x3f05, 0x1e},
	{0x3f08, 0x04},
	{0x4500, 0x59},
	{0x4501, 0xa4},
	{0x4509, 0x10},
	{0x4602, 0x0f},
	{0x4603, 0x00},
	{0x4809, 0x01},
	{0x4816, 0x51},
	{0x4837, 0x1a},
	{0x5000, 0x06},
	{0x5780, 0x7f},
	{0x5781, 0x06},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x16},
	{0x5786, 0x12},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x57a0, 0x00},
	{0x57a1, 0x74},
	{0x57a2, 0x01},
	{0x57a3, 0xf4},
	{0x57a4, 0xf0},
	{0x6000, 0x06},
	{0x6002, 0x06},
	{0x36e9, 0x26},
	{0x36f9, 0x05},
	{0x0100, 0x01},

	{SC2310_REG_DELAY, 10},
	{SC2310_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2310_init_regs_1920_1080_15fps_mipi[] = {
	/* {0x0103, 0x01}, */
	/* {0x0100, 0x00}, */
	/* {SC2310_REG_DELAY, 10}, */
	{0x36e9, 0xa3},//bypass pll1	20180830
	{0x36f9, 0x85},//bypass pll2	20180830
	{0x4509, 0x10},
	{0x4500, 0x39},
	{0x3907, 0x00},
	{0x3908, 0x44},
	{0x3633, 0x87},
	{0x3306, 0x7e},
	{0x330b, 0x00},
	{0x3635, 0x4c},
	{0x330e, 0x7a},
	{0x3302, 0x1f}, //3302 need be odd why????  3366	 3302	 3621
	{0x3e01, 0x8c},
	{0x3e02, 0x80},
	{0x3e09, 0x1f},
	{0x3e08, 0x3f},
	{0x3e06, 0x03},
	{0x337f, 0x03}, //new auto precharge  330e in 3372   [7:6] 11: close div_rst 00:open div_rst
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x330e, 0x30},
	{0x3320, 0x06}, // New ramp offset timing
	{0x3326, 0x00},
	{0x331e, 0x11},
	{0x331f, 0x21},
	{0x3303, 0x20},
	{0x3309, 0x30},
	{0x4501, 0xc4},
	{0x3e06, 0x00},
	{0x3e08, 0x03},
	{0x3e09, 0x10},
	{0x3366, 0x7c}, // div_rst gap
	{0x3622, 0x02},
	{0x3633, 0x63},
	{0x3038, 0x88},
	{0x3635, 0x45},
	{0x363b, 0x04},
	{0x363c, 0x06},
	{0x363d, 0x05},
	{0x3633, 0x23},
	{0x3301, 0x10},
	{0x3306, 0x58},
	{0x3622, 0x06},//blksun
	{0x3631, 0x88},
	{0x3630, 0x38},
	{0x3633, 0x22},
	{0x3018, 0x33},//[7:5] lane_num-1
	{0x3031, 0x0c},//[3:0] bitmode
	{0x3037, 0x40},//[6:5] bitsel  40:12bit
	{0x3001, 0xFE},//[0] c_y
	{0x4603, 0x00},//[0] data_fifo mipi mode
	{0x4837, 0x35},//[7:0] pclk period * 2
	{0x36e9, 0x83},
	{0x36eb, 0x0f},
	{0x36ec, 0x1f},
	{0x303f, 0x01},
	{0x330b, 0x20},
	{0x3640, 0x00},
	{0x3308, 0x10},
	{0x3637, 0x0a},
	{0x3e09, 0x20}, //3f for 2x fine gain
	{0x363b, 0x08},
	{0x3637, 0x09}, // ADC range: 14.8k fullwell  blc target : 0.9k	  output fullwell: 13.9k (5fps 27C  linear fullwell is 14.5K)
	{0x3638, 0x14},
	{0x3636, 0x65},
	{0x3907, 0x01},
	{0x3908, 0x01},
	{0x3320, 0x01}, //ramp
	{0x331e, 0x15},
	{0x331f, 0x25},
	{0x3366, 0x80},
	{0x3634, 0x34},
	{0x57a4, 0xf0}, //default c0,
	{0x3635, 0x41}, //fpn
	{0x3e02, 0x30}, //minimum exp 3? debug
	{0x3333, 0x30}, //col fpn  G >br	 ?
	{0x331b, 0x83},
	{0x3334, 0x40},
	{0x3306, 0x6c},
	{0x3638, 0x17},
	{0x330a, 0x01},
	{0x330b, 0x14},
	{0x3302, 0x10},
	{0x3308, 0x08},
	{0x3303, 0x18},
	{0x3309, 0x18},
	{0x331e, 0x11},
	{0x331f, 0x11},
	{0x3f00, 0x0d}, //[2]	hts/2-4
	{0x3f04, 0x02},
	{0x3f05, 0x22},
	{0x3622, 0xe6},
	{0x3633, 0x22},
	{0x3630, 0xc8},
	{0x3301, 0x10},
	{0x36e9, 0xa3},
	{0x36eb, 0x0b},
	{0x36ec, 0x0f},
	{0x3638, 0x27},
	{0x33aa, 0x00}, //power save mode
	{0x3624, 0x02},
	{0x3621, 0xac},
	{0x4509, 0x40},
	{0x391e, 0x00},
	{0x391f, 0xc0},
	{0x3635, 0x45},
	{0x336c, 0x40},
	{0x3621, 0xae},
	{0x3623, 0x08},
	{0x36fa, 0xad}, //charge pump
	{0x3634, 0x44},
	{0x3621, 0xac}, //fifo delay
	{0x4500, 0x59},
	{0x3623, 0x18}, //for more grp rdout setup margin
	{0x3f08, 0x04},
	{0x3f00, 0x0d}, //[2]	hts/2-4-{3f08}
	{0x3f04, 0x02}, //0321
	{0x3f05, 0x1e}, //0321
	{0x336c, 0x42}, //recover read timing
	{0x5000, 0x06},//dpc enable
	{0x5780, 0x7f},//auto blc setting
	{0x57a0, 0x00},	//gain0 = 2x	0x0710ÖÁ0x071f
	{0x57a1, 0x71},
	{0x57a2, 0x01},	//gain1 = 8x	0x1f10ÖÁ0x1f1f
	{0x57a3, 0xf1},
	{0x5781, 0x06},	//white	1x
	{0x5782, 0x04},	//2x
	{0x5783, 0x02},	//8x
	{0x5784, 0x01},	//128x
	{0x5785, 0x16},	//black	1x
	{0x5786, 0x12},	//2x
	{0x5787, 0x08},	//8x
	{0x5788, 0x02},	//128x
	{0x3933, 0x28},
	{0x3934, 0x0a},
	{0x3940, 0x1b},
	{0x3941, 0x40},
	{0x3942, 0x08},
	{0x3943, 0x0e},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x3211, 0x08},
	{0x3213, 0x08},
	{0x36e9, 0xa3},
	{0x36ea, 0x77},
	{0x36eb, 0x0b},
	{0x36ec, 0x0f},
	{0x36ed, 0x03},
	{0x36f9, 0x85},
	{0x36fa, 0x2d},
	{0x36fb, 0x10},
	{0x320c, 0x04},//hts=1125*2=2250
	{0x320d, 0x65},
	{0x320e, 0x12},//vts=2400
	{0x320f, 0x60},
	{0x3235, 0x12},//vts*2-0x02
	{0x3236, 0xbe},
	{0x3f04, 0x02},//{0x320c,0x320d}/2-0x3f08-0x04
	{0x3f05, 0x2a},
	{0x3802, 0x00},
	{0x3624, 0x47},
	{0x3621, 0xac},
	{0x36fa, 0x2f},
	{0x3637, 0x08},
	{0x3638, 0x25},
	{0x3635, 0x40},
	{0x363b, 0x08},
	{0x363c, 0x05},
	{0x363d, 0x05},
	{0x3303, 0x1c},
	{0x3309, 0x1c},
	{0x3324, 0x02}, //falling edge: ramp_offset_en cover ramp_integ_en
	{0x3325, 0x02},
	{0x333d, 0x08}, //count_div_rst_width
	{0x3314, 0x04},
	{0x36fa, 0x28},
	{0x3205, 0x93},
	{0x3e14, 0xb0}, //[7]:1 ana fine gain double 20~3f
	{0x3e1e, 0x35}, //[7]:1 open DCG function in 0x3e03=0x03 [6]:0 DCG >2	[2] 1: dig_fine_gain_en [1:0] max fine gain  01: 3f
	{0x3e0e, 0x66}, //[7:3] fine gain to compsensate 2.4x DCGgain  5 : 2.3125x  6:2.375x  [2]:1 DCG gain between sa1gain 2~4	 [1]:1 dcg gain in 0x3e08[5]
	{0x3364, 0x1d},//[4] fine gain op 1~20--3f 0~10--1f [4] ana dithring en
	{0x33b6, 0x07},//gain0 when dcg off
	{0x33b7, 0x07},//gain1 when dcg off
	{0x33b8, 0x10},//sel0 when dcg off
	{0x33b9, 0x10},//sel1 when dcg off
	{0x33ba, 0x10},//sel2 when dcg off
	{0x33bb, 0x07},//gain0 when dcg on
	{0x33bc, 0x07},//gain1 when dcg on
	{0x33bd, 0x14},//sel0 when dcg on
	{0x33be, 0x14},//sel1 when dcg on
	{0x33bf, 0x14},//sel2 when dcg on
	{0x360f, 0x05},//[0] 3622 auto en
	{0x367a, 0x40},//gain0
	{0x367b, 0x40},//gain1
	{0x3671, 0xf6},//sel0
	{0x3672, 0x16},//sel1
	{0x3673, 0x16},//sel2
	{0x366e, 0x04},//[2] fine gain op 1~20--3f 0~10--1f
	{0x3670, 0x4a},//[1] 3630 auto en, [3] 3633 auto en, [6] 363a auto en
	{0x367c, 0x40},//gain0	3e08[5:2] 1000
	{0x367d, 0x58},//gain1 3e08[5:2] 1011
	{0x3674, 0xc8},//sel0
	{0x3675, 0x54},//sel1
	{0x3676, 0x18},//sel2
	{0x367e, 0x40},//gain0	3e08[5:2] 1000
	{0x367f, 0x58},//gain1	3e08[5:2] 1011
	{0x3677, 0x22},//sel0
	{0x3678, 0x53},//sel1
	{0x3679, 0x55},//sel2
	{0x36a0, 0x58},//gain0	3e08[5:2] 1011
	{0x36a1, 0x78},//gain1	3e08[5:2] 1111
	{0x3696, 0x83},//sel0
	{0x3697, 0x87},//sel1
	{0x3698, 0x9f},//sel2
	{0x4837, 0x31},
	{0x6000, 0x00},
	{0x6002, 0x00},
	{0x301c, 0x78},//close dvp
	{0x3037, 0x44},//[3:0] pump div	range [10M,20M],sclk=81/2=40.5M,div=4-->sclk/4=10.125M,duty cycle-->even number
	{0x3038, 0x44},//[7:4]ppump & [3:0]npump
	{0x3632, 0x18},//[5:4]idac driver
	{0x5785, 0x40},//black	point 1x
	{0x4809, 0x01},//mipi first frame, lp status
	{0x3637, 0x10},
	{0x5000, 0x06},//dpc enable
	{0x5780, 0x7f},//auto blc setting
	{0x57a0, 0x00},	//gain0 = 2x	0x0740ÖÁ0x077f
	{0x57a1, 0x74},
	{0x57a2, 0x01},	//gain1 = 8x	0x1f40ÖÁ0x1f7f
	{0x57a3, 0xf4},
	{0x5781, 0x06},	//white	1x
	{0x5782, 0x04},	//2x
	{0x5783, 0x02},	//8x
	{0x5784, 0x01},	//128x
	{0x5785, 0x16},	//black	1x
	{0x5786, 0x12},	//2x
	{0x5787, 0x08},	//8x
	{0x5788, 0x02},	//128x
	{0x4501, 0xb4},//reduce bit
	{0x3637, 0x20},
	{0x4509, 0x20},//blc quantification	//20181206
	{0x3364, 0x1d},//[4] fine gain op 1~20--3f 0~10--1f [4] ana dithring en
	{0x33b6, 0x07},//gain0 when dcg off	gain<dcg
	{0x33b7, 0x07},//gain1 when dcg off
	{0x33b8, 0x10},//sel0 when dcg off
	{0x33b9, 0x10},//sel1 when dcg off
	{0x33ba, 0x10},//sel2 when dcg off
	{0x33bb, 0x07},//gain0 when dcg on		gain>=dcg
	{0x33bc, 0x07},//gain1 when dcg on
	{0x33bd, 0x20},//sel0 when dcg on
	{0x33be, 0x20},//sel1 when dcg on
	{0x33bf, 0x20},//sel2 when dcg on
	{0x360f, 0x05},//[0] 3622 auto en
	{0x367a, 0x40},//gain0
	{0x367b, 0x40},//gain1
	{0x3671, 0xf6},//sel0
	{0x3672, 0x16},//sel1
	{0x3673, 0x16},//sel2
	{0x366e, 0x04},//[2] fine gain op 1~20--3f 0~10--1f
	{0x3670, 0x4a},//[1] 3630 auto en, [3] 3633 auto en, [6] 363a auto en
	{0x367c, 0x40},//gain0	3e08[5:2] 1000
	{0x367d, 0x58},//gain1 3e08[5:2] 1011
	{0x3674, 0xc8},//sel0
	{0x3675, 0x54},//sel1
	{0x3676, 0x18},//sel2
	{0x367e, 0x40},//gain0	3e08[5:2] 1000
	{0x367f, 0x58},//gain1	3e08[5:2] 1011
	{0x3677, 0x22},//sel0
	{0x3678, 0x33},//sel1
	{0x3679, 0x44},//sel2
	{0x36a0, 0x58},//gain0	3e08[5:2] 1011
	{0x36a1, 0x78},//gain1	3e08[5:2] 1111
	{0x3696, 0x83},//sel0
	{0x3697, 0x87},//sel1
	{0x3698, 0x9f},//sel2
	{0x3637, 0x17},//fullwell 8.6k
	{0x331e, 0x11},
	{0x331f, 0x21},//1d
	{0x3303, 0x1c},	//[hl,to][1,1a,2e]
	{0x3309, 0x3c},	//[hl,to][1,32,46]
	{0x330a, 0x00},
	{0x330b, 0xc8},	//[bs,to][1,a8,ec]
	{0x3306, 0x68},	//[hl,bs][1,46,88]
	{0x3200, 0x00},
	{0x3201, 0x04},
	{0x3202, 0x00},
	{0x3203, 0x04},
	{0x3204, 0x07},
	{0x3205, 0x8b},
	{0x3206, 0x04},
	{0x3207, 0x43},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3380, 0x1b},
	{0x3341, 0x07},//3318[3:0] + 2
	{0x3343, 0x03},//3318[3:0] -2
	{0x3e25, 0x03},//blc dithering(analog fine gain)
	{0x3e26, 0x40},
	{0x3366, 0x70},//[60,78]
	{0x3e00, 0x00},//max_exposure = vts*2-6;	min_exposure = 3;	20180712
	{0x3e01, 0x95},
	{0x3e02, 0xa0},
	{0x3e03, 0x0b},//gain map 0x0b mode	gain=1x
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3905, 0xd8},
	{0x36e9, 0x23},//enable pll1	20180830
	{0x36f9, 0x05},//enable pll2	20180830
	{0x0100, 0x01},

	{SC2310_REG_DELAY, 10},
	{SC2310_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2310_init_regs_1920_1080_15fps_mipi_dol[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0xa3},//bypass pll1	20180830
	{0x36f9, 0x85},//bypass pll2	20180830
	{0x4509, 0x10},
	{0x4500, 0x39},
	{0x3907, 0x00},
	{0x3908, 0x44},
	{0x3633, 0x87},
	{0x3306, 0x7e},
	{0x330b, 0x00},
	{0x3635, 0x4c},
	{0x330e, 0x7a},
	{0x3302, 0x1f}, //3302 need be odd why????  3366  3302   3621
	{0x3e01, 0x8c},
	{0x3e02, 0x80},
	{0x3e09, 0x1f},
	{0x3e08, 0x3f},
	{0x3e06, 0x03},
	{0x337f, 0x03}, //new auto precharge  330e in 3372   [7:6] 11: close div_rst 00:open div_rst
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x330e, 0x30},
	{0x3320, 0x06}, // New ramp offset timing
	{0x3326, 0x00},
	{0x331e, 0x11},
	{0x331f, 0x21},
	{0x3303, 0x20},
	{0x3309, 0x30},
	{0x4501, 0xc4},
	{0x3e06, 0x00},
	{0x3e08, 0x03},
	{0x3e09, 0x10},
	{0x3366, 0x7c}, // div_rst gap
	{0x3622, 0x02},
	{0x3633, 0x63},
	{0x3038, 0x88},
	{0x3635, 0x45},
	{0x363b, 0x04},
	{0x363c, 0x06},
	{0x363d, 0x05},
	{0x3633, 0x23},
	{0x3301, 0x10},
	{0x3306, 0x58},
	{0x3622, 0x06},//blksun
	{0x3631, 0x88},
	{0x3630, 0x38},
	{0x3633, 0x22},
	{0x3018, 0x33},//[7:5] lane_num-1
	{0x3031, 0x0c},//[3:0] bitmode
	{0x3037, 0x40},//[6:5] bitsel  40:12bit
	{0x3001, 0xFE},//[0] c_y
	{0x4603, 0x00},//[0] data_fifo mipi mode
	{0x4837, 0x35},//[7:0] pclk period * 2
	{0x36e9, 0x83},
	{0x36eb, 0x0f},
	{0x36ec, 0x1f},
	{0x303f, 0x01},
	{0x330b, 0x20},
	{0x3640, 0x00},
	{0x3308, 0x10},
	{0x3637, 0x0a},
	{0x3e09, 0x20}, //3f for 2x fine gain
	{0x363b, 0x08},
	{0x3637, 0x09}, // ADC range: 14.8k fullwell  blc target : 0.9k   output fullwell: 13.9k (5fps 27C  linear fullwell is 14.5K)
	{0x3638, 0x14},
	{0x3636, 0x65},
	{0x3907, 0x01},
	{0x3908, 0x01},
	{0x3320, 0x01}, //ramp
	{0x331e, 0x15},
	{0x331f, 0x25},
	{0x3366, 0x80},
	{0x3634, 0x34},
	{0x57a4, 0xf0}, //default c0,
	{0x3635, 0x41}, //fpn
	{0x3e02, 0x30}, //minimum exp 3? debug
	{0x3333, 0x30}, //col fpn  G >br  ?
	{0x331b, 0x83},
	{0x3334, 0x40},
	{0x3306, 0x6c},
	{0x3638, 0x17},
	{0x330a, 0x01},
	{0x330b, 0x14},
	{0x3302, 0x10},
	{0x3308, 0x08},
	{0x3303, 0x18},
	{0x3309, 0x18},
	{0x331e, 0x11},
	{0x331f, 0x11},
	{0x3f00, 0x0d}, //[2]   hts/2-4
	{0x3f04, 0x02},
	{0x3f05, 0x22},
	{0x3622, 0xe6},
	{0x3633, 0x22},
	{0x3630, 0xc8},
	{0x3301, 0x10},
	{0x36e9, 0xa3},
	{0x36eb, 0x0b},
	{0x36ec, 0x0f},
	{0x3638, 0x27},
	{0x33aa, 0x00}, //power save mode
	{0x3624, 0x02},
	{0x3621, 0xac},
	{0x4509, 0x40},
	{0x391e, 0x00},
	{0x391f, 0xc0},
	{0x3635, 0x45},
	{0x336c, 0x40},
	{0x3621, 0xae},
	{0x3623, 0x08},
	{0x36fa, 0xad}, //charge pump
	{0x3634, 0x44},
	{0x3621, 0xac}, //fifo delay
	{0x4500, 0x59},
	{0x3623, 0x18}, //for more grp rdout setup margin
	{0x3f08, 0x04},
	{0x3f00, 0x0d}, //[2]   hts/2-4-{3f08}
	{0x3f04, 0x02}, //0321
	{0x3f05, 0x1e}, //0321
	{0x336c, 0x42}, //recover read timing
	{0x5000, 0x06},//dpc enable
	{0x5780, 0x7f},//auto blc setting
	{0x57a0, 0x00},	//gain0 = 2x	0x0710ÖÁ0x071f
	{0x57a1, 0x71},
	{0x57a2, 0x01},	//gain1 = 8x	0x1f10ÖÁ0x1f1f
	{0x57a3, 0xf1},
	{0x5781, 0x06},	//white	1x
	{0x5782, 0x04},	//2x
	{0x5783, 0x02},	//8x
	{0x5784, 0x01},	//128x
	{0x5785, 0x16},	//black	1x
	{0x5786, 0x12},	//2x
	{0x5787, 0x08},	//8x
	{0x5788, 0x02},	//128x
	{0x3933, 0x28},
	{0x3934, 0x0a},
	{0x3940, 0x1b},
	{0x3941, 0x40},
	{0x3942, 0x08},
	{0x3943, 0x0e},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x3211, 0x08},
	{0x3213, 0x08},
	{0x36e9, 0xa3},
	{0x36ea, 0x77},
	{0x36eb, 0x0b},
	{0x36ec, 0x0f},
	{0x36ed, 0x03},
	{0x36f9, 0x85},
	{0x36fa, 0x2d},
	{0x36fb, 0x10},
	{0x320c, 0x04},//hts=1125*2=2250
	{0x320d, 0x65},
	{0x320e, 0x04},//vts=1200
	{0x320f, 0xb0},
	{0x3235, 0x12},//vts*2-0x02
	{0x3236, 0xbe},
	{0x3f04, 0x02},//{0x320c,0x320d}/2-0x3f08-0x04
	{0x3f05, 0x2a},
	{0x3802, 0x00},
	{0x3624, 0x47},
	{0x3621, 0xac},
	{0x36fa, 0x2f},
	{0x3637, 0x08},
	{0x3638, 0x25},
	{0x3635, 0x40},
	{0x363b, 0x08},
	{0x363c, 0x05},
	{0x363d, 0x05},
	{0x3303, 0x1c},
	{0x3309, 0x1c},
	{0x3324, 0x02}, //falling edge: ramp_offset_en cover ramp_integ_en
	{0x3325, 0x02},
	{0x333d, 0x08}, //count_div_rst_width
	{0x3314, 0x04},
	{0x36fa, 0x28},
	{0x3205, 0x93},
	{0x3e14, 0xb0}, //[7]:1 ana fine gain double 20~3f
	{0x3e1e, 0x35}, //[7]:1 open DCG function in 0x3e03=0x03 [6]:0 DCG >2   [2] 1: dig_fine_gain_en [1:0] max fine gain  01: 3f
	{0x3e0e, 0x66}, //[7:3] fine gain to compsensate 2.4x DCGgain  5 : 2.3125x  6:2.375x  [2]:1 DCG gain between sa1gain 2~4  [1]:1 dcg gain in 0x3e08[5]
	{0x3364, 0x1d},//[4] fine gain op 1~20--3f 0~10--1f [4] ana dithring en
	{0x33b6, 0x07},//gain0 when dcg off
	{0x33b7, 0x07},//gain1 when dcg off
	{0x33b8, 0x10},//sel0 when dcg off
	{0x33b9, 0x10},//sel1 when dcg off
	{0x33ba, 0x10},//sel2 when dcg off
	{0x33bb, 0x07},//gain0 when dcg on
	{0x33bc, 0x07},//gain1 when dcg on
	{0x33bd, 0x14},//sel0 when dcg on
	{0x33be, 0x14},//sel1 when dcg on
	{0x33bf, 0x14},//sel2 when dcg on
	{0x360f, 0x05},//[0] 3622 auto en
	{0x367a, 0x40},//gain0
	{0x367b, 0x40},//gain1
	{0x3671, 0xf6},//sel0
	{0x3672, 0x16},//sel1
	{0x3673, 0x16},//sel2
	{0x366e, 0x04},//[2] fine gain op 1~20--3f 0~10--1f
	{0x3670, 0x4a},//[1] 3630 auto en, [3] 3633 auto en, [6] 363a auto en
	{0x367c, 0x40},//gain0  3e08[5:2] 1000
	{0x367d, 0x58},//gain1 3e08[5:2] 1011
	{0x3674, 0xc8},//sel0
	{0x3675, 0x54},//sel1
	{0x3676, 0x18},//sel2
	{0x367e, 0x40},//gain0  3e08[5:2] 1000
	{0x367f, 0x58},//gain1  3e08[5:2] 1011
	{0x3677, 0x22},//sel0
	{0x3678, 0x53},//sel1
	{0x3679, 0x55},//sel2
	{0x36a0, 0x58},//gain0  3e08[5:2] 1011
	{0x36a1, 0x78},//gain1  3e08[5:2] 1111
	{0x3696, 0x83},//sel0
	{0x3697, 0x87},//sel1
	{0x3698, 0x9f},//sel2
	{0x4837, 0x31},
	{0x6000, 0x00},
	{0x6002, 0x00},
	{0x301c, 0x78},//close dvp
	{0x3037, 0x44},//[3:0] pump div	range [10M,20M],sclk=81/2=40.5M,div=4-->sclk/4=10.125M,duty cycle-->even number
	{0x3038, 0x44},//[7:4]ppump & [3:0]npump
	{0x3632, 0x18},//[5:4]idac driver
	{0x5785, 0x40},//black	point 1x
	{0x4809, 0x01},//mipi first frame, lp status
	{0x3637, 0x10},
	{0x5000, 0x06},//dpc enable
	{0x5780, 0x7f},//auto blc setting
	{0x57a0, 0x00},	//gain0 = 2x	0x0740ÖÁ0x077f
	{0x57a1, 0x74},
	{0x57a2, 0x01},	//gain1 = 8x	0x1f40ÖÁ0x1f7f
	{0x57a3, 0xf4},
	{0x5781, 0x06},	//white	1x
	{0x5782, 0x04},	//2x
	{0x5783, 0x02},	//8x
	{0x5784, 0x01},	//128x
	{0x5785, 0x16},	//black	1x
	{0x5786, 0x12},	//2x
	{0x5787, 0x08},	//8x
	{0x5788, 0x02},	//128x
	{0x4501, 0xb4},//reduce bit
	{0x3637, 0x20},
	{0x4509, 0x20},//blc quantification	//20181206
	{0x3364, 0x1d},//[4] fine gain op 1~20--3f 0~10--1f [4] ana dithring en
	{0x33b6, 0x07},//gain0 when dcg off	gain<dcg
	{0x33b7, 0x07},//gain1 when dcg off
	{0x33b8, 0x10},//sel0 when dcg off
	{0x33b9, 0x10},//sel1 when dcg off
	{0x33ba, 0x10},//sel2 when dcg off
	{0x33bb, 0x07},//gain0 when dcg on		gain>=dcg
	{0x33bc, 0x07},//gain1 when dcg on
	{0x33bd, 0x20},//sel0 when dcg on
	{0x33be, 0x20},//sel1 when dcg on
	{0x33bf, 0x20},//sel2 when dcg on
	{0x360f, 0x05},//[0] 3622 auto en
	{0x367a, 0x40},//gain0
	{0x367b, 0x40},//gain1
	{0x3671, 0xf6},//sel0
	{0x3672, 0x16},//sel1
	{0x3673, 0x16},//sel2
	{0x366e, 0x04},//[2] fine gain op 1~20--3f 0~10--1f
	{0x3670, 0x4a},//[1] 3630 auto en, [3] 3633 auto en, [6] 363a auto en
	{0x367c, 0x40},//gain0  3e08[5:2] 1000
	{0x367d, 0x58},//gain1 3e08[5:2] 1011
	{0x3674, 0xc8},//sel0
	{0x3675, 0x54},//sel1
	{0x3676, 0x18},//sel2
	{0x367e, 0x40},//gain0  3e08[5:2] 1000
	{0x367f, 0x58},//gain1  3e08[5:2] 1011
	{0x3677, 0x22},//sel0
	{0x3678, 0x33},//sel1
	{0x3679, 0x44},//sel2
	{0x36a0, 0x58},//gain0  3e08[5:2] 1011
	{0x36a1, 0x78},//gain1  3e08[5:2] 1111
	{0x3696, 0x9f},//sel0
	{0x3697, 0x9f},//sel1
	{0x3698, 0x9f},//sel2
	{0x3637, 0x17},//fullwell 8.6k
	{0x331e, 0x11},
	{0x331f, 0x21},//1d
	{0x3303, 0x1c},	//[hl,to][1,1a,2e]
	{0x3309, 0x3c},	//[hl,to][1,32,46]
	{0x330a, 0x00},
	{0x330b, 0xc8},	//[bs,to][1,a8,ec]
	{0x3306, 0x68},	//[hl,bs][1,46,88]
	{0x3200, 0x00},
	{0x3201, 0x04},
	{0x3202, 0x00},
	{0x3203, 0x04},
	{0x3204, 0x07},
	{0x3205, 0x8b},
	{0x3206, 0x04},
	{0x3207, 0x43},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3380, 0x1b},
	{0x3341, 0x07},//3318[3:0] + 2
	{0x3343, 0x03},//3318[3:0] -2
	{0x3e25, 0x03},//blc dithering(analog fine gain)
	{0x3e26, 0x40},
	{0x3366, 0x70},//[60,78]
	{0x4816, 0x51},//bit[4]
	{0x3220, 0x51},//bit[6]
	{0x4602, 0x0f},//bit[3:0]
	{0x33c0, 0x05},//bit[2]
	{0x6000, 0x06},
	{0x6002, 0x06},
	/* {0x320e, 0x1a},//double vts */
	/* {0x320f, 0x60}, */
	{0x320e, 0x12},//double vts
	{0x320f, 0x60},
	{0x3202, 0x00},//x_start must be 0x00
	{0x3203, 0x00},
	{0x3206, 0x04},//1088	activeBoard=4
	{0x3207, 0x3f},
	{0x3e00, 0x01},//max long exposure = 0x103e
	{0x3e01, 0x19},
	{0x3e02, 0xe0},
	{0x3e04, 0x11},//max short exposure = 0x104
	{0x3e05, 0x80},
	{0x3e23, 0x01},//max long exp : max short exp <= 16:1
	{0x3e24, 0x1a},
	{0x3e03, 0x0b},//gain map 0x0b mode	gain=1x
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3905, 0xd8},
	{0x36e9, 0x23},//enable pll1	20180830
	{0x36f9, 0x05},//enable pll2	20180830
	{0x0100, 0x01},
	{SC2310_REG_DELAY, 10},
	{SC2310_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2310_init_regs_1920_1080_25fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0xa3},//bypass pll1
	{0x36f9, 0x85},//bypass pll2
	{0x337f, 0x03},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x330e, 0x30},
	{0x3326, 0x00},
	{0x3631, 0x88},
	{0x3018, 0x33},
	{0x3031, 0x0c},
	{0x3001, 0xfe},
	{0x4603, 0x00},
	{0x303f, 0x01},
	{0x3640, 0x00},
	{0x3636, 0x65},
	{0x3907, 0x01},
	{0x3908, 0x01},
	{0x3320, 0x01},
	{0x3366, 0x70},
	{0x57a4, 0xf0},
	{0x3333, 0x30},
	{0x331b, 0x83},
	{0x3334, 0x40},
	{0x3302, 0x10},
	{0x3308, 0x08},
	{0x3622, 0xe6},
	{0x3633, 0x22},
	{0x3630, 0xc8},
	{0x3301, 0x10},
	{0x33aa, 0x00},
	{0x391e, 0x00},
	{0x391f, 0xc0},
	{0x3634, 0x44},
	{0x4500, 0x59},
	{0x3623, 0x18},
	{0x3f08, 0x04},
	{0x3f00, 0x0d},
	{0x336c, 0x42},
	{0x3933, 0x28},
	{0x3934, 0x0a},
	{0x3940, 0x1b},
	{0x3941, 0x40},
	{0x3942, 0x08},
	{0x3943, 0x0e},
	{0x36ea, 0x77},
	{0x36eb, 0x0b},
	{0x36ec, 0x0f},
	{0x36ed, 0x03},
	{0x36fb, 0x10},
	{0x320c, 0x04},//1125
	{0x320d, 0x65},
	{0x320e, 0x05},//1440
	{0x320f, 0xa0},
	{0x3235, 0x09},
	{0x3236, 0x5e},
	{0x3f04, 0x02},
	{0x3f05, 0x2a},
	{0x3802, 0x00},
	{0x3624, 0x47},
	{0x3621, 0xac},
	{0x3638, 0x25},
	{0x3635, 0x40},
	{0x363b, 0x08},
	{0x363c, 0x05},
	{0x363d, 0x05},
	{0x3324, 0x02},
	{0x3325, 0x02},
	{0x333d, 0x08},
	{0x3314, 0x04},
	{0x36fa, 0x28},
	{0x3e14, 0xb0},
	{0x3e1e, 0x35},
	{0x3e0e, 0x66},
	{0x4837, 0x31},
	{0x6000, 0x00},
	{0x6002, 0x00},
	{0x301c, 0x78},
	{0x3037, 0x44},
	{0x3038, 0x44},
	{0x3632, 0x18},
	{0x4809, 0x01},
	{0x5000, 0x06},
	{0x5780, 0x7f},
	{0x57a0, 0x00},
	{0x57a1, 0x74},
	{0x57a2, 0x01},
	{0x57a3, 0xf4},
	{0x5781, 0x06},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x16},
	{0x5786, 0x12},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x4501, 0xb4},
	{0x4509, 0x20},
	{0x3364, 0x1d},
	{0x33b6, 0x07},
	{0x33b7, 0x07},
	{0x33b8, 0x10},
	{0x33b9, 0x10},
	{0x33ba, 0x10},
	{0x33bb, 0x07},
	{0x33bc, 0x07},
	{0x33bd, 0x20},
	{0x33be, 0x20},
	{0x33bf, 0x20},
	{0x360f, 0x05},
	{0x367a, 0x40},
	{0x367b, 0x40},
	{0x3671, 0xf6},
	{0x3672, 0x16},
	{0x3673, 0x16},
	{0x366e, 0x04},
	{0x3670, 0x4a},
	{0x367c, 0x40},
	{0x367d, 0x58},
	{0x3674, 0xc8},
	{0x3675, 0x54},
	{0x3676, 0x18},
	{0x367e, 0x40},
	{0x367f, 0x58},
	{0x3677, 0x22},
	{0x3678, 0x33},
	{0x3679, 0x44},
	{0x36a0, 0x58},
	{0x36a1, 0x78},
	{0x3696, 0x83},
	{0x3697, 0x87},
	{0x3698, 0x9f},
	{0x3637, 0x17},
	{0x331e, 0x11},
	{0x331f, 0x21},
	{0x3303, 0x1c},
	{0x3309, 0x3c},
	{0x330a, 0x00},
	{0x330b, 0xc8},
	{0x3306, 0x68},
	{0x3200, 0x00},
	{0x3201, 0x04},
	{0x3202, 0x00},
	{0x3203, 0x04},
	{0x3204, 0x07},
	{0x3205, 0x8b},
	{0x3206, 0x04},
	{0x3207, 0x43},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3380, 0x1b},
	{0x3341, 0x07},
	{0x3343, 0x03},
	{0x3e25, 0x03},
	{0x3e26, 0x40},
	{0x3e00, 0x00},
	{0x3e01, 0x95},
	{0x3e02, 0xa0},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3905, 0xd8},
	{0x36e9, 0x23},
	{0x36f9, 0x05},
	{0x0100, 0x01},
	{SC2310_REG_DELAY, 10},
	{SC2310_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2310_init_regs_1920_1080_25fps_dvp[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0xa3},//bypass pll1
	{0x36f9, 0x85},//bypass pll2
	{0x337f, 0x03},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x330e, 0x30},
	{0x3326, 0x00},
	{0x3631, 0x88},
	{0x3640, 0x00},
	{0x3636, 0x65},
	{0x3907, 0x01},
	{0x3908, 0x01},
	{0x3320, 0x01},
	{0x3366, 0x70},
	{0x57a4, 0xf0},
	{0x3333, 0x30},
	{0x331b, 0x83},
	{0x3334, 0x40},
	{0x3302, 0x10},
	{0x3308, 0x08},
	{0x3622, 0xe6},
	{0x3633, 0x22},
	{0x3630, 0xc8},
	{0x3301, 0x10},
	{0x33aa, 0x00},
	{0x391e, 0x00},
	{0x391f, 0xc0},
	{0x3634, 0x44},
	{0x4500, 0x59},
	{0x3623, 0x18},
	{0x3f08, 0x04},
	{0x3f00, 0x0d},
	{0x336c, 0x42},
	{0x3933, 0x28},
	{0x3934, 0x0a},
	{0x3940, 0x1b},
	{0x3941, 0x40},
	{0x3942, 0x08},
	{0x3943, 0x0e},
	{0x36ea, 0x77},
	{0x36eb, 0x0b},
	{0x36ec, 0x0f},
	{0x36ed, 0x03},
	{0x36fb, 0x10},
	{0x320c, 0x04},
	{0x320d, 0x65},
	{0x320e, 0x05},
	{0x320f, 0xa0},
	{0x3235, 0x09},
	{0x3236, 0x5e},
	{0x3f04, 0x02},
	{0x3f05, 0x2a},
	{0x3802, 0x00},
	{0x3624, 0x47},
	{0x3621, 0xac},
	{0x3019, 0xff},
	{0x301c, 0xb4},
	{0x3018, 0x1f},
	{0x3031, 0x0a},
	{0x3001, 0xff},
	{0x4603, 0x01},
	{0x4837, 0x36},
	{0x303f, 0x81},
	{0x3d08, 0x01},
	{0x3638, 0x25},
	{0x3635, 0x40},
	{0x363b, 0x08},
	{0x363c, 0x05},
	{0x363d, 0x05},
	{0x3641, 0x01},
	{0x3324, 0x02},
	{0x3325, 0x02},
	{0x333d, 0x08},
	{0x3314, 0x04},
	{0x36fa, 0x28},
	{0x3e14, 0xb0},
	{0x3e1e, 0x35},
	{0x3e0e, 0x66},
	{0x6000, 0x00},
	{0x6002, 0x00},
	{0x3037, 0x24},
	{0x3038, 0x44},
	{0x3632, 0x18},
	{0x4809, 0x01},
	{0x5000, 0x06},
	{0x5780, 0x7f},
	{0x57a0, 0x00},
	{0x57a1, 0x74},
	{0x57a2, 0x01},
	{0x57a3, 0xf4},
	{0x5781, 0x06},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x16},
	{0x5786, 0x12},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x4501, 0xb4},
	{0x4509, 0x20},
	{0x3364, 0x1d},
	{0x33b6, 0x07},
	{0x33b7, 0x07},
	{0x33b8, 0x10},
	{0x33b9, 0x10},
	{0x33ba, 0x10},
	{0x33bb, 0x07},
	{0x33bc, 0x07},
	{0x33bd, 0x20},
	{0x33be, 0x20},
	{0x33bf, 0x20},
	{0x360f, 0x05},
	{0x367a, 0x40},
	{0x367b, 0x40},
	{0x3671, 0xf6},
	{0x3672, 0x16},
	{0x3673, 0x16},
	{0x366e, 0x04},
	{0x3670, 0x4a},
	{0x367c, 0x40},
	{0x367d, 0x58},
	{0x3674, 0xc8},
	{0x3675, 0x54},
	{0x3676, 0x18},
	{0x367e, 0x40},
	{0x367f, 0x58},
	{0x3677, 0x22},
	{0x3678, 0x33},
	{0x3679, 0x44},
	{0x36a0, 0x58},
	{0x36a1, 0x78},
	{0x3696, 0x83},
	{0x3697, 0x87},
	{0x3698, 0x9f},
	{0x3637, 0x17},
	{0x331e, 0x11},
	{0x331f, 0x21},
	{0x3303, 0x1c},
	{0x3309, 0x3c},
	{0x330a, 0x00},
	{0x330b, 0xc8},
	{0x3306, 0x68},
	{0x3200, 0x00},
	{0x3201, 0x04},
	{0x3202, 0x00},
	{0x3203, 0x04},
	{0x3204, 0x07},
	{0x3205, 0x8b},
	{0x3206, 0x04},
	{0x3207, 0x43},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3380, 0x1b},
	{0x3341, 0x07},
	{0x3343, 0x03},
	{0x3e25, 0x03},
	{0x3e26, 0x40},
	{0x3e00, 0x00},
	{0x3e01, 0x95},
	{0x3e02, 0xa0},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3905, 0xd8},
	{0x36e9, 0x23},
	{0x36f9, 0x05},
	{0x0100, 0x01},
	{SC2310_REG_DELAY, 10},
	{SC2310_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the sc2310_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc2310_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2310_init_regs_1920_1080_25fps_dvp,
	},//[0]
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2310_init_regs_1920_1080_15fps_mipi,
	},//[1]
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2310_init_regs_1920_1080_25fps_mipi,
	},//[2]
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 15 << 16 | 2,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2310_init_regs_1920_1080_15fps_mipi_dol,
	}, //[3]
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2310_init_regs_1920_1080_25fps_mipi_dol,
	} //[4]
};

struct tx_isp_sensor_win_setting *wsize = &sc2310_win_sizes[0];

static enum v4l2_mbus_pixelcode sc2310_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
	V4L2_MBUS_FMT_SBGGR12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sc2310_stream_on_dvp[] = {
	{0x0100, 0x01},
	{SC2310_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2310_stream_off_dvp[] = {
	{0x0100, 0x00},
	{SC2310_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2310_stream_on_mipi[] = {
	{0x0100,0x01},
	{SC2310_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2310_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC2310_REG_END, 0x00},	/* END MARKER */
};

int sc2310_read(struct tx_isp_subdev *sd, uint16_t reg,	unsigned char *value)
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

int sc2310_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int sc2310_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC2310_REG_END) {
		if (vals->reg_num == SC2310_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2310_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
static int sc2310_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC2310_REG_END) {
		if (vals->reg_num == SC2310_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2310_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc2310_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc2310_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc2310_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC2310_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc2310_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != SC2310_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc2310_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = 0;
	if(data_type == TX_SENSOR_DATA_TYPE_LINEAR){
		expo = value*2;
		ret += sc2310_write(sd, 0x3e00, (unsigned char)((expo >> 12) & 0x0f));
		ret += sc2310_write(sd, 0x3e01, (unsigned char)((expo >> 4) & 0xff));
		ret += sc2310_write(sd, 0x3e02, (unsigned char)((expo & 0x0f) << 4));
		if (value < 0x50) {
			ret += sc2310_write(sd, 0x3812, 0x00);
			ret += sc2310_write(sd, 0x3314, 0x14);
			ret += sc2310_write(sd, 0x3812, 0x30);
		} else if(value > 0xa0){
			ret += sc2310_write(sd, 0x3812, 0x00);
			ret += sc2310_write(sd, 0x3314, 0x04);
			ret += sc2310_write(sd, 0x3812, 0x30);
		}
	} else {
		expo = value*4;
		ret += sc2310_write(sd, 0x3e00, (unsigned char)((expo >> 12) & 0x0f));
		ret += sc2310_write(sd, 0x3e01, (unsigned char)((expo >> 4) & 0xff));
		ret += sc2310_write(sd, 0x3e02, (unsigned char)((expo & 0x0f) << 4));
		if (value < 0x50) {
			ret += sc2310_write(sd, 0x3812, 0x00);
			ret += sc2310_write(sd, 0x3314, 0x14);
			ret += sc2310_write(sd, 0x3812, 0x30);
		} else if(value > 0xa0){
			ret += sc2310_write(sd, 0x3812, 0x00);
			ret += sc2310_write(sd, 0x3314, 0x04);
			ret += sc2310_write(sd, 0x3812, 0x30);
		}

	}

	if (ret < 0)
		return ret;

	return 0;
}

static int sc2310_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = 0;

	expo = value * 4;
	ret += sc2310_write(sd, 0x3e04, (unsigned char)((expo >> 4) & 0xff));
	ret += sc2310_write(sd, 0x3e05, (unsigned char)((expo & 0x0f) << 4));
	if (value < 0x50) {
		ret += sc2310_write(sd, 0x3812, 0x00);
		ret += sc2310_write(sd, 0x3314, 0x14);
		ret += sc2310_write(sd, 0x3812, 0x30);
	}
	else if(value > 0xa0){
		ret += sc2310_write(sd, 0x3812, 0x00);
		ret += sc2310_write(sd, 0x3314, 0x04);
		ret += sc2310_write(sd, 0x3812, 0x30);
	}

	return 0;
}


static int sc2310_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sc2310_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc2310_write(sd, 0x3e08, (unsigned char)(value >> 8 & 0x3f));
//	ret += sc2310_write(sd, 0x3e08, (unsigned char)((value >> 8 << 2) | 0x03));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc2310_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2310_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sc2310_write(sd, 0x3e13, (unsigned char)(value & 0xff));
	ret += sc2310_write(sd, 0x3e12, (unsigned char)(value >> 8 & 0x3f));

	return 0;
}

static int sc2310_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2310_init(struct tx_isp_subdev *sd, int enable)
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
	ret = sc2310_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc2310_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc2310_write_array(sd, sc2310_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2310_write_array(sd, sc2310_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2310 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc2310_write_array(sd, sc2310_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2310_write_array(sd, sc2310_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2310 stream off\n");
	}

	return ret;
}

static int sc2310_set_fps(struct tx_isp_subdev *sd, int fps)
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
	sclk = SC2310_SUPPORT_SCLK;

	ret = sc2310_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc2310_read(sd, 0x320d, &tmp);
	if(ret < 0)
		return -1;
	hts = ((hts << 8) + tmp)*2;
	if (0 != ret) {
		ISP_ERROR("err: sc2310 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sc2310_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc2310_write(sd, 0x320e, (unsigned char)(vts >> 8));

	if(ret < 0)
		return -1;

	if (0 != ret) {
		ISP_ERROR("err: sc2310_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native =vts - 3;
	sensor->video.attr->integration_time_limit =vts - 3;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time =vts - 3;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc2310_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc2310_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc2310_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(40);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(40);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(40);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc2310_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc2310_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc2310 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc2310 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "sc2310", sizeof("sc2310"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc2310_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = sc2310_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc2310_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = sc2310_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc2310_set_analog_gain_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc2310_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc2310_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc2310_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc2310_write_array(sd, sc2310_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2310_write_array(sd, sc2310_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc2310_write_array(sd, sc2310_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2310_write_array(sd, sc2310_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc2310_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sc2310_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc2310_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc2310_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc2310_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc2310_core_ops = {
	.g_chip_ident = sc2310_g_chip_ident,
	.reset = sc2310_reset,
	.init = sc2310_init,
	/*.ioctl = sc2310_ops_ioctl,*/
	.g_register = sc2310_g_register,
	.s_register = sc2310_s_register,
};

static struct tx_isp_subdev_video_ops sc2310_video_ops = {
	.s_stream = sc2310_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc2310_sensor_ops = {
	.ioctl	= sc2310_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc2310_ops = {
	.core = &sc2310_core_ops,
	.video = &sc2310_video_ops,
	.sensor = &sc2310_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc2310",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc2310_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	enum v4l2_mbus_pixelcode mbus;
	unsigned long rate = 0;
	int i = 0;
	int ret;

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
	if((sensor_max_fps == TX_SENSOR_MAX_FPS_25) && (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)){
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

	if(data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		sc2310_attr.wdr_cache = 0;
		if((data_interface == TX_SENSOR_DATA_INTERFACE_DVP) && (sensor_max_fps == TX_SENSOR_MAX_FPS_25)){
			wsize = &sc2310_win_sizes[0];
			memcpy((void*)(&(sc2310_attr.dvp)),(void*)(&sc2310_dvp),sizeof(sc2310_dvp));
		} else if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_max_fps == TX_SENSOR_MAX_FPS_25)){
			wsize = &sc2310_win_sizes[2];
			memcpy((void*)(&(sc2310_attr.mipi)),(void*)(&sc2310_mipi_linear),sizeof(sc2310_mipi_linear));
		} else if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_max_fps == TX_SENSOR_MAX_FPS_15)){
			wsize = &sc2310_win_sizes[1];
			memcpy((void*)(&(sc2310_attr.mipi)),(void*)(&sc2310_mipi_linear),sizeof(sc2310_mipi_linear));
			sc2310_attr.max_integration_time_native = 0x1260 - 3;//  0x1260*2 - 0x11a - 5;
			sc2310_attr.integration_time_limit = 0x1260 - 3;
			sc2310_attr.max_integration_time = 0x1260 - 3;
			sc2310_attr.max_integration_time_native = 0x1260 - 3;
			sc2310_attr.total_width = 0x465 * 2;
			sc2310_attr.total_height = 0x1260;
			sc2310_attr.max_integration_time = 0x1260 - 3;
		} else {
			ISP_ERROR("Can not support this data interface and fps!!!\n");
			goto err_set_sensor_data_interface;
		}
	} else if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		sc2310_attr.wdr_cache = wdr_bufsize;
		if(sensor_max_fps == TX_SENSOR_MAX_FPS_25){
			wsize = &sc2310_win_sizes[4];
			memcpy((void*)(&(sc2310_attr.mipi)),(void*)(&sc2310_mipi_wdr_25),sizeof(sc2310_mipi_wdr_25));
			sc2310_attr.min_integration_time = 3;
			sc2310_attr.min_integration_time_short = 2;
			sc2310_attr.max_integration_time_short = 0x8b - 1;
			sc2310_attr.min_integration_time_native = 3;
			sc2310_attr.max_integration_time_native = 0xce7;//  0x1260*2 - 0x11a - 5;
			sc2310_attr.integration_time_limit = 0xce7;
			sc2310_attr.max_integration_time = 0xce7;
			sc2310_attr.max_integration_time_native = 0xce7;
			sc2310_attr.total_width = 0x465 * 2;
			sc2310_attr.total_height = 0x1a60;
			sc2310_attr.max_integration_time = 0xce7;
		} else {
			wsize = &sc2310_win_sizes[3];
			memcpy((void*)(&(sc2310_attr.mipi)),(void*)(&sc2310_mipi_wdr),sizeof(sc2310_mipi_wdr));
			sc2310_attr.min_integration_time = 3;
			sc2310_attr.min_integration_time_short = 2;
			sc2310_attr.max_integration_time_short = 0x8b - 1;
			sc2310_attr.min_integration_time_native = 3;
			sc2310_attr.max_integration_time_native = 0xce7;//  0x1260*2 - 0x11a - 5;
			sc2310_attr.integration_time_limit = 0xce7;
			sc2310_attr.max_integration_time = 0xce7;
			sc2310_attr.max_integration_time_native = 0xce7;
			sc2310_attr.total_width = 0x465 * 2;
			sc2310_attr.total_height = 0x1a60;
			sc2310_attr.max_integration_time = 0xce7;

			/* sc2310_attr.min_integration_time = 3; */
			/* sc2310_attr.min_integration_time_short = 2; */
			/* sc2310_attr.max_integration_time_short = 0x8d - 1; */
			/* sc2310_attr.min_integration_time_native = 3; */
			/* sc2310_attr.max_integration_time_native = 0x8e9;//  0x1260*2 - 0x11a - 5; */
			/* sc2310_attr.integration_time_limit = 0x8e9; */
			/* sc2310_attr.max_integration_time = 0x8e9; */
			/* sc2310_attr.max_integration_time_native = 0x8e9; */
			/* sc2310_attr.total_width = 0x465 * 2; */
			/* sc2310_attr.total_height = 0x1260; */
			/* sc2310_attr.max_integration_time = 0x8e9; */
		}
	} else {
		ISP_ERROR("Can not support this data type!!!\n");
	}
	sc2310_attr.dbus_type = data_interface;
	sc2310_attr.data_type = data_type;

	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
		ret = set_sensor_gpio_function(sensor_gpio_func);
		if (ret < 0)
			goto err_set_sensor_gpio;
		switch(sensor_gpio_func){
		case DVP_PA_LOW_10BIT:
		case DVP_PA_HIGH_10BIT:
			mbus = sc2310_mbus_code[0];
			break;
		case DVP_PA_12BIT:
			mbus = sc2310_mbus_code[1];
			break;
		default:
			goto err_set_sensor_gpio;
		}
		for(i = 0; i < ARRAY_SIZE(sc2310_win_sizes); i++)
			sc2310_win_sizes[i].mbus_code = mbus;
		sc2310_attr.dvp.gpio = sensor_gpio_func;
	}
	/*
	  convert sensor-gain into isp-gain,
	*/
	sc2310_attr.max_again = 356010;
	sc2310_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sc2310_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc2310_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc2310\n");

	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sc2310_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc2310_id[] = {
	{ "sc2310", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc2310_id);

static struct i2c_driver sc2310_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc2310",
	},
	.probe		= sc2310_probe,
	.remove		= sc2310_remove,
	.id_table	= sc2310_id,
};

static __init int init_sc2310(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc2310 dirver.\n");
		return -1;
	}

	return private_i2c_add_driver(&sc2310_driver);
}

static __exit void exit_sc2310(void)
{
	private_i2c_del_driver(&sc2310_driver);
}

module_init(init_sc2310);
module_exit(exit_sc2310);

MODULE_DESCRIPTION("A low-level driver for OmniVision sc2310 sensors");
MODULE_LICENSE("GPL");
