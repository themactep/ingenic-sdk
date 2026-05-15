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

#define I2CID 0x01	/*i2c对应的节点,默认使用i2c-1*/
#define PWM_CHN TCU_CHN2	/*pwm 对应的通道,君正T23默认使用TCU_CHN2提供频率*/
unsigned int ms32006_fclk = 24000000;//芯片输入时钟选择，此参数与运动速度有关,君正T系列芯片通过gpio提供一个24mhz的频率


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
	MOTOR_2P_4W,	/*两相四线*/
	MOTOR_4P_5W,	/*四相五线*/
}motor_sel;

typedef enum {
	MOTOR_IRCUT_CLOSE,	/*IRCUT 高阻态*/
	MOTOR_IRCUT_CW,		/*IRCUT 正转*/
	MOTOR_IRCUT_CCW,	/*IRCUT 反转*/
}motor_ircut_type;

typedef enum motordirtype
{
	MOTOR_CW,	/*顺时针正转*/
	MOTOR_CCW,	/*逆时针反转*/
}motor_dir_type;

typedef struct {
	unsigned char bhstate;
	unsigned char chstate;
	unsigned char dhstate;
	unsigned char ehstate;
}motor_dbg_state_t;

typedef struct motor_move {
	motor_dir_type mot1_dir; 	//电机转动方向
	unsigned int mot1_pps;		//每秒脉冲数，用于计算电机转动速度
	unsigned int mot1_step;		//电机步长

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
