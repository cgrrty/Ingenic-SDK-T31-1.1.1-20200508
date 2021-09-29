/*
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __JZ_FB_H__
#define __JZ_FB_H__

#include <linux/fb.h>
#include "jz_dsim.h"

#define FB_MODE_IS_VGA    (1 << 30)
/* LCD controller supported display device output mode */
enum jzfb_lcd_type {
	LCD_TYPE_GENERIC_16_BIT = 0,
	LCD_TYPE_GENERIC_18_BIT = 0 | (1 << 7),
	LCD_TYPE_GENERIC_24_BIT = 0 | (1 << 6),
	LCD_TYPE_SPECIAL_TFT_1 = 1,
	LCD_TYPE_SPECIAL_TFT_2 = 2,
	LCD_TYPE_SPECIAL_TFT_3 = 3,
	LCD_TYPE_NON_INTERLACED_TV = 4 | (1 << 26),
	LCD_TYPE_INTERLACED_TV = 6 | (1 << 26) | (1 << 30),
	LCD_TYPE_8BIT_SERIAL = 0xc,
	LCD_TYPE_SLCD = 0xd | (1 << 31),
};

/* lcd glb cfg color */
enum lcd_glb_cfg_color {
	LCD_GLB_COLOR_RGB = (0 << 19),
	LCD_GLB_COLOR_RBG = (1 << 19),
	LCD_GLB_COLOR_GRB = (2 << 19),
	LCD_GLB_COLOR_BRG = (3 << 19),
	LCD_GLB_COLOR_GBR = (4 << 19),
	LCD_GLB_COLOR_BGR = (5 << 19),
};

/* lcd glb cfg format */
enum lcd_glb_cfg_format {
	LCD_GLB_FORMAT_555 = (0 << 16),
	LCD_GLB_FORMAT_565 = (1 << 16),
	LCD_GLB_FORMAT_888 = (2 << 16),
	LCD_GLB_FORMAT_NV12 = (4 << 16),
	LCD_GLB_FORMAT_NV21 = (5 << 16),
};

/* tft lcd cfg color even*/
enum tft_lcd_cfg_color_even {
	TFT_LCD_COLOR_EVEN_RGB = (0 << 5),
	TFT_LCD_COLOR_EVEN_RBG = (1 << 5),
	TFT_LCD_COLOR_EVEN_BGR = (2 << 5),
	TFT_LCD_COLOR_EVEN_BRG = (3 << 5),
	TFT_LCD_COLOR_EVEN_GBR = (4 << 5),
	TFT_LCD_COLOR_EVEN_GRB = (5 << 5),
};

/* tft lcd cfg color odd*/
enum tft_lcd_cfg_color_odd {
	TFT_LCD_COLOR_ODD_RGB = (0 << 2),
	TFT_LCD_COLOR_ODD_RBG = (1 << 2),
	TFT_LCD_COLOR_ODD_BGR = (2 << 2),
	TFT_LCD_COLOR_ODD_BRG = (3 << 2),
	TFT_LCD_COLOR_ODD_GBR = (4 << 2),
	TFT_LCD_COLOR_ODD_GRB = (5 << 2),
};

/* tft lcd cfg mode */
enum tft_lcd_cfg_mode {
	TFT_LCD_MODE_24_BIT = (0 << 1),
	TFT_LCD_MODE_8_BIT_RGB = (2 << 0),
	TFT_LCD_MODE_8_BIT_RGBD = (3 << 0),
};

/* smart lcd data format */
enum smart_lcd_format {
	SMART_LCD_FORMAT_565 = (0 << 21),
	SMART_LCD_FORMAT_666 = (1 << 21),
	SMART_LCD_FORMAT_888 = (2 << 21),
};

/* smart lcd command width */
enum smart_lcd_cwidth {
	SMART_LCD_CWIDTH_8_BIT = (0),
	SMART_LCD_CWIDTH_9_BIT = (1),
	SMART_LCD_CWIDTH_16_BIT = (2),
	SMART_LCD_CWIDTH_18_BIT = (3),
	SMART_LCD_CWIDTH_24_BIT = (4),
};

/* smart lcd data width */
enum smart_lcd_dwidth {
	SMART_LCD_DWIDTH_8_BIT = (0 << 3),
	SMART_LCD_DWIDTH_9_BIT = (1 << 3),
	SMART_LCD_DWIDTH_16_BIT = (2 << 3),
	SMART_LCD_DWIDTH_18_BIT = (3 << 3),
	SMART_LCD_DWIDTH_24_BIT = (4 << 3),
};

