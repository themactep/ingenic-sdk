#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "motor.h"

int main()
{
    int fd;

	jz_motor_param_t motor_param_1 = {0};
	motor_param_1.motor = MOTOR_1;
	motor_param_1.type = MOTOR_HALF;
	motor_param_1.move.mot1_dir = MOTOR_CW;
	motor_param_1.move.mot1_pps = 500;//(10~5859)
	motor_param_1.move.mot1_step = 4095;//full:2047  half:4095
	motor_param_1.mts = MOTOR_4P_5W;

	jz_motor_param_t motor_param_2 = {0};
	motor_param_2.motor = MOTOR_2;
	motor_param_2.type = MOTOR_HALF;
	motor_param_2.move.mot2_dir = MOTOR_CCW;
	motor_param_2.move.mot2_pps = 500;//(10~5859)
	motor_param_2.move.mot2_step = 4000;//full:2047  half:4095
	motor_param_2.mts = MOTOR_4P_5W;

    // 打开设备节点文件
    fd = open("/dev/i2c_motor", O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

	// reset
	if (ioctl(fd, I2C_MOTOR_RESET, &motor_param_1) < 0) {
		perror("ioctl: I2C_MOTOR_RESET");
		close(fd);
		exit(1);
	}
	if (ioctl(fd, I2C_MOTOR_RESET, &motor_param_2) < 0) {
		perror("ioctl: I2C_MOTOR_RESET");
		close(fd);
		exit(1);
	}

    // 向设备发送命令
    if (ioctl(fd, I2C_MOTOR_RUN, &motor_param_1) < 0) {
        perror("ioctl: I2C_MOTOR_RUN");
        close(fd);
        exit(1);
    }
	if (ioctl(fd, I2C_MOTOR_RUN, &motor_param_2) < 0) {
        perror("ioctl: I2C_MOTOR_RUN");
        close(fd);
        exit(1);
    }

	motor_param_1.irctype = MOTOR_IRCUT_CW;
	if (ioctl(fd, I2C_MOTOR_IRCUT, &motor_param_1) < 0) {
			perror("ioctl: I2C_MOTOR_RESET");
			close(fd);
			exit(1);
	}

	motor_param_1.irctype = MOTOR_IRCUT_CCW;
	if (ioctl(fd, I2C_MOTOR_IRCUT, &motor_param_1) < 0) {
			perror("ioctl: I2C_MOTOR_RESET");
			close(fd);
			exit(1);
	}

	motor_param_1.irctype = MOTOR_IRCUT_CLOSE;
	if (ioctl(fd, I2C_MOTOR_IRCUT, &motor_param_1) < 0) {
			perror("ioctl: I2C_MOTOR_RESET");
			close(fd);
			exit(1);
	}
	sleep(2);

	if (ioctl(fd, I2C_MOTOR_DEBUG, &motor_param_1)< 0) {
		perror("ioctl: I2C_MOTOR_DEBUG");
		close(fd);
		exit(1);
	}

    // 从设备接收数据
    printf("start to pause\n");
    if (ioctl(fd, I2C_MOTOR_PAUSE, &motor_param_1)< 0) {
        perror("ioctl: I2C_MOTOR_PAUSE");
        close(fd);
        exit(1);
    }
	if (ioctl(fd, I2C_MOTOR_PAUSE, &motor_param_2)< 0) {
		perror("ioctl: I2C_MOTOR_PAUSE");
		close(fd);
		exit(1);
	}

	printf("start to stop\n");
    if (ioctl(fd, I2C_MOTOR_STOP, &motor_param_1) < 0) {
        perror("ioctl: I2C_MOTOR_STOP");
        close(fd);
        exit(1);
	}
	if (ioctl(fd, I2C_MOTOR_STOP, &motor_param_2) < 0) {
        perror("ioctl: I2C_MOTOR_STOP");
        close(fd);
        exit(1);
	}

	printf("start to reset\n");
	if (ioctl(fd, I2C_MOTOR_RESET, &motor_param_1) < 0) {
        perror("ioctl: I2C_MOTOR_RESET");
        close(fd);
        exit(1);
	}
	if (ioctl(fd, I2C_MOTOR_RESET, &motor_param_2) < 0) {
		perror("ioctl: I2C_MOTOR_RESET");
		close(fd);
		exit(1);
	}

    // 关闭设备节点文件
    close(fd);

    return 0;
}
