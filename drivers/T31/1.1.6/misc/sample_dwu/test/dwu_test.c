/*
 * ddr watch unit test header file.
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 * Author: Damon <jiansheng.zhang@ingenic.com>
 *
 */
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <asm/cachectl.h>
#include <sys/cachectl.h>
#include <sys/time.h>
#include "pthread.h"

#include "dwu_test.h"

static int dwu_inited = 0;
static int dwu_dwufd = 0;
char *owner = "DWU";
#define TAG "DWU"
pthread_mutex_t dwu_mutex = PTHREAD_MUTEX_INITIALIZER;

static int dwu_init(void)
{
	int ret = 0;
	pthread_mutex_lock(&dwu_mutex);
	if (1 == dwu_inited) {
		ret = 0;
		goto warn_dwu_has_inited;
	}
	if ((dwu_dwufd = open("/dev/dwu", O_RDWR)) < 0) {
		printf("dwu: Cann't open /dev/dwu\n");
		ret = dwu_dwufd;
		goto err_dwu_open;
	}

    dwu_inited = 1;
	pthread_mutex_unlock(&dwu_mutex);

    return 0;

err_dwu_open:
warn_dwu_has_inited:
	pthread_mutex_unlock(&dwu_mutex);
	return ret;
}

int dwu_deinit(void)
{
	int ret = 0;
	pthread_mutex_lock(&dwu_mutex);

    if (0 == dwu_inited) {
		printf("dwu: warning dwu has deinited\n");
		ret = 0;
		goto warn_dwu_has_deinited;
	}
	if (dwu_dwufd != 0)
	{
		close(dwu_dwufd);
		dwu_dwufd = 0;
	}

	dwu_inited = 0;
	pthread_mutex_unlock(&dwu_mutex);
	return 0;

warn_dwu_has_deinited:
	pthread_mutex_unlock(&dwu_mutex);
	return ret;
}


int dwu_param_set(uint32_t dev_num)
{
	int ret = 0;
    struct dwu_param dwup;

	if ((ret = dwu_init()) != 0) {
		printf("dwu: error dwu_init ret = %d\n", ret);
		return -1;
	}

    pthread_mutex_lock(&dwu_mutex);

    dwup.dwu_id0             = 0x60;
    dwup.dwu_id1             = 0x54;
    dwup.dwu_id2             = 0x52;
    dwup.dwu_id3             = 0x52;
    dwup.dwu_start_addr      = 0x00000004;
    dwup.dwu_end_addr        = 0x0fffffff;
    dwup.dwu_err_addr        = 0xffffffff;
    dwup.dwu_dev_num         = dev_num;


    ret = ioctl(dwu_dwufd, IOCTL_DWU_START, &dwup);
    if (ret < 0) {
        printf("ioctl test error: ret = %d\n", ret);
        goto err_ioctl_dwu_start;
    }

    pthread_mutex_unlock(&dwu_mutex);
    return 0;
err_ioctl_dwu_start:
    pthread_mutex_unlock(&dwu_mutex);
    return -1;
}

void help(const char *cmd)
{
	printf("usage: %s test_file dev_num\n", cmd);
	printf("e.g. : %s ./dwu_test dev_num\n", cmd);

}

int dwu_test(int num, char **data)
{
    char **argv = data;
    int ret = -1;
    unsigned int dev_num = 0;

    dev_num = atoi(argv[1]);

	if ((dev_num < 0) || (dev_num > 4)) {
	    printf("the chn is not support,only support 4 func_id\n");
	    return -1;
	}

    ret= dwu_param_set(dev_num);
    if(ret != 0)
    {
        printf("dwu_param_set error : ret = %d\n",ret);
        return -1;
    }

    return 0;

}

int main(int argc, char **argv)
{
    int num = argc;
    char **data = argv;
    int ret = 0;

	if (argc == 1) {
	    help(argv[0]);
	    return -1;
	}
	if (argc < 1) {
	    printf("err : param not enough\n");
	    return -1;
	}
    if (argc > 2) {
        printf("err : param too many\n");
        return -1;
    }

    ret = dwu_test(num, data);
    printf("DWU test PASS!!!!!!!!!!!!!\r\n");
    if(ret != 0){
        printf("dwu_test error!!!!! ret = %d\r\n",ret);
    }

    return 0;
}
