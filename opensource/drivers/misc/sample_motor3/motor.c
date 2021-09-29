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
#include "ms419xx_spi_dev.h"


#define JZ_MOTOR_DRIVER_VERSION "H20181015a"

static unsigned int hmaxstep = 967;
module_param(hmaxstep, int, S_IRUGO);
MODULE_PARM_DESC(hmaxstep, "The max steps of horizontal motor");

static unsigned int vmaxstep = 187;
module_param(vmaxstep, int, S_IRUGO);
MODULE_PARM_DESC(vmaxstep, "The max steps of vertical motor");

static unsigned int hdir = 0;
module_param(hdir, int, S_IRUGO);
MODULE_PARM_DESC(hdir, "The left is forward when hdir is 0; The left is opposite when hdir is 1");

static unsigned int vdir = 0;
module_param(vdir, int, S_IRUGO);
MODULE_PARM_DESC(hdir, "The down is forward when vdir is 0; The down is opposite when vdir is 1");

static irqreturn_t jz_timer_interrupt(int irq, void *dev_id)
{
	struct motor_device *mdev = dev_id;
	struct motor_driver *motors = mdev->motors;
	irqreturn_t ret = IRQ_HANDLED;
	bool fall_edge = false;
	int i = 0;

//	printk("mdev->vdfz_valid = %d\n", mdev->vdfz_valid);
	/* operate VDFZ */
	if(mdev->vdfz_valid){
		if(mdev->vdfz_state){
			gpio_direction_output(MS419XX_VDFZ_GPIO, 0);
			mdev->vdfz_state = 0;
			fall_edge = true;
		}else{
			gpio_direction_output(MS419XX_VDFZ_GPIO, 1);
			mdev->vdfz_state = 1;
		}
	}else{
		gpio_direction_output(MS419XX_VDFZ_GPIO, 0);
		mdev->vdfz_state = 0;
	}

	/* VDFZ is fall edge */
	if(fall_edge){
		/* operate register */
		if(mdev->reg_state == REGISTER_SYNC){
			mdev->reg_state = REGISTER_UPDATE;
		}
		for(i = 0; i < HAS_MOTOR_CNT; i++){
			if(mdev->reg_state == REGISTER_UPDATE){
				motors[i].move_dir = motors[i].move_dir_prebuild;
			}
			motors[i].cur_steps += motors[i].move_dir == MOTOR_MOVE_STOP ? 0 : 1;
			motors[i].cur_position += motors[i].move_dir;
		}

		/* update motor's position */
		if(mdev->mode == MOTOR_OPS_CRUISE){
			for(i = 0; i < HAS_MOTOR_CNT; i++){
				if(motors[i].cur_position <= 0){
					motors[i].move_dir_prebuild = MOTOR_MOVE_RIGHT_UP;
					motors[i].cur_position = 0;
					mdev->reg_state = REGISTER_CHANGE;
				}
				if(motors[i].cur_position >= motors[i].max_position){
					motors[i].move_dir_prebuild = MOTOR_MOVE_LEFT_DOWN;
					motors[i].cur_position = motors[i].max_position;
					mdev->reg_state = REGISTER_CHANGE;
				}
			}
		}else{
			/* normal mode */
			for(i = 0; i < HAS_MOTOR_CNT; i++){
				if(motors[i].move_dir == MOTOR_MOVE_STOP)
					continue;

				if(motors[i].cur_position <= 0){
					motors[i].move_dir_prebuild = MOTOR_MOVE_STOP;
					motors[i].cur_position = 0;
					mdev->reg_state = REGISTER_CHANGE;
				} else if(motors[i].cur_position >= motors[i].max_position){
					motors[i].move_dir_prebuild = MOTOR_MOVE_STOP;
					motors[i].cur_position = motors[i].max_position;
					mdev->reg_state = REGISTER_CHANGE;
				} else {
					if(motors[i].cur_steps == motors[i].dst_steps){
						motors[i].move_dir_prebuild = MOTOR_MOVE_STOP;
						mdev->reg_state = REGISTER_CHANGE;
					}
				}
			}
		}
	}

	switch(mdev->reg_state){
		case REGISTER_CHANGE:
			mdev->vdfz_valid = 0;
			ret = IRQ_WAKE_THREAD;
			break;
		case REGISTER_UPDATE:
			mdev->reg_state = REGISTER_NOCHANGE;
		case REGISTER_NOCHANGE:
			if(motors[HORIZONTAL_MOTOR].move_dir == MOTOR_MOVE_STOP && motors[VERTICAL_MOTOR].move_dir == MOTOR_MOVE_STOP){
				mdev->vdfz_valid = 0;
				ret = IRQ_WAKE_THREAD;
			}
			break;
		default:
			break;
	}

	return ret;
}

