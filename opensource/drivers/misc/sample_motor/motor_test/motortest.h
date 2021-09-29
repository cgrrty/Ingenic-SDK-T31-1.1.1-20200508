#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#ifndef __motortest_H__
#define __motortest_H__

#define MOTOR_MOVE_STOP		0x0
#define MOTOR_MOVE_RUN		0x1

/* directional_attr */
#define MOTOR_DIRECTIONAL_UP	0x0
#define MOTOR_DIRECTIONAL_DOWN	0x1
#define MOTOR_DIRECTIONAL_LEFT	0x2
#define MOTOR_DIRECTIONAL_RIGHT	0x3

#define MOTOR1_MAX_SPEED	1000
#define MOTOR1_MIN_SPEED	10

/* ioctl cmd */
#define MOTOR_STOP		0x1
#define MOTOR_RESET		0x2
#define MOTOR_MOVE		0x3
#define MOTOR_GET_STATUS	0x4
#define MOTOR_SPEED		0x5
#define MOTOR_GOBACK	0x6
#define MOTOR_CRUISE	0x7

enum motor_status {
	MOTOR_IS_STOP,
	MOTOR_IS_RUNNING,
};

struct motor_message {
	int x;
	int y;
	enum motor_status status;
	int speed;
};

struct motors_steps{
	int x;
	int y;
};

struct motor_move_st {
	int motor_directional;
	int motor_move_steps;
	int motor_move_speed;
};
struct motor_status_st {
	int directional_attr;
	int total_steps;
	int current_steps;
	int min_speed;
	int cur_speed;
	int max_speed;
	int move_is_min;
	int move_is_max;
};

struct motor_reset_data {
	unsigned int x_max_steps;
	unsigned int y_max_steps;
	unsigned int x_cur_step;
	unsigned int y_cur_step;
};

struct motors_steps jb_motors_steps;
struct motor_message jb_motor_message;
struct motor_status_st motor_status;
struct motor_move_st motor_action;
struct motor_reset_data motor_reset_data;

#endif
