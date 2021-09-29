/*
 * Cryptographic API.
 *
 * Support for Tseries AES HW.
 *
 * Copyright (c) 2019 Ingenic Corporation
 * Author: Niky shen <xianghui.shen@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/crypto.h>
#include <linux/interrupt.h>
#include <crypto/scatterwalk.h>
#include <crypto/aes.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <soc/base.h>
#include <soc/irq.h>
//#include <mach/platform.h>
#include "jz-aes.h"

/*#define DEBUG*/
#ifdef DEBUG
#define aes_debug(format, ...) {printk(format, ## __VA_ARGS__);}
#else
#define aes_debug(format, ...) do{ } while(0)
#endif

void* aesmem_v = NULL;
//unsigned int aesmem_p = 0;
//struct aes_para para;

#if 0
struct aes_operation *aes_ope = NULL;
void aes_bit_set(struct aes_operation *aes_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = aes_reg_read(aes_ope, offset);
	tmp |= bit;
	aes_reg_write(aes_ope, offset, tmp);
}

void aes_bit_clr(struct aes_operation *aes_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = aes_reg_read(aes_ope, offset);
	tmp &= ~(bit);
	aes_reg_write(aes_ope, offset, tmp);
}
#endif

static void debug_regs(struct aes_operation *aes)
{
	printk("AES_ASCR = 0x%08x\n", aes_reg_read(aes, AES_ASCR));
	printk("AES_ASSR = 0x%08x\n", aes_reg_read(aes, AES_ASSR));
	printk("AES_ASINTM = 0x%08x\n", aes_reg_read(aes, AES_ASINTM));
	printk("AES_ASSA = 0x%08x\n", aes_reg_read(aes, AES_ASSA));
	printk("AES_ASDA = 0x%08x\n", aes_reg_read(aes, AES_ASDA));
	printk("AES_ASTC = 0x%08x\n", aes_reg_read(aes, AES_ASTC));
}

static int aes_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);
	if(aes->state){
		printk("AES driver is busy, can't be open again!\n");
		return -EBUSY;
	}

	init_completion(&aes->done_comp);
	init_completion(&aes->key_comp);
	aes->state = 1;

	printk("%s: aes open successful!\n", __func__);
	return 0;
}

static int aes_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);

	aes->state = 0;
	return 0;
}

static ssize_t aes_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t aes_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static int aes_start_encrypt_processing(struct aes_operation *aes)
{
	struct aes_para *para = &aes->para;
	int i = 0;
	int ret = 0;
	int offset = 0, len = 0;
	int debug = 0;

	para->donelen = 0;
	para->status = JZ_AES_STATUS_PREPARE;
	if(para->datalen % 16){
		printk("The size of data should be 16bytes aligned!\n");
		return -EINVAL;
	}

	if(para->enworkmode >= IN_UNF_CIPHER_WORK_MODE_OTHER){
		printk("The enworkmode is invalid!\n");
		return -EINVAL;
	}

	para->status = JZ_AES_STATUS_WORKING;
	while(offset < para->datalen){
		len = para->datalen - offset > aes->dma_datalen ? aes->dma_datalen : para->datalen - offset;
		if(copy_from_user(aes->src_addr_v, (void __user *)(para->src + offset), len)){
			ret = -EINVAL;
			break;
		}

		dma_sync_single_for_device(NULL, (dma_addr_t)aes->dst_addr_p, len, DMA_FROM_DEVICE);
		dma_sync_single_for_device(NULL, (dma_addr_t)aes->src_addr_p, len, DMA_TO_DEVICE);

		init_completion(&aes->done_comp);
		init_completion(&aes->key_comp);

		/* Clear AES done and KEY done status */
		aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR));

		/* enable AES dma done and KEY done interrupt */
		aes_reg_write(aes, AES_ASINTM, 0x5);

		/* Clear the IV and KEYS; enable AES module */
		aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<10 | 1 << 0) ;

		if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ECB){
			/* Enable AES DMA, Encrypts, ECB mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<0 | 1<<8);
		}else{
			/* Enable AES DMA, Encrypts, CBC mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<8 | 1<<5 | 1<<0);
			for(i = 0; i < 4; i++)
				aes_reg_write(aes, AES_ASIV, para->aesiv[i]);

			/* write IV init */
			aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<1);
		}

		/* set transfer count */
		aes_reg_write(aes, AES_ASTC, len / 16);

		/* input KEYS */
		for(i = 0; i < 4; i++)
		{
			aes_reg_write(aes, AES_ASKY, para->aeskey[i]);
		}

		/* write KEYS start */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<2);

		/* wait for key done */
		i = 1000;
		while(i--){
			if(!wait_for_completion_interruptible(&aes->key_comp))
				break;
		}

		aes_reg_write(aes, AES_ASSA, aes->src_addr_p);
		aes_reg_write(aes, AES_ASDA, aes->dst_addr_p);

		/* start AES DMA */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<9);

		/* wait for AES done */
		i = 1000;
		debug = 0;
		while(i--){
			if(!wait_for_completion_interruptible(&aes->done_comp)){
				break;
			}
		}

		if(i <= 0){
    		printk("%s[%d]: timeout!\n",__func__,__LINE__);
			break;
		}

		if (copy_to_user((void __user *)(para->dst + offset), aes->dst_addr_v, len)) {
			ret = -EFAULT;
			break;
		}

		offset += len;
		para->donelen = offset;
	}

	para->status = JZ_AES_STATUS_DONE;
	return 0;
}