static int direction_to_motor(unsigned int motor, int direction)
{
	int dir = 0;

	if(direction == MOTOR_MOVE_STOP)
		return MS419XX_STOP;

	if(motor == HORIZONTAL_MOTOR){
		if(hdir == MOTOR_LEFT_FORWARD){
			if(direction == MOTOR_MOVE_LEFT_DOWN)
				dir = MS419XX_FORWARD;
			else
				dir = MS419XX_REVERSE;
		}else{
			/* hdir == MOTOR_LEFT_REVERSE */
			if(direction == MOTOR_MOVE_LEFT_DOWN)
				dir = MS419XX_REVERSE;
			else
				dir = MS419XX_FORWARD;
		}
	}else{
		if(vdir == MOTOR_DOWN_FORWARD){
			if(direction == MOTOR_MOVE_LEFT_DOWN)
				dir = MS419XX_FORWARD;
			else
				dir = MS419XX_REVERSE;
		}else{
			/* vdir == MOTOR_DOWN_REVERSE */
			if(direction == MOTOR_MOVE_LEFT_DOWN)
				dir = MS419XX_REVERSE;
			else
				dir = MS419XX_FORWARD;
		}
	}
	return dir;
}


static irqreturn_t jz_timer_thread_handle(int this_irq, void *dev_id)
{
	unsigned long flags;
	struct motor_device *mdev = dev_id;
	struct motor_driver *motors = mdev->motors;
	int value = MS419XX_STOP;

	if(mdev->reg_state == REGISTER_CHANGE){
		if(motors[HORIZONTAL_MOTOR].move_dir != motors[HORIZONTAL_MOTOR].move_dir_prebuild){
			value = direction_to_motor(HORIZONTAL_MOTOR, motors[HORIZONTAL_MOTOR].move_dir_prebuild);
			jz_spidev_write(0x24, 1, value, 2);
		}
		if(motors[VERTICAL_MOTOR].move_dir != motors[VERTICAL_MOTOR].move_dir_prebuild){
			value = direction_to_motor(VERTICAL_MOTOR, motors[VERTICAL_MOTOR].move_dir_prebuild);
			jz_spidev_write(0x29, 1, value, 2);
		}

		spin_lock_irqsave(&mdev->slock, flags);
		mdev->reg_state = REGISTER_SYNC;
		mdev->vdfz_valid = 1;
		spin_unlock_irqrestore(&mdev->slock, flags);
	}else{
		if(motors[HORIZONTAL_MOTOR].move_dir == MOTOR_MOVE_STOP && motors[VERTICAL_MOTOR].move_dir == MOTOR_MOVE_STOP){
			jz_tcu_disable_counter(mdev->tcu);
			if(mdev->wait_stop){
				mdev->wait_stop = 0;
				complete(&mdev->stop_completion);
			}
		}
	}

	return IRQ_HANDLED;
}

