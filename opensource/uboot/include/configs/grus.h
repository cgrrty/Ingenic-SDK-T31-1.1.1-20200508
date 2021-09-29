/*
 * Ingenic grus configuration
 *
 * Copyright (c) 2013 Ingenic Semiconductor Co.,Ltd
 * Author: Justin <ptkang@ingenic.cn>
 * Based on: include/configs/mensa.h
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

#ifndef __CONFIG_GRUS_H__
#define __CONFIG_GRUS_H__

/**
 * Basic configuration(SOC, Cache, UART, DDR).
 */
#define CONFIG_MIPS32		/* MIPS32 CPU core */
#define CONFIG_CPU_XBURST
#define CONFIG_SYS_LITTLE_ENDIAN
#define CONFIG_JZ4780		/* Jz4780 SoC */

#define CONFIG_SYS_APLL_FREQ		-1
#define CONFIG_SYS_MPLL_FREQ		1200000000
#define CONFIG_SYS_EPLL_FREQ		204000000
#define CONFIG_SYS_VPLL_FREQ		888000000
#define CONFIG_SYS_VPLL_M		(CONFIG_SYS_VPLL_FREQ * 2 / CONFIG_SYS_EXTAL)
#define CONFIG_SYS_VPLL_N		1
#define CONFIG_SYS_VPLL_OD		2

#define CONFIG_SYS_EXTAL		48000000	/* EXTAL freq: 48 MHz */
#define CONFIG_SYS_HZ			1000 /* incrementer freq */

#define CONFIG_SYS_CPU_FREQ		CONFIG_SYS_MPLL_FREQ
#define CONFIG_SYS_MEM_DIV		3
#define CONFIG_SYS_MEM_FREQ		(CONFIG_SYS_MPLL_FREQ / CONFIG_SYS_MEM_DIV)

#define CONFIG_SYS_DCACHE_SIZE		32768
#define CONFIG_SYS_ICACHE_SIZE		32768
#define CONFIG_SYS_CACHELINE_SIZE	32

#define CONFIG_SYS_UART_INDEX		3
#define CONFIG_BAUDRATE			57600

#define CONFIG_DDR_PARAMS_CREATOR
#define CONFIG_DDR_HOST_CC
#define CONFIG_DDR_TYPE_DDR3
#define CONFIG_DDR_CS0			1	/* 1-connected, 0-disconnected */
#define CONFIG_DDR_CS1			0	/* 1-connected, 0-disconnected */
#define CONFIG_DDR_DW32			1	/* 1-32bit-width, 0-16bit-width */
#define CONFIG_DDR3_H5TQ2G83CFR_H9C
#define CONFIG_DDR_CHIP_ODT
#define CONFIG_DDR_PHY_ODT
#define CONFIG_DDR_PHY_DQ_ODT
#define CONFIG_DDR_PHY_DQS_ODT

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
#define BOOTARGS_COMMON "console=ttyS3,57600 mem=256M@0x0 mem=768M@0x30000000"
#ifdef CONFIG_BOOT_ANDROID
  #define CONFIG_BOOTARGS BOOTARGS_COMMON " ip=off root=/dev/ram0 rw rdinit=/init"
#else
  #ifdef CONFIG_SPL_MMC_SUPPORT
    #define CONFIG_BOOTARGS BOOTARGS_COMMON " ip=off root=/dev/ram0 rw rdinit=/linuxrc"
  #else
    #define CONFIG_BOOTARGS BOOTARGS_COMMON " ubi.mtd=1 root=ubi0:root rootfstype=ubifs rw"
  #endif
#endif

/**
 * Boot command definitions.
 */
#define CONFIG_BOOTDELAY 3
#ifdef CONFIG_BOOT_ANDROID
  #ifdef CONFIG_SPL_MMC_SUPPORT
    #define CONFIG_BOOTCOMMAND	\
	  "batterydet; cls; boota mmc 0 0x80f00000 6144"
    #define CONFIG_NORMAL_BOOT CONFIG_BOOTCOMMAND
    #define CONFIG_RECOVERY_BOOT "boota mmc 0 0x80f00000 24576"
  #else
    #define CONFIG_BOOTCOMMAND  "nand_zm read ndboot 0 0x400000 0x80f00000;boota mem 0x80f00000"
    #define CONFIG_NORMAL_BOOT CONFIG_BOOTCOMMAND
    #define CONFIG_RECOVERY_BOOT "boota nand 0 0x80f00000 24576"
  #endif
