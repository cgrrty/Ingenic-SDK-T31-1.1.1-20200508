#include <stdio.h>
#include "nand_common.h"

#define ATO_MID			    0x9B
#define ATO_NAND_DEVICD_COUNT	    1


static struct device_struct device[] = {
	DEVICE_STRUCT(0x12, 2048, 2, 0, 0, 0, NULL),
};

static struct nand_desc ato_nand = {

	.id_manufactory = ATO_MID,
	.device_counts = ATO_NAND_DEVICD_COUNT,
	.device = device,
};

int ato_nand_register_func(void) {
	return nand_register(&ato_nand);
}
