#ifndef __TX_SENSOR_COMMON_H__
#define __TX_SENSOR_COMMON_H__
#include <soc/gpio.h>
#include <txx-funcs.h>

#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_6_1)
#define SEN_TCLK "vpll"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_TCLK "vpll"
#endif

#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_6_1)
#define SEN_MCLK "mux_cim"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_MCLK "cgu_cim"
#endif

#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_6_1)
#define SEN_BCLK "div_cim"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_BCLK "cgu_cim"
#endif

static inline int set_sensor_gpio_function(int func_set)
{
	int ret = 0;
	/* VDD select 1.8V */
//	*(volatile unsigned int*)(0xB0010104) = 0x1;
	/* *(volatile unsigned int*)(0xB0010130) = 0x2aaa000; */
	switch (func_set) {
	case DVP_PA_LOW_8BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x000340ff);
		pr_info("set sensor gpio as PA-low-8bit\n");
		break;
	case DVP_PA_HIGH_8BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x00034ff0);
		pr_info("set sensor gpio as PA-high-8bit\n");
		break;
	case DVP_PA_LOW_10BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x000343ff);
		pr_info("set sensor gpio as PA-low-10bit\n");
		break;
	case DVP_PA_HIGH_10BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x00034ffc);
		pr_info("set sensor gpio as PA-high-10bit\n");
		break;
	case DVP_PA_12BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x00034fff);
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
	/* *(volatile unsigned int*)(0xB0010130) = 0x2aaa000; */
	switch (mclk_set) {
	case 0 ... 2:
		/* ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0001c0ff); */
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x00008000);
		pr_info("set sensor mclk(%d) gpio\n", mclk_set);
		break;
	default:
		pr_err("set sensor mclk error: unknow function %d\n", mclk_set);
		ret = -1;
		break;
	}

	return ret;
}

#ifdef SENSOR_PROC_OWNED_BY_ISP
/*
 * Open ISP stack: open-tx-isp's tx_isp_sinfo publishes the pre-bind sensor
 * registry (/proc/jz/sensor/sensorN/) that Raptor reads before it binds the
 * sensor. The raw private_i2c_add_driver() registers only with the I2C core,
 * so publish the driver to the registry with its own SENSOR_I2C_ADDRESS -
 * otherwise sensorN/i2c_addr reads 0 pre-bind and rvd's autodetect fails.
 * tx_isp_sinfo_driver_add() merges a repeat call for the same driver (t31's
 * wrapper publishes the legacy 0 first), so one macro serves every family.
 * Only defined for the open stack; the proprietary ISP has no registry and
 * keeps the flat tree the sensor module publishes.
 */
int tx_isp_sinfo_driver_add(struct i2c_driver *drv, int def_i2c_addr,
			    struct module *owner);

static inline int __sinfo_i2c_add_driver(struct i2c_driver *drv,
					 int def_i2c_addr,
					 struct module *owner)
{
	int ret = (private_i2c_add_driver)(drv);
	if (!ret)
		tx_isp_sinfo_driver_add(drv, def_i2c_addr, owner);
	return ret;
}

#define private_i2c_add_driver(drv) \
	__sinfo_i2c_add_driver((drv), SENSOR_I2C_ADDRESS, THIS_MODULE)
#endif /* SENSOR_PROC_OWNED_BY_ISP */
#endif// __TX_SENSOR_COMMON_H__
