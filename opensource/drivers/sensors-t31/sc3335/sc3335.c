/*
 * sc3335.c
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

#define SC3335_CHIP_ID_H	(0xcc)
#define SC3335_CHIP_ID_L	(0x1a)
#define sc3335_REG_END		0xffff
#define sc3335_REG_DELAY	0xfffe
#define sc3335_SUPPORT_30FPS_SCLK (101250000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20200413a"

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

struct again_lut sc3335_again_lut[] = {
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
	{0x758, 95645},
	{0x75a, 97786},
	{0x75c, 99848},
	{0x75e, 101897},
	{0x760, 103872},
	{0x762, 105837},
	{0x764, 107731},
	{0x766, 109618},
	{0x768, 111440},
	{0x76a, 113255},
	{0x76c, 115008},
	{0x76e, 116756},
	{0x770, 118446},
	{0x772, 120133},
	{0x774, 121764},
	{0x776, 123393},
	{0x778, 124969},
	{0x77a, 126545},
	{0x77c, 128070},
	{0x77e, 129595},
	{0xf40, 131072},
	{0xf42, 133981},
	{0xf44, 136803},
	{0xf46, 139544},
	{0xf48, 142208},
	{0xf4a, 144798},
	{0xf4c, 147320},
	{0xf4e, 149776},
	{0xf50, 152169},
	{0xf52, 154504},
	{0xf54, 156782},
	{0xf56, 159007},
	{0xf58, 161181},
	{0xf5a, 163306},
	{0xf5c, 165384},
	{0xf5e, 167417},
	{0xf60, 169408},
	{0xf62, 171357},
	{0xf64, 173267},
	{0xf66, 175140},
	{0xf68, 176976},
	{0xf6a, 178777},
	{0xf6c, 180544},
	{0xf6e, 182279},
	{0xf70, 183982},
	{0xf72, 185656},
	{0xf74, 187300},
	{0xf76, 188916},
	{0xf78, 190505},
	{0xf7a, 192068},
	{0xf7c, 193606},
	{0xf7e, 195119},
	{0x1f40, 196608},
	{0x1f42, 199517},
	{0x1f44, 202339},
	{0x1f46, 205080},
	{0x1f48, 207744},
	{0x1f4a, 210334},
	{0x1f4c, 212856},
	{0x1f4e, 215312},
	{0x1f50, 217705},
	{0x1f52, 220040},
	{0x1f54, 222318},
	{0x1f56, 224543},
	{0x1f58, 226717},
	{0x1f5a, 228842},
	{0x1f5c, 230920},
	{0x1f5e, 232953},
	{0x1f60, 234944},
	{0x1f62, 236893},
	{0x1f64, 238803},
	{0x1f66, 240676},
	{0x1f68, 242512},
	{0x1f6a, 244313},
	{0x1f6c, 246080},
	{0x1f6e, 247815},
	{0x1f70, 249518},
	{0x1f72, 251192},
	{0x1f74, 252836},
	{0x1f76, 254452},
	{0x1f78, 256041},
	{0x1f7a, 257604},
	{0x1f7c, 259142},
	{0x1f7e, 260655},
};

struct tx_isp_sensor_attribute sc3335_attr;

unsigned int sc3335_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc3335_again_lut;
	while(lut->gain <= sc3335_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc3335_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc3335_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sc3335_mipi={
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

struct tx_isp_sensor_attribute sc3335_attr={
	.name = "sc3335",
	.chip_id = 0xcc1a,
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
	.total_width = 2500,
	.total_height = 1620,
	.max_integration_time = 1620 - 4,
	.one_line_expr_in_us = 25,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc3335_alloc_again,
	.sensor_ctrl.alloc_dgain = sc3335_alloc_dgain,
};


static struct regval_list sc3335_init_regs_2304_1296_25fps_mipi[] = {
	/*Version: V01P10_20200409B*/
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3333, 0x10},
	{0x391f, 0x18},
	{0x3908, 0x82},
	{0x3f09, 0x48},
	{0x3310, 0x06},
	{0x3302, 0x10},
	{0x3320, 0x09},
	{0x33ac, 0x0c},
	{0x3309, 0x60},
	{0x331f, 0x59},
	{0x3622, 0x16},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x5799, 0x00},
	{0x3364, 0x17},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3301, 0x04},
	{0x3393, 0x0b},
	{0x3394, 0x16},
	{0x3395, 0x16},
	{0x369c, 0x18},
	{0x369d, 0x38},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3677, 0x84},
	{0x3678, 0x85},
	{0x3679, 0x86},
	{0x3670, 0x0e},
	{0x367c, 0x18},
	{0x367d, 0x38},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x68},
	{0x3304, 0x40},
	{0x331e, 0x39},
	{0x33ae, 0x1c},
	{0x4505, 0x08},
	{0x3637, 0x24},
	{0x3253, 0x04},
	{0x363a, 0x1f},
	{0x3314, 0x96},
	{0x4509, 0x20},
	{0x330b, 0xb6},
	{0x3306, 0x38},
	{0x36ea, 0x71},
	{0x36ed, 0x24},
	{0x36fa, 0x71},
	{0x36fd, 0x27},
	{0x301f, 0x04},
	{0x320c, 0x04},
	{0x320d, 0xe2},
	{0x320e, 0x06},
	{0x320f, 0x54},/*vts25fps*/
	{0x3e01, 0xa8},
	{0x3e02, 0x40},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x363c, 0x05},
	{0x36e9, 0x29},
	{0x36f9, 0x29},
	{0x0100, 0x01},
	{sc3335_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3335_init_regs_2304_1296_25fps_dvp[] = {
	{sc3335_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc3335_win_sizes[] = {
	/* resolution 2304*1296 */
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc3335_init_regs_2304_1296_25fps_mipi,
	},
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc3335_init_regs_2304_1296_25fps_dvp,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sc3335_win_sizes[0];

static enum v4l2_mbus_pixelcode sc3335_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sc3335_stream_on_dvp[] = {
	{0x0100, 0x01},
	{sc3335_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3335_stream_off_dvp[] = {
	{0x0100, 0x00},
	{sc3335_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3335_stream_on_mipi[] = {
	{0x0100, 0x01},
	{sc3335_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3335_stream_off_mipi[] = {
	{0x0100, 0x00},
	{sc3335_REG_END, 0x00},	/* END MARKER */
};

int sc3335_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc3335_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int sc3335_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != sc3335_REG_END) {
		if (vals->reg_num == sc3335_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc3335_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
static int sc3335_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != sc3335_REG_END) {
		if (vals->reg_num == sc3335_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc3335_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc3335_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc3335_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc3335_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC3335_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc3335_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC3335_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc3335_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	sc3335_write(sd,0x3812,0x00);
	ret = sc3335_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc3335_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc3335_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	if (again < 0x740) {
		ret += sc3335_write(sd,0x363c,0x05);
	} else if (again >= 0x740 && again < 0x1f40){
		ret += sc3335_write(sd,0x363c,0x07);
	} else {
		ret += sc3335_write(sd,0x363c,0x07);
	}

	ret += sc3335_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sc3335_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));
	ret += sc3335_write(sd,0x3812,0x30);
	if (ret < 0)
		return ret;

	return 0;
}

static int sc3335_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret += sc3335_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc3335_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc3335_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc3335_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int again = value;

	/* denoise logic */
	ret = sc3335_write(sd,0x3812,0x00);
	if (again < 0x740) {
		ret += sc3335_write(sd,0x363c,0x05);
	} else if (again >= 0x740 && again < 0x1f40){
		ret += sc3335_write(sd,0x363c,0x07);
	} else {
		ret += sc3335_write(sd,0x363c,0x07);
	}
	ret += sc3335_write(sd,0x3812,0x30);

	ret += sc3335_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc3335_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc3335_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc3335_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc3335_init(struct tx_isp_subdev *sd, int enable)
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

	ret = sc3335_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc3335_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc3335_write_array(sd, sc3335_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3335_write_array(sd, sc3335_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc3335 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc3335_write_array(sd, sc3335_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3335_write_array(sd, sc3335_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc3335 stream off\n");
	}

	return ret;
}

static int sc3335_set_fps(struct tx_isp_subdev *sd, int fps)
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
	sclk = sc3335_SUPPORT_30FPS_SCLK;

	ret = sc3335_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc3335_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc3335 read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp) << 1;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sc3335_write(sd,0x3812,0x00);
	ret += sc3335_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc3335_write(sd, 0x320e, (unsigned char)(vts >> 8));
	ret += sc3335_write(sd,0x3812,0x30);
	if (0 != ret) {
		ISP_ERROR("err: sc3335_write err\n");
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

static int sc3335_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc3335_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc3335_reset");
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
		ret = private_gpio_request(pwdn_gpio,"sc3335_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc3335_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc3335 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc3335 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc3335", sizeof("sc3335"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sc3335_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc3335_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
//		if(arg)
//			ret = sc3335_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if(arg)
//			ret = sc3335_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc3335_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc3335_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc3335_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc3335_write_array(sd, sc3335_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3335_write_array(sd, sc3335_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = sc3335_write_array(sd, sc3335_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3335_write_array(sd, sc3335_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc3335_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sc3335_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc3335_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc3335_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc3335_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc3335_core_ops = {
	.g_chip_ident = sc3335_g_chip_ident,
	.reset = sc3335_reset,
	.init = sc3335_init,
	/*.ioctl = sc3335_ops_ioctl,*/
	.g_register = sc3335_g_register,
	.s_register = sc3335_s_register,
};

static struct tx_isp_subdev_video_ops sc3335_video_ops = {
	.s_stream = sc3335_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc3335_sensor_ops = {
	.ioctl	= sc3335_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc3335_ops = {
	.core = &sc3335_core_ops,
	.video = &sc3335_video_ops,
	.sensor = &sc3335_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc3335",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc3335_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
		wsize = &sc3335_win_sizes[1];
		sc3335_attr.dvp.gpio = sensor_gpio_func;
		ret = set_sensor_gpio_function(sensor_gpio_func);
		if (ret < 0)
			goto err_set_sensor_gpio;
		/* sc3335_attr.max_integration_time_native = 1498; */
		/* sc3335_attr.integration_time_limit = 1498; */
		/* sc3335_attr.total_width = 2400; */
		/* sc3335_attr.total_height = 1500; */
		/* sc3335_attr.max_integration_time = 1498; */
	} else if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI)){
		wsize = &sc3335_win_sizes[0];
		memcpy((void*)(&(sc3335_attr.mipi)),(void*)(&sc3335_mipi),sizeof(sc3335_mipi));
	} else {
		ISP_ERROR("Can not support this data interface!!!\n");
		goto err_set_sensor_data_interface;
	}

	sc3335_attr.dbus_type = data_interface;

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sc3335_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc3335_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc3335\n");

	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sc3335_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc3335_id[] = {
	{ "sc3335", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc3335_id);

static struct i2c_driver sc3335_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc3335",
	},
	.probe		= sc3335_probe,
	.remove		= sc3335_remove,
	.id_table	= sc3335_id,
};

static __init int init_sc3335(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc3335 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc3335_driver);
}

static __exit void exit_sc3335(void)
{
	private_i2c_del_driver(&sc3335_driver);
}

module_init(init_sc3335);
module_exit(exit_sc3335);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc3335 sensors");
MODULE_LICENSE("GPL");
