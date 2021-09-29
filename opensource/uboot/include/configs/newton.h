/*
 * Ingenic newton configuration
 *
 * Copyright (c) 2013 Ingenic Semiconductor Co.,Ltd
 * Author: Zoro <ykli@ingenic.cn>
 * Based on: include/configs/urboard.h
 *           Written by Paul Burton <paul.burton@imgtec.com>
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

#ifndef __CONFIG_NEWTON_H__
#define __CONFIG_NEWTON_H__

/**
 * Basic configuration(SOC, Cache, UART, DDR).
 */
#define CONFIG_MIPS32		/* MIPS32 CPU core */
#define CONFIG_CPU_XBURST
#define CONFIG_SYS_LITTLE_ENDIAN
#define CONFIG_JZ4775		/* Jz4775 SoC */

#define CONFIG_SYS_APLL_FREQ		1008000000
#define CONFIG_SYS_MPLL_FREQ		-1

#define CONFIG_SYS_EXTAL		24000000	/* EXTAL freq: 48 MHz */
#define CONFIG_SYS_HZ			1000 /* incrementer freq */

#define CONFIG_SYS_CPU_FREQ		CONFIG_SYS_APLL_FREQ
#define CONFIG_SYS_MEM_DIV		6
#define CONFIG_SYS_MEM_FREQ		(CONFIG_SYS_APLL_FREQ / CONFIG_SYS_MEM_DIV)

#define CONFIG_SYS_DCACHE_SIZE		16384
#define CONFIG_SYS_ICACHE_SIZE		16384
#define CONFIG_SYS_CACHELINE_SIZE	32

#define CONFIG_SYS_UART_INDEX		3
#define CONFIG_BAUDRATE			57600

#define CONFIG_DDR_PARAMS_CREATOR
#define CONFIG_DDR_HOST_CC
#define CONFIG_DDR_TYPE_LPDDR
#define CONFIG_DDR_CS0			1	/* 1-connected, 0-disconnected */
#define CONFIG_DDR_CS1			1	/* 1-connected, 0-disconnected */
#define CONFIG_DDR_DW32			1	/* 1-32bit-width, 0-16bit-width */
#define CONFIG_MCP_KMS5U000JM_B308

/* #define CONFIG_DDR_DLL_OFF */
/*
 * #define CONFIG_DDR_CHIP_ODT
 * #define CONFIG_DDR_PHY_ODT
 * #define CONFIG_DDR_PHY_DQ_ODT
 * #define CONFIG_DDR_PHY_DQS_ODT
 * #define CONFIG_DDR_PHY_IMPED_PULLUP		0xe
 * #define CONFIG_DDR_PHY_IMPED_PULLDOWN	0xe
 */

/**
 * Boot arguments definitions.
 */
#define BOOTARGS_COMMON "console=ttyS3,57600 mem=256M@0x0 mem=128M@0x30000000"

#ifdef CONFIG_BOOT_ANDROID
  #define CONFIG_BOOTARGS BOOTARGS_COMMON " ip=off root=/dev/ram0 rw rdinit=/init"
#else
  #ifdef CONFIG_SPL_MMC_SUPPORT
    #define CONFIG_BOOTARGS BOOTARGS_COMMON "ip=off root=/dev/mmcblk0p1 rw init=/linuxrc"
  #else
    #define CONFIG_BOOTARGS BOOTARGS_COMMON " ubi.mtd=1 root=ubi0:root rootfstype=ubifs rw"
  #endif
#endif

/**
 * Boot command definitions.
 */
#define CONFIG_BOOTDELAY 1
#ifdef CONFIG_BOOT_ANDROID
  #ifdef CONFIG_SPL_MMC_SUPPORT
    #define CONFIG_BOOTCOMMAND	\
	  "batterydet; cls; boota mmc 0 0x80f00000 6144"
    #define CONFIG_NORMAL_BOOT CONFIG_BOOTCOMMAND
    #define CONFIG_RECOVERY_BOOT "boota mmc 0 0x80f00000 22528"
  #else
    #define CONFIG_BOOTCOMMAND "boota nand 0 0x80f00000 6144"
    #define CONFIG_NORMAL_BOOT CONFIG_BOOTCOMMAND
    #define CONFIG_RECOVERY_BOOT "boota nand 0 0x80f00000 22528"
  #endif
