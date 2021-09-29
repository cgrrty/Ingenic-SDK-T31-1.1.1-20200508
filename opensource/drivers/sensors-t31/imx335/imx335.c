/*
 * imx335.c
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

#define IMX335_CHIP_ID_H	(0x08)
#define IMX335_CHIP_ID_L	(0x00)
#define IMX335_REG_END		0xffff
#define IMX335_REG_DELAY	0xfffe
#define IMX335_SUPPORT_SCLK     (74250000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20190923"
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16

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

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut imx335_again_lut[] = {
	/* analog gain */
	{0x0, 0},
	{0x1, 3265},
	{0x2, 6531},
	{0x3, 9796},
	{0x4, 13062},
	{0x5, 16327},
	{0x6, 19593},
	{0x7, 22859},
	{0x8, 26124},
	{0x9, 29390},
	{0xa, 32655},
	{0xb, 35921},
	{0xc, 39187},
	{0xd, 42452},
	{0xe, 45718},
	{0xf, 48983},
	{0x10, 52249},
	{0x11, 55514},
	{0x12, 58780},
	{0x13, 62046},
	{0x14, 65311},
	{0x15, 68577},
	{0x16, 71842},
	{0x17, 75108},
	{0x18, 78374},
	{0x19, 81639},
	{0x1a, 84905},
	{0x1b, 88170},
	{0x1c, 91436},
	{0x1d, 94702},
	{0x1e, 97967},
	{0x1f, 101233},
	{0x20, 104498},
	{0x21, 107764},
	{0x22, 111029},
	{0x23, 114295},
	{0x24, 117561},
	{0x25, 120826},
	{0x26, 124092},
	{0x27, 127357},
	{0x28, 130623},
	{0x29, 133889},
	{0x2a, 137154},
	{0x2b, 140420},
	{0x2c, 143685},
	{0x2d, 146951},
	{0x2e, 150217},
	{0x2f, 153482},
	{0x30, 156748},
	{0x31, 160013},
	{0x32, 163279},
	{0x33, 166544},
	{0x34, 169810},
	{0x35, 173076},
	{0x36, 176341},
	{0x37, 179607},
	{0x38, 182872},
	{0x39, 186138},
	{0x3a, 189404},
	{0x3b, 192669},
	{0x3c, 195935},
	{0x3d, 199200},
	{0x3e, 202466},
	{0x3f, 205732},
	{0x40, 208997},
	{0x41, 212263},
	{0x42, 215528},
	{0x43, 218794},
	{0x44, 222059},
	{0x45, 225325},
	{0x46, 228591},
	{0x47, 231856},
	{0x48, 235122},
	{0x49, 238387},
	{0x4a, 241653},
	{0x4b, 244919},
	{0x4c, 248184},
	{0x4d, 251450},
	{0x4e, 254715},
	{0x4f, 257981},
	{0x50, 261247},
	{0x51, 264512},
	{0x52, 267778},
	{0x53, 271043},
	{0x54, 274309},
	{0x55, 277574},
	{0x56, 280840},
	{0x57, 284106},
	{0x58, 287371},
	{0x59, 290637},
	{0x5a, 293902},
	{0x5b, 297168},
	{0x5c, 300434},
	{0x5d, 303699},
	{0x5e, 306965},
	{0x5f, 310230},
	{0x60, 313496},
	{0x61, 316762},
	{0x62, 320027},
	{0x63, 323293},
	{0x64, 326558},
	/* analog+digital */
	{0x65, 329824},
	{0x66, 333089},
	{0x67, 336355},
	{0x68, 339621},
	{0x69, 342886},
	{0x6a, 346152},
	{0x6b, 349417},
	{0x6c, 352683},
	{0x6d, 355949},
	{0x6e, 359214},
	{0x6f, 362480},
	{0x70, 365745},
	{0x71, 369011},
	{0x72, 372277},
	{0x73, 375542},
	{0x74, 378808},
	{0x75, 382073},
	{0x76, 385339},
	{0x77, 388604},
	{0x78, 391870},
	{0x79, 395136},
	{0x7a, 398401},
	{0x7b, 401667},
	{0x7c, 404932},
	{0x7d, 408198},
	{0x7e, 411464},
	{0x7f, 414729},
	{0x80, 417995},
	{0x81, 421260},
	{0x82, 424526},
	{0x83, 427792},
	{0x84, 431057},
	{0x85, 434323},
	{0x86, 437588},
	{0x87, 440854},
	{0x88, 444119},
	{0x89, 447385},
	{0x8a, 450651},
	{0x8b, 453916},
	{0x8c, 457182},
	{0x8d, 460447},
	{0x8e, 463713},
	{0x8f, 466979},
	{0x90, 470244},
	{0x91, 473510},
	{0x92, 476775},
	{0x93, 480041},
	{0x94, 483307},
	{0x95, 486572},
	{0x96, 489838},
	{0x97, 493103},
	{0x98, 496369},
	{0x99, 499634},
	{0x9a, 502900},/*204x*/
	{0x9b, 506166},
	{0x9c, 509431},
	{0x9d, 512697},
	{0x9e, 515962},
	{0x9f, 519228},
	{0xa0, 522494},
	{0xa1, 525759},
	{0xa2, 529025},
	{0xa3, 532290},
	{0xa4, 535556},
	{0xa5, 538822},
	{0xa6, 542087},
	{0xa7, 545353},
	{0xa8, 548618},
	{0xa9, 551884},
	{0xaa, 555149},
	{0xab, 558415},
	{0xac, 561681},
	{0xad, 564946},
	{0xae, 568212},
	{0xaf, 571477},
	{0xb0, 574743},
	{0xb1, 578009},
	{0xb2, 581274},
	{0xb3, 584540},
	{0xb4, 587805},
	{0xb5, 591071},
	{0xb6, 594337},
	{0xb7, 597602},
	{0xb8, 600868},
	{0xb9, 604133},
	{0xba, 607399},
	{0xbb, 610664},
	{0xbc, 613930},
	{0xbd, 617196},
	{0xbe, 620461},
	{0xbf, 623727},
	{0xc0, 626992},
	{0xc1, 630258},
	{0xc2, 633524},
	{0xc3, 636789},
	{0xc4, 640055},
	{0xc5, 643320},
	{0xc6, 646586},
	{0xc7, 649852},
	{0xc8, 653117},
	{0xc9, 656383},
	{0xca, 659648},
	{0xcb, 662914},
	{0xcc, 666179},
	{0xcd, 669445},
	{0xce, 672711},
	{0xcf, 675976},
	{0xd0, 679242},
	{0xd1, 682507},
	{0xd2, 685773},
	{0xd3, 689039},
	{0xd4, 692304},
	{0xd5, 695570},
	{0xd6, 698835},
	{0xd7, 702101},
	{0xd8, 705367},
	{0xd9, 708632},
	{0xda, 711898},
	{0xdb, 715163},
	{0xdc, 718429},
	{0xdd, 721694},
	{0xde, 724960},
	{0xdf, 728226},
	{0xe0, 731491},
	{0xe1, 734757},
	{0xe2, 738022},
	{0xe3, 741288},
	{0xe4, 744554},
	{0xe5, 747819},
	{0xe6, 751085},
	{0xe7, 754350},
	{0xe8, 757616},
	{0xe9, 760882},
	{0xea, 764147},
	{0xeb, 767413},
	{0xec, 770678},
	{0xed, 773944},
	{0xee, 777209},
	{0xef, 780475},
	{0xf0, 783741},
};

