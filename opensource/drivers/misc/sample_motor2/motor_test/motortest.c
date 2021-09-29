#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "motortest.h"

int main(int argc, const char* argv[])
{
	int a,b,c,d,e,f;
	int fd = open("/dev/motor", 0);
	while(1)
	{
		printf("action：\n");
		printf("1:MOTOR_STOP 2:MOTOR_RESET 3:MOTOR_MOVE 4:MOTOR_GET_STATUS 5:MOTOR_SPEED 6:MOTOR_GOBACK 7:MOTOR_CRUISE\n");
		printf("choose:");
		scanf("%d",&d);
		switch(d)
		{
			case MOTOR_STOP:
				ioctl(fd, d,0);
				break;
			case MOTOR_RESET:
				memset(&motor_reset_data, 0, sizeof(motor_reset_data));
				ioctl(fd, d,&motor_reset_data);
				printf("x_max_steps=%d,y_max_steps=%d,x_cur_step=%d,y_cur_step=%d\n",
						motor_reset_data.x_max_steps,motor_reset_data.y_max_steps,
						motor_reset_data.x_cur_step,motor_reset_data.y_cur_step);
				break;
			case MOTOR_MOVE:
				printf("steps x：");
				scanf("%d",&a);
				printf("steps y：");
				scanf("%d",&b);
				printf("x=%d,y=%d\n",a,b);
				jb_motors_steps.x = a;
				jb_motors_steps.y = b;
				ioctl(fd, d, (unsigned long)&jb_motors_steps);
				break;
			case MOTOR_GET_STATUS:
				ioctl(fd, d, (unsigned long)&jb_motor_message);
				printf("xcurrent_steps=%d\n",jb_motor_message.x);
				printf("ycurrent_steps=%d\n",jb_motor_message.y);
				printf("cur_status=%d\n",jb_motor_message.status);
				printf("cur_speed=%d\n",jb_motor_message.speed);
				break;
			case MOTOR_SPEED:
				printf("speed：");
				scanf("%d",&e);
				ioctl(fd, d, (unsigned long)&e);
				break;
			case MOTOR_GOBACK:
				ioctl(fd, d,0);
				break;
			case MOTOR_CRUISE:
				ioctl(fd, d,0);
				break;

		}
		//motor_action.motor_directional = MOTOR_DIRECTIONAL_UP;
		//motor_action.motor_move_steps = 1000;
		//motor_action.motor_move_speed = MOTOR1_MAX_SPEED;

		//close(fd);
	}
	return 0;
}