#else  /* CONFIG_BOOT_ANDROID */
  #ifdef CONFIG_SPL_MMC_SUPPORT
    #define CONFIG_BOOTCOMMAND "mmc dev 0;mmc read 0x80f00000 0x1800 0x3000; bootm 0x80f00000"
  #else
    #define CONFIG_BOOTCOMMAND						\
	"mtdparts default; ubi part system; ubifsmount ubi:boot; "	\
	"ubifsload 0x80f00000 vmlinux.ub; bootm 0x80f00000"
  #endif
#endif /* CONFIG_BOOT_ANDROID */

/**
 * Drivers configuration.
 */
#define  CONFIG_LCD
#ifdef CONFIG_LCD
#define LCD_BPP				5
#define CONFIG_GPIO_LCD_PWM	 	GPIO_PE(1)
#define CONFIG_LCD_LOGO
#define CONFIG_RLE_LCD_LOGO
/*#define CONFIG_LCD_INFO_BELOW_LOGO*/      /*display the console info on lcd panel for debugg */
#define CONFIG_SYS_WHITE_ON_BLACK
#define CONFIG_SYS_PCLK_FREQ		33260000
#define CONFIG_SYS_PWM_PERIOD		10000 /* Pwm period in ns */
#define CONFIG_SYS_PWM_CHN		1  /* Pwm channel ok*/
#define CONFIG_SYS_PWM_FULL		256
#define CONFIG_SYS_BACKLIGHT_LEVEL	80 /* Backlight brightness is (80 / 256) */
#define CONFIG_VIDEO_JZ4775
#define CONFIG_JZ_PWM
/*#define CONFIG_VIDEO_BYD_BM8766U*/
/*#define CONFIG_VIDEO_BM347WV_F_8991FTGF*/
#define CONFIG_SLCD_FUNCTION
#define CONFIG_VIDEO_TRULY_TFT240240_2_E
#ifdef CONFIG_RLE_LCD_LOGO
#define CONFIG_CMD_BATTERYDET   	/* detect battery and show charge logo */
#define CONFIG_CMD_LOGO_RLE   	/* display the logo using rle command */
#endif
#endif /* CONFIG_LCD */

/* MMC */
#define CONFIG_GENERIC_MMC		1
#define CONFIG_MMC			1
#define CONFIG_JZ_MMC 1
#define CONFIG_JZ_MMC_MSC0 1
#define CONFIG_JZ_MMC_MSC0_PA_4BIT 1
#define CONFIG_JZ_MMC_MSC1 1
#define CONFIG_JZ_MMC_MSC1_PE 1
#ifndef CONFIG_JZ_MMC_SPLMSC
#define CONFIG_JZ_MMC_SPLMSC 0
#endif


/* I2C */
/* add the I2C later */
#define CONFIG_SOFT_I2C
#ifdef CONFIG_SOFT_I2C
#define CONFIG_SYS_I2C_SPEED		50     /* the function is not implemented */
#define CONFIG_SYS_I2C_SLAVE		0x00   /* the function is not implemented */
#define CONFIG_SOFT_I2C_GPIO_SCL	GPIO_PE(3)
#define CONFIG_SOFT_I2C_GPIO_SDA	GPIO_PE(0)
#endif
/* PMU */
/* add the pmu later */
#define CONFIG_REGULATOR
#ifdef  CONFIG_REGULATOR
#define CONFIG_PMU_D2041
#endif

/* Ethernet: gmac and 9161 */
#undef CONFIG_NET_JZ4775
#ifdef CONFIG_NET_JZ4775
#define CONFIG_GPIO_DM9161_RESET	GPIO_PB(7)
#define CONFIG_GPIO_DM9161_RESET_ENLEVEL	0
#endif
/* DEBUG ETHERNET
#define CONFIG_SERVERIP		192.168.4.122
#define CONFIG_IPADDR		192.168.4.121
#define CONFIG_GATEWAYIP        192.168.4.1
#define CONFIG_NETMASK          255.255.255.0
#define CONFIG_ETHADDR          00:11:22:33:44:55
*/

/* GPIO */
#define CONFIG_JZ_GPIO

/**
 * Command configuration.
 */
