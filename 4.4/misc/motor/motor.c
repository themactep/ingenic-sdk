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
#include <linux/mfd/ingenic-tcu.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#ifdef CONFIG_SOC_T31
#include <dt-bindings/interrupt-controller/t31-irq.h>

#endif
#ifdef CONFIG_SOC_T40
#include <dt-bindings/interrupt-controller/t40-irq.h>

#endif

#include <soc/base.h>
#include <soc/extal.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <soc/gpio.h>
#include "motor.h"

#include "../tcu_alloc/tcu_alloc.h"

#define JZ_MOTOR_DRIVER_VERSION "H20171206a"
#define MOTOR_MAX_TCU 8


/*
 * Motors GPIOs
 */

/* Pan motor */

int hst1 = -1;
module_param(hst1, int, S_IRUGO);
MODULE_PARM_DESC(hst1, "Pan motor Phase A pin");

int hst2 = -1;
module_param(hst2, int, S_IRUGO);
MODULE_PARM_DESC(hst2, "Pan motor Phase B pin");

int hst3 = -1;
module_param(hst3, int, S_IRUGO);
MODULE_PARM_DESC(hst3, "Pan motor Phase C pin");

int hst4 = -1;
module_param(hst4, int, S_IRUGO);
MODULE_PARM_DESC(hst4, "Pan motor Phase D pin");

/* Tilt motor */

int vst1 = -1;
module_param(vst1, int, S_IRUGO);
MODULE_PARM_DESC(vst1, "Tilt motor Phase A pin");

int vst2 = -1;
module_param(vst2, int, S_IRUGO);
MODULE_PARM_DESC(vst2, "Tilt motor Phase B pin");

int vst3 = -1;
module_param(vst3, int, S_IRUGO);
MODULE_PARM_DESC(vst3, "Tilt motor Phase C pin");

int vst4 = -1;
module_param(vst4, int, S_IRUGO);
MODULE_PARM_DESC(vst4, "Tilt motor Phase D pin");

/*
 * Motors GPIOs for direction control
 */

int motor_switch_gpio = -1;
module_param(motor_switch_gpio, int, S_IRUGO);
MODULE_PARM_DESC(motor_switch_gpio, "Motor switching GPIO");

/* invert_gpio_dir=1 (true): LOW=energize, HIGH=cut -> value=1 */
/* invert_gpio_dir=0 (false): HIGH=energize, LOW=cut -> value=0 */
int invert_gpio_dir = 0;
module_param(invert_gpio_dir, int, S_IRUGO);
MODULE_PARM_DESC(invert_gpio_dir, "Invert motor GPIO values. Default: 0 (no, HIGH=energize, LOW=cut)");

/*
 * Motors motion parameters
 *
 *  0 - motor lower physical motion limit
 *  (axis)min - motor start point
 *  (axis)max - motor stop point
 *  (axis)maxstep - motor upper physical motion limit
 *
 *   0 ---- hmin --------------- hmax ---- hmaxstep
 */
// TODO: make use of hmin and hmax

/* Pan motor */

int hmaxstep = 0x7fffffff;
module_param(hmaxstep, int, S_IRUGO);
MODULE_PARM_DESC(hmaxstep, "The max steps of pan motor");

int hmin = 0;
module_param(hmin, int, S_IRUGO);
MODULE_PARM_DESC(hmin, "Pan motor start point step count");

int hmax = -1;
module_param(hmax, int, S_IRUGO);
MODULE_PARM_DESC(hmax, "Pan motor stop point step count");

int hmin_gpio = -1;
module_param(hmin_gpio, int, S_IRUGO);
MODULE_PARM_DESC(hmin_gpio, "Pan motor start point endstop GPIO");

int hmax_gpio = -1;
module_param(hmax_gpio, int, S_IRUGO);
MODULE_PARM_DESC(hmax_gpio, "Pan motor stop point endstop GPIO");

int hlevel = 0;
module_param(hlevel, int, S_IRUGO);
MODULE_PARM_DESC(hlevel, "Pan motor endstop trigger level, default is 0");

/* Tilt motor */

int vmaxstep = 0x7fffffff;
module_param(vmaxstep, int, S_IRUGO);
MODULE_PARM_DESC(vmaxstep, "The max steps of tilt motor");

int vmin = 0;
module_param(vmin, int, S_IRUGO);
MODULE_PARM_DESC(vmin, "Tilt motor start point step count");

int vmax = -1;
module_param(vmax, int, S_IRUGO);
MODULE_PARM_DESC(vmax, "Tilt motor stop point step count");

int vmin_gpio = -1;
module_param(vmin_gpio, int, S_IRUGO);
MODULE_PARM_DESC(vmin_gpio, "Tilt motor start point endstop GPIO");

int vmax_gpio = -1;
module_param(vmax_gpio, int, S_IRUGO);
MODULE_PARM_DESC(vmax_gpio, "Tilt motor stop point endstop GPIO");

int vlevel = 0;
module_param(vlevel, int, S_IRUGO);
MODULE_PARM_DESC(vlevel, "Tilt motor endstop trigger level, default is 0");


