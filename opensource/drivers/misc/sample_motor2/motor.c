/*
 * motor.c - Ingenic motor driver
 *
 * Copyright (C) 2015 Ingenic Semiconductor Co.,Ltd
 *       http://www.ingenic.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/mfd/core.h>
#include <linux/mempolicy.h>
#include <linux/interrupt.h>
#include <linux/mfd/jz_tcu.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>

#include <soc/irq.h>
#include <soc/base.h>
#include <soc/extal.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <soc/gpio.h>
#include <mach/platform.h>
#include "motor.h"

#define JZ_MOTOR_DRIVER_VERSION "H20171206a"

extern int jzgpio_ctrl_pull(enum gpio_port port, int enable_pull,unsigned long pins);

struct motor_platform_data motors_pdata[HAS_MOTOR_CNT] = {
	{
		.name = "Horizontal motor",
		.motor_min_gpio		= HORIZONTAL_MIN_GPIO,
		.motor_max_gpio 	= HORIZONTAL_MAX_GPIO,
		.motor_gpio_level	= HORIZONTAL_GPIO_LEVEL,
		.motor_st1_gpio		= HORIZONTAL_ST1_GPIO,
		.motor_st2_gpio		= HORIZONTAL_ST2_GPIO,
		.motor_st3_gpio		= HORIZONTAL_ST3_GPIO,
		.motor_st4_gpio		= HORIZONTAL_ST4_GPIO,
	},
	{
		.name = "Vertical motor",
		.motor_min_gpio		= VERTICAL_MIN_GPIO,
		.motor_max_gpio 	= VERTICAL_MAX_GPIO,
		.motor_gpio_level	= VERTICAL_GPIO_LEVEL,
		.motor_st1_gpio		= VERTICAL_ST1_GPIO,
		.motor_st2_gpio		= VERTICAL_ST2_GPIO,
		.motor_st3_gpio		= VERTICAL_ST3_GPIO,
		.motor_st4_gpio		= VERTICAL_ST4_GPIO,
	},
};

static void motor_set_default(struct motor_device *mdev)
{
	int index = 0;
	struct motor_driver *motor = NULL;
	mdev->dev_state = MOTOR_OPS_STOP;
	for(index = 0; index < HAS_MOTOR_CNT; index++){
		motor =  &mdev->motors[index];
		motor->state = MOTOR_OPS_STOP;
		if (motor->pdata->motor_st1_gpio)
			gpio_direction_output(motor->pdata->motor_st1_gpio, 0);
		if (motor->pdata->motor_st2_gpio)
			gpio_direction_output(motor->pdata->motor_st2_gpio, 0);
		if (motor->pdata->motor_st3_gpio)
			gpio_direction_output(motor->pdata->motor_st3_gpio, 0);
		if (motor->pdata->motor_st4_gpio)
			gpio_direction_output(motor->pdata->motor_st4_gpio, 0);
	}
	return;
}

static unsigned char step_8[8] = {
	0x08,
	0x0c,
	0x04,
	0x06,
	0x02,
	0x03,
	0x01,
	0x09
};

static void motor_move_step(struct motor_device *mdev)
{
	struct motor_driver *motor = NULL;
	int index = 0;
	int step = 0;

	for(index = 0; index < HAS_MOTOR_CNT; index++){
		motor =  &mdev->motors[index];
		if(motor->state != MOTOR_OPS_STOP){
			step = motor->cur_steps % 8;
			step = step < 0 ? step + 8 : step;
			if (motor->pdata->motor_st1_gpio)
				gpio_direction_output(motor->pdata->motor_st1_gpio, step_8[step] & 0x8);
			if (motor->pdata->motor_st2_gpio)
				gpio_direction_output(motor->pdata->motor_st2_gpio, step_8[step] & 0x4);
			if (motor->pdata->motor_st3_gpio)
				gpio_direction_output(motor->pdata->motor_st3_gpio, step_8[step] & 0x2);
			if (motor->pdata->motor_st4_gpio)
				gpio_direction_output(motor->pdata->motor_st4_gpio, step_8[step] & 0x1);
		}else{
			if (motor->pdata->motor_st1_gpio)
				gpio_direction_output(motor->pdata->motor_st1_gpio, 0);
			if (motor->pdata->motor_st2_gpio)
				gpio_direction_output(motor->pdata->motor_st2_gpio, 0);
			if (motor->pdata->motor_st3_gpio)
				gpio_direction_output(motor->pdata->motor_st3_gpio, 0);
			if (motor->pdata->motor_st4_gpio)
				gpio_direction_output(motor->pdata->motor_st4_gpio, 0);
		}
		if(motor->state == MOTOR_OPS_RESET){
			motor->total_steps++;
		}
	}

	return;
}

static irqreturn_t jz_timer_interrupt(int irq, void *dev_id)
{
	struct motor_device *mdev = dev_id;
	struct motor_move *dst = &mdev->dst_move;
	struct motor_move *cur = &mdev->cur_move;
	struct motor_driver *motors = mdev->motors;
	int flag = 0;

	if(motors[HORIZONTAL_MOTOR].state == MOTOR_OPS_STOP
			&& motors[VERTICAL_MOTOR].state == MOTOR_OPS_STOP){
		mdev->dev_state = MOTOR_OPS_STOP;
		motor_move_step(mdev);
		return IRQ_HANDLED;
	}

	if(mdev->dev_state == MOTOR_OPS_CRUISE){
		motors[HORIZONTAL_MOTOR].cur_steps += motors[HORIZONTAL_MOTOR].move_dir;
		motors[VERTICAL_MOTOR].cur_steps += motors[VERTICAL_MOTOR].move_dir;
		motor_move_step(mdev);
	}else{
		while(cur->times < dst->times){
			if(cur->one.x < dst->one.x && motors[HORIZONTAL_MOTOR].state != MOTOR_OPS_STOP){
				motors[HORIZONTAL_MOTOR].cur_steps += motors[HORIZONTAL_MOTOR].move_dir;
				cur->one.x++;
				flag = 1;
			}
			if(cur->one.y < dst->one.y && motors[VERTICAL_MOTOR].state != MOTOR_OPS_STOP){
				motors[VERTICAL_MOTOR].cur_steps += motors[VERTICAL_MOTOR].move_dir;
				cur->one.y++;
				flag = 1;
			}

			if(flag){
				flag = 0;
				motor_move_step(mdev);
				break;
			}else{
				cur->one.x = 0;
				cur->one.y = 0;
				cur->times++;
			}
		}

		if(mdev->cur_move.times == mdev->dst_move.times &&
				mdev->cur_move.one.x == 0 && mdev->cur_move.one.y == 0){
			motors[HORIZONTAL_MOTOR].state = MOTOR_OPS_STOP;
			motors[VERTICAL_MOTOR].state = MOTOR_OPS_STOP;
		}
	}
	return IRQ_HANDLED;
}

static void gpio_keys_min_timer(unsigned long _data)
{
	struct motor_driver *motor = (struct motor_driver *)_data;
	int value = 0;
    value = gpio_get_value(motor->pdata->motor_min_gpio);

	if(value == motor->pdata->motor_gpio_level){
	//	printk("%s min %d\n",motor->pdata->name,__LINE__);
		if(motor->state == MOTOR_OPS_RESET){
			motor->reset_min_pos = 1;
			if(motor->reset_max_pos && motor->reset_min_pos){
				motor->max_steps = motor->total_steps;
				motor->state = MOTOR_OPS_STOP;
				motor->cur_steps = 0;
				complete(&motor->reset_completion);
			}else{
				motor->total_steps = 0;
			}
			motor->move_dir = MOTOR_MOVE_RIGHT_UP;
		}else if(motor->state == MOTOR_OPS_NORMAL){
			if(motor->move_dir == MOTOR_MOVE_LEFT_DOWN){
				motor->state = MOTOR_OPS_STOP;
			}
		}else
			motor->move_dir = MOTOR_MOVE_RIGHT_UP;


		motor->cur_steps = 0;
		motor->min_pos_irq_cnt++;
		//printk("%s min; cur_steps = %d max_steps = %d\n", motor->pdata->name,motor->cur_steps, motor->max_steps);
	}
}

static irqreturn_t motor_min_gpio_interrupt(int irq, void *dev_id)
{
	struct motor_driver *motor = dev_id;

	mod_timer(&motor->min_timer,
			jiffies + msecs_to_jiffies(5));

	return IRQ_HANDLED;
}

static void gpio_keys_max_timer(unsigned long _data)
{
	struct motor_driver *motor = (struct motor_driver *)_data;
	int value = 0;
     value = gpio_get_value(motor->pdata->motor_max_gpio);

	if(value == motor->pdata->motor_gpio_level){

		if(motor->state == MOTOR_OPS_RESET){
			motor->reset_max_pos = 1;
			if(motor->reset_max_pos && motor->reset_min_pos){
				motor->max_steps = motor->total_steps;
				motor->state = MOTOR_OPS_STOP;
				motor->cur_steps = motor->max_steps;
				complete(&motor->reset_completion);
			}else{
				motor->total_steps = 0;
			}
			motor->move_dir = MOTOR_MOVE_LEFT_DOWN;
		}else if(motor->state == MOTOR_OPS_NORMAL){
			if(motor->move_dir == MOTOR_MOVE_RIGHT_UP){
				motor->state = MOTOR_OPS_STOP;
			}
		}else
			motor->move_dir = MOTOR_MOVE_LEFT_DOWN;
		motor->cur_steps = motor->max_steps;
		motor->max_pos_irq_cnt++;
		//printk("%s max; cur_steps = %d max_steps = %d\n", motor->pdata->name,motor->cur_steps, motor->max_steps);
	}
}


static irqreturn_t motor_max_gpio_interrupt(int irq, void *dev_id)
{
	struct motor_driver *motor = dev_id;
	mod_timer(&motor->max_timer,
			jiffies + msecs_to_jiffies(5));
   	return IRQ_HANDLED;
}


static inline int calc_max_divisor(int a, int b)
{
	int r = 0;
	while(b !=0 ){
		r = a % b;
		a = b;
		b = r;
	}
	return a;
}

static long motor_ops_move(struct motor_device *mdev, int x, int y)
{
	struct motor_driver *motors = mdev->motors;
	unsigned long flags;
	int x_dir = MOTOR_MOVE_STOP;
	int y_dir = MOTOR_MOVE_STOP;
	int x1 = 0;
	int y1 = 0;
	int times = 1;
	int value = 0;

	/* check x value */
	if(x > 0){
		value = gpio_get_value(mdev->motors[HORIZONTAL_MOTOR].pdata->motor_max_gpio);
		if(value == mdev->motors[HORIZONTAL_MOTOR].pdata->motor_gpio_level)
			x = 0;
	}else{
		value = gpio_get_value(mdev->motors[HORIZONTAL_MOTOR].pdata->motor_min_gpio);
		if(value == mdev->motors[HORIZONTAL_MOTOR].pdata->motor_gpio_level)
			x = 0;
	}
	/* check y value */
	if(y > 0){
		value = gpio_get_value(mdev->motors[VERTICAL_MOTOR].pdata->motor_max_gpio);
		if(value == mdev->motors[VERTICAL_MOTOR].pdata->motor_gpio_level)
			y = 0;
	}else{
		value = gpio_get_value(mdev->motors[VERTICAL_MOTOR].pdata->motor_min_gpio);
		if(value == mdev->motors[VERTICAL_MOTOR].pdata->motor_gpio_level)
			y = 0;
	}

	x_dir = x > 0 ? MOTOR_MOVE_RIGHT_UP : (x < 0 ? MOTOR_MOVE_LEFT_DOWN: MOTOR_MOVE_STOP);
	y_dir = y > 0 ? MOTOR_MOVE_RIGHT_UP : (y < 0 ? MOTOR_MOVE_LEFT_DOWN: MOTOR_MOVE_STOP);
	x1 = x < 0 ? 0 - x : x;
	y1 = y < 0 ? 0 - y : y;

	if((x1 + y1) == 0){
		return 0;
	}else if(x1 == 0){
		times = 1;
	}else if(y1 == 0){
		times = 1;
	}else
		times = calc_max_divisor(x1, y1);

	mutex_lock(&mdev->dev_mutex);
	spin_lock_irqsave(&mdev->slock, flags);
	mdev->dev_state = MOTOR_OPS_NORMAL;
	mdev->dst_move.one.x = x1 / times;
	mdev->dst_move.one.y = y1 / times;
	mdev->dst_move.times = times;
	mdev->cur_move.one.x = 0;
	mdev->cur_move.one.y = 0;
	mdev->cur_move.times = 0;
	motors[HORIZONTAL_MOTOR].state = MOTOR_OPS_NORMAL;
	motors[HORIZONTAL_MOTOR].move_dir = x_dir;
	motors[VERTICAL_MOTOR].state = MOTOR_OPS_NORMAL;
	motors[VERTICAL_MOTOR].move_dir = y_dir;
	spin_unlock_irqrestore(&mdev->slock, flags);
	mutex_unlock(&mdev->dev_mutex);
	//printk("%s%d x=%d y=%d t=%d\n",__func__,__LINE__,mdev->dst_move.one.x,mdev->dst_move.one.y,mdev->dst_move.times);
	//printk("x_dir=%d,y_dir=%d\n",x_dir,y_dir);
	jz_tcu_enable_counter(mdev->tcu);
	return 0;
}

