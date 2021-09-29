/*
 * imx327.c
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

#define IMX327_CHIP_ID_H	(0xb2)
#define IMX327_CHIP_ID_L	(0x01)
#define IMX327_REG_END		0xffff
#define IMX327_REG_DELAY	0xfffe
#define IMX327_SUPPORT_SCLK (148500000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20190822"
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

static int wdr_bufsize = 230400;//cache lines corrponding on VPB1
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

static int rhs1 = 101;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct tx_isp_sensor_attribute imx327_attr;

unsigned int imx327_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
}

unsigned int imx327_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
}

unsigned int imx327_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus mipi_2dol_lcg = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 891,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 1952,
	.image_theight = 1109,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 1,
	.mipi_sc.mipi_crop_start0x = 16,
	.mipi_sc.mipi_crop_start0y = 12,
	.mipi_sc.mipi_crop_start1x = 16,
	.mipi_sc.mipi_crop_start1y = 62,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 1,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_NOT_VC_MODE,
};

struct tx_isp_mipi_bus mipi_linear = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 445,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 1948,
	.image_theight = 1109,
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
};

struct tx_isp_sensor_attribute imx327_attr={
	.name = "imx327",
	.chip_id = 0xb201,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_SONY_MODE,
		.clk = 445,
		.lans = 2,
		.settle_time_apative_en = 0,
		.image_twidth = 1948,
		.image_theight = 1109,
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

	.max_again = 589824,
	.max_again_short = 589824,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1176,
	.min_integration_time_short = 1,
	.max_integration_time_short = 98,
	.integration_time_limit = 1176,
	.total_width = 2136,
	.total_height = 2781,
	.max_integration_time = 1176,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = imx327_alloc_again,
	.sensor_ctrl.alloc_again_short = imx327_alloc_again_short,
	.sensor_ctrl.alloc_dgain = imx327_alloc_dgain,
	.wdr_cache = 0,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list imx327_init_regs_1920_1080_30fps_mipi_2dol_lcg[] = {
//add
	{0x3000, 0x01},//standy
	{0x3001, 0x01},
	{0x3002, 0x01},//Master stop
//	{IMX327_REG_DELAY, 0x18},
//	{0x3002, 0x00},
	{0x3005, 0x01},//ADBIT 01:12bit
	{0x3007, 0x40},//WINMODE[6:4]
	{0x3009, 0x01},//FRSEL[1:0]
	{0x300a, 0xf0},
	{0x300c, 0x11},//WDMODE[0] WDSEL[5:4]
	{0x3010, 0x61},//FPGC gain for each gain
	{0x30f0, 0x64},//FPGC1 gain for each gain
	{0x3011, 0x02},
	{0x3018, 0x6E},//Vmax FSC=vmax*2
	{0x3019, 0x05},//0x486
	{0x301c, 0x58},//Hmax
	{0x301d, 0x08},
#if 0
	{0x3018, 0x86},//Vmax FSC=vmax*2
	{0x3019, 0x04},//0x486  1158  60.04fps
	{0x301c, 0x05},//Hmax   2136 0x858
	{0x301d, 0x0a},
#endif
	{0x3020, 0x02},//SHS1 S
	{0x3021, 0x00},
	{0x3024, 0x73},//SHS2 L
	{0x3025, 0x04},
	{0x3028, 0x00},//SHS3
	{0x3029, 0x00},
	{0x302a, 0x00},
	{0x3030, 0x65},//RHS1
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x3034, 0x00},//RHS2
	{0x3035, 0x00},
	{0x3036, 0x00},
	{0x303c, 0x04},//WINPV[7:0]
	{0x303d, 0x00},//WINPV[2:0]
	{0x303e, 0x41},//WINWV[7:0]
	{0x303f, 0x04},//WINWV[2:0]
	{0x3045, 0x05},
	{0x3046, 0x01},//ODBIT[1:0]
	{0x304b, 0x0a},
	{0x305c, 0x18},
	{0x305d, 0x03},
	{0x305e, 0x20},
	{0x305f, 0x01},
	{0x309e, 0x4a},
	{0x309f, 0x4a},
	{0x30d2, 0x19},
	{0x30d7, 0x03},
	{0x3106, 0x11},
	{0x3129, 0x00},
	{0x313b, 0x61},
	{0x315e, 0x1a},
	{0x3164, 0x1a},
#if 0
	{0x31a0, 0xbc},
	{0x31a1, 0x00},

#endif
	{0x317c, 0x00},
	{0x31ec, 0x0e},
	{0x3204, 0x4a},
	{0x3209, 0xf0},
	{0x320a, 0x22},
	{0x3344, 0x38},
	{0x3405, 0x00},
	{0x3407, 0x01},
	{0x3414, 0x00},//OPB_SIZE_V[5:0]
	{0x3415, 0x00},//NULL0_SIZE_V[5:0]
	{0x3418, 0x7a},//Y_OUT_SIZE[7:0]
	{0x3419, 0x09},//Y_OUT_SIZE[4:0]
	{0x3441, 0x0c},
	{0x3442, 0x0c},
	{0x3443, 0x01},
	{0x3444, 0x20},
	{0x3445, 0x25},
	{0x3446, 0x77},
	{0x3447, 0x00},
	{0x3448, 0x67},
	{0x3449, 0x00},
	{0x344a, 0x47},
	{0x344b, 0x00},
	{0x344c, 0x37},
	{0x344d, 0x00},
	{0x344e, 0x3f},
	{0x344f, 0x00},
	{0x3450, 0xff},
	{0x3451, 0x00},
	{0x3452, 0x3f},
	{0x3453, 0x00},
	{0x3454, 0x37},
	{0x3455, 0x00},
	{0x346a, 0x9c},//EBD
	{0x346b, 0x07},
	{0x3472, 0xa0},
	{0x3473, 0x07},
	{0x347b, 0x23},
	{0x3480, 0x49},
//	{IMX327_REG_DELAY, 0x18},
	{0x3001, 0x00},//standy cancel
	{0x3002, 0x00},//Master start
	{0x3000, 0x01},//standy cancel
	{IMX327_REG_END, 0x00},/* END MARKER */

};