static char *motor_tcu_channels;
module_param_named(tcu_channels, motor_tcu_channels, charp, 0644);
MODULE_PARM_DESC(tcu_channels, "Comma-separated TCU channels for motor (e.g., 2 or 2,3)");

static int motor_bind_channel = 2; /* first channel parsed; used for binding */
static void motor_parse_channels(void)
{
	if (motor_tcu_channels && *motor_tcu_channels) {
		char *str = kstrdup(motor_tcu_channels, GFP_KERNEL);
		char *p = str, *tok;
		if (!str)
			return;
		while ((tok = strsep(&p, ",")) != NULL) {
			unsigned long v;
			if (!kstrtoul(tok, 0, &v) && v < MOTOR_MAX_TCU) {
				motor_bind_channel = (int)v; /* pick first valid for binding */
				break;
			}
		}
		kfree(str);
	}
}


static char motor_driver_name[16] = "tcu_chn2";


struct motor_platform_data motors_pdata[NUMBER_OF_MOTORS] = {
	{
		.name = "Horizontal motor",
		.motor_min_gpio		= -1,
		.motor_max_gpio 	= -1,
		.motor_endstop_level	=  0,
		.motor_st1_gpio		= -1,
		.motor_st2_gpio		= -1,
		.motor_st3_gpio		= -1,
		.motor_st4_gpio		= -1,
	},
	{
		.name = "Vertical motor",
		.motor_min_gpio		= -1,
		.motor_max_gpio 	= -1,
		.motor_endstop_level	=  0,
		.motor_st1_gpio		= -1,
		.motor_st2_gpio		= -1,
		.motor_st3_gpio		= -1,
		.motor_st4_gpio		= -1,
	},
};

/* This is the driving pattern for common step motors. The least significant bit
 * is the phase D. 0x08 means phase A is active(powered), 0x0c means phase A and B, etc.
 * If the common terminal is positive, this will likely to produce clockwise rotation
 * viewed from the shaft side.
 */
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

/* motor_power_off disables all motor GPIO pins to cut power
 * When gpio_invert=true (invert_gpio_dir=1): set to HIGH (1) to cut power
 * When gpio_invert=false (invert_gpio_dir=0): set to LOW (0) to cut power
 *
 * Normally you won't need this as the interrupt handler(stepping program) will
 * turn off the power automatically if motor state is MOTOR_OPS_STOP.
 *
 * This is used to initialize the pins as outputs.
 *
 * NOTE: Use gpio_set_value in interrupt context for safety.
 * NOTE: check against motor_move_step
 */
static void motor_power_off(struct motor_device *mdev)
{
	int index;
	int value;
	struct motor_driver *motor = NULL;

	/* Power-off state depends on inversion setting */
	/* invert_gpio_dir=1 (true): LOW=energize, HIGH=cut -> value=1 */
	/* invert_gpio_dir=0 (false): HIGH=energize, LOW=cut -> value=0 */
	value = (invert_gpio_dir & 0x1);

	for (index = 0; index < NUMBER_OF_MOTORS; index++) {
		motor = &mdev->motors[index];

		if (motor->pdata->motor_st1_gpio != -1)
			gpio_direction_output(motor->pdata->motor_st1_gpio, value);
		if (motor->pdata->motor_st2_gpio != -1)
			gpio_direction_output(motor->pdata->motor_st2_gpio, value);
		if (motor->pdata->motor_st3_gpio != -1)
			gpio_direction_output(motor->pdata->motor_st3_gpio, value);
		if (motor->pdata->motor_st4_gpio != -1)
			gpio_direction_output(motor->pdata->motor_st4_gpio, value);
	}
}

/*
 * motor_power_on re-enables motor GPIO pins as outputs
 * Called before motor movement to energize the coils, appling torque to
 * maintain current position.
 *
 * NOTE: this function may run in spinlock/interrupt context, take care
 * NOTE: check against motor_move_step
 */
static void motor_power_on(struct motor_device *mdev)
{
	int index;
	int value;
	int step;
	struct motor_driver *motor = NULL;

	// old driver behavior is to power all coils, which doesn't really make sense
	// value = ((0 ^ invert_gpio_dir) & 0x1);

	for (index = 0; index < NUMBER_OF_MOTORS; index++) {
		motor = &mdev->motors[index];

		// apply torque, assuming cur_steps reflects actual position
		step = motor->cur_steps % 8;
		step = step < 0 ? step + 8 : step;
		value = invert_gpio_dir ? (step_8[step] ^ 0xff) : step_8[step];

		if (motor->pdata->motor_st1_gpio != -1)
			gpio_set_value(motor->pdata->motor_st1_gpio, value & 0x8);
		if (motor->pdata->motor_st2_gpio != -1)
			gpio_set_value(motor->pdata->motor_st2_gpio, value & 0x4);
		if (motor->pdata->motor_st3_gpio != -1)
			gpio_set_value(motor->pdata->motor_st3_gpio, value & 0x2);
		if (motor->pdata->motor_st4_gpio != -1)
			gpio_set_value(motor->pdata->motor_st4_gpio, value & 0x1);
	}
}

