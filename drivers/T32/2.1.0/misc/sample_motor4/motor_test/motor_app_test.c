#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <error.h>
#include "../tmi8152_spi_dev.h"

int main(int argc, char *argv[])
{
	int fd;
	char key;
	spi_motor_param_t param;
	memset(&param,0,sizeof(param));

	//1、配置通道、细分和前进模式
	param.motor_num = MOTOR_1;
	param.motor_config.ch_stepmd = TMI8152_STEPMD_128;//细分越大，速度越慢
	param.motor_config.rotation_dir = MOTOR_DRV_CW;

	//2.配置时钟分频通道
	param.motor_config.sysclk_div = TMI8152_CDIV_4; //注意过小的分频可能导致电机不转动，电机物理特性决定，建议设置为TMI8152_CDIV_4
	param.motor_config.clk_csel = TMI8152_CSEL_1;

	//3.配置模式，并设置目标圈数
	param.motor_config.control_mode = MOTOR_MANUAL;//手动模式，

	/*这个是要运行的步数，根据步进电机规格书进行计算，如w-24BYJ-DC 电机，步距角为（5.625°/64）,减速比为1/64，驱动方式为1-2相励磁
	则转一圈需要：360°/(5.625°/64)/(拍数，4相电机1-2相励磁拍数为8)=512*/
	param.motor_config.circle_set = 512*2;//设置转动2圈


	fd = open("/dev/motor", O_RDWR);
	if(fd < 0) {
		printf("open err\n");
		return -1;
	}

	ioctl(fd,SPI_MOTOR_RUN,&param);
	while(1){
		sleep(1);
		printf("press q to quit\n");
		scanf("%c",&key);
		if('q' == key){
			printf("wait exit\n");
			break;
		}
	}

	ioctl(fd,SPI_MOTOR_IRCUT,&param);
#if 0
	ioctl(fd,SPI_MOTOR_PAUSE,&param);
	sleep(1);
	ioctl(fd,SPI_MOTOR_RESET,&param);
	ioctl(fd,SPI_MOTOR_RUN,&param);
	sleep(2);
#endif
	ioctl(fd,SPI_MOTOR_STOP,&param);

	close(fd);
	return 0;
}

