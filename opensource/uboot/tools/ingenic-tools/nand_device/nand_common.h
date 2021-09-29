#ifndef __NAND_COMMON_H
#define __NAND_COMMON_H

struct device_struct {
	unsigned char device_id;
	unsigned int  page_size;
	unsigned char addr_len;
	unsigned char ecc_bit;
	unsigned char bit_counts;
	unsigned char eccstat_count;
	unsigned char *eccerrstatus;
};

struct nand_desc {
	unsigned char id_manufactory;
	unsigned int  device_counts;
	struct device_struct *device;
};

#define DEVICE_STRUCT(id, pagesize, addrlen, bit, bitcounts,  eccstatcount, err) {  \
		.device_id = id, 	\
		.page_size = pagesize,  \
		.addr_len = addrlen,    \
		.ecc_bit = bit,	\
		.bit_counts = bitcounts,	\
		.eccstat_count = eccstatcount, \
		.eccerrstatus = err,	\
}


int nand_register(struct nand_desc *);
#endif
