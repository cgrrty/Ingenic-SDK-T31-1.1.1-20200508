/*
 * sample-wdt.c
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/watchdog.h>

int fd;

static int wdt_keep_alive(void)
{
	int ret = -1;
    int dummy;
    ret = ioctl(fd, WDIOC_KEEPALIVE, &dummy);
	if (0 != ret) {
		printf("err(%s,%d): %s\n", __func__, __LINE__, strerror(errno));
		return -1;
	}
	return 0;
}

static int wdt_enable()
{
	int ret = -1;
	int flags = 0;
	flags = WDIOS_ENABLECARD;
	ret = ioctl(fd, WDIOC_SETOPTIONS, &flags);
	if (0 != ret) {
		printf("err(%s,%d): %s\n", __func__, __LINE__, strerror(errno));
		return -1;
	}
	return 0;

}

static int wdt_disable()
{
	int ret = -1;
	int flags = 0;
	flags = WDIOS_DISABLECARD;
	ret = ioctl(fd, WDIOC_SETOPTIONS, &flags);
	if (0 != ret) {
		printf("err(%s,%d): %s\n", __func__, __LINE__, strerror(errno));
		return -1;
	}
	return 0;

}

static int wdt_set_timeout(int to)
{
	int ret = -1;
    int timeout = to;
    ret = ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
	if (0 != ret) {
		printf("err(%s,%d): %s\n", __func__, __LINE__, strerror(errno));
		return -1;
	}
	return 0;

}

static int wdt_get_timeout()
{
	int ret = -1;
    int timeout = 0;
    ret = ioctl(fd, WDIOC_GETTIMEOUT, &timeout);
	if (0 != ret) {
		printf("err(%s,%d): %s\n", __func__, __LINE__, strerror(errno));
		return -1;
	}
	return timeout;

}

int main(int argc, char *argv[])
{
	int flags;
	fd = open("/dev/watchdog", O_WRONLY);
	if (-1 == fd) {
		printf("err(%s,%d): %s\n", __func__, __LINE__, strerror(errno));
		exit(-1);
	}
	wdt_enable();
	wdt_set_timeout(60);
	printf("info: timeout = %d\n", wdt_get_timeout());
	while (1) {
		sleep(50);
		wdt_keep_alive();
	}
	wdt_disable();
    close(fd);
    return 0;
}

