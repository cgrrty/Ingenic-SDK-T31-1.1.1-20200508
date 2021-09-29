
/*
 * DDR driver for inno DDR PHY.
 * Used by t30
 *
 * Copyright (C) 2017 Ingenic Semiconductor Co.,Ltd
 * Author: Zoro <ykli@ingenic.cn>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* #define DEBUG */
#include <config.h>
#include <common.h>
#include <ddr/ddr_common.h>
#include <generated/ddr_reg_values.h>

#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/ddr_innophy.h>
#define CONFIG_DWC_DEBUG 1
#include "ddr_debug.h"

#if (CONFIG_DDR_CS1 == 1)
#ifndef DDR_ROW1
#error "please define DDR_ROW1"
#endif /* DDR_ROW1 */
#ifndef DDR_COL1
#error "please define DDR_COL1"
#endif /* DDR_COL1 */
#endif /* CONFIG_DDR_CS1 */

DECLARE_GLOBAL_DATA_PTR;
extern unsigned int sdram_size(int cs, struct ddr_params *p);

struct ddr_params *ddr_params_p = NULL;
#ifndef CONFIG_FPGA
extern void reset_dll(void);
#endif

#define BYPASS_ENABLE       1
#define BYPASS_DISABLE      0
#define IS_BYPASS_MODE(x)     (((x) & 1) == BYPASS_ENABLE)
	/* DDR3, */
	/* LPDDR, */
	/* LPDDR2, */
	/* DDR2,  */
	/* VARIABLE, */

#define DDR_TYPE_MODE(x)     (((x) >> 1) & 0xf)

static void dump_ddrc_register(void)
{
#ifdef CONFIG_DWC_DEBUG
	printf("DDRC_STATUS         0x%x\n", ddr_readl(DDRC_STATUS));
	printf("DDRC_CFG            0x%x\n", ddr_readl(DDRC_CFG));
	printf("DDRC_CTRL           0x%x\n", ddr_readl(DDRC_CTRL));
	printf("DDRC_LMR            0x%x\n", ddr_readl(DDRC_LMR));
	printf("DDRC_DLP            0x%x\n", ddr_readl(DDRC_DLP));
	printf("DDRC_TIMING1        0x%x\n", ddr_readl(DDRC_TIMING(1)));
	printf("DDRC_TIMING2        0x%x\n", ddr_readl(DDRC_TIMING(2)));
	printf("DDRC_TIMING3        0x%x\n", ddr_readl(DDRC_TIMING(3)));
	printf("DDRC_TIMING4        0x%x\n", ddr_readl(DDRC_TIMING(4)));
	printf("DDRC_TIMING5        0x%x\n", ddr_readl(DDRC_TIMING(5)));
	printf("DDRC_TIMING6        0x%x\n", ddr_readl(DDRC_TIMING(6)));
	printf("DDRC_REFCNT         0x%x\n", ddr_readl(DDRC_REFCNT));
	printf("DDRC_MMAP0          0x%x\n", ddr_readl(DDRC_MMAP0));
	printf("DDRC_MMAP1          0x%x\n", ddr_readl(DDRC_MMAP1));
	printf("DDRC_REMAP1         0x%x\n", ddr_readl(DDRC_REMAP(1)));
	printf("DDRC_REMAP2         0x%x\n", ddr_readl(DDRC_REMAP(2)));
	printf("DDRC_REMAP3         0x%x\n", ddr_readl(DDRC_REMAP(3)));
	printf("DDRC_REMAP4         0x%x\n", ddr_readl(DDRC_REMAP(4)));
	printf("DDRC_REMAP5         0x%x\n", ddr_readl(DDRC_REMAP(5)));
	printf("DDRC_AUTOSR_EN      0x%x\n", ddr_readl(DDRC_AUTOSR_EN));
#endif
}
static void reset_controller(void)
{
#ifndef CONFIG_FPGA
	ddr_writel(0xf << 20, DDRC_CTRL);
#else
	ddr_writel(0xc0 << 16, DDRC_CTRL);
#endif
	mdelay(5);
	ddr_writel(0, DDRC_CTRL);
	mdelay(5);
}

