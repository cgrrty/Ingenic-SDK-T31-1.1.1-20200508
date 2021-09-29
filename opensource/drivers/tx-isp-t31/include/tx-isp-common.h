#ifndef __TX_ISP_COMMON_H__
#define __TX_ISP_COMMON_H__
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <media/media-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <linux/clk.h>

#include "txx-funcs.h"
#include "tx-isp-device.h"
#include "tx-isp-debug.h"

#define ISP_SUCCESS 0

/* T31 */
#define TX_ISP_INPUT_PORT_MAX_WIDTH		4096
#define TX_ISP_INPUT_PORT_MAX_HEIGHT		4096
#define TX_ISP_FR_CAHNNEL_MAX_WIDTH		2624
#define TX_ISP_FR_CAHNNEL_MAX_HEIGHT		2048
#define TX_ISP_DS1_CAHNNEL_MAX_WIDTH		1920
#define TX_ISP_DS1_CAHNNEL_MAX_HEIGHT		1080


/*****************************************************
 			sensor attributes
*****************************************************/
#define SENSOR_R_BLACK_LEVEL	0
#define SENSOR_GR_BLACK_LEVEL	1
#define SENSOR_GB_BLACK_LEVEL	2
#define SENSOR_B_BLACK_LEVEL	3

/* External v4l2 format info. */
#define V4L2_I2C_REG_MAX		(150)
#define V4L2_I2C_ADDR_16BIT		(0x0002)
#define V4L2_I2C_DATA_16BIT		(0x0004)
#define V4L2_SBUS_MASK_SAMPLE_8BITS	0x01
#define V4L2_SBUS_MASK_SAMPLE_16BITS	0x02
#define V4L2_SBUS_MASK_SAMPLE_32BITS	0x04
#define V4L2_SBUS_MASK_ADDR_8BITS	0x08
#define V4L2_SBUS_MASK_ADDR_16BITS	0x10
#define V4L2_SBUS_MASK_ADDR_32BITS	0x20
#define V4L2_SBUS_MASK_ADDR_STEP_16BITS 0x40
#define V4L2_SBUS_MASK_ADDR_STEP_32BITS 0x80
#define V4L2_SBUS_MASK_SAMPLE_SWAP_BYTES 0x100
#define V4L2_SBUS_MASK_SAMPLE_SWAP_WORDS 0x200
#define V4L2_SBUS_MASK_ADDR_SWAP_BYTES	0x400
#define V4L2_SBUS_MASK_ADDR_SWAP_WORDS	0x800
#define V4L2_SBUS_MASK_ADDR_SKIP	0x1000
#define V4L2_SBUS_MASK_SPI_READ_MSB_SET 0x2000
#define V4L2_SBUS_MASK_SPI_INVERSE_DATA 0x4000
#define V4L2_SBUS_MASK_SPI_HALF_ADDR	0x8000
#define V4L2_SBUS_MASK_SPI_LSB		0x10000

//RGBIR
enum rgbir_mbus_fmt{
	V4L2_MBUS_FMT_SRGIB8_1X8 = 0x3100,
	V4L2_MBUS_FMT_SBGIR8_1X8,
	V4L2_MBUS_FMT_SRIGB8_1X8,
	V4L2_MBUS_FMT_SBIGR8_1X8,
	V4L2_MBUS_FMT_SGRBI8_1X8,
	V4L2_MBUS_FMT_SGBRI8_1X8,
	V4L2_MBUS_FMT_SIRBG8_1X8,
	V4L2_MBUS_FMT_SIBRG8_1X8,
	V4L2_MBUS_FMT_SRGGI8_1X8,
	V4L2_MBUS_FMT_SBGGI8_1X8,
	V4L2_MBUS_FMT_SGRIG8_1X8,
	V4L2_MBUS_FMT_SGBIG8_1X8,
	V4L2_MBUS_FMT_SGIRG8_1X8,
	V4L2_MBUS_FMT_SGIBG8_1X8,
	V4L2_MBUS_FMT_SIGGR8_1X8,
	V4L2_MBUS_FMT_SIGGB8_1X8,

