/*
 * kernel/drivers/video/jz_fb_v14/jz_fb.c
 *
 * Copyright (c) 2016 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * Core file for Ingenic Display Controller driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/suspend.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <asm/cacheflush.h>
#include <mach/platform.h>
#include <soc/gpio.h>
#include <linux/proc_fs.h>
#include <mach/libdmmu.h>
#include "../include/jzfb.h"
#include "jz_fb.h"
#include "regs.h"
#include "jz_gpio.h"


#include "../include/frame240x240.h"    //nv12 pic
#include "../include/240_240_RGB_565.h" //rgb565 pic

/* #define CONFIG_SLCDC_USE_TE */
/*#define CONFIG_SLCDC_CONTINUA */

/* #define LCDC_TINY_NV12 */
#define DISPLAY_PIC


static void dump_lcdc_registers(struct jzfb *jzfb);
static void jzfb_enable(struct fb_info *info);
static void jzfb_disable(struct fb_info *info);
static int jzfb_set_par(struct fb_info *info);
static void wait_busy(void);
void dump_cpm_reg(void);

static int uboot_inited;
static int showFPS = 0;
static struct jzfb *jzfb;
static const struct fb_fix_screeninfo jzfb_fix  = {
    .id = "jzfb",
    .type = FB_TYPE_PACKED_PIXELS,
    .visual = FB_VISUAL_TRUECOLOR,
    .xpanstep = 0,
    .ypanstep = 1,
    .ywrapstep = 0,
    .accel = FB_ACCEL_NONE,
};

static int jzfb_open(struct fb_info *info, int user)
{
    struct jzfb *jzfb = info->par;

    dev_dbg(info->dev, "####open count : %d\n", ++jzfb->open_cnt);

    return 0;
}

static int jzfb_release(struct fb_info *info, int user)
{
    dev_dbg(info->dev, "####open count : %d\n", --jzfb->open_cnt);

    return 0;
}

    static void
jzfb_videomode_to_var(struct fb_var_screeninfo *var,
                      const struct fb_videomode *mode, int lcd_type)
{
    var->xres = mode->xres;
    var->yres = mode->yres;
    var->xres_virtual = mode->xres;
    var->yres_virtual = mode->yres * NUM_FRAME_BUFFERS;
    var->xoffset = 0;
    var->yoffset = 0;
    var->left_margin = mode->left_margin;
    var->right_margin = mode->right_margin;
    var->upper_margin = mode->upper_margin;
    var->lower_margin = mode->lower_margin;
    var->hsync_len = mode->hsync_len;
    var->vsync_len = mode->vsync_len;
    var->sync = mode->sync;
    var->vmode = mode->vmode & FB_VMODE_MASK;
    if (lcd_type == LCD_TYPE_SLCD) {
        uint64_t pixclk =
            KHZ2PICOS((var->xres + var->left_margin +
                       var->hsync_len) * (var->yres +
                                          var->upper_margin +
                                          var->lower_margin +
                                          var->vsync_len) * 60 / 1000);
        var->pixclock =
            (mode->pixclock < pixclk) ? pixclk : mode->pixclock;
    } else {
        var->pixclock = mode->pixclock;
    }
}

static int jzfb_get_controller_bpp(struct jzfb *jzfb)
{
    switch (jzfb->pdata->bpp) {
    case 18:
    case 24:
        return 32;
    case 15:
        return 16;
    default:
        return jzfb->pdata->bpp;
    }
}

static struct fb_videomode *jzfb_get_mode(struct fb_var_screeninfo *var,
                                          struct fb_info *info)
{
    size_t i;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode = jzfb->pdata->modes;

    for (i = 0; i < jzfb->pdata->num_modes; ++i, ++mode) {
        if (mode->flag & FB_MODE_IS_VGA) {
            if (mode->xres == var->xres &&
                mode->yres == var->yres
                && mode->pixclock == var->pixclock)
                return mode;
        } else {
            if (mode->xres == var->xres && mode->yres == var->yres
                && mode->vmode == var->vmode
                && mode->right_margin == var->right_margin) {
                if (jzfb->pdata->lcd_type != LCD_TYPE_SLCD) {
                    if (mode->pixclock == var->pixclock)
                        return mode;
                } else {
                    return mode;
                }
            }
        }
    }

    return NULL;
}

static int jzfb_prepare_dma_desc(struct fb_info *info)
{
    int i;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode;
    mode = info->mode;

    /* use for double buffer technology  */
    jzfb->frm_size = mode->xres * mode->yres * 2;

    for(i = 0; i < MAX_DESC_NUM; i++) {
#ifdef CONFIG_SLCDC_USE_TE
        if(i >= (MAX_DESC_NUM-1))
            jzfb->framedesc[i]->NextDesAddr = jzfb->framedesc_phys[0];
        else
            jzfb->framedesc[i]->NextDesAddr = jzfb->framedesc_phys[i+1];
#else	/* CONFIG_SLCDC_USE_TE */
        jzfb->framedesc[i]->NextDesAddr = jzfb->framedesc_phys[i];
#endif	/* CONFIG_SLCDC_USE_TE */
        jzfb->framedesc[i]->BufferAddr_RGB_Y = jzfb->vidmem_rgb_y_phys[i];
        jzfb->framedesc[i]->Stride_RGB_Y =  mode->xres;
        jzfb->framedesc[i]->ChainCfg = 0x1;
        jzfb->framedesc[i]->InterruptCtrl = reg_read(jzfb, LCDC_TINY_INTC);
        jzfb->framedesc[i]->BufferAddr_UV = jzfb->vidmem_uv_phys[i];
        jzfb->framedesc[i]->Stride_UV = mode->xres;
    }
    reg_write(jzfb, LCDC_TINY_CHAIN_ADDR, jzfb->framedesc_phys[0]);
    return 0;
}

static int jzfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode;

    if (var->bits_per_pixel != jzfb_get_controller_bpp(jzfb) &&
        var->bits_per_pixel != jzfb->pdata->bpp){
        dev_err(info->dev, "%s var->bits_per_pixel = %d\n", __func__,var->bits_per_pixel);
        return 0;	/* workaround for tizen-2.3 window mananger... */
        /* return -EINVAL; */
    }
    mode = jzfb_get_mode(var, info);
    if (mode == NULL) {
        dev_err(info->dev, "%s get video mode failed\n", __func__);
        return 0;
        /* return -EINVAL; */
    }

    jzfb_videomode_to_var(var, mode, jzfb->pdata->lcd_type);

    switch (jzfb->pdata->bpp) {
    case 16:
        var->red.offset = 11;
        var->red.length = 5;
        var->green.offset = 5;
        var->green.length = 6;
        var->blue.offset = 0;
        var->blue.length = 5;
        break;
    case 17 ... 32:
        if (jzfb->fmt_order == FORMAT_X8B8G8R8) {
            var->red.offset = 0;
            var->green.offset = 8;
            var->blue.offset = 16;
        } else {
            /* default: FORMAT_X8R8G8B8 */
            var->red.offset = 16;
            var->green.offset = 8;
            var->blue.offset = 0;
        }

        var->transp.offset = 24;
        var->transp.length = 8;
        var->red.length = 8;
        var->green.length = 8;
        var->blue.length = 8;
        var->bits_per_pixel = 32;
        break;
    default:
        dev_err(jzfb->dev, "Not support for %d bpp\n",
                jzfb->pdata->bpp);
        break;
    }

    return 0;
}





static void slcd_send_mcu_command(struct jzfb *jzfb,unsigned int content)
{
    unsigned int tmp = reg_read(jzfb, LCDC_SLCD_PANEL_CFG);

    reg_write(jzfb, LCDC_SLCD_CMD, 0<<30 | content);

    reg_write(jzfb, LCDC_SLCD_CMD, 2<<30 | content);

}

void slcd_send_mcu_data(struct jzfb *jzfb ,unsigned int content)
{
    reg_write(jzfb, LCDC_SLCD_CMD, 0<<30 | content);

}



static void slcd_send_mcu_command_data(struct jzfb *jzfb, unsigned long data, unsigned int flag)
{
    int count = 10000;
    while ((reg_read(jzfb, LCDC_SLCD_ST) & SLCD_ST_BUSY) && count--) {
        udelay(10);
        printk("Error\n");
    }
    if (count < 0) {
        dev_err(jzfb->dev, "SLCDC wait busy state wrong");
    }
    reg_write(jzfb, LCDC_SLCD_CMD, flag | data);
}

static void jzfb_slcd_mcu_init(struct fb_info *info)
{
    unsigned int i;
    struct jzfb *jzfb = info->par;
    struct jzfb_platform_data *pdata = jzfb->pdata;
    int count = 10000;

    if (pdata->lcd_type != LCD_TYPE_SLCD)
        return;

    /*
     *set cmd_width and data_width
     * */
    if (pdata->smart_config.length_data_table
        && pdata->smart_config.data_table) {
        for (i = 0; i < pdata->smart_config.length_data_table; i++) {
            reg_write(jzfb, LCDC_SLCD_PANEL_CFG,
                      reg_read(jzfb, LCDC_SLCD_PANEL_CFG) & ~(SLCD_FMT_EN));
            switch (pdata->smart_config.data_table[i].type) {
            case SMART_CONFIG_DATA:
                /* slcd_send_mcu_data(jzfb,pdata->smart_config.data_table[i].value); */
                slcd_send_mcu_command_data(jzfb,
                                           pdata->smart_config.data_table[i].value,
                                           FLAG_PAR);
                break;
            case SMART_CONFIG_CMD:
                /* slcd_send_mcu_command(jzfb,pdata->smart_config.data_table[i].value); */
                slcd_send_mcu_command_data(jzfb,
                                           pdata->smart_config.data_table[i].value,
                                           FLAG_CMD);
                break;
            case SMART_CONFIG_UDELAY:
                udelay(pdata->smart_config.data_table[i].value);
                break;
            default:
                dev_err(jzfb->dev, "Unknow SLCD data type\n");
                break;
            }
        }

        while ((reg_read(jzfb, LCDC_SLCD_ST) & SLCD_ST_BUSY)
               && count--) {
            udelay(10);
        }
        if (count < 0) {
            dev_err(jzfb->dev,
                    "SLCDC wait busy state wrong");
        }

    }

}

