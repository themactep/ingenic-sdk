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
	SENSOR_TYPE_GC2063=0,
	SENSOR_TYPE_GC4663,
	SENSOR_TYPE_SC500AI,
	SENSOR_TYPE_JXF37,
	SENSOR_TYPE_GC2083,
	SENSOR_TYPE_GC2607,
	SENSOR_TYPE_GC3003,
	SENSOR_TYPE_GC3003A,
	SENSOR_TYPE_GC4023,
	SENSOR_TYPE_GC5603,
	SENSOR_TYPE_GC5613,
	SENSOR_TYPE_GC8613,
	SENSOR_TYPE_SC031GS,
	SENSOR_TYPE_SC200AI,
	SENSOR_TYPE_SC2210,
	SENSOR_TYPE_SC233a,
	SENSOR_TYPE_SC301IOT,
	SENSOR_TYPE_SC3336,
	SENSOR_TYPE_SC3338,
	SENSOR_TYPE_SC401AI,
	SENSOR_TYPE_SC4210,
	SENSOR_TYPE_SC430AI,
	SENSOR_TYPE_SC4336,
	SENSOR_TYPE_SC4336P,
	SENSOR_TYPE_SC450AI,
	SENSOR_TYPE_SC501AI,
	SENSOR_TYPE_SC5239,
	SENSOR_TYPE_SC5336,
	SENSOR_TYPE_SC5338,
	SENSOR_TYPE_SC8238,
	SENSOR_TYPE_SC830AI,
	SENSOR_TYPE_CV3001,
	SENSOR_TYPE_CV4001,
	SENSOR_TYPE_CV5001,
	SENSOR_TYPE_JXF51,
	SENSOR_TYPE_JXK06,
	SENSOR_TYPE_JXK08,
	SENSOR_TYPE_IMX327,
	SENSOR_TYPE_IMX355,
	SENSOR_TYPE_IMX363,
	SENSOR_TYPE_IMX378,
	SENSOR_TYPE_IMX386,
	SENSOR_TYPE_IMX415,
	SENSOR_TYPE_IMX482,
	SENSOR_TYPE_IMX515,
	SENSOR_TYPE_IMX662,
	SENSOR_TYPE_MIS2009,
	SENSOR_TYPE_MIS4001,
	SENSOR_TYPE_OS04A10,
	SENSOR_TYPE_OS04C10,
	SENSOR_TYPE_OS08A10,
	SENSOR_TYPE_SC850SL,
	SENSOR_TYPE_SC835HAI,
};

typedef struct SENSOR_INFO_S
{
	unsigned char *name;
} SENSOR_INFO_T;

SENSOR_INFO_T g_sinfo[] =
{
	{"gc2063"},
	{"gc4663"},
	{"sc500ai"},
	{"jxf37"},
	{"gc2083"},
	{"gc2607"},
	{"gc3003"},
	{"gc3003a"},
	{"gc4023"},
	{"gc5603"},
	{"gc5613"},
	{"gc8613"},
	{"sc031gs"},
	{"sc200ai"},
	{"sc2210"},
	{"sc233a"},
	{"sc301iot"},
	{"sc3336"},
	{"sc3338"},
	{"sc401ai"},
	{"sc4210"},
	{"sc430ai"},
	{"sc4336"},
	{"sc4336p"},
	{"sc450ai"},
	{"sc501ai"},
	{"sc5239"},
	{"sc5336"},
	{"sc5338"},
	{"sc8238"},
	{"sc830ai"},
	{"cv3001"},
	{"cv4001"},
	{"cv5001"},
	{"jxf51"},
	{"jxk06"},
	{"jxk08"},
	{"imx327"},
	{"imx355"},
	{"imx363"},
	{"imx378"},
	{"imx386"},
	{"imx415"},
	{"imx482"},
	{"imx515"},
	{"imx662"},
	{"mis2009"},
	{"mis4001"},
	{"os04a10"},
	{"os04c10"},
	{"os08a10"},
	{"sc850sl"},
	{"sc835hai"},
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