	V4L2_MBUS_FMT_SRGIB10_1X10 = 0x3200,
	V4L2_MBUS_FMT_SBGIR10_1X10,
	V4L2_MBUS_FMT_SRIGB10_1X10,
	V4L2_MBUS_FMT_SBIGR10_1X10,
	V4L2_MBUS_FMT_SGRBI10_1X10,
	V4L2_MBUS_FMT_SGBRI10_1X10,
	V4L2_MBUS_FMT_SIRBG10_1X10,
	V4L2_MBUS_FMT_SIBRG10_1X10,
	V4L2_MBUS_FMT_SRGGI10_1X10,
	V4L2_MBUS_FMT_SBGGI10_1X10,
	V4L2_MBUS_FMT_SGRIG10_1X10,
	V4L2_MBUS_FMT_SGBIG10_1X10,
	V4L2_MBUS_FMT_SGIRG10_1X10,
	V4L2_MBUS_FMT_SGIBG10_1X10,
	V4L2_MBUS_FMT_SIGGR10_1X10,
	V4L2_MBUS_FMT_SIGGB10_1X10,

	V4L2_MBUS_FMT_SRGIB12_1X12 = 0x3300,
	V4L2_MBUS_FMT_SBGIR12_1X12,
	V4L2_MBUS_FMT_SRIGB12_1X12,
	V4L2_MBUS_FMT_SBIGR12_1X12,
	V4L2_MBUS_FMT_SGRBI12_1X12,
	V4L2_MBUS_FMT_SGBRI12_1X12,
	V4L2_MBUS_FMT_SIRBG12_1X12,
	V4L2_MBUS_FMT_SIBRG12_1X12,
	V4L2_MBUS_FMT_SRGGI12_1X12,
	V4L2_MBUS_FMT_SBGGI12_1X12,
	V4L2_MBUS_FMT_SGRIG12_1X12,
	V4L2_MBUS_FMT_SGBIG12_1X12,
	V4L2_MBUS_FMT_SGIRG12_1X12,
	V4L2_MBUS_FMT_SGIBG12_1X12,
	V4L2_MBUS_FMT_SIGGR12_1X12,
	V4L2_MBUS_FMT_SIGGB12_1X12,
};

struct tx_isp_sensor_win_setting {
	int	width;
	int	height;
	int fps;
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	void *regs;	/* Regs to tweak; the default fps is fast */
};


/***************************************************
 *  Provide extensions to v4l2 for ISP driver.
 ****************************************************/
#define V4L2_PIX_FMT_RGB310   v4l2_fourcc('R', 'G', 'B', 'A') /* 32  RGB-10-10-10  */
#define V4L2_MBUS_FMT_RGB888_3X8_LE (V4L2_MBUS_FMT_Y8_1X8 - 0x10)

/*
 *------ definition sensor associated structure -----
 */

/* define control bus */
enum tx_sensor_control_bus_type{
	TX_SENSOR_CONTROL_INTERFACE_I2C = 1,
	TX_SENSOR_CONTROL_INTERFACE_SPI,
};
struct tx_isp_i2c_board_info {
	char type[I2C_NAME_SIZE];
	int addr;
//	struct i2c_board_info board_info;
	int i2c_adapter_id;
};

struct tx_isp_spi_board_info {
	char modalias[SPI_NAME_SIZE];
	int bus_num;
//	struct spi_board_info board_info;
};

/* define data bus */
enum tx_sensor_data_bus_type{
	TX_SENSOR_DATA_INTERFACE_MIPI = 1,
	TX_SENSOR_DATA_INTERFACE_DVP,
	TX_SENSOR_DATA_INTERFACE_BT601,
	TX_SENSOR_DATA_INTERFACE_BT656,
	TX_SENSOR_DATA_INTERFACE_BT1120,
};