static void jzfb_enable(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;

    mutex_lock(&jzfb->lock);
    if (!(jzfb->is_lcd_en)) {
        reg_write(jzfb, LCDC_TINY_CTRL, CHAIN_START);
        if(jzfb->pdata->lcd_type != LCD_TYPE_SLCD)
            reg_write(jzfb, LCDC_TINY_CTRL, TFT_START);
    }
    if(jzfb->pdata->lcd_type == LCD_TYPE_SLCD) {
        uint32_t smart_cfg;

        smart_cfg = reg_read(jzfb, LCDC_SLCD_PANEL_CFG);
        /* smart_cfg |= SLCD_FMT_EN; */
        reg_write(jzfb, LCDC_SLCD_PANEL_CFG, smart_cfg);
        /* when CHAINcfg is sigle mode, CHAIN_START must be setted */
        reg_write(jzfb, LCDC_TINY_CTRL, CHAIN_START);

        reg_write(jzfb, LCDC_TINY_CTRL, SLCD_START);
    }

    if (jzfb->is_lcd_en) {
        mutex_unlock(&jzfb->lock);
        return;
    }

    jzfb->is_lcd_en = 1;
    jzfb->is_frm_end = 0;
    mutex_unlock(&jzfb->lock);
}

static void jzfb_disable(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;

    mutex_lock(&jzfb->lock);
    if(!jzfb->is_lcd_en) {
        mutex_unlock(&jzfb->lock);
        return;
    }

    if(!(reg_read(jzfb, LCDC_TINY_INTC) & FRM_END)) {
        while(!(reg_read(jzfb, LCDC_TINY_ST) & FRM_END))
            printk("wait frame end.\n");
        reg_write(jzfb, LCDC_TINY_CTRL, QCK_STOP);
        reg_write(jzfb, LCDC_TINY_CSR, FRM_END);
        down(&jzfb->dis_lock);
    } else if((reg_read(jzfb, LCDC_DISP_COMMON) & DISP_COM_SLCD)
              && !(reg_read(jzfb, LCDC_SLCD_PANEL_CFG) & SLCD_CONTINUA_MODE)
              && jzfb->is_frm_end){
        reg_write(jzfb, LCDC_TINY_CTRL, QCK_STOP);
        down(&jzfb->dis_lock);
    }

    jzfb->is_lcd_en = 0;
    mutex_unlock(&jzfb->lock);
}

static void jzfb_chain_start(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;

    if(!(reg_read(jzfb, LCDC_TINY_ST) & WORKING))
        reg_write(jzfb, LCDC_TINY_CTRL, CHAIN_START);
    else
        printk("Dma chain has started!\n");
}

static void jzfb_tft_start(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;

    reg_write(jzfb, LCDC_TINY_CTRL, TFT_START);
}

static void jzfb_slcd_start(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;

    reg_write(jzfb, LCDC_TINY_CTRL, SLCD_START);
}

static void jzfb_qck_stop(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;

    reg_write(jzfb, LCDC_TINY_CTRL, QCK_STOP);
}

static void jzfb_gen_stop(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;

    reg_write(jzfb, LCDC_TINY_CTRL, GEN_STOP);
}

#if 0
static void jzfb_tft_8766_set_gpio(void)
{
    REG_GPIO_PXINTC(2)  = 0xffffffff;
    REG_GPIO_PXMASKC(2) = 0xffffffff;
    REG_GPIO_PXPAT1C(2) = 0xffffffff;
    REG_GPIO_PXPAT0C(2) = 0xffffffff;

    REG_GPIO_PXINTC(4)  = 0x0000000f;
    REG_GPIO_PXMASKS(4) = 0x0000000f;
    REG_GPIO_PXPAT1C(4) = 0x0000000f;
    REG_GPIO_PXPAT0S(4) = 0x0000000f;
}

static void jzfb_slcd_truly240_set_gpio(void)
{
    REG_GPIO_PXINTC(1)  = 0x21EFC0;
    REG_GPIO_PXMASKC(1) = 0x21EFC0;
    REG_GPIO_PXPAT1S(1) = 0x21EFC0;
    REG_GPIO_PXPAT0C(1) = 0x21EFC0;

    REG_GPIO_PXINTC(1)  = (1 << 20);	//CS set 1
    REG_GPIO_PXMASKS(1) = (1 << 20);
    REG_GPIO_PXPAT1C(1) = (1 << 20);
    REG_GPIO_PXPAT0S(1) = (1 << 20);

    REG_GPIO_PXINTC(1)  = (1 << 22);	//RD set 1
    REG_GPIO_PXMASKS(1) = (1 << 22);
    REG_GPIO_PXPAT1C(1) = (1 << 22);
    REG_GPIO_PXPAT0S(1) = (1 << 22);

    REG_GPIO_PXINTC(1)  = (1 << 19);	//RST set 0
    REG_GPIO_PXMASKS(1) = (1 << 19);
    REG_GPIO_PXPAT1C(1) = (1 << 19);
    REG_GPIO_PXPAT0C(1) = (1 << 19);
    udelay(100000);
    REG_GPIO_PXINTC(1)  = (1 << 19);
    REG_GPIO_PXMASKS(1) = (1 << 19);
    REG_GPIO_PXPAT1C(1) = (1 << 19);
    REG_GPIO_PXPAT0S(1) = (1 << 19);

    REG_GPIO_PXINTC(1)  = (1 << 20);	//CS set 0
    REG_GPIO_PXMASKS(1) = (1 << 20);
    REG_GPIO_PXPAT1C(1) = (1 << 20);
    REG_GPIO_PXPAT0C(1) = (1 << 20);
}
#endif
static int jzfb_tft_set_par(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;
    struct jzfb_platform_data *pdata = jzfb->pdata;
    struct fb_videomode *mode;
    uint32_t hds, vds;
    uint32_t hde, vde;
    uint32_t hps, vps;
    uint32_t hpe, vpe;
    uint32_t tft_cfg;
    uint32_t cfg;

    cfg = 0;
    tft_cfg = 0;
    mode = info->mode;

    /* jzfb_tft_8766_set_gpio(); */

    hps = mode->hsync_len;
    hpe = mode->hsync_len + mode->left_margin + mode->xres + mode->right_margin;
    vps = mode->vsync_len;
    vpe = mode->vsync_len + mode->upper_margin + mode->yres + mode->lower_margin;

    hds = mode->hsync_len + mode->left_margin;
    hde = hds + mode->xres;
    vds = mode->vsync_len + mode->upper_margin;
    vde = vds + mode->yres;

    if(jzfb->fmt_order == FORMAT_X8B8G8R8)
        tft_cfg |= TFT_LCD_COLOR_EVEN_BGR | TFT_LCD_COLOR_ODD_BGR;
    else
        tft_cfg |= TFT_LCD_COLOR_EVEN_RGB | TFT_LCD_COLOR_ODD_RGB;

    if (pdata->lcd_type == LCD_TYPE_GENERIC_24_BIT) {
        tft_cfg |= TFT_LCD_MODE_24_BIT;
    } else if(pdata->lcd_type == LCD_TYPE_8BIT_SERIAL) {
        tft_cfg |= TFT_LCD_MODE_8_BIT_RGB;
    } else {
        return -1;
    }

    jzfb_prepare_dma_desc(info);

#ifdef LCDC_TINY_NV12
    cfg |= LCD_GLB_FORMAT_NV12;
    cfg |= DMA_SEL_NV12;
#else /* LCDC_TINY_NV12 */
    cfg |= LCD_GLB_COLOR_RGB;
    switch (jzfb->pdata->bpp) {
    case 15:
        cfg |= LCD_GLB_FORMAT_555;
        break;
    case 16:
        cfg |= LCD_GLB_FORMAT_565;
        break;
    case 24:
        cfg |= LCD_GLB_FORMAT_888;
        break;
    default:
        printk("ERR: please check out your bpp config\n");
        break;
    }
    /*cfg |= CLKGATE_CLS;*/
    /* cfg |= BURST_LEN; */
    cfg |= DMA_SEL_RGB;
#endif /* LCDC_TINY_NV12 */

    mutex_lock(&jzfb->lock);
    reg_write(jzfb, LCDC_TFT_TIMING_HSYNC, (hps << 16) | hpe);
    reg_write(jzfb, LCDC_TFT_TIMING_VSYNC, (vps << 16) | vpe);
    reg_write(jzfb, LCDC_TFT_TIMING_HDE, (hds << 16) | hde);
    reg_write(jzfb, LCDC_TFT_TIMING_VDE, (vds << 16) | vde);
    reg_write(jzfb, LCDC_TFT_TRAN_CFG, tft_cfg);

    reg_write(jzfb, LCDC_TINY_GLB_CFG, cfg);
    mutex_unlock(&jzfb->lock);

    return 0;
}