static void motor_ops_stop(struct motor_device *mdev)
{
	unsigned long flags;
	struct motor_driver *motors = mdev->motors;

	jz_tcu_disable_counter(mdev->tcu);
	mutex_lock(&mdev->dev_mutex);
	spin_lock_irqsave(&mdev->slock, flags);
	mdev->dev_state = MOTOR_OPS_STOP;
	motors[HORIZONTAL_MOTOR].state = MOTOR_OPS_STOP;
	motors[VERTICAL_MOTOR].state = MOTOR_OPS_STOP;
	spin_unlock_irqrestore(&mdev->slock, flags);
	mutex_unlock(&mdev->dev_mutex);
	motor_move_step(mdev);
	return;
}

static long motor_ops_cruise(struct motor_device *mdev)
{
	unsigned long flags;
	struct motor_driver *motors = mdev->motors;
	mutex_lock(&mdev->dev_mutex);
	spin_lock_irqsave(&mdev->slock, flags);
	mdev->dev_state = MOTOR_OPS_CRUISE;
	motors[HORIZONTAL_MOTOR].state = MOTOR_OPS_CRUISE;
	motors[VERTICAL_MOTOR].state = MOTOR_OPS_CRUISE;
	spin_unlock_irqrestore(&mdev->slock, flags);
	mutex_unlock(&mdev->dev_mutex);
	jz_tcu_enable_counter(mdev->tcu);
	return 0;
}