static void remap_swap(int a, int b)
{
	uint32_t remmap[2], tmp[2];

	remmap[0] = ddr_readl(DDRC_REMAP(a / 4 + 1));
	remmap[1] = ddr_readl(DDRC_REMAP(b / 4 + 1));

#define BIT(bit) ((bit % 4) * 8)
#define MASK(bit) (0x1f << BIT(bit))
	tmp[0] = (remmap[0] & MASK(a)) >> BIT(a);
	tmp[1] = (remmap[1] & MASK(b)) >> BIT(b);

	remmap[0] &= ~MASK(a);
	remmap[1] &= ~MASK(b);

	ddr_writel(remmap[0] | (tmp[1] << BIT(a)), DDRC_REMAP(a / 4 + 1));
	ddr_writel(remmap[1] | (tmp[0] << BIT(b)), DDRC_REMAP(b / 4 + 1));
#undef BIT
#undef MASK
}

static void mem_remap(void)
{
	uint32_t start = 0, num = 0;
	int row, col, dw32, bank8, cs0, cs1;
	uint32_t size0 = 0, size1 = 0;

#ifdef CONFIG_DDR_HOST_CC
#if (CONFIG_DDR_CS0 == 1)
			row = DDR_ROW;
			col = DDR_COL;
			dw32 = CONFIG_DDR_DW32;
			bank8 = DDR_BANK8;
#endif

	size0 = (unsigned int)(DDR_CHIP_0_SIZE);
	size1 = (unsigned int)(DDR_CHIP_1_SIZE);

	/* For two different size ddr chips, just don't remmap */

#if (CONFIG_DDR_CS1 == 1)
	if (size0 != size1)
		return;
#endif

#if (CONFIG_DDR_CS0 == 1)
#if (CONFIG_DDR_CS1 == 1)
	if (size1 && size0) {
		if (size1 <= size0) {
			row = DDR_ROW1;
			col = DDR_COL1;
			dw32 = CONFIG_DDR_DW32;
			bank8 = DDR_BANK8;
		} else {
			row = DDR_ROW;
			col = DDR_COL;
			dw32 = CONFIG_DDR_DW32;
			bank8 = DDR_BANK8;
		}
	} else {
		printf("Error: The DDR size is 0\n");
		hang();
	}
#else /*CONFIG_DDR_CS1 == 1*/
	if (size0) {
		row = DDR_ROW;
		col = DDR_COL;
		dw32 = CONFIG_DDR_DW32;
		bank8 = DDR_BANK8;
	} else {
		printf("Error: The DDR size is 0\n");
		hang();
	}

#endif /* CONFIG_DDR_CS1 == 1 */
#else /* CONFIG_DDR_CS0 == 1 */
	if (size1) {
		row = DDR_ROW1;
		col = DDR_COL1;
		dw32 = CONFIG_DDR_DW32;
		bank8 = DDR_BANK8;
	} else {
		printf("Error: The DDR size is 0\n");
		hang();
	}

#endif /* CONFIG_DDR_CS0 == 1 */
	cs0 = CONFIG_DDR_CS0;
	cs1 = CONFIG_DDR_CS1;
#else /* CONFIG_DDR_HOST_CC */
	size0 = ddr_params_p->size.chip0;
	size1 = ddr_params_p->size.chip1;
	if (size0 && size1) {
		if (size1 <= size0) {
			row = ddr_params_p->row1;
			col = ddr_params_p->col1;
			dw32 = ddr_params_p->dw32;
			bank8 = ddr_params_p->bank8;
		} else {
			row = ddr_params_p->row;
			col = ddr_params_p->col;
			dw32 = ddr_params_p->dw32;
			bank8 = ddr_params_p->bank8;
		}
	} else if (size0) {
		row = ddr_params_p->row;
		col = ddr_params_p->col;
		dw32 = ddr_params_p->dw32;
		bank8 = ddr_params_p->bank8;
	} else if (size1) {
		row = ddr_params_p->row1;
		col = ddr_params_p->col1;
		dw32 = ddr_params_p->dw32;
		bank8 = ddr_params_p->bank8;
	} else {
		printf("Error: The DDR size is 0\n");
		hang();
	}

	cs0 = ddr_params_p->cs0;
	cs1 = ddr_params_p->cs1;
#endif /* CONFIG_DDR_HOST_CC */
	start += row + col + (dw32 ? 4 : 2) / 2;
	start -= 12;

	if (bank8)
		num += 3;
	else
		num += 2;

	if (cs0 && cs1)
		num++;

	for (; num > 0; num--)
		remap_swap(0 + num - 1, start + num - 1);
}

