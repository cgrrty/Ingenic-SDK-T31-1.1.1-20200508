/*
 * imx307.c
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

#define IMX307_CHIP_ID_H	(0xA0)
#define IMX307_CHIP_ID_L	(0xB2)
#define IMX307_REG_END		0xffff
#define IMX307_REG_DELAY	0xfffe

#define IMX307_SUPPORT_PCLK (74250*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 10
#define AGAIN_MAX_DB 0x50
#define DGAIN_MAX_DB 0x3c
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION	"H20180402a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
unsigned int imx307_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
}

unsigned int imx307_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	*sensor_dgain = 0;

	return 0;
}

struct tx_isp_sensor_attribute imx307_attr = {
	.name = "imx307",
	.chip_id = 0xa0b2,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_SONY_MODE,
		.clk = 445,
		.lans = 2,
		.settle_time_apative_en = 0,
		.image_twidth = 1948,
		.image_theight = 1097,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.mipi_crop_start0x = 12,
		.mipi_sc.mipi_crop_start0y = 20,
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
	},
	.max_again = 458752,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1125-2,
	.integration_time_limit = 1125-2,
	.total_width = 2200,
	.total_height = 1125,
	.max_integration_time = 1125-2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = imx307_alloc_again,
	.sensor_ctrl.alloc_dgain = imx307_alloc_dgain,
};


static struct regval_list imx307_init_regs_1920_1080_25fps[] = {
	/* inclk 37.25M clk 1080p@30fps Sunnic */
	{0x3000, 0x01},
	{0x3001, 0x01},
	{0x3002, 0x01},
	{IMX307_REG_DELAY, 0x18},
	{0x3005, 0x01},
	{0x3007, 0x00},
	{0x3009, 0x12},
	{0x300A, 0xF0},
	{0x300B, 0x00},
	{0x3011, 0x0A},
	{0x3012, 0x64},
	{0x3014, 0x00},
	{0x3018, 0x65},
	{0x3019, 0x04},/*vts 30fps*/
	{0x301A, 0x00},
	{0x301C, 0x30},
	{0x301D, 0x11},
	{0x3020, 0xFE},
	{0x3021, 0x03},
	{0x3022, 0x00},
	{0x3046, 0x01},
	{0x3048, 0x00},
	{0x3049, 0x08},
	{0x304B, 0x0A},
	{0x305C, 0x18},
	{0x305D, 0x03},
	{0x305E, 0x20},
	{0x305F, 0x01},
	{0x309E, 0x4A},
	{0x309F, 0x4A},
	{0x311C, 0x0E},
	{0x3128, 0x04},
	{0x3129, 0x00},
	{0x313B, 0x41},
	{0x315E, 0x1A},
	{0x3164, 0x1A},
	{0x317C, 0x00},
	{0x31EC, 0x0E},
	{0x3405, 0x10},
	{0x3407, 0x01},
	{0x3414, 0x0A},
	{0x3418, 0x49},// Y_OUT_SIZE
	{0x3419, 0x04},
	{0x3441, 0x0C},
	{0x3442, 0x0C},
	{0x3443, 0x01},
	{0x3444, 0x20},
	{0x3445, 0x25},
	{0x3446, 0x57},
	{0x3447, 0x00},
	{0x3448, 0x37},
	{0x3449, 0x00},
	{0x344A, 0x24},//HS_PREPARE //0x1f  24
	{0x344B, 0x00},
	{0x344C, 0x1F},
	{0x344D, 0x00},
	{0x344E, 0x1F},
	{0x344F, 0x00},
	{0x3450, 0x77},
	{0x3451, 0x00},
	{0x3452, 0x1F},
	{0x3453, 0x00},
	{0x3454, 0x17},
	{0x3455, 0x00},
	{0x3472, 0x9C},//X_OUT_SIZE
	{0x3473, 0x07},
	{0x346a, 0x9c},//EBD
	{0x346b, 0x07},
	{0x3480, 0x49},
	{IMX307_REG_DELAY, 0x18},
	{0x3001, 0x00},
	{0x3002, 0x00},
//	{0x3000, 0x01},
	{IMX307_REG_DELAY, 0xfe},
	{0x3000, 0x00},