static int jzfb_slcd_set_par(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;
    struct jzfb_platform_data *pdata = jzfb->pdata;
    struct fb_videomode *mode;
    uint32_t cfg;
    uint32_t smart_cfg;
    uint32_t smart_frm_size;
    uint32_t smart_wtime;
    uint32_t smart_timing;
    uint32_t smart_slowtime;
    mode = info->mode;

    cfg = 0;
    smart_cfg = 0;
    smart_wtime = 0;
    smart_timing = 0;
    smart_slowtime = 0;
    smart_frm_size = 0;

    switch(pdata->smart_config.bus_width){
    case 8:
        smart_cfg |= SMART_LCD_CWIDTH_8_BIT;
        smart_cfg |= SMART_LCD_DWIDTH_8_BIT;
        break;
    case 9:
        smart_cfg |= SMART_LCD_CWIDTH_9_BIT;
        smart_cfg |= SMART_LCD_DWIDTH_9_BIT;
        break;
    case 16:
        smart_cfg |= SMART_LCD_CWIDTH_16_BIT;
        smart_cfg |= SMART_LCD_DWIDTH_16_BIT;
        break;
    case 18:
        smart_cfg |= SMART_LCD_CWIDTH_18_BIT;
        smart_cfg |= SMART_LCD_DWIDTH_18_BIT;
        break;
    case 24:
        smart_cfg |= SMART_LCD_CWIDTH_24_BIT;
        smart_cfg |= SMART_LCD_DWIDTH_24_BIT;
        break;
    default:
        printk("ERR: please check out your bus width config\n");
        break;
    }

    if (pdata->smart_config.clkply_active_rising)
        smart_cfg |= SLCD_CLK_PLY;
    if (pdata->smart_config.rsply_cmd_high)
        smart_cfg |= SLCD_DC_MD;
    if (pdata->smart_config.csply_active_high)
        smart_cfg |= (1 << 11);

    smart_cfg |= SLCD_CS_DP;
    smart_cfg |= SLCD_RDY_DP;
    /* debug WR active status  */
    smart_cfg |= SLCD_WR_DP;
    /* smart_cfg &= ~SLCD_WR_DP; */
    smart_cfg &= ~SLCD_SWITCH_WAIT_TE;
    smart_cfg |= SLCD_CONTINUA_MODE;

#ifdef CONFIG_SLCDC_CONTINUA
    smart_cfg |= SLCD_CONTINUA_MODE;
#endif

    switch (jzfb->pdata->bpp) {
    case 16:
        smart_cfg |= SMART_LCD_FORMAT_565;
        break;
    case 18:
        smart_cfg |= SMART_LCD_FORMAT_666;
        break;
    case 24:
        smart_cfg |= SMART_LCD_FORMAT_888;
        break;
    default:
        printk("ERR: please check out your bpp config\n");
        break;
    }
    if (pdata->smart_config.cfg_6800_md)
        smart_cfg |= SLCD_DBI_TYPE_6800;
    else
        smart_cfg |= SLCD_DBI_TYPE_8080;

    smart_frm_size |= mode->yres << 16;
    smart_frm_size |= mode->xres;

    jzfb_prepare_dma_desc(info);

#ifdef LCDC_TINY_NV12
    cfg |= LCD_GLB_FORMAT_NV12;
    cfg |= DMA_SEL_NV12;
#else /* LCDC_TINY_NV12 */
    cfg |= LCD_GLB_COLOR_RGB;
    switch (jzfb->pdata->bpp) {
    case 15:
        cfg |= LCD_GLB_FORMAT_555;
        break;
    case 16:
        cfg |= LCD_GLB_FORMAT_565;
        break;
    case 24:
        cfg |= LCD_GLB_FORMAT_888;
        break;
    default:
        printk("ERR: please check out your bpp config\n");
        break;
    }
    /*cfg |= CLKGATE_CLS;*/
    /* cfg |= BURST_LEN; */
    cfg |= DMA_SEL_RGB;
#endif /* LCDC_TINY_NV12 */

    mutex_lock(&jzfb->lock);
    reg_write(jzfb, LCDC_SLCD_PANEL_CFG, smart_cfg);
    reg_write(jzfb, LCDC_SLCD_FRM_SIZE, smart_frm_size);
    reg_write(jzfb, LCDC_SLCD_WR_DUTY, smart_wtime);
    reg_write(jzfb, LCDC_SLCD_TIMING, smart_timing);
    reg_write(jzfb, LCDC_SLCD_SLOW_TIME, smart_slowtime);

    reg_write(jzfb, LCDC_TINY_GLB_CFG, cfg);
    mutex_unlock(&jzfb->lock);
    jzfb_slcd_mcu_init(info);

#ifdef CONFIG_SLCDC_USE_TE
    smart_cfg &= ~SLCD_TE_ANTI_JIT;
    smart_cfg |= SLCD_TE_MD;
    smart_cfg |= SLCD_SWITCH_WAIT_TE;
    reg_write(jzfb, LCDC_SLCD_PANEL_CFG, smart_cfg);

#endif

    return 0;
}

static int jzfb_set_par(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;
    struct jzfb_platform_data *pdata = jzfb->pdata;
    struct fb_var_screeninfo *var = &info->var;
    struct fb_videomode *mode;

    mode = jzfb_get_mode(var, info);
    if (mode == NULL) {
        dev_err(info->dev, "%s get video mode failed\n", __func__);
        return -EINVAL;
    }

    info->mode = mode;

    reg_write(jzfb, LCDC_TINY_CSR, 0xe);	//mask of LCDC_TINY_CSR

    /* reg_write(jzfb, LCDC_TINY_INTC, FRM_END | DMA_END | GEN_STOPM | QCK_STOPM); */
    reg_write(jzfb, LCDC_TINY_INTC, DMA_END);
    reg_write(jzfb, LCDC_TINY_INTC, FRM_END);
    reg_write(jzfb, LCDC_TINY_INTC, QCK_STOPM);
    /* reg_write(jzfb, LCDC_TINY_INTC, GEN_STOPM | QCK_STOPM); */
    if (pdata->lcd_type == LCD_TYPE_SLCD) {
        reg_write(jzfb, LCDC_DISP_COMMON, DISP_COM_SLCD);
        jzfb_slcd_set_par(info);
    } else {
        reg_write(jzfb, LCDC_DISP_COMMON, DISP_COM_TFT);
        jzfb_tft_set_par(info);
    }

    return 0;
}

static int jzfb_blank(int blank_mode, struct fb_info *info)
{
    return 0;
}

static int jzfb_alloc_devmem(struct jzfb *jzfb)
{
    int i;
    unsigned int videosize = 0;
    struct fb_videomode *mode;
    void *page;
    unsigned int phys_addr;
#if 1
    for(i = 0; i < MAX_DESC_NUM; i++) {
        jzfb->framedesc[i] = dma_alloc_coherent(jzfb->dev,
                                                sizeof(struct jzfb_framedesc), &jzfb->framedesc_phys[i], GFP_KERNEL);

        if (!jzfb->framedesc[i])
            return -ENOMEM;
    }
#endif

    /* jzfb->framedesc[0]=kmalloc(sizeof(struct jzfb_framedesc),GFP_KERNEL); */
    /* jzfb->framedesc_phys[0]=virt_to_phys(jzfb->framedesc[0]); */
    /* if (!jzfb->framedesc[0]) */
    /* return -ENOMEM; */



    mode = jzfb->pdata->modes;
    if (!mode) {
        dev_err(jzfb->dev, "Checkout video mode fail\n");
        return -EINVAL;
    }

    videosize = ALIGN(mode->xres, PIXEL_ALIGN) * mode->yres;
    videosize *= jzfb_get_controller_bpp(jzfb) >> 3;
    videosize *= NUM_FRAME_BUFFERS;
    jzfb->vidmem_size = PAGE_ALIGN(videosize);

    /**
     * Use the dma alloc coherent has waste some space,
     * If you need to alloc buffer for dma, open it,
     * else close it and use the Kmalloc.
     * And in jzfb_free_devmem() function is also set.
     */
#if 1
    jzfb->vidmem[0] = dma_alloc_coherent(jzfb->dev,
                                         jzfb->vidmem_size,
                                         &jzfb->vidmem_rgb_y_phys[0], GFP_KERNEL);

    dma_cache_wback_inv((unsigned int )jzfb->vidmem[0] &(~0x20000000),jzfb->vidmem_size);

    if (!jzfb->vidmem[0])
        return -ENOMEM;
    for (page = jzfb->vidmem[0];
         page < jzfb->vidmem[0] + PAGE_ALIGN(jzfb->vidmem_size);
         page += PAGE_SIZE) {
        SetPageReserved(virt_to_page(page));
    }
    for(i = 1; i < NUM_FRAME_BUFFERS; i++) {
        jzfb->vidmem_rgb_y_phys[i] = jzfb->vidmem_rgb_y_phys[i-1] + videosize/NUM_FRAME_BUFFERS;
        jzfb->vidmem[i] = jzfb->vidmem[i-1] + videosize/NUM_FRAME_BUFFERS;
    }
    for(i = 0; i < NUM_FRAME_BUFFERS; i++) {
        jzfb->vidmem_uv_phys[i] = (unsigned int)
            (jzfb->vidmem_rgb_y_phys[i] + mode->xres * mode->yres);
    }
#endif

    /* jzfb->vidmem[0]=kmalloc(jzfb->vidmem_size,GFP_KERNEL); */
    /* jzfb->vidmem_rgb_y_phys[0]=virt_to_phys(jzfb->vidmem[0]); */
    /* jzfb->vidmem_uv_phys[0]=(unsigned int)jzfb->vidmem_rgb_y_phys[0]+mode->xres*mode->yres; */

    return 0;
}

static void jzfb_free_devmem(struct jzfb *jzfb)
{
    int i;
    for(i = 0; i < MAX_DESC_NUM; i ++)
        dma_free_coherent(jzfb->dev,
                          sizeof(struct jzfb_framedesc),
                          jzfb->framedesc[i], jzfb->framedesc_phys[i]);
    dma_free_coherent(jzfb->dev, jzfb->vidmem_size * NUM_FRAME_BUFFERS,
                      jzfb->vidmem[i], jzfb->vidmem_rgb_y_phys[i]);
}

#define SPEC_TIME_IN_NS (1000*1000000)  /* 1s */

static inline int timeval_sub_to_us(struct timeval lhs,
                                    struct timeval rhs)
{
    int sec, usec;
    sec = lhs.tv_sec - rhs.tv_sec;
    usec = lhs.tv_usec - rhs.tv_usec;

    return (sec*1000000 + usec);
}

static inline int time_us2ms(int us)
{
    return (us/1000);
}

static int jzfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
    int next_frm;
    struct jzfb *jzfb = info->par;
#if 0
    {/*debug*/
        static struct timeval time_now, time_last;
        unsigned int interval_in_us;
        unsigned int interval_in_ms;
        static unsigned int fpsCount = 0;

        jzfb->pan_display_count++;
        if(showFPS){
            switch(showFPS){
            case 1:
                fpsCount++;
                do_gettimeofday(&time_now);
                interval_in_us = timeval_sub_to_us(time_now, time_last);
                if ( interval_in_us > (USEC_PER_SEC) ) { /* 1 second = 1000000 us. */
                    printk(KERN_DEBUG " Pan display FPS: %d\n",fpsCount);
                    fpsCount = 0;
                    time_last = time_now;
                }
                break;
            case 2:
                do_gettimeofday(&time_now);
                interval_in_us = timeval_sub_to_us(time_now, time_last);
                interval_in_ms = time_us2ms(interval_in_us);
                printk(KERN_DEBUG " Pan display interval ms: %d\n",interval_in_ms);
                time_last = time_now;
                break;
            default:
                if (showFPS > 3) {
                    int d, f;
                    fpsCount++;
                    do_gettimeofday(&time_now);
                    interval_in_us = timeval_sub_to_us(time_now, time_last);
                    if (interval_in_us > USEC_PER_SEC * showFPS ) { /* 1 second = 1000000 us. */
                        d = fpsCount / showFPS;
                        f = (fpsCount * 10) / showFPS - d * 10;
                        printk(KERN_DEBUG " Pan display FPS: %d.%01d\n", d, f);
                        fpsCount = 0;
                        time_last = time_now;
                    }
                }
                break;
            }
        }
    }/*end debug*/
#endif /* debug */
    if (var->xoffset - info->var.xoffset) {
        dev_err(info->dev, "No support for X panning for now\n");
        return -EINVAL;
    }

    if (var->yres == 720 || var->yres == 1080) {	/* work around for HDMI device */
        switch (var->yoffset) {
        case 1440:
        case (1080 * 2):
            next_frm = 2;
            break;
        case 720:
        case (1080 * 1):
            next_frm = 1;
            break;
        default:
            next_frm = 0;
            break;
        }
    } else
        next_frm = var->yoffset / var->yres;

#if (NUM_FRAME_BUFFERS == 1)
    next_frm = 0;
