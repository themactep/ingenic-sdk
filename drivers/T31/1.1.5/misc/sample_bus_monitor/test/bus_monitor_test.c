/*
 * bus_monitor test header file.
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 * Author: Damon <jiansheng.zhang@ingenic.cn>
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

#include "bus_monitor_test.h"

static int monitor_inited = 0;
static int monitor_monitorfd = 0;
char *owner = "MONITOR";
#define TAG "MONITOR"
pthread_mutex_t monitor_mutex = PTHREAD_MUTEX_INITIALIZER;

static int monitor_init(void)
{
	int ret = 0;
	pthread_mutex_lock(&monitor_mutex);
	if (1 == monitor_inited) {
		ret = 0;
		goto warn_monitor_has_inited;
	}
	if ((monitor_monitorfd = open("/dev/monitor", O_RDWR)) < 0) {
		printf("monitor: Cann't open /dev/monitor\n");
		ret = monitor_monitorfd;
		goto err_monitor_open;
	}

    monitor_inited = 1;
	pthread_mutex_unlock(&monitor_mutex);

    return 0;

err_monitor_open:
warn_monitor_has_inited:
	pthread_mutex_unlock(&monitor_mutex);
	return ret;
}

int monitor_deinit(void)
{
	int ret = 0;
	pthread_mutex_lock(&monitor_mutex);

    if (0 == monitor_inited) {
		printf("monitor: warning monitor has deinited\n");
		ret = 0;
		goto warn_monitor_has_deinited;
	}
	if (monitor_monitorfd != 0)
	{
		close(monitor_monitorfd);
		monitor_monitorfd = 0;
	}

	monitor_inited = 0;
	pthread_mutex_unlock(&monitor_mutex);
	return 0;

warn_monitor_has_deinited:
	pthread_mutex_unlock(&monitor_mutex);
	return ret;
}


int monitor_param_set(uint32_t monitor_mode, uint32_t chn, uint32_t id)
{
	int ret = 0;
    struct monitor_param monitorp;

	if ((ret = monitor_init()) != 0) {
		printf("monitor: error monitor_init ret = %d\n", ret);
		return -1;
	}

    pthread_mutex_lock(&monitor_mutex);

    monitorp.monitor_id         = id;
    monitorp.monitor_start      = 0x00000000;
    monitorp.monitor_end        = 0x0fffffff;
    monitorp.monitor_mode       = monitor_mode;
    monitorp.monitor_chn        = chn;


    ret = ioctl(monitor_monitorfd, IOCTL_MONITOR_START, &monitorp);
    if (ret < 0) {
        printf("ioctl test error: ret = %d\n", ret);
        goto err_ioctl_monitor_start;
    }

    pthread_mutex_unlock(&monitor_mutex);
    return 0;
err_ioctl_monitor_start:
    pthread_mutex_unlock(&monitor_mutex);
    return -1;
}

void help(const char *cmd)
{
	printf("usage: %s test_file ch0 [ch1 ch2 ch3 ch4 ch5 ch6]\n", cmd);
	printf("e.g. : %s ./monitor_test monitor_mode chn id\n", cmd);

}

int monitor_test(int num, char **data)
{
    char **argv = data;
    int ret = -1;
	unsigned int monitor_mode, id;
    unsigned int chn = 0;

    monitor_mode = atoi(argv[1]);
	chn = atoi(argv[2]);
	id = atoi(argv[3]);

	if ((chn < 0) || (chn > 6)) {
	    printf("the chn is not support,only support chn 0,1,2,3,4,5,6\n");
	    return -1;
	}

    ret= monitor_param_set(monitor_mode, chn, id);
    if(ret != 0)
    {
        printf("monitor_param_set error : ret = %d\n",ret);
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
	if (argc < 3) {
	    printf("err : param not enough\n");
	    return -1;
	}
    if (argc > 4) {
        printf("err : param too many\n");
        return -1;
    }


    ret = monitor_test(num, data);
    printf("MONITOR test PASS!!!!!!!!!!!!!\r\n");
    if(ret != 0){
        printf("monitor_test error!!!!! ret = %d\r\n",ret);
    }

    return 0;
}
