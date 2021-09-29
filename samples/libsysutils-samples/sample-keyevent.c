/*
 * sample-keyevent.c
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 */

#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sysutils/su_misc.h>

static void* key_event_listener(void *p)
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

int main()
{
	int ret, evfd;
	pthread_t key_tid;

	evfd = SU_Key_OpenEvent();
	if (evfd < 0) {
		printf("Key event open error\n");
		return -1;
	}

	ret = pthread_create(&key_tid, NULL, key_event_listener, &evfd);
	if (ret < 0) {
		perror("key_event_listener thread create error\n");
		return ret;
	}

	printf("Press Enter to exit.\n");
	getchar();

	pthread_cancel(key_tid);
	pthread_join(key_tid, NULL);
	SU_Key_CloseEvent(evfd);

	return 0;
}
