
#ifndef __INCLUDE__GPIO__I2C__
#define __INCLUDE__GPIO__I2C__

struct i2c {
	unsigned int scl;
	unsigned int sda;
};

void i2c_init(struct i2c *i2c);
int  i2c_write(struct i2c *i2c,unsigned char chip,
		unsigned int addr, int alen, unsigned char *buffer, int len);
int  i2c_read(struct i2c *i2c,unsigned char chip,
		unsigned int addr, int alen, unsigned char *buffer, int len);

int i2c_probe(struct i2c *i2c, unsigned char addr);
#endif
