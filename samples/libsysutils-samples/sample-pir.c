/*
 * sample-pir.c
 * PIR sample fo k1mbv02
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 */

#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>

#include <sysutils/su_misc.h>

#define PIR_KEYCODE KEY_F13

static void* pir_event_listener(void *p)
{
	SUKeyEvent event;
	int ret, key_code;
	int evfd = *(int *)p;

	printf("Start read key event.\n");

	while (1) {
		ret = SU_Key_ReadEvent(evfd, &key_code, &event);
		if (ret != 0) {
			printf("Get Key event error");
			return NULL;
		}

		printf("Keycode: %d, event: %s\n", key_code,
			   event == KEY_PRESSED ? "Pressed" : "Released");
	}

	return NULL;
}

static int sys_write(const char *path, const char *cmd)
{
	int fd = open(path, O_WRONLY);
	if (fd < 0) {
		printf("open %s error: %s", path, strerror(errno));
		return -1;
	}

	int ret = write(fd, cmd, strlen(cmd));
	if (!(ret == strlen(cmd))) {
		printf("write %s error: %s", path, strerror(errno));
		return -1;
	}

	close(fd);
	return 0;
}

static int pir_enable(void)
{
	/* Enable PIR power */
	if (sys_write("/proc/board/power/pir", "1") < 0)
		goto err;

	if (SU_Key_EnableEvent(PIR_KEYCODE) < 0)
		goto err;

	printf("%s\n", __func__);
	return 0;
err:
	printf("%s error\n", __func__);
	return -1;
}

static int pir_disable(void)
{
	if (SU_Key_DisableEvent(PIR_KEYCODE) < 0)
		goto err;

	/* Disable PIR power */
	if (sys_write("/proc/board/power/pir", "0") < 0)
		goto err;

	printf("%s\n", __func__);
	return 0;
err:
	printf("%s error\n", __func__);
	return -1;
}

static void show_help(void)
{
	printf("Help:\n");
	printf("\tq exit\n");
	printf("\t0 disable\n");
	printf("\t1 enable\n");
}

int main()
{
	int ret, evfd;
	pthread_t pir_tid;

	show_help();

	pir_disable();

	evfd = SU_Key_OpenEvent();
	if (evfd < 0) {
		printf("Key event open error\n");
		return -1;
	}

	ret = pthread_create(&pir_tid, NULL, pir_event_listener, &evfd);
	if (ret < 0) {
		perror("pir_event_listener thread create error\n");
		return ret;
	}

	while (1) {
		char r = getchar();

		if (r == 'q') {
			break;
		} else if (r == '0') {
			if (pir_disable() < 0)
				break;
		} else if (r == '1') {
			if (pir_enable() < 0)
				break;
		} else if (r == '\r' || r == '\n') {
			continue;
		} else {
			show_help();
		}
	}

	pthread_cancel(pir_tid);
	pthread_join(pir_tid, NULL);
	SU_Key_CloseEvent(evfd);

	return 0;
}
