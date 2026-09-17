#ifndef __TX_SENSOR_COMMON_H__
#define __TX_SENSOR_COMMON_H__
#include <soc/gpio.h>

#include <txx-funcs.h>

#define SENSOR_R_BLACK_LEVEL	0
#define SENSOR_GR_BLACK_LEVEL	1
#define SENSOR_GB_BLACK_LEVEL	2
#define SENSOR_B_BLACK_LEVEL	3

/* External v4l2 format info. */
#define V4L2_I2C_REG_MAX		(150)
#define V4L2_I2C_ADDR_16BIT		(0x0002)
#define V4L2_I2C_DATA_16BIT		(0x0004)
#define V4L2_SBUS_MASK_SAMPLE_8BITS	0x01
#define V4L2_SBUS_MASK_SAMPLE_16BITS	0x02
#define V4L2_SBUS_MASK_SAMPLE_32BITS	0x04
#define V4L2_SBUS_MASK_ADDR_8BITS	0x08
#define V4L2_SBUS_MASK_ADDR_16BITS	0x10
#define V4L2_SBUS_MASK_ADDR_32BITS	0x20
#define V4L2_SBUS_MASK_ADDR_STEP_16BITS 0x40
#define V4L2_SBUS_MASK_ADDR_STEP_32BITS 0x80
#define V4L2_SBUS_MASK_SAMPLE_SWAP_BYTES 0x100
#define V4L2_SBUS_MASK_SAMPLE_SWAP_WORDS 0x200
#define V4L2_SBUS_MASK_ADDR_SWAP_BYTES	0x400
#define V4L2_SBUS_MASK_ADDR_SWAP_WORDS	0x800
#define V4L2_SBUS_MASK_ADDR_SKIP	0x1000
#define V4L2_SBUS_MASK_SPI_READ_MSB_SET 0x2000
#define V4L2_SBUS_MASK_SPI_INVERSE_DATA 0x4000
#define V4L2_SBUS_MASK_SPI_HALF_ADDR	0x8000
#define V4L2_SBUS_MASK_SPI_LSB		0x10000

static inline int set_sensor_gpio_function(int func_set)
{
	int ret = 0;
#if (defined(CONFIG_SOC_T10) || defined(CONFIG_SOC_T20) || defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21))
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
#else
	ret = -1;
#endif
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