#endif
    jzfb->current_buffer = next_frm;

    if (jzfb->pdata->lcd_type != LCD_TYPE_INTERLACED_TV &&
        jzfb->pdata->lcd_type != LCD_TYPE_SLCD) {
        jzfb->framedesc[0]->BufferAddr_RGB_Y = jzfb->vidmem_rgb_y_phys[0]
            + jzfb->frm_size * next_frm;
    } else if (jzfb->pdata->lcd_type == LCD_TYPE_SLCD) {

        /* smart tft spec code here */
        jzfb->framedesc[0]->BufferAddr_RGB_Y = jzfb->vidmem_rgb_y_phys[0]
            + jzfb->frm_size * next_frm;
        if (!jzfb->is_lcd_en)
            return -EINVAL;;
        slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
        wait_busy();

        jzfb_enable(jzfb->fb);
    } else {
        /* LCD_TYPE_INTERLACED_TV */
    }

    return 0;
}

static int jzfb_set_frmdesc(struct jzfb *jzfb, struct jzfb_frm_desc *frm_cfg)
{
    unsigned int cfg;
    unsigned int w,h;
    w = jzfb->fb->mode->xres;
    h = jzfb->fb->mode->yres;
    cfg = 0;

    jzfb->framedesc[0]->Stride_RGB_Y = frm_cfg->stride_rgb_y;
    jzfb->framedesc[0]->Stride_UV = frm_cfg->stride_uv;
    jzfb->framedesc[0]->ChainCfg = frm_cfg->chain_cfg;
    jzfb->framedesc[0]->InterruptCtrl = frm_cfg->int_frm_end << 1;

    if(frm_cfg->select_nv12) {
        cfg |= LCD_GLB_FORMAT_NV12;
        cfg |= DMA_SEL_NV12;
        /*cfg |= CLKGATE_CLS;*/
    } else {
        cfg |= LCD_GLB_COLOR_RGB;
        switch (jzfb->pdata->bpp) {
        case 15:
            cfg |= LCD_GLB_FORMAT_555;
            break;
        case 16:
            cfg |= LCD_GLB_FORMAT_565;
            break;
        case 24:
            cfg |= LCD_GLB_FORMAT_888;
            break;
        default:
            printk("ERR: please check out your bpp config\n");
            break;
        }
        /*cfg |= CLKGATE_CLS;*/
        /*cfg |= BURST_LEN;*/
        cfg |= DMA_SEL_RGB;
    }
    reg_write(jzfb, LCDC_TINY_GLB_CFG, cfg);

    return 0;
}

static int jzfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    struct jzfb *jzfb = info->par;
    switch (cmd) {
    case JZFB_SET_FRAME_CONFIG:
        {
            struct jzfb_frm_desc frm_cfg;
            copy_from_user(&frm_cfg, argp, sizeof(struct jzfb_frm_desc));
            jzfb_set_frmdesc(jzfb, &frm_cfg);
            break;
        }
    case JZFB_GET_FRAME_CONFIG:
        break;
    case JZFB_ENABLE:
        jzfb_enable(info);
        break;
    case JZFB_GET_MODE:
        copy_to_user(argp, jzfb->fb->mode, sizeof(struct fb_videomode));
        break;
    case JZFB_DISABLE:
        jzfb_disable(info);
        break;

    default:
        printk("Command:%x Error!\n",cmd);
        break;
    }
    return 0;
}

static int jzfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    struct jzfb *jzfb = info->par;
    unsigned long start;
    unsigned long off;
    u32 len;

    off = vma->vm_pgoff << PAGE_SHIFT;

    /* frame buffer memory */
    start = jzfb->fb->fix.smem_start;
    len = PAGE_ALIGN((start & ~PAGE_MASK) + jzfb->fb->fix.smem_len);
    start &= PAGE_MASK;

    if ((vma->vm_end - vma->vm_start + off) > len)
        return -EINVAL;
    off += start;

    vma->vm_pgoff = off >> PAGE_SHIFT;
    vma->vm_flags |= VM_IO;

    pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
    /* Write-Acceleration */
    pgprot_val(vma->vm_page_prot) |= _CACHE_CACHABLE_WA;

    if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
                           vma->vm_end - vma->vm_start, vma->vm_page_prot))
    {
        return -EAGAIN;
    }

    return 0;
}

static irqreturn_t jzfb_irq_handler(int irq, void *data)
{
    struct jzfb *jzfb = (struct jzfb *)data;

    /* unsigned int status = reg_read(jzfb, LCDC_TINY_ST); */
    unsigned int status = reg_read(jzfb, LCDC_TINY_INT_FLAG);
    unsigned int mask = reg_read(jzfb, LCDC_TINY_INTC);

    unsigned int flag = status & mask;

    if(flag & FRM_END) {
        /* #if !defined(CONFIG_SLCDC_CONTINUA) */
        /* 		printk(">>>>>>>>FRM_END\n"); */
        /* #endif */

        if(!(jzfb->is_lcd_en)) {
            reg_write(jzfb, LCDC_TINY_CTRL, QCK_STOP);
            reg_write(jzfb, LCDC_TINY_CSR, FRM_END);
        } else {
            reg_write(jzfb, LCDC_TINY_CSR, FRM_END);
            jzfb->is_frm_end = 1;
            /*slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
              wait_busy();
              jzfb_slcd_start(jzfb->fb);*/
#ifdef CONFIG_SLCDC_USE_TE
            /*reg_write(jzfb, LCDC_SLCD_PANEL_CFG,
              reg_read(jzfb, LCDC_SLCD_PANEL_CFG) & (~SLCD_FMT_EN));

              slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
              slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
              slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
              slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);

              reg_write(jzfb, LCDC_SLCD_PANEL_CFG,
              reg_read(jzfb, LCDC_SLCD_PANEL_CFG) | SLCD_FMT_EN);
              reg_write(jzfb, LCDC_TINY_CTRL, SLCD_START);*/
#endif
        }
    }

    if(flag & DMA_END) {
        reg_write(jzfb, LCDC_TINY_CSR, DMA_END);
    }

    if(flag & TFT_UNDER) {
        reg_write(jzfb, LCDC_TINY_CSR, TFT_UNDER);
    }

    if(flag & GEN_STOPM) {
        printk(">>>>>>>>GEN_STOP\n");
        reg_write(jzfb, LCDC_TINY_CSR, GEN_STOPM);
    }

    if(flag & QCK_STOPM) {
        printk(">>>>>>>>QCK_STOP\n");
        reg_write(jzfb, LCDC_TINY_CSR, QCK_STOPM);
        up(&jzfb->dis_lock);
    }

    return IRQ_HANDLED;
}

static inline uint32_t convert_color_to_hw(unsigned val, struct fb_bitfield *bf)
{
    return (((val << bf->length) + 0x7FFF - val) >> 16) << bf->offset;
}
static int jzfb_setcolreg(unsigned regno, unsigned red, unsigned green,
                          unsigned blue, unsigned transp, struct fb_info *fb)
{
    if (regno >= 16)
        return -EINVAL;

    ((uint32_t *)(fb->pseudo_palette))[regno] =
        convert_color_to_hw(red, &fb->var.red) |
        convert_color_to_hw(green, &fb->var.green) |
        convert_color_to_hw(blue, &fb->var.blue) |
        convert_color_to_hw(transp, &fb->var.transp);

    return 0;
}
static struct fb_ops jzfb_ops = {
    .owner = THIS_MODULE,
    .fb_open = jzfb_open,
    .fb_release = jzfb_release,
    .fb_check_var = jzfb_check_var,
    .fb_set_par = jzfb_set_par,
    .fb_setcolreg = jzfb_setcolreg,
    .fb_blank = jzfb_blank,
    .fb_pan_display = jzfb_pan_display,
    .fb_fillrect = cfb_fillrect,
    .fb_copyarea = cfb_copyarea,
    .fb_imageblit = cfb_imageblit,
    .fb_ioctl = jzfb_ioctl,
    .fb_mmap = jzfb_mmap,
};
void jzfb_debug_te_color(struct fb_info *info)
{
    int i, j;
    int w, h;
    unsigned int *p32;
    unsigned int *_p32;
    unsigned int c32;
    unsigned int _c32;

    unsigned short *p16;
    unsigned short *_p16;
    unsigned short c16;
    unsigned short _c16;

    unsigned int bpp;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode = jzfb->pdata->modes;
    p32 = (unsigned int *)jzfb->vidmem[0];
    _p32 = (unsigned int *)jzfb->vidmem[1];
    p16 = (unsigned short *)jzfb->vidmem[0];
    _p16 = (unsigned short *)jzfb->vidmem[1];
    w = mode->xres;
    h = mode->yres;

    bpp = jzfb_get_controller_bpp(jzfb);

    for(i = 0; i < w; i++) {
        for(j = 0; j < h; j++) {
            c32 = 0xFFFFFFFF;
            _c32 = 0x0000FF00;
            c16 = 0xFFFF;
            _c16 = 0x07E0;
            switch (bpp) {
            case 32:
                *p32++ = c32;
                *_p32++ = _c32;
                break;
            default:
                *p16++ = c16;
                *_p16++ = _c16;
                break;
            }
        }
    }
}
static void jzfb_display_nv12_single_color_red(struct fb_info *info)
{
    int i, j;
    int w, h;
    unsigned char *p_y;
    unsigned char *p_uv;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode = jzfb->pdata->modes;

    w = mode->xres;
    h = mode->yres;

    p_y = (unsigned char *)jzfb->vidmem[1];
    p_uv = p_y + w * h;
    printk("----vidmem[1]:%x\n",(unsigned int)jzfb->vidmem[1]);
    printk("----p_y:%x, p_uv:%x\n",(unsigned int)p_y,(unsigned int)p_uv);

    printf("w:%x, h:%x\n",w,h);
    for(j = 0; j < h; j++) {
        for(i = 0; i < w; i++) {
            unsigned char _y;
            _y = (unsigned char)(0xff*0.299);	//red
            /*_y = (unsigned char)(0xff*0.114);	//blue*/
            *p_y++ = _y;
        }
    }
    for(j = 0; j < h/2; j++) {
        for(i = 0; i < w/2; i++) {
            unsigned char _u, _v;
            _u = (unsigned char)(128 - 0.1687*0xff);	//red
            _v = (unsigned char)(128 + 0.5*0xff);		//red
            /*_u = (unsigned char)(128 + 0.5*0xff);		//blue*/
            /*_v = (unsigned char)(128 - 0.0813*0xff);	//blue*/
            *p_uv++ = _u;
            *p_uv++ = _v;
        }
    }
}

