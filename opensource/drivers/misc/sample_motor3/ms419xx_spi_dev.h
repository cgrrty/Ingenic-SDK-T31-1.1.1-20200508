#ifndef __JZ_SPI_DEV_H__
#define __JZ_SPI_DEV_H__

#include <linux/wait.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <mach/jzssi.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>


int jz_spidev_read(int addr, char addr_size, int *value, char value_size);
int jz_spidev_write(int addr, char addr_size, int value, char value_size);

int __init jz_spidev_init(void);
void __exit jz_spidev_exit(void);

#endif /*__JZ_SPI_DEV_H__*/