static long motor_ops_goback(struct motor_device *mdev)
{
	struct motor_driver *motors = mdev->motors;
	int sx, sy;
	int cx, cy;
	sx = motors[HORIZONTAL_MOTOR].max_steps >> 1;
	sy = motors[VERTICAL_MOTOR].max_steps >> 1;
	cx = motors[HORIZONTAL_MOTOR].cur_steps;
	cy = motors[VERTICAL_MOTOR].cur_steps;
	//printk("sx=%d,sy=%d,cx=%d,cy=%d\n",sx,sy,cx,cy);
	return motor_ops_move(mdev, sx-cx, sy-cy);
}

static void motor_get_message(struct motor_device *mdev, struct motor_message *msg)
{
	struct motor_driver *motors = mdev->motors;
	msg->x = motors[HORIZONTAL_MOTOR].cur_steps;
	msg->y = motors[VERTICAL_MOTOR].cur_steps;
	msg->speed = mdev->tcu_speed;
	if(mdev->dev_state == MOTOR_OPS_STOP)
		msg->status = MOTOR_IS_STOP;
	else
		msg->status = MOTOR_IS_RUNNING;
	return;
}

static inline int motor_ops_reset_check_params(struct motor_reset_data *rdata)
{
	if(rdata->x_max_steps == 0 || rdata->y_max_steps == 0){
		return -1;
	}
	if(rdata->x_max_steps < rdata->x_cur_step || rdata->x_max_steps < rdata->x_cur_step)
		return -1;
	return 0;
}

