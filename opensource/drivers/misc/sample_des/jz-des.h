#ifndef __JZ_DES_H__
#define __JZ_DES_H__

#define JZDES_IOC_MAGIC  'D'
#define IOCTL_DES_GET_PARA			_IOW(JZDES_IOC_MAGIC, 110, unsigned int)
#define IOCTL_DES_START_EN_PROCESSING		_IOW(JZDES_IOC_MAGIC, 111, unsigned int)
#define IOCTL_DES_START_DE_PROCESSING		_IOW(JZDES_IOC_MAGIC, 112, unsigned int)

#define DESCR1    0x00   //DES control register 1
#define DESCR2    0x04   //DES control register 2
#define DESSR     0x08   //DES status register
#define DESK1L    0x10   //DES key register 1L
#define DESK1R    0x14   //DES key register 1R
#define DESK2L    0x18   //DES key register 2L
#define DESK2R    0x1C   //DES key register 2R
#define DESK3L    0x20   //DES key register 3L
#define DESK3R    0x24   //DES key register 3R
#define DESIVL    0x28   //DES IV(initial vector)register L
#define DESIVR    0x2C   //DES IV(initial vector)register R
#define DESDIN    0x30   //DES data input register
#define DESDOUT   0x34   //DES data output register

typedef enum IN_UNF_CIPHER_WORK_MODE_E
{
	IN_UNF_CIPHER_WORK_MODE_ECB = 0x0,
	IN_UNF_CIPHER_WORK_MODE_CBC = 0x1,
	IN_UNF_CIPHER_WORK_MODE_OTHER = 0x2
}IN_UNF_CIPHER_WORK_MODE;

typedef enum JZ_DES_PROCESS_E {
	JZ_DES_PROCESS_ENCRYPT = 0x0,
	JZ_DES_PROCESS_DECRYPT = 0x1,
} JZ_DES_PROCESS;

typedef enum JZ_DES_STATUS_E{
	JZ_DES_STATUS_STOP = 0,
	JZ_DES_STATUS_WORK = 1
}JZ_DES_STATUS;

typedef enum JZ_DES_ALGORITHME_E{
	JZ_DES_ALGORITHME_SDES = 0,
	JZ_DES_ALGORITHME_TDES = 1,
	JZ_DES_ALGORITHME_OTHER = 2
}JZ_DES_ALGORITHME;

struct des_para {
	unsigned int status;
	unsigned int enworkmode;
	unsigned int algorithms;
	unsigned int desiv[2];
	unsigned int deskeys[6];
        unsigned int datalen;
        unsigned char *src;
	unsigned char *dst;
	unsigned int donelen;
};

struct des_operation{
	struct miscdevice des_dev;
	struct resource *io_res;
	struct resource *dma_res_tx;
        struct resource *dma_res_rx;

	void __iomem *iomem;
	struct clk *clk;
	struct device *dev;
	char name[16];
	int state;
        int ioctl_state;
	struct des_para para;

	int dma_type;

	/*DMA Tx*/
	struct dma_slave_config dma_config_tx;
	struct dma_async_tx_descriptor *desc_tx;
	struct dma_chan *dma_chan_tx;
	/*DMA Rx*/
        struct dma_slave_config dma_config_rx;
        struct dma_async_tx_descriptor *desc_rx;
        struct dma_chan *dma_chan_rx;

        //des input dma buffer
	dma_addr_t src_addr_p;
	void *src_addr_v;
        //des output dma buffer
	dma_addr_t dst_addr_p;
	void *dst_addr_v;

	unsigned int dma_datalen;

	struct completion dma_tx_done_comp;
        struct completion dma_rx_done_comp;
        struct completion done_comp;
};

#define JZ_DES_DMA_DATALEN (16 * 1024)
#define miscdev_to_desops(mdev) (container_of(mdev, struct des_operation, des_dev))


static inline unsigned int des_reg_read(struct des_operation *des_ope, int offset)
{
	//printk("%s, read:0x%08x, val = 0x%08x\n", __func__, des_ope->iomem + offset, readl(des_ope->iomem + offset));
	return readl(des_ope->iomem + offset);
}

static inline void des_reg_write(struct des_operation *des_ope, int offset, unsigned int val)
{
	writel(val, des_ope->iomem + offset);
	//printk("%s, write:0x%08x, val = 0x%08x\n", __func__, des_ope->iomem + offset, val);
}

void des_bit_set(struct des_operation *des_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = des_reg_read(des_ope, offset);
	tmp |= bit;
	des_reg_write(des_ope, offset, tmp);
}

void des_bit_clr(struct des_operation *des_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = des_reg_read(des_ope, offset);
	tmp &= ~(bit);
	des_reg_write(des_ope, offset, tmp);
}

#define JZ_DES_STATE_STOP              des_bit_clr( des, DESCR1, 0x01)
#define JZ_DES_STATE_WORK              des_bit_set( des, DESCR1, 0x01)
#define JZ_DES_ENDIAN_LITTLE           des_bit_set( des, DESCR2, 0x10)
#define JZ_DES_ENDIAN_BIG              des_bit_clr( des, DESCR2, 0x10)
#define JZ_DES_PROCESS_ENCRYPTION      des_bit_clr( des, DESCR2, 0x08)
#define JZ_DES_PROCESS_DECRYPTION      des_bit_set( des, DESCR2, 0x08)
#define JZ_DES_DMA_ENABLE              des_bit_set( des, DESCR2, 0x01)
#define JZ_DES_DMA_DISABLE             des_bit_clr( des, DESCR2, 0x01)
#define JZ_DES_MODE_ECB                des_bit_clr( des, DESCR2, 0x04)
#define JZ_DES_MODE_CBC                des_bit_set( des, DESCR2, 0x04)
#define JZ_DES_ALGORITHM_SDES          des_bit_clr( des, DESCR2, 0X02)
#define JZ_DES_ALGORITHM_TDES          des_bit_set( des, DESCR2, 0X02)

#endif