enum tx_sensor_data_type {
	TX_SENSOR_DATA_TYPE_LINEAR = 0,
	TX_SENSOR_DATA_TYPE_WDR_FS,
	TX_SENSOR_DATA_TYPE_WDR_DOL,
	TX_SENSOR_DATA_TYPE_WDR_NATIVE,
};

enum tx_sensor_frm_mode {
	TX_SENSOR_DEFAULT_FRAME_MODE = 0,
	TX_SENSOR_WDR_2_FRAME_MODE,
	TX_SENSOR_WDR_3_FRAME_MODE,
	TX_SENSOR_WDR_4_FRAME_MODE,
};

enum tx_sensor_mode {
	TX_SENSOR_DEFAULT_MODE = 0,
	TX_SENSOR_NOT_VC_MODE,
	TX_SENSOR_VC_MODE,
};

enum tx_sensor_csi_fmt {
	TX_SENSOR_RAW8 = 0,
	TX_SENSOR_RAW10,
	TX_SENSOR_RAW12,
};

typedef enum {
	DVP_PA_LOW_10BIT,
	DVP_PA_HIGH_10BIT,
	DVP_PA_12BIT,
	DVP_PA_LOW_8BIT,
	DVP_PA_HIGH_8BIT,
} sensor_dvp_gpio_mode;

typedef enum {
	SENSOR_DVP_HREF_MODE,
	SENSOR_DVP_HSYNC_MODE,
	SENSOR_DVP_SONY_MODE,
} sensor_dvp_timing_mode;

typedef enum {
	SENSOR_MIPI_OTHER_MODE,
	SENSOR_MIPI_SONY_MODE,
}sensor_mipi_mode;

typedef enum {
	SENSOR_BT_8BIT,
	SENSOR_BT_16BIT,
}sensor_bt_mode;

typedef enum {
	ISP_CLK_960P_MODE = 60000000,
	ISP_CLK_1080P_MODE = 90000000,
	ISP_CLK_3M_MODE = 100000000,
} isp_clk_mode;

typedef enum {
	TX_SENSOR_MAX_FPS_120 = 120,
	TX_SENSOR_MAX_FPS_60 = 60,
	TX_SENSOR_MAX_FPS_50 = 50,
	TX_SENSOR_MAX_FPS_45 = 45,
	TX_SENSOR_MAX_FPS_30 = 30,
	TX_SENSOR_MAX_FPS_25 = 25,
	TX_SENSOR_MAX_FPS_20 = 20,
	TX_SENSOR_MAX_FPS_15 = 15,
	TX_SENSOR_MAX_FPS_12 = 12,
	TX_SENSOR_MAX_FPS_10 = 10,
	TX_SENSOR_MAX_FPS_5 = 5,
} sensor_max_fps_mode;

typedef enum {
	TX_SENSOR_RES_30 = 30,
	TX_SENSOR_RES_100 = 100,
	TX_SENSOR_RES_130 = 130,
	TX_SENSOR_RES_200 = 200,
	TX_SENSOR_RES_300 = 300,
	TX_SENSOR_RES_400 = 400,
	TX_SENSOR_RES_500 = 500,
	TX_SENSOR_RES_800 = 800,
} sensor_resolution_type;

enum tx_isp_dvp_polarity {
	DVP_POLARITY_DEFAULT,
	DVP_POLARITY_HIGH,
	DVP_POLARITY_LOW,
};

typedef struct {
	unsigned short vblanking;
	unsigned short hblanking;
} sensor_dvp_blanking;

enum  mipi_sensor_data_type_value {
	YUV422_8BIT = 0x1e,
	RAW8 = 0x2a,
	RAW10 = 0x2b,
	RAW12 = 0x2c,
};

typedef struct {
	unsigned char hsync_polar;
	unsigned char vsync_polar;
	unsigned char pclk_polar;
} sensor_dvp_polar;