static long motor_ops_move(struct motor_device *mdev, int x, int y, int block)
{
	struct motor_driver *motors = mdev->motors;
	unsigned long flags;
	int x_dir = MOTOR_MOVE_STOP;
	int y_dir = MOTOR_MOVE_STOP;
	int x1 = 0;
	int y1 = 0;
	long ret = 0;
	/* check x value */
	if(x > 0){
		if(motors[HORIZONTAL_MOTOR].cur_position >= motors[HORIZONTAL_MOTOR].max_position)
			x = 0;
	}else{
		if(motors[HORIZONTAL_MOTOR].cur_position <= 0)
			x = 0;
	}
	/* check y value */
	if(y > 0){
		if(motors[VERTICAL_MOTOR].cur_position >= motors[VERTICAL_MOTOR].max_position)
			y = 0;
	}else{
		if(motors[VERTICAL_MOTOR].cur_position <= 0)
			y = 0;
	}

	if(x > 0){
		x_dir = MOTOR_MOVE_RIGHT_UP;
	}else if (x < 0){
		x_dir = MOTOR_MOVE_LEFT_DOWN;
	}else
		x_dir = MOTOR_MOVE_STOP;

	if(y > 0){
		y_dir = MOTOR_MOVE_RIGHT_UP;
	}else if (y < 0){
		y_dir = MOTOR_MOVE_LEFT_DOWN;
	}else
		y_dir = MOTOR_MOVE_STOP;

	x1 = x < 0 ? 0 - x : x;
	y1 = y < 0 ? 0 - y : y;

	mutex_lock(&mdev->dev_mutex);
	spin_lock_irqsave(&mdev->slock, flags);
	mdev->mode = MOTOR_OPS_NORMAL;
	motors[HORIZONTAL_MOTOR].move_dir_prebuild = x_dir;
	if(motors[HORIZONTAL_MOTOR].move_dir_prebuild != motors[HORIZONTAL_MOTOR].move_dir)
		mdev->reg_state = REGISTER_CHANGE;
	motors[HORIZONTAL_MOTOR].dst_steps = x1;
	motors[HORIZONTAL_MOTOR].cur_steps = 0;

	motors[VERTICAL_MOTOR].move_dir_prebuild = y_dir;
	if(motors[VERTICAL_MOTOR].move_dir_prebuild != motors[VERTICAL_MOTOR].move_dir)
		mdev->reg_state = REGISTER_CHANGE;
	motors[VERTICAL_MOTOR].dst_steps = y1;
	motors[VERTICAL_MOTOR].cur_steps = 0;

	if(block)
		mdev->wait_stop = 1;
	spin_unlock_irqrestore(&mdev->slock, flags);
	mutex_unlock(&mdev->dev_mutex);

	jz_tcu_enable_counter(mdev->tcu);

	if(block){
		do{
			ret = wait_for_completion_interruptible_timeout(&mdev->stop_completion, msecs_to_jiffies(15000));
			if(ret == 0){
				ret = -ETIMEDOUT;
				break;
			}
		}while(ret == -ERESTARTSYS);
	}
	return ret;
}

static long motor_ops_stop(struct motor_device *mdev)
{
	long ret = 0;
	struct motor_driver *motors = mdev->motors;

	if(motors[HORIZONTAL_MOTOR].move_dir == MOTOR_MOVE_STOP && motors[VERTICAL_MOTOR].move_dir == MOTOR_MOVE_STOP)
		return ret;
	ret = motor_ops_move(mdev, 0, 0, 1);
	return ret;
}

static long motor_ops_goback(struct motor_device *mdev)
{
	unsigned long flags;
	struct motor_driver *motors = mdev->motors;
	int sx, sy;
	int cx, cy;
	mutex_lock(&mdev->dev_mutex);
	spin_lock_irqsave(&mdev->slock, flags);
	sx = motors[HORIZONTAL_MOTOR].max_position >> 1;
	sy = motors[VERTICAL_MOTOR].max_position >> 1;
	cx = motors[HORIZONTAL_MOTOR].cur_position;
	cy = motors[VERTICAL_MOTOR].cur_position;
	spin_unlock_irqrestore(&mdev->slock, flags);
	mutex_unlock(&mdev->dev_mutex);

	mdev->cruise_xdir = sx-cx > 0 ? MOTOR_MOVE_RIGHT_UP : MOTOR_MOVE_LEFT_DOWN;
	mdev->cruise_ydir = sy-cy > 0 ? MOTOR_MOVE_RIGHT_UP : MOTOR_MOVE_LEFT_DOWN;
	//printk("sx=%d,sy=%d,cx=%d,cy=%d\n",sx,sy,cx,cy);
	return motor_ops_move(mdev, sx-cx, sy-cy, 0);
}

static long motor_ops_cruise(struct motor_device *mdev)
{
	unsigned long flags;
	struct motor_driver *motors = mdev->motors;
	motor_ops_goback(mdev);
	mutex_lock(&mdev->dev_mutex);
	spin_lock_irqsave(&mdev->slock, flags);
	mdev->mode = MOTOR_OPS_CRUISE;
	motors[HORIZONTAL_MOTOR].move_dir_prebuild = mdev->cruise_xdir;
	motors[VERTICAL_MOTOR].move_dir_prebuild = mdev->cruise_ydir;
	mdev->reg_state = REGISTER_CHANGE;
	spin_unlock_irqrestore(&mdev->slock, flags);
	mutex_unlock(&mdev->dev_mutex);
	jz_tcu_enable_counter(mdev->tcu);
	return 0;
}

