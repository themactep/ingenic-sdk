/*
 * gc13b0.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *   0 			3840*2880	 	20	 	  mipi 2lane   Linear   10 	 	 20 		null
 *
 * @I2C addr:0x4a
 *
 * @FSync:
 *
 */
#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>

#include <tx-isp-common.h>
#include <sensor-common.h>
#include <txx-funcs.h>
#include <tx-isp-fast.h>

#define SENSOR_VERSION  "H20250618a"

// #define SENSOR_TEST
//#define SENSOR_SLAVE
//#define SENSOR_PM

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
//#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
//#define SENSOR_HCG
//#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x13)
#define SENSOR_CHIP_ID_L    (0xB0)
#define SENSOR_OUTPUT_MIN_FPS 14
#define SENSOR_MCLK 24000000

#ifdef CONFIG_ZERATUL
#define ZRT_SENSOR_WITHOUT_INIT
#endif

#ifdef SENSOR_WDR_2_FRAME
static int wdr_line = xxx;
#endif

#ifndef SENSOR_I2C_REG_8BIT
#define SENSOR_I2C_REG_16BIT
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_8BIT
#define SENSOR_REG_END    0xff
#define SENSOR_REG_DELAY  0xfe
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_16BIT
#define SENSOR_REG_END    0xffff
#define SENSOR_REG_DELAY  0xfffe
#endif /* SENSOR_I2C_REG_16BIT */

struct regval_list {
#ifdef SENSOR_I2C_REG_8BIT
	uint8_t reg_num;
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_16BIT
	uint16_t reg_num;
#endif /* SENSOR_I2C_REG_16BIT */
	uint8_t value;
};