static void jzfb_display_nv12_single_color_blue(struct fb_info *info)
{
    int i, j;
    int w, h;
    unsigned char *p_y;
    unsigned char *p_uv;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode = jzfb->pdata->modes;

    w = mode->xres;
    h = mode->yres;

    p_y = (unsigned char *)jzfb->vidmem[0];
    p_uv = p_y + w * h;
    printk("----vidmem[1]:%x\n",(unsigned int)jzfb->vidmem[0]);
    printk("----p_y:%x, p_uv:%x\n",(unsigned int)p_y,(unsigned int)p_uv);

    printf("w:%x, h:%x\n",w,h);
    for(j = 0; j < h; j++) {
        for(i = 0; i < w; i++) {
            unsigned char _y;
            /* _y = (unsigned char)(0xff*0.299);	//red */
            _y = (unsigned char)(0xff*0.114);	//blue
            *p_y++ = _y;
        }
    }
    for(j = 0; j < h/2; j++) {
        for(i = 0; i < w/2; i++) {
            unsigned char _u, _v;
            /* _u = (unsigned char)(128 - 0.1687*0xff);	//red */
            /* _v = (unsigned char)(128 + 0.5*0xff);		//red */
            _u = (unsigned char)(128 + 0.5*0xff);		//blue
            _v = (unsigned char)(128 - 0.0813*0xff);	//blue
            *p_uv++ = _u;
            *p_uv++ = _v;
        }
    }
}
static void jzfb_display_nv12_single_color_green(struct fb_info *info)
{
    int i, j;
    int w, h;
    unsigned char *p_y;
    unsigned char *p_uv;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode = jzfb->pdata->modes;

    w = mode->xres;
    h = mode->yres;

    p_y = (unsigned char *)jzfb->vidmem[2];
    p_uv = p_y + w * h;
    printk("----vidmem[1]:%x\n",(unsigned int)jzfb->vidmem[2]);
    printk("----p_y:%x, p_uv:%x\n",(unsigned int)p_y,(unsigned int)p_uv);

    printf("w:%x, h:%x\n",w,h);
    for(j = 0; j < h; j++) {
        for(i = 0; i < w; i++) {
            unsigned char _y;
            /* _y = (unsigned char)(0xff*0.299);	//red */
            _y = (unsigned char)(0xff*0.2);	//blue
            *p_y++ = _y;
        }
    }
    for(j = 0; j < h/2; j++) {
        for(i = 0; i < w/2; i++) {
            unsigned char _u, _v;
            /* _u = (unsigned char)(128 - 0.1687*0xff);	//red */
            /* _v = (unsigned char)(128 + 0.5*0xff);		//red */
            _u = (unsigned char)(128 + 0.2*0xff);		//blue
            _v = (unsigned char)(128 + 0.0813*0xff);	//blue
            *p_uv++ = _u;
            *p_uv++ = _v;
        }
    }
}

static void jzfb_display_pic(struct fb_info *info)
{
    int w,h;
    int bpp;
    unsigned char *p8;
    unsigned short *p16;
    unsigned int *p32;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode = jzfb->pdata->modes;

    if (!jzfb->vidmem_rgb_y_phys[0]) {
        dev_err(jzfb->dev, "Not allocate frame buffer yet\n");
        return;
    }
    if (!jzfb->vidmem[0])
        jzfb->vidmem[0] = (void *)phys_to_virt(jzfb->vidmem_rgb_y_phys[0]);
    p8 = (unsigned char *)jzfb->vidmem[0];
    w = mode->xres;
    h = mode->yres;

    /*bpp = 32;*/
    bpp = jzfb_get_controller_bpp(jzfb);
#ifdef LCDC_TINY_NV12
    memcpy(p8,buf_240_240_NV12,w*h+w*h/2);
#else
    memcpy(p8,buf_240_240_RGB565,w*h*2);
#endif
}

static void jzfb_display_color_bar(struct fb_info *info, unsigned int color_mode)
{
    int i, j;
    int *p;
    int w, h;
    int bpp;
    unsigned char *p8;
    unsigned short *p16;
    unsigned int *p32;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode = jzfb->pdata->modes;

    if (!mode) {
        dev_err(jzfb->dev, "%s, video mode is NULL\n", __func__);
        return;
    }
    if (!jzfb->vidmem_rgb_y_phys[0]) {
        dev_err(jzfb->dev, "Not allocate frame buffer yet\n");
        return;
    }
    if (!jzfb->vidmem[0])
        jzfb->vidmem[0] = (void *)phys_to_virt(jzfb->vidmem_rgb_y_phys[0]);
    /*p16 = (unsigned short *)jzfb->vidmem[color_mode];*/
    /*p32 = (unsigned int *)jzfb->vidmem[color_mode];*/
    p8 = (unsigned char *)jzfb->vidmem[0];
    p16 = (unsigned short *)jzfb->vidmem[0];
    p32 = (unsigned int *)jzfb->vidmem[0];
    w = mode->xres;
    h = mode->yres;

    /*bpp = 32;*/
    bpp = jzfb_get_controller_bpp(jzfb);

    if(color_mode)
        p = &i;	//h
    else
        p = &j;	//v

    dev_info(info->dev,
             "LCD H COLOR BAR w,h,bpp(%d,%d,%d), jzfb->vidmem[0]=%p\n", w, h,
             bpp, jzfb->vidmem[0]);

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            short c16;
            int c32;
            switch ((*p / 10) % 4) {
            case 0:
                c16 = 0xF800;
                c32 = 0x00FF0000;
                break;
            case 1:
                c16 = 0x07E0;
                c32 = 0x0000FF00;
                break;
            case 2:
                c16 = 0x001F;
                c32 = 0x000000FF;
                break;
            default:
                c16 = 0xFFFF;
                c32 = 0xFFFFFFFF;
                break;
            }
            switch (bpp) {
            case 18:
            case 24:
            case 32:
                *p32++ = c32;
                break;
            default:
                *p16++ = c16;
            }
        }
        if (w % PIXEL_ALIGN) {
            switch (bpp) {
            case 18:
            case 24:
            case 32:
                p32 += (ALIGN(mode->xres, PIXEL_ALIGN) - w);
                break;
            default:
                p16 += (ALIGN(mode->xres, PIXEL_ALIGN) - w);
                break;
            }
        }
    }
}

static void dump_lcdc_registers(struct jzfb *jzfb)
{
    printk("-----------------dc_reg------------------\n");
    printk("LCDC_TINY_CHAIN_ADDR:   %lx\n",reg_read(jzfb, LCDC_TINY_CHAIN_ADDR	 ));
    printk("LCDC_TINY_GLB_CFG:      %lx\n",reg_read(jzfb, LCDC_TINY_GLB_CFG		 ));
    printk("LCDC_TINY_CSC_MULT_YRV: %lx\n",reg_read(jzfb, LCDC_TINY_CSC_MULT_YRV ));
    printk("LCDC_TINY_CSC_MULT_GUGV:%lx\n",reg_read(jzfb, LCDC_TINY_CSC_MULT_GUGV));
    printk("LCDC_TINY_CSC_MULT_BU:  %lx\n",reg_read(jzfb, LCDC_TINY_CSC_MULT_BU	 ));
    printk("LCDC_TINY_CSC_SUB_YUV:  %lx\n",reg_read(jzfb, LCDC_TINY_CSC_SUB_YUV	 ));
    printk("LCDC_TINY_CTRL:         %lx\n",reg_read(jzfb, LCDC_TINY_CTRL		 ));
    printk("LCDC_TINY_ST:           %lx\n",reg_read(jzfb, LCDC_TINY_ST			 ));
    printk("LCDC_TINY_CSR:          %lx\n",reg_read(jzfb, LCDC_TINY_CSR			 ));
    printk("LCDC_TINY_INTC:         %lx\n",reg_read(jzfb, LCDC_TINY_INTC		 ));
    printk("LCDC_TINY_INT_FLAG:     %lx\n",reg_read(jzfb, LCDC_TINY_INT_FLAG	 ));
    printk("LCDC_TINY_CHAIN_SITE:   %lx\n",reg_read(jzfb, LCDC_TINY_CHAIN_SITE	 ));
    printk("LCDC_TINY_DMA_SITE:     %lx\n",reg_read(jzfb, LCDC_TINY_DMA_SITE	 ));
    printk("LCDC_DISP_COMMON:       %lx\n",reg_read(jzfb, LCDC_DISP_COMMON		 ));
    printk("-----------------dc_reg------------------\n");

    printk("----------------tft_reg------------------\n");
    printk("LCDC_TFT_TIMING_HSYNC:       %lx\n",reg_read(jzfb, LCDC_TFT_TIMING_HSYNC));
    printk("LCDC_TFT_TIMING_VSYNC:       %lx\n",reg_read(jzfb, LCDC_TFT_TIMING_VSYNC));
    printk("LCDC_TFT_TIMING_HDE:         %lx\n",reg_read(jzfb, LCDC_TFT_TIMING_HDE));
    printk("LCDC_TFT_TIMING_VDE:         %lx\n",reg_read(jzfb, LCDC_TFT_TIMING_VDE));
    printk("LCDC_TFT_TRAN_CFG:           %lx\n",reg_read(jzfb, LCDC_TFT_TRAN_CFG));
    printk("LCDC_TFT_ST:                 %lx\n",reg_read(jzfb, LCDC_TFT_ST));
    printk("----------------tft_reg------------------\n");

    printk("---------------slcd_reg------------------\n");
    printk("LCDC_SLCD_PANEL_CFG     %lx\n",reg_read(jzfb, LCDC_SLCD_PANEL_CFG));
    printk("LCDC_SLCD_WR_DUTY       %lx\n",reg_read(jzfb, LCDC_SLCD_WR_DUTY  ));
    printk("LCDC_SLCD_TIMING        %lx\n",reg_read(jzfb, LCDC_SLCD_TIMING   ));
    printk("LCDC_SLCD_FRM_SIZE      %lx\n",reg_read(jzfb, LCDC_SLCD_FRM_SIZE ));
    printk("LCDC_SLCD_SLOW_TIME     %lx\n",reg_read(jzfb, LCDC_SLCD_SLOW_TIME));
    printk("LCDC_SLCD_CMD           %lx\n",reg_read(jzfb, LCDC_SLCD_CMD      ));
    printk("LCDC_SLCD_ST            %lx\n",reg_read(jzfb, LCDC_SLCD_ST       ));
    printk("---------------slcd_reg------------------\n");

    reg_write(jzfb, LCDC_TINY_CTRL, (1 << 2));
    printk("====================Frame Descriptor register======================\n");
    printk("NextDesAddr             %lx\n",reg_read(jzfb, LCDC_TINY_DES_READ));
    printk("BufferAddr_RGB_Y        %lx\n",reg_read(jzfb, LCDC_TINY_DES_READ));
    printk("Stride_RGB_Y            %lx\n",reg_read(jzfb, LCDC_TINY_DES_READ));
    printk("ChainCfg                %lx\n",reg_read(jzfb, LCDC_TINY_DES_READ));
    printk("InterruptCtrl           %lx\n",reg_read(jzfb, LCDC_TINY_DES_READ));
    printk("BufferAddr_UV           %lx\n",reg_read(jzfb, LCDC_TINY_DES_READ));
    printk("Stride_UV               %lx\n",reg_read(jzfb, LCDC_TINY_DES_READ));
    printk("=================Frame Descriptor register end======================\n");
    return;
}

    static ssize_t