static long motor_ops_reset(struct motor_device *mdev, struct motor_reset_data *rdata)
{
	unsigned long flags;
	int index = 0;
	long ret = 0;
	int value = 0;
	int times = 0;
	struct motor_message msg;
	//printk("%s%d\n",__func__,__LINE__);

	if(mdev == NULL || rdata == NULL){
		printk("ERROR: the parameters of %s is wrong!!\n",__func__);
		return -EPERM;
	}

	if(motor_ops_reset_check_params(rdata) == 0){
		/* app set max steps and current pos */
		mutex_lock(&mdev->dev_mutex);
		spin_lock_irqsave(&mdev->slock, flags);
		mdev->motors[HORIZONTAL_MOTOR].max_steps = rdata->x_max_steps;
		mdev->motors[HORIZONTAL_MOTOR].cur_steps = rdata->x_cur_step;
		mdev->motors[VERTICAL_MOTOR].max_steps = rdata->y_max_steps;
		mdev->motors[VERTICAL_MOTOR].cur_steps = rdata->y_cur_step;
		spin_unlock_irqrestore(&mdev->slock, flags);
		mutex_unlock(&mdev->dev_mutex);
	}else{
		/* driver calculate max steps. */
		mutex_lock(&mdev->dev_mutex);
		spin_lock_irqsave(&mdev->slock, flags);
		for(index = 0; index < HAS_MOTOR_CNT; index++){
			value = gpio_get_value(mdev->motors[index].pdata->motor_max_gpio);
			if(value == mdev->motors[index].pdata->motor_gpio_level){
				mdev->motors[index].move_dir = MOTOR_MOVE_LEFT_DOWN;
			}else
				mdev->motors[index].move_dir = MOTOR_MOVE_RIGHT_UP;
			mdev->motors[index].state = MOTOR_OPS_RESET;
			mdev->motors[index].max_steps = 0x0fffffff;
			mdev->motors[index].cur_steps = 0x00ffffff;
			mdev->motors[index].min_pos_irq_cnt = 0;
			mdev->motors[index].max_pos_irq_cnt = 0;
			mdev->motors[index].reset_max_pos = 0;
			mdev->motors[index].reset_min_pos = 0;
		}
		mdev->dst_move.one.x = 0x0fffffff;
		mdev->dst_move.one.y = 0x0fffffff;
		mdev->dst_move.times = 1;
		mdev->cur_move.one.x = 0;
		mdev->cur_move.one.y = 0;
		mdev->cur_move.times = 0;
		mdev->dev_state = MOTOR_OPS_RESET;
		spin_unlock_irqrestore(&mdev->slock, flags);
		mutex_unlock(&mdev->dev_mutex);
		jz_tcu_enable_counter(mdev->tcu);

		for(index = 0; index < HAS_MOTOR_CNT; index++){
			do{
				ret = wait_for_completion_interruptible_timeout(&mdev->motors[index].reset_completion, msecs_to_jiffies(150000));
				if(ret == 0){
					ret = -ETIMEDOUT;
					goto exit;
				}
			}while(ret == -ERESTARTSYS);
		}
	}
	//printk("x_max = %d, y_max = %d\n", mdev->motors[HORIZONTAL_MOTOR].max_steps,
			//mdev->motors[VERTICAL_MOTOR].max_steps);
	ret = motor_ops_goback(mdev);
	/*ret =  motor_ops_move(mdev, (mdev->motors[HORIZONTAL_MOTOR].max_steps) >> 1, */
			/*(mdev->motors[VERTICAL_MOTOR].max_steps) >> 1);*/

	do{
		msleep(10);
		motor_get_message(mdev, &msg);
		times++;
		if(times > 1000){
			printk("ERROR:wait motor timeout %s%d\n",__func__,__LINE__);
			ret = -ETIMEDOUT;
			goto exit;
		}
	}while(msg.status == MOTOR_IS_RUNNING);
	ret = 0;

	/* sync data */
	 rdata->x_max_steps	= mdev->motors[HORIZONTAL_MOTOR].max_steps;
	 rdata->x_cur_step	= mdev->motors[HORIZONTAL_MOTOR].cur_steps;
	 rdata->y_max_steps	= mdev->motors[VERTICAL_MOTOR].max_steps;
	 rdata->y_cur_step	= mdev->motors[VERTICAL_MOTOR].cur_steps;

exit:
	jz_tcu_disable_counter(mdev->tcu);
	msleep(10);
	motor_set_default(mdev);
	return ret;
}

