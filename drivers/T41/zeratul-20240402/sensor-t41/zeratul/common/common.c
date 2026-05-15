//#include <linux/init.h>
/*#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>
*/
/*#include <tx-isp-common.h>
#include <sensor-common.h>
#include <txx-funcs.h>
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/fs.h>


#include "../include/tx-isp-common.h"
/*
#include "../include/fast_start_common.h"
#include "include/tx-isp-frame-channel.h"
#include "include/tx-isp-device.h"
#include "include/tx-isp-tuning.h"
*/

//int init_sensor_one(void);
//extern int init_sensor_one(void);
int sensor_register_one(void);
int sensor_register_two(void);
int sensor_register_third(void);
struct list_head sensor_list;
//INIT_LIST_HEAD(&sensor_list);
struct add_sensor *sensors[8];

extern int use_num_sensor;

int add_sensor_chose(void)
{
//	printk("======= enter add_sensor_chose common  use_num_sensor = %d ======= \n", use_num_sensor);
	switch(use_num_sensor) {
		case 0:
			sensor_register_one();
			break;

		case 1:
			sensor_register_two();
			break;


		default:
			printk("====== swicth no match ====== \n");
			break;
		
	}
//	sensor_register_one();	
//	INIT_LIST_HEAD(&sensor_list);

	//	init_sensor_one();

	struct add_sensor *use_sensor;
	use_sensor = sensors[use_num_sensor];
	use_sensor->sensor_init();

	return 0;
}