static void motor_set_default(struct motor_device *mdev)
{
	int index = 0;
	struct motor_driver *motor = NULL;
	mdev->dev_state = MOTOR_OPS_STOP;
	for(index = 0; index < NUMBER_OF_MOTORS; index++){
		motor =  &mdev->motors[index];
		motor->state = MOTOR_OPS_STOP;
	}

	/* Cut power to motor coils */
	motor_power_off(mdev);

	return;
}

static void motor_move_step(struct motor_device *mdev, int index)
{
	struct motor_driver *motor = NULL;
	int step;
	int value;

	motor =  &mdev->motors[index];
	if (motor->state != MOTOR_OPS_STOP) {
		step = motor->cur_steps % 8;
		step = step < 0 ? step + 8 : step;

		value = invert_gpio_dir ? (step_8[step] ^ 0xff) : step_8[step];

		if (motor->pdata->motor_st1_gpio != -1)
			gpio_set_value(motor->pdata->motor_st1_gpio, value & 0x8);
		if (motor->pdata->motor_st2_gpio != -1)
			gpio_set_value(motor->pdata->motor_st2_gpio, value & 0x4);
		if (motor->pdata->motor_st3_gpio != -1)
			gpio_set_value(motor->pdata->motor_st3_gpio, value & 0x2);
		if (motor->pdata->motor_st4_gpio != -1)
			gpio_set_value(motor->pdata->motor_st4_gpio, value & 0x1);
	} else {
		// power off the coils
		value = invert_gpio_dir ? 1 : 0;

		if (motor->pdata->motor_st1_gpio != -1)
			gpio_set_value(motor->pdata->motor_st1_gpio, value);
		if (motor->pdata->motor_st2_gpio != -1)
			gpio_set_value(motor->pdata->motor_st2_gpio, value);
		if (motor->pdata->motor_st3_gpio != -1)
			gpio_set_value(motor->pdata->motor_st3_gpio, value);
		if (motor->pdata->motor_st4_gpio != -1)
			gpio_set_value(motor->pdata->motor_st4_gpio, value);
	}
	if (motor->state == MOTOR_OPS_RESET) {
		motor->total_steps++;
	}

	return;
}

static void move_to_min_pose_ops(struct motor_driver *motor)
{
	if (motor->move_dir == MOTOR_MOVE_NEGATIVE) {
		if (motor->state == MOTOR_OPS_CRUISE) {
			// auto turn back
			motor->move_dir = MOTOR_MOVE_POSITIVE;
		} else if (motor->pdata->motor_min_gpio == -1) {
			motor->state = MOTOR_OPS_STOP;
		}

		// only update cur_steps if *hitting* this direction, ignore leaving
		if (motor->pdata->motor_min_gpio == -1)
			motor->cur_steps = 0;
	}
}

static void move_to_max_pose_ops(struct motor_driver *motor, int index)
{
	if (motor->move_dir == MOTOR_MOVE_POSITIVE) {
		if (motor->state == MOTOR_OPS_RESET && motor->pdata->motor_max_gpio == -1) {
			// soft endstop, apply preconfigured max_steps values and report completion
			motor->state = MOTOR_OPS_STOP;
			if (index == PAN_MOTOR)
				motor->max_steps = hmaxstep;
			else
				motor->max_steps = vmaxstep;
			complete(&motor->reset_completion);
		} else if (motor->state == MOTOR_OPS_CRUISE) {
			// auto turn back
			motor->move_dir = MOTOR_MOVE_NEGATIVE;
		} else if (motor->pdata->motor_max_gpio == -1) {
			motor->state = MOTOR_OPS_STOP;
		}

		// only update cur_steps if *hitting* this direction, ignore leaving
		if (motor->pdata->motor_max_gpio == -1)
			motor->cur_steps = motor->max_steps;
	}
}