static void motor_get_message(struct motor_device *mdev, struct motor_message *msg)
{
	struct motor_driver *motors = mdev->motors;
	msg->x = motors[HORIZONTAL_MOTOR].cur_steps;
	msg->y = motors[VERTICAL_MOTOR].cur_steps;
	msg->speed = mdev->tcu_speed;
	if(motors[HORIZONTAL_MOTOR].move_dir == MOTOR_MOVE_STOP && motors[VERTICAL_MOTOR].move_dir == MOTOR_MOVE_STOP)
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
	long ret = 0;
	int times = 0;
	struct motor_message msg;
	printk("%s%d\n",__func__,__LINE__);

	if(mdev == NULL || rdata == NULL){
		printk("ERROR: the parameters of %s is wrong!!\n",__func__);
		return -EPERM;
	}

	jz_spidev_write(0x20, 1, 0x1e01, 2);
	jz_spidev_write(0x22, 1, 0x0001, 2);
	jz_spidev_write(0x27, 1, 0x0001, 2); //
	jz_spidev_write(0x23, 1, 0xa0a0, 2); // AB PWM duty
	jz_spidev_write(0x28, 1, 0xa0a0, 2); // CD PWM duty

	jz_spidev_write(0x25, 1, 0x0100, 2); // INTCTAB, when frequenc division is 64, the time is 4.1ms pre angle.
	jz_spidev_write(0x2a, 1, 0x0100, 2); // INTCTCD,

	if(motor_ops_reset_check_params(rdata) == 0){
		/* app set max steps and current pos */
		mutex_lock(&mdev->dev_mutex);
		spin_lock_irqsave(&mdev->slock, flags);
		mdev->motors[HORIZONTAL_MOTOR].max_position = rdata->x_max_steps;
		mdev->motors[HORIZONTAL_MOTOR].cur_position = rdata->x_cur_step;
		mdev->motors[VERTICAL_MOTOR].max_position = rdata->y_max_steps;
		mdev->motors[VERTICAL_MOTOR].cur_position = rdata->y_cur_step;
		spin_unlock_irqrestore(&mdev->slock, flags);
		mutex_unlock(&mdev->dev_mutex);
	}else{
		ret = motor_ops_move(mdev, mdev->motors[HORIZONTAL_MOTOR].max_position, mdev->motors[VERTICAL_MOTOR].max_position, 1);
	}

	ret = motor_ops_goback(mdev);

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
	 rdata->x_max_steps	= mdev->motors[HORIZONTAL_MOTOR].max_position;
	 rdata->x_cur_step	= mdev->motors[HORIZONTAL_MOTOR].cur_position;
	 rdata->y_max_steps	= mdev->motors[VERTICAL_MOTOR].max_position;
	 rdata->y_cur_step	= mdev->motors[VERTICAL_MOTOR].cur_position;

exit:
#if 0
	 {
	 	int value = 0;
		jz_spidev_read(0x20, 1, &value, 2);
		jz_spidev_read(0x22, 1, &value, 2);
		jz_spidev_read(0x27, 1, &value, 2);
		jz_spidev_read(0x23, 1, &value, 2);
		jz_spidev_read(0x28, 1, &value, 2);
		jz_spidev_read(0x25, 1, &value, 2);
		jz_spidev_read(0x2a, 1, &value, 2);
		jz_spidev_read(0x24, 1, &value, 2);
		jz_spidev_read(0x29, 1, &value, 2);
	 }
#endif
	msleep(10);
	return ret;
}