dump_lcd(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    dump_lcdc_registers(jzfb);

    return 0;
}

    static ssize_t
dump_h_color_bar(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    jzfb_display_color_bar(jzfb->fb, 1);
    /* jzfb_enable(jzfb->fb); */
    return 0;
}

    static ssize_t
dump_v_color_bar(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    jzfb_display_color_bar(jzfb->fb, 0);
    /* jzfb_enable(jzfb->fb); */
    return 0;
}

    static ssize_t
vsync_skip_r(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    mutex_lock(&jzfb->lock);
    snprintf(buf, 3, "%d\n", jzfb->vsync_skip_ratio);
    printk("vsync_skip_map = 0x%08x\n", jzfb->vsync_skip_map);
    mutex_unlock(&jzfb->lock);
    return 3;		/* sizeof ("%d\n") */
}

static int vsync_skip_set(struct jzfb *jzfb, int vsync_skip)
{
    unsigned int map_wide10 = 0;
    int rate, i, p, n;
    int fake_float_1k;

    if (vsync_skip < 0 || vsync_skip > 9)
        return -EINVAL;

    rate = vsync_skip + 1;
    fake_float_1k = 10000 / rate;	/* 10.0 / rate */

    p = 1;
    n = (fake_float_1k * p + 500) / 1000;	/* +0.5 to int */

    for (i = 1; i <= 10; i++) {
        map_wide10 = map_wide10 << 1;
        if (i == n) {
            map_wide10++;
            p++;
            n = (fake_float_1k * p + 500) / 1000;
        }
    }
    mutex_lock(&jzfb->lock);
    jzfb->vsync_skip_map = map_wide10;
    jzfb->vsync_skip_ratio = rate - 1;	/* 0 ~ 9 */
    mutex_unlock(&jzfb->lock);

    printk("vsync_skip_ratio = %d\n", jzfb->vsync_skip_ratio);
    printk("vsync_skip_map = 0x%08x\n", jzfb->vsync_skip_map);

    return 0;
}

    static ssize_t
vsync_skip_w(struct device *dev, struct device_attribute *attr,
             const char *buf, size_t count)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);

    if ((count != 1) && (count != 2))
        return -EINVAL;
    if ((*buf < '0') && (*buf > '9'))
        return -EINVAL;

    vsync_skip_set(jzfb, *buf - '0');

    return count;
}

static ssize_t fps_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    printk("\n-----you can choice print way:\n");
    printk("Example: echo NUM > show_fps\n");
    printk("NUM = 0: close fps statistics\n");
    printk("NUM = 1: print recently fps\n");
    printk("NUM = 2: print interval between last and this pan_display\n");
    printk("NUM = 3: print pan_display count\n\n");
    return 0;
}

static ssize_t fps_store(struct device *dev,
                         struct device_attribute *attr, const char *buf, size_t n)
{
    int num = 0;
    num = simple_strtoul(buf, NULL, 0);
    if(num < 0 || num > 3){
        printk("\n--please 'cat show_fps' to view using the method\n\n");
        return n;
    }
    showFPS = num;
    if(showFPS == 3)
        printk(KERN_DEBUG " Pand display count=%d\n",jzfb->pan_display_count);
    return n;
}

static ssize_t debug_enable(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    jzfb_enable(jzfb->fb);
    return 0;
}

static ssize_t debug_disable(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    jzfb_disable(jzfb->fb);
    return 0;
}

static ssize_t test_jzfb_chain_start(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    jzfb_chain_start(jzfb->fb);
    return 0;
}

static ssize_t test_jzfb_tft_start(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    jzfb_tft_start(jzfb->fb);
    return 0;
}

static ssize_t test_jzfb_slcd_start(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
    wait_busy();
    jzfb_slcd_start(jzfb->fb);
    return 0;
}

static ssize_t test_jzfb_qck_stop(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    jzfb_qck_stop(jzfb->fb);
    return 0;
}

static ssize_t test_jzfb_gen_stop(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    struct jzfb *jzfb = dev_get_drvdata(dev);
    reg_write(jzfb, LCDC_TINY_CSR, 0x67);
    dump_lcdc_registers(jzfb);
    jzfb_gen_stop(jzfb->fb);
    return 0;
}

static void wait_busy()
{
    int count = 10000;
    while ((reg_read(jzfb, LCDC_SLCD_ST) & SLCD_ST_BUSY)
           && count--) {
        udelay(10);
    }
    if (count < 0) {
        dev_err(jzfb->dev,
                "SLCDC wait busy state wrong");
    }
}

/**********************lcd_debug***************************/
static DEVICE_ATTR(dump_lcd, S_IRUGO|S_IWUSR, dump_lcd, NULL);
static DEVICE_ATTR(dump_h_color_bar, S_IRUGO|S_IWUSR, dump_h_color_bar, NULL);
static DEVICE_ATTR(dump_v_color_bar, S_IRUGO|S_IWUSR, dump_v_color_bar, NULL);
static DEVICE_ATTR(vsync_skip, S_IRUGO|S_IWUSR, vsync_skip_r, vsync_skip_w);
static DEVICE_ATTR(show_fps, S_IRUGO|S_IWUSR, fps_show, fps_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR, debug_enable, NULL);
static DEVICE_ATTR(disable, S_IRUGO|S_IWUSR, debug_disable, NULL);
static DEVICE_ATTR(test_jzfb_chain_start, S_IRUGO|S_IWUSR, test_jzfb_chain_start, NULL);
static DEVICE_ATTR(test_jzfb_tft_start, S_IRUGO|S_IWUSR, test_jzfb_tft_start, NULL);
static DEVICE_ATTR(test_jzfb_slcd_start, S_IRUGO|S_IWUSR, test_jzfb_slcd_start, NULL);
static DEVICE_ATTR(test_jzfb_qck_stop, S_IRUGO|S_IWUSR, test_jzfb_qck_stop, NULL);
static DEVICE_ATTR(test_jzfb_gen_stop, S_IRUGO|S_IWUSR, test_jzfb_gen_stop, NULL);

static struct attribute *lcd_debug_attrs[] = {
    &dev_attr_dump_lcd.attr,
    &dev_attr_dump_h_color_bar.attr,
    &dev_attr_dump_v_color_bar.attr,
    &dev_attr_vsync_skip.attr,
    &dev_attr_show_fps.attr,
    &dev_attr_enable.attr,
    &dev_attr_disable.attr,
    &dev_attr_test_jzfb_chain_start.attr,
    &dev_attr_test_jzfb_tft_start.attr,
    &dev_attr_test_jzfb_slcd_start.attr,
    &dev_attr_test_jzfb_qck_stop.attr,
    &dev_attr_test_jzfb_gen_stop.attr,
    NULL,
};

const char lcd_group_name[] = "debug";
static struct attribute_group lcd_debug_attr_group = {
    .name	= lcd_group_name,
    .attrs	= lcd_debug_attrs,
};
void test_yuv_to_yuv(struct fb_info *info)
{
    int k = 1;
    uint32_t cfg = 0;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode = jzfb->pdata->modes;

    jzfb->framedesc[0]->NextDesAddr = jzfb->framedesc_phys[1];
    jzfb->framedesc[0]->Stride_RGB_Y = mode->xres;
    jzfb->framedesc[0]->Stride_UV = mode->xres;
    jzfb->framedesc[0]->BufferAddr_UV = jzfb->vidmem_uv_phys[0];
    jzfb->framedesc[0]->ChainCfg = 0;
    jzfb->framedesc[0]->InterruptCtrl = reg_read(jzfb, LCDC_TINY_INTC);
    jzfb->framedesc[1]->NextDesAddr = jzfb->framedesc_phys[2];
    jzfb->framedesc[1]->Stride_RGB_Y = mode->xres;
    jzfb->framedesc[1]->Stride_UV = mode->xres;
    jzfb->framedesc[1]->BufferAddr_UV = jzfb->vidmem_uv_phys[1];
    jzfb->framedesc[1]->ChainCfg = 0;
    jzfb->framedesc[1]->InterruptCtrl = reg_read(jzfb, LCDC_TINY_INTC);

    jzfb->framedesc[2]->NextDesAddr = jzfb->framedesc_phys[0];
    jzfb->framedesc[2]->Stride_RGB_Y = mode->xres;
    jzfb->framedesc[2]->Stride_UV = mode->xres;
    jzfb->framedesc[2]->BufferAddr_UV = jzfb->vidmem_uv_phys[2];
    jzfb->framedesc[2]->ChainCfg = 0;
    jzfb->framedesc[2]->InterruptCtrl = reg_read(jzfb, LCDC_TINY_INTC);

    jzfb_display_nv12_single_color_red(info);
    jzfb_display_nv12_single_color_blue(info);
    jzfb_display_nv12_single_color_green(info);

    while(k){
        cfg |= LCD_GLB_FORMAT_NV12;
        cfg |= DMA_SEL_NV12;
        reg_write(jzfb, LCDC_TINY_GLB_CFG, cfg);
        reg_write(jzfb, LCDC_TINY_CHAIN_ADDR, jzfb->framedesc_phys[0]);
        /* cfg |= LCD_GLB_COLOR_RGB; */
        /* cfg |= LCD_GLB_FORMAT_565; */
        /*cfg |= CLKGATE_CLS;*/
        /*cfg |= BURST_LEN;*/
        /* cfg |= DMA_SEL_RGB; */
        /* jzfb_disable(info); */
        jzfb_enable(info);
    }
}
void test_rgb_to_yuv(struct fb_info *info)
{
    int count = 1;
    uint32_t cfg = 0;
    struct jzfb *jzfb = info->par;
    struct fb_videomode *mode = jzfb->pdata->modes;

    jzfb->framedesc[0]->NextDesAddr = jzfb->framedesc_phys[0];
    jzfb->framedesc[1]->NextDesAddr = jzfb->framedesc_phys[1];
    jzfb->framedesc[1]->Stride_RGB_Y = mode->xres;
    jzfb->framedesc[1]->Stride_UV = mode->xres;
    jzfb->framedesc[1]->BufferAddr_UV = jzfb->vidmem_uv_phys[1];
    jzfb->framedesc[1]->ChainCfg = 0;
    jzfb->framedesc[1]->InterruptCtrl = reg_read(jzfb, LCDC_TINY_INTC);

    jzfb_display_nv12_single_color_red(info);
    jzfb_display_color_bar(info, 0);

    while(count --){
        /* mdelay(5000); */
        cfg = 0;
        cfg |= LCD_GLB_FORMAT_NV12;
        cfg |= DMA_SEL_NV12;
        reg_write(jzfb, LCDC_TINY_GLB_CFG, cfg);
        reg_write(jzfb, LCDC_TINY_CHAIN_ADDR, jzfb->framedesc_phys[1]);

        slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
        wait_busy();

        jzfb_enable(info);
        msleep(300);
        jzfb_disable(info);
        /* msleep(15); */

        printk(KERN_ERR "draw color.\n");
        cfg = 0;
        cfg |= LCD_GLB_COLOR_RGB;
        cfg |= LCD_GLB_FORMAT_565;
        cfg |= DMA_SEL_RGB;
        reg_write(jzfb, LCDC_TINY_GLB_CFG, cfg);
        reg_write(jzfb, LCDC_TINY_CHAIN_ADDR, jzfb->framedesc_phys[0]);

        slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
        wait_busy();

        jzfb_enable(info);
        msleep(100);
        jzfb_disable(info);
        /* msleep(15); */
    }
}