static irqreturn_t jz_timer_interrupt(int irq, void *dev_id)
{
	struct motor_device *mdev = dev_id;
	struct motor_move *dst = &mdev->dst_move;
	struct motor_move *cur = &mdev->cur_move;
	struct motor_driver *motors = mdev->motors;
	struct step_spread_param *step_spread = &mdev->step_spread;
	int still_moving = 0;

	if(motors[PAN_MOTOR].state == MOTOR_OPS_STOP
			&& motors[TILT_MOTOR].state == MOTOR_OPS_STOP)
	{
		mdev->dev_state = MOTOR_OPS_STOP;
		motor_move_step(mdev, PAN_MOTOR);
		motor_move_step(mdev, TILT_MOTOR);

		if (mdev->wait_stop) {
			mdev->wait_stop = 0;
			complete(&mdev->stop_completion);
		}

		return IRQ_HANDLED;
	}

	/* Soft limits, also does auto turn back for cruise */
	if (motors[PAN_MOTOR].cur_steps <= 0)
		move_to_min_pose_ops(&motors[PAN_MOTOR]);
	if (motors[PAN_MOTOR].cur_steps >= motors[PAN_MOTOR].max_steps)
		move_to_max_pose_ops(&motors[PAN_MOTOR],PAN_MOTOR);
	if (motors[TILT_MOTOR].cur_steps <= 0)
		move_to_min_pose_ops(&motors[TILT_MOTOR]);
	if (motors[TILT_MOTOR].cur_steps >= motors[TILT_MOTOR].max_steps)
		move_to_max_pose_ops(&motors[TILT_MOTOR],TILT_MOTOR);

	if (mdev->dev_state == MOTOR_OPS_CRUISE) {
		motors[PAN_MOTOR].cur_steps += motors[PAN_MOTOR].move_dir;
		motors[TILT_MOTOR].cur_steps += motors[TILT_MOTOR].move_dir;
		motor_move_step(mdev, PAN_MOTOR);
		motor_move_step(mdev, TILT_MOTOR);
	} else {
		while (cur->times < dst->times) {
			if (cur->one.x < dst->one.x
				&& motors[PAN_MOTOR].state != MOTOR_OPS_STOP)
			{
				still_moving = 1;
				if (step_spread->factor_a >= step_spread->factor_b) {
					motors[PAN_MOTOR].cur_steps += motors[PAN_MOTOR].move_dir;
					cur->one.x++;
					motor_move_step(mdev, PAN_MOTOR);
				} else {
					step_spread->numerator += step_spread->factor_a;
					if (step_spread->numerator >= step_spread->factor_b) {
						step_spread->numerator -= step_spread->factor_b;
						motors[PAN_MOTOR].cur_steps += motors[PAN_MOTOR].move_dir;
						cur->one.x++;
						motor_move_step(mdev, PAN_MOTOR);
					}
				}
			}
			if (cur->one.y < dst->one.y
				&& motors[TILT_MOTOR].state != MOTOR_OPS_STOP)
			{
				still_moving = 1;
				if (step_spread->factor_a <= step_spread->factor_b) {
					motors[TILT_MOTOR].cur_steps += motors[TILT_MOTOR].move_dir;
					cur->one.y++;
					motor_move_step(mdev, TILT_MOTOR);
				} else {
					step_spread->numerator += step_spread->factor_b;
					if (step_spread->numerator >= step_spread->factor_a) {
						step_spread->numerator -= step_spread->factor_a;
						motors[TILT_MOTOR].cur_steps += motors[TILT_MOTOR].move_dir;
						cur->one.y++;
						motor_move_step(mdev, TILT_MOTOR);
					}
				}
			}

			if (still_moving) {
				still_moving = 0;
				break;
			} else {
				cur->one.x = 0;
				cur->one.y = 0;
				cur->times++;
				step_spread->numerator = 0;
			}
		}

		if (mdev->cur_move.times == mdev->dst_move.times
			&& mdev->cur_move.one.x == 0
			&& mdev->cur_move.one.y == 0)
		{
			motors[PAN_MOTOR].state = MOTOR_OPS_STOP;
			motors[TILT_MOTOR].state = MOTOR_OPS_STOP;
		}
	}

	return IRQ_HANDLED;
}