static struct regval_list imx327_init_regs_1920_1080_30fps_mipi[] = {
	{0x3000, 0x01},//standy
	{0x3001, 0x01},
	{0x3002, 0x01},//Master stop
//	{IMX327_REG_DELAY, 0x18},
	{0x3005, 0x01},
	{0x3007, 0x00},
	{0x3009, 0x02},
	{0x300A, 0xF0},
	{0x300B, 0x00},
	{0x3011, 0x0A},
	{0x3012, 0x64},
	{0x3014, 0x00},
	{0x3018, 0x65},// 1125
	{0x3019, 0x04},
	{0x301A, 0x00},
	{0x301C, 0xa0},//0x1130:30fps 0x14a0:25fps
	{0x301D, 0x14},
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
	{0x30D2, 0x19},
	{0x30D7, 0x03},
	{0x3129, 0x00},
	{0x313B, 0x61},
	{0x315E, 0x1A},
	{0x3164, 0x1A},
	{0x317C, 0x00},
	{0x31EC, 0x0E},
	{0x3405, 0x10},
	{0x3407, 0x01},
	{0x3414, 0x0A},
	{0x3418, 0x49},
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
	{0x344A, 0x2F},//1f
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
	{0x346a, 0x9c},//EBD
	{0x346b, 0x07},
	{0x3472, 0x9C},
	{0x3473, 0x07},
	{0x3480, 0x49},
//	{IMX327_REG_DELAY, 0x18},
	{0x3001, 0x00},//standy cancel
	{0x3002, 0x00},//Master start
	{0x3000, 0x01},//standy cancel
	{IMX327_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the imx327_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting imx327_win_sizes[] = {
	/* 1948*1109 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= imx327_init_regs_1920_1080_30fps_mipi,
	},
	/* 1948*1109 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= imx327_init_regs_1920_1080_30fps_mipi_2dol_lcg,
	}
};

static struct tx_isp_sensor_win_setting *wsize = &imx327_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list imx327_stream_on_mipi[] = {
	{0x3000, 0x00},
	{IMX327_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list imx327_stream_off_mipi[] = {
	{0x3000, 0x01},
	{IMX327_REG_END, 0x00},	/* END MARKER */
};

int imx327_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int imx327_write(struct tx_isp_subdev *sd, uint16_t reg,
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

static int imx327_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != IMX327_REG_END) {
		if (vals->reg_num == IMX327_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx327_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
static int imx327_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != IMX327_REG_END) {
		if (vals->reg_num == IMX327_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx327_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int imx327_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int imx327_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = imx327_read(sd, 0x301e, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX327_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = imx327_read(sd, 0x301f, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX327_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int imx327_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shs1 = 0;

	//short frame use shs1
	shs1 = rhs1 - value - 1;
	ret = imx327_write(sd, 0x3020, (unsigned char)(shs1 & 0xff));
	ret += imx327_write(sd, 0x3021, (unsigned char)((shs1 >> 8) & 0xff));
	ret += imx327_write(sd, 0x3022, (unsigned char)((shs1 >> 16) & 0x3));

	return 0;
}

static int imx327_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;

	if(data_type == TX_SENSOR_DATA_TYPE_LINEAR){
		vmax = imx327_attr.total_height;
		shs = vmax - value - 1;
		ret = imx327_write(sd, 0x3020, (unsigned char)(shs & 0xff));
		ret += imx327_write(sd, 0x3021, (unsigned char)((shs >> 8) & 0xff));
		ret += imx327_write(sd, 0x3022, (unsigned char)((shs >> 16) & 0x3));
	} else {
		//long frame use shs2
		vmax = imx327_attr.total_height;
		shs = vmax - value - 1;
		ret = imx327_write(sd, 0x3024, (unsigned char)(shs & 0xff));
		ret += imx327_write(sd, 0x3025, (unsigned char)((shs >> 8) & 0xff));
		ret += imx327_write(sd, 0x3026, (unsigned char)((shs >> 16) & 0x3));
	}

	if (0 != ret) {
		ISP_ERROR("err: imx327_write err\n");
		return ret;
	}

	return 0;
}

static int imx327_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = imx327_write(sd, 0x30f2, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int imx327_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = imx327_write(sd, 0x3014, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int imx327_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx327_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx327_init(struct tx_isp_subdev *sd, int enable)
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
	ret = imx327_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int imx327_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = imx327_write_array(sd, imx327_stream_on_mipi);
		pr_debug("imx327 stream on\n");

	}
	else {
		ret = imx327_write_array(sd, imx327_stream_off_mipi);
		pr_debug("imx327 stream off\n");
	}

	return ret;
}

static int imx327_set_fps(struct tx_isp_subdev *sd, int fps)
{
	return 0;
}

static int imx327_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;
	/* struct timeval tv; */

	/* do_gettimeofday(&tv); */
	/* printk("%d:before:time is %d.%d\n", __LINE__,tv.tv_sec,tv.tv_usec); */
	ret = imx327_write(sd, 0x3000, 0x1);
	if(wdr_en == 1){
		memcpy((void*)(&(imx327_attr.mipi)),(void*)(&mipi_2dol_lcg),sizeof(mipi_2dol_lcg));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		wsize = &imx327_win_sizes[1];
		imx327_attr.data_type = data_type;
		imx327_attr.wdr_cache = wdr_bufsize;

		imx327_attr.max_again = 589824;
		imx327_attr.max_again_short = 589824;
		imx327_attr.max_dgain = 0;
		imx327_attr.min_integration_time = 1;
		imx327_attr.min_integration_time_native = 1;
		imx327_attr.max_integration_time_native = 1176;
		imx327_attr.min_integration_time_short = 1;
		imx327_attr.max_integration_time_short = 98;
		imx327_attr.integration_time_limit = 1176;
		imx327_attr.total_width = 2136;
		imx327_attr.total_height = 2781;
		imx327_attr.max_integration_time = 1176;
		imx327_attr.integration_time_apply_delay = 2;
		imx327_attr.again_apply_delay = 2;
		imx327_attr.dgain_apply_delay = 0;

		sensor->video.attr = &imx327_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0){
		memcpy((void*)(&(imx327_attr.mipi)),(void*)(&mipi_linear),sizeof(mipi_linear));
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		imx327_attr.data_type = data_type;
		wsize = &imx327_win_sizes[0];

		imx327_attr.data_type = data_type;
		imx327_attr.wdr_cache = wdr_bufsize;
		imx327_attr.max_again = 589824;
		imx327_attr.max_again_short = 589824;
		imx327_attr.max_dgain = 0;
		imx327_attr.min_integration_time = 1;
		imx327_attr.min_integration_time_native = 1;
		imx327_attr.max_integration_time_native = 1123;
		imx327_attr.min_integration_time_short = 1;
		imx327_attr.max_integration_time_short = 98;
		imx327_attr.integration_time_limit = 1123;
		imx327_attr.total_width = 5280;
		imx327_attr.total_height = 1125;
		imx327_attr.max_integration_time = 1123;
		imx327_attr.integration_time_apply_delay = 2;
		imx327_attr.again_apply_delay = 2;
		imx327_attr.dgain_apply_delay = 0;

		sensor->video.attr = &imx327_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int imx327_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;

	private_gpio_direction_output(reset_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);

	ret = imx327_write_array(sd, wsize->regs);
	ret = imx327_write_array(sd, imx327_stream_on_mipi);

	return 0;
}

static int imx327_set_mode(struct tx_isp_subdev *sd, int value)
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

static int imx327_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"imx327_reset");
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
		ret = private_gpio_request(pwdn_gpio,"imx327_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = imx327_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an imx327 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("imx327 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "imx327", sizeof("imx327"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int imx327_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = imx327_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = imx327_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = imx327_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = imx327_set_analog_gain_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = imx327_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = imx327_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = imx327_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(arg)
			ret = imx327_write_array(sd, imx327_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(arg)
			ret = imx327_write_array(sd, imx327_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = imx327_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = imx327_set_wdr(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = imx327_set_wdr_stop(sd, *(int*)arg);
		break;
	default:
		break;;
	}

	return 0;
}

static int imx327_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = imx327_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int imx327_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	imx327_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops imx327_core_ops = {
	.g_chip_ident = imx327_g_chip_ident,
	.reset = imx327_reset,
	.init = imx327_init,
	.g_register = imx327_g_register,
	.s_register = imx327_s_register,
};

static struct tx_isp_subdev_video_ops imx327_video_ops = {
	.s_stream = imx327_s_stream,
};

static struct tx_isp_subdev_sensor_ops	imx327_sensor_ops = {
	.ioctl	= imx327_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops imx327_ops = {
	.core = &imx327_core_ops,
	.video = &imx327_video_ops,
	.sensor = &imx327_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "imx327",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int imx327_probe(struct i2c_client *client,
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

	sd = &sensor->sd;
	video = &sensor->video;

	if(data_type == TX_SENSOR_DATA_TYPE_LINEAR){
		imx327_attr.data_type = data_type;
		wsize = &imx327_win_sizes[0];

		imx327_attr.max_again = 589824;
		imx327_attr.max_again_short = 589824;
		imx327_attr.max_dgain = 0;
		imx327_attr.min_integration_time = 1;
		imx327_attr.min_integration_time_native = 1;
		imx327_attr.max_integration_time_native = 1123;
		imx327_attr.min_integration_time_short = 1;
		imx327_attr.max_integration_time_short = 98;
		imx327_attr.integration_time_limit = 1123;
		imx327_attr.total_width = 5280;
		imx327_attr.total_height = 1125;
		imx327_attr.max_integration_time = 1123;
		imx327_attr.integration_time_apply_delay = 2;
		imx327_attr.again_apply_delay = 2;
		imx327_attr.dgain_apply_delay = 0;
	} else if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL){
		wsize = &imx327_win_sizes[1];
		imx327_attr.data_type = data_type;
		imx327_attr.wdr_cache = wdr_bufsize;

		imx327_attr.max_again = 589824;
		imx327_attr.max_again_short = 589824;
		imx327_attr.max_dgain = 0;
		imx327_attr.min_integration_time = 1;
		imx327_attr.min_integration_time_native = 1;
		imx327_attr.max_integration_time_native = 1176;
		imx327_attr.min_integration_time_short = 1;
		imx327_attr.max_integration_time_short = 98;
		imx327_attr.integration_time_limit = 1176;
		imx327_attr.total_width = 2136;
		imx327_attr.total_height = 2781;
		imx327_attr.max_integration_time = 1176;
		imx327_attr.integration_time_apply_delay = 2;
		imx327_attr.again_apply_delay = 2;
		imx327_attr.dgain_apply_delay = 0;
		memcpy((void*)(&(imx327_attr.mipi)),(void*)(&mipi_2dol_lcg),sizeof(mipi_2dol_lcg));
	} else {
		ISP_ERROR("Can not support this data mode!!!\n");
	}

	sensor->video.attr = &imx327_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx327_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->imx327\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int imx327_remove(struct i2c_client *client)
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

static const struct i2c_device_id imx327_id[] = {
	{ "imx327", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx327_id);

static struct i2c_driver imx327_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "imx327",
	},
	.probe		= imx327_probe,
	.remove		= imx327_remove,
	.id_table	= imx327_id,
};

static __init int init_imx327(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init imx327 dirver.\n");
		return -1;
	}

	return private_i2c_add_driver(&imx327_driver);
}

static __exit void exit_imx327(void)
{
	private_i2c_del_driver(&imx327_driver);
}

module_init(init_imx327);
module_exit(exit_imx327);

MODULE_DESCRIPTION("A low-level driver for Sony imx327 sensor");
MODULE_LICENSE("GPL");
