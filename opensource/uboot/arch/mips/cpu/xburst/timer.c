/*
 * Timer for JZ4775, JZ4780
 *
 * Copyright (c) 2013 Imagination Technologies
 * Author: Paul Burton <paul.burton@imgtec.com>
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

#include <config.h>
#include <common.h>
#include <div64.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/arch/ost.h>

DECLARE_GLOBAL_DATA_PTR;

unsigned int multiple __attribute__ ((section(".data")));

static uint32_t tcu_readl(uint32_t off)
{
	return readl((void __iomem *)TCU_BASE + off);
}

static void tcu_writew(uint16_t val, uint32_t off)
{
	writew(val, (void __iomem *)TCU_BASE + off);
}

static void tcu_writel(uint32_t val, uint32_t off)
{
	writel(val, (void __iomem *)TCU_BASE + off);
}

#define USEC_IN_1SEC 1000000
int timer_init(void)
{
#ifdef CONFIG_BURNER
	multiple = gd->arch.gi->extal / USEC_IN_1SEC / OST_DIV;
#else
	multiple = CONFIG_SYS_EXTAL / USEC_IN_1SEC / OST_DIV;
#endif

	reset_timer();
	tcu_writel(OSTCSR_CNT_MD | OSTCSR_PRESCALE | OSTCSR_EXT_EN, TCU_OSTCSR);
	tcu_writew(TER_OSTEN, TCU_TESR);

	return 0;
}

void reset_timer(void)
{
	tcu_writel(0, TCU_OSTCNTH);
	tcu_writel(0, TCU_OSTCNTL);
	tcu_writel(0, TCU_OSTDR);
}

static uint64_t get_timer64(void)
{
	uint32_t low = tcu_readl(TCU_OSTCNTL);
	uint32_t high = tcu_readl(TCU_OSTCNTHBUF);
	return ((uint64_t)high << 32) | low;
}

ulong get_timer(ulong base)
{
	return lldiv(get_timer64(), (USEC_IN_1SEC/CONFIG_SYS_HZ) * multiple) - base;
}

void __udelay(unsigned long usec)
{
	/* OST count increments at 3MHz */
	uint64_t end = get_timer64() + ((uint64_t)usec * multiple);
	while (get_timer64() < end);
}

unsigned long long get_ticks(void)
{
	return get_timer64();
}

ulong get_tbclk(void)
{
	return CONFIG_SYS_HZ;
}