/**
 * @reg: the register address
 * @value: the value to be written
 * @type: operation type, 0: write register, 1: write command, 2: write data
 * @udelay: delay time in us
 */

enum smart_config_type {
	SMART_CONFIG_CMD =  0,
	SMART_CONFIG_DATA =  1,
	SMART_CONFIG_UDELAY =  2,
};

struct smart_lcd_data_table {
	enum smart_config_type type;
	uint32_t value;
};

struct jzdsi_data {
	struct fb_videomode *modes;
	struct video_config video_config;
	struct dsi_config dsi_config;
	unsigned int bpp_info;
	unsigned int max_bps;
};

/**
 * struct jzfb_platform_data - the platform data of frame buffer
 *
 * @num_modes: size of modes
 * @modes: list of valid video modes
 * @lcd_type: lcd type
 * @bpp: bits per pixel for the lcd
 * @width: width of the lcd display in mm
 * @height: height of the lcd display in mm
 * @pinmd: 16bpp lcd data pin mapping. 0: LCD_D[15:0],1: LCD_D[17:10] LCD_D[8:1]
 * @pixclk_falling_edge: pixel clock at falling edge
 * @data_enable_active_low: data enable active low
 * @smart_type: smart lcd transfer type, 0: parrallel, 1: serial
 * @cmd_width: smart lcd command width
 * @data_width:smart lcd data Width
 * @clkply_active_rising: smart lcd clock polarity: 0: Active edge is Falling,
 * 1: Active edge is Rasing
 * @rsply_cmd_high: smart lcd RS polarity. 0: Command_RS=0, Data_RS=1;
 * 1: Command_RS=1, Data_RS=0
 * @csply_active_high: smart lcd CS Polarity. 0: Active level is low,
 * 1: Active level is high
 * @write_gram_cmd: write graphic ram command
 * @bus_width: bus width in bit
 * @length_data_table: array size of data_table
 * @data_table: init data table
 * @dither_enable: enable dither function: 1, disable dither function: 0
 * when LCD'bpp is lower than 24, suggest to enable it
 * @dither_red: 0:8bit dither, 1:6bit dither, 2: 5bit dither, 3: 4bit dither
 * @dither_green: 0:8bit dither, 1:6bit dither, 2: 5bit dither, 3: 4bit dither
 * @dither_blue: 0:8bit dither, 1:6bit dither, 2: 5bit dither, 3: 4bit dither
 * @spl: special_tft SPL signal register setting
 * @cls: special_tft CLS signal register setting
 * @ps: special_tft PS signal register setting
 * @rev: special_tft REV signal register setting
 */

struct jzfb_smart_config {
	unsigned clkply_active_rising:1;
	unsigned rsply_cmd_high:1;
	unsigned csply_active_high:1;

	unsigned cfg_6800_md:1;
	unsigned cfg_fmt_conv:1;
	unsigned datatx_type_serial:1;
	unsigned cmdtx_type_serial:1;
	unsigned cfg_cmd_9bit:1;

	size_t length_cmd;
	unsigned long *write_gram_cmd;
	unsigned bus_width;
	size_t length_data_table;
	struct smart_lcd_data_table *data_table;
};

struct jzfb_platform_data {
	size_t num_modes;
	struct fb_videomode *modes;
	struct jzdsi_data *dsi_pdata;

	enum jzfb_lcd_type lcd_type;
	unsigned int bpp;
	unsigned int width;
	unsigned int height;
	unsigned pinmd:1;

	unsigned pixclk_falling_edge:1;
	unsigned data_enable_active_low:1;

	struct jzfb_smart_config smart_config;

	unsigned dither_enable:1;
	struct {
		unsigned dither_red;
		unsigned dither_green;
		unsigned dither_blue;
	} dither;

	struct {
		int (*lcd_initialize_begin)(void*);
		int (*lcd_initialize_end)(void*);

		int (*lcd_power_on_begin)(void*);
		int (*lcd_power_on_end)(void*);

		int (*lcd_power_off_begin)(void*);
		int (*lcd_power_off_end)(void*);

		int (*dma_transfer_begin)(void*);
		int (*dma_transfer_end)(void*);
	} lcd_callback_ops;

};

#endif