#else  /* CONFIG_BOOT_ANDROID */
  #ifdef CONFIG_SPL_MMC_SUPPORT
    #define CONFIG_BOOTCOMMAND "mmc dev 0;mmc read 0x80f00000 0x1800 0x9000; bootm 0x80f00000"
  #else
	#ifdef CONFIG_SPL_SPI_SUPPORT
		#define CONFIG_BOOTCOMMAND "sf probe; sf read 0x80f00000 0x60000 0x340000; bootm 0x80f00000"
	#else
		#ifdef CONFIG_JZ_NAND_MGR
			#define CONFIG_BOOTCOMMAND  "nand_zm read ndboot 0 0x400000 0x80600000;bootm"
																	/*order ops pt offset len dst */
			/*#define CONFIG_BOOTCOMMAND		"nand_zm read ndboot;bootm"*/
		#else
		#define CONFIG_BOOTCOMMAND						\
	"mtdparts default; ubi part system; ubifsmount ubi:boot; "	\
	"ubifsload 0x80f00000 vmlinux.ub; bootm 0x80f00000"
		#endif /* endif CONFIG_JZ_NAND_MGR */
	#endif
  #endif
#endif /* CONFIG_BOOT_ANDROID */

/**
 * Drivers configuration.
 */
#define CONFIG_LCD
#ifdef CONFIG_LCD
#define LCD_BPP				5
#define CONFIG_GPIO_LCD_PWM	 	        GPIO_PE(0)
#define CONFIG_GPIO_LCD_POWERON                GPIO_PE(9)/*(32*4 + 9)*/
#define CONFIG_LCD_LOGO
#define CONFIG_RLE_LCD_LOGO
/*#define CONFIG_LCD_INFO_BELOW_LOGO*/      /*display the console info on lcd panel for debugg */
#define CONFIG_SYS_WHITE_ON_BLACK
#define CONFIG_SYS_PCLK_FREQ		33260000
#define CONFIG_SYS_PWM_PERIOD		10000 /* Pwm period in ns */
#define CONFIG_SYS_PWM_CHN		0  /* Pwm channel ok*/
#define CONFIG_SYS_PWM_FULL		256
#define CONFIG_SYS_BACKLIGHT_LEVEL	80 /* Backlight brightness is (80 / 256) */
#define CONFIG_FB_JZ4780_LCDC1
#define CONFIG_VIDEO_JZ4780
#define CONFIG_JZ_PWM
#define CONFIG_VIDEO_BYD_BM8766U
/*#define CONFIG_VIDEO_BM347WV_F_8991FTGF*/
#ifdef CONFIG_RLE_LCD_LOGO
/*#define CONFIG_CMD_BATTERYDET*/ 	/* detect battery and show charge logo */
#define CONFIG_CMD_LOGO_RLE	/*display the logo using the rle command*/
#endif
#endif /* CONFIG_LCD */

#define CONFIG_JZ_SPI_FLASH

/* MMC */
#define CONFIG_GENERIC_MMC		1
#define CONFIG_MMC			1
#define CONFIG_JZ_MMC 1
#define CONFIG_JZ_MMC_MSC0 1
#if !(defined(CONFIG_SPL_SPI_SUPPORT) || defined(CONFIG_SPI_BURNER))
#define CONFIG_JZ_MMC_MSC0_PA_4BIT 1
#endif
#define CONFIG_JZ_MMC_MSC1 1
#define CONFIG_JZ_MMC_MSC1_PE 1
#ifndef CONFIG_JZ_MMC_SPLMSC
#define CONFIG_JZ_MMC_SPLMSC 0
#endif

/* I2C */
#define CONFIG_SOFT_I2C
#define CONFIG_SYS_I2C_SPEED		50     /* the function is not implemented */
#define CONFIG_SYS_I2C_SLAVE		0x00   /* the function is not implemented */
#define CONFIG_SOFT_I2C_GPIO_SCL	GPIO_PE(31)
#define CONFIG_SOFT_I2C_GPIO_SDA	GPIO_PE(30)

/* SPI */
#if defined(CONFIG_SPI_BURNER)
/* CONFIG_SOFT_SPI */
#ifdef CONFIG_SOFT_SPI
#undef SPI_INIT
#define SPI_DELAY
#define	SPI_SDA(val)    gpio_direction_output(GPIO_PA(21), val)
#define	SPI_SCL(val)	gpio_direction_output(GPIO_PA(18), val)
#define	SPI_READ	gpio_get_value(GPIO_PA(20))
#else
#define CONFIG_JZ_SPI
#endif
#define CONFIG_SSI_BASE SSI0_BASE
#define CONFIG_CMD_SF
#define CONFIG_SPI_BUILD
#define CONFIG_SPI_FLASH_INGENIC
#define CONFIG_SPI_FLASH
#endif
/* PMU */
#define CONFIG_REGULATOR
#define CONFIG_PMU_ACT8600

