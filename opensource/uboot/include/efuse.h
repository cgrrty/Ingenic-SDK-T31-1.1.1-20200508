#ifndef __EFUSE_H__
#define __EFUSE_H__

#ifdef CONFIG_CMD_EFUSE
int efuse_write(void *buf, int length, off_t offset);
int efuse_read(void *buf, int length, off_t offset);
int efuse_init(int gpio_pin);
void efuse_deinit(void);
void efuse_debug_enable(int enable);
#else
static int inline efuse_write(void *buf, int length, off_t offset) {return 0;}
static int inline efuse_read(void *buf, int length, off_t offset) {return 0;}
static int inline efuse_init(int gpio_pin) {return 0;}
static void inline efuse_deinit(void)	{;}
static void inline efuse_debug_enable(int enable) {;}
#endif /*CONFIG_CMD_EFUSE_N*/
#endif	/*EFUSE_H*/
