/*
 * T30 GPIO definitions
 * Copyright (c) 2017  Ingenic Semiconductor Co.,Ltd
 *
 * Author: Sonil <ztyan@ingenic.cn>
 * Based on: newxboot/modules/gpio/jz4775_gpio.c|jz4780_gpio.c
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

static struct jz_gpio_func_def uart_gpio_func[] = {
	[0] = { .port = GPIO_PORT_B, .func = GPIO_FUNC_0, .pins = 0x9 << 19},
	[1] = { .port = GPIO_PORT_B, .func = GPIO_FUNC_0, .pins = 0x3 << 23},
};

static struct jz_gpio_func_def gpio_func[] = {
#if defined(CONFIG_JZ_MMC_MSC0_PB)
	{ .port = GPIO_PORT_B, .func = GPIO_FUNC_0, .pins = (0x3<<4|0xf<<0)},
#endif
#if defined(CONFIG_JZ_SFC_PA)
	{ .port = GPIO_PORT_A, .func = GPIO_FUNC_1, .pins = (0x3<<23) | (0x3<<27)},
#endif
};