#define CONFIG_CMD_BOOTD	/* bootd			*/
#define CONFIG_CMD_CONSOLE	/* coninfo			*/
#define CONFIG_CMD_DHCP 	/* DHCP support			*/
#define CONFIG_CMD_ECHO		/* echo arguments		*/
#define CONFIG_CMD_EXT4 	/* ext4 support			*/
#define CONFIG_CMD_FAT		/* FAT support			*/
#define CONFIG_CMD_LOADB	/* loadb			*/
#define CONFIG_CMD_LOADS	/* loads			*/
#define CONFIG_CMD_MEMORY	/* md mm nm mw cp cmp crc base loop mtest */
#define CONFIG_CMD_MISC		/* Misc functions like sleep etc*/
#define CONFIG_CMD_MMC		/* MMC/SD support			*/
#define CONFIG_CMD_NET		/* networking support			*/
#define CONFIG_CMD_PING
#define CONFIG_CMD_RUN		/* run command in env variable	*/
#define CONFIG_CMD_SAVEENV	/* saveenv			*/
#define CONFIG_CMD_SETGETDCR	/* DCR support on 4xx		*/
#define CONFIG_CMD_SOURCE	/* "source" command support	*/
#define CONFIG_CMD_GETTIME

/* USB */
#ifdef CONFIG_BOOT_ANDROID
/* #define CONFIG_CMD_FASTBOOT */
#define CONFIG_USB_GADGET
#define CONFIG_USB_GADGET_DUALSPEED
#define CONFIG_USB_JZ_DWC2_UDC
#define CONFIG_FASTBOOT_GADGET
#define CONFIG_FASTBOOT_FUNCTION
#define CONFIG_G_FASTBOOT_VENDOR_NUM	(0x18d1)
#define CONFIG_G_FASTBOOT_PRODUCT_NUM	(0xdddd)
#define CONFIG_USB_GADGET_VBUS_DRAW 500
#endif  /*CONFIG_CMD_FASTBOOT*/

/**
 * Serial download configuration
 */
#define CONFIG_LOADS_ECHO	1	/* echo on for serial download */

/**
 * Miscellaneous configurable options
 */
#define CONFIG_DOS_PARTITION

#define CONFIG_LZO
#define CONFIG_RBTREE

#define CONFIG_SKIP_LOWLEVEL_INIT
#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_SYS_NO_FLASH
#define CONFIG_SYS_FLASH_BASE	0 /* init flash_base as 0 */
#define CONFIG_ENV_OVERWRITE
#define CONFIG_MISC_INIT_R 1

#define CONFIG_BOOTP_MASK	(CONFIG_BOOTP_DEFAUL)

#define CONFIG_SYS_MAXARGS 16
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD "# "
#define CONFIG_SYS_CBSIZE 1024 /* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)

#define CONFIG_SYS_MONITOR_LEN		(2 * 1024 * 1024)
#define CONFIG_SYS_MALLOC_LEN		(64 * 1024 * 1024)
#define CONFIG_SYS_BOOTPARAMS_LEN	(128 * 1024)

#define CONFIG_SYS_SDRAM_BASE		0x80000000 /* cached (KSEG0) address */
#define CONFIG_SYS_SDRAM_MAX_TOP	0x90000000 /* don't run into IO space */
#define CONFIG_SYS_INIT_SP_OFFSET	0x400000
#define CONFIG_SYS_LOAD_ADDR		0x88000000
#define CONFIG_SYS_MEMTEST_START	0x80000000
#define CONFIG_SYS_MEMTEST_END		0x88000000

#define CONFIG_SYS_TEXT_BASE		0x80100000
#define CONFIG_SYS_MONITOR_BASE		CONFIG_SYS_TEXT_BASE

/**
 * Environment
 */
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_ENV_SIZE			(32 << 10)
#define CONFIG_ENV_OFFSET		(CONFIG_SYS_MONITOR_LEN + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)
#else
#define CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_SIZE			(32 << 10)
#define CONFIG_ENV_OFFSET		(CONFIG_SYS_NAND_BLOCK_SIZE * 5)
#endif

#ifdef CONFIG_SPL_SPI_SUPPORT
#define CONFIG_SPL_PAD_TO		16384
#endif
#ifdef CONFIG_SPL_MMC_SUPPORT
#define CONFIG_SPL_PAD_TO		15872  /* u-boot start addr - mbr size(512) */
#endif
#ifdef CONFIG_JZ_NAND_MGR
#define CONFIG_SPL_PAD_TO		16384
#endif

/**
 * SPL configuration
 */