static void gpio_keys_min_timer(unsigned long _data)
{
	struct motor_driver *motor = (struct motor_driver *)_data;
	int value = 0;
    value = gpio_get_value(motor->pdata->motor_min_gpio);

	if(value == motor->pdata->motor_endstop_level){
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
			motor->move_dir = MOTOR_MOVE_POSITIVE;
		}else if(motor->state == MOTOR_OPS_NORMAL){
			if(motor->move_dir == MOTOR_MOVE_NEGATIVE){
				motor->state = MOTOR_OPS_STOP;
			}
		}else
			motor->move_dir = MOTOR_MOVE_POSITIVE;


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

	if(value == motor->pdata->motor_endstop_level){

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
			motor->move_dir = MOTOR_MOVE_NEGATIVE;
		}else if(motor->state == MOTOR_OPS_NORMAL){
			if(motor->move_dir == MOTOR_MOVE_POSITIVE){
				motor->state = MOTOR_OPS_STOP;
			}
		}else
			motor->move_dir = MOTOR_MOVE_NEGATIVE;
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

	/* check x value, use endstop if available */
	if (x > 0) {
		if (mdev->motors[PAN_MOTOR].pdata->motor_max_gpio != -1) {
			value = gpio_get_value(mdev->motors[PAN_MOTOR].pdata->motor_max_gpio);
			if(value == mdev->motors[PAN_MOTOR].pdata->motor_endstop_level)
				x = 0;
		} else {
			if (motors[PAN_MOTOR].cur_steps >= motors[PAN_MOTOR].max_steps)
				x = 0;
		}

	} else {
		if (mdev->motors[PAN_MOTOR].pdata->motor_min_gpio != -1) {
			value = gpio_get_value(mdev->motors[PAN_MOTOR].pdata->motor_min_gpio);
			if(value == mdev->motors[PAN_MOTOR].pdata->motor_endstop_level)
				x = 0;
		} else {
			if (motors[PAN_MOTOR].cur_steps <= 0)
				x = 0;
		}
	}
	/* check y value, use endstop if available */
	if (y > 0) {
		if (mdev->motors[TILT_MOTOR].pdata->motor_max_gpio != -1) {
			value = gpio_get_value(mdev->motors[TILT_MOTOR].pdata->motor_max_gpio);
			if(value == mdev->motors[TILT_MOTOR].pdata->motor_endstop_level)
				y = 0;
		} else {
			if (motors[TILT_MOTOR].cur_steps >= motors[TILT_MOTOR].max_steps)
				y = 0;
		}

	} else {
		if (mdev->motors[TILT_MOTOR].pdata->motor_min_gpio != -1) {
			value = gpio_get_value(mdev->motors[TILT_MOTOR].pdata->motor_min_gpio);
			if(value == mdev->motors[TILT_MOTOR].pdata->motor_endstop_level)
				y = 0;
		} else {
			if (motors[TILT_MOTOR].cur_steps <= 0)
				y = 0;
		}
	}

	// always set MOTOR_MOVE_POSITIVE or MOTOR_MOVE_NEGATIVE to keep torque during movement
	x_dir = x > 0 ? MOTOR_MOVE_POSITIVE : MOTOR_MOVE_NEGATIVE;
	y_dir = y > 0 ? MOTOR_MOVE_POSITIVE : MOTOR_MOVE_NEGATIVE;

	x1 = x < 0 ? 0 - x : x;
	y1 = y < 0 ? 0 - y : y;

	if ((x1 + y1) == 0) {
		return 0;
	} else if (x1 == 0) {
		times = 1;
	} else if (y1 == 0) {
		times = 1;
	} else {
		// to calculate proper diagonal speed
		times = calc_max_divisor(x1, y1);
	}

	x1 = x1 / times;
	y1 = y1 / times;

	mutex_lock(&mdev->dev_mutex);
	spin_lock_irqsave(&mdev->slock, flags);

	/* Enable motor GPIO outputs before movement */
	motor_power_on(mdev);

	mdev->dev_state = MOTOR_OPS_NORMAL;
	mdev->dst_move.one.x = x1;
	mdev->dst_move.one.y = y1;
	mdev->dst_move.times = times;
	mdev->cur_move.one.x = 0;
	mdev->cur_move.one.y = 0;
	mdev->cur_move.times = 0;

	/*
	 * A simple step spread algorithm:
	 * - find which axis has more steps to go, call it axis_a, the other axis_b
	 *     - factor_a = axis_a_steps + 1
	 *     - factor_b = axis_b_steps + 1
	 * - for each independent segment(time), the step count of axis_b is determined by:
	 *    floor( (current axis_a steps) * factor_b / factor_a )
	 *
	 * In practice, the multiplication and division are replaced by addition and comparision,
	 * due to the restrictions in the hard interrupt context.
	 * The bigger one in factor_a and factor_b becomes the real factor_a in the algorithm.
	 */
	mdev->step_spread.factor_a		= x1 + 1;
	mdev->step_spread.factor_b		= y1 + 1;
	mdev->step_spread.numerator		= 0;

	motors[PAN_MOTOR].state = MOTOR_OPS_NORMAL;
	motors[PAN_MOTOR].move_dir = x_dir;
	motors[TILT_MOTOR].state = MOTOR_OPS_NORMAL;
	motors[TILT_MOTOR].move_dir = y_dir;

	spin_unlock_irqrestore(&mdev->slock, flags);
	mutex_unlock(&mdev->dev_mutex);
	//printk("%s%d x=%d y=%d t=%d\n",__func__,__LINE__,mdev->dst_move.one.x,mdev->dst_move.one.y,mdev->dst_move.times);
	//printk("x_dir=%d,y_dir=%d\n",x_dir,y_dir);
	ingenic_tcu_counter_begin(mdev->tcu);

	return 0;
}

static void motor_ops_stop(struct motor_device *mdev)
{
	long ret;
	unsigned long flags;
	unsigned int remainder;

	struct motor_driver *motors = mdev->motors;
	struct motor_move *dst = &mdev->dst_move;
	struct motor_move *cur = &mdev->cur_move;

	if (mdev->dev_state == MOTOR_OPS_STOP)
		return;

	mutex_lock(&mdev->dev_mutex);
	spin_lock_irqsave(&mdev->slock, flags);

	// why a buffer distance is used here?
	if (mdev->dev_state == MOTOR_OPS_NORMAL) {
		remainder = dst->one.x - cur->one.x;
		if (remainder > 30) {
			dst->one.x = 29;
			cur->one.x = 0;
		}
		remainder = dst->one.y - cur->one.y;
		if (remainder > 8) {
			dst->one.y = 6;
			cur->one.y = 0;
		}
	}

	if (mdev->dev_state == MOTOR_OPS_CRUISE) {
		mdev->dev_state = MOTOR_OPS_NORMAL;
		motors[PAN_MOTOR].state = MOTOR_OPS_NORMAL;
		motors[TILT_MOTOR].state = MOTOR_OPS_NORMAL;
		dst->one.x = 0;
		cur->one.x = 0;
		dst->one.y = 0;
		cur->one.y = 0;
	}

	mdev->wait_stop = 1;
	spin_unlock_irqrestore(&mdev->slock, flags);
	mutex_unlock(&mdev->dev_mutex);

	do {
		ret = wait_for_completion_interruptible_timeout(&mdev->stop_completion, msecs_to_jiffies(15000));
		if (ret == 0) {
			ret = -ETIMEDOUT;
			break;
		}
	} while (ret == -ERESTARTSYS);

	ingenic_tcu_counter_stop(mdev->tcu);

	motor_set_default(mdev);

	return;
}