struct vic_mipi_sensor_ctl {
	unsigned int hcrop_diff_en;
	unsigned int mipi_vcomp_en;
	unsigned int mipi_hcomp_en;
	unsigned short mipi_crop_start0x;
	unsigned short mipi_crop_start0y;
	unsigned short mipi_crop_start1x;
	unsigned short mipi_crop_start1y;
	unsigned short mipi_crop_start2x;
	unsigned short mipi_crop_start2y;
	unsigned short mipi_crop_start3x;
	unsigned short mipi_crop_start3y;
	unsigned int line_sync_mode;
	unsigned int work_start_flag;
	unsigned int data_type_en;
	enum  mipi_sensor_data_type_value data_type_value;//0x1E 2A 2B 2C
	unsigned int del_start;
	unsigned int sensor_fid_mode;
	enum tx_sensor_frm_mode sensor_frame_mode;
	enum tx_sensor_mode sensor_mode;
	enum tx_sensor_csi_fmt sensor_csi_fmt;
};

struct mipi_crop_ch {
	unsigned int all_image_width;
	unsigned int hcrop_del_start_pixel;
};

struct vic_mipi_vcrop_del{
	unsigned int del0;
	unsigned int del1;
	unsigned int del2;
	unsigned int del3;
};

struct tx_isp_mipi_bus{
	sensor_mipi_mode mode;
	unsigned int clk;
	unsigned int data;
	unsigned char lans;
	unsigned int def2wdr;
	unsigned int image_twidth;
	unsigned int image_theight;
	unsigned int front_invlid_pix;
	unsigned int after_invlid_pix;
	unsigned int settle_time_apative_en;
	struct vic_mipi_sensor_ctl mipi_sc;
	struct vic_mipi_vcrop_del mipi_vcropdel;
};

struct tx_isp_dvp_bus{
	sensor_dvp_gpio_mode gpio;
	sensor_dvp_timing_mode mode;
	sensor_dvp_blanking blanking;
	sensor_dvp_polar polar;
	unsigned int dvp_hcomp_en;
};

struct tx_isp_bt1120_bus{
	sensor_bt_mode gpio;
};
struct tx_isp_bt656_bus{
	sensor_bt_mode gpio;
};
struct tx_isp_bt601_bus{
	sensor_bt_mode gpio;
};

/* define sensor attribute */

#define TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS 	1
#define TX_ISP_SENSOR_FULL_RES_MAX_FPS 		2

struct tx_isp_sensor_register_info{
	char name[32];
	enum tx_sensor_control_bus_type cbus_type;
	union {
		struct tx_isp_i2c_board_info i2c;
		struct tx_isp_spi_board_info spi;
	};
	unsigned short rst_gpio;
	unsigned short pwdn_gpio;
	unsigned short power_gpio;
};

typedef struct tx_isp_sensor_ctrl{
	/* isp_gain mean that the value is output of ISP-FW,it is not a gain multiplier unit.
	 *  gain_mutiplier = (2^(isp_gain/(2^LOG_GAIN_SHIFT))).
	 *  the fuction will convert gain_mutiplier to sensor_Xgain.
	 *  the return value is isp_gain of sensor_Xgain's inverse conversion.
	 */
	unsigned int (*alloc_again)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again);
	unsigned int (*alloc_again_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again);
	unsigned int (*alloc_dgain)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain);
	unsigned int (*alloc_dgain_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain);
	unsigned int (*alloc_integration_time)(unsigned int it, unsigned char shift, unsigned int *sensor_it);
	unsigned int (*alloc_integration_time_short)(unsigned int it_short, unsigned char shift, unsigned int *sensor_it_short);
} TX_ISP_SENSOR_CTRL;

