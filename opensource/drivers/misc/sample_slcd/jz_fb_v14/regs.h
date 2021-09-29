/* drivers/video/jz4780/regs.h
 *
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * Register definition file for ingenic jz4780 Display Controller driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _REGS_LCDC_H_
#define _REGS_LCDC_H_


#define LCDC_TINY_CHAIN_ADDR			0x0000
#define LCDC_TINY_GLB_CFG				0x0004
#define LCDC_TINY_IPU_CFG				0x0008
#define	LCDC_TINY_CSC_MULT_YRV			0x0100
#define	LCDC_TINY_CSC_MULT_GUGV			0x0104
#define	LCDC_TINY_CSC_MULT_BU			0x0108
#define	LCDC_TINY_CSC_SUB_YUV			0x010c
#define	LCDC_TINY_CTRL					0x1000
#define LCDC_TINY_ST					0x1004
#define LCDC_TINY_CSR					0x1008
#define LCDC_TINY_INTC					0x100c
#define LCDC_TINY_INT_FLAG				0x101c
#define LCDC_TINY_CHAIN_SITE			0x2000
#define LCDC_TINY_DMA_SITE				0x2004
#define LCDC_TINY_DES_READ				0x3000

#define LCDC_DISP_COMMON				0x8000

#define LCDC_TFT_TIMING_HSYNC	        0x9000
#define LCDC_TFT_TIMING_VSYNC	        0x9004
#define LCDC_TFT_TIMING_HDE				0x9008
#define LCDC_TFT_TIMING_VDE				0x900c
#define LCDC_TFT_TRAN_CFG		        0x9010
#define LCDC_TFT_ST				        0x9018

#define LCDC_SLCD_PANEL_CFG				0xa000
#define LCDC_SLCD_WR_DUTY				0xa004
#define LCDC_SLCD_TIMING				0xa008
#define LCDC_SLCD_FRM_SIZE				0xa00c
#define LCDC_SLCD_SLOW_TIME				0xa010
#define LCDC_SLCD_CMD					0xa014
#define LCDC_SLCD_ST					0xa018

//**************************glb cfg**************************
#define BURST_LEN			(1 << 1)
#define DMA_SEL_NV12		(1 << 0)
#define DMA_SEL_RGB			(0 << 0)
#define IPU_MD				(1 << 2)
#define CLKGATE_CLS			(1 << 3)

#define QCK_STOPM			(1 << 6)
#define GEN_STOPM			(1 << 5)
#define TFT_UNDER			(1 << 3)
#define DMA_END				(1 << 2)
#define FRM_END				(1 << 1)
#define WORKING				(1 << 0)

#define TFT_START			(1 << 5)
#define SLCD_START			(1 << 4)
#define GEN_STOP			(1 << 3)
#define DES_CNT_RST			(1 << 2)
#define QCK_STOP			(1 << 1)
#define CHAIN_START			(1 << 0)

#define DISP_COM_TFT		0
#define DISP_COM_SLCD		1
//**************************glb cfg**************************

//**************************tft cfg**************************
#define PIX_CLK_INV			(1 << 10)
#define DE_DL				(1 << 9)
#define SYNC_DL				(1 << 8)

#define TFT_COLOR_RGB		0
#define TFT_COLOR_RBG		1
#define TFT_COLOR_BGR		2
#define TFT_COLOR_BRG		3
#define TFT_COLOR_GBR		4
#define TFT_COLOR_GRB		5

#define TFT_MODE_24BIT		(0 << 1)
#define TFT_MODE_RGB		2
#define TFT_MODE_RGBD		3
//**************************tft cfg**************************

//**************************slcd cfg**************************
#define SLCD_SINGLE_MODE		(0 << 31)
#define SLCD_CONTINUA_MODE		(1 << 31)
#define SLCD_RDY_ANTI_JIT		(1 << 27)
#define SLCD_FMT_EN				(1 << 26)
#define SLCD_DBI_TYPE_6800		(1 << 23)
#define SLCD_DBI_TYPE_8080		(2 << 23)
#define SLCD_TE_ANTI_JIT		(1 << 20)
#define SLCD_TE_MD				(1 << 19)
#define SLCD_SWITCH_WAIT_TE		(1 << 18)
#define SLCD_SWITCH_WAIT_RDY	(1 << 17)
#define SLCD_CS_DP				(1 << 11)
#define SLCD_RDY_DP				(1 << 10)
#define SLCD_DC_MD				(1 << 9)
#define SLCD_WR_DP				(1 << 8)
#define SLCD_CLK_PLY			(1 << 7)
#define SLCD_TE_DP				(1 << 6)

#define FLAG_CMD			(1 << 31)
#define FLAG_PAR			(1 << 30)
#define FLAG_DOP			(0 << 30)

#define SLCD_ST_BUSY		1
//**************************slcd cfg**************************


#endif /* _REGS_LCDC_H_ */
