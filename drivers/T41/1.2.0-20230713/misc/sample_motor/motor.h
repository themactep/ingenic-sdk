#ifndef __MOTOR_H__
#define __MOTOR_H__

#include <linux/proc_fs.h>
#include <jz_proc.h>
//********************************************* USER Config
#define STOP_GPIO_LEVEL 0   //电机停转时所有GPIO电平状态
#define MOTOR_DEF_SPEED 600 //电机最大速度，即驱动电机方波频率
#define MOTOR_GEAR 1        //电机速度档位

#define MOTOR0_MAX_STEP 700 //电机运动最大步数范围
#define MOTOR1_MAX_STEP 6000

#define MAGNETIC_LOCK 0
//电机0
#define MOTOR0_WIRE_1 GPIO_PD(9)
#define MOTOR0_WIRE_2 GPIO_PD(10)
#define MOTOR0_WIRE_3 GPIO_PD(11)
#define MOTOR0_WIRE_4 GPIO_PD(12)
//电机1
#define MOTOR1_WIRE_1 GPIO_PD(13)
#define MOTOR1_WIRE_2 GPIO_PD(14)
#define MOTOR1_WIRE_3 GPIO_PD(15)
#define MOTOR1_WIRE_4 GPIO_PD(16)
//********************************************* USER Config END

//********************************************* commands
#define MOTOR_STOP 0 //停止电机转动
// #define MOTOR_START 1    //电机启动
#define MOTOR_STEPS 2	   //转动电机，相对坐标，参数的正负代表方向
#define MOTOR_COORDINATE 3 //转动电机，绝对坐标
#define MOTOR_GET_STATUS 4 //获取电机当前坐标、目标坐标、档位、速度
#define MOTOR_SET_SPEED 5  //设置电机速度
#define MOTOR_RESET 6	   //电机复位校准
#define MOTOR_NO_RESET 7   //跳过复位步骤，直接指定当前坐标
//********************************************* CMD END

#define MOTOR_NUMBER 2	 //电机数量
#define MOTOR_WIRE_NUM 4 //电机线数

#define MOTOR_MAX_SPEED 800
#define MOTOR_MIN_SPEED 200

#define CON_WIRE(pin, n) gpio_set_value(pin, n)

enum enMotorDirection
{
	MOTOR_NEGATIVE = -1, //负方向
	MOTOR_STOP_MID = 0,
	MOTOR_POSITIVE = 1, //正方向
};

struct SysStatus
{
	unsigned char irq_status;
	unsigned char reset_status; //复位状态
#define RESET_NO 0				//未复位
#define RESET_ING 1				//正在复位校准
#define RESET_END 2				//复位校准结束
	unsigned int steps_sum[MOTOR_NUMBER];
};

struct stMotorControl
{
	int coordinate[MOTOR_NUMBER];	//电机当前绝对坐标
	int target_coord[MOTOR_NUMBER]; //目标坐标
	int maxstep[MOTOR_NUMBER];		  //最大步数
	unsigned char gear[MOTOR_NUMBER]; //档位
	unsigned char wire[MOTOR_NUMBER][MOTOR_WIRE_NUM];
	unsigned char tcu_n;
};

//接收绝对坐标、目标坐标
struct stTargetCoord
{
	int coordinate[MOTOR_NUMBER];
};
//接收相对坐标、转动步数
struct stRotationSteps
{
	int step[MOTOR_NUMBER];
};

struct stMotorSpeed
{
	unsigned short speed;
	unsigned char gear[MOTOR_NUMBER];
};

struct stMotorStatus
{
	int coordinate[MOTOR_NUMBER];	  //电机当前绝对坐标
	int target_coord[MOTOR_NUMBER];	  //目标坐标
	unsigned int speed;				  //最大速度
	unsigned char gear[MOTOR_NUMBER]; //档位
	unsigned char reset_status;		  //复位状态
};

// Parameter check
#if ((MOTOR_DEF_SPEED > MOTOR_MAX_SPEED) || (MOTOR_DEF_SPEED < MOTOR_MIN_SPEED))
#error "Motor speed error !!!"
#endif

#if ((MOTOR_GEAR > 10) || (MOTOR_GEAR < 1))
#error "Motor gear error !!!"
#endif

#if MOTOR_NUMBER != 2
#error "Motor number error !!!"
#endif
#endif // __MOTOR_H__ END