struct tx_isp_sensor_attribute imx335_attr;

/*
 * the part of driver maybe modify about different sensor and different board.
 */

unsigned int imx335_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
}



unsigned int imx335_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}


struct tx_isp_sensor_attribute imx335_attr={
	.name = "imx335",
	.chip_id = 0x080,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_SONY_MODE,
		.clk = 594,
		.lans = 2,
		.settle_time_apative_en = 0,
		.image_twidth = 2616,
		.image_theight = 1964,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.mipi_crop_start0x =12,
		.mipi_sc.mipi_crop_start0y = 33,
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

	.max_again = 589815,//460447,
	.max_again_short = 589815,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 4116,
	.min_integration_time_short = 1,
	.max_integration_time_short = 10,
	.integration_time_limit = 4116,
	.total_width = 1200,
	.total_height = 4125,
	.max_integration_time = 4116,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = imx335_alloc_again,
	.sensor_ctrl.alloc_dgain = imx335_alloc_dgain,
};

static struct regval_list imx335_init_regs_2592_1944_15fps_mipi[] = {
	{0x3000, 0x01},
	{0x3001, 0x00},
	{0x3002, 0x01},
	{0x3004, 0x00},
	{0x300C, 0x5B},
	{0x300D, 0x40},
	{0x3018, 0x00},
	{0x302C, 0x30},
	{0x302D, 0x00},
	{0x302E, 0x38},
	{0x302F, 0x0A},
	{0x3030, 0x1d},
	{0x3031, 0x10},
	{0x3032, 0x00},
	{0x3034, 0xb0},
	{0x3035, 0x04},/* 20fps 0x384 */
	{0x3048, 0x00},
	{0x3049, 0x00},
	{0x304A, 0x03},
	{0x304B, 0x01},
	{0x304C, 0x14},
	{0x304E, 0x00},
	{0x304F, 0x00},/* vflip */
	{0x3050, 0x00},
	{0x3056, 0xAC},
	{0x3057, 0x07},
	{0x3058, 0x09},
	{0x3059, 0x00},
	{0x305A, 0x00},
	{0x305C, 0x12},
	{0x305D, 0x00},
	{0x305E, 0x00},
	{0x3060, 0xE8},
	{0x3061, 0x00},
	{0x3062, 0x00},
	{0x3064, 0x09},
	{0x3065, 0x00},
	{0x3066, 0x00},
	{0x3068, 0xCE},
	{0x3069, 0x00},
	{0x306A, 0x00},
	{0x306C, 0x68},
	{0x306D, 0x06},
	{0x306E, 0x00},
	{0x3072, 0x28},
	{0x3073, 0x00},
	{0x3074, 0xB0},
	{0x3075, 0x00},
	{0x3076, 0x58},
	{0x3077, 0x0F},
	{0x3078, 0x01},
	{0x3079, 0x02},
	{0x307A, 0xFF},
	{0x307B, 0x02},
	{0x307C, 0x00},
	{0x307D, 0x00},
	{0x307E, 0x00},
	{0x307F, 0x00},
	{0x3080, 0x01},
	{0x3081, 0x02},
	{0x3082, 0xFF},
	{0x3083, 0x02},
	{0x3084, 0x00},
	{0x3085, 0x00},
	{0x3086, 0x00},
	{0x3087, 0x00},
	{0x30A4, 0x33},
	{0x30A8, 0x10},
	{0x30A9, 0x04},
	{0x30AC, 0x00},
	{0x30AD, 0x00},
	{0x30B0, 0x10},
	{0x30B1, 0x08},
	{0x30B4, 0x00},
	{0x30B5, 0x00},
	{0x30B6, 0x00},
	{0x30B7, 0x00},
	{0x30C6, 0x00},
	{0x30C7, 0x00},
	{0x30CE, 0x00},
	{0x30CF, 0x00},
	{0x30D8, 0x4C},
	{0x30D9, 0x10},
	{0x30E8, 0x00},
	{0x30E9, 0x00},
	{0x30EA, 0x00},
	{0x30EB, 0x00},
	{0x30EC, 0x00},
	{0x30ED, 0x00},
	{0x30EE, 0x00},
	{0x30EF, 0x00},
	{0x3112, 0x08},
	{0x3113, 0x00},
	{0x3116, 0x08},
	{0x3117, 0x00},
	{0x314C, 0x80},
	{0x314D, 0x00},
	{0x315A, 0x06},
	{0x3167, 0x01},
	{0x3168, 0x68},
	{0x316A, 0x7E},
	{0x3199, 0x00},
	{0x319D, 0x00},
	{0x319E, 0x03},
	{0x31A0, 0x2A},
	{0x31A1, 0x00},
	{0x31D4, 0x00},
	{0x31D5, 0x00},
	{0x3288, 0x21},
	{0x328A, 0x02},
	{0x3300, 0x00},
	{0x3302, 0x32},
	{0x3303, 0x00},
	{0x3414, 0x05},
	{0x3416, 0x18},
	{0x341C, 0xff},//0xFF},
	{0x341D, 0x01},
	{0x3648, 0x01},
	{0x364A, 0x04},
	{0x364C, 0x04},
	{0x3678, 0x01},
	{0x367C, 0x31},
	{0x367E, 0x31},
	{0x3706, 0x10},
	{0x3708, 0x03},
	{0x3714, 0x02},
	{0x3715, 0x02},
	{0x3716, 0x01},
	{0x3717, 0x03},
	{0x371C, 0x3D},
	{0x371D, 0x3F},
	{0x372C, 0x00},
	{0x372D, 0x00},
	{0x372E, 0x46},
	{0x372F, 0x00},
	{0x3730, 0x89},
	{0x3731, 0x00},
	{0x3732, 0x08},
	{0x3733, 0x01},
	{0x3734, 0xFE},
	{0x3735, 0x05},
	{0x3740, 0x02},
	{0x375D, 0x00},
	{0x375E, 0x00},
	{0x375F, 0x11},
	{0x3760, 0x01},
	{0x3768, 0x1B},
	{0x3769, 0x1B},
	{0x376A, 0x1B},
	{0x376B, 0x1B},
	{0x376C, 0x1A},
	{0x376D, 0x17},
	{0x376E, 0x0F},
	{0x3776, 0x00},
	{0x3777, 0x00},
	{0x3778, 0x46},
	{0x3779, 0x00},
	{0x377A, 0x89},
	{0x377B, 0x00},
	{0x377C, 0x08},
	{0x377D, 0x01},
	{0x377E, 0x23},
	{0x377F, 0x02},
	{0x3780, 0xD9},
	{0x3781, 0x03},
	{0x3782, 0xF5},
	{0x3783, 0x06},
	{0x3784, 0xA5},
	{0x3788, 0x0F},
	{0x378A, 0xD9},
	{0x378B, 0x03},
	{0x378C, 0xEB},
	{0x378D, 0x05},
	{0x378E, 0x87},
	{0x378F, 0x06},
	{0x3790, 0xF5},
	{0x3792, 0x43},
	{0x3794, 0x7A},
	{0x3796, 0xA1},
	{0x37B0, 0x36},
	{0x3A01, 0x01},
	{0x3A04, 0x48},
	{0x3A05, 0x09},
	{0x3A18, 0x67},
	{0x3A19, 0x00},
	{0x3A1A, 0x27},
	{0x3A1B, 0x00},
	{0x3A1C, 0x27},
	{0x3A1D, 0x00},
	{0x3A1E, 0xB7},
	{0x3A1F, 0x00},
	{0x3A20, 0x2F},
	{0x3A21, 0x00},
	{0x3A22, 0x4F},
	{0x3A23, 0x00},
	{0x3A24, 0x2F},
	{0x3A25, 0x00},
	{0x3A26, 0x47},
	{0x3A27, 0x00},
	{0x3A28, 0x27},
	{0x3A29, 0x00},