void ddr_controller_init(void)
{
	dwc_debug("DDR Controller init\n");
//dsqiu
//mdelay(1);
	ddr_writel(DDRC_CTRL_CKE | DDRC_CTRL_ALH, DDRC_CTRL);
	ddr_writel(0, DDRC_CTRL);
	/* DDRC CFG init*/
	ddr_writel(DDRC_CFG_VALUE, DDRC_CFG);
	/* DDRC timing init*/
	ddr_writel(DDRC_TIMING1_VALUE, DDRC_TIMING(1));
//	ddr_writel(0x040e0806, DDRC_TIMING(1));
	ddr_writel(DDRC_TIMING2_VALUE, DDRC_TIMING(2));
	ddr_writel(DDRC_TIMING3_VALUE, DDRC_TIMING(3));
	ddr_writel(DDRC_TIMING4_VALUE, DDRC_TIMING(4));
	ddr_writel(DDRC_TIMING5_VALUE, DDRC_TIMING(5));
//	ddr_writel(0xff070405, DDRC_TIMING(5));
	ddr_writel(DDRC_TIMING6_VALUE, DDRC_TIMING(6));

	/* DDRC memory map configure*/
	ddr_writel(DDRC_MMAP0_VALUE, DDRC_MMAP0);
	ddr_writel(DDRC_MMAP1_VALUE, DDRC_MMAP1);
	ddr_writel(DDRC_CTRL_CKE | DDRC_CTRL_ALH, DDRC_CTRL);
	ddr_writel(DDRC_REFCNT_VALUE, DDRC_REFCNT);
	ddr_writel(DDRC_CTRL_VALUE & 0xffff8fff, DDRC_CTRL);
}

void dump_inno_phy(void)
{
	printf("INNO_DQ_WIDTH   :%X\n",phy_readl(INNO_DQ_WIDTH));
	printf("INNO_PLL_FBDIV  :%X\n",phy_readl(INNO_PLL_FBDIV));
	printf("INNO_PLL_PDIV   :%X\n",phy_readl(INNO_PLL_PDIV));
	printf("INNO_MEM_CFG    :%X\n",phy_readl(INNO_MEM_CFG));
	printf("INNO_PLL_CTRL   :%X\n",phy_readl(INNO_PLL_CTRL));
	printf("INNO_CHANNEL_EN :%X\n",phy_readl(INNO_CHANNEL_EN));
	printf("INNO_CWL        :%X\n",phy_readl(INNO_CWL));
	printf("INNO_CL         :%X\n",phy_readl(INNO_CL));
}