#define TX_ISP_GAIN_FIXED_POINT 16
#define LOG2_GAIN_SHIFT 16
struct tx_isp_sensor_attribute{
	const char *name;
	unsigned int chip_id;
	enum tx_sensor_control_bus_type cbus_type;
	unsigned int cbus_mask;
	unsigned int cbus_device;
	enum tx_sensor_data_bus_type dbus_type;
	union {
		struct tx_isp_mipi_bus 		mipi;
		struct tx_isp_dvp_bus 		dvp;
		struct tx_isp_bt1120_bus 	bt1120;
		struct tx_isp_bt656_bus		bt656bus;
		struct tx_isp_bt601_bus		bt601bus;
		char string[64];
	};
	enum tx_sensor_data_type data_type;
	unsigned int max_again;	//the format is .16
	unsigned int max_dgain;	//the format is .16
	unsigned int again;
	unsigned int dgain;
	unsigned short min_integration_time;
	unsigned short min_integration_time_native;
	unsigned short max_integration_time_native;
	unsigned short integration_time_limit;
	unsigned int integration_time;
	unsigned short total_width;
	unsigned short total_height;
	unsigned short max_integration_time;
	unsigned short integration_time_apply_delay;
	unsigned short again_apply_delay;
	unsigned short dgain_apply_delay;
	unsigned short one_line_expr_in_us;
	TX_ISP_SENSOR_CTRL sensor_ctrl;
	unsigned short min_integration_time_short;
	unsigned short max_integration_time_short;
	unsigned int integration_time_short;
	unsigned int max_again_short;	//the format is .16
	unsigned int again_short;
	unsigned int wdr_cache;	//the format is .16
	unsigned int expo;
	void *priv; /* point to struct tx_isp_sensor_board_info */
};

/* define common struct */
enum tx_isp_priv_ioctl_direction {
	TX_ISP_PRIVATE_IOCTL_SET,
	TX_ISP_PRIVATE_IOCTL_GET,
};

#define NOTIFICATION_TYPE_CORE_OPS (0x1<<24)
#define NOTIFICATION_TYPE_SENSOR_OPS (0x2<<24)
#define NOTIFICATION_TYPE_FS_OPS 	(0x3<<24)
#define NOTIFICATION_TYPE_TUN_OPS 	(0x4<<24)
#define NOTIFICATION_TYPE_OPS(n)	((n) & (0xff<<24))
enum tx_isp_notification {
	/* the events of subdev */
	TX_ISP_EVENT_SUBDEV_INIT = NOTIFICATION_TYPE_CORE_OPS,
	TX_ISP_EVENT_SYNC_SENSOR_ATTR,
	/* the events of sensor are defined as follows. */
	TX_ISP_EVENT_SENSOR_REGISTER = NOTIFICATION_TYPE_SENSOR_OPS,
	TX_ISP_EVENT_SENSOR_RELEASE,
	TX_ISP_EVENT_SENSOR_ENUM_INPUT,
	TX_ISP_EVENT_SENSOR_GET_INPUT,
	TX_ISP_EVENT_SENSOR_SET_INPUT,
	TX_ISP_EVENT_SENSOR_INT_TIME,
	TX_ISP_EVENT_SENSOR_INT_TIME_SHORT,
	TX_ISP_EVENT_SENSOR_AGAIN,
	TX_ISP_EVENT_SENSOR_AGAIN_SHORT,
	TX_ISP_EVENT_SENSOR_DGAIN,
	TX_ISP_EVENT_SENSOR_FPS,
	TX_ISP_EVENT_SENSOR_BLACK_LEVEL,
	TX_ISP_EVENT_SENSOR_WDR,
	TX_ISP_EVENT_SENSOR_RESIZE,
	TX_ISP_EVENT_SENSOR_PREPARE_CHANGE,
	TX_ISP_EVENT_SENSOR_FINISH_CHANGE,
	TX_ISP_EVENT_SENSOR_VFLIP,
	TX_ISP_EVENT_SENSOR_S_REGISTER,
	TX_ISP_EVENT_SENSOR_G_REGISTER,
	TX_ISP_EVENT_SENSOR_WDR_STOP,
	TX_ISP_EVENT_SENSOR_WDR_OPEN,
	TX_ISP_EVENT_SENSOR_LOGIC,
	TX_ISP_EVENT_SENSOR_EXPO,
	/* the events of frame-channel are defined as follows. */
	TX_ISP_EVENT_FRAME_CHAN_BYPASS_ISP = NOTIFICATION_TYPE_FS_OPS,
	TX_ISP_EVENT_FRAME_CHAN_GET_FMT,
	TX_ISP_EVENT_FRAME_CHAN_SET_FMT,
	TX_ISP_EVENT_FRAME_CHAN_STREAM_ON,
	TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF,
	TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER,
	TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER,
	TX_ISP_EVENT_FRAME_CHAN_FREE_BUFFER,
	TX_ISP_EVENT_FRAME_CHAN_SET_BANKS,
	/* the tuning node of isp's core */
	TX_ISP_EVENT_ACTIVATE_MODULE = NOTIFICATION_TYPE_TUN_OPS,
	TX_ISP_EVENT_SLAVE_MODULE,
	TX_ISP_EVENT_CORE_FRAME_DONE,
	TX_ISP_EVENT_CORE_DAY_NIGHT,
};

