/*
 * Cryptographic API.
 *
 * Support for Tseries AES HW.
 *
 * Copyright (c) 2020 Ingenic Corporation
 * Author: Weijie Xu <weijie.xu@ingenic.com>
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
#include <crypto/hash.h>
#include <linux/delay.h>
#include <mach/jzdma.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <soc/base.h>
#include <soc/irq.h>
#include <mach/platform.h>
#include "jz-hash.h"

#ifdef DEBUG
#define hash_debug(format, ...) {printk(format, ## __VA_ARGS__);}
#else
#define hash_debug(format, ...) do{ } while(0)
#endif

typedef unsigned int u32;

#if 0
static void debug_hash_regs(hash_operation_t *hash)
{
    printk("HSCR   = 0x%08x\n", hash_reg_read(hash, HASH_HSCR));
    printk("HSSR   = 0x%08x\n", hash_reg_read(hash, HASH_HSSR));
    printk("HSINTM = 0x%08x\n", hash_reg_read(hash, HASH_HSINTM));
    printk("HSSA   = 0x%08x\n", hash_reg_read(hash, HASH_HSSA));
    printk("HSTC   = 0x%08x\n", hash_reg_read(hash, HASH_HSTC));
    printk("HSDI   = 0x%08x\n", hash_reg_read(hash, HASH_HSDI));
    printk("HSDO   = 0x%08x\n", hash_reg_read(hash, HASH_HSDO));
    printk("HSCG   = 0x%08x\n", hash_reg_read(hash, HASH_HSCG));
}
#endif

static ssize_t hash_read(struct file *filp, char __user * buffer, size_t count, loff_t * ppos)
{
    return 0;
}

static ssize_t hash_write(struct file *filp,const char __user * buffer, size_t count, loff_t * ppos)
{
    return 0;
}

static int hash_open(struct inode *inode, struct file *filp)
{
    struct miscdevice *dev = filp->private_data;
    hash_operation_t *hash = miscdev_to_hashops(dev);
    if(hash->state){
        printk("HASH driver is busy,can't open again!\n");
        return -EBUSY;
    }
    hash->state = 1;
    printk("%s:hash open successsful!\n",__func__);
    return 0;
}

static int hash_digest_execute(hash_operation_t *hash,int cmd)
{
    struct hash_para *para = &hash->para;
	int ret = 0;
    int offset  = 0;
    int length = 0;
    int round = 0;
    int i = 0;
    u32 *crypttext;
    //MD5 SHA1 SHA224 SHA256 (512bit*N)(64byte*N)
    //SHA384 SHA512          (1024bit*N)(128byte*N)
    if(para->plaintext_len % 64 != 0){
        printk("The plain text must be 512bit or 1024bit aligned!\n");
        return -EINVAL;
    }

	//set register to default
	hash_reg_write(hash,HASH_HSCR,0x30000000);
	hash_reg_write(hash,HASH_HSSR,0x0);
	hash_reg_write(hash,HASH_HSINTM,0x0);
	hash_reg_write(hash,HASH_HSTC,0x0);
	hash_reg_write(hash,HASH_HSSA,0x0);
	hash_reg_write(hash,HASH_HSSR,0x03);
	hash_bit_set(hash, HASH_HSCG, 0x0);

    //clock gate enable
    hash_bit_set(hash, HASH_HSCG, 1 << 0);
    //Degest mode set
    switch(cmd){
        case IOCTL_HASH_MD5:
            round = CPYPT_OUT_ROUND_MD5;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (0 << 1));
            break;
        case IOCTL_HASH_SHA1:
            round = CRYPT_OUT_ROUND_SHA1;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (1 << 1));
            break;
        case IOCTL_HASH_SHA224:
            round = CRYPT_OUT_ROUNT_SHA224;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (2 << 1));
            break;
        case IOCTL_HASH_SHA256:
            round = CRYPT_OUT_ROUNT_SHA256;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (3 << 1));
            break;
        case IOCTL_HASH_SHA384:
            round = CRYPT_OUT_ROUNT_SHA384;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (4 << 1));
            break;
        case IOCTL_HASH_SHA512:
            round = CRYPT_OUT_ROUND_SHA512;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (5 << 1));
            break;
    }
    crypttext = (u32*)kzalloc(round*sizeof(u32),GFP_KERNEL);
    //hash enable
	hash_reg_write(hash,HASH_HSCR,hash_reg_read(hash,HASH_HSCR)|(1 << 0));
	if(cmd == IOCTL_HASH_MD5){
		hash_reg_write(hash,HASH_HSCR,hash_reg_read(hash,HASH_HSCR)|(1 << 8));
	}else{
		hash_reg_write(hash,HASH_HSCR,hash_reg_read(hash,HASH_HSCR)|(1 << 7));
	}

	hash_bit_set(hash,HASH_HSCR, 1<<4);

    while(offset < para->plaintext_len){

        length = para->plaintext_len - offset > hash->dma_datalen ? hash->dma_datalen : para->plaintext_len - offset;
		//clear one round done
		hash_reg_write(hash, HASH_HSSR,0x03);

		//enable multi hash round done interrupt
		hash_reg_write(hash, HASH_HSINTM,0x03);

		//dma enable
		hash_bit_set(hash,HASH_HSCR,1<<5);

		//transfer count :1bloct = 512bit
		hash_reg_write(hash, HASH_HSTC, length/64);

		//set dma source address
		hash_reg_write(hash, HASH_HSSA, hash->src_addr_p);

        if(copy_from_user(hash->src_addr_v, (void __user *)(para->src + offset),length)){
            ret = -EINVAL;
            break;
        }
        dma_sync_single_for_device(NULL, (dma_addr_t)hash->src_addr_p, length, DMA_TO_DEVICE);

		init_completion(&hash->done_comp);
		init_completion(&hash->one_comp);

        //dma start
        hash_bit_set(hash,HASH_HSCR, 1<<6);

		i = 1000;
		while(i--){
			if(!wait_for_completion_interruptible(&hash->done_comp))
				break;
		}

		if(i <= 0){
	         printk("%s[%d]: timeout!\n",__func__,__LINE__);
	         ret = -EFAULT;
	    }

        offset += length;
    }

    for(i = 0;i < round;i ++){
        crypttext[i] = hash_reg_read(hash,HASH_HSDO);
		hash_debug("0x%08x\n",crypttext[i]);
    }
    if(copy_to_user((void __user *)(para->dst), crypttext, round * sizeof(u32))){
        ret = -EFAULT;
    }
	//debug_hash_regs(hash);
    para->crypttext_len = round * sizeof(u32);
    kfree(crypttext);
    return ret;
}

static long hash_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct miscdevice *dev = filp->private_data;
    hash_operation_t *hash = miscdev_to_hashops(dev);
    void __user *argp = (void __user *)arg;
    int ret = 0;
    switch(cmd){
        case IOCTL_HASH_GET_PARA:
            {
                if (copy_to_user(argp, &hash->para, sizeof(struct hash_para))){
                    printk("hash set para copy_to_user error!!!\n");
	                return -EFAULT;
                }
            }
            break;
        case IOCTL_HASH_MD5:
        case IOCTL_HASH_SHA1:
        case IOCTL_HASH_SHA224:
        case IOCTL_HASH_SHA256:
        case IOCTL_HASH_SHA384:
        case IOCTL_HASH_SHA512:
            {
                if (copy_from_user(&hash->para, argp, sizeof(struct hash_para)))
                {
                    printk("hash get para copy_from_user error!!!\n");
	                return -EFAULT;
	            }

                ret = hash_digest_execute(hash,cmd);

                if (ret)
                {
	                printk("hash digest error!\n");
                    return -1;
                }
                if (copy_to_user(argp, &hash->para, sizeof(struct hash_para))) {
		            printk("hash set para copy_to_user error!!!\n");
		            return -EFAULT;
	            }
            }
            break;
        default:
            break;
    }
    return 0;
}

static int hash_release(struct inode *inode, struct file *filp)
{
    struct miscdevice *dev = filp->private_data;
    struct hash_operation *hash = miscdev_to_hashops(dev);
    hash->state = 0;
    return 0;
}

const struct file_operations hash_fops = {
    .owner = THIS_MODULE,
    .read = hash_read,
    .write = hash_write,
    .open = hash_open,
    .unlocked_ioctl = hash_ioctl,
    .release = hash_release,
};


static irqreturn_t hash_ope_irq_handler(int irq, void *data)
{
	hash_operation_t *hash_ope = data;
	unsigned int status = hash_reg_read(hash_ope, HASH_HSSR);
	unsigned int mask = hash_reg_read(hash_ope, HASH_HSINTM);

	unsigned int pending = status & mask;

	/* clear interrupt */
	hash_debug("mask = %d status = %d\n",mask,status);

	if(pending & HASH_HSSR_ORD){
		hash_bit_set(hash_ope, HASH_HSSR,0x01);
		complete(&hash_ope->done_comp);
	}

	if(pending & HASH_HSSR_MRD){
		hash_bit_set(hash_ope, HASH_HSSR,0x02);
        complete(&hash_ope->done_comp);
    }
	return 0;
}