/* Ethernet:  davicom DM9000 */
#define CONFIG_DRIVER_DM9000		1
#define CONFIG_DM9000_BASE		0xb6000000
#define DM9000_IO			CONFIG_DM9000_BASE
#define DM9000_DATA			(CONFIG_DM9000_BASE + 2)
#define CONFIG_NET_JZ4780
#define CONFIG_GPIO_DM9000_RESET	GPIO_PF(18)
#define CONFIG_GPIO_DM9000_RESET_ENLEVEL	0

/* DEBUG Ethernet
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
#define CONFIG_CMD_ECHO		/* echo arguments		*/
#define CONFIG_CMD_EXT4 	/* ext4 support			*/
#define CONFIG_CMD_FAT		/* FAT support			*/
#define CONFIG_CMD_LOADB	/* loadb			*/
#define CONFIG_CMD_LOADS	/* loads			*/
#define CONFIG_CMD_MEMORY	/* md mm nm mw cp cmp crc base loop mtest */
#define CONFIG_CMD_MISC		/* Misc functions like sleep etc*/
#define CONFIG_CMD_RUN		/* run command in env variable	*/
#define CONFIG_CMD_SETGETDCR	/* DCR support on 4xx		*/
#define CONFIG_CMD_SOURCE	/* "source" command support	*/
#define CONFIG_CMD_GETTIME
#ifndef CONFIG_SPL_SPI_SUPPORT
#define CONFIG_CMD_MMC		/* MMC/SD support		*/
#define CONFIG_CMD_SAVEENV	/* saveenv			*/
#define CONFIG_CMD_NET		/* networking support		*/
#define CONFIG_CMD_DHCP		/* DHCP support			*/
#define CONFIG_CMD_PING
#endif

/* USB */
#ifdef CONFIG_BOOT_ANDROID
#define CONFIG_CMD_FASTBOOT
#define CONFIG_USB_GADGET
#define CONFIG_USB_GADGET_DUALSPEED
#define CONFIG_USB_JZ_DWC2_UDC
#define CONFIG_FASTBOOT_GADGET
#define CONFIG_FASTBOOT_FUNCTION
#define CONFIG_G_FASTBOOT_VENDOR_NUM	(0x18d1)
#define CONFIG_G_FASTBOOT_PRODUCT_NUM	(0xdddd)
#define CONFIG_USB_GADGET_VBUS_DRAW 500
#endif  /*CONFIG_CMD_FASTBOOT*/

#if 0
#define CONFIG_SERVERIP		192.168.1.1
#define CONFIG_IPADDR		192.168.1.2
#define CONFIG_GATEWAYIP        192.168.1.1
#define CONFIG_NETMASK          255.255.255.0
#define CONFIG_ETHADDR          00:11:22:33:44:55
#define CONFIG_USB_GADGET
#define CONFIG_USB_GADGET_DUALSPEED
#define CONFIG_USB_JZ_DWC2_UDC_V1_1
/*#define CONFIG_USB_SELF_POLLING*/
#define CONFIG_USB_ETH_SUBSET
#define CONFIG_USBNET_DEV_ADDR		"de:ad:be:ef:00:01"
#define CONFIG_USBNET_HOST_ADDR		"de:ad:be:ef:00:02"
#define CONFIG_USB_ETHER
#define CONFIG_USB_ETH_RNDIS
#define CONFIG_USB_ETH_HALT
/*#define CONFIG_EXTRA_ENV_SETTINGS		\
	"netretry=no\0"		\
	""*/
#endif

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

#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
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

#ifdef CONFIG_SPL_SPI_SUPPORT
#define CONFIG_SPL_PAD_TO		16384
#endif
#ifdef CONFIG_SPL_MMC_SUPPORT
#define CONFIG_SPL_PAD_TO		15872  /* u-boot start addr - mbr size(512) */
#endif
#ifdef CONFIG_JZ_NAND_MGR
#define CONFIG_CMD_ZM_NAND		/* support nand manager */
#define CONFIG_SPL_PAD_TO		16384
#endif


#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x20 /* 16KB offset */
#define CONFIG_SYS_U_BOOT_MAX_SIZE_SECTORS	0x400 /* 512 KB */

#define CONFIG_SPL_BOARD_INIT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_GPIO_SUPPORT
#define CONFIG_SPL_I2C_SUPPORT
#define CONFIG_SPL_REGULATOR_SUPPORT
#define CONFIG_SPL_CORE_VOLTAGE		1250

