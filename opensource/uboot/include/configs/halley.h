/*
 * Ingenic mensa configuration
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

#ifndef __CONFIG_HALLEY_H__
#define __CONFIG_HALLEY_H__

/**
 * Basic configuration(SOC, Cache, UART, DDR).
 */
#define CONFIG_MIPS32		/* MIPS32 CPU core */
#define CONFIG_CPU_XBURST
#define CONFIG_SYS_LITTLE_ENDIAN
#define CONFIG_M150		/* Jz4775 SoC */

#define CONFIG_SYS_APLL_FREQ		800000000
#define CONFIG_SYS_MPLL_FREQ		-1

#define CONFIG_SYS_EXTAL		24000000	/* EXTAL freq: 48 MHz */
#define CONFIG_SYS_HZ			1000 /* incrementer freq */

#define CONFIG_SYS_CPU_FREQ		CONFIG_SYS_APLL_FREQ
#define CONFIG_SYS_MEM_DIV		8
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
#define CONFIG_DDR_CS1			0	/* 1-connected, 0-disconnected */
#define CONFIG_DDR_DW32			1	/* 1-32bit-width, 0-16bit-width */
#define CONFIG_MDDR_W94AD6KB_HX51

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
#define BOOTARGS_COMMON "console=ttyS3,57600n8 mem=128M@0x0"
#if defined(CONFIG_SPL_NOR_SUPPORT) || defined(CONFIG_SPL_SPI_SUPPORT)
	#if defined(CONFIG_SPL_SPI_SUPPORT)
	#define	 CONFIG_BOOTARGS BOOTARGS_COMMON "mem=128M@0x0 ip=off root=/dev/ram0 rw rdinit=/linuxrc"
	#define CONFIG_BOOTCOMMAND "spinor read 0x40000 0x7B0000 0x80600000;bootm 0x80600000"
	#else
	#define	 CONFIG_BOOTARGS BOOTARGS_COMMON " ip=192.168.10.205:192.168.10.1:192.168.10.1:255.255.255.0 nfsroot=192.168.4.3:/home/rootdir rw"
	#define CONFIG_BOOTCOMMAND "tftpboot xxx/uImage; bootm"
	#endif
#elif defined(CONFIG_SPL_MMC_SUPPORT)
	#ifdef CONFIG_JZ_MMC_MSC0
		#define CONFIG_BOOTARGS BOOTARGS_COMMON " root=/dev/mmcblk0p1"
		#define CONFIG_BOOTCOMMAND "mmc dev 0;mmc read 0x80f00000 0x1800 0x3000; bootm 0x80f00000"
	#else
		#define CONFIG_BOOTARGS BOOTARGS_COMMON " root=/dev/mmcblk1p1"
		#define CONFIG_BOOTCOMMAND "mmc dev 1;mmc read 0x80f00000 0x1800 0x3000; bootm 0x80f00000"
	#endif
#elif defined(CONFIG_JZ_NAND_MGR)
	#define  CONFIG_BOOTARGS BOOTARGS_COMMON " root=/dev/ram0 rw rdinit=/linuxrc"
	#define CONFIG_BOOTCOMMAND  "nand_zm read ndboot 0 0x400000 0x80600000;bootm"
#endif

#define PARTITION_NUM 10

/**
 * Boot command definitions.
 */
#define CONFIG_BOOTDELAY 1


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
#define CONFIG_CMD_MEMORY	/* md mm nm mw cp cmp crc base loop mtest */
#define CONFIG_CMD_MISC		/* Misc functions like sleep etc*/
#define CONFIG_CMD_RUN		/* run command in env variable	*/
#define CONFIG_CMD_SAVEENV	/* saveenv			*/
#define CONFIG_CMD_SETGETDCR	/* DCR support on 4xx		*/
#define CONFIG_CMD_SOURCE	/* "source" command support	*/
#define CONFIG_CMD_GETTIME
#define CONFIG_CMD_UNZIP        /* unzip from memory to memory  */
#ifdef CONFIG_SPL_MMC_SUPPORT
#define CONFIG_CMD_MMC		/* MMC/SD support */
#endif


/* MMC */
#ifdef CONFIG_CMD_MMC
#define CONFIG_GENERIC_MMC		1
#define CONFIG_MMC			1
#define CONFIG_JZ_MMC			1
#ifdef CONFIG_JZ_MMC_MSC0
#define CONFIG_JZ_MMC_SPLMSC 0
#define CONFIG_JZ_MMC_MSC0_PA_4BIT 1
#endif
#ifdef CONFIG_JZ_MMC_MSC1
#define CONFIG_JZ_MMC_SPLMSC 1
#define CONFIG_JZ_MMC_MSC1_PE 1
#endif
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

#if defined(CONFIG_SPL_MMC_SUPPORT)
	#if defined(CONFIG_SUPPORT_EMMC_BOOT)
	#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD "-emmc# "
	#elif	defined(CONFIG_JZ_MMC_MSC0)
	#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD "-msc0# "
	#else
	#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD "-msc1# "
	#endif
#elif defined(CONFIG_JZ_NAND_MGR)
#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD "-nand# "
#elif defined(CONFIG_SPL_NOR_SUPPORT)
#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD "-nor# "
#elif defined(CONFIG_SPL_SPI_SUPPORT)
#define CONFIG_SYS_PROMPT CONFIG_SYS_BOARD "-spinor# "
#endif