struct tx_isp_sensor_attribute gc13b0_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut gc13b0_again_lut[] = {
	/* {0x00, 0}, */
	/* ..., */
	{0x800 ,0},
	{0x840 ,5732},
	{0x880 ,11136},
	{0x8C0 ,16248},
	{0x900 ,21098},
	{0x940 ,25711},
	{0x980 ,30109},
	{0x9C0 ,34312},
	{0xA00 ,38336},
	{0xA40 ,42196},
	{0xA80 ,45904},
	{0xAC0 ,49472},
	{0xB00 ,52911},
	{0xB40 ,56229},
	{0xB80 ,59434},
	{0xBC0 ,62534},
	{0xC00 ,65536},
	{0xC80 ,71268},
	{0xD00 ,76672},
	{0xD80 ,81784},
	{0xE00 ,86634},
	{0xE80 ,91247},
	{0xF00 ,95645},
	{0xF80 ,99848},
	{0x1000 ,103872},
	{0x1080 ,107732},
	{0x1100 ,111440},
	{0x1180 ,115008},
	{0x1200 ,118447},
	{0x1280 ,121765},
	{0x1300 ,124970},
	{0x1380 ,128070},
	{0x1400 ,131072},
	{0x1500 ,136804},
	{0x1600 ,142208},
	{0x1700 ,147320},
	{0x1800 ,152170},
	{0x1900 ,156783},
	{0x1A00 ,161181},
	{0x1B00 ,165384},
	{0x1C00 ,169408},
	{0x1D00 ,173268},
	{0x1E00 ,176976},
	{0x1F00 ,180544},
	{0x2000 ,183983},
	{0x2100 ,187301},
	{0x2200 ,190506},
	{0x2300 ,193606},
	{0x2400 ,196608},
	{0x2600 ,202340},
	{0x2800 ,207744},
	{0x2A00 ,212856},
	{0x2C00 ,217706},
	{0x2E00 ,222319},
	{0x3000 ,226717},
	{0x3200 ,230920},
	{0x3400 ,234944},
	{0x3600 ,238804},
	{0x3800 ,242512},
	{0x3A00 ,246080},
	{0x3C00 ,249519},
	{0x3E00 ,252837},
	{0x4000 ,256042},
	{0x4200 ,259142},
	{0x4400 ,262144}
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc13b0_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc13b0_again_lut;
	while (lut->gain <= gc13b0_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc13b0_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	/* ...; */
#else
	/* Non analog gain table */
	/* ...; */
#endif  /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int gc13b0_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc13b0_again_lut;
	while(lut->gain <= gc13b0_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc13b0_attr.again_short.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	...;
#else
	/* Non analog gain table */

	...;
#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int gc13b0_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int gc13b0_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc13b0_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc13b0_mipi_linear_2lane = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	/* .clk = 1106, */
	.clk = 1504,
	.lans = 2,
	.image_twidth = 3840,
	.image_theight = 2880,
	/* .image_twidth = 2104, */
	/* .image_theight = 1560, */
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_dvp_bus gc13b0_dvp = {
	.gpio = DVP_PA_LOW_10BIT,
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.hblanking = 0,
		.vblanking = 0,
	},
	.polar = {
		.hsync_polar = 0,
		.vsync_polar = 0,
		.pclk_polar = 0,
	},
	.dvp_hcomp_en = 0,
};

struct tx_isp_sensor_attribute gc13b0_attr = {
	.name = "gc13b0",
	.chip_id = 0x13b0,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x4a,
	.sensor_ctrl.alloc_again = gc13b0_alloc_again,
	.sensor_ctrl.alloc_dgain = gc13b0_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = gc13b0_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list gc13b0_init_regs_3840_3120_20fps_mipi_2lane[] = {
	{0x0315,0xd7},
	{0x031c,0xe0},
	{0x031a,0x06},
	{0x0fe2,0x17},
	{0x0f9b,0xc9},
	{0x0900,0x10},
	{0x0fbf,0x36},
	{0x0fd2,0x36},
	{0x0fc1,0x04},
	{0x0fc2,0x5f},
	{0x0fcb,0x0c},
	{0x0fc4,0x10},
	{0x0fc5,0xb4},
	{0x0fd4,0x21},
	{0x0fd5,0x35},
	{0x0fdc,0x01},
	{0x0fd6,0x63},
	{0x0901,0x00},
	{0x0fd3,0x07},
	{0x0fc0,0xb4},
	{0x0fc3,0xb8},
	{0x0fcc,0x1d},
	{0x0fcd,0x1d},
	{0x0fde,0x4a},
	{0x0fdf,0x4a},
	{0x05a0,0xc1},
	{0x0af4,0x68},
	{0x0118,0x51},
	{0x0101,0x00},
	{0x0004,0x06},
	{0x001f,0x01},
	{0x0a67,0x80},
	{0x0a70,0x80},
	{0x0a66,0x00},
	{0x0a74,0x17},
	{0x0a68,0x04},
	{0x0a4e,0x40},
	{0x0a52,0x0c},
	{0x0f08,0x03},
	{0x0217,0xf8},
	{0x0219,0x95},
	{0x0225,0x04},
	{0x0512,0x00},
	{0x021f,0x10},
	{0x022a,0x00},
	{0x0255,0x24},
	{0x0256,0x0c},
	{0x0257,0x78},
	{0x02b2,0x0d},
	{0x0082,0x37},
	{0x0f26,0x10},
	{0x0f27,0x10},
	{0x0f30,0x7f},
	{0x0ec0,0x00},
	{0x0ec8,0x02},
	{0x0ec9,0x40},
	{0x0f9c,0xe8},
	{0x0fc6,0x10},
	{0x0f9a,0x8c},
	{0x0f9e,0x10},
	{0x0f9f,0x12},
	{0x0fa0,0x6b},
	{0x0fa1,0x30},
	{0x0fa8,0x15},
	{0x0faa,0xd4},
	{0x0fb4,0x04},
	{0x0fb8,0x44},
	{0x0fba,0x08},
	{0x0fbb,0x07},
	{0x0fbd,0x5d},
	{0x0fbe,0x00},
	{0x0f98,0x44},
	{0x0f99,0x00},
	{0x0ed6,0x02},
	{0x0e52,0x20},
	{0x0e53,0x40},
	{0x0e0f,0x03},
	{0x0fca,0x1b},
	{0x0e7c,0x1a},
	{0x0e7d,0x00},
	{0x0e7e,0x62},
	{0x0e78,0xff},
	{0x0f2e,0x40},
	{0x0f5f,0x00},
	{0x0f42,0x97},
	{0x0f32,0x01},
	{0x0f33,0x2b},
	{0x0f34,0x0b},
	{0x0f44,0x0f},
	{0x0f4a,0x50},
	{0x0f4b,0x0d},
	{0x0f4d,0x00},
	{0x0f4e,0x18},
	{0x0070,0x01},
	{0x0071,0x81},
	{0x0072,0x05},
	{0x0114,0x01},
	{0x0182,0x20},
	{0x0902,0x03},
	{0x096a,0xd9},
	{0x096b,0x8f},
	{0x0190,0x00},
	{0x0266,0x03},
	{0x0af5,0x10},
	{0x0b00,0x17},
	{0x0b01,0xe2},
	{0x0b02,0x0f},
	{0x0b03,0x00},
	{0x0b04,0x1f},
	{0x0b05,0x01},
	{0x0b06,0x09},
	{0x0b07,0x00},
	{0x0b08,0x27},
	{0x0b09,0xd3},
	{0x0b0a,0x0f},
	{0x0b0b,0x00},
	{0x0b0c,0xb6},
	{0x0b0d,0xc0},
	{0x0b0e,0x0f},
	{0x0b0f,0x00},
	{0x0b10,0xba},
	{0x0b11,0xc3},
	{0x0b12,0x0f},
	{0x0b13,0x00},
	{0x0b14,0xc1},
	{0x0b15,0x80},
	{0x0b16,0x01},
	{0x0b17,0x00},
	{0x0b18,0x01},
	{0x0b19,0x80},
	{0x0b1a,0x01},
	{0x0b1b,0x00},
	{0x0b1c,0x00},
	{0x0b1d,0xff},
	{0x0b1e,0x7a},
	{0x0b1f,0x00},
	{0x0b20,0x00},
	{0x0b21,0xff},
	{0x0b22,0x7a},
	{0x0b23,0x00},
	{0x0b24,0x00},
	{0x0b25,0xff},
	{0x0b26,0x7a},
	{0x0b27,0x00},
	{0x0b28,0x80},
	{0x0b29,0x1c},
	{0x0b2a,0x03},
	{0x0b2b,0x00},
	{0x0b2c,0x10},
	{0x0b2d,0xfe},
	{0x0b2e,0x03},
	{0x0b2f,0x00},
	{0x0b30,0x00},
	{0x0b31,0xfe},
	{0x0b32,0x03},
	{0x0b33,0x00},
	{0x0b34,0x9b},
	{0x0b35,0x1c},
	{0x0b36,0x03},
	{0x0b37,0x00},
	{0x0b38,0x00},
	{0x0b39,0xff},
	{0x0b3a,0x7a},
	{0x0b3b,0x00},
	{0x0b3c,0x00},
	{0x0b3d,0xff},
	{0x0b3e,0x7a},
	{0x0b3f,0x00},
	{0x0b40,0x00},
	{0x0b41,0xff},
	{0x0b42,0x7a},
	{0x0b43,0x00},
	{0x0b44,0x80},
	{0x0b45,0x1c},
	{0x0b46,0x03},
	{0x0b47,0x00},
	{0x0b48,0x10},
	{0x0b49,0xfe},
	{0x0b4a,0x03},
	{0x0b4b,0x00},
	{0x0b4c,0x00},
	{0x0b4d,0xfe},
	{0x0b4e,0x03},
	{0x0b4f,0x00},
	{0x0b50,0x9b},
	{0x0b51,0x1c},
	{0x0b52,0x03},
	{0x0b53,0x00},
	{0x0b54,0x00},
	{0x0b55,0xff},
	{0x0b56,0x7a},
	{0x0b57,0x00},
	{0x0b58,0x00},
	{0x0b59,0xff},
	{0x0b5a,0x7a},
	{0x0b5b,0x00},
	{0x0b5c,0x00},
	{0x0b5d,0xff},
	{0x0b5e,0x7a},
	{0x0b5f,0x00},
	{0x0b60,0x00},
	{0x0b61,0x83},
	{0x0b62,0x00},
	{0x0b63,0x00},
	{0x0b64,0x0d},
	{0x0b65,0x62},
	{0x0b66,0x02},
	{0x0b67,0x00},
	{0x0b68,0x99},
	{0x0b69,0x12},
	{0x0b6a,0x01},
	{0x0b6b,0x00},
	{0x0b6c,0x1f},
	{0x0b6d,0x81},
	{0x0b6e,0x01},
	{0x0b6f,0x00},
	{0x0b70,0x10},
	{0x0b71,0x84},
	{0x0b72,0x01},
	{0x0b73,0x00},
	{0x0b74,0x01},
	{0x0b75,0xb4},
	{0x0b76,0x09},
	{0x0b77,0x00},
	{0x0b78,0x01},
	{0x0b79,0x74},
	{0x0b7a,0x09},
	{0x0b7b,0x00},
	{0x0b7c,0x00},
	{0x0b7d,0x14},
	{0x0b7e,0x09},
	{0x0b7f,0x00},
	{0x0b80,0x00},
	{0x0b81,0x94},
	{0x0b82,0x09},
	{0x0b83,0x00},
	{0x0b84,0x01},
	{0x0b85,0x54},
	{0x0b86,0x09},
	{0x0b87,0x00},
	{0x0b88,0x00},
	{0x0b89,0xff},
	{0x0b8a,0x7a},
	{0x0b8b,0x00},
	{0x0b8c,0x00},
	{0x0b8d,0xff},
	{0x0b8e,0x7a},
	{0x0b8f,0x00},
	{0x0b90,0x00},
	{0x0b91,0xff},
	{0x0b92,0x7a},
	{0x0b93,0x00},
	{0x0b94,0x09},
	{0x0b95,0x12},
	{0x0b96,0x01},
	{0x0b97,0x00},
	{0x0b98,0x00},
	{0x0b99,0xff},
	{0x0b9a,0x7a},
	{0x0b9b,0x00},
	{0x0b9c,0xe0},
	{0x0b9d,0x1c},
	{0x0b9e,0x03},
	{0x0b9f,0x00},
	{0x0ba0,0x00},
	{0x0ba1,0xb4},
	{0x0ba2,0x09},
	{0x0ba3,0x00},
	{0x0ba4,0x00},
	{0x0ba5,0x74},
	{0x0ba6,0x09},
	{0x0ba7,0x00},
	{0x0ba8,0x00},
	{0x0ba9,0x14},
	{0x0baa,0x09},
	{0x0bab,0x00},
	{0x0bac,0x00},
	{0x0bad,0x94},
	{0x0bae,0x09},
	{0x0baf,0x00},
	{0x0bb0,0x00},
	{0x0bb1,0x54},
	{0x0bb2,0x09},
	{0x0bb3,0x00},
	{0x0bb4,0x00},
	{0x0bb5,0x84},
	{0x0bb6,0x01},
	{0x0bb7,0x00},
	{0x0bb8,0x00},
	{0x0bb9,0x81},
	{0x0bba,0x01},
	{0x0bbb,0x00},
	{0x0bbc,0xb8},
	{0x0bbd,0xc3},
	{0x0bbe,0x0f},
	{0x0bbf,0x00},
	{0x0bc0,0xb4},
	{0x0bc1,0xc0},
	{0x0bc2,0x0f},
	{0x0bc3,0x00},
	{0x0bc4,0x07},
	{0x0bc5,0xd3},
	{0x0bc6,0x0f},
	{0x0bc7,0x00},
	{0x0bc8,0x00},
	{0x0bc9,0x01},
	{0x0bca,0x09},
	{0x0bcb,0x00},
	{0x0bcc,0x07},
	{0x0bcd,0xe2},
	{0x0bce,0x0f},
	{0x0bcf,0x00},
	{0x0bd0,0x02},
	{0x0bd1,0x83},
	{0x0bd2,0x00},
	{0x0bd3,0x00},
	{0x0bd4,0x09},
	{0x0bd5,0x62},
	{0x0bd6,0x02},
	{0x0bd7,0x00},
	{0x0af5,0x00},
	{0x0ae9,0x22},
	{0x0aea,0x36},
	{0x0ae8,0x02},
	{0x0ae8,0x04},
	{0x0315,0xd7},

	{0x031c,0xe0},
	{0x031a,0x06},
	{0x0fe2,0x17},
	{0x0f9b,0xc9},
	{0x0900,0x10},
	{0x0fbf,0x36},
	{0x0fd2,0x36},
	{0x0fc1,0x00},
	{0x0fc2,0x4e},
	{0x0fcb,0x1c},
	{0x0fc4,0x10},
	{0x0fc5,0xb4},
	{0x0fd4,0x20},
	{0x0fd5,0xbc},
	{0x0fdc,0x01},
	{0x0fd6,0x23},
	{0x0901,0x00},
	{0x0fd3,0x07},
	{0x0fc0,0xb4},
	{0x0fc3,0xb8},
	{0x0511,0x85},
	{0x0da1,0x49},
	{0x0004,0x06},
	{0x001f,0x00},
	{0x0da2,0x00},
	{0x0da3,0x01},
	{0x04c0,0x51},
	{0x04c1,0x2a},
	{0x04c2,0x00},
	{0x0ad2,0x51},
	{0x0ad3,0x01},
	{0x0ad0,0x02},
	{0x0ad5,0x02},
	{0x0ad6,0x10},
	{0x0ad7,0x90},
	{0x0ad8,0x0c},
	{0x0ad9,0x40},
	{0x0ada,0x00},
	{0x0adb,0x00},
	{0x0adc,0x00},
	{0x0add,0x00},
	{0x0aae,0x00},
	{0x0aaf,0xe0},
	{0x0ab2,0x26},
	{0x0ab3,0x78},
	{0x0ab6,0x45},
	{0x0ab7,0x40},
	{0x0aab,0x45},
	{0x0aac,0x88},
	{0x0aa9,0x46},
	{0x0aaa,0x20},
	{0x0aa6,0x0a},
	{0x0aa7,0x46},
	{0x0aa8,0x98},
	{0x0ab0,0x48},
	{0x0ab1,0x90},
	{0x0a93,0x44},
	{0x0a92,0x40},
	{0x0a91,0xf6},
	{0x0a90,0xb7},
	{0x0a94,0x80},
	{0x0f08,0x03},
	{0x0f0a,0x60},
	{0x0519,0x06},
	{0x0247,0x0a},
	{0x0248,0x08},
	{0x024c,0x24},
	{0x0250,0xf8},
	{0x0580,0x01},
	{0x0516,0x0c},
	{0x0210,0x90},
	{0x0212,0x01},
	{0x0215,0x04},
	{0x0218,0x20},
	{0x021a,0x99},
	{0x021c,0x00},
	{0x021d,0x10},
	{0x0220,0x02},
	{0x0221,0x04},
	{0x0222,0x02},
	{0x0252,0x03},
	{0x0253,0x83},
	{0x02e6,0x03},
	{0x02e7,0x3a},
	{0x0f90,0x00},
	{0x0f91,0x14},
	{0x0f92,0x08},
	{0x0f93,0x5a},
	{0x0f94,0x08},
	{0x0346,0x00},
	{0x0347,0x28},
	{0x0348,0x10},
	{0x0349,0x90},
	{0x034a,0x0c},
	{0x034b,0x40},
	{0x0226,0x0c},
	{0x0227,0xc0},
	{0x0340,0x06},
	{0x0341,0xa0},
	{0x0202,0x06},
	{0x0203,0x90},
	{0x0342,0x0a},
	{0x0343,0x50},
	{0x024d,0x01},
	{0x024e,0x0f},
	{0x024f,0x04},
	{0x0ec3,0xff},
	{0x0ec4,0x99},
	{0x0eca,0x00},
	{0x0ecd,0x00},
	{0x0e59,0x00},
	{0x0fa7,0x3f},
	{0x0fbc,0xad},
	{0x0fc9,0x60},
	{0x0fa9,0x65},
	{0x0fab,0x40},
	{0x0fe0,0x07},
	{0x0e17,0x2f},
	{0x0e35,0x2f},
	{0x0e03,0x00},
	{0x0e04,0x68},
	{0x0e05,0x00},
	{0x0e06,0x42},
	{0x0e1b,0x00},
	{0x0e1c,0x7d},
	{0x0fb3,0xef},
	{0x0fb5,0x31},
	{0x0fd0,0x05},
	{0x0fd1,0x1a},
	{0x0f6d,0x0e},
	{0x0f9d,0x1c},
	{0x0e0d,0x36},
	{0x0e1e,0x36},
	{0x0e72,0x36},
	{0x0e73,0x36},
	{0x0e0e,0x1d},
	{0x0e1f,0x00},
	{0x0e20,0xc2},
	{0x0e2c,0x00},
	{0x0e2d,0xf8},
	{0x02d0,0x01},
	{0x0f2f,0x03},
	{0x0f38,0x1f},
	{0x0106,0x00},
	{0x0595,0x09},
	{0x05a3,0x09},
	{0x05a4,0x0c},
	{0x0590,0x00},
	{0x0591,0x8c},
	{0x05a5,0x00},
	{0x05a6,0x00},
	{0x05a7,0x10},
	{0x05ac,0x00},
	{0x05ad,0x01},
	{0x05ae,0x00},
	{0x0800,0x05},
	{0x0801,0x06},
	{0x0802,0x09},
	{0x0803,0x0e},
	{0x0804,0x14},
	{0x0805,0x1c},
	{0x0806,0x28},
	{0x0807,0x39},
	{0x0808,0x0e},
	{0x0809,0x39},
	{0x080a,0x0e},
	{0x080b,0x3a},
	{0x080c,0x0e},
	{0x080d,0x70},
	{0x080e,0x0e},
	{0x080f,0x71},
	{0x0810,0x0e},
	{0x0811,0x0b},
	{0x0812,0x0e},
	{0x0813,0x1d},
	{0x0814,0x0e},
	{0x0815,0x51},
	{0x0816,0x00},
	{0x0817,0x00},
	{0x0818,0x00},
	{0x0819,0x00},
	{0x081a,0x00},
	{0x081b,0x00},
	{0x081c,0x00},
	{0x081d,0x00},
	{0x081e,0x00},
	{0x081f,0x00},
	{0x0820,0x03},
	{0x0821,0x08},
	{0x0822,0x03},
	{0x0823,0x08},
	{0x0824,0x03},
	{0x0825,0x03},
	{0x0826,0x00},
	{0x0827,0x00},
	{0x0828,0x00},
	{0x0829,0x00},
	{0x082a,0x00},
	{0x082b,0x00},
	{0x082c,0x03},
	{0x082d,0x03},
	{0x082e,0x03},
	{0x082f,0x03},
	{0x0830,0x03},
	{0x0831,0x03},
	{0x0832,0x00},
	{0x0833,0x00},
	{0x0834,0x00},
	{0x0835,0x00},
	{0x0836,0x00},
	{0x0837,0x00},
	{0x0838,0x02},
	{0x0839,0x1d},
	{0x083a,0x02},
	{0x083b,0x1d},
	{0x083c,0x03},
	{0x083d,0x03},
	{0x083e,0x00},
	{0x083f,0x00},
	{0x0840,0x00},
	{0x0841,0x00},
	{0x0842,0x00},
	{0x0843,0x00},
	{0x0844,0x02},
	{0x0845,0x15},
	{0x0846,0x02},
	{0x0847,0x15},
	{0x0848,0x03},
	{0x0849,0x03},
	{0x084a,0x00},
	{0x084b,0x00},
	{0x084c,0x00},
	{0x084d,0x00},
	{0x084e,0x00},
	{0x084f,0x00},
	{0x0850,0x02},
	{0x0851,0x0e},
	{0x0852,0x02},
	{0x0853,0x0e},
	{0x0854,0x03},
	{0x0855,0x03},
	{0x0856,0x00},
	{0x0857,0x00},
	{0x0858,0x00},
	{0x0859,0x00},
	{0x085a,0x00},
	{0x085b,0x00},
	{0x085c,0x02},
	{0x085d,0x05},
	{0x085e,0x02},
	{0x085f,0x05},
	{0x0860,0x03},
	{0x0861,0x03},
	{0x0862,0x00},
	{0x0863,0x00},
	{0x0864,0x00},
	{0x0865,0x00},
	{0x0866,0x00},
	{0x0867,0x00},
	{0x0868,0x01},
	{0x0869,0x1a},
	{0x086a,0x01},
	{0x086b,0x1a},
	{0x086c,0x03},
	{0x086d,0x03},
	{0x086e,0x00},
	{0x086f,0x00},
	{0x0870,0x00},
	{0x0871,0x00},
	{0x0872,0x00},
	{0x0873,0x00},
	{0x0874,0x01},
	{0x0875,0x0e},
	{0x0876,0x01},
	{0x0877,0x0e},
	{0x0878,0x03},
	{0x0879,0x03},
	{0x087a,0x00},
	{0x087b,0x00},
	{0x087c,0x00},
	{0x087d,0x00},
	{0x087e,0x00},
	{0x087f,0x00},
	{0x0880,0x00},
	{0x0881,0x20},
	{0x0882,0x00},
	{0x0883,0x20},
	{0x0884,0x03},
	{0x0885,0x03},
	{0x0886,0x00},
	{0x0887,0x00},
	{0x0888,0x00},
	{0x0889,0x00},
	{0x088a,0x00},
	{0x088b,0x00},
	{0x088c,0x00},
	{0x088d,0x00},
	{0x088e,0x00},
	{0x088f,0x00},
	{0x0890,0x00},
	{0x0891,0x00},
	{0x0892,0x00},
	{0x0893,0x00},
	{0x0894,0x00},
	{0x0895,0x01},
	{0x0896,0x00},
	{0x0897,0x00},
	{0x0898,0x01},
	{0x0899,0x01},
	{0x089a,0x6b},
	{0x089b,0x00},
	{0x089c,0x02},
	{0x089d,0x01},
	{0x089e,0xfb},
	{0x089f,0x00},
	{0x08a0,0x03},
	{0x08a1,0x02},
	{0x08a2,0xd4},
	{0x08a3,0x00},
	{0x08a4,0x04},
	{0x08a5,0x03},
	{0x08a6,0xf5},
	{0x08a7,0x00},
	{0x08a8,0x05},
	{0x08a9,0x05},
	{0x08aa,0xb0},
	{0x08ab,0x00},
	{0x08ac,0x06},
	{0x08ad,0x07},
	{0x08ae,0xf6},
	{0x08af,0x00},
	{0x08b0,0x07},
	{0x08b1,0x0b},
	{0x08b2,0x30},
	{0x08b3,0x80},
	{0x08b4,0x06},
	{0x08b5,0x0f},
	{0x08b6,0x80},
	{0x08b7,0x80},
	{0x08b8,0x07},
	{0x05ac,0x01},
	{0x0209,0x01},
	{0x020a,0x24},
	{0x0fa5,0x00},
	{0x0204,0x04},
	{0x0205,0x00},
	{0x0d03,0x00},
	{0x0d04,0x76},
	{0x008b,0x00},
	{0x0040,0x22},
	{0x0042,0x01},
	{0x0030,0x04},
	{0x0031,0x0b},
	{0x0032,0x00},
	{0x0033,0x01},
	{0x0092,0x06},
	{0x0093,0x3c},
	{0x0058,0x00},
	{0x0059,0x00},
	{0x005a,0x00},
	{0x005b,0x00},
	{0x005c,0x00},
	{0x005d,0x00},
	{0x005e,0x00},
	{0x005f,0x00},
	{0x0074,0x40},
	{0x0077,0x00},
	{0x0073,0xc1},
	{0x0016,0x24},
	{0x0005,0x0b},
	{0x0006,0x38},
	{0x0007,0x08},
	{0x0008,0x3b},
	{0x0009,0x00},
	{0x000c,0x00},
	{0x000d,0xa5},
	{0x000e,0x3f},
	{0x000f,0x58},
	{0x0010,0xdf},
	{0x0011,0xb4},
	{0x0012,0x64},
	{0x0013,0x52},
	{0x0014,0x1e},
	{0x0015,0xd8},
	{0x0019,0x04},
	{0x0da0,0x0f},
	{0x04c2,0x87},
	{0x04c3,0x00},
	{0x04ca,0x46},
	{0x04cb,0x0f},
	{0x0430,0x87},
	{0x0431,0x08},
	{0x0432,0x08},
	{0x0433,0x10},
	{0x0434,0x87},
	{0x0435,0x0c},
	{0x0436,0x16},
	{0x0437,0x10},
	{0x0438,0x83},
	{0x0439,0x0e},
	{0x043a,0x03},
	{0x043b,0x10},
	{0x043c,0x83},
	{0x043d,0x12},
	{0x043e,0x1a},
	{0x043f,0x10},
	{0x0440,0x63},
	{0x0441,0x07},
	{0x0442,0x18},
	{0x0443,0x10},
	{0x0444,0x63},
	{0x0445,0x07},
	{0x0446,0x18},
	{0x0447,0x10},
	{0x0359,0x00},
	{0x0351,0x00},
	{0x0352,0x80},
	{0x0353,0x00},
	{0x0354,0xc8},
	{0x034c,0x0f},
	{0x034d,0x00},
	{0x034e,0x0b},
	{0x034f,0x40},
	{0x00c8,0x00},
	{0x00c9,0x10},
	{0x00ca,0x00},
	{0x00cb,0x18},
	{0x00cc,0x0c},
	{0x00cd,0x30},
	{0x00ce,0x10},
	{0x00cf,0x78},
	{0x00d8,0x00},
	{0x00d9,0x10},
	{0x00da,0x00},
	{0x00db,0x28},
	{0x00dc,0x0c},
	{0x00dd,0x30},
	{0x00de,0x10},
	{0x00df,0x68},
	{0x00c1,0x00},
	{0x00c2,0x03},
	{0x0110,0x04},
	{0x0117,0x20},
	{0x0120,0x10},
	{0x0123,0x36},
	{0x0124,0x36},
	{0x012a,0x2b},
	{0x012b,0x00},
	{0x012c,0x00},
	{0x012d,0x0a},
	{0x012e,0x08},
	{0x012f,0x8a},
	{0x0113,0x02},
	{0x0119,0x05},
	{0x011a,0x05},
	{0x0128,0x2b},
	{0x01a8,0xff},
	{0x01a0,0xff},
	{0x01a1,0x0d},
	{0x01a2,0x0f},
	{0x01a3,0x35},
	{0x01a4,0x01},
	{0x01a5,0x10},
	{0x01a6,0x0d},
	{0x01a7,0x38},
	{0x01a9,0x0f},
	{0x01aa,0x18},
	{0x01ab,0x0d},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0a90,0x41},
	{0x0a94,0x80},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0a90,0x00},
	{0x0314,0x70},
	{0x0089,0x02},
	{0x00bc,0x03},
	{0x00bd,0x00},
	{0x0ad0,0x03},
	{0x0004,0x07},
	{0x0da2,0x01},
	{0x0080,0x30},
	{0x0061,0x40},
	{0x0084,0x01},

	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the gc13b0_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc13b0_win_sizes[] = {
	/* 3840*2880 [0] */
	{
		.width          = 3840,
		.height         = 2880,
		.fps            = 20 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc13b0_init_regs_3840_3120_20fps_mipi_2lane,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &gc13b0_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc13b0_stream_on_mipi[] = {
	{0x0100,0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list gc13b0_stream_off_mipi[] = {
	{0x0100,0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc13b0_read(struct tx_isp_subdev *sd, unsigned char reg,
		  unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int gc13b0_write(struct tx_isp_subdev *sd, unsigned char reg,
		   unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int gc13b0_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc13b0_read(sd, vals->reg_num, &val);
			/* ISP_WARNING("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc13b0_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc13b0_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc13b0_read(struct tx_isp_subdev *sd, uint16_t reg,
		  unsigned char *value) {
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr   = client->addr,
			.flags  = 0,
			.len    = 2,
			.buf    = buf,
		},
		[1] = {
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.len    = 1,
			.buf    = value,
		}
	};

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int gc13b0_write(struct tx_isp_subdev *sd, uint16_t reg,
		   unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr   = client->addr,
		.flags  = 0,
		.len    = 3,
		.buf    = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int gc13b0_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc13b0_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc13b0_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc13b0_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int gc13b0_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = ISP_SUCCESS;

	rate = private_clk_get_rate(sensor->mclk);
	if(((rate / 1000) % (mclk / 1000)) != 0) {
		ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
		sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka)) {
			pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclka);
			if(((rate / 1000) % (mclk / 1000)) != 0) {
				switch(mclk) {
				case 12000000:
				case 24000000:
					private_clk_set_rate(sclka, 1200000000);
					break;
				case 27000000:
				case 37125000:
					private_clk_set_rate(sclka, 1188000000);
					break;
				default:
					ret = -1;
					goto error;
				}
			} else {
				if ((mclk != 12000000) && (mclk != 27000000) && (mclk != 37125000) && (mclk != 24000000)) {
					ret = -1;
					goto error;
				}
			}
		}
	}

	private_clk_set_rate(sensor->mclk, mclk);
	private_clk_prepare_enable(sensor->mclk);

error:
	return ret;
}

static int gc13b0_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;

	sensor->video.fps.min = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

#ifdef SENSOR_WDR_2_FRAME
	sensor->video.wdr = 1;
#else
	sensor->video.wdr = 0;
#endif

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int gc13b0_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &gc13b0_win_sizes[0];
		gc13b0_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc13b0_attr.again.value = 0;
		gc13b0_attr.again.max = 262144;
		gc13b0_attr.again.min = 0;
		gc13b0_attr.again.apply_delay = 2;

		gc13b0_attr.integration_time.value = 0xad0;
		gc13b0_attr.integration_time.max = 0x6a0 - 56;
		gc13b0_attr.integration_time.min = 8;
		gc13b0_attr.integration_time.apply_delay = 2;

		gc13b0_attr.total_width = 0xa50;
		gc13b0_attr.total_height = 0x6a0;

		gc13b0_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		gc13b0_attr.hcg.base_gain = ;
		gc13b0_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc13b0_attr.again_short.value = ;
		gc13b0_attr.again_short.max = ;
		gc13b0_attr.again_short.min = ;
		gc13b0_attr.again_short.apply_delay = ;

		gc13b0_attr.integration_time_short.value = ;
		gc13b0_attr.integration_time_short.max = ;
		gc13b0_attr.integration_time_short.min = ;
		gc13b0_attr.integration_time_short.apply_delay = ;

		gc13b0_attr.wdr_cache = wdr_line * gc13b0_attr.total_width;

#ifdef SENSOR_HCG
		gc13b0_attr.hcg_short.base_gain = ;
		gc13b0_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc13b0_attr.mipi)), (void *)(&gc13b0_mipi_linear_2lane), sizeof(gc13b0_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int gc13b0_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	gc13b0_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc13b0_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc13b0_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc13b0_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc13b0_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc13b0_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

#ifndef ZRT_SENSOR_WITHOUT_INIT
	switch (info->mclk) {
	case TISP_SENSOR_MCLK0:
		sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
		sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
		sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
		set_sensor_mclk_function(1);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	ret = gc13b0_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.attr->fsync_attr.call_times = 1;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	gc13b0_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc13b0_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc13b0_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc13b0_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc13b0_g_chip_ident(struct tx_isp_subdev *sd,
				 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	gc13b0_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "gc13b0_reset");
		if (!ret) {
			/* *(volatile u32 *)0xb0010018 = 0x1 << 20; */
			/* *(volatile u32 *)0xb0010024 = 0x1 << 20; */
			/* *(volatile u32 *)0xb0010038 = 0x1 << 20; */
			/* *(volatile u32 *)0xb0010048 = 0x1 << 20; */
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(10);
			/* *(volatile u32 *)0xb0010018 = 0x1 << 20; */
			/* *(volatile u32 *)0xb0010024 = 0x1 << 20; */
			/* *(volatile u32 *)0xb0010038 = 0x1 << 20; */
			/* *(volatile u32 *)0xb0010044 = 0x1 << 20; */
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "gc13b0_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = gc13b0_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc13b0 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
#ifdef AUTHOR
	ISP_WARNING("gc13b0 version is %s from %s\n", TVERSION, AUTHOR);
#else
	ISP_WARNING("gc13b0 version is %s\n", TVERSION);
#endif /* AUTHOR */
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", gc13b0_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "gc13b0", sizeof("gc13b0"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc13b0_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc13b0_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.fps.value = wsize->fps;
		sensor->video.fps.max = wsize->fps;
		sensor->video.fps.apply_delay = 1;
		ret = gc13b0_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int gc13b0_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = ISP_SUCCESS;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc13b0_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc13b0_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc13b0_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int gc13b0_fsync(struct tx_isp_subdev *sd, struct tx_isp_frame_sync *sync)
{
#ifndef SENSOR_SLAVE
	/* master */
#else
	/* slave */
#endif /* SENSOR_SLAVE */

	return 0;
}

static int gc13b0_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = gc13b0_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc13b0_write_array(sd, gc13b0_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc13b0 stream on\n");
		}

	} else {
		ret = gc13b0_write_array(sd, gc13b0_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("gc13b0 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc13b0_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	gc13b0_write(sd, 0x0202, (it >> 8) & 0xff);
	gc13b0_write(sd, 0x0203, it & 0xff);

	gc13b0_write(sd, 0x0204, (again >> 8) & 0xff);
	gc13b0_write(sd, 0x0205, again & 0xff);

	return ret;
}
#else
static int gc13b0_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	gc13b0_write(sd, 0x0202, (value >> 8) & 0xff);
	gc13b0_write(sd, 0x0203, value & 0xff);

	return ret;
}

static int gc13b0_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	gc13b0_write(sd, 0x0204, (value >> 8) & 0xff);
	gc13b0_write(sd, 0x0205, value & 0xff);

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc13b0_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc13b0_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc13b0_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = gc13b0_attr_set(sd, wsize);
	}

	return ret;
}

static int gc13b0_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;
	int ret = ISP_SUCCESS;

	switch (info->default_boot) {
	case 0:
		sclk = 0xa50 * 0x6a0 * 20;  /**< HTS * VTS * FPS */
		max_fps = 20;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
		return -1;
	}

	val = 0;
	ret += gc13b0_read(sd, 0x0342, &val);
	hts = val << 8;
	val = 0;
	ret += gc13b0_read(sd, 0x0343, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: gc13b0 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	gc13b0_write(sd, 0x0340, (unsigned char) (vts & 0xff));
	gc13b0_write(sd, 0x0341, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: gc13b0_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 56;
#else
	/* WDR mode */
	switch (info->default_boot) {
	case 0:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - xx;
		break;
	case 1:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - xx;
		sensor->video.attr->integration_time_short.max = vts - xx;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}
#endif /* SENSOR_WDR_2_FRAME */

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}

static int gc13b0_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;

	par->drop_frame = ;
	par->reset = ;

	/* 2'b01:mirror,2'b10:filp */
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		...;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		...;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		...;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		...;
		break;
	}

	sensor->video.hvflip_mode = par->hvflip;
	gc13b0_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_PM
static int gc13b0_pm(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;

	switch (data->cmd) {
	case TX_SENSOR_PM_RESUME:
		...
		if (ret < 0) {
			ISP_WARNING("TX_SENSOR_PM_RESUME error !!!\n", __func__, __LINE__);
		}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_RESUME\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_SUSPEND:
		...
		if (ret < 0) {
			ISP_WARNING("TX_SENSOR_PM_SUSPEND error !!!\n", __func__, __LINE__);
		}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_SUSPEND !!!\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_PREPARE:
		...
		if (ret < 0) {
			ISP_WARNING("TX_SENSOR_PM_PREPARE error !!!\n", __func__, __LINE__);
		}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_PREPARE !!!\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_COMPLETE:
		...
		if (ret < 0) {
			ISP_WARNING("TX_SENSOR_PM_SUSPEND error !!!\n", __func__, __LINE__);
		}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_COMPLETE !!!\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_STREAMON:
		...
		if (ret < 0) {
			ISP_WARNING("TX_SENSOR_PM_STREAMON error !!!\n", __func__, __LINE__);
		}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_STREAMON !!!\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_STREAMOFF:
		...
		if (ret < 0) {
			ISP_WARNING("[%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
		}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_STREAMOFF !!!\n", __func__, __LINE__); */
		break;
	default:
		ISP_WARNING("[%s %d] Don't Support this function !!! \n", __func__, __LINE__);
		return -EINVAL;
		break;
	}

	return ret;
}
#endif /* SENSOR_PM */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int gc13b0_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int gc13b0_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int gc13b0_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc13b0_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = gc13b0_write_array(sd, gc13b0_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		gc13b0_setting_select(sd, 1);
		gc13b0_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		gc13b0_setting_select(sd, 0);
		gc13b0_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int gc13b0_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = gc13b0_write_array(sd, wsize->regs);
	ret = gc13b0_write_array(sd, gc13b0_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc13b0_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = gc13b0_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = gc13b0_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = gc13b0_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc13b0_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc13b0_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc13b0_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = gc13b0_write_array(sd, gc13b0_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = gc13b0_write_array(sd, gc13b0_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc13b0_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = gc13b0_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_PM
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret =  gc13b0_pm(sd, arg);
		break;
#endif /* SENSOR_PM */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = gc13b0_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = gc13b0_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = gc13b0_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = gc13b0_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = gc13b0_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc13b0_core_ops = {
	.g_chip_ident = gc13b0_g_chip_ident,
	.reset = gc13b0_reset,
	.init = gc13b0_init,
	.g_register = gc13b0_g_register,
	.s_register = gc13b0_s_register,
	.fsync = gc13b0_fsync,
};

static struct tx_isp_subdev_video_ops gc13b0_video_ops = {
	.s_stream = gc13b0_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc13b0_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = gc13b0_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc13b0_ops = {
	.core = &gc13b0_core_ops,
	.video = &gc13b0_video_ops,
	.sensor = &gc13b0_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "gc13b0",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc13b0_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	sd = &sensor->sd;
	video = &sensor->video;

#ifdef SENSOR_MIR_FLIP
	sensor->video.hvflip_mode = TX_ISP_SENSOR_HVFLIP_NOMAL;
#endif /* SENSOR_MIR_FLIP */
	sensor->video.attr = &gc13b0_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc13b0_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc13b0\n");

	return 0;
}

static int gc13b0_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;

	if (info->rst_gpio != -1)
		private_gpio_free(info->rst_gpio);
	if (info->pwdn_gpio != -1)
		private_gpio_free(info->pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);

	kfree(sensor);

	return 0;
}

static const struct i2c_device_id gc13b0_id[] = {
	{"gc13b0", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc13b0_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver gc13b0_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "gc13b0",
	},
	.probe          = gc13b0_probe,
	.remove         = gc13b0_remove,
	.id_table       = gc13b0_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc13b0(void) {
	return private_i2c_add_driver(&gc13b0_driver);
}

static __exit void exit_gc13b0(void) {
	private_i2c_del_driver(&gc13b0_driver);
}

module_init(init_gc13b0);
module_exit(exit_gc13b0);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc13b0(void) {
	return private_i2c_add_driver(&gc13b0_driver);
}

static void exit_first_gc13b0(void) {
	private_i2c_del_driver(&gc13b0_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "gc13b0",
	.i2c_addr = 0x4a,
	.width = 3840,
	.height = 2880,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_gc13b0,
	.exit_sensor = exit_first_gc13b0
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A gc13b0 low-level CIS driver");
MODULE_LICENSE("GPL");