void ddr_inno_phy_init()
{
	u32 reg = 0;
	printf("ddr_inno_phy_init ..!\n");
    phy_writel(0x14,INNO_PLL_FBDIV);
	phy_writel(0x1a,INNO_PLL_CTRL);
	phy_writel(0x5,INNO_PLL_PDIV);
	phy_writel(0x18,INNO_PLL_CTRL);

    while(!(phy_readl(INNO_PLL_LOCK) & (1<<3))); //wait pll lock

	phy_writel(0x0,INNO_TRAINING_CTRL);
	phy_writel(0x03,INNO_DQ_WIDTH);
#ifdef CONFIG_DDR_TYPE_DDR3
	phy_writel(0x30,INNO_MEM_CFG);  // MEMSEL  =  DDR3  ,    BURSEL = burst8
	phy_writel((readl(PHY_BASE + 0x154) & 0xffffff8f),0x154);
	phy_writel((readl(PHY_BASE + 0x114) & 0xffffff8f),0x114);
	phy_writel(0x0d,INNO_CHANNEL_EN);
	phy_writel(0x6, INNO_CWL);
	phy_writel(0x7, INNO_CL);
#else
	phy_writel(0x11,INNO_MEM_CFG);  // MEMSEL  =  DDR2  ,    BURSEL = burst8
	phy_writel(0x0d,INNO_CHANNEL_EN);
	phy_writel(((DDRP_MR0_VALUE&0xf0)>>4)-1, INNO_CWL);
	reg = ((DDRP_MR0_VALUE&0xf0)>>4);
	phy_writel(reg, INNO_CL);
#endif
	printf("phy reg = 0x%x, CL = 0x%x\n", reg, phy_readl(INNO_CL));
	phy_writel(0x00,INNO_AL);

	//phy strength
#if 0
	printf("phy strength 1 0x%x\n", phy_readl(0x10*4));
	printf("phy strength 2 0x%x\n", phy_readl(0x11*4));
	printf("phy strength 3 0x%x\n", phy_readl(0x12*4));
	printf("phy strength 4 0x%x\n", phy_readl(0x13*4));
	printf("phy 0x14 0x%x\n", phy_readl(0x14*4));
#if 0
	phy_writel(0x2, 0x10*4);
	phy_writel(0x2, 0x11*4);
	phy_writel(0x2, 0x12*4);
	phy_writel(0x2, 0x13*4);
#endif
#if 0
	phy_writel(15, 0x129*4);
	phy_writel(15, 0x134*4);
#endif
	printf("phy 0x129 0x%x\n", phy_readl(0x129*4));
	printf("phy 0x134 0x%x\n", phy_readl(0x134*4));
#endif
    writel(0,DDR_APB_PHY_INIT); //start high
	while(!(readl(DDR_APB_PHY_INIT) & (1<<2)));//pll locked
	printf("ddr_inno_phy_init ..! 11:  %X\n", readl(DDR_APB_PHY_INIT));
    writel(0,REG_DDR_CTRL);

    while(!(readl(DDR_APB_PHY_INIT) & (1<<1))); //init_complete
	printf("ddr_inno_phy_init ..! 22:  %X\n", readl(DDR_APB_PHY_INIT));
    while(!readl(T31_INIT_COMP));
	printf("ddr_inno_phy_init ..! 33:  %X\n", readl(DDR_APB_PHY_INIT));
	writel(0,REG_DDR_CTRL);

    /* DDR3 init procedure:
     *     poweron-phyinit-DFI_RST(kgdreset)_SET-delay200us-DFI_RST(kgdreset)_CLEAR-delay500us-LMR */
#ifdef CONFIG_DDR_TYPE_DDR3
    ddr_writel(DDRC_CTRL_DFI_RST, DDRC_CTRL);
    udelay(200);
    ddr_writel(0,DDRC_CTRL);
    udelay(500);
#endif
    writel(DDRC_CFG_VALUE,REG_DDR_CFG);// r=13 , c=10 , bank=4 , bitwidth=16 ,  0x0a688a40
	writel(0x0a,REG_DDR_CTRL);

#ifdef CONFIG_DDR_TYPE_DDR3
	writel((0x08 << 12) | 0x211,REG_DDR_LMR);
    printf("REG_DDR_LMR: %x\n",readl(REG_DDR_LMR));
	writel(0,REG_DDR_LMR);

    writel(0x311,REG_DDR_LMR);
	printf("REG_DDR_LMR: %x\n", readl(REG_DDR_LMR));
	writel(0,REG_DDR_LMR);

	writel((0x6 << 12) | 0x111,REG_DDR_LMR); // 0x84
	printf("REG_DDR_LMR: %x\n", readl(REG_DDR_LMR));
	writel(0,REG_DDR_LMR);

	reg = ((DDRP_MR0_VALUE)<<12)|0x011;
	writel(reg, REG_DDR_LMR);
//	writel((0x830 << 12) | 0x011, REG_DDR_LMR);
	printf("REG_DDR_LMR, MR0: %x\n", readl(REG_DDR_LMR));
	writel(0,REG_DDR_LMR);

	writel(0x19,REG_DDR_LMR);
	printf("REG_DDR_LMR: %x\n", readl(REG_DDR_LMR));
	writel(0,REG_DDR_LMR);
#else
	writel(0x400001,REG_DDR_LMR);
	while((0x1&readl(REG_DDR_LMR)) == 1);

	writel(0x211,REG_DDR_LMR);
    printf("REG_DDR_LMR: %x\n",readl(REG_DDR_LMR));
	while((0x1&readl(REG_DDR_LMR)) == 1);

    writel(0x311,REG_DDR_LMR);
	printf("REG_DDR_LMR: %x\n", readl(REG_DDR_LMR));
	while((0x1&readl(REG_DDR_LMR)) == 1);

	writel(0x111,REG_DDR_LMR);
	printf("REG_DDR_LMR: %x\n", readl(REG_DDR_LMR));
	while((0x1&readl(REG_DDR_LMR)) == 1);

	reg = ((DDRP_MR0_VALUE)<<12)|0x011;
	writel(reg, REG_DDR_LMR);
	printf("REG_DDR_LMR, MR0: %x\n", reg);
	while((0x1&readl(REG_DDR_LMR)) == 1);
	udelay(5*1000);

	writel(0x400001,REG_DDR_LMR);
	while((0x1&readl(REG_DDR_LMR)) == 1);

	writel(0x400009,REG_DDR_LMR);
	while((0x1&readl(REG_DDR_LMR)) == 1);

	writel(0x400009,REG_DDR_LMR);
	while((0x1&readl(REG_DDR_LMR)) == 1);
	udelay(5*1000);

#endif
#if 0
	writel(0x19,REG_DDR_LMR);
	printf("REG_DDR_LMR: %x\n", readl(REG_DDR_LMR));
	writel(0,REG_DDR_LMR);
#endif

#ifdef CONFIG_DDR_TYPE_DDR3
	//write level
	writel(0x4,0xb301100C);
	writel(0x40,0xb3011010);
	writel(0xa4,0xb3011008);

	while (0x3 != readl((PHY_BASE + 0xc0)));
	printf("T31_c0: %x\n", readl((PHY_BASE + 0xc0)));

	writel(0xa1,0xb3011008);
#endif
	phy_calibration(0x1,0x1,0x1,0x1);

#ifdef CONFIG_DDR_TYPE_DDR3
	writel(0x50,0xb3011004);
#else
	writel(0x51,0xb3011004);
#endif
	writel(0x24,0xb3011028);
	printf("jz-04 :  0x%x \n",readl(0xb3011004));
	printf("jz-08 :  0x%x \n",readl(0xb3011008));
	printf("jz-28 :  0x%x \n",readl(0xb3011028));
#if 0
	int i = 0;
	printf("------------------------\n");
	for (i = 0; i < 500; i++) {
		printf("phy 0x%x, 0x%x\n", i, phy_readl(i*4));
	}
#endif
#if 1
    while(((readl(PHY_BASE + 0x190) & 0xe0) <= 0x20) && ((readl(PHY_BASE + 0x194) & 0xe0) <= 0x20)) {
        writel((readl(PHY_BASE + 0x04) | 0x40), (PHY_BASE + 0x04));
        writel((((readl(PHY_BASE + 0x28) & ~(0xe)) | 0x6)), (PHY_BASE + 0x28));
        break;
    }
#endif
	dwc_debug("DDR PHY init OK\n");
}