static int motor_speed(struct motor_device *mdev, int speed)
{
	__asm__("ssnop");
	if ((speed < MOTOR_MIN_SPEED) || (speed > MOTOR_MAX_SPEED)) {
		dev_err(mdev->dev, "speed(%d) set error\n", speed);
		return -1;
	}
	__asm__("ssnop");

	mdev->tcu_speed = speed;
	jz_tcu_set_period(mdev->tcu, (24000000 / 64 / mdev->tcu_speed));

	return 0;
}

static int motor_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct motor_device *mdev = container_of(dev, struct motor_device, misc_dev);
	int ret = 0;
	if(mdev->flag){
		ret = -EBUSY;
		dev_err(mdev->dev, "Motor driver busy now!\n");
	}else{
		mdev->flag = 1;
	}

	return ret;
}

static int motor_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct motor_device *mdev = container_of(dev, struct motor_device, misc_dev);
	motor_ops_stop(mdev);
	mdev->flag = 0;
	return 0;
}

static long motor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	struct motor_device *mdev = container_of(dev, struct motor_device, misc_dev);
	long ret = 0;

	if(mdev->flag == 0){
		printk("Please Open /dev/motor Firstly\n");
		return -EPERM;
	}

	switch (cmd) {
		case MOTOR_STOP:
			motor_ops_stop(mdev);
			/*printk("MOTOR_STOP!!!!!!!!!!!!!!!!!!!\n");*/
			break;
		case MOTOR_RESET:
			{
				struct motor_reset_data rdata;
				if(arg == 0){
					ret = -EPERM;
					break;
				}
				if (copy_from_user(&rdata, (void __user *)arg,
							sizeof(rdata))) {
					dev_err(mdev->dev, "[%s][%d] copy from user error\n",
							__func__, __LINE__);
					return -EFAULT;
				}
				ret = motor_ops_reset(mdev, &rdata);
				if(!ret){
					if (copy_to_user((void __user *)arg, &rdata,
								sizeof(rdata))) {
						dev_err(mdev->dev, "[%s][%d] copy to user error\n",
								__func__, __LINE__);
						return -EFAULT;
					}
				}
				/*printk("MOTOR_RESET!!!!!!!!!!!!!!!!!!!\n");*/
				break;
			}
		case MOTOR_MOVE:
			{
				struct motors_steps dst;
				if (copy_from_user(&dst, (void __user *)arg,
							sizeof(struct motors_steps))) {
					dev_err(mdev->dev, "[%s][%d] copy from user error\n",
							__func__, __LINE__);
					return -EFAULT;
				}
				ret = motor_ops_move(mdev, dst.x, dst.y);
				/*printk("MOTOR_MOVE!!!!!!!!!!!!!!!!!!!\n");*/
			}
			break;
		case MOTOR_GET_STATUS:
			{
				struct motor_message msg;

				motor_get_message(mdev, &msg);
				if (copy_to_user((void __user *)arg, &msg,
							sizeof(struct motor_message))) {
					dev_err(mdev->dev, "[%s][%d] copy to user error\n",
							__func__, __LINE__);
					return -EFAULT;
				}
			}
			/*printk("MOTOR_GET_STATUS!!!!!!!!!!!!!!!!!!\n");*/
			break;
		case MOTOR_SPEED:
			{
				int speed;

				if (copy_from_user(&speed, (void __user *)arg, sizeof(int))) {
					dev_err(mdev->dev, "[%s][%d] copy to user error\n", __func__, __LINE__);
					return -EFAULT;
				}

				motor_speed(mdev, speed);
			}
			/*printk("MOTOR_SPEED!!!!!!!!!!!!!!!!!!!!!!!\n");*/
			break;
		case MOTOR_GOBACK:
			/*printk("MOTOR_GOBACK!!!!!!!!!!!!!!!!!!!!!!!\n");*/
			ret = motor_ops_goback(mdev);
			break;
		case MOTOR_CRUISE:
			/*printk("MOTOR_CRUISE!!!!!!!!!!!!!!!!!!!!!!!\n");*/
			ret = motor_ops_cruise(mdev);
			break;
		default:
			return -EINVAL;
	}

	return ret;
}