static int motor_speed(struct motor_device *mdev, int speed)
{
	return 0;
	if ((speed < MOTOR_MIN_SPEED) || (speed > MOTOR_MAX_SPEED)) {
		dev_err(mdev->dev, "speed(%d) set error\n", speed);
		return -1;
	}

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
				ret = motor_ops_move(mdev, dst.x, dst.y, 0);
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
	len += seq_printf(m ,"Motor mode is %s (normal; cruise)\n", mdev->mode == MOTOR_OPS_NORMAL? "normal" : "cruise");
	len += seq_printf(m ,"The max speed is %d and the min speed is %d\n", MOTOR_MAX_SPEED, MOTOR_MIN_SPEED);
	motor_get_message(mdev, &msg);
	len += seq_printf(m ,"The status of motor is %s\n", msg.status?"running":"stop");
	len += seq_printf(m ,"The pos of motor is (%d, %d)\n", msg.x, msg.y);
	len += seq_printf(m ,"The speed of motor is %d\n", msg.speed);

	for(index = 0; index < HAS_MOTOR_CNT; index++){
		len += seq_printf(m ,"## motor is %s ##\n", mdev->motors[index].pdata->name);
		len += seq_printf(m ,"max steps %d\n", mdev->motors[index].max_position);
		len += seq_printf(m ,"motor direction %d\n", mdev->motors[index].move_dir);
		len += seq_printf(m ,"the irq's counter of max pos is %d\n", mdev->motors[index].max_pos_irq_cnt);
		len += seq_printf(m ,"the irq's counter of min pos is %d\n", mdev->motors[index].min_pos_irq_cnt);
	}
	return len;
}

static int motor_info_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, motor_info_show, PDE_DATA(inode),1024);
}

static const struct file_operations motor_info_fops ={
	.read = seq_read,
	.open = motor_info_open,
	.llseek = seq_lseek,
	.release = single_release,
};

static int motor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct motor_device *mdev;
	struct proc_dir_entry *proc;

	mdev = devm_kzalloc(&pdev->dev, sizeof(struct motor_device), GFP_KERNEL);
	if (!mdev) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "kzalloc motor device memery error\n");
		goto error_devm_kzalloc;
	}

	ret = gpio_request(MS419XX_VDFZ_GPIO, "ms419xx_vdfz");
	if(ret){
		dev_err(&pdev->dev, "Failed to request vdfz gpio(%d)\n", MS419XX_VDFZ_GPIO);
		goto error_gpio;
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

	mdev->tcu_speed = 500;
	jz_tcu_config_chn(mdev->tcu);
	jz_tcu_set_period(mdev->tcu, (24000000 / 64 / mdev->tcu_speed));
	jz_tcu_start_counter(mdev->tcu);

	mutex_init(&mdev->dev_mutex);
	spin_lock_init(&mdev->slock);

	platform_set_drvdata(pdev, mdev);

	mdev->motors[HORIZONTAL_MOTOR].max_position = hmaxstep;
	mdev->motors[VERTICAL_MOTOR].max_position = vmaxstep;

	mdev->run_step_irq = platform_get_irq(pdev,0);
	if (mdev->run_step_irq < 0) {
		ret = mdev->run_step_irq;
		dev_err(&pdev->dev, "Failed to get platform irq: %d\n", ret);
		goto error_get_irq;
	}
	ret = request_threaded_irq(mdev->run_step_irq, jz_timer_interrupt, jz_timer_thread_handle, IRQF_ONESHOT, "jz_motor", mdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to run request_irq() !\n");
		goto error_request_irq;
	}

	init_completion(&mdev->stop_completion);
	mdev->wait_stop = 0;
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

	mdev->flag = 0;
	//printk("%s%d\n",__func__,__LINE__);
	return 0;

error_misc_register:
	free_irq(mdev->run_step_irq, mdev);
error_request_irq:
error_get_irq:
	gpio_free(MS419XX_VDFZ_GPIO);
error_gpio:
	kfree(mdev);
error_devm_kzalloc:
	return ret;
}

static int motor_remove(struct platform_device *pdev)
{
	struct motor_device *mdev = platform_get_drvdata(pdev);

	jz_tcu_disable_counter(mdev->tcu);
	jz_tcu_stop_counter(mdev->tcu);
	mutex_destroy(&mdev->dev_mutex);

	free_irq(mdev->run_step_irq, mdev);

	if (mdev->proc)
		proc_remove(mdev->proc);
	misc_deregister(&mdev->misc_dev);

	gpio_free(MS419XX_VDFZ_GPIO);
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
	jz_spidev_init();
	return platform_driver_register(&motor_driver);
}

static void __exit motor_exit(void)
{
	jz_spidev_exit();
	platform_driver_unregister(&motor_driver);
}

module_init(motor_init);
module_exit(motor_exit);

MODULE_LICENSE("GPL");