void test_slcd_partial_refresh(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;
    unsigned int smart_frm_size;

    jzfb->framedesc[0]->NextDesAddr = jzfb->framedesc_phys[0];
    jzfb->framedesc[1]->NextDesAddr = jzfb->framedesc_phys[1];

    reg_write(jzfb, LCDC_TINY_CHAIN_ADDR, jzfb->framedesc_phys[1]);

    jzfb_enable(info);

    mdelay(1000);

    jzfb_disable(info);

    reg_write(jzfb, LCDC_TINY_CHAIN_ADDR, jzfb->framedesc_phys[0]);
#if 1
    smart_frm_size = (100 << 16) | 100;
    reg_write(jzfb, LCDC_SLCD_FRM_SIZE, smart_frm_size);

    slcd_send_mcu_command_data(jzfb, 0x2a, FLAG_CMD);
    wait_busy();
    slcd_send_mcu_command_data(jzfb, 0, FLAG_PAR);
    wait_busy();
    slcd_send_mcu_command_data(jzfb, 0+60, FLAG_PAR);
    wait_busy();
    slcd_send_mcu_command_data(jzfb, 0 , FLAG_PAR);
    wait_busy();
    slcd_send_mcu_command_data(jzfb, 99+60 , FLAG_PAR);
    wait_busy();

    slcd_send_mcu_command_data(jzfb, 0x2b, FLAG_CMD);
    wait_busy();
    slcd_send_mcu_command_data(jzfb, 0, FLAG_PAR);
    wait_busy();
    slcd_send_mcu_command_data(jzfb, 0+60, FLAG_PAR);
    wait_busy();
    slcd_send_mcu_command_data(jzfb, 0 , FLAG_PAR);
    wait_busy();
    slcd_send_mcu_command_data(jzfb, 99+60 , FLAG_PAR);
    wait_busy();

    slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
    wait_busy();

    /*mdelay(1000);

      jzfb_display_color_bar(info, 0);	//v
      jzfb_enable(jzfb->fb);
      mdelay(1000);*/
#endif
    /*jzfb_display_color_bar(info, 0);	//v*/
    /*jzfb_display_color_bar(info, 1);	//h*/
    while(1) {
        unsigned int *_tmp;
        jzfb_display_color_bar(info, 1);	//h
        slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
        wait_busy();
        mdelay(1000);
        _tmp = (unsigned int *)((unsigned int)jzfb->vidmem[0] | 0xA0000000);
        _tmp += 119;
        printk("H Color bar, pixel:%x\n", *_tmp);
        jzfb_disable(info);
        jzfb_enable(info);
        mdelay(1000);
        jzfb_display_color_bar(info, 0);	//v
        slcd_send_mcu_command_data(jzfb, 0x2c, FLAG_CMD);
        wait_busy();
        mdelay(1000);
        printk("V Color bar, pixel:%x\n", *_tmp);
        jzfb_disable(info);
        jzfb_enable(info);
        mdelay(1000);
        /*printk("jzfb->vidmem_rgb_y_phys[0]:%x\n",jzfb->vidmem_rgb_y_phys[0]);*/
        /*dump_lcdc_registers(jzfb);*/
    }
}

static void test_pattern(struct jzfb *jzfb)
{
    struct jzfb_platform_data *pdata = jzfb->pdata;
    struct fb_videomode *mode;

    mode = pdata->modes;
    if (mode == NULL) {
        dev_err(jzfb->dev, "%s get video mode failed\n", __func__);
        return ;
    }

    reg_write(jzfb, LCDC_SLCD_PANEL_CFG,
              reg_read(jzfb, LCDC_SLCD_PANEL_CFG) | SLCD_FMT_EN);

#ifdef CONFIG_SLCDC_USE_TE
    /* jzfb_debug_te_color(jzfb->fb); */
#ifndef CONFIG_SLCDC_CONTINUA
    test_rgb_to_yuv(jzfb->fb);
    /* test_slcd_partial_refresh(jzfb->fb); */
    /* test_yuv_to_yuv(jzfb->fb); */
#endif
#else
#ifdef DISPLAY_PIC
    jzfb_display_pic(jzfb->fb);
#else
    jzfb_display_color_bar(jzfb->fb, 1);	//h */
    jzfb_enable(jzfb->fb);
    mdelay(1000);
    jzfb_display_color_bar(jzfb->fb, 0);	//v
    jzfb_enable(jzfb->fb);
    mdelay(1000);
    jzfb_display_color_bar(jzfb->fb, 1);	//h */
    jzfb_enable(jzfb->fb);
    mdelay(1000);
#endif
#endif

    jzfb_enable(jzfb->fb);
    dump_lcdc_registers(jzfb);
}

int lcd_display_inited_by_uboot( void )
{
    return uboot_inited;
}

static int refresh_pixclock_auto_adapt(struct fb_info *info)
{
    struct jzfb *jzfb = info->par;
    struct jzfb_platform_data *pdata = jzfb->pdata;
    struct fb_var_screeninfo *var = &info->var;
    struct fb_videomode *mode;
    uint16_t hds, vds;
    uint16_t hde, vde;
    uint16_t ht, vt;
    unsigned long rate;

    mode = pdata->modes;
    if (mode == NULL) {
        dev_err(jzfb->dev, "%s get video mode failed\n", __func__);
        return -EINVAL;
    }

    hds = mode->hsync_len + mode->left_margin;
    hde = hds + mode->xres;
    ht = hde + mode->right_margin;

    vds = mode->vsync_len + mode->upper_margin;
    vde = vds + mode->yres;
    vt = vde + mode->lower_margin;

    if(mode->refresh){
        if (pdata->lcd_type == LCD_TYPE_8BIT_SERIAL) {
            rate = mode->refresh * (vt + 2 * mode->xres) * ht;
        } else {
            rate = mode->refresh * vt * ht;
        }
        mode->pixclock = KHZ2PICOS(rate / 1000);

        var->pixclock = mode->pixclock;
    }else if(mode->pixclock){
        rate = PICOS2KHZ(mode->pixclock) * 1000;
        mode->refresh = rate / vt / ht;
    }else{
        dev_err(jzfb->dev,"+++++++++++%s error:lcd important config info is absenced\n",__func__);
        return -EINVAL;
    }
    return 0;

}

unsigned char * sbufflcd = NULL;
#define SBUFF_SIZE		128
static ssize_t slcd_proc_read(struct file *filp, char __user * buff, size_t len, loff_t * offset)
{
    printk(KERN_INFO "[+lcd_proc] call slcd_proc_read()\n");

    len = strlen(sbufflcd);
    if (*offset >= len) {
        return 0;
    }
    len -= *offset;
    if (copy_to_user(buff, sbufflcd + *offset, len)) {
        return -EFAULT;
    }
    *offset += len;
    printk("%s: sbufflcd = %s\n", __func__, sbufflcd);

    return len;
}

