/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */


#ifndef __TX_ISP_H__
#define __TX_ISP_H__

#define DUMMY_CLOCK_RATE 0x0000ffff

/* --------------------------------------------------------------------------
 * T-video constants
 */

/* Video Device Descriptor types */
#define TX_ISP_TYPE_HEADER				0x00
#define TX_ISP_TYPE_SUBDEV				0x01
#define TX_ISP_TYPE_WIDGET				0x02

#define TX_ISP_HEADER_ID(n)	(TX_ISP_TYPE_HEADER<<4 | n)
#define TX_ISP_SUBDEV_ID(n)	(TX_ISP_TYPE_SUBDEV<<4 | n)
#define TX_ISP_WIDGET_ID(n)	(TX_ISP_TYPE_WIDGET<<4 | n)
#define TX_ISP_GET_ID(n)	((n) & 0xf)

/* Video Device Descriptor Subtypes */
#define TX_ISP_SUBTYPE_UNDEFINE					(0x00)
#define TX_ISP_SUBTYPE_INPUT_TERMINAL			(0x01)
#define TX_ISP_SUBTYPE_OUTPUT_TERMINAL			(0x02)
#define TX_ISP_SUBTYPE_PROCESSING_UNIT			(0x03)
#define TX_ISP_SUBTYPE_CONTROLLER				(0x04)
#define TX_ISP_SUBTYPE_SELECTOR_UNIT			(0x05)

/* Video Device Descriptor pad types */
#define TX_ISP_PADTYPE_UNDEFINE			0x00
#define TX_ISP_PADTYPE_INPUT			0x01
#define TX_ISP_PADTYPE_OUTPUT			0x02
#define TX_ISP_PADSTATE_FREE			(0x2)
#define TX_ISP_PADSTATE_LINKED			(0x3)
#define TX_ISP_PADSTATE_STREAM			(0x4)
#define TX_ISP_PADLINK_DDR				(0x1<<4)
#define TX_ISP_PADLINK_LFB				(0x1<<5)
#define TX_ISP_PADLINK_FS				(0x1<<6)

/* Video Device Descriptor link types */
#define TX_ISP_LINKFLAG_DYNAMIC		(0x0)
#define TX_ISP_LINKFLAG_ENABLED		(0x1)
#define TX_ISP_LINKFLAG(v)			((v) & 0xf)


#define TX_ISP_NAME_LEN 16
#define TX_ISP_PADS_PER_SUBDEV 8
#define TX_ISP_LINKS_PER_PADS 4

/*
 * the names of subdev are defined here.
 */
#define TX_ISP_VIN_NAME "isp-w00"
#define TX_ISP_CSI_NAME "isp-w01"
#define TX_ISP_VIC_NAME "isp-w02"
#define TX_ISP_CORE_NAME "isp-m0"
#define TX_ISP_LDC_NAME "isp-m1"
#define TX_ISP_NCU_NAME "isp-m2"
#define TX_ISP_MSCALER_NAME "isp-m3"
#define TX_ISP_FS_NAME "isp-fs"

/* the name of subdev resource */
#define TX_ISP_DEV_NAME "isp-device"
#define TX_ISP_IRQ_NAME "isp-irq"
#define TX_ISP_IRQ_ID 	"isp-irq-id"

/* Video pad Descriptor */
struct tx_isp_pad_descriptor {
	unsigned char  type;
	unsigned char  links_type;
	/*unsigned char  links_type[TX_ISP_LINKS_PER_PADS];*/
};

/*
  @ name: the clock's name
  @ rate: the rate of the clock.
*/
struct tx_isp_device_clk{
	const char *name;
	unsigned long rate;
};

/* All TX descriptors have these 2 fields at the beginning */
struct tx_isp_descriptor {
	unsigned char  type;
	unsigned char  subtype;
	unsigned char  parentid;
	unsigned char  unitid;
};

/* Video device entity Descriptor */
struct tx_isp_device_descriptor {
	unsigned char  type;
	unsigned char  subtype;
	unsigned char  parentid;
	unsigned char  unitid;
	unsigned char  entity_num;
	struct platform_device **entities;
};

/* Video subdev entity Descriptor */
struct tx_isp_subdev_descriptor {
	unsigned char  type;
	unsigned char  subtype;
	unsigned char  parentid;
	unsigned char  unitid;
	unsigned char  clks_num;
	struct tx_isp_device_clk *clks;
	unsigned char  pads_num;
	struct tx_isp_pad_descriptor *pads;
};

/* Video widget entity Descriptor */
struct tx_isp_widget_descriptor {
	unsigned char  type;
	unsigned char  subtype;
	unsigned char  parentid;
	unsigned char  unitid;
	unsigned char  clks_num;
	struct tx_isp_device_clk *clks;
};

#endif /*__TX_ISP_H__*/
