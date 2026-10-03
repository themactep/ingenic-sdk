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

#if defined(SENSOR_PROC_OWNED_BY_ISP) || defined(SENSOR_REGISTRY_IN_SDK)
/*
 * Two configurations publish the pre-bind sensor registry
 * (/proc/jz/sensor/sensorN/) that Raptor reads before it binds the sensor: the
 * open ISP stack (SENSOR_PROC_OWNED_BY_ISP, registry from open-tx-isp) and the
 * proprietary ISP with the SDK registry (SENSOR_REGISTRY_IN_SDK). The raw
 * private_i2c_add_driver() registers only with the I2C core, so publish the
 * driver to the registry with its own SENSOR_I2C_ADDRESS - otherwise
 * sensorN/i2c_addr reads 0 pre-bind and rvd's autodetect fails.
 * tx_isp_sinfo_driver_add() merges a repeat call for the same driver, so one
 * macro serves every family. The proprietary build keeps the vendor flat tree
 * the sensor module publishes alongside the registry.
 */
int tx_isp_sinfo_driver_add(struct i2c_driver *drv, int def_i2c_addr,
			    struct module *owner);
void tx_isp_sinfo_driver_del(struct i2c_driver *drv);

static inline int __sinfo_i2c_add_driver(struct i2c_driver *drv,
					 int def_i2c_addr,
					 struct module *owner)
{
	int ret = (private_i2c_add_driver)(drv);
	if (!ret)
		tx_isp_sinfo_driver_add(drv, def_i2c_addr, owner);
	return ret;
}

static inline void __sinfo_i2c_del_driver(struct i2c_driver *drv)
{
	tx_isp_sinfo_driver_del(drv);
	(private_i2c_del_driver)(drv);
}

#define private_i2c_add_driver(drv) \
	__sinfo_i2c_add_driver((drv), SENSOR_I2C_ADDRESS, THIS_MODULE)
#define private_i2c_del_driver(drv) \
	__sinfo_i2c_del_driver((drv))
#endif /* SENSOR_PROC_OWNED_BY_ISP || SENSOR_REGISTRY_IN_SDK */

#ifdef SENSOR_REGISTRY_IN_SDK
/*
 * The proprietary T40/T41 ISP never calls tx_isp_sinfo_sensor_bind() itself,
 * so the registry keeps the driver_add slot but no subdev: the live state
 * stays empty and status "loaded". The open stack's core binds the sensor
 * itself, so this hook is SDK-registry only. Bind at the sensor's
 * tx_isp_subdev_init(), where the subdev and its attributes become valid.
 */
int tx_isp_sinfo_sensor_bind(void *subdev, struct module *owner);
void tx_isp_sinfo_sensor_unbind(void *subdev, struct module *owner);

static inline int __sinfo_subdev_init(struct platform_device *pdev,
				      struct tx_isp_subdev *sd,
				      struct tx_isp_subdev_ops *ops)
{
	int ret = (tx_isp_subdev_init)(pdev, sd, ops);
	if (!ret)
		tx_isp_sinfo_sensor_bind(sd, THIS_MODULE);
	return ret;
}

static inline void __sinfo_subdev_deinit(struct tx_isp_subdev *sd)
{
	tx_isp_sinfo_sensor_unbind(sd, THIS_MODULE);
	(tx_isp_subdev_deinit)(sd);
}

#define tx_isp_subdev_init(pdev, sd, ops) \
	__sinfo_subdev_init((pdev), (sd), (ops))
#define tx_isp_subdev_deinit(sd) __sinfo_subdev_deinit((sd))
#endif /* SENSOR_REGISTRY_IN_SDK */
#endif// __TX_SENSOR_COMMON_H__
