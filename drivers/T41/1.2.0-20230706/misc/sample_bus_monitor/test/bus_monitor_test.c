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
#include <unistd.h>
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
    struct monitor_param monitorp;
    memcpy(&monitorp,monitordump,sizeof(struct monitor_param));

    ret = ioctl(monitor_monitorfd, IOCTL_MONITOR_DUMP_REG, &monitorp);
    if (ret < 0) {
        printf("ioctl test error: ret = %d\n", ret);
        return -1;
    }

    return 0;
}

int monitor_param_set(uint32_t monitor_mode, uint32_t chn, uint32_t id, uint32_t fun_mode, uint32_t value0, uint32_t value1)
{
	int ret = 0;
    struct monitor_param monitorp;

    pthread_mutex_lock(&monitor_mutex);
    monitorp.monitor_id         = id;
    monitorp.monitor_start      = value0;
    monitorp.monitor_end        = value1;
    monitorp.monitor_mode       = monitor_mode;
    monitorp.monitor_chn        = chn;
    monitorp.monitor_fun_mode   = fun_mode;

    ret = ioctl(monitor_monitorfd, IOCTL_MONITOR_START, &monitorp);
    if (ret < 0) {
        printf("ioctl test error: ret = %d\n", ret);
        goto monitor_err;
    }


    //pthread_mutex_unlock(&monitor_mutex);

    ret = bus_monitor_dumpreg(&monitorp);
    if (ret < 0) {
        printf("%s error: ret = %d\n",__func__, ret);
        goto monitor_err;
    }

    pthread_mutex_unlock(&monitor_mutex);

    return 0;

monitor_err:
    monitor_deinit();
    return -1;

}

void help(const char *cmd)
{
	printf("usage: %s test_file ch0 [ch1 ch2 ch3 ch4 ch5 ch6]\n", cmd);
	printf("e.g. : %s ./monitor_test monitor_mode chn id func_mode value0 value1\n", cmd);

}


int monitor_test(int num, char **data)
{
    int ret = -1;
    char **argv = data;
    unsigned int monitor_mode, id;
    unsigned int chn;
    unsigned int fun_mode;
    unsigned int value0;
    unsigned int value1;

    monitor_mode = atoi(argv[1]);
	chn = atoi(argv[2]);
	id = atoi(argv[3]);
	fun_mode = atoi(argv[4]);
    value0 = atoi(argv[5]);
    value1 = atoi(argv[6]);

	if ((chn < 0) || (chn > 7)) {
	    printf("the chn is not support,only support chn 0,1,2,3,4,5,6\n");
	    return -1;
    }

    ret= monitor_param_set(monitor_mode, chn, id, fun_mode, value0, value1);
    if(ret != 0)
    {
        printf("monitor_param_set error : ret = %d\n",ret);
        return -1;
    }

    return 0;

}