struct tx_isp_notify_argument{
	int value;
	int ret;
};

struct frame_image_scalercap {
	unsigned short max_width;
	unsigned short max_height;
	unsigned short min_width;
	unsigned short min_height;
};
struct frame_image_scaler {
	unsigned short out_width;
	unsigned short out_height;
};

/* isp image tuning */
struct isp_image_tuning_default_ctrl {
	enum tx_isp_priv_ioctl_direction dir;
	struct v4l2_control control;
};

/**
 * struct frame_image_format
 * @type:	enum v4l2_buf_type; type of the data stream
 */
struct frame_image_format {
	unsigned int type;
	struct v4l2_pix_format pix;

	/* crop */
	bool	crop_enable;
	unsigned int crop_top;
	unsigned int crop_left;
	unsigned int crop_width;
	unsigned int crop_height;

	/* scaler */
	bool	scaler_enable;
	unsigned int scaler_out_width;
	unsigned int scaler_out_height;

	unsigned int rate_bits;
	unsigned int rate_mask;

	/* crop front */
	bool	fcrop_enable;
	unsigned int fcrop_top;
	unsigned int fcrop_left;
	unsigned int fcrop_width;
	unsigned int fcrop_height;
};

#define ISP_LFB_DEFAULT_BUF_BASE0 0xf0000000
#define ISP_LFB_DEFAULT_BUF_BASE1 0xf8000000
enum tx_isp_module_link_id {
	LINK_ISP_LFB_NCU_MS_FS,
	LINK_ISP_LDC_NCU_MS_FS,
	LINK_ISP_DDR_FS,
};

struct isp_buf_info {
	uint32_t paddr;
	uint32_t size;
};

#define VIDIOC_REGISTER_SENSOR			  _IOW('V', BASE_VIDIOC_PRIVATE + 1, struct tx_isp_sensor_register_info)
#define VIDIOC_RELEASE_SENSOR			  _IOW('V', BASE_VIDIOC_PRIVATE + 2, struct tx_isp_sensor_register_info)
#define VIDIOC_SET_FRAME_FORMAT			  _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct frame_image_format)
#define VIDIOC_GET_FRAME_FORMAT               _IOR('V', BASE_VIDIOC_PRIVATE + 4, struct frame_image_format)
#define VIDIOC_DEFAULT_CMD_SET_BANKS	      _IOW('V', BASE_VIDIOC_PRIVATE + 5, int)
#define VIDIOC_DEFAULT_CMD_ISP_TUNING	      _IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct isp_image_tuning_default_ctrl)

