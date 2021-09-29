/*
 * sample-led.c
 * LED sample fo k1mbv02
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sysutils/su_misc.h>

static void show_help(void)
{
	printf("Help:\n");
	printf("\tq exit\n");
	printf("\te[n] enable led n. eg: e0 -- enable LED0\n");
	printf("\td[n] disable led n. eg: d0 -- disable LED0\n");
}

int main()
{
	char cmd[4];
	int on;

	show_help();
	while (1) {
		fgets(cmd, 4, stdin);

		if (cmd[0] == 'q')
			return 0;
		else if (cmd[0] == 'e')
			on = 1;
		else if (cmd[0] == 'd')
			on = 0;
		else
			show_help();
		if (cmd[1] >= '0' && cmd[1] <= '9') {
			int led_num  = atoi(&cmd[1]);
			if (SU_LED_Command(led_num, on) < 0) {
				printf("LED switch error\n");
				return -1;
			}
		} else {
			show_help();
		}
	}

	return 0;
}
