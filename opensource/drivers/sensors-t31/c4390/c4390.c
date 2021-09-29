/*
 * c4390.c
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

#define C4390_CHIP_ID_H	(0x04)
#define C4390_CHIP_ID_L	(0x01)
#define C4390_REG_END		0xffff
#define C4390_REG_DELAY	0x0000
#define C4390_SUPPORT_30FPS_SCLK (126120960)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20200323a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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

struct again_lut c4390_again_lut[] = {
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

struct tx_isp_sensor_attribute c4390_attr;

unsigned int c4390_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = c4390_again_lut;
	while(lut->gain <= c4390_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == c4390_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int c4390_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute c4390_attr={
	.name = "c4390",
	.chip_id = 0x0401,
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
	.max_integration_time_native = 1766 - 4,
	.integration_time_limit = 1766 - 4,
	.total_width = 2856,
	.total_height = 1766,
	.max_integration_time = 1766 - 4,
	.one_line_expr_in_us = 23,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = c4390_alloc_again,
	.sensor_ctrl.alloc_dgain = c4390_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};


static struct regval_list c4390_init_regs_2k_25fps_mipi[] = {
	{0x32aa,0x05},
	{0x32ab,0x08},
	{0x32ac,0xdf},//ADC range 750 mv
	{0x3211,0x10},
	{0x3290,0xa7}, //aoff
	{0x3182,0x40},
	{0x3292,0x00},
	{0x3296,0x10}, //hvdd
	{0x3291,0x04},
	{0x3287,0x53},
	{0x3215,0x1f}, //line length adjust with timing
	{0x3280,0x8c},
	{0x32c8,0x22}, //dmirror
	{0x32ca,0x22}, //channel merge
	{0x3d1f,0x22},
	{0x3d21,0x22},
	{0x3885,0x22}, //mfifo
	{0x3607,0x22},
	{0x3605,0x22},
	{0x3288,0x50}, //tm
	{0x0401,0x3b},
	{0x0403,0x00},
	{0x3584,0x02},
	{0x3087,0x07},
	{0x0340,0x08}, //20fps: 0x08 vts
	{0x0341,0xa0}, //20fps: 0xa4  1766
	/* {0x0340,0x05}, //20fps: 0x08 vts */
	/* {0x0341,0xc0}, //20fps: 0xa4  1472 */
	{0x0342,0x0b}, //hts
	{0x0343,0x28}, //default 08  2856
	{0x3180,0x20},
	{0x3187,0x04},
	{0x3114,0x4a},
	{0x3115,0x00},
	{0x3126,0x04},
	{0x3c01,0x13},
	{0x3584,0x22},
	{0x308c,0x70},
	{0x308d,0x71},
	{0x3403,0x00},
	{0x3407,0x05},
	{0x3410,0x04},
	{0x3414,0x05},
	{0x3600,0x08},
	{0x3500,0x10},
	{0x3584,0x02},
	{0x3411,0x08},
	{0x3412,0x09},
	{0x3415,0x01},
	{0x3416,0x01},
	{0xe060,0x31},
	{0xe061,0x12},
	{0xe062,0xe4},
	{0xe06c,0x31},
	{0xe06d,0x12},
	{0xe06e,0xe8},
	{0x0400,0x47},
	{0x0404,0x05},
	{0x0405,0x00},
	{0x0406,0x05},
	{0x0407,0x00},
	{0xe000,0x31},
	{0xe001,0x0a},
	{0xe002,0x88},
	{0xe003,0x31},
	{0xe004,0x0b},
	{0xe005,0x88},
	{0xe006,0x31},
	{0xe007,0x0c},
	{0xe008,0x88},
	{0xe009,0x31},
	{0xe00a,0x0d},
	{0xe00b,0x88},
	{0xe00c,0x32},
	{0xe00d,0xac},
	{0xe00e,0xdf},
	{0xe030,0x31},
	{0xe031,0x0a},
	{0xe032,0x90},
	{0xe033,0x31},
	{0xe034,0x0b},
	{0xe035,0x90},
	{0xe036,0x31},
	{0xe037,0x0c},
	{0xe038,0xa0},
	{0xe039,0x31},
	{0xe03a,0x0d},
	{0xe03b,0x90},
	{0xe03c,0x32},
	{0xe03d,0xac},
	{0xe03e,0xdb},
	{0x3500,0x00},
	{0x3584,0x22},
	{0x3108,0xcf},
	{0x3112,0xe4},
	{0x3113,0xfb},
	{0x3181,0x50},
	{0x300b,0x10},
	{0x0309,0x10},
	{0x0307,0x4f}, //default 0x4e
	{0x0304,0x00}, //05: 15fps, 00: 30fps
	{0x3880,0x00},
	{0x3881,0x08},
	{0x3805,0x07},

	{0x3806,0x06},
	{0x3807,0x06},
	{0x3808,0x14},
	{0x3809,0x85},
	{0x380a,0x6d},
	{0x380b,0xaa},

	{0x0101,0x01}, /* default mirror on */
	{0x3009,0x03}, /* default startx pos */
	{0x3009,0x10}, /* default starty pos */
	{0x0100,0x01}, //stream on

	{C4390_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting c4390_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= c4390_init_regs_2k_25fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &c4390_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list c4390_stream_on[] = {
	{0x0100, 0x01},
	{C4390_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list c4390_stream_off[] = {
	{0x0100, 0x00},
	{C4390_REG_END, 0x00},	/* END MARKER */
};

int c4390_read(struct tx_isp_subdev *sd,  uint16_t reg,
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

int c4390_write(struct tx_isp_subdev *sd, uint16_t reg,
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

static int c4390_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != C4390_REG_END) {
		if (vals->reg_num == C4390_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = c4390_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}

	return 0;
}

static int c4390_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != C4390_REG_END) {
		if (vals->reg_num == C4390_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = c4390_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int c4390_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int c4390_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = c4390_read(sd, 0x0000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != C4390_CHIP_ID_H)
		return -ENODEV;
	ret = c4390_read(sd, 0x0001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != C4390_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int c4390_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = c4390_write(sd, 0x0203, value & 0xff);
	ret += c4390_write(sd, 0x0202, value >> 8);
	if (ret < 0) {
		ISP_ERROR("c4390_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int c4390_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = c4390_write(sd, 0x0205, value);
	if (ret < 0) {
		ISP_ERROR("c4390_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int c4390_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int c4390_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int c4390_init(struct tx_isp_subdev *sd, int enable)
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
	ret = c4390_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int c4390_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = c4390_write_array(sd, c4390_stream_on);
		pr_debug("c4390 stream on\n");
	} else {
		ret = c4390_write_array(sd, c4390_stream_off);
		pr_debug("c4390 stream off\n");
	}

	return ret;
}

static int c4390_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	wpclk = C4390_SUPPORT_30FPS_SCLK;
	max_fps = SENSOR_OUTPUT_MAX_FPS;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}
	ret += c4390_read(sd, 0x0342, &tmp);
	hts = tmp;
	ret += c4390_read(sd, 0x0343, &tmp);
	if(ret < 0)
		return -1;
	hts = (hts << 8) + tmp;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = c4390_write(sd, 0x0340, (unsigned char)((vts & 0xff00) >> 8));
	ret += c4390_write(sd, 0x0341, (unsigned char)(vts & 0xff));
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

static int c4390_set_mode(struct tx_isp_subdev *sd, int value)
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

static int c4390_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"c4390_reset");
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
		ret = private_gpio_request(pwdn_gpio,"c4390_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = c4390_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an c4390 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("c4390 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "c4390", sizeof("c4390"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int c4390_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;
	unsigned char startx = 0;
	unsigned char starty = 0;

	val &= 0xfc;
	switch(enable){
	case 0:
		sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		startx = 0x03;
		starty = 0x10;
		break;
	case 1:
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGBRG10_1X10;
		startx = 0x04;
		starty = 0x10;
		break;
	case 2:
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGRBG10_1X10;
		startx = 0x03;
		starty = 0x0f;
		break;
	case 3:
		sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		startx = 0x04;
		starty = 0x0f;
		break;
	default:
		ISP_ERROR("Sensor Can Not Support This HV flip mode!!!\n");
	}

	sensor->video.mbus_change = 0;
	val |= (enable ^ 0x01);/* c4390,default mirror on */
	ret += c4390_write(sd, 0x0101, val);
	ret += c4390_write(sd, 0x3009, startx);
	ret += c4390_write(sd, 0x300b, starty);

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int c4390_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = c4390_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = c4390_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = c4390_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = c4390_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = c4390_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = c4390_write_array(sd, c4390_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = c4390_write_array(sd, c4390_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = c4390_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = c4390_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int c4390_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = c4390_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int c4390_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	c4390_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops c4390_core_ops = {
	.g_chip_ident = c4390_g_chip_ident,
	.reset = c4390_reset,
	.init = c4390_init,
	/*.ioctl = c4390_ops_ioctl,*/
	.g_register = c4390_g_register,
	.s_register = c4390_s_register,
};

static struct tx_isp_subdev_video_ops c4390_video_ops = {
	.s_stream = c4390_s_stream,
};

static struct tx_isp_subdev_sensor_ops	c4390_sensor_ops = {
	.ioctl	= c4390_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops c4390_ops = {
	.core = &c4390_core_ops,
	.video = &c4390_video_ops,
	.sensor = &c4390_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "c4390",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int c4390_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

//	c4390_attr.dvp.gpio = sensor_gpio_func;

	/*
	  convert sensor-gain into isp-gain,
	*/

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &c4390_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &c4390_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->c4390\n");

	return 0;

err_get_mclk:
	kfree(sensor);

	return -1;
}

static int c4390_remove(struct i2c_client *client)
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

static const struct i2c_device_id c4390_id[] = {
	{ "c4390", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, c4390_id);

static struct i2c_driver c4390_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "c4390",
	},
	.probe		= c4390_probe,
	.remove		= c4390_remove,
	.id_table	= c4390_id,
};

static __init int init_c4390(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init c4390 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&c4390_driver);
}

static __exit void exit_c4390(void)
{
	private_i2c_del_driver(&c4390_driver);
}

module_init(init_c4390);
module_exit(exit_c4390);

MODULE_DESCRIPTION("A low-level driver for c4390 sensors");
MODULE_LICENSE("GPL");