//1080p@30fps Ryan
#if 0
	{0x3000, 0x01},
	{0x3001, 0x01},
	{0x3002, 0x01},
	{IMX307_REG_DELAY, 0x18},
	{0x3005, 0x01},
	{0x3405, 0x10},
	{0x3007, 0x00},
	{0x3407, 0x01},
	{0x3009, 0x02},
	{0x300a, 0xf0},
	{0x3011, 0x0a},
	{0x3414, 0x0a},
	{0x3018, 0x65},//VMAX
	{0x3019, 0x04},
	{0x3418, 0x48},
	{0x3419, 0x04},
	{0x301c, 0x30},//HMAX
	{0x301d, 0x11},
	{0x311c, 0x0e},
	{0x3128, 0x04},
	{0x3129, 0x00},
	{0x313b, 0x41},
	{0x3441, 0x0C},
	{0x3442, 0x0C},
	{0x3443, 0x01},
	{0x3444, 0x20},
	{0x3445, 0x25},
	{0x3446, 0x57},
	{0x3046, 0x01},
	{0x3447, 0x00},
	{0x3448, 0x37},//THSZERO
	{0x3449, 0x00},
	{0x344a, 0x1f},//THSPREPARE
	{0x344b, 0x00},
	{0x304b, 0x0a},
	{0x344c, 0x1f},
	{0x344d, 0x00},
	{0x344e, 0x1f},//0x22
	{0x344f, 0x00},
	{0x3450, 0x77},
	{0x3451, 0x00},
	{0x3452, 0x1f},
	{0x3453, 0x00},
	{0x3454, 0x17},
	{0x3455, 0x00},
	{0x305c, 0x18},
	{0x305d, 0x03},
	{0x305e, 0x20},
	{0x315e, 0x1a},
	{0x305f, 0x01},
	{0x3164, 0x1a},
	{0x3472, 0x9c},
	{0x3473, 0x07},
	{0x317c, 0x00},
	{0x3480, 0x49},
	{0x309e, 0x4a},
	{0x309f, 0x4a},
	{0x31ec, 0x0e},
	{IMX307_REG_DELAY, 0x18},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3000, 0x01},
#endif

	{IMX307_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the imx307_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting imx307_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= imx307_init_regs_1920_1080_25fps,
	}
};

