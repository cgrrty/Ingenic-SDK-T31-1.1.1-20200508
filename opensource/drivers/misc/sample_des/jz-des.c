/*
 * Cryptographic API.
 *
 * Support for Tseries AES HW.
 *
 * Copyright (c) 2019 Ingenic Corporation
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
#include <crypto/des.h>
#include <linux/delay.h>
#include <mach/jzdma.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <soc/base.h>
#include <soc/irq.h>
#include <mach/platform.h>
#include "jz-des.h"



#ifdef DEBUG
#define des_debug(format, ...) {printk(format, ## __VA_ARGS__);}
#else
#define des_debug(format, ...) do{ } while(0)
#endif

#if 0
static void debug_des_regs(struct des_operation *des)
{
    printk("DESCR1  = 0x%08x\n", des_reg_read(des, DESCR1));
    printk("DESCR2  = 0x%08x\n", des_reg_read(des, DESCR2));
    printk("DESSR   = 0x%08x\n", des_reg_read(des, DESSR));
    printk("DES1L   = 0x%08x\n", des_reg_read(des, DESK1L));
    printk("DES1R   = 0x%08x\n", des_reg_read(des, DESK1R));
    printk("DES2L   = 0x%08x\n", des_reg_read(des, DESK2L));
    printk("DES2R   = 0x%08x\n", des_reg_read(des, DESK2R));
    printk("DES3L   = 0x%08x\n", des_reg_read(des, DESK3L));
    printk("DES3R   = 0x%08x\n", des_reg_read(des, DESK3R));
    printk("DESIVL  = 0x%08x\n", des_reg_read(des, DESIVL));
    printk("DESIVR  = 0x%08x\n", des_reg_read(des, DESIVR));
    printk("DESDIN  = 0x%08x\n", des_reg_read(des, DESDIN));
    printk("DESDOUT = 0x%08x\n", des_reg_read(des, DESDOUT));
}
#endif

static int des_open(struct inode *inode,struct file *filp)
{
    struct miscdevice *dev = filp->private_data;
    struct des_operation *des = miscdev_to_desops(dev);
    if(des->state)
    {
        printk("DES driver is busy,can't open again!\n");
        return -EBUSY;
    }
    des->state = 1;
    //printk("%s:des open successsful!\n",__func__);
    return 0;
}

static ssize_t des_read(struct file *filp, char __user * buffer, size_t count, loff_t * ppos)
{
    return 0;
}

static ssize_t des_write(struct file *filp,const char __user * buffer, size_t count, loff_t * ppos)
{
    return 0;
}

static int des_release(struct inode *inode, struct file *file)
{
    struct miscdevice *dev = file->private_data;
    struct des_operation *des = miscdev_to_desops(dev);
    des->state = 0;
    return 0;
}

static int des_register_config(struct des_operation *des,int flags)
{
    int ret = 0;
    int reg_offset = 0;
    int i = 0;
    struct des_para *para = &des->para;
    //DES work mode
    JZ_DES_STATE_STOP;
    para->status = JZ_DES_STATUS_STOP;
    //little endian
    JZ_DES_ENDIAN_LITTLE;

    if(flags == JZ_DES_PROCESS_ENCRYPT)
    {
        JZ_DES_PROCESS_ENCRYPTION;
    }
    else
    {
        JZ_DES_PROCESS_DECRYPTION;
    }

    if(para->datalen >= 32)
    {
        JZ_DES_DMA_ENABLE;
    }
    else
    {
        JZ_DES_DMA_DISABLE;
    }

    //only support ECB and CBC mode
    if(para->enworkmode >= IN_UNF_CIPHER_WORK_MODE_OTHER)
    {
        printk("The enworkmode is invalid!\n");
        return -EINVAL;
    }

    if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ECB)
    {
        JZ_DES_MODE_ECB;
    }
    else//CBC mode
    {
        JZ_DES_MODE_CBC;
    }

    if(para->algorithms >= JZ_DES_ALGORITHME_OTHER)
    {
        printk("The encrypt algorithms is invalid!\n");
        return -EINVAL;
    }

    if(para->algorithms == JZ_DES_ALGORITHME_SDES)
    {
        JZ_DES_ALGORITHM_SDES;
        des_reg_write( des, DESK1L, para->deskeys[0]);
        des_reg_write( des, DESK1R, para->deskeys[1]);
    }
    else
    {
        JZ_DES_ALGORITHM_TDES;
        reg_offset = DESK1L;
        for(i = 0; i < 6; i++)
        {
            des_reg_write( des, reg_offset, para->deskeys[i]);
            reg_offset += 0x04;
        }
    }
    return ret;
}

static void des_dma_tx_callback(void *data)
{
    //printk("%s\n", __func__);
    struct completion *dma_tx = data;
    complete(dma_tx);
}

static void des_dma_rx_callback(void *data)
{
    //printk("%s\n", __func__);
    struct completion *dma_rx = data;
    complete(dma_rx);
}

static bool dma_chan_filter(struct dma_chan *chan, void *filter_param)
{
    struct des_operation *des = (struct des_operation*)filter_param;
    return des->dma_type == (int)chan->private;
}

static int des_dma_submit_slave_sg(struct des_operation *des)
{
    unsigned long flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
    unsigned long dma_slave_transfer_len = 0;
    dma_slave_transfer_len = des->para.datalen - des->para.donelen > JZ_DES_DMA_DATALEN ? JZ_DES_DMA_DATALEN : des->para.datalen - des->para.donelen;

    if(dma_slave_transfer_len % 32 != 0)
    {
        dma_slave_transfer_len = (dma_slave_transfer_len / 32 + 1) * 32;
    }

    des->desc_tx = dmaengine_prep_slave_single(des->dma_chan_tx,des->src_addr_p ,dma_slave_transfer_len,DMA_MEM_TO_DEV, flags);
    if (!des->desc_tx)
    {
        printk("failed to prepare dma_tx desc\n");
        return -1;
    }
    des->desc_rx = dmaengine_prep_slave_single(des->dma_chan_rx, des->dst_addr_p,dma_slave_transfer_len,DMA_DEV_TO_MEM, flags);
    if (!des->desc_rx)
    {
        printk("failed to prepare dma_rx desc\n");
        return -1;
    }

    des->desc_tx->callback = des_dma_tx_callback;
    des->desc_tx->callback_param = &des->dma_tx_done_comp;
    des->desc_rx->callback = des_dma_rx_callback;
    des->desc_rx->callback_param = &des->dma_rx_done_comp;

    if(dmaengine_submit(des->desc_tx) < 0)
    {
        printk("DMA tx Request error\n");
        return -1;
    }

    if(dmaengine_submit(des->desc_rx) < 0)
    {
        printk("DMA rx Request error\n");
        return -1;
    }
    return 0;
}

static void des_dma_channel_release(struct des_operation *des)
{
    dma_release_channel(des->dma_chan_tx);
    dma_release_channel(des->dma_chan_rx);
}

static int des_dma_prepare(struct des_operation *des)
{
    dma_cap_mask_t mask_tx,mask_rx;
    dma_cap_zero(mask_tx);
    dma_cap_set(DMA_SLAVE,mask_tx);
    des->dma_chan_tx = dma_request_channel(mask_tx,dma_chan_filter,(void *)des);
    if(!des->dma_chan_tx)
    {
        printk("Request DMA TX channel error!\n");
        return -1;
    }

    dma_cap_zero(mask_rx);
    dma_cap_set(DMA_SLAVE,mask_rx);
    des->dma_chan_rx = dma_request_channel(mask_rx,dma_chan_filter,(void *)des);
    if(!des->dma_chan_rx)
    {
        printk("Request DMA RX channel error!\n");
        return -1;
    }

    des->dma_config_tx.direction = DMA_MEM_TO_DEV;
    //des->dma_config_tx.src_addr = des->src_addr_p;
    des->dma_config_tx.dst_addr = (dma_addr_t)(DES_IOBASE + DESDIN);
    des->dma_config_tx.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    des->dma_config_tx.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    des->dma_config_tx.src_maxburst = 32;
    des->dma_config_tx.dst_maxburst = 32;

    dmaengine_slave_config(des->dma_chan_tx,&des->dma_config_tx);

    des->dma_config_rx.direction = DMA_DEV_TO_MEM;
    des->dma_config_rx.src_addr = (dma_addr_t)(DES_IOBASE + DESDOUT);
    //des->dma_config_rx.dst_addr = des->dst_addr_p;
    des->dma_config_rx.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    des->dma_config_rx.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    des->dma_config_rx.src_maxburst = 32;
    des->dma_config_rx.dst_maxburst = 32;

    dmaengine_slave_config(des->dma_chan_rx,&des->dma_config_rx);

    return 0;
}


static int des_cpu_operation(struct des_operation *des)
{
    int i = 0;
    int ret = 0;
    struct des_para *para = &des->para;
    unsigned int cpu_buffer[32];

    if(copy_from_user(cpu_buffer, (void __user *)(para->src), para->datalen))
    {
        ret = -EINVAL;
    }

    if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_CBC)
    {
        des_reg_write( des, DESIVL, para->desiv[0]);
        des_reg_write( des, DESIVR, para->desiv[1]);
    }

    //DES go into work mode
    JZ_DES_STATE_WORK;
    para->status = JZ_DES_STATUS_WORK;
    //set CR1 TO 1,will clear in/out_fifo
    for(i = 0; i < para->datalen / 4; i++)
    {
         des_reg_write(des, DESDIN, cpu_buffer[i]);
    }

    for(i = 0; i < para->datalen / 4; i++)
    {
         while(des_reg_read(des,DESSR) & 0x01);
         cpu_buffer[i] = des_reg_read(des, DESDOUT);
    }

    JZ_DES_STATE_STOP;
    para->status = JZ_DES_STATUS_STOP;
    if (copy_to_user((void __user *)(para->dst), cpu_buffer, para->datalen))
    {
         ret = -EFAULT;
    }
    para->donelen += para->datalen;

    return ret;
}

static int des_dma_operation(struct des_operation *des)
{
    int len = 0;
    int offset = 0;
    int ret = 0;
    struct des_para *para = &des->para;
    while(offset < para->datalen)
    {
        len = para->datalen - offset > des->dma_datalen ? des->dma_datalen : para->datalen - offset;

        if(copy_from_user(des->src_addr_v, (void __user *)(para->src + offset), len))
        {
            ret = -EINVAL;
            break;
        }

        //DMA sync
        dma_sync_single_for_device(NULL, (dma_addr_t)des->dst_addr_p, len, DMA_FROM_DEVICE);
        dma_sync_single_for_device(NULL, (dma_addr_t)des->src_addr_p, len, DMA_TO_DEVICE);

        des_dma_submit_slave_sg(des);

        init_completion(&des->dma_tx_done_comp);
        init_completion(&des->dma_rx_done_comp);
        if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_CBC)
        {
           des_reg_write( des, DESIVL, para->desiv[0]);
           des_reg_write( des, DESIVR, para->desiv[1]);
        }
        //DES go into work mode
        JZ_DES_STATE_WORK;
        para->status = JZ_DES_STATUS_WORK;

        //start DMA transfer
        dma_async_issue_pending(des->dma_chan_tx);
        dma_async_issue_pending(des->dma_chan_rx);

        wait_for_completion(&des->dma_tx_done_comp);
        wait_for_completion(&des->dma_rx_done_comp);

        //DES exit work mode
        JZ_DES_STATE_STOP;
        para->status = JZ_DES_STATUS_STOP;

        if (copy_to_user((void __user *)(para->dst+offset), des->dst_addr_v, len))
        {
            ret = -EFAULT;
            break;
        }
        offset += len;
        para->donelen = offset;
    }
    return ret;
}

static int des_start_encrypt_processing(struct des_operation *des)
{
    struct des_para *para = &des->para;
    int ret = 0;

    //data process must ensure n * 64bit
    if(para->datalen % 8)
    {
        printk("The size of data should be 8 bytes aligned!\n");
        return -EINVAL;
    }
    des_register_config(des,JZ_DES_PROCESS_ENCRYPT);

    para->donelen = 0;
    if(para->datalen >= 32)
    {
        ret = des_dma_operation(des);
    }
    if(para->datalen < 32)
    {
	ret = des_cpu_operation(des);
    }

    return ret;
}

static int des_start_decrypt_processing(struct des_operation *des)
{
    struct des_para *para = &des->para;
    int ret = 0;

    //data process must ensure n * 64bit
    if(para->datalen % 8)
    {
        printk("The size of data should be 8 bytes aligned!\n");
        return -EINVAL;
    }

    des_register_config(des,JZ_DES_PROCESS_DECRYPT);

    para->donelen = 0;
    if(para->datalen >= 32)
    {
        ret = des_dma_operation(des);
    }
    if(para->datalen < 32)
    {
        ret = des_cpu_operation(des);
    }
    return ret;
}

static long des_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct miscdevice *dev = file->private_data;
    struct des_operation *des = miscdev_to_desops(dev);
    void __user *argp = (void __user *)arg;
    int ret = 0;
    if(des->ioctl_state)
    {
        printk("DES ioctl is busy,Please try again after finished!\n");
        return -EBUSY;
    }
    des->ioctl_state = 1;
    switch(cmd)
    {
        case IOCTL_DES_GET_PARA:
            if (copy_to_user(argp, &des->para, sizeof(struct des_para)))
            {
	        printk("aes set para copy_to_user error!!!\n");
	        return -EFAULT;
	    }
            break;
        case IOCTL_DES_START_EN_PROCESSING:
            if (copy_from_user(&des->para, argp, sizeof(struct des_para)))
            {
                printk("des get para copy_from_user error!!!\n");
	        return -EFAULT;
	    }

            ret = des_start_encrypt_processing(des);

            if (ret)
            {
	        printk("des encrypt error!\n");
                return -1;
            }
            if (copy_to_user(argp, &des->para, sizeof(struct des_para))) {
		printk("des set para copy_to_user error!!!\n");
		return -EFAULT;
	    }
            break;
        case IOCTL_DES_START_DE_PROCESSING:
            if(copy_from_user(&des->para, argp, sizeof(struct des_para)))
            {
                printk("des get para copy_from_user error!!!\n");
                return -EFAULT;
            }
            ret = des_start_decrypt_processing(des);
            if (ret)
            {
                printk("des decrypt error!\n");
                return -1;
            }
  	    if (copy_to_user(argp, &des->para, sizeof(struct des_para))) {
		printk("des set para copy_to_user error!!!\n");
		return -EFAULT;
	    }
	    break;
        default:
            break;
    }
    des->ioctl_state = 0;
    return 0;
}

const struct file_operations des_fops = {
    .owner = THIS_MODULE,
    .read = des_read,
    .write = des_write,
    .open = des_open,
    .unlocked_ioctl = des_ioctl,
    .release = des_release,
};

static int jz_des_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct des_operation *des_ope = NULL;

    des_ope = (struct des_operation*)kzalloc(sizeof(struct des_operation),GFP_KERNEL);
    if(!des_ope)
    {
        dev_err(&pdev->dev, "alloc des mem_region failed!\n");
        return -ENOMEM;
    }
    sprintf(des_ope->name,"jz-des");
    des_debug("%s,des name is : %s\n",__func__,des_ope->name);

    //platform_device dma resource get
    des_ope->dma_res_tx = platform_get_resource(pdev, IORESOURCE_DMA, 0);
    if (!des_ope->dma_res_tx)
    {
        dev_err(&pdev->dev, "failed to get platform dma_tx resource\n");
        ret = -EINVAL;
        goto failed_get_dma;
    }
    des_ope->dma_res_rx = platform_get_resource( pdev, IORESOURCE_DMA, 1);
    if (!des_ope->dma_res_rx)
    {
        dev_err(&pdev->dev, "failed to get platform dma_rx resource\n");
        ret = -EINVAL;
        goto failed_get_dma;
    }
    des_ope->dma_type = GET_MAP_TYPE(des_ope->dma_res_rx->start);

    //platform_device mem resource get
    des_ope->io_res = platform_get_resource(pdev, IORESOURCE_MEM,0);
    if(!des_ope->io_res)
    {
        dev_err(&pdev->dev, "failed to get dev io resources\n");
	ret = -EINVAL;
	goto failed_get_mem;
    }

    //I/O request
    des_ope->io_res = request_mem_region(des_ope->io_res->start,
                des_ope->io_res->end - des_ope->io_res->start + 1,
                pdev->name);
    if(!des_ope->io_res)
    {
        dev_err(&pdev->dev, "failed to request regs memory region");
	ret = -EINVAL;
	goto failed_req_region;
    }
    //ioremap
    des_ope->iomem = ioremap(des_ope->io_res->start,resource_size(des_ope->io_res));
    if(!des_ope->iomem)
    {
        dev_err(&pdev->dev, "failed to remap regs memory region\n");
	ret = -EINVAL;
	goto failed_iomap;
    }
    des_debug("%s, aes iomem is :0x%08x\n", __func__, (unsigned int)aes_ope->iomem);

    //dma
    des_ope->src_addr_v = dma_alloc_noncoherent(&pdev->dev,
                                JZ_DES_DMA_DATALEN,
                                &des_ope->src_addr_p,
                                GFP_KERNEL | GFP_DMA);
    if (des_ope->src_addr_v == NULL)
    {
	printk("failed to alloc dma src buffer\n");
	ret = -ENOMEM;
	goto failed_alloc_src;
    }

    des_ope->dst_addr_v = dma_alloc_noncoherent(&pdev->dev,
				        JZ_DES_DMA_DATALEN,
				        &des_ope->dst_addr_p,
				        GFP_KERNEL | GFP_DMA);
    if (des_ope->dst_addr_v == NULL)
    {
        printk("failed to alloc dma dst buffer\n");
        ret = -ENOMEM;
	goto failed_alloc_dst;
    }
    //request channel and slave config
    if(des_dma_prepare(des_ope) < 0 )
    {
        ret = -EINVAL;
        goto failed_config_dma_channel;
    }

    des_ope->dma_datalen = JZ_DES_DMA_DATALEN;

    des_ope->des_dev.minor = MISC_DYNAMIC_MINOR;
    des_ope->des_dev.fops  = &des_fops;
    des_ope->des_dev.name  = "des";
    des_ope->dev = &pdev->dev;

    ret = misc_register(&des_ope->des_dev);
    if(ret)
    {
        dev_err(&pdev->dev,"request misc device failed!\n");
		ret = -EINVAL;
		goto failed_misc;
    }

    des_ope->clk = clk_get(des_ope->dev,"des");
    if (IS_ERR(des_ope->clk))
    {
		ret = dev_err(&pdev->dev, "des clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
    clk_enable(des_ope->clk);

    platform_set_drvdata(pdev, des_ope);

    des_debug("%s: probe() done\n", __func__);

    return 0;

failed_clk:
    misc_deregister(&des_ope->des_dev);
failed_misc:
    des_dma_channel_release(des_ope);
failed_config_dma_channel:
    dma_free_noncoherent(&pdev->dev, JZ_DES_DMA_DATALEN,des_ope->dst_addr_v, des_ope->dst_addr_p);
failed_alloc_dst:
    dma_free_noncoherent(&pdev->dev, JZ_DES_DMA_DATALEN,des_ope->src_addr_v, des_ope->src_addr_p);
failed_alloc_src:
    iounmap(des_ope->iomem);
failed_iomap:
    release_mem_region(des_ope->io_res->start, des_ope->io_res->end - des_ope->io_res->start + 1);
failed_req_region:
failed_get_mem:
failed_get_dma:
    kfree(des_ope);
    return ret;
}

static int jz_des_remove(struct platform_device *pdev)
{
    struct des_operation *des_ope = platform_get_drvdata(pdev);
    misc_deregister(&des_ope->des_dev);
    clk_enable(des_ope->clk);
    clk_put(des_ope->clk);
    dma_free_noncoherent(&pdev->dev, JZ_DES_DMA_DATALEN, des_ope->dst_addr_v, des_ope->dst_addr_p);
    dma_free_noncoherent(&pdev->dev, JZ_DES_DMA_DATALEN, des_ope->src_addr_v, des_ope->src_addr_p);
    iounmap(des_ope->iomem);
    kfree(des_ope);
    return 0;
}

static struct platform_driver jz_des_driver = {
    .probe = jz_des_probe,
    .remove = jz_des_remove,
    .driver = {
        .name = "jz-des",
        .owner = THIS_MODULE,
    }
};

static struct resource jz_des_resources[] = {
    [0] = {
	.start  = DES_IOBASE,
	.end    = DES_IOBASE + 0x38,
	.flags  = IORESOURCE_MEM,
    },
    [1] = {
        .start  = JZDMA_REQ_DES,
        .end    =JZDMA_REQ_DES,
        .flags  = IORESOURCE_DMA,
    },
    [2] = {
        .start  = JZDMA_REQ_DES,
        .end    = JZDMA_REQ_DES,
        .flags  =  IORESOURCE_DMA,
    },
};

struct platform_device jz_des_device = {
    .name   = "jz-des",
    .id = 0,
    .resource   = jz_des_resources,
    .num_resources  = ARRAY_SIZE(jz_des_resources),
};


static int __init jz_des_mod_init(void)
{
    int ret = 0;
    des_debug("loading %s driver\n", "jz-des");

    ret = platform_device_register(&jz_des_device);
    if(ret){
	printk("Failed to insmod des device!!!\n");
	return ret;
    }
    return  platform_driver_register(&jz_des_driver);
}

static void __exit jz_des_mod_exit(void)
{
    platform_device_unregister(&jz_des_device);
    platform_driver_unregister(&jz_des_driver);
}

module_init(jz_des_mod_init);
module_exit(jz_des_mod_exit);

MODULE_DESCRIPTION("Ingenic DES hw support");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Weijie Xu");