static struct file_operations motor_fops = {
	.open = motor_open,
	.release = motor_release,
	.unlocked_ioctl = motor_ioctl,
};

static int motor_info_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct motor_device *mdev = (struct motor_device *)(m->private);
	struct motor_message msg;
	int index = 0;

	len += seq_printf(m ,"The version of Motor driver is %s\n",JZ_MOTOR_DRIVER_VERSION);
	len += seq_printf(m ,"Motor driver is %s\n", mdev->flag?"opened":"closed");
	len += seq_printf(m ,"The max speed is %d and the min speed is %d\n", MOTOR_MAX_SPEED, MOTOR_MIN_SPEED);
	motor_get_message(mdev, &msg);
	len += seq_printf(m ,"The status of motor is %s\n", msg.status?"running":"stop");
	len += seq_printf(m ,"The pos of motor is (%d, %d)\n", msg.x, msg.y);
	len += seq_printf(m ,"The speed of motor is %d\n", msg.speed);

	for(index = 0; index < HAS_MOTOR_CNT; index++){
		len += seq_printf(m ,"## motor is %s ##\n", mdev->motors[index].pdata->name);
		len += seq_printf(m ,"max steps %d\n", mdev->motors[index].max_steps);
		len += seq_printf(m ,"motor direction %d\n", mdev->motors[index].move_dir);
		len += seq_printf(m ,"motor state %d(normal; cruise; reset)\n", mdev->motors[index].state);
		len += seq_printf(m ,"the irq's counter of max pos is %d\n", mdev->motors[index].max_pos_irq_cnt);
		len += seq_printf(m ,"the irq's counter of min pos is %d\n", mdev->motors[index].min_pos_irq_cnt);
	}
	return len;
}

static int motor_info_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, motor_info_show, PDE_DATA(inode), 1024);
}

static const struct file_operations motor_info_fops ={
	.read = seq_read,
	.open = motor_info_open,
	.llseek = seq_lseek,
	.release = single_release,
};