static long motor_ops_goback(struct motor_device *mdev)
{
	struct motor_driver *motors = mdev->motors;
	int sx, sy;
	int cx, cy;
	sx = motors[PAN_MOTOR].max_steps >> 1;
	sy = motors[TILT_MOTOR].max_steps >> 1;
	cx = motors[PAN_MOTOR].cur_steps;
	cy = motors[TILT_MOTOR].cur_steps;
	//printk("sx=%d,sy=%d,cx=%d,cy=%d\n",sx,sy,cx,cy);
	return motor_ops_move(mdev, sx-cx, sy-cy);
}

static long motor_ops_cruise(struct motor_device *mdev)
{
	struct motor_driver *motors = mdev->motors;
	unsigned long flags;

	motor_ops_goback(mdev);

	mutex_lock(&mdev->dev_mutex);
	spin_lock_irqsave(&mdev->slock, flags);

	/* Enable motor GPIO outputs before cruise */
	motor_power_on(mdev);

	mdev->dev_state = MOTOR_OPS_CRUISE;
	motors[PAN_MOTOR].state = MOTOR_OPS_CRUISE;
	motors[TILT_MOTOR].state = MOTOR_OPS_CRUISE;

	spin_unlock_irqrestore(&mdev->slock, flags);
	mutex_unlock(&mdev->dev_mutex);

	ingenic_tcu_counter_begin(mdev->tcu);
	return 0;
}

static void motor_get_message(struct motor_device *mdev, struct motor_message *msg)
{
	struct motor_driver *motors = mdev->motors;
	msg->x = motors[PAN_MOTOR].cur_steps;
	msg->y = motors[TILT_MOTOR].cur_steps;
	msg->speed = mdev->tcu_speed;
	if(mdev->dev_state == MOTOR_OPS_STOP)
		msg->status = MOTOR_IS_STOP;
	else
		msg->status = MOTOR_IS_RUNNING;

	/* This is not standard, return the max_steps of each motor */
	msg->x_max_steps = motors[PAN_MOTOR].max_steps;
	msg->y_max_steps = motors[TILT_MOTOR].max_steps;

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
		mdev->motors[PAN_MOTOR].max_steps = rdata->x_max_steps;
		mdev->motors[PAN_MOTOR].cur_steps = rdata->x_cur_step;
		mdev->motors[TILT_MOTOR].max_steps = rdata->y_max_steps;
		mdev->motors[TILT_MOTOR].cur_steps = rdata->y_cur_step;
		spin_unlock_irqrestore(&mdev->slock, flags);
		mutex_unlock(&mdev->dev_mutex);
	}else{
		/* driver calculate max steps. */

		mutex_lock(&mdev->dev_mutex);
		spin_lock_irqsave(&mdev->slock, flags);

		/* Enable motor GPIO outputs before reset/homing */
		motor_power_on(mdev);

		for (index = 0; index < NUMBER_OF_MOTORS; index++) {
			struct motor_driver *drv = &mdev->motors[index];
			int half = drv->max_steps > 0 ? drv->max_steps / 2 : 0;
			drv->move_dir = MOTOR_MOVE_POSITIVE;
			drv->state = MOTOR_OPS_RESET;
			drv->cur_steps = half > 0 ? half : 0;
		}
		mdev->dst_move.one.x = mdev->motors[PAN_MOTOR].max_steps;
		mdev->dst_move.one.y = mdev->motors[TILT_MOTOR].max_steps;
		mdev->dst_move.times = 1; // ensures final reset move is excuted in jz_timer_interrupt
		mdev->cur_move.one.x = 0;
		mdev->cur_move.one.y = 0;
		mdev->cur_move.times = 0;
		mdev->dev_state = MOTOR_OPS_RESET;

		// no step spread
		mdev->step_spread.factor_a = 1;
		mdev->step_spread.factor_b = 1;

		spin_unlock_irqrestore(&mdev->slock, flags);
		mutex_unlock(&mdev->dev_mutex);
		ingenic_tcu_counter_begin(mdev->tcu);

		for(index = 0; index < NUMBER_OF_MOTORS; index++){
			do{
				ret = wait_for_completion_interruptible_timeout(&mdev->motors[index].reset_completion, msecs_to_jiffies(150000));
				if(ret == 0){
					ret = -ETIMEDOUT;
					goto exit;
				}
			}while(ret == -ERESTARTSYS);
		}
	}

	/*
	 * At this point 2 motors *think* they just reached their max mechanical limits,
	 * while they can be either blocked by the limit, or just didn't reach it.
	 * Run a reverse move back to the origin so max step counts reflect
	 * the total travel range, then finish at center via motor_ops_goback().
	 */
	ret = motor_ops_move(mdev,
			     -mdev->motors[PAN_MOTOR].cur_steps,
			     -mdev->motors[TILT_MOTOR].cur_steps);

	do {
		msleep(10);
		motor_get_message(mdev, &msg);
		times++;
		if (times > 1000) {
			printk("ERROR:wait motor timeout %s%d\n", __func__, __LINE__);
			ret = -ETIMEDOUT;
			goto exit;
		}
	} while (msg.status == MOTOR_IS_RUNNING);

	//printk("x_max = %d, y_max = %d\n", mdev->motors[PAN_MOTOR].max_steps,
			//mdev->motors[TILT_MOTOR].max_steps);
	ret = motor_ops_goback(mdev);
	/*ret =  motor_ops_move(mdev, (mdev->motors[PAN_MOTOR].max_steps) >> 1, */
			/*(mdev->motors[TILT_MOTOR].max_steps) >> 1);*/

	if (ret)
		goto exit;

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
	rdata->x_max_steps	= mdev->motors[PAN_MOTOR].max_steps;
	rdata->x_cur_step	= mdev->motors[PAN_MOTOR].cur_steps;
	rdata->y_max_steps	= mdev->motors[TILT_MOTOR].max_steps;
	rdata->y_cur_step	= mdev->motors[TILT_MOTOR].cur_steps;