#define CONFIG_SPL
#define CONFIG_SPL_FRAMEWORK
#define CONFIG_SPL_STACK		0x80003000 /* 12K stack_size */
#define CONFIG_SPL_BSS_START_ADDR	0x80003000
#define CONFIG_SPL_BSS_MAX_SIZE		0x1000 /* stack_size + bss_size <= CONFIG_SYS_DCACHE_SIZE */

#define CONFIG_SPL_NO_CPU_SUPPORT_CODE
#define CONFIG_SPL_START_S_PATH		"$(CPUDIR)/$(SOC)"
#define CONFIG_SPL_LDSCRIPT		"$(CPUDIR)/$(SOC)/u-boot-spl.lds"

#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x20 /* 16KB offset */
#define CONFIG_SYS_U_BOOT_MAX_SIZE_SECTORS	0x400 /* 512 KB */
#define CONFIG_SYS_NAND_U_BOOT_OFFS	(CONFIG_SYS_NAND_BLOCK_SIZE * 4)
#define CONFIG_SYS_NAND_U_BOOT_DST	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_NAND_U_BOOT_START	CONFIG_SYS_NAND_U_BOOT_DST
#define CONFIG_SYS_NAND_U_BOOT_SIZE	(512 * 1024)

#define CONFIG_SPL_BOARD_INIT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_GPIO_SUPPORT

#ifdef CONFIG_SPL_MMC_SUPPORT

#define CONFIG_SPL_TEXT_BASE		0xf4000a00
#define CONFIG_SPL_MAX_SIZE		((16 * 1024) - 0xa00)

#define CONFIG_SPL_SERIAL_SUPPORT

#else /* !CONFIG_SPL_MMC_SUPPORT */

#define CONFIG_SPL_NAND_SUPPORT
#define CONFIG_SPL_NAND_BASE
#define CONFIG_SPL_NAND_DRIVERS
#define CONFIG_SPL_NAND_SIMPLE
#define CONFIG_SPL_NAND_LOAD

#define CONFIG_SPL_TEXT_BASE		0xf4000800
#define CONFIG_SPL_MAX_SIZE		((16 * 1024) - 0x800)

/* the NAND SPL is small enough to enable serial */
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_LIBCOMMON_SUPPORT

#endif /* !CONFIG_SPL_MMC_SUPPORT */

/**
 * GPT configuration
 */
#ifdef CONFIG_GPT_CREATOR
#define CONFIG_GPT_TABLE_PATH   "$(TOPDIR)/board/$(BOARDDIR)"
#else
/**
 * MBR configuration
 */
#define CONFIG_MBR_P0_OFF	56mb
#define CONFIG_MBR_P0_END	512mb
#define CONFIG_MBR_P0_TYPE 	linux

#define CONFIG_MBR_P1_OFF	568mb
#define CONFIG_MBR_P1_END 	1024mb
#define CONFIG_MBR_P1_TYPE 	linux

#define CONFIG_MBR_P2_OFF	1592mb
#define CONFIG_MBR_P2_END	1mb
#define CONFIG_MBR_P2_TYPE 	linux

#define CONFIG_MBR_P3_OFF	1593mb
#define CONFIG_MBR_P3_END	1mb
#define CONFIG_MBR_P3_TYPE 	fat

#endif

/**
 * Keys.
 */
/*#define CONFIG_GPIO_USB_DETECT		GPIO_PA(16)*/
#define CONFIG_GPIO_USB_DETECT		GPIO_PB(29)
#define CONFIG_GPIO_USB_DETECT_ENLEVEL	0

#define CONFIG_GPIO_RECOVERY		GPIO_PD(19)
#define CONFIG_GPIO_RECOVERY_ENLEVEL	0

#define CONFIG_GPIO_FASTBOOT		GPIO_PG(15)
#define CONFIG_GPIO_FASTBOOT_ENLEVEL	0

#define CONFIG_GPIO_BACK		GPIO_PD(19)
#define CONFIG_GPIO_BACK_ENLEVEL	0

#define CONFIG_GPIO_PWR_WAKE		GPIO_PA(30)
#define CONFIG_GPIO_PWR_WAKE_ENLEVEL	0

/* TEST
#define CONFIG_GPIO_DC_DETECT           GPIO_PG(10)
#define CONFIG_GPIO_DC_DETECT_ENLEVEL   1

#define CONFIG_GPIO_CHARGE_DETECT               GPIO_PG(12)
#define CONFIG_GPIO_CHARGE_DETECT_ENLEVEL       0
*/
#endif /* __CONFIG_NEWTON_H__ */
