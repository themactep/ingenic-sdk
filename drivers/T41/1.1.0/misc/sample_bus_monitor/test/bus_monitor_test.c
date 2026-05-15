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

int bus_monitor_dumpreg(struct monitor_param *monitordump)
{
	int ret = 0;
    int run_sta = 0;
    struct monitor_param monitorp;
    memcpy(&monitorp,monitordump,sizeof(struct monitor_param));
    while(1)
    {
        printf("dump reg? (0:yes,1:no):");
        scanf("%d",&run_sta);
        if(!run_sta){
            ret = ioctl(monitor_monitorfd, IOCTL_MONITOR_DUMP_REG, &monitorp);
            if (ret < 0) {
                printf("ioctl test error: ret = %d\n", ret);
                return -1;
            }
        }
        else
            return 0;
    }
}

int monitor_param_set(uint32_t monitor_mode, uint32_t chn, uint32_t id, uint32_t fun_mode)
{
	int ret = 0;
    struct monitor_param monitorp;

	if ((ret = monitor_init()) != 0) {
		printf("monitor: error monitor_init ret = %d\n", ret);
        goto monitor_err;
	}

    pthread_mutex_lock(&monitor_mutex);

    monitorp.monitor_id         = id;
    monitorp.monitor_start      = 0x04100000;
    monitorp.monitor_end        = 0x04ffffff;
    monitorp.monitor_mode       = monitor_mode;
    monitorp.monitor_chn        = chn;
    monitorp.monitor_fun_mode   = fun_mode;



    ret = ioctl(monitor_monitorfd, IOCTL_MONITOR_START, &monitorp);
    if (ret < 0) {
        printf("ioctl test error: ret = %d\n", ret);
        goto monitor_err;
    }

    /*
    ret = ioctl(monitor_monitorfd, IOCTL_MONITOR_BUF_LOCK, &monitorp);
    if (ret < 0) {
        printf("ioctl buf lock error: ret = %d\n", ret);
    }
    */

    ret = bus_monitor_dumpreg(&monitorp);
    if (ret < 0) {
        printf("%s error: ret = %d\n",__func__, ret);
        goto monitor_err;
    }

    ret = monitor_deinit();
    if (ret < 0) {
        printf("%s error: ret = %d\n",__func__, ret);
    }

    return 0;

monitor_err:
    monitor_deinit();
    return -1;

}

void help(const char *cmd)
{
	printf("usage: %s test_file ch0 [ch1 ch2 ch3 ch4 ch5 ch6]\n", cmd);
	printf("e.g. : %s ./monitor_test monitor_mode chn id\n", cmd);

}


int monitor_test(int num, char **data)
{
    int ret = -1;
    char **argv = data;
    unsigned int monitor_mode, id;
    unsigned int chn;
    unsigned int fun_mode;

    monitor_mode = atoi(argv[1]);
	chn = atoi(argv[2]);
	id = atoi(argv[3]);
	fun_mode = atoi(argv[4]);

	if ((chn < 0) || (chn > 7)) {
	    printf("the chn is not support,only support chn 0,1,2,3,4,5,6\n");
	    return -1;
    }
	
    ret= monitor_param_set(monitor_mode, chn, id, fun_mode);
    if(ret != 0)
    {
        printf("monitor_param_set error : ret = %d\n",ret);
        return -1;
    }

    return 0;

}

int bus_monitor_check(struct monitor_param *monitorp)
{
    char check[20];
    struct monitor_param *monitor =  monitorp;

    printf("0.check_id function\n"
            "1.check_addr function\n"
            "2.check_id_and_addr function\n"
            "3.vals function\n"
            "4.valm function\n");
    printf("bus monitor function,please input:");

    scanf("%s",check);
    if(atoi(check) > 4 || atoi(check) < 0)
        goto input_err;
    monitor->monitor_mode = atoi(check);

    if(monitor->monitor_mode == 1 || monitor->monitor_mode == 2){
        printf("check addr function start addr:\n"
                "eg:0x300000\n");
        scanf("%s",check);
        monitor->monitor_start = strtol(check, NULL, 16);

        printf("check addr function end addr:\n");
        scanf("%s",check);
        monitor->monitor_end = strtol(check, NULL, 16);

    }

    printf("Chn num,please input:");

    scanf("%s",check);
    if(atoi(check) > 7 || atoi(check) < 0)
        goto input_err;
    monitor->monitor_chn = atoi(check);

    if(monitor->monitor_mode != 1){    
        printf("0.LDC\n"
                "1.LCDC\n"
                "2.AIP\n"
                "3.ISP\n"
                "4.IVDC_W\n"
                "5.IPU\n"
                "6.MSC0\n"
                "7.MSC1\n"
                "8.I2D\n"
                "9.DRAWBOX\n"
                "10.JILOO\n"
                "11.EL\n"
                "12.IVDC_R\n"
                "13.CPU\n");
        printf("bus monitor id,please input:");

        scanf("%s",check);
        if(atoi(check) > 13 || atoi(check) < 0)
            goto input_err;
        monitor->monitor_id = atoi(check);
    }else{
        monitor->monitor_id = 0;
    }

    if(monitor->monitor_mode > 2){
        printf("0.user conter,irqreturn time = 1\n"
                "1.user conter,irqreturn time = 1/8\n"
                "2.max time =  0xffffffff,irqreturn time = 1/8\n"
                "3.\n"
                "4.max time = 0x88880000,irqreturn time = 1/8\n");
    printf("bus monitor id,please input:");

        scanf("%s",check);
        if(atoi(check) > 4 || atoi(check) < 0)
            goto input_err;
        monitor->monitor_fun_mode = atoi(check);
    }else{
        monitor->monitor_fun_mode = 0;
    }


   printf("monitor function = %d, chn = %d, id = %d, fun_mode = %d\n",monitor->monitor_mode,monitor->monitor_chn,monitor->monitor_id,monitor->monitor_fun_mode); 
   return 0;

input_err:
    printf("input_err !!!!\n");
    return -1;
}

int main(int argc, char **argv)
{
    int ret = 0;
    int num = argc;
    char **data = argv;

    if (argc == 1){
        struct monitor_param monitor;
        ret = bus_monitor_check(&monitor);
        if(ret != 0){
            printf("monitor check error!!!!! ret = %d\r\n",ret);
            return 0;
        }
        ret = monitor_param_set(monitor.monitor_mode, monitor.monitor_chn, monitor.monitor_id, monitor.monitor_fun_mode);
        if(ret != 0)
        {
            printf("monitor_param_set error : ret = %d\n",ret);
            return -1;
        }

    }else{
        if (argc < 4) {
            help(argv[0]);
            printf("err : param not enough\n");
            return -1;
        }
        if (argc > 5) {
            help(argv[0]);
            printf("err : param too many\n");
            return -1;
        }


        ret = monitor_test(num, data);
        if(ret != 0){
            printf("monitor_test error!!!!! ret = %d\r\n",ret);
        }

    }
    return 0;
}