static int aes_start_decrypt_processing(struct aes_operation *aes)
{
	struct aes_para *para = &aes->para;
	int i = 0;
	int ret = 0;
	int offset = 0, len = 0;
	int debug = 0;

	para->donelen = 0;
	para->status = JZ_AES_STATUS_PREPARE;
	if(para->datalen % 16){
		printk("The size of data should be 16bytes aligned!\n");
		return -EINVAL;
	}

	if(para->enworkmode >= IN_UNF_CIPHER_WORK_MODE_OTHER){
		printk("The enworkmode is invalid!\n");
		return -EINVAL;
	}

	para->status = JZ_AES_STATUS_WORKING;
	while(offset < para->datalen){
		len = para->datalen - offset > aes->dma_datalen ? aes->dma_datalen : para->datalen - offset;
		if(copy_from_user(aes->src_addr_v, (void __user *)(para->src + offset), len)){
			ret = -EINVAL;
			break;
		}

		dma_sync_single_for_device(NULL, (dma_addr_t)aes->dst_addr_p, len, DMA_FROM_DEVICE);
		dma_sync_single_for_device(NULL, (dma_addr_t)aes->src_addr_p, len, DMA_TO_DEVICE);

	init_completion(&aes->done_comp);
	init_completion(&aes->key_comp);
		/* Clear AES done and KEY done status */
		aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR));

		/* enable AES dma done and KEY done interrupt */
		aes_reg_write(aes, AES_ASINTM, 0x5);

		/* Clear the IV and KEYS; enable AES module */
		aes_reg_write(aes, AES_ASCR,3 << 28 | 1<<10 | 1 << 0) ;

		if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ECB){
			/* Enable AES DMA, Decrypts, ECB mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<0 | 1<<4 | 1<<8);
		}else{
			/* Enable AES DMA, Decrypts, CBC mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<8 | 1<<5 | 1<<4 | 1<<0);
			for(i = 0; i < 4; i++)
				aes_reg_write(aes, AES_ASIV, para->aesiv[i]);
			/* write IV init */
			aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<1);
		}

		/* set transfer count */
		aes_reg_write(aes, AES_ASTC, len / 16);

		/* input KEYS */
		for(i = 0; i < 4; i++)
		{
			aes_reg_write(aes, AES_ASKY, para->aeskey[i]);
		}

		/* write KEYS start */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<2);

		/* wait for key done */
		i = 1000;
		while(i--){
			if(!wait_for_completion_interruptible(&aes->key_comp))
				break;
		}

		aes_reg_write(aes, AES_ASSA, aes->src_addr_p);
		aes_reg_write(aes, AES_ASDA, aes->dst_addr_p);

		/* start AES DMA */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<9);

		/* wait for AES done */
		i = 1000;
		debug = 0;
		while(i--){
			if(!wait_for_completion_interruptible(&aes->done_comp)){
				break;
			}
		}

		if(i <= 0){
    		printk("%s[%d]: timeout!\n",__func__,__LINE__);
			break;
		}

		if (copy_to_user((void __user *)(para->dst + offset), aes->dst_addr_v, len)) {
			ret = -EFAULT;
			break;
		}

		offset += len;
		para->donelen = offset;
	}

	para->status = JZ_AES_STATUS_DONE;
	return 0;
}

