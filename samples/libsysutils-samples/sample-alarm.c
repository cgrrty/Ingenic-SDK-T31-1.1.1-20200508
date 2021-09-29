/*
 * sample-alarm.c
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 */

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sysutils/su_base.h>

#define ALARM_TIME_SEC				10

static sem_t alarm_sem;

static void* alarm_listener(void *p)
{
	printf("Wait Alarm event\n");

	int ret = SU_Base_PollingAlarm(ALARM_TIME_SEC*1000 + 5000); /* Add 5 second timeout. */
	if (ret < 0) {
		printf("SU_Base_PollAlarm() error\n");
	}

	printf("Alarm timeout...\n");
	sem_post(&alarm_sem);

	return NULL;
}

int main()
{
	int ret;

	sem_init(&alarm_sem, 0 ,0);

	SUTime now_tm;
	ret = SU_Base_GetTime(&now_tm);
	if (ret < 0) {
		printf("SU_Base_GetTime() error\n");
		return ret;
	}

	printf("Now: %d.%d.%d %02d:%02d:%02d\n",
		   now_tm.year, now_tm.mon, now_tm.mday,
		   now_tm.hour , now_tm.min, now_tm.sec);

	SUTime alarm_tm = now_tm;
	alarm_tm.sec += ALARM_TIME_SEC;
	if (alarm_tm.sec >= 60) {
		alarm_tm.sec %= 60;
		alarm_tm.min++;
	}
	if (alarm_tm.min == 60) {
		alarm_tm.min = 0;
		alarm_tm.hour++;
	}
	if (alarm_tm.hour == 24)
		alarm_tm.hour = 0;

	ret = SU_Base_SetAlarm(&alarm_tm);
	if (ret < 0) {
		printf("SU_Base_SetAlarm() error\n");
		return ret;
	}

	SUTime alarm_tm_check;
	ret = SU_Base_GetAlarm(&alarm_tm_check);
	if (ret < 0) {
		printf("SU_Base_GetAlarm() error\n");
		return ret;
	}

	printf("ALarm Time: %d.%d.%d %02d:%02d:%02d\n",
		   alarm_tm_check.year, alarm_tm_check.mon, alarm_tm_check.mday,
		   alarm_tm_check.hour , alarm_tm_check.min, alarm_tm_check.sec);

	ret = SU_Base_EnableAlarm();
	if (ret < 0) {
		printf("SU_Base_EnableAlarm() error\n");
		return ret;
	}

	pthread_t alarm_tid;
	ret = pthread_create(&alarm_tid, NULL, alarm_listener, NULL);
	if (ret < 0) {
		SU_Base_DisableAlarm();
		perror("alarm_listener thread create error\n");
		return ret;
	}

	sem_wait(&alarm_sem);

	ret = SU_Base_GetTime(&now_tm);
	if (ret < 0) {
		printf("SU_Base_GetTime() error\n");
		return ret;
	}

	printf("Now: %d.%d.%d %02d:%02d:%02d\n",
		   now_tm.year, now_tm.mon, now_tm.mday,
		   now_tm.hour , now_tm.min, now_tm.sec);

	ret = SU_Base_DisableAlarm();
	if (ret < 0) {
		printf("SU_Base_DisableAlarm() error\n");
		return -1;
	}

	pthread_cancel(alarm_tid);
	pthread_join(alarm_tid, NULL);

	return 0;
}