#ifdef CONFIG_SPL_SPI_SUPPORT
#define CONFIG_SPL_TEXT_BASE		0xf4000800
#define CONFIG_SPL_MAX_SIZE		((16 * 1024) - 0x800)
#define CONFIG_JZ_SPI
#define CONFIG_JZ_SPI_FLASH
#define CONFIG_CMD_SF
#define CONFIG_SPI_FLASH_INGENIC
#define CONFIG_SPI_FLASH
#define CONFIG_CMD_SPI
#endif
#define CONFIG_UBOOT_OFFSET		16384

#ifdef CONFIG_SPL_MMC_SUPPORT
#define CONFIG_SPL_TEXT_BASE		0xf4000a00
#define CONFIG_SPL_MAX_SIZE		((16 * 1024) - 0xa00)
#define CONFIG_SPL_SERIAL_SUPPORT
#endif /* CONFIG_SPL_MMC_SUPPORT */

#ifdef CONFIG_JZ_NAND_MGR
/**/
#define CONFIG_SPL_NAND_BASE
#define CONFIG_SPL_NAND_DRIVERS
#define CONFIG_SPL_NAND_SIMPLE
#define CONFIG_SPL_NAND_LOAD

/*nand gpio init*/
#define CONFIG_NAND_LOADER
#define CFG_NAND_BW8    1
#define CONFIG_NAND_CS  1

/* environment */
#define CONFIG_ENV_IS_IN_NAND_ZM
#define CMDLINE_PARTITION   "ndcmdline"
#define CONFIG_ENV_SIZE         (32 << 10)
#define CONFIG_ENV_OFFSET       0

/**/
#define CONFIG_SPL_TEXT_BASE		0xf4000800
#define CONFIG_SPL_MAX_SIZE		((16 * 1024) - 0x800)

/* the NAND SPL is small enough to enable serial */
#define CONFIG_SPL_SERIAL_SUPPORT

#endif /* CONFIG_JZ_NAND_MGR */

#ifdef CONFIG_SPL_SPI_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPI_SPL_CHECK
#define CONFIG_SYS_SPI_BOOT_FREQ	1000000
#endif

#ifdef CONFIG_SPL_NOR_SUPPORT
#define CONFIG_SYS_UBOOT_BASE		0x80007000
#define CONFIG_SYS_OS_BASE		0
#define CONFIG_SYS_SPL_ARGS_ADDR	0
#define CONFIG_SYS_FDT_BASE		0
#endif

/**
 * MBR configuration
 */
#ifdef CONFIG_MBR_CREATOR
#define CONFIG_MBR_P0_OFF	64mb
#define CONFIG_MBR_P0_END	556mb
#define CONFIG_MBR_P0_TYPE 	linux

#define CONFIG_MBR_P1_OFF	580mb
#define CONFIG_MBR_P1_END 	1604mb
#define CONFIG_MBR_P1_TYPE 	linux

#define CONFIG_MBR_P2_OFF	28mb
#define CONFIG_MBR_P2_END	58mb
#define CONFIG_MBR_P2_TYPE 	linux

#define CONFIG_MBR_P3_OFF	1609mb
#define CONFIG_MBR_P3_END	7800mb
#define CONFIG_MBR_P3_TYPE 	fat
#endif

/**
 * Keys.
 */
#define CONFIG_GPIO_USB_DETECT		GPIO_PF(12)
#define CONFIG_GPIO_USB_DETECT_ENLEVEL	1

#define CONFIG_GPIO_RECOVERY		GPIO_PF(9)	/* SW1 */
#define CONFIG_GPIO_RECOVERY_ENLEVEL	0

#define CONFIG_GPIO_FASTBOOT		GPIO_PF(10)	/* SW2 */
#define CONFIG_GPIO_FASTBOOT_ENLEVEL	0

#define CONFIG_GPIO_MENU		GPIO_PF(10)	/* SW2 */
#define CONFIG_GPIO_MENU_ENLEVEL	0

#define CONFIG_GPIO_VOL_SUB		GPIO_PD(18)	/* SW4 */
#define CONFIG_GPIO_VOL_SUB_ENLEVEL	0

#define CONFIG_GPIO_VOL_ADD		GPIO_PD(17)	/* SW5 */
#define CONFIG_GPIO_VOL_ADD_ENLEVEL	1

#define CONFIG_GPIO_BACK		GPIO_PF(9)	/* SW1 */
#define CONFIG_GPIO_BACK_ENLEVEL	0

#define CONFIG_GPIO_PWR_WAKE		GPIO_PA(30)
#define CONFIG_GPIO_PWR_WAKE_ENLEVEL	0

#endif /* __CONFIG_GRUS_H__ */