static long aes_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);
	void __user *argp  = (void __user *)arg;
	int ret = 0;

	switch(cmd) {
	case IOCTL_AES_GET_PARA:
		if (copy_to_user(argp, &aes->para, sizeof(struct aes_para))) {
			printk("aes set para copy_to_user error!!!\n");
			return -EFAULT;
		}
		break;
	case IOCTL_AES_START_EN_PROCESSING:
		if(aes->para.status != JZ_AES_STATUS_DONE){
			printk("aes is busy now!!!\n");
			return -EBUSY;
		}
		if (copy_from_user(&aes->para, argp, sizeof(struct aes_para))) {
			printk("aes get para copy_from_user error!!!\n");
			return -EFAULT;
		}
		ret = aes_start_encrypt_processing(aes);
		if (ret) {
			printk("aes encrypt error!\n");
			return -1;
		}
		if (copy_to_user(argp, &aes->para, sizeof(struct aes_para))) {
			printk("aes set para copy_to_user error!!!\n");
			return -EFAULT;
		}
		break;
	case IOCTL_AES_START_DE_PROCESSING:
		if(aes->para.status != JZ_AES_STATUS_DONE){
			printk("aes is busy now!!!\n");
			return -EBUSY;
		}
		if (copy_from_user(&aes->para, argp, sizeof(struct aes_para))) {
			printk("aes get para copy_from_user error!!!\n");
			return -EFAULT;
		}
		ret = aes_start_decrypt_processing(aes);
		if (ret) {
			printk("aes decrypt error!\n");
			return -1;
		}
		if (copy_to_user(argp, &aes->para, sizeof(struct aes_para))) {
			printk("aes set para copy_to_user error!!!\n");
			return -EFAULT;
		}
		break;
	default:
		break;
	}
	return 0;
}

const struct file_operations aes_fops = {
	.owner = THIS_MODULE,
	.read = aes_read,
	.write = aes_write,
	.open = aes_open,
	.unlocked_ioctl = aes_ioctl,
	.release = aes_release,
};

static irqreturn_t aes_ope_irq_handler(int irq, void *data)
{
	struct aes_operation *aes_ope = data;
	unsigned int status = aes_reg_read(aes_ope, AES_ASSR);
	unsigned int mask = aes_reg_read(aes_ope, AES_ASINTM);
	unsigned int pending = status & mask;

	/* clear interrupt */
	aes_reg_write(aes_ope, AES_ASSR, status);
	if(aes_ope->para.status != JZ_AES_STATUS_WORKING){
		return IRQ_HANDLED;
	}
	if(pending & AES_ASSR_DMAD){
		complete(&aes_ope->done_comp);
	}
	if(pending & AES_ASSR_KEYD){
		complete(&aes_ope->key_comp);
	}

	return IRQ_HANDLED;
}

