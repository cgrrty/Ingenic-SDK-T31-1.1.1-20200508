#ifndef __JZ_HASH_H__
#define __JZ_HASH_H__

#define JZHASH_IOC_MAGIC                    'H'
#define IOCTL_HASH_GET_PARA                 _IOW(JZHASH_IOC_MAGIC, 110, unsigned int)
#define IOCTL_HASH_MD5                      _IOW(JZHASH_IOC_MAGIC, 111, unsigned int)
#define IOCTL_HASH_SHA1                     _IOW(JZHASH_IOC_MAGIC, 112, unsigned int)
#define IOCTL_HASH_SHA224                   _IOW(JZHASH_IOC_MAGIC, 113, unsigned int)
#define IOCTL_HASH_SHA256                   _IOW(JZHASH_IOC_MAGIC, 114, unsigned int)
#define IOCTL_HASH_SHA384                   _IOW(JZHASH_IOC_MAGIC, 115, unsigned int)
#define IOCTL_HASH_SHA512                   _IOW(JZHASH_IOC_MAGIC, 116, unsigned int)

#define HASH_HSCR       0x00    //hash control register
#define HASH_HSSR       0x04    //hash status register
#define HASH_HSINTM     0x08    //hash interupt mask register
#define HASH_HSSA       0x0C    //hash DMA source address
#define HASH_HSTC       0x10    //hash transfer count
#define HASH_HSDI       0x14    //hash data input register
#define HASH_HSDO       0x18    //hash data output register
#define HASH_HSCG       0x1C    //hash clock gate register

#define HASH_HSSR_MRD		(1 << 1)
#define HASH_HSSR_ORD		(1 << 0)

#define CPYPT_OUT_ROUND_MD5     4
#define CRYPT_OUT_ROUND_SHA1    5
#define CRYPT_OUT_ROUNT_SHA224  7
#define CRYPT_OUT_ROUNT_SHA256  8
#define CRYPT_OUT_ROUNT_SHA384  12
#define CRYPT_OUT_ROUND_SHA512  16

#define JZ_HASH_DMA_DATALEN (16 * 1024)
#define miscdev_to_hashops(mdev) (container_of(mdev, struct hash_operation, hash_dev))

struct hash_para{
    unsigned int digest_mode;
    unsigned char *src;
    unsigned char *dst;
    unsigned int plaintext_len;
    unsigned int crypttext_len;
};

typedef struct hash_operation{
    char name[16];
    int state;
    struct miscdevice hash_dev;
    struct resource *io_res;
    struct resource *dma_res;
    void __iomem *iomem;
    struct clk *clk;
    struct device *dev;
    struct hash_para para;

    dma_addr_t src_addr_p;
    void *src_addr_v;
    unsigned int dma_datalen;
    int dma_type;
    int irq;
	struct completion one_comp;
	struct completion done_comp;
}hash_operation_t;

static inline unsigned int hash_reg_read(hash_operation_t *hash_ope, int offset)
{
	//printk("%s, read:0x%08x, val = 0x%08x\n", __func__, hash_ope->iomem + offset, readl(hash_ope->iomem + offset));
	return readl(hash_ope->iomem + offset);
}

static inline void hash_reg_write(hash_operation_t *hash_ope, int offset, unsigned int val)
{
    //printk("%s, write:0x%08x, val = 0x%08x\n", __func__, hash_ope->iomem + offset, val);
	writel(val, hash_ope->iomem + offset);
}

void hash_bit_set(hash_operation_t *hash_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = hash_reg_read(hash_ope, offset);
	tmp |= bit;
	hash_reg_write(hash_ope, offset, tmp);
}

void hash_bit_clr(hash_operation_t *hash_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = hash_reg_read(hash_ope, offset);
	tmp &= ~(bit);
	hash_reg_write(hash_ope, offset, tmp);
}

#endif
