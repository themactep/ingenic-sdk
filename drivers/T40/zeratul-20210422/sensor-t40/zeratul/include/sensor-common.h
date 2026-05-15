#ifndef __TX_SENSOR_COMMON_H__
#define __TX_SENSOR_COMMON_H__
#include <soc/gpio.h>
#include <txx-funcs.h>

static inline int set_sensor_gpio_function(int func_set)
{
	int ret = 0;
	/* VDD select 1.8V */
//	*(volatile unsigned int*)(0xB0010104) = 0x1;
	*(volatile unsigned int*)(0xB0010130) = 0x2aaa000;
	switch (func_set) {
	case DVP_PA_LOW_8BIT:
		/* ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0001c0ff); */
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0001c0ff);
		pr_info("set sensor gpio as PA-low-8bit\n");
		break;
	case DVP_PA_HIGH_8BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0001cff0);
		pr_info("set sensor gpio as PA-high-8bit\n");
		break;
	case DVP_PA_LOW_10BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0001c3ff);
		pr_info("set sensor gpio as PA-low-10bit\n");
		break;
	case DVP_PA_HIGH_10BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0001cffc);
		pr_info("set sensor gpio as PA-high-10bit\n");
		break;
	case DVP_PA_12BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0001cfff);
		pr_info("set sensor gpio as PA-12bit\n");
		break;
	default:
		pr_err("set sensor gpio error: unknow function %d\n", func_set);
		ret = -1;
		break;
	}
	

	return ret;
}

static inline int set_sensor_mclk_function(int mclk_set)
{
	int ret = 0;
	/* VDD select 1.8V */
//	*(volatile unsigned int*)(0xB0010104) = 0x1;
	*(volatile unsigned int*)(0xB0010130) = 0x2aaa000;
	switch (mclk_set) {
	case 0:
		/* ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0001c0ff); */
		ret = private_jzgpio_set_func(GPIO_PORT_C, GPIO_FUNC_1, 0x80000000);
		pr_info("set sensor mclk(%d) gpio\n", mclk_set);
		break;
	case 1:
		ret = private_jzgpio_set_func(GPIO_PORT_C, GPIO_FUNC_1, 0x40000000);
		pr_info("set sensor mclk(%d) gpio\n", mclk_set);
		break;
	case 2:
		ret = private_jzgpio_set_func(GPIO_PORT_C, GPIO_FUNC_1, 0x20000000);
		pr_info("set sensor mclk(%d) gpio\n", mclk_set);
		break;
	default:
		pr_err("set sensor mclk error: unknow function %d\n", mclk_set);
		ret = -1;
		break;
	}
	
	return ret;
}

#endif// __TX_SENSOR_COMMON_H__
