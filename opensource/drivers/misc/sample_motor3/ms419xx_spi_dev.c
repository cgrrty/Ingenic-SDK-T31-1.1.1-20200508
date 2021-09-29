/*
 * X1000 SoC SPI_NAND driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.

 *  You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include "ms419xx_spi_dev.h"

static struct spi_device *g_spi = NULL;

int jz_spidev_read(int addr, char addr_size, int *value, char value_size)
{
	struct spi_message message;
	struct spi_transfer transfer[2];
	unsigned char rbuf[4] = {0,0,0,0};
	unsigned char wbuf[4] = {0,0,0,0};
	int ret = 0;

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	wbuf[0] = (addr | 0x40) & 0xff;

	transfer[0].tx_buf = wbuf;
	transfer[0].len = addr_size;
	spi_message_add_tail(&transfer[0], &message);

	transfer[1].rx_buf = rbuf;
	transfer[1].len = value_size;
	transfer[1].cs_change = 1;
	spi_message_add_tail(&transfer[1], &message);

	ret = spi_sync(g_spi, &message);

	if(ret) {
		printk("%s -- %s --%d  spi_sync() error !\n",__FILE__,__func__,__LINE__);
		return ret;
	}
	*value = *(unsigned int*)rbuf;
	return ret;
}

int jz_spidev_write(int addr, char addr_size, int value, char value_size)
{
	int ret = 0;
	struct spi_message message;
	struct spi_transfer transfer[1];
	char wbuf[8];

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	wbuf[0] = addr & 0xff ;

	wbuf[1] = value & 0xff;
	wbuf[2] = (value >> 8) & 0xff;

	transfer[0].tx_buf = wbuf;
	transfer[0].len = addr_size + value_size;
	transfer[0].cs_change = 1;
	spi_message_add_tail(&transfer[0], &message);

	ret = spi_sync(g_spi, &message);
	if(ret) {
		printk("spi_sync error ! %s %s %d\n",__FILE__,__func__,__LINE__);
		ret=-EIO;
	}

	return ret;
}


static int jz_spidev_probe(struct spi_device *spi)
{
	spi->mode = SPI_MODE_3 | SPI_CS_HIGH | SPI_LSB_FIRST | SPI_CPOL | SPI_CPHA;

	g_spi = spi;

	return 0;
}

static int jz_spidev_remove(struct spi_device *spi)
{
	return 0;
}

struct spi_device_id jz_id_table[] = {
	{
		.name = "spidev",
	},
};

static struct spi_driver jz_spidev_driver = {
	.driver = {
		.name   = "spidev",
		.owner  = THIS_MODULE,
	},
	.id_table   = jz_id_table,
	.probe      = jz_spidev_probe,
	.remove     = jz_spidev_remove,
	.shutdown   = NULL,
};

int __init jz_spidev_init(void)
{
	return spi_register_driver(&jz_spidev_driver);
}

void __exit jz_spidev_exit(void)
{
	spi_unregister_driver(&jz_spidev_driver);
}
