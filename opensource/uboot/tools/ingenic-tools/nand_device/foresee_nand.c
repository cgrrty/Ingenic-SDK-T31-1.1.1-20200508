#include <stdio.h>
#include "nand_common.h"

#define ATO_MID			    0xCD
#define ATO_NAND_DEVICD_COUNT	    1

static unsigned char fw_xaw[] = {0x05, 0x06};

static struct device_struct device[] = {
	DEVICE_STRUCT(0xB1, 2048, 2, 4, 3, 2, fw_xaw),
};

static struct nand_desc foresee_nand = {

	.id_manufactory = ATO_MID,
	.device_counts = ATO_NAND_DEVICD_COUNT,
	.device = device,
};

int foresee_nand_register_func(void) {
	return nand_register(&foresee_nand);
}