void phy_dqs_delay(int delay_l,int delay_h)
{
	writel(delay_l,T31_DQS_DELAY_L);
	writel(delay_h,T31_DQS_DELAY_H);

	printf("T31_DQS_DELAY_L: %x\n",readl(T31_DQS_DELAY_L));
	printf("T31_DQS_DELAY_H: %x\n",readl(T31_DQS_DELAY_H));
}
/*
 * Name     : phy_calibration()
 * Function : control the RX DQS window delay to the DQS
 *
 * a_low_8bit_delay		= al8_2x * clk_2x + al8_1x * clk_1x;
 * a_high_8bit_delay	= ah8_2x * clk_2x + ah8_1x * clk_1x;
 *
 * */
void phy_calibration(int al8_1x,int ah8_1x,int al8_2x,int ah8_2x)
{
#if 1
	printf("T31_0x5: %x\n",readl(PHY_BASE + 0x14));
	printf("T31_0x15: %x\n",readl(PHY_BASE + 0x54));
	printf("T31_0x4: %x\n",readl(PHY_BASE + 0x10));
	printf("T31_0x14: %x\n",readl(PHY_BASE + 0x50));

	int m=phy_readl(INNO_TRAINING_CTRL);
	printf("INNO_TRAINING_CTRL 1: %x\n", phy_readl(INNO_TRAINING_CTRL));
	m=(0xa1);
	phy_writel(m,INNO_TRAINING_CTRL);
	printf("INNO_TRAINING_CTRL 2: %x\n", phy_readl(INNO_TRAINING_CTRL));
	while (0x3 != readl((PHY_BASE + 0xcc)));
	printf("T31_cc: %x\n", readl((PHY_BASE + 0xcc)));
	phy_writel(0xa0,INNO_TRAINING_CTRL);
	printf("INNO_TRAINING_CTRL 3: %x\n", phy_readl(INNO_TRAINING_CTRL));
	printf("T31_118: %x\n", readl((PHY_BASE + 0x118)));
	printf("T31_158: %x\n", readl((PHY_BASE + 0x158)));
	printf("T31_190: %x\n", readl((PHY_BASE + 0x190)));
	printf("T31_194: %x\n", readl((PHY_BASE + 0x194)));
#else
	int m=phy_readl(INNO_TRAINING_CTRL);
	m=(0x1<<1);
	phy_writel(m,INNO_TRAINING_CTRL);
	//printf("T31_REG02: %x\n", phy_readl(INNO_TRAINING_CTRL));
	int x=readl(T31_REG46);
	int y=readl(T31_REG56);
	x=(x&(0x83)+(al8_1x<<3)+(al8_2x<<4));
	y=(y&(0x83)+(ah8_1x<<3)+(ah8_2x<<4));
	//debug for t30
	writel(30, T31_REG46);
	writel(27, T31_REG56);
	printf("T31_REG46: %x\n", readl(T31_REG46));
	printf("T31_REG56: %x\n", readl(T31_REG56));
#endif

}
/* DDR sdram init */
void sdram_init(void)
{

	int type = VARIABLE;
	unsigned int mode;
	unsigned int bypass = 0;
	unsigned int rate;
#ifdef CONFIG_DDR_TYPE_DDR3
	type = DDR3;
#endif
#ifdef CONFIG_DDR_TYPE_LPDDR
	type = LPDDR;
#endif
#ifdef CONFIG_DDR_TYPE_LPDDR2
	type = LPDDR2;
#endif

#ifdef CONFIG_DDR_TYPE_DDR2
	type = DDR2;
#endif

#ifndef CONFIG_DDR_HOST_CC
	struct ddrc_reg ddrc;
	struct ddrp_reg ddrp;
#ifndef CONFIG_DDR_TYPE_VARIABLE
	struct ddr_params ddr_params;
	ddr_params_p = &ddr_params;
#else
	ddr_params_p = &gd->arch.gi->ddr_params;
	ddr_params_p->freq = gd->arch.gi->cpufreq / gd->arch.gi->ddr_div;
#endif
	fill_in_params(ddr_params_p, type);
	ddr_params_creator(&ddrc, &ddrp, ddr_params_p);
	ddr_params_assign(&ddrc, &ddrp, ddr_params_p);
#endif /* CONFIG_DDR_HOST_CC */

	dwc_debug("sdram init start\n");
#ifndef CONFIG_FPGA
	clk_set_rate(DDR, gd->arch.gi->ddrfreq);
	reset_dll();
	rate = clk_get_rate(DDR);
#else
	rate = gd->arch.gi->ddrfreq;
#endif
#ifdef CONFIG_M200
	if(rate <= 150000000)
		bypass = 1;
#endif
	reset_controller();

#ifdef CONFIG_DDR_AUTO_SELF_REFRESH
	ddr_writel(0x0 ,DDRC_AUTOSR_EN);
#endif

	ddr_inno_phy_init();
	dump_inno_phy();
#if 0
	/*force CKE1 HIGH*/
	ddr_writel(DDRC_CFG_VALUE, DDRC_CFG);
	ddr_writel((1 << 1), DDRC_CTRL);
	mode = (type << 1) | (bypass & 1);
	/* DDR PHY init*/
	ddr_writel(0, DDRC_CTRL);
#endif
	/* DDR Controller init*/
	ddr_controller_init();
	/* DDRC address remap configure*/
	mem_remap();											// open remap function
	ddr_writel(DDRC_CTRL_VALUE & 0xffff07ff, DDRC_CTRL);	// must modify after opening remap function

	ddr_writel(ddr_readl(DDRC_STATUS) & ~DDRC_DSTATUS_MISS, DDRC_STATUS);
#ifdef CONFIG_DDR_AUTO_SELF_REFRESH
	if(!bypass)
		ddr_writel(0 , DDRC_DLP);
#ifdef CONFIG_FPGA
	//ddr_writel(ddr_readl(DDRP_DSGCR)&~(1<<4),DDRP_DSGCR);
#endif
	ddr_writel(0x1 ,DDRC_AUTOSR_EN);
#endif
	ddr_writel(0 , DDRC_DLP);

	dump_ddrc_register();
	dwc_debug("sdram init finished\n");
#undef DDRTYPE
}