static enum v4l2_mbus_pixelcode imx307_mbus_code[] = {
	V4L2_MBUS_FMT_SRGGB12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list imx307_stream_on[] = {
	{0x3000, 0x00},
	{IMX307_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list imx307_stream_off[] = {
	{0x3000, 0x01},
	{IMX307_REG_END, 0x00},	/* END MARKER */
};

int imx307_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int imx307_write(struct tx_isp_subdev *sd, uint16_t reg,
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

static int imx307_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != IMX307_REG_END) {
		if (vals->reg_num == IMX307_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx307_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
			ISP_WARNING("{0x%0x, 0x%02x}\n",vals->reg_num, val);
		}
		vals++;
	}

	return 0;
}

static int imx307_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != IMX307_REG_END) {
		if (vals->reg_num == IMX307_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx307_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int imx307_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int imx307_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = imx307_read(sd, 0x3008, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX307_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = imx307_read(sd, 0x301e, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX307_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int imx307_set_integration_time_short(struct tx_isp_subdev *sd, int int_time)
{
	return 0;
}

static int imx307_set_integration_time(struct tx_isp_subdev *sd, int int_time)
{
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;
	vmax = imx307_attr.total_height;
	shs = vmax - int_time - 2;
	ret = imx307_write(sd, 0x3020, (unsigned char)(shs & 0xff));
	ret += imx307_write(sd, 0x3021, (unsigned char)((shs >> 8) & 0xff));
	ret += imx307_write(sd, 0x3022, (unsigned char)((shs >> 16) & 0x3));
	if (0 != ret) {
		printk("err: imx307_write err\n");
		return ret;
	}

	return 0;
}

static int imx307_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = imx307_write(sd, 0x3014, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int imx307_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx307_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx307_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx307_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &imx307_win_sizes[0];
	int ret = 0;
	if(!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = imx307_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int imx307_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = imx307_write_array(sd, imx307_stream_on);
		pr_debug("imx307 stream on\n");
	}
	else {
		ret = imx307_write_array(sd, imx307_stream_off);
		pr_debug("imx307 stream off\n");
	}

	return ret;
}

static int imx307_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int pclk = 0;
	unsigned short hmax = 0;
	unsigned short vmax = 0;
	unsigned short cur_int = 0;
	unsigned short shs = 0;
	unsigned char value = 0;
	unsigned int newformat = 0; //the format is 24.8

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	pclk = IMX307_SUPPORT_PCLK;

#if 0
	/*method 1 change hts*/
	ret = imx307_read(sd, 0x3018, &value);
	vmax = value;
	ret += imx307_read(sd, 0x3019, &value);
	vmax |= value << 8;
	ret += imx307_read(sd, 0x301a, &value);
	vmax |= (value|0x3) << 16;

	hmax = ((pclk << 4) / (vmax * (newformat >> 4))) << 1;
	ret += imx307_write(sd, 0x301c, hmax & 0xff);
	ret += imx307_write(sd, 0x301d, (hmax >> 8) & 0xff);
	if (0 != ret) {
		printk("err: imx307_write err\n");
		return ret;
	}
	sensor->video.attr->total_width = hmax >> 1;
#endif

	/*method 2 change vts*/
	ret = imx307_read(sd, 0x301c, &value);
	hmax = value;
	ret += imx307_read(sd, 0x301d, &value);
	hmax = ((value << 8) | hmax) >> 1;

	vmax = ((pclk << 4) / (hmax * (newformat >> 4)));
	ret += imx307_write(sd, 0x3018, vmax & 0xff);
	ret += imx307_write(sd, 0x3019, (vmax >> 8) & 0xff);
	ret += imx307_write(sd, 0x301a, (vmax >> 16) & 0x03);

	/*record current integration time*/
	ret = imx307_read(sd, 0x3020, &value);
	shs = value;
	ret += imx307_read(sd, 0x3021, &value);
	shs = (value << 8) | shs;
	ret += imx307_read(sd, 0x3022, &value);
	shs = ((value & 0x03) << 16) | shs;
	cur_int = sensor->video.attr->total_height - shs -2;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vmax - 2;
	sensor->video.attr->integration_time_limit = vmax - 2;
	sensor->video.attr->total_height = vmax;
	sensor->video.attr->max_integration_time = vmax - 2;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	ret = imx307_set_integration_time(sd,cur_int);
	if(ret < 0)
		return -1;

	return ret;
}

static int imx307_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &imx307_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &imx307_win_sizes[0];
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

static int imx307_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"imx307_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(50);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = gpio_request(pwdn_gpio,"imx307_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(15);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = imx307_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an imx307 chip.\n",
		       client->addr, client->adapter->name);
		return ret;
	}
	printk("imx307 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "imx307", sizeof("imx307"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int imx307_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = imx307_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = imx307_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = imx307_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = imx307_set_analog_gain_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = imx307_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = imx307_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = imx307_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(arg)
			ret = imx307_write_array(sd, imx307_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(arg)
			ret = imx307_write_array(sd, imx307_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = imx307_set_fps(sd, *(int*)arg);
		break;
	default:
		break;;
	}

	return 0;
}

static int imx307_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = imx307_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int imx307_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	imx307_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops imx307_core_ops = {
	.g_chip_ident = imx307_g_chip_ident,
	.reset = imx307_reset,
	.init = imx307_init,
	.g_register = imx307_g_register,
	.s_register = imx307_s_register,
};

static struct tx_isp_subdev_video_ops imx307_video_ops = {
	.s_stream = imx307_s_stream,
};

static struct tx_isp_subdev_sensor_ops	imx307_sensor_ops = {
	.ioctl	= imx307_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops imx307_ops = {
	.core = &imx307_core_ops,
	.video = &imx307_video_ops,
	.sensor = &imx307_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "imx307",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int imx307_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &imx307_win_sizes[0];
	int ret;
	unsigned long rate = 0;

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

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &imx307_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx307_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->imx307\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int imx307_remove(struct i2c_client *client)
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

static const struct i2c_device_id imx307_id[] = {
	{ "imx307", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx307_id);

static struct i2c_driver imx307_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "imx307",
	},
	.probe		= imx307_probe,
	.remove		= imx307_remove,
	.id_table	= imx307_id,
};

static __init int init_imx307(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to init imx307 dirver.\n");
		return -1;
	}

	return private_i2c_add_driver(&imx307_driver);
}

static __exit void exit_imx307(void)
{
	i2c_del_driver(&imx307_driver);
}

module_init(init_imx307);
module_exit(exit_imx307);

MODULE_DESCRIPTION("A low-level driver for Sony imx307 sensors");
MODULE_LICENSE("GPL");