	/*pattern generator*/
#ifdef IMX335_TESTPATTERN
	{0x3148, 0x10},
	{0x3280, 0x00},
	{0x329c, 0x01},
	{0x329e, 0x0b},
	{0x32a0, 0x11},/*tpg colorwidth*/
#endif
	{0x3002, 0x00},
	{0x3000, 0x00},

	{IMX335_REG_END, 0x00},/* END MARKER */
};

static struct regval_list imx335_init_regs_2592_1944_25fps_mipi[] = {
	{0x3000, 0x01},
	{0x3001, 0x00},
	{0x3002, 0x01},
	{0x3004, 0x00},
	{0x300c, 0x5b},
	{0x300d, 0x40},
	{0x3018, 0x00},
	{0x302c, 0x30},
	{0x302d, 0x00},
	{0x302e, 0x38},
	{0x302f, 0x0a},
	{0x3030, 0x18},
	{0x3031, 0x15},//1518
	{0x3032, 0x00},
	{0x3034, 0x26},
	{0x3035, 0x02},
	{0x3050, 0x00},
	{0x315a, 0x02},
	{0x316a, 0x7e},
	{0x319d, 0x00},
	{0x31a1, 0x00},
	{0x3288, 0x21},
	{0x328a, 0x02},
	{0x3414, 0x05},
	{0x3416, 0x18},
	{0x341c, 0xff},
	{0x341d, 0x01},
	{0x3648, 0x01},
	{0x364a, 0x04},
	{0x364c, 0x04},
	{0x3678, 0x01},
	{0x367c, 0x31},
	{0x367e, 0x31},
	{0x3706, 0x10},
	{0x3708, 0x03},
	{0x3714, 0x02},
	{0x3715, 0x02},
	{0x3716, 0x01},
	{0x3717, 0x03},
	{0x371c, 0x3d},
	{0x371d, 0x3f},
	{0x372c, 0x00},
	{0x372d, 0x00},
	{0x372e, 0x46},
	{0x372f, 0x00},
	{0x3730, 0x89},
	{0x3731, 0x00},
	{0x3732, 0x08},
	{0x3733, 0x01},
	{0x3734, 0xfe},
	{0x3735, 0x05},
	{0x3740, 0x02},
	{0x375d, 0x00},
	{0x375e, 0x00},
	{0x375f, 0x11},
	{0x3760, 0x01},
	{0x3768, 0x1b},
	{0x3769, 0x1b},
	{0x376a, 0x1b},
	{0x376b, 0x1b},
	{0x376c, 0x1a},
	{0x376d, 0x17},
	{0x376e, 0x0f},
	{0x3776, 0x00},
	{0x3777, 0x00},
	{0x3778, 0x46},
	{0x3779, 0x00},
	{0x377a, 0x89},
	{0x377b, 0x00},
	{0x377c, 0x08},
	{0x377d, 0x01},
	{0x377e, 0x23},
	{0x377f, 0x02},
	{0x3780, 0xd9},
	{0x3781, 0x03},
	{0x3782, 0xf5},
	{0x3783, 0x06},
	{0x3784, 0xa5},
	{0x3788, 0x0f},
	{0x378a, 0xd9},
	{0x378b, 0x03},
	{0x378c, 0xeb},
	{0x378d, 0x05},
	{0x378e, 0x87},
	{0x378f, 0x06},
	{0x3790, 0xf5},
	{0x3792, 0x43},
	{0x3794, 0x7a},
	{0x3796, 0xa1},
	{0x3a01, 0x01},
	{0x3002, 0x00},
	{0x3000, 0x00},

