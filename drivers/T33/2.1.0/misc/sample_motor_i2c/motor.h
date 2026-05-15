/*
 * Copyright (C) 2015 Ingenic Semiconductor Co.,Ltd
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

#ifndef __I2C_MOTOR_H__
#define __I2C_MOTOR_H__

enum motor_tcu_chn;

#define I2CID 0x02	/* The node corresponding to i2c defaults to using i2c-2 */
#define PWM_CHN TCU_CHN2	/* The channel corresponding to PWM, Junzheng PRJ007 defaults to using TCU_CN2 to provide frequency */
unsigned int ms32006_fclk = 24000000;//Chip input clock selection, this parameter is related to the motion speed. Junzheng T series chips provide a frequency of 24MHz through gpio


#define I2C_MOTOR_STOP   	_IOWR('m', 1, jz_motor_param_t)
#define I2C_MOTOR_RUN  		_IOWR('m', 2, jz_motor_param_t)
#define I2C_MOTOR_PAUSE   	_IOWR('m', 3, jz_motor_param_t)
#define I2C_MOTOR_RESET  	_IOWR('m', 4, jz_motor_param_t)
#define I2C_MOTOR_DEBUG		_IOWR('m', 5, jz_motor_param_t)
#define I2C_MOTOR_IRCUT  	_IOWR('m', 6, jz_motor_param_t)

typedef enum {
	TCU_CHN0,
	TCU_CHN1,
	TCU_CHN2,
	TCU_CHN3,
	TCU_BUTT,
}motor_tcu_chn;

typedef enum motornum {
	MOTOR_DFT,
	MOTOR_1,
	MOTOR_2,
	MOTOR_BUTT,
}motor_num;

typedef enum motorruntype {
	MOTOR_HALF,
	MOTOR_FULL,
}motor_run_type;

typedef enum {
	MOTOR_2P_4W,	/*Two phases and four lines*/
	MOTOR_4P_5W,	/*Four phases and five lines*/
}motor_sel;

typedef enum {
	MOTOR_IRCUT_CLOSE,	/*IRCUT High resistance state*/
	MOTOR_IRCUT_CW,		/*IRCUT corotation*/
	MOTOR_IRCUT_CCW,	/*IRCUT reversal*/
}motor_ircut_type;

typedef enum motordirtype
{
	MOTOR_CW,	/* Clockwise rotation */
	MOTOR_CCW,	/* Reverse counterclockwise */
}motor_dir_type;

typedef struct {
	unsigned char bhstate;
	unsigned char chstate;
	unsigned char dhstate;
	unsigned char ehstate;
}motor_dbg_state_t;

typedef struct motor_move {
	motor_dir_type mot1_dir; 	//Motor rotation direction
	unsigned int mot1_pps;		//Pulse per second, used to calculate the rotational speed of the motor
	unsigned int mot1_step;		//Motor step size

	motor_dir_type mot2_dir;
	unsigned int mot2_pps;
	unsigned int mot2_step;
}move_param_t;

typedef struct i2c_motor_param {
	motor_num motor;
	motor_run_type type;
	motor_sel mts;
	move_param_t move;
	motor_ircut_type irctype;
	motor_dbg_state_t mrs; /*motor run status*/
}jz_motor_param_t;


#endif // __MOTOR_H__
