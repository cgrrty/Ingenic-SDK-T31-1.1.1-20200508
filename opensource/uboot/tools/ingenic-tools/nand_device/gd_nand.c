#include <stdio.h>
#include "nand_common.h"

#define GD_MID			    0xC8
#define GD_NAND_DEVICD_COUNT	    6

static unsigned char gdxb_errstat[]= {0x2};
static unsigned char gdxc_errstat[]= {0x7};

static struct device_struct device[] = {
	DEVICE_STRUCT(0xD1, 2048, 2, 4, 2, 1, gdxb_errstat),
	DEVICE_STRUCT(0xD2, 2048, 2, 4, 2, 1, gdxb_errstat),
	DEVICE_STRUCT(0xD4, 4096, 2, 4, 2, 1, gdxb_errstat),
	DEVICE_STRUCT(0xB1, 2048, 3, 4, 3, 1, gdxc_errstat),
	DEVICE_STRUCT(0xB2, 2048, 3, 4, 3, 1, gdxc_errstat),
	DEVICE_STRUCT(0xB4, 4096, 3, 4, 3, 1, gdxc_errstat),
};

static struct nand_desc gd_nand = {

	.id_manufactory = GD_MID,
	.device_counts = GD_NAND_DEVICD_COUNT,
	.device = device,
};

int gd_nand_register_func(void) {
	return nand_register(&gd_nand);
}