static int motor_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct motor_device *mdev;
	struct motor_driver *motor = NULL;
	struct proc_dir_entry *proc;
	//printk("%s%d\n",__func__,__LINE__);
	mdev = devm_kzalloc(&pdev->dev, sizeof(struct motor_device), GFP_KERNEL);
	if (!mdev) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "kzalloc motor device memery error\n");
		goto error_devm_kzalloc;
	}

	mdev->cell = mfd_get_cell(pdev);
	if (!mdev->cell) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "Failed to get mfd cell for jz_adc_aux!\n");
		goto error_devm_kzalloc;
	}

	mdev->dev = &pdev->dev;
	mdev->tcu = (struct jz_tcu_chn *)mdev->cell->platform_data;
	mdev->tcu->irq_type = FULL_IRQ_MODE;
	mdev->tcu->clk_src = TCU_CLKSRC_EXT;
	mdev->tcu->prescale = TCU_PRESCALE_64;

	mdev->tcu_speed = MOTOR_MAX_SPEED;
	jz_tcu_config_chn(mdev->tcu);
	jz_tcu_set_period(mdev->tcu, (24000000 / 64 / mdev->tcu_speed));
	jz_tcu_start_counter(mdev->tcu);

	mutex_init(&mdev->dev_mutex);
	spin_lock_init(&mdev->slock);

	platform_set_drvdata(pdev, mdev);

	for(i = 0; i < HAS_MOTOR_CNT; i++) {
		motor = &(mdev->motors[i]);
		motor->pdata = &motors_pdata[i];
		motor->move_dir	= MOTOR_MOVE_STOP;
		init_completion(&motor->reset_completion);

		if (motor->pdata->motor_min_gpio != -1) {
			gpio_request(motor->pdata->motor_min_gpio, "motor_min_gpio");
			motor->min_pos_irq = gpio_to_irq(motor->pdata->motor_min_gpio);

			ret = request_irq(motor->min_pos_irq,
					motor_min_gpio_interrupt,
					(motor->pdata->motor_gpio_level ?
					 IRQF_TRIGGER_RISING :
					 IRQF_TRIGGER_FALLING)
					| IRQF_DISABLED ,
					"motor_min_gpio", motor);
			if (ret) {
				dev_err(&pdev->dev, "request motor_min_gpio error\n");
				motor->min_pos_irq = 0;
				goto error_min_gpio;
			}

		}
		if (motor->pdata->motor_max_gpio != -1) {
			gpio_request(motor->pdata->motor_max_gpio, "motor_max_gpio");
			motor->max_pos_irq = gpio_to_irq(motor->pdata->motor_max_gpio);

			ret = request_irq(motor->max_pos_irq,
					motor_max_gpio_interrupt,
					(motor->pdata->motor_gpio_level ?
					 IRQF_TRIGGER_RISING :
					 IRQF_TRIGGER_FALLING)
					| IRQF_DISABLED ,
					"motor_max_gpio", motor);
			if (ret) {
				dev_err(&pdev->dev, "request motor_max_gpio error\n");
				motor->max_pos_irq = 0;
				goto error_max_gpio;
			}
		}

		if (motor->pdata->motor_st1_gpio != -1) {
			gpio_request(motor->pdata->motor_st1_gpio, "motor_st1_gpio");
		}
		if (motor->pdata->motor_st2_gpio != -1) {
			gpio_request(motor->pdata->motor_st2_gpio, "motor_st2_gpio");
		}
		if (motor->pdata->motor_st3_gpio != -1) {
			gpio_request(motor->pdata->motor_st3_gpio, "motor_st3_gpio");
		}
		if (motor->pdata->motor_st4_gpio != -1) {
			gpio_request(motor->pdata->motor_st4_gpio, "motor_st4_gpio");
		}

		setup_timer(&motor->min_timer,
			    gpio_keys_min_timer, (unsigned long)motor);
		setup_timer(&motor->max_timer,
			    gpio_keys_max_timer, (unsigned long)motor);
	}
	jzgpio_ctrl_pull(GPIO_PORT_C, 1, 1<<13);
	jzgpio_ctrl_pull(GPIO_PORT_C, 1, 1<<14);
	jzgpio_ctrl_pull(GPIO_PORT_C, 1, 1<<18);

	mdev->run_step_irq = platform_get_irq(pdev,0);
	if (mdev->run_step_irq < 0) {
		ret = mdev->run_step_irq;
		dev_err(&pdev->dev, "Failed to get platform irq: %d\n", ret);
		goto error_get_irq;
	}

	ret = request_irq(mdev->run_step_irq, jz_timer_interrupt, 0,
				"jz_timer_interrupt", mdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to run request_irq() !\n");
		goto error_request_irq;
	}

	mdev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	mdev->misc_dev.name = "motor";
	mdev->misc_dev.fops = &motor_fops;
	ret = misc_register(&mdev->misc_dev);
	if (ret < 0) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "misc_register failed\n");
		goto error_misc_register;
	}

	/* debug info */
	proc = jz_proc_mkdir("motor");
	if (!proc) {
		mdev->proc = NULL;
		printk("create dev_attr_isp_info failed!\n");
	} else {
		mdev->proc = proc;
	}
	proc_create_data("motor_info", S_IRUGO, proc, &motor_info_fops, (void *)mdev);

	motor_set_default(mdev);
	mdev->flag = 0;
	//printk("%s%d\n",__func__,__LINE__);
	return 0;

