#include <stdio.h>
#include "nand_common.h"

#define XTX_MID			    0xA1
#define XTX_NAND_DEVICD_COUNT	    2

static unsigned char xtx_xaw[] = {0x2};

static struct device_struct device[2] = {
	DEVICE_STRUCT(0xE1, 2048, 2, 4, 2, 1, xtx_xaw),
	DEVICE_STRUCT(0xE2, 2048, 2, 4, 2, 1, xtx_xaw),
};

static struct nand_desc xtx_nand = {

	.id_manufactory = XTX_MID,
	.device_counts = XTX_NAND_DEVICD_COUNT,
	.device = device,
};

int xtx_nand_register_func(void) {
	return nand_register(&xtx_nand);
}