int bus_monitor_check(struct monitor_param *monitorp, int id_num, int mode_num, int value0, int value1)
{
    int ret = 0;
    struct monitor_param *monitor =  monitorp;

    monitor->monitor_mode = mode_num;  // vals function
    //monitor->monitor_fun_mode = 4;  // max time 8888000-1
    monitor->monitor_fun_mode = 0;
    monitor->monitor_id = id_num;
    monitor->monitor_start = value0;
    monitor->monitor_end = value1;

    switch(id_num){
        case 0:
            monitor->monitor_chn = 0;
            printf("Start monitoring LCDC : \n");
            break;
        case 1:
            monitor->monitor_chn = 0;
            printf("Start monitoring LDC : \n");
            break;
        case 2:
            monitor->monitor_chn = 1;
            printf("Start monitoring AIP : \n");
            break;
        case 3:
            monitor->monitor_chn = 2;
            printf("Start monitoring ISP : \n");
            break;
        case 4:
            monitor->monitor_chn = 2;
            printf("Start monitoring IVDE : \n");
            break;
        case 5:
            monitor->monitor_chn = 3;
            printf("Start monitoring IPU : \n");
            break;
        case 6:
            monitor->monitor_chn = 3;
            printf("Start monitoring MSC0 : \n");
            break;
        case 7:
            monitor->monitor_chn = 3;
            printf("Start monitoring MSC1 : \n");
            break;
        case 8:
            monitor->monitor_chn = 3;
            printf("Start monitoring I2D : \n");
            break;
        case 9:
            monitor->monitor_chn = 3;
            printf("Start monitoring DRAWBOX : \n");
            break;
        case 10:
            monitor->monitor_chn = 4;
            printf("Start monitoring JILOO : \n");
            break;
        case 11:
            monitor->monitor_chn = 4;
            printf("Start monitoring EL : \n");
            break;
        case 12:
            monitor->monitor_chn = 4;
            printf("Start monitoring IVDC : \n");
            break;
        case 13:
            monitor->monitor_chn = 6;
            printf("Start monitoring CPU : \n");
            break;
        case 14:
            monitor->monitor_chn = 5;
			printf("Start monitoring USB : \n");
			break;
		case 15:
			monitor->monitor_chn = 5;
			printf("Start monitoring DMIC : \n");
			break;
		case 16:
			monitor->monitor_chn = 5;
			printf("Start monitoring AES : \n");
			break;
		case 17:
			monitor->monitor_chn = 5;
			printf("Start monitoring SFC0 : \n");
			break;
		case 18:
			monitor->monitor_chn = 5;
			printf("Start monitoring GMAC : \n");
			break;
		case 19:
			monitor->monitor_chn = 5;
			printf("Start monitoring BRI : \n");
			break;
		case 20:
			monitor->monitor_chn = 5;
			printf("Start monitoring HASH : \n");
			break;
		case 21:
			monitor->monitor_chn = 5;
			printf("Start monitoring SFC1 : \n");
			break;
		case 22:
			monitor->monitor_chn = 5;
			printf("Start monitoring PWM : \n");
			break;
		case 23:
			monitor->monitor_chn = 5;
			printf("Start monitoring PDMA : \n");
			break;
		case 24:
			monitor->monitor_chn = 7;
			printf("Start monitoring CPU_LEP : \n");
			break;
		default:
			printf("No ID for this module !");
			monitor->monitor_chn = 0;
            monitor->monitor_id = 0;
            return -id_num;
    }

    ret = monitor_param_set(monitor->monitor_mode, monitor->monitor_chn, monitor->monitor_id,monitor->monitor_fun_mode, monitor->monitor_start, monitor->monitor_end);
    if(ret != 0){
        printf("monitor_param_set error : ret = %d\n",ret);
        return -1;
    }

    return 0;
}



int main(int argc, char **argv)
{
    int ret = 0;
    int num = argc;
    char **data = argv;

    if ((ret = monitor_init()) != 0) {
        printf("monitor: error monitor_init ret = %d\n", ret);
        goto monitor_err;
    }

    if (argc == 1){
        struct monitor_param monitor;
        int id_num = 0;
        int get_mode;
        int addr_start, addr_end;
        printf("Please select monitoring mode: \n1.Monitor the IDs of all modules. \n2.Monitor all module ADDRs by range. \n");
        printf("3.Monitor the data transmission volume of all modules.\n Your choice is:");
        scanf("%d", &get_mode);
        if(1 == get_mode){
            for (id_num = 0; id_num < 25 ;id_num++){
                ret = bus_monitor_check(&monitor, id_num, 0, 0x40000, 0x280000);
                if(ret != 0){
                    printf("monitor check error!!!!! ret = %d\r\n",ret);
                    goto monitor_err;
                }
            }
        }
        else if(2 == get_mode){
            printf("value0(eg.:0x410000):");
            scanf("%x", &addr_start);
            printf("value1:");
            getchar();
            scanf("%x", &addr_end);
            for (id_num = 0; id_num < 25 ;id_num++){
                ret = bus_monitor_check(&monitor, id_num, 1, addr_start, addr_end);
                if(ret != 0){
                    printf("monitor check error!!!!! ret = %d\r\n",ret);
                    goto monitor_err;
                }
            }
        }
        else if(3 == get_mode){
            for (id_num = 0; id_num < 25 ;id_num++){
                ret = bus_monitor_check(&monitor, id_num, 3, 0x40000, 0x280000);
                if(ret != 0){
                    printf("monitor check error!!!!! ret = %d\r\n",ret);
                    goto monitor_err;
                }
            }
        }

    }else{
        if (argc < 6) {
            help(argv[0]);
            printf("err : param not enough\n");
            goto monitor_err;
        }
        if (argc > 7) {
            help(argv[0]);
            printf("err : param too many\n");
            goto monitor_err;
        }

        ret = monitor_test(num, data);
        if(ret != 0){
            printf("monitor_test error!!!!! ret = %d\r\n",ret);
            goto monitor_err;
        }

    }
    monitor_deinit();
    return 0;

monitor_err:
    monitor_deinit();
    return -1;
}