#define CONFIG_SYS_CBSIZE 1024 /* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)

#if defined(CONFIG_SUPPORT_EMMC_BOOT)
#define CONFIG_SYS_MONITOR_LEN		(384 * 1024)
#else
#define CONFIG_SYS_MONITOR_LEN		(512 << 10)
#endif

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
#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_ENV_SIZE			(32 << 10)
#define CONFIG_ENV_OFFSET		(CONFIG_SYS_MONITOR_LEN + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)
#elif   defined(CONFIG_JZ_NAND_MGR) /* endif CONFIG_ENV_IS_IN_MMC */
#define CONFIG_ENV_IS_IN_NAND_ZM
#define CONFIG_ENV_OFFSET		0
#define CONFIG_ENV_SIZE			(32 << 10)
#define CMDLINE_PARTITION		"ndcmdline"
#define CONFIG_SPL_NAND_BASE
#define CONFIG_SPL_NAND_DRIVERS
#define CONFIG_SPL_NAND_SIMPLE
#define CONFIG_SPL_NAND_LOAD
#define CONFIG_NAND_LOADER
#define CFG_NAND_BW8			1
#define CONFIG_NAND_CS			1
#else
#define CONFIG_ENV_IS_NOWHERE
#define CONFIG_ENV_SIZE			(32 << 10)
#define CONFIG_ENV_OFFSET		(CONFIG_SYS_NAND_BLOCK_SIZE * 5)
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
#ifdef CONFIG_SPL_NOR_SUPPORT
#define CONFIG_SPL_LDSCRIPT             "$(CPUDIR)/$(SOC)/u-boot-nor-spl.lds"
#else
#define CONFIG_SPL_LDSCRIPT		"$(CPUDIR)/$(SOC)/u-boot-spl.lds"
#endif
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x20 /* 16KB offset */
#define CONFIG_SYS_U_BOOT_MAX_SIZE_SECTORS	0x400 /* 512 KB */
#define CONFIG_SYS_NAND_U_BOOT_OFFS	(CONFIG_SYS_NAND_BLOCK_SIZE * 2)
#define CONFIG_SYS_NAND_U_BOOT_DST	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_NAND_U_BOOT_START	CONFIG_SYS_NAND_U_BOOT_DST
#define CONFIG_SYS_NAND_U_BOOT_SIZE	(512 * 1024)
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_GPIO_SUPPORT
#define CONFIG_SPL_BOARD_INIT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#if defined(CONFIG_SPL_NOR_SUPPORT)
#define CONFIG_SPL_TEXT_BASE		0xba000000
#define CONFIG_SYS_UBOOT_BASE		(CONFIG_SPL_TEXT_BASE + CONFIG_SPL_PAD_TO - 0x40)
					/* 0x40 = sizeof (image_header)*/
#define CONFIG_SYS_OS_BASE		0
#define CONFIG_SYS_SPL_ARGS_ADDR	0
#define CONFIG_SYS_FDT_BASE		0
#define CONFIG_SPL_PAD_TO		32768
#define CONFIG_SPL_MAX_SIZE		(32 * 1024)
#elif defined(CONFIG_SPL_MMC_SUPPORT)
#define CONFIG_SPL_PAD_TO		15872  /* u-boot start addr - mbr size(512) */
#define CONFIG_SPL_TEXT_BASE		0xf4000a00
#define CONFIG_SPL_MAX_SIZE		((16 * 1024) - 0xa00)
#elif defined(CONFIG_JZ_NAND_MGR)
#define CONFIG_CMD_ZM_NAND  /* nand zone manager support */
#define CONFIG_SPL_PAD_TO		16384
#define CONFIG_SPL_TEXT_BASE		0xf4000800
#define CONFIG_SPL_MAX_SIZE		((16 * 1024) - 0x800)
#elif defined(CONFIG_SPL_SPI_SUPPORT)
#define CONFIG_SPI_SPL_CHECK
#define CONFIG_SYS_SPI_BOOT_FREQ	1000000
#define CONFIG_SPL_TEXT_BASE		0xf4000800
#define CONFIG_SPL_MAX_SIZE		((16 * 1024) - 0x800)
#define CONFIG_SPL_PAD_TO		16384
#define CONFIG_CMD_SPI
#endif

#ifdef CONFIG_CMD_SPI
#define CONFIG_SSI_BASE SSI0_BASE
#define CONFIG_SPI_BUILD
#ifdef CONFIG_SOFT_SPI
#undef SPI_INIT
#define SPI_DELAY
#define	SPI_SDA(val)    gpio_direction_output(GPIO_PA(21), val)
#define	SPI_SCL(val)	gpio_direction_output(GPIO_PA(18), val)
#define	SPI_READ	gpio_get_value(GPIO_PA(20))
#else
#define CONFIG_JZ_SPI
#endif
/*#define CONFIG_JZ_SPI_FLASH*/
#define CONFIG_CMD_SF
#define CONFIG_SPI_FLASH_INGENIC
#define CONFIG_SPI_FLASH
#define CONFIG_UBOOT_OFFSET             (4<<12)
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
#else
#define CONFIG_GPT_TABLE_PATH	"$(TOPDIR)/board/$(BOARDDIR)"
#endif

#endif /* __CONFIG_F4775_H__ */