static ssize_t slcd_proc_write(struct file *filp, const char __user * buff, size_t len, loff_t * offset)
{
    int i = 0;
    int control[4] = {0};
    unsigned char *p = NULL;
    char *after = NULL;

    printk("%s: set lcd proc:\n", __func__);
    memset(sbufflcd, 0, SBUFF_SIZE);
    len = len < SBUFF_SIZE ? len : SBUFF_SIZE;
    if (copy_from_user(sbufflcd, buff, len)) {
        printk(KERN_INFO "[+lcd_proc]: copy_from_user() error!\n");
        return -EFAULT;
    }
    p = sbufflcd;
    control[0] = simple_strtoul(p, &after, 0);
    printk("control[0] = 0x%08x, after = %s\n", control[0], after);
    for (i = 1; i < 4; i++) {
        if (after[0] == ' ')
            after++;
        p = after;
        control[i] = simple_strtoul(p, &after, 0);
        printk("control[%d] = 0x%08x, after = %s\n", i, control[i], after);
    }

    if (control[0] == 1) {
        test_pattern(jzfb);
        return len;
    }

    return len;
}
static struct file_operations slcd_devices_fileops = {
    .owner		= THIS_MODULE,
    .read		= slcd_proc_read,
    .write		= slcd_proc_write,
};
static int jzfb_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct fb_info *fb;
    struct jzfb_platform_data *pdata = pdev->dev.platform_data;
    struct fb_videomode *video_mode;
    struct resource *mem;
    unsigned long rate;
    static struct proc_dir_entry *proc_lcd_dir;
    struct proc_dir_entry *entry;

    if (!pdata) {
        dev_err(&pdev->dev, "Missing platform data\n");
        return -ENXIO;
    }

    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!mem) {
        dev_err(&pdev->dev, "Failed to get register memory resource\n");
        return -ENXIO;
    }

    mem = request_mem_region(mem->start, resource_size(mem), pdev->name);
    if (!mem) {
        dev_err(&pdev->dev,
                "Failed to request register memory region\n");
        return -EBUSY;
    }

    fb = framebuffer_alloc(sizeof(struct jzfb), &pdev->dev);
    if (!fb) {
        dev_err(&pdev->dev, "Failed to allocate framebuffer device\n");
        ret = -ENOMEM;
        goto err_release_mem_region;
    }
    printk("FrameBuffer= %ld\n",sizeof(struct jzfb));

    fb->fbops = &jzfb_ops;
    fb->flags = FBINFO_DEFAULT;

    jzfb = fb->par;
    jzfb->fb = fb;
    jzfb->dev = &pdev->dev;
    jzfb->pdata = pdata;
    jzfb->mem = mem;

    if (pdata->lcd_type != LCD_TYPE_INTERLACED_TV &&
        pdata->lcd_type != LCD_TYPE_SLCD) {
        jzfb->desc_num = MAX_DESC_NUM - 2;
    } else {
        jzfb->desc_num = MAX_DESC_NUM;
    }

    mutex_init(&jzfb->lock);
    mutex_init(&jzfb->suspend_lock);

    /*  Must be turned on lcd/cgu_lpc/ahb1 3 clk  */
    sprintf(jzfb->clk_name, "lcd");
    sprintf(jzfb->pclk_name, "cgu_lpc");
    jzfb->clk = clk_get(&pdev->dev, jzfb->clk_name);
    jzfb->pclk = clk_get(&pdev->dev, jzfb->pclk_name);
    jzfb->ahb = clk_get(&pdev->dev, "ahb1");

    if (IS_ERR(jzfb->clk) || IS_ERR(jzfb->pclk)) {
        ret = PTR_ERR(jzfb->clk);
        dev_err(&pdev->dev, "Failed to get lcdc clock: %d\n", ret);
        /* goto err_framebuffer_release; */
    }

    /* Don't read or write lcdc registers until here. */

    jzfb->base = ioremap(mem->start, resource_size(mem));
    if (!jzfb->base) {
        dev_err(&pdev->dev,
                "Failed to ioremap register memory region\n");
        ret = -EBUSY;
        goto err_put_clk;
    }

    if(refresh_pixclock_auto_adapt(fb)){
        goto err_put_clk;
    }

    platform_set_drvdata(pdev, jzfb);

    fb_videomode_to_modelist(pdata->modes, pdata->num_modes, &fb->modelist);
    video_mode = jzfb->pdata->modes;
    /*if (!video_mode)
      goto err_iounmap;*/
    jzfb_videomode_to_var(&fb->var, video_mode, jzfb->pdata->lcd_type);
    fb->var.width = pdata->width;
    fb->var.height = pdata->height;
    fb->var.bits_per_pixel = pdata->bpp;

    /* pixclock at 30M ~ 5M HZ ,if pixclock is too large will have BUG*/
    rate = PICOS2KHZ(fb->var.pixclock) * 1000 * 5;
    ret = clk_set_rate(jzfb->pclk, rate);
    if(rate != clk_get_rate(jzfb->pclk)) {
        dev_err(&pdev->dev, "the rate to be setted :%ld, the rate getted in actually:%ld \n", rate, clk_get_rate(jzfb->pclk));
    }
    /* Enable lcd/cgu_lpc/ahb1 clk */
    clk_enable(jzfb->clk);
    clk_enable(jzfb->pclk);
    clk_enable(jzfb->ahb);


    /* Android generic FrameBuffer format is A8B8G8R8(B3B2B1B0), so we set A8B8G8R8 as default.
     *
     * If set rgb order as A8B8G8R8, both SLCD cmd_buffer and data_buffer bytes sequence changed.
     * so remain slcd format X8R8G8B8, until fix this problem.(<lgwang@ingenic.cn>, 2014-06-20)
     */
#ifdef CONFIG_ANDROID
    if (pdata->lcd_type == LCD_TYPE_SLCD) {
        jzfb->fmt_order = FORMAT_X8R8G8B8;
    }
    else {
        jzfb->fmt_order = FORMAT_X8B8G8R8;
    }
#else
    jzfb->fmt_order = FORMAT_X8R8G8B8;
#endif

    jzfb_check_var(&fb->var, fb);

    ret = jzfb_alloc_devmem(jzfb);
    if (ret) {
        dev_err(&pdev->dev, "Failed to allocate video memory\n");
        goto err_iounmap;
    }
    fb->fix = jzfb_fix;
    fb->fix.line_length = fb->var.bits_per_pixel * ALIGN(fb->var.xres,
                                                         PIXEL_ALIGN) >> 3;
    fb->fix.mmio_start = mem->start;
    fb->fix.mmio_len = resource_size(mem);
    fb->fix.smem_start = jzfb->vidmem_rgb_y_phys[0];
    fb->fix.smem_len = jzfb->vidmem_size;
    fb->screen_base = jzfb->vidmem[0];
    fb->pseudo_palette = jzfb->pseudo_palette;
    jzfb->irq = platform_get_irq(pdev, 0);
    sprintf(jzfb->irq_name, "lcdc%d", pdev->id);
    if (request_irq(jzfb->irq, jzfb_irq_handler, IRQF_DISABLED,
                    jzfb->irq_name, jzfb)) {
        dev_err(&pdev->dev, "request irq failed\n");
        ret = -EINVAL;
        goto err_free_devmem;
    }

    ret = sysfs_create_group(&jzfb->dev->kobj, &lcd_debug_attr_group);
    if (ret) {
        dev_err(&pdev->dev, "device create sysfs group failed\n");

        ret = -EINVAL;
        goto err_free_irq;
    }

    vsync_skip_set(jzfb, CONFIG_FB_VSYNC_SKIP);

    sema_init(&jzfb->dis_lock, 0);
    jzfb->timestamp.rp = 0;
    jzfb->timestamp.wp = 0;

    ret = register_framebuffer(fb);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register framebuffer: %d\n",
                ret);
        /*goto err_kthread_stop;*/
        goto err_free_file;
    }

    if (0 && lcd_display_inited_by_uboot()) {
        printk("#######lcd is enabled by uboot, keep par!!\n");
        /* remain uboot logo, set blank state, keep clk
         * but what if uboot's par is different with kernel's.
         * */
        jzfb->blank = FB_BLANK_UNBLANK;
    } else {
        jzfb_set_par(jzfb->fb);
    }

    sbufflcd = kmalloc(SBUFF_SIZE, GFP_KERNEL);
    proc_lcd_dir = proc_mkdir("slcd", NULL);
    if (!proc_lcd_dir)
        return -ENOMEM;

    entry = proc_create("slcd", 0, proc_lcd_dir, &slcd_devices_fileops);
    if (!entry) {
        printk("%s: create proc_create error!\n", __func__);
        return -1;
    }

    test_pattern(jzfb);
    jzfb_enable(jzfb->fb);
    return 0;

err_free_file:
    sysfs_remove_group(&jzfb->dev->kobj, &lcd_debug_attr_group);
err_free_irq:
    free_irq(jzfb->irq, jzfb);
err_free_devmem:
    jzfb_free_devmem(jzfb);
err_iounmap:
    iounmap(jzfb->base);
err_put_clk:
    if (jzfb->clk)
        clk_put(jzfb->clk);
    if (jzfb->pclk)
        clk_put(jzfb->pclk);
    if (jzfb->pwcl)
        clk_put(jzfb->pwcl);
err_framebuffer_release:
    framebuffer_release(fb);
err_release_mem_region:
    release_mem_region(mem->start, resource_size(mem));
    return ret;
}

static int jzfb_remove(struct platform_device *pdev)
{
    struct jzfb *jzfb = platform_get_drvdata(pdev);

    jzfb_free_devmem(jzfb);
    platform_set_drvdata(pdev, NULL);
    clk_put(jzfb->pclk);
    clk_put(jzfb->clk);
    /* clk_put(jzfb->pwcl); */

    sysfs_remove_group(&jzfb->dev->kobj, &lcd_debug_attr_group);

    iounmap(jzfb->base);
    release_mem_region(jzfb->mem->start, resource_size(jzfb->mem));

    framebuffer_release(jzfb->fb);

    return 0;
}

static void jzfb_shutdown(struct platform_device *pdev)
{
    struct jzfb *jzfb = platform_get_drvdata(pdev);
    int is_fb_blank;
    mutex_lock(&jzfb->suspend_lock);
    is_fb_blank = (jzfb->is_suspend != 1);
    jzfb->is_suspend = 1;
    mutex_unlock(&jzfb->suspend_lock);
    if (is_fb_blank)
        fb_blank(jzfb->fb, FB_BLANK_POWERDOWN);
}

#ifdef CONFIG_PM
void dump_cpm_reg(void)
{
    printk("----reg:0x10000020 value=0x%08x  (24bit) Clock Gate Register0\n",
           *(volatile unsigned int *)0xb0000020);
    printk("----reg:0x100000e4 value=0x%08x  (5bit_lcdc 21bit_lcdcs) Power Gate Register: \n",
           *(volatile unsigned int *)0xb00000e4);
    printk("----reg:0x100000b8 value=0x%08x  (10bit) SRAM Power Control Register0 \n",
           *(volatile unsigned int *)0xb00000b8);
    printk("----reg:0x10000064 value=0x%08x  Lcd pixclock \n",
           *(volatile unsigned int *)0xb0000064);
}

static int jzfb_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct jzfb *jzfb = platform_get_drvdata(pdev);

    mutex_lock(&jzfb->suspend_lock);
    jzfb->is_suspend = 1;
    mutex_unlock(&jzfb->suspend_lock);
#if 1
    printk("----[ lcd suspend ]:\n");
    /* dump_lcdc_registers(jzfb); */
    /* dump_cpm_reg(); */
#endif

    return 0;
}

static int jzfb_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct jzfb *jzfb = platform_get_drvdata(pdev);

    printk("####%s, %d ,jzfb->blank:%x, is_lcd_en:%x\n", __func__, __LINE__, jzfb->blank, jzfb->is_lcd_en);

    mutex_lock(&jzfb->suspend_lock);
    jzfb->is_suspend = 0;
    mutex_unlock(&jzfb->suspend_lock);

#if 1
    printk("----[ lcd resume ]:\n");
    /* dump_lcdc_registers(jzfb); */
    /* dump_cpm_reg(); */
#endif

    return 0;
}

static const struct dev_pm_ops jzfb_pm_ops = {
    .suspend = jzfb_suspend,
    .resume = jzfb_resume,
};
#endif
static struct platform_driver jzfb_driver = {
    .probe = jzfb_probe,
    .remove = jzfb_remove,
    .shutdown = jzfb_shutdown,
    .driver = {
        .name = "jz-fb",
#ifdef CONFIG_PM
        .pm = &jzfb_pm_ops,
#endif

    },
};

static int __init jzfb_init(void)
{
    platform_driver_register(&jzfb_driver);
    return 0;
}

static void __exit jzfb_cleanup(void)
{
    platform_driver_unregister(&jzfb_driver);
}

#ifdef CONFIG_EARLY_INIT_RUN
rootfs_initcall(jzfb_init);
#else
module_init(jzfb_init);
#endif

module_exit(jzfb_cleanup);

MODULE_DESCRIPTION("JZ LCD Controller driver");
MODULE_AUTHOR("Sean Tang <ctang@ingenic.cn>");
MODULE_LICENSE("GPL");