phys_size_t initdram(int board_type)
{
#ifdef CONFIG_DDR_HOST_CC
	/* SDRAM size was calculated when compiling. */
#ifndef EMC_LOW_SDRAM_SPACE_SIZE
#define EMC_LOW_SDRAM_SPACE_SIZE 0x10000000 /* 256M */
#endif /* EMC_LOW_SDRAM_SPACE_SIZE */
	unsigned int ram_size;
	ram_size = (unsigned int)(DDR_CHIP_0_SIZE) + (unsigned int)(DDR_CHIP_1_SIZE);

	if (ram_size > EMC_LOW_SDRAM_SPACE_SIZE)
		ram_size = EMC_LOW_SDRAM_SPACE_SIZE;

	return ram_size;
#elif defined (CONFIG_BURNER)
	/* SDRAM size was defined in global info. */
	ddr_params_p = &gd->arch.gi->ddr_params;
	return ddr_params_p->size.chip0 + ddr_params_p->size.chip1;
#else
	ddr_params_p->dw32 = CONFIG_DDR_DW32;
	ddr_params_p->bank8 = DDR_BANK8;
	ddr_params_p->cs0 = CONFIG_DDR_CS0;
	ddr_params_p->cs1 = CONFIG_DDR_CS1;
	ddr_params_p->row = DDR_ROW;
	ddr_params_p->col = DDR_COL;
#ifdef DDR_ROW1
	ddr_params_p->row1 = DDR_ROW1;
#endif
#ifdef DDR_COL1
	ddr_params_p->col1 = DDR_COL1;
#endif
	return sdram_size(0, ddr_params_p) + sdram_size(1, ddr_params_p);
#endif
}