exit:
	ingenic_tcu_counter_stop(mdev->tcu);
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
	ingenic_tcu_set_period(mdev->tcu->cib.id,(24000000 / 64 / mdev->tcu_speed));
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

	if (mdev->flag == 0) {
		dev_err(mdev->dev, "Please open /dev/motor before issuing commands.\n");
		return -EPERM;
	}

	switch (cmd) {
		case MOTOR_STOP:
			motor_ops_stop(mdev);
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

	seq_printf(m ,"The version of Motor driver is %s\n",JZ_MOTOR_DRIVER_VERSION);
	seq_printf(m ,"Motor driver is %s\n", mdev->flag?"opened":"closed");
	seq_printf(m ,"The max speed is %d and the min speed is %d\n", MOTOR_MAX_SPEED, MOTOR_MIN_SPEED);
	motor_get_message(mdev, &msg);
	seq_printf(m ,"The status of motor is %s\n", msg.status?"running":"stop");
	seq_printf(m ,"The pos of motor is (%d, %d)\n", msg.x, msg.y);
	seq_printf(m ,"The speed of motor is %d\n", msg.speed);

	for(index = 0; index < NUMBER_OF_MOTORS; index++){
		seq_printf(m ,"## motor is %s ##\n", mdev->motors[index].pdata->name);
		seq_printf(m ,"max steps %d\n", mdev->motors[index].max_steps);
		seq_printf(m ,"motor direction %d\n", mdev->motors[index].move_dir);
		seq_printf(m ,"motor state %d(normal; cruise; reset)\n", mdev->motors[index].state);
		seq_printf(m ,"the irq's counter of max pos is %d\n", mdev->motors[index].max_pos_irq_cnt);
		seq_printf(m ,"the irq's counter of min pos is %d\n", mdev->motors[index].min_pos_irq_cnt);
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
	mdev->tcu = (struct ingenic_tcu_chn *)mdev->cell->platform_data;

	{
		int ch = mdev->tcu->cib.id;
		ret = tcu_alloc_claim(ch, "motor");
		if (ret) {
			const char *own = tcu_alloc_owner(ch);
			dev_err(&pdev->dev, "TCU ch%d busy (owner=%s), motor refusing to bind: %d\n", ch, own ? own : "unknown", ret);
			goto error_devm_kzalloc;
		}
	}

	mdev->tcu->irq_type = FULL_IRQ_MODE;
	mdev->tcu->clk_src = TCU_CLKSRC_EXT;
	mdev->tcu_speed = MOTOR_DEF_SPEED;
	mdev->tcu->is_pwm = 0;
	mdev->tcu->cib.func = TRACKBALL_FUNC;
	mdev->tcu->clk_div = TCU_PRESCALE_64;
	ingenic_tcu_config(mdev->tcu);
	ingenic_tcu_set_period(mdev->tcu->cib.id,(24000000 / 64 / mdev->tcu_speed));
	//ingenic_tcu_counter_begin(mdev->tcu);
	mutex_init(&mdev->dev_mutex);
	spin_lock_init(&mdev->slock);

	platform_set_drvdata(pdev, mdev);

	/* copy module parameters to the motors struct */
	motors_pdata[0].motor_st1_gpio = hst1;
	motors_pdata[0].motor_st2_gpio = hst2;
	motors_pdata[0].motor_st3_gpio = hst3;
	motors_pdata[0].motor_st4_gpio = hst4;
	motors_pdata[0].motor_min_gpio = hmin_gpio;
	motors_pdata[0].motor_max_gpio = hmax_gpio;
	motors_pdata[0].motor_endstop_level = hlevel;

	motors_pdata[1].motor_st1_gpio = vst1;
	motors_pdata[1].motor_st2_gpio = vst2;
	motors_pdata[1].motor_st3_gpio = vst3;
	motors_pdata[1].motor_st4_gpio = vst4;
	motors_pdata[1].motor_min_gpio = vmin_gpio;
	motors_pdata[1].motor_max_gpio = vmax_gpio;
	motors_pdata[1].motor_endstop_level = vlevel;

	if (hmax == -1)
		hmax = hmaxstep;
	if (vmax == -1)
		vmax = vmaxstep;

	// have no switch support yet
	// motors_pdata[0].motor_switch_gpio = motor_switch_gpio;

	for(i = 0; i < NUMBER_OF_MOTORS; i++) {
		motor = &(mdev->motors[i]);
		motor->pdata = &motors_pdata[i];
		motor->move_dir	= MOTOR_MOVE_STOP;
		init_completion(&motor->reset_completion);

		if (motor->pdata->motor_min_gpio != -1) {
			gpio_request(motor->pdata->motor_min_gpio, "motor_min_gpio");
			motor->min_pos_irq = gpio_to_irq(motor->pdata->motor_min_gpio);

			ret = request_irq(motor->min_pos_irq,
					motor_min_gpio_interrupt,
					(motor->pdata->motor_endstop_level ?
					 IRQF_TRIGGER_RISING :
					 IRQF_TRIGGER_FALLING)
					| UMH_DISABLED ,
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
					(motor->pdata->motor_endstop_level ?
					 IRQF_TRIGGER_RISING :
					 IRQF_TRIGGER_FALLING)
					| UMH_DISABLED ,
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

	mdev->motors[PAN_MOTOR].max_steps = hmaxstep;
	mdev->motors[TILT_MOTOR].max_steps = vmaxstep;

	/*
	 * Seed the logical position at mid travel so the very first
	 * MOTORS_MOVE request can safely step in the negative direction.
	 * Otherwise the guard in motor_ops_move() treats cur_steps == 0 as
	 * already at the minimum stop and clamps any backward motion, which
	 * prevents the initial two-way homing sweep from ever starting.
	 */
	if (mdev->motors[PAN_MOTOR].max_steps > 0) {
		int half = mdev->motors[PAN_MOTOR].max_steps / 2;
		mdev->motors[PAN_MOTOR].cur_steps = half > 0 ? half : 1;
	}
	if (mdev->motors[TILT_MOTOR].max_steps > 0) {
		int half = mdev->motors[TILT_MOTOR].max_steps / 2;
		mdev->motors[TILT_MOTOR].cur_steps = half > 0 ? half : 1;
	}

	ingenic_tcu_channel_to_virq(mdev->tcu);
	mdev->run_step_irq = mdev->tcu->virq[0];
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

	motor_set_default(mdev);
	mdev->flag = 0;
	ingenic_tcu_counter_begin(mdev->tcu);

	//printk("%s%d\n",__func__,__LINE__);
	return 0;

error_misc_register:
	free_irq(mdev->run_step_irq, mdev);
error_request_irq:
error_get_irq:
error_max_gpio:
error_min_gpio:
	for(i = 0; i < NUMBER_OF_MOTORS; i++) {
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
	int ch = mdev->tcu->cib.id;

	ingenic_tcu_counter_stop(mdev->tcu);

	/* Release ownership */
	tcu_alloc_release(ch, "motor");

	mutex_destroy(&mdev->dev_mutex);

	free_irq(mdev->run_step_irq, mdev);
	for(i = 0; i < NUMBER_OF_MOTORS; i++) {
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

static const struct of_device_id motor_ingenic_tcu_ids[] = {
	{.compatible = "ingenic,tcu_chn2",},
	{}
};

static struct platform_driver motor_driver = {
	.probe = motor_probe,
	.remove = motor_remove,
	.driver = {
		.name = motor_driver_name,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(motor_ingenic_tcu_ids),
	}
};

static int __init motor_init(void)
{
	/* Parse CSV channels and configure dynamic binding name based on first channel */
	motor_parse_channels();
	snprintf(motor_driver_name, sizeof(motor_driver_name), "tcu_chn%d", motor_bind_channel);

	/* Update compatible field */
	snprintf((char *)motor_ingenic_tcu_ids[0].compatible, sizeof(motor_ingenic_tcu_ids[0].compatible), "ingenic,tcu_chn%d", motor_bind_channel);

	return platform_driver_register(&motor_driver);
}

static void __exit motor_exit(void)
{
	platform_driver_unregister(&motor_driver);
}

module_init(motor_init);
module_exit(motor_exit);

MODULE_LICENSE("GPL");