static int jz_hash_probe(struct platform_device *pdev)
{
    int ret = 0;
    hash_operation_t *hash_ope = (hash_operation_t*)kzalloc(sizeof(hash_operation_t),GFP_KERNEL);
    if(!hash_ope){
        dev_err(&pdev->dev, "alloc hash mem_region failed!\n");
        return -ENOMEM;
    }
    sprintf(hash_ope->name,"jz-hash");
    hash_debug("%s,hash name is : %s\n",__func__,hash_ope->name);

    //platform_device mem resource get
    hash_ope->io_res = platform_get_resource(pdev, IORESOURCE_MEM,0);
    if(!hash_ope->io_res)
    {
        dev_err(&pdev->dev, "failed to get dev io resources\n");
	    ret = -EINVAL;
	    goto failed_get_mem;
    }

    //I/O request
    hash_ope->io_res = request_mem_region(hash_ope->io_res->start,
                hash_ope->io_res->end - hash_ope->io_res->start + 1,
                pdev->name);
    if(!hash_ope->io_res)
    {
        dev_err(&pdev->dev, "failed to request regs memory region");
	    ret = -EINVAL;
	    goto failed_req_region;
    }

    //ioremap
    hash_ope->iomem = ioremap(hash_ope->io_res->start,resource_size(hash_ope->io_res));
    if(!hash_ope->iomem)
    {
        dev_err(&pdev->dev, "failed to remap regs memory region\n");
	    ret = -EINVAL;
	    goto failed_iomap;
    }
    hash_debug("%s, hash iomem is :0x%08x\n", __func__, (unsigned int)hash_ope->iomem);

    //interrupt
    hash_ope->irq = platform_get_irq(pdev, 0);
    if (request_irq(hash_ope->irq, hash_ope_irq_handler, IRQF_SHARED, hash_ope->name, hash_ope)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto failed_get_irq;
	}
	hash_debug("%s, hash irq is : %d\n",__func__, hash_ope->irq);

    //dma
    hash_ope->src_addr_v = dma_alloc_noncoherent(&pdev->dev,
                                JZ_HASH_DMA_DATALEN,
                                &hash_ope->src_addr_p,
                                GFP_KERNEL | GFP_DMA);
    if (hash_ope->src_addr_v == NULL)
    {
	    printk("failed to alloc dma src buffer\n");
	    ret = -ENOMEM;
	    goto failed_alloc_dma;
    }
    hash_ope->dma_datalen = JZ_HASH_DMA_DATALEN;

    hash_ope->hash_dev.minor = MISC_DYNAMIC_MINOR;
    hash_ope->hash_dev.fops  = &hash_fops;
    hash_ope->hash_dev.name  = "hash";
    hash_ope->dev = &pdev->dev;

    ret = misc_register(&hash_ope->hash_dev);
    if(ret)
    {
        dev_err(&pdev->dev,"request misc device failed!\n");
		ret = -EINVAL;
		goto failed_misc;
    }

    hash_ope->clk = clk_get(hash_ope->dev,"hash");
    if (IS_ERR(hash_ope->clk))
    {
		ret = dev_err(&pdev->dev, "hash clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
    clk_enable(hash_ope->clk);

    platform_set_drvdata(pdev, hash_ope);

    hash_debug("%s: probe() done\n", __func__);

    return 0;

failed_clk:
    misc_deregister(&hash_ope->hash_dev);
failed_misc:
    dma_free_noncoherent(&pdev->dev, JZ_HASH_DMA_DATALEN,hash_ope->src_addr_v, hash_ope->src_addr_p);
failed_alloc_dma:
    free_irq(hash_ope->irq, hash_ope);
failed_get_irq:
    iounmap(hash_ope->iomem);
failed_iomap:
    release_mem_region(hash_ope->io_res->start, hash_ope->io_res->end - hash_ope->io_res->start + 1);
failed_req_region:
failed_get_mem:
    kfree(hash_ope);
    return ret;
}

static int jz_hash_remove(struct platform_device *pdev)
{
    hash_operation_t *hash_ope = platform_get_drvdata(pdev);
    misc_deregister(&hash_ope->hash_dev);
    clk_enable(hash_ope->clk);
    clk_put(hash_ope->clk);
    dma_free_noncoherent(&pdev->dev, JZ_HASH_DMA_DATALEN, hash_ope->src_addr_v, hash_ope->src_addr_p);
    iounmap(hash_ope->iomem);
    kfree(hash_ope);
    return 0;
}

static struct platform_driver jz_hash_driver = {
    .probe = jz_hash_probe,
    .remove = jz_hash_remove,
    .driver = {
        .name = "jz-hash",
        .owner = THIS_MODULE,
    }
};

static struct resource jz_hash_resources[] = {
    [0] = {
	    .start  = HASH_IOBASE,
	    .end    = HASH_IOBASE + 0x20,
	    .flags  = IORESOURCE_MEM,
    },
    [1] = {
		.start  = IRQ_HASH,
		.end    = IRQ_HASH,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device jz_hash_device = {
    .name   = "jz-hash",
    .id = 0,
    .resource   = jz_hash_resources,
    .num_resources  = ARRAY_SIZE(jz_hash_resources),
};

static int __init jz_hash_mod_init(void)
{
    int ret = 0;
    hash_debug("loading %s driver\n", "jz-hash");

    ret = platform_device_register(&jz_hash_device);
    if(ret){
	    printk("Failed to insmod hash device driver!!!\n");
	    return ret;
    }
    return platform_driver_register(&jz_hash_driver);
}

static void __exit jz_hash_mod_exit(void)
{
    platform_device_unregister(&jz_hash_device);
    platform_driver_unregister(&jz_hash_driver);
}

module_init(jz_hash_mod_init);
module_exit(jz_hash_mod_exit);

MODULE_DESCRIPTION("Ingenic HASH hw support");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Weijie Xu");