	{IMX335_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the imx335_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting imx335_win_sizes[] = {
	/* 2592*1944 */
	{
		.width		= 2592,
		.height		= 1944,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= imx335_init_regs_2592_1944_15fps_mipi,
	},//[0]
	/* 2592*1944 */
	{
		.width		= 2592,
		.height		= 1944,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= imx335_init_regs_2592_1944_25fps_mipi,
	}//[1]

};
static struct tx_isp_sensor_win_setting *wsize = &imx335_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list imx335_stream_on_mipi[] = {
	{0x3000, 0x00},
	{IMX335_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list imx335_stream_off_mipi[] = {
	{0x3000, 0x01},
	{IMX335_REG_END, 0x00},	/* END MARKER */
};

int imx335_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	int ret;
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

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int imx335_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
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

static int imx335_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != IMX335_REG_END) {
		if (vals->reg_num == IMX335_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx335_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
static int imx335_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != IMX335_REG_END) {
		if (vals->reg_num == IMX335_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx335_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int imx335_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int imx335_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;


	ret = imx335_read(sd, 0x3112, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX335_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = imx335_read(sd, 0x3113, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX335_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int imx335_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shr0 = 0;
	unsigned short vmax = 0;

	vmax = imx335_attr.total_height;
	shr0 = vmax - value;
	ret = imx335_write(sd, 0x3058, (unsigned char)(shr0 & 0xff));
	ret += imx335_write(sd, 0x3059, (unsigned char)((shr0 >> 8) & 0xff));
	ret += imx335_write(sd, 0x305a, (unsigned char)((shr0 >> 16) & 0x0f));
	if (0 != ret) {
		ISP_ERROR("err: imx335_write err\n");
		return ret;
	}

	return 0;
}

static int imx335_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = imx335_write(sd, 0x30e8, (unsigned char)(value & 0xff));
	ret += imx335_write(sd, 0x30e9, (unsigned char)((value >> 8) & 0x07));
	if (ret < 0)
		return ret;

	return 0;
}

static int imx335_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx335_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx335_init(struct tx_isp_subdev *sd, int enable)
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
	ret = imx335_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int imx335_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = imx335_write_array(sd, imx335_stream_on_mipi);
		pr_debug("imx335 stream on\n");

	}
	else {
		ret = imx335_write_array(sd, imx335_stream_off_mipi);
		pr_debug("imx335 stream off\n");
	}

	return ret;
}

static int imx335_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	wpclk = IMX335_SUPPORT_SCLK;
	max_fps = sensor_max_fps;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}
	ret += imx335_read(sd, 0x3035, &tmp);
	hts = tmp & 0x0f;
	ret += imx335_read(sd, 0x3034, &tmp);
	if(ret < 0)
		return -1;
	hts = (hts << 8) + tmp;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += imx335_write(sd, 0x3001, 0x01);
	ret = imx335_write(sd, 0x3032, (unsigned char)((vts & 0xf0000) >> 16));
	ret += imx335_write(sd, 0x3031, (unsigned char)((vts & 0xff00) >> 8));
	ret += imx335_write(sd, 0x3030, (unsigned char)(vts & 0xff));
	ret += imx335_write(sd, 0x3001, 0x00);
	if(ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 9;
	sensor->video.attr->integration_time_limit = vts - 9;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 9;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int imx335_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
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

static int imx335_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"imx335_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(100);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"imx335_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = imx335_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an imx335 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("imx335 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "imx335", sizeof("imx335"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int imx335_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = imx335_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = imx335_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = imx335_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = imx335_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = imx335_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(arg)
			ret = imx335_write_array(sd, imx335_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(arg)
			ret = imx335_write_array(sd, imx335_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = imx335_set_fps(sd, *(int*)arg);
		break;
	default:
		break;;
	}

	return 0;
}

static int imx335_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = imx335_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int imx335_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	imx335_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops imx335_core_ops = {
	.g_chip_ident = imx335_g_chip_ident,
	.reset = imx335_reset,
	.init = imx335_init,
	.g_register = imx335_g_register,
	.s_register = imx335_s_register,
};

static struct tx_isp_subdev_video_ops imx335_video_ops = {
	.s_stream = imx335_s_stream,
};

static struct tx_isp_subdev_sensor_ops	imx335_sensor_ops = {
	.ioctl	= imx335_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops imx335_ops = {
	.core = &imx335_core_ops,
	.video = &imx335_video_ops,
	.sensor = &imx335_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "imx335",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int imx335_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	int ret;
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
	if (((rate / 1000) % 37125) != 0) {
		struct clk *vpll;
		vpll = clk_get(NULL,"vpll");
		if (IS_ERR(vpll)) {
			pr_err("get vpll failed\n");
		} else {
			rate = clk_get_rate(vpll);
			if (((rate / 1000) % 37125) != 0) {
				clk_set_rate(vpll,891000000);
			}
			ret = clk_set_parent(sensor->mclk, vpll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}

	private_clk_set_rate(sensor->mclk, 37125000);
	private_clk_enable(sensor->mclk);

	if(sensor_max_fps == TX_SENSOR_MAX_FPS_25){
		wsize = &imx335_win_sizes[1];
		imx335_attr.total_width = 550;
		imx335_attr.total_height = 5400;
		imx335_attr.max_integration_time = 5400 - 9;
		imx335_attr.integration_time_limit = 5400 - 9;
		imx335_attr.max_integration_time_native = 5400 - 9;
	} else if(sensor_max_fps == TX_SENSOR_MAX_FPS_15){
		wsize = &imx335_win_sizes[0];
		imx335_attr.total_width = 1200;
		imx335_attr.total_height = 4125;
		imx335_attr.max_integration_time = 4125 - 9;
		imx335_attr.integration_time_limit = 4125 - 9;
		imx335_attr.max_integration_time_native = 4125 - 9;
	} else {
		ISP_ERROR("Can not support this fps!!!\n");
		return -1;
	}

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &imx335_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx335_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->imx335\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int imx335_remove(struct i2c_client *client)
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

static const struct i2c_device_id imx335_id[] = {
	{ "imx335", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx335_id);

static struct i2c_driver imx335_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "imx335",
	},
	.probe		= imx335_probe,
	.remove		= imx335_remove,
	.id_table	= imx335_id,
};

static __init int init_imx335(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init imx335 dirver.\n");
		return -1;
	}

	return private_i2c_add_driver(&imx335_driver);
}

static __exit void exit_imx335(void)
{
	private_i2c_del_driver(&imx335_driver);
}

module_init(init_imx335);
module_exit(exit_imx335);

MODULE_DESCRIPTION("A low-level driver for Sony imx335 sensor");
MODULE_LICENSE("GPL");