static int jz_aes_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct aes_operation *aes_ope = NULL;
	aes_ope = (struct aes_operation *)kzalloc(sizeof(struct aes_operation), GFP_KERNEL);
	if (!aes_ope) {
		dev_err(&pdev->dev, "alloc aes mem_region failed!\n");
		return -ENOMEM;
	}
	sprintf(aes_ope->name, "jz-aes");
	aes_debug("%s, aes name is : %s\n",__func__, aes_ope->name);

	aes_ope->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!aes_ope->res) {
		dev_err(&pdev->dev, "failed to get dev resources\n");
		ret = -EINVAL;
		goto failed_get_mem;
	}

	aes_ope->res = request_mem_region(aes_ope->res->start,
			aes_ope->res->end - aes_ope->res->start + 1,
			pdev->name);
	if (!aes_ope->res) {
		dev_err(&pdev->dev, "failed to request regs memory region");
		ret = -EINVAL;
		goto failed_req_region;
	}
	aes_ope->iomem = ioremap(aes_ope->res->start, resource_size(aes_ope->res));
	if (!aes_ope->iomem) {
		dev_err(&pdev->dev, "failed to remap regs memory region\n");
		ret = -EINVAL;
		goto failed_iomap;
	}
	aes_debug("%s, aes iomem is :0x%08x\n", __func__, (unsigned int)aes_ope->iomem);

	aes_ope->irq = platform_get_irq(pdev, 0);
	if (request_irq(aes_ope->irq, aes_ope_irq_handler, IRQF_SHARED, aes_ope->name, aes_ope)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto failed_get_irq;
	}
	aes_debug("%s, aes irq is : %d\n",__func__, aes_ope->irq);

	aes_ope->src_addr_v = dma_alloc_noncoherent(&pdev->dev,
							  JZ_AES_DMA_DATALEN,
							  &aes_ope->src_addr_p,
							  GFP_KERNEL | GFP_DMA);

	if (aes_ope->src_addr_v == NULL){
		printk("failed to alloc dma src buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_src;
	}

	aes_ope->dst_addr_v = dma_alloc_noncoherent(&pdev->dev,
							  JZ_AES_DMA_DATALEN,
							  &aes_ope->dst_addr_p,
							  GFP_KERNEL | GFP_DMA);

	if (aes_ope->dst_addr_v == NULL){
		printk("failed to alloc dma dst buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_dst;
	}
	aes_ope->dma_datalen = JZ_AES_DMA_DATALEN;
	/****************************************************************/
	aes_ope->aes_dev.minor = MISC_DYNAMIC_MINOR;
	aes_ope->aes_dev.fops = &aes_fops;
	aes_ope->aes_dev.name = "aes";

	ret = misc_register(&aes_ope->aes_dev);
	if(ret) {
		dev_err(&pdev->dev,"request misc device failed!\n");
		ret = -EINVAL;
		goto failed_misc;
	}

	aes_ope->clk = clk_get(aes_ope->dev, "aes");
	if (IS_ERR(aes_ope->clk)) {
		ret = dev_err(&pdev->dev, "aes clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}

	clk_enable(aes_ope->clk);

	platform_set_drvdata(pdev, aes_ope);

	init_completion(&aes_ope->done_comp);
	init_completion(&aes_ope->key_comp);

	aes_ope->para.status = JZ_AES_STATUS_DONE;
	aes_debug("%s: probe() done\n", __func__);

	return 0;

failed_clk:
	misc_deregister(&aes_ope->aes_dev);
failed_misc:
	dma_free_noncoherent(&pdev->dev, JZ_AES_DMA_DATALEN,
							  aes_ope->dst_addr_v, aes_ope->dst_addr_p);
failed_alloc_dst:
	dma_free_noncoherent(&pdev->dev, JZ_AES_DMA_DATALEN,
							  aes_ope->src_addr_v, aes_ope->src_addr_p);
failed_alloc_src:
	free_irq(aes_ope->irq, aes_ope);
failed_get_irq:
	iounmap(aes_ope->iomem);
failed_iomap:
	release_mem_region(aes_ope->res->start, aes_ope->res->end - aes_ope->res->start + 1);
failed_req_region:
failed_get_mem:
	kfree(aes_ope);
	return ret;
}

static int jz_aes_remove(struct platform_device *pdev)
{
	struct aes_operation *aes_ope = platform_get_drvdata(pdev);

	misc_deregister(&aes_ope->aes_dev);
	clk_enable(aes_ope->clk);
	clk_put(aes_ope->clk);

	dma_free_noncoherent(&pdev->dev, JZ_AES_DMA_DATALEN,
							  aes_ope->src_addr_v, aes_ope->src_addr_p);
	dma_free_noncoherent(&pdev->dev, JZ_AES_DMA_DATALEN,
							  aes_ope->dst_addr_v, aes_ope->dst_addr_p);
	free_irq(aes_ope->irq, aes_ope);
	iounmap(aes_ope->iomem);
	kfree(aes_ope);

	return 0;
}

static struct platform_driver jz_aes_driver = {
	.probe	= jz_aes_probe,
	.remove	= jz_aes_remove,
	.driver	= {
		.name	= "jz-aes",
		.owner	= THIS_MODULE,
	},
};

static struct resource jz_aes_resources[] = {
	[0] = {
		.start  = AES_IOBASE,
		.end    = AES_IOBASE + 0x28,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_AES,
		.end    = IRQ_AES,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device jz_aes_device = {
	.name   = "jz-aes",
	.id = 0,
	.resource   = jz_aes_resources,
	.num_resources  = ARRAY_SIZE(jz_aes_resources),
};


static int __init jz_aes_mod_init(void)
{
	int ret = 0;
	aes_debug("loading %s driver\n", "jz-aes");

	ret = platform_device_register(&jz_aes_device);
	if(ret){
		printk("Failed to insmod aes device!!!\n");
		return ret;
	}
	return  platform_driver_register(&jz_aes_driver);
}

static void __exit jz_aes_mod_exit(void)
{
	platform_device_unregister(&jz_aes_device);
	platform_driver_unregister(&jz_aes_driver);
}

module_init(jz_aes_mod_init);
module_exit(jz_aes_mod_exit);

MODULE_DESCRIPTION("Ingenic AES hw support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Elvis Wang");

