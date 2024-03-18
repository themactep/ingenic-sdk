#ifndef __TX_SENSOR_COMMON_H__
#define __TX_SENSOR_COMMON_H__

#if defined(CONFIG_SOC_T10) || defined(CONFIG_SOC_T20)
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <linux/i2c.h>
#include <soc/gpio.h>
#include "tx-isp-common.h"
#define private_jzgpio_set_func jzgpio_set_func
#else
#include <soc/gpio.h>
#include <txx-funcs.h>
#endif

#if !defined(CONFIG_SOC_T40) && !defined(CONFIG_SOC_T41)
#define SENSOR_R_BLACK_LEVEL  0
#define SENSOR_GR_BLACK_LEVEL 1
#define SENSOR_GB_BLACK_LEVEL 2
#define SENSOR_B_BLACK_LEVEL  3

/* External v4l2 format info. */
#define V4L2_I2C_REG_MAX                  (150)
#define V4L2_I2C_ADDR_16BIT               (0x0002)
#define V4L2_I2C_DATA_16BIT               (0x0004)
#define V4L2_SBUS_MASK_SAMPLE_8BITS       0x01
#define V4L2_SBUS_MASK_SAMPLE_16BITS      0x02
#define V4L2_SBUS_MASK_SAMPLE_32BITS      0x04
#define V4L2_SBUS_MASK_ADDR_8BITS         0x08
#define V4L2_SBUS_MASK_ADDR_16BITS        0x10
#define V4L2_SBUS_MASK_ADDR_32BITS        0x20
#define V4L2_SBUS_MASK_ADDR_STEP_16BITS   0x40
#define V4L2_SBUS_MASK_ADDR_STEP_32BITS   0x80
#define V4L2_SBUS_MASK_SAMPLE_SWAP_BYTES  0x100
#define V4L2_SBUS_MASK_SAMPLE_SWAP_WORDS  0x200
#define V4L2_SBUS_MASK_ADDR_SWAP_BYTES    0x400
#define V4L2_SBUS_MASK_ADDR_SWAP_WORDS    0x800
#define V4L2_SBUS_MASK_ADDR_SKIP          0x1000
#define V4L2_SBUS_MASK_SPI_READ_MSB_SET   0x2000
#define V4L2_SBUS_MASK_SPI_INVERSE_DATA   0x4000
#define V4L2_SBUS_MASK_SPI_HALF_ADDR      0x8000
#define V4L2_SBUS_MASK_SPI_LSB            0x10000
#endif

#if defined(CONFIG_SOC_T40)
#define ADDRBASE 0x0001c000
#else
#define ADDRBASE 0x00034000
#endif

#if defined(CONFIG_SOC_T41)
#ifdef CONFIG_KERNEL_4_4_94
#define SEN_TCLK "vpll"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_TCLK "sclka"
#endif

#ifdef CONFIG_KERNEL_4_4_94
#define SEN_MCLK "mux_cim"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_MCLK "cgu_cim"
#endif

#ifdef CONFIG_KERNEL_4_4_94
#define SEN_BCLK "div_cim"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_BCLK "cgu_cim"
#endif
#endif

#if defined(CONFIG_SOC_T10)
struct tx_isp_sensor_win_setting {
	int width;
	int height;
	int fps;
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	void *regs;	/* Regs to tweak; the default fps is fast */
};
#endif

static inline int set_sensor_gpio_function(int func_set) {
	int ret = 0;

	/* VDD select 1.8V */
#if defined(CONFIG_SOC_T21)
	*(volatile unsigned int*)(0xB0010104) = 0x1;
#elif defined(CONFIG_SOC_T23) || defined(CONFIG_SOC_T31) || defined(CONFIG_SOC_T40)
	*(volatile unsigned int*)(0xB0010130) = 0x2aaa000;
#endif

	switch (func_set) {
		case DVP_PA_LOW_8BIT:
			ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, ADDRBASE | 0x0ff);
			pr_info("set sensor gpio as PA-low-8bit\n");
			break;
		case DVP_PA_HIGH_8BIT:
			ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, ADDRBASE | 0xff0);
			pr_info("set sensor gpio as PA-high-8bit\n");
			break;
		case DVP_PA_LOW_10BIT:
			ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, ADDRBASE | 0x3ff);
			pr_info("set sensor gpio as PA-low-10bit\n");
			break;
		case DVP_PA_HIGH_10BIT:
			ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, ADDRBASE | 0xffc);
			pr_info("set sensor gpio as PA-high-10bit\n");
			break;
		case DVP_PA_12BIT:
			ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, ADDRBASE | 0xfff);
			pr_info("set sensor gpio as PA-12bit\n");
			break;
		default:
			pr_err("set sensor gpio error: unknow function %d\n", func_set);
			ret = -1;
			break;
	}
	return ret;
}

#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
static inline int set_sensor_mclk_function(int mclk_set)
{
	int ret = 0;

#if defined(CONFIG_SOC_T40)
	/* VDD select 1.8V */
	*(volatile unsigned int*)(0xB0010130) = 0x2aaa000;

	switch (mclk_set) {
		case 0:
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
#elif defined(CONFIG_SOC_T41)
	switch (mclk_set) {
		case 0 ... 2:
			ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x00008000);
			pr_info("set sensor mclk(%d) gpio\n", mclk_set);
			break;
		default:
			pr_err("set sensor mclk error: unknow function %d\n", mclk_set);
			ret = -1;
			break;
	}
#endif
	return ret;
}
#endif

#endif// __TX_SENSOR_COMMON_H__