error_misc_register:
	free_irq(mdev->run_step_irq, mdev);
error_request_irq:
error_get_irq:
error_max_gpio:
error_min_gpio:
	for(i = 0; i < HAS_MOTOR_CNT; i++) {
		motor = &(mdev->motors[i]);
		if(motor->pdata == NULL)
			continue;
		if (motor->min_pos_irq) {
			gpio_free(motor->pdata->motor_min_gpio);
			free_irq(motor->min_pos_irq, motor);
		}
		if (motor->max_pos_irq) {
			gpio_free(motor->pdata->motor_max_gpio);
			free_irq(motor->max_pos_irq, motor);
		}
		if (motor->pdata->motor_st1_gpio != -1)
			gpio_free(motor->pdata->motor_st1_gpio);

		if (motor->pdata->motor_st2_gpio != -1)
			gpio_free(motor->pdata->motor_st2_gpio);

		if (motor->pdata->motor_st3_gpio != -1)
			gpio_free(motor->pdata->motor_st3_gpio);

		if (motor->pdata->motor_st4_gpio != -1)
			gpio_free(motor->pdata->motor_st4_gpio);
		motor->pdata = 0;
		motor->min_pos_irq = 0;
		motor->max_pos_irq = 0;
		del_timer_sync(&motor->min_timer);
		del_timer_sync(&motor->max_timer);
	}
	kfree(mdev);
error_devm_kzalloc:
	return ret;
}

static int motor_remove(struct platform_device *pdev)
{
	int i;
	struct motor_device *mdev = platform_get_drvdata(pdev);
	struct motor_driver *motor = NULL;

	jz_tcu_disable_counter(mdev->tcu);
	jz_tcu_stop_counter(mdev->tcu);
	mutex_destroy(&mdev->dev_mutex);

	free_irq(mdev->run_step_irq, mdev);
	for(i = 0; i < HAS_MOTOR_CNT; i++) {
		motor = &(mdev->motors[i]);
		if(motor->pdata == NULL)
			continue;
		if (motor->min_pos_irq) {
			gpio_free(motor->pdata->motor_min_gpio);
			free_irq(motor->min_pos_irq, motor);
		}
		if (motor->max_pos_irq) {
			gpio_free(motor->pdata->motor_max_gpio);
			free_irq(motor->max_pos_irq, motor);
		}
		if (motor->pdata->motor_st1_gpio != -1)
			gpio_free(motor->pdata->motor_st1_gpio);

		if (motor->pdata->motor_st2_gpio != -1)
			gpio_free(motor->pdata->motor_st2_gpio);

		if (motor->pdata->motor_st3_gpio != -1)
			gpio_free(motor->pdata->motor_st3_gpio);

		if (motor->pdata->motor_st4_gpio != -1)
			gpio_free(motor->pdata->motor_st4_gpio);
		motor->pdata = 0;
		motor->min_pos_irq = 0;
		motor->max_pos_irq = 0;
		motor->min_pos_irq_cnt = 0;
		motor->max_pos_irq_cnt = 0;
		del_timer_sync(&motor->min_timer);
		del_timer_sync(&motor->max_timer);
	}

	if (mdev->proc)
		proc_remove(mdev->proc);

	misc_deregister(&mdev->misc_dev);

	kfree(mdev);
	return 0;
}

static struct platform_driver motor_driver = {
	.probe = motor_probe,
	.remove = motor_remove,
	.driver = {
		.name	= "tcu_chn2",
		.owner	= THIS_MODULE,
	}
};

static int __init motor_init(void)
{
	return platform_driver_register(&motor_driver);
}

static void __exit motor_exit(void)
{
	platform_driver_unregister(&motor_driver);
}

module_init(motor_init);
module_exit(motor_exit);

MODULE_LICENSE("GPL");

