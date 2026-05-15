#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>

#define MOTOR_STOP 0	   //Stop the motor rotation
#define MOTOR_STEPS 2	   //Rotating motor, relative coordinates
#define MOTOR_COORDINATE 3 //Rotating motor, absolute coordinates
#define MOTOR_GET_STATUS 4 //Obtain the current coordinates, target coordinates, gear position, and speed of the motor
#define MOTOR_SET_SPEED 5  //Set motor speed
#define MOTOR_RESET 6	   //Motor reset calibration

#define MOTOR_NUMBER 2 //number of motors

#define RESET_NO 0	//Not reset
#define RESET_ING 1 //Resetting calibration
#define RESET_END 2 //Reset calibration completed

struct stMotorSpeed
{
	unsigned short speed;
	unsigned char gear[MOTOR_NUMBER];
};

struct stTargetCoord
{
	int coordinate[MOTOR_NUMBER];
};

struct stRotationSteps
{
	int step[MOTOR_NUMBER];
};

struct stMotorStatus
{
	int coordinate[MOTOR_NUMBER];    //Current absolute coordinates of the motor
	int target_coord[MOTOR_NUMBER];  //Target coordinates
	unsigned int speed;              //maximum speed
	unsigned char gear[MOTOR_NUMBER];//Gear position
	unsigned char reset_status;		 //Reset state
};
struct stMotorStatus motor_status;
struct stTargetCoord target_coord;
struct stRotationSteps rotation_step;
struct stMotorSpeed motor_speed;

int main(int argc, char *argv[])
{
	int fd;
	int n;
	char ch;
	char *path = "/dev/motor";

	if ((fd = open(path, O_RDWR)) < 0)
	{
		printf("open %s failed:%s\n", path, strerror(errno));
		return -1;
	}

	while (1)
	{
		printf("\nSTOP:0  STEPS:2  COORDINATE:3  GET_STATUS:4  SET_SPEED:5  RESET:6  NO_RESET:7 Exit:q\n");
		printf("Choose:");
		scanf(" %c", &ch);
		switch (ch)
		{
		case '0': //MOTOR_STOP
			ioctl(fd, MOTOR_STOP, 0);
			break;
		case '2': //MOTOR_STEPS
			printf("Input(step0,step1):");
			scanf("%d,%d", &rotation_step.step[0], &rotation_step.step[1]);
			ioctl(fd, MOTOR_STEPS, &rotation_step);
			break;
		case '3': //MOTOR_COORDINATE
			printf("Input(target_coord0,target_coord1):");
			scanf("%d,%d", &target_coord.coordinate[0], &target_coord.coordinate[1]);
			ioctl(fd, MOTOR_COORDINATE, (unsigned long)&target_coord);
			break;
		case '4': //MOTOR_GET_STATUS
			ioctl(fd, MOTOR_GET_STATUS, (unsigned long)&motor_status);
			printf("Rest:%d\n", motor_status.reset_status);
			printf("speed=%u,gear0=%u,gear1=%u\n", motor_status.speed, motor_status.gear[0], motor_status.gear[1]);
			printf("coord0=%u,target0=%u,coord1=%u,target1=%u\n", motor_status.coordinate[0], motor_status.target_coord[0], motor_status.coordinate[1], motor_status.target_coord[1]);
			break;
		case '5': //MOTOR_SET_SPEED
			printf("Input(speed,gear0,gear1):");
			scanf("%u,%u,%u", &motor_speed.speed, &motor_speed.gear[0], &motor_speed.gear[1]);
			printf("speed,gear0,gear1:%u,%u,%u\n", motor_speed.speed, motor_speed.gear[0], motor_speed.gear[1]);
			ioctl(fd, MOTOR_SET_SPEED, (unsigned long)&motor_speed);
			break;
		case '6': //MOTOR_RESET
			ioctl(fd, MOTOR_RESET, 0);
			break;
		case '7': // MOTOR_NO_RESET
			printf("Input(target_coord0,target_coord1):");
			scanf("%d,%d", &target_coord.coordinate[0], &target_coord.coordinate[1]);
			ioctl(fd, MOTOR_COORDINATE, (unsigned long)&target_coord);
		case 'q': //Exit
			close(fd);
			printf("Exit\n");
			return 0;
			break;
		}
	}

	printf("END\n");
	close(fd);
	return 0;
}

#if MOTOR_NUMBER != 2
#error "Motor number error !!!"
#endif

