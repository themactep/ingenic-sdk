#ifndef __MOTOR_H__
#define __MOTOR_H__

#include <linux/proc_fs.h>
#include <jz_proc.h>
//********************************************* USER Config
#define STOP_GPIO_LEVEL 0    //All GPIO level states when the motor stops running
#define MOTOR_DEF_SPEED 500  //Maximum speed of motor
#define MOTOR_GEAR 1         //Motor speed gear

#define MOTOR0_MAX_STEP 4000 //Maximum step range of motor motion
#define MOTOR1_MAX_STEP 1000

#define MOTOR_LIMIT_LEVEL 0 //Trigger limit level

#define MAGNETIC_LOCK 0

//motor 0
#define MOTOR0_WIRE_1 GPIO_PC(0)
#define MOTOR0_WIRE_2 GPIO_PC(1)
#define MOTOR0_WIRE_3 GPIO_PC(2)
#define MOTOR0_WIRE_4 GPIO_PC(3)

#define MOTOR0_LIMIT_MIN GPIO_PC(8) //Starting point limit
#define MOTOR0_LIMIT_MAX GPIO_PC(9) //End limit

//motor 1
#define MOTOR1_WIRE_1 GPIO_PC(4)
#define MOTOR1_WIRE_2 GPIO_PC(5)
#define MOTOR1_WIRE_3 GPIO_PC(6)
#define MOTOR1_WIRE_4 GPIO_PC(7)

#define MOTOR1_LIMIT_MIN GPIO_PC(10)
#define MOTOR1_LIMIT_MAX GPIO_PC(11)
//********************************************* USER Config END

//********************************************* commands
#define MOTOR_STOP 0 //Stop the motor rotation
// #define MOTOR_START 1    //Start the motor rotation
#define MOTOR_STEPS 2	   //Rotating motor, relative coordinates, positive and negative parameters represent direction
#define MOTOR_COORDINATE 3 //Rotating motor, absolute coordinates
#define MOTOR_GET_STATUS 4 //Obtain the current coordinates, target coordinates, gear position, and speed of the motor
#define MOTOR_SET_SPEED 5  //Set motor speed
#define MOTOR_RESET 6	   //Motor reset calibration
#define MOTOR_NO_RESET 7   //Skip the reset step and directly specify the current coordinates
//********************************************* CMD END

#define MOTOR_NUMBER 2	 //number of motors
#define MOTOR_WIRE_NUM 4 //Number of motor wires

#define MOTOR_MAX_SPEED 1000
#define MOTOR_MIN_SPEED 100

#define CON_WIRE(pin, n) gpio_set_value(pin, n)

enum enMotorDirection
{
	MOTOR_NEGATIVE = -1, //negative direction
	MOTOR_STOP_MID = 0,
	MOTOR_POSITIVE = 1, //positive direction
};

struct SysStatus
{
	unsigned char irq_status;
	unsigned char reset_status; //Reset state
#define RESET_NO 0				//not reset 
#define RESET_ING 1				//Resetting calibration
#define RESET_END 2				//Reset calibration completed
	// unsigned int steps_sum[MOTOR_NUMBER];
};

struct stMotorControl
{
	int coordinate[MOTOR_NUMBER];	//Current absolute coordinates of the motor
	int target_coord[MOTOR_NUMBER]; //Target coordinates
	int maxstep[MOTOR_NUMBER];		  //Maximum number of steps
	unsigned char gear[MOTOR_NUMBER]; //Gear position
	unsigned char wire[MOTOR_NUMBER][MOTOR_WIRE_NUM];
	unsigned char limit[MOTOR_NUMBER][2];
	unsigned char motor_status[MOTOR_NUMBER];
	unsigned char tcu_n;
};

//Receive absolute coordinates and target coordinates
struct stTargetCoord
{
	int coordinate[MOTOR_NUMBER];
};
//Receive relative coordinates and rotation steps
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
	int coordinate[MOTOR_NUMBER];	  //Current absolute coordinates of the motor
	int target_coord[MOTOR_NUMBER];	  //Target coordinates
	unsigned int speed;				  //maximum speed
	unsigned char gear[MOTOR_NUMBER]; //Gear position
	unsigned char reset_status;		  //Reset state
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
