/*
 * sample_sinfo.c
 *
 * two ways to get sensor info
 *
 * 1. open /dev/sinfo; ioctl TOCTL_SINFO_GET
 *
 * 2. echo 1 >/proc/jz/sinfo/info; cat /proc/jz/sinfo/info
 *
 * */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#define SENSOR_INFO_IOC_MAGIC  'S'
#define IOCTL_SINFO_GET			_IO(SENSOR_INFO_IOC_MAGIC, 100)
#define IOCTL_SINFO_FLASH		_IO(SENSOR_INFO_IOC_MAGIC, 101)

#define SENSOR_TYPE_INVALID	-1

enum SENSOR_TYPE
{
	SENSOR_TYPE_GC2053=0,
	SENSOR_TYPE_SC301IOT,
	SENSOR_TYPE_GC1084,
	SENSOR_TYPE_SC1346,
	SENSOR_TYPE_SC1A4T,
	SENSOR_TYPE_SC200AI,
	SENSOR_TYPE_SC2310,
	SENSOR_TYPE_SC2331,
	SENSOR_TYPE_SC2336,
	SENSOR_TYPE_SC2336P,
	SENSOR_TYPE_JXF23,
	SENSOR_TYPE_JXF37PA,
	SENSOR_TYPE_JXF38P,
	SENSOR_TYPE_JXH63P,
	SENSOR_TYPE_IMX327,
	SENSOR_TYPE_GC2063,
	SENSOR_TYPE_JXF51,
	SENSOR_TYPE_SC3336P,
};

typedef struct SENSOR_INFO_S
{
	unsigned char *name;
} SENSOR_INFO_T;

SENSOR_INFO_T g_sinfo[] =
{
	{"gc2053"},
	{"sc301IoT"},
	{"gc1084"},
	{"sc1346"},
	{"sc1a4t"},
	{"sc200ai"},
	{"sc2310"},
	{"sc2331"},
	{"sc2336"},
	{"sc2336p"},
	{"jxf23"},
	{"jxf37pa"},
	{"jxf38p"},
	{"jxh63p"},
	{"imx327"},
	{"gc2063"},
	{"jxf51"},
	{"sc3336p"},
};
int main(int argc,char **argv)
{
	int ret  = 0;
	int fd   = 0;
	int data = -1;
	/* open device file */
	fd = open("/dev/sinfo", O_RDWR);
	if (-1 == fd) {
		printf("err: open failed\n");
		return -1;
	}
	/* iotcl to get sensor info. */
	/* cmd is IOCTL_SINFO_GET, data note sensor type according to SENSOR_TYPE */

	ret = ioctl(fd,IOCTL_SINFO_GET,&data);
	if (0 != ret) {
		printf("err: ioctl failed\n");
		return ret;
	}
	if (SENSOR_TYPE_INVALID == data)
		printf("##### sensor not found\n");
	else
		printf("##### sensor : %s\n", g_sinfo[data].name);

	/* close device file */
	close(fd);
	return 0;
}