#define VIDIOC_CREATE_SUBDEV_LINKS            _IOW('V', BASE_VIDIOC_PRIVATE + 16, int)
#define VIDIOC_DESTROY_SUBDEV_LINKS	 		  _IOW('V', BASE_VIDIOC_PRIVATE + 17, int)
#define VIDIOC_LINKS_STREAMON	 			  _IOW('V', BASE_VIDIOC_PRIVATE + 18, int)
#define VIDIOC_LINKS_STREAMOFF	 			  _IOW('V', BASE_VIDIOC_PRIVATE + 19, int)
#define VIDIOC_SET_BUF_INFO	 		    	  _IOW('V', BASE_VIDIOC_PRIVATE + 20, struct isp_buf_info)
#define VIDIOC_GET_BUF_INFO					  _IOW('V', BASE_VIDIOC_PRIVATE + 21, struct isp_buf_info)
#define VIDIOC_SET_WDR_BUF_INFO	 		    	  _IOW('V', BASE_VIDIOC_PRIVATE + 22, struct isp_buf_info)
#define VIDIOC_GET_WDR_BUF_INFO	 		    	  _IOW('V', BASE_VIDIOC_PRIVATE + 23, struct isp_buf_info)
#define VIDIOC_ISP_WDR_ENABLE                             _IOW('V', BASE_VIDIOC_PRIVATE + 24, int)
#define VIDIOC_ISP_WDR_DISABLE                             _IOW('V', BASE_VIDIOC_PRIVATE + 25, int)

enum tx_isp_vidioc_default_command {
	TX_ISP_VIDIOC_DEFAULT_CMD_BYPASS_ISP,
	TX_ISP_VIDIOC_DEFAULT_CMD_SCALER_CAP,
	TX_ISP_VIDIOC_DEFAULT_CMD_SET_SCALER,
};

enum tx_isp_frame_channel_bypass_isp{
	TX_ISP_FRAME_CHANNEL_BYPASS_ISP_DISABLE,
	TX_ISP_FRAME_CHANNEL_BYPASS_ISP_ENABLE,
};
#if 0
enum {
	TX_ISP_STATE_CLOSE = 0,
	TX_ISP_STATE_OPEN,
	TX_ISP_STATE_STOP = TX_ISP_STATE_OPEN,
	TX_ISP_STATE_RUN,
};
#endif
struct tx_isp_video_in {
	struct v4l2_mbus_framefmt mbus;
	unsigned int mbus_change;
	struct tx_isp_sensor_attribute *attr;
	unsigned int vi_max_width;	//the max width of sensor output setting
	unsigned int vi_max_height;	//the max height of sensor output setting
	unsigned int fps;
	int grp_id;
	unsigned int shvflip;
};

enum tx_isp_notify_statement {
	TX_ISP_NOTIFY_LINK_SETUP = 0x10,
	TX_ISP_NOTIFY_LINK_DESTROY,
};

struct tx_isp_sensor{
	struct tx_isp_subdev sd;
	int index;
	unsigned int type;
	struct list_head list;
	struct tx_isp_sensor_register_info info;
	struct tx_isp_sensor_attribute attr;
	struct tx_isp_video_in video;
	struct clk *mclk;
	void *priv;
};
#define sd_to_sensor_device(_ep) container_of(_ep, struct tx_isp_sensor, sd)

#define tx_isp_readl(base, reg)		__raw_readl((base) + (reg))
#define tx_isp_writel(base, reg, value)		__raw_writel((value), ((base) + (reg)))
#define tx_isp_readw(base, reg)		__raw_readw((base) + (reg))
#define tx_isp_writew(base, reg, value)		__raw_writew((value), ((base) + (reg)))
#define tx_isp_readb(base, reg)		__raw_readb((base) + (reg))
#define tx_isp_writeb(base, reg, value)		__raw_writeb((value), ((base) + (reg)))

#endif /*__TX_ISP_COMMON_H__*/
