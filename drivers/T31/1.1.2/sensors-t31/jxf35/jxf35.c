/*
 * jxf35.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#define JXF35_CHIP_ID_H	(0x0f)
#define JXF35_CHIP_ID_L	(0x35)
#define JXF35_REG_END	0xff
#define JXF35_REG_DELAY	0xfe
#define JXF35_SUPPORT_30FPS_SCLK (86400000)
#define JXF35_SUPPORT_15FPS_SCLK (43200000)
#define JXF35_SUPPORT_55FPS_SCLK (39600000)
#define JXF35_SUPPORT_30FPS_MIPI_SCLK (43200000)
#define JXF35_SUPPORT_60FPS_MIPI_SCLK (39591300)
#define JXF35_SUPPORT_180_60FPS_MIPI_SCLK (51840000)
#define JXF35_SUPPORT_VGA_SCLK (43189920)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20200930a"
typedef enum {
	SENSOR_RES_30 = 30,
	SENSOR_RES_180 = 180,
	SENSOR_RES_200 = 200,
} Sensor_RES;

typedef enum {
	PS5270_SENSOR_MAX_FPS_15 = 15,
	PS5270_SENSOR_MAX_FPS_30 = 30,
	PS5270_SENSOR_MAX_FPS_55 = 55,
	PS5270_SENSOR_MAX_FPS_60 = 60,
	PS5270_SENSOR_MAX_FPS_120 = 120,
} Sensor_FPS;

/* VGA@120fps: insmod sensor_jxf35_t31.ko data_interface=1 sensor_resolution=30 sensor_max_fps=120  */
/* 1080p@25fps: insmod sensor_jxf35_t31.ko data_interface=1 sensor_max_fps=30 sensor_resolution=200 */
/* 1080p@60fps: insmod sensor_jxf35_t31.ko data_interface=1 sensor_max_fps=60 sensor_resolution=200 */
/* 1728x972@15fps: insmod sensor_jxf35_t31.ko data_interface=1 sensor_max_fps=15 sensor_resolution=180 */
/* 1728x972@55fps: insmod sensor_jxf35_t31.ko data_interface=1 sensor_max_fps=55 sensor_resolution=180 */

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int sensor_max_fps = PS5270_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int sensor_resolution = SENSOR_RES_200;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor Resolution");

static unsigned char r2f_val = 0x64;
static unsigned char r0c_val = 0x40;
static unsigned char r80_val = 0x02;


struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut jxf35_again_lut[] = {
	{0x0, 0 },
	{0x1, 5731 },
	{0x2, 11136},
	{0x3, 16248},
	{0x4, 21097},
	{0x5, 25710},
	{0x6, 30109},
	{0x7, 34312},
	{0x8, 38336},
	{0x9, 42195},
	{0xa, 45904},
	{0xb, 49472},
	{0xc, 52910},
	{0xd, 56228},
	{0xe, 59433},
	{0xf, 62534},
	{0x10, 65536},
	{0x11, 71267},
	{0x12, 76672},
	{0x13, 81784},
	{0x14, 86633},
	{0x15, 91246},
	{0x16, 95645},
	{0x17, 99848},
	{0x18, 103872},
	{0x19, 107731},
	{0x1a, 111440},
	{0x1b, 115008},
	{0x1c, 118446},
	{0x1d, 121764},
	{0x1e, 124969},
	{0x1f, 128070},
	{0x20, 131072},
	{0x21, 136803},
	{0x22, 142208},
	{0x23, 147320},
	{0x24, 152169},
	{0x25, 156782},
	{0x26, 161181},
	{0x27, 165384},
	{0x28, 169408},
	{0x29, 173267},
	{0x2a, 176976},
	{0x2b, 180544},
	{0x2c, 183982},
	{0x2d, 187300},
	{0x2e, 190505},
	{0x2f, 193606},
	{0x30, 196608},
	{0x31, 202339},
	{0x32, 207744},
	{0x33, 212856},
	{0x34, 217705},
	{0x35, 222318},
	{0x36, 226717},
	{0x37, 230920},
	{0x38, 234944},
	{0x39, 238803},
	{0x3a, 242512},
	{0x3b, 246080},
	{0x3c, 249518},
	{0x3d, 252836},
	{0x3e, 256041},
	{0x3f, 259142},
};

static struct regval_list jxf35_init_regs_vga_120fps_mipi[] = {
	{0x12, 0x43},
	{0x48, 0x96},
	{0x48, 0x16},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x48},
	{0x11, 0x80},
	{0x0D, 0xF0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x18},
	{0x57, 0x60},
	{0x64, 0xE0},
	{0x20, 0xA2},
	{0x21, 0x02},
	{0x22, 0x16},
	{0x23, 0x02},
	{0x24, 0xA0},
	{0x25, 0xE0},
	{0x26, 0x10},
	{0x27, 0x08},
	{0x28, 0x19},
	{0x29, 0x02},
	{0x2A, 0x00},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x10},
	{0x2E, 0x14},
	{0x2F, 0x44},
	{0x41, 0xC8},
	{0x42, 0x13},
	{0x46, 0x03},
	{0x76, 0x20},
	{0x77, 0x03},
	{0x80, 0x06},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x70, 0xDD},
	{0x71, 0xCB},
	{0x72, 0xD5},
	{0x73, 0x59},
	{0x74, 0x02},
	{0x78, 0x94},
	{0x89, 0x81},
	{0x6E, 0x2C},
	{0x84, 0x20},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x32, 0x4F},
	{0x33, 0x58},
	{0x34, 0x5F},
	{0x35, 0x5F},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0x70},
	{0x3D, 0x8F},
	{0x3E, 0xFF},
	{0x3F, 0x85},
	{0x40, 0xFF},
	{0x56, 0x32},
	{0x59, 0x67},
	{0x85, 0x3C},
	{0x8A, 0x04},
	{0x91, 0x10},
	{0x9C, 0xE1},
	{0x5A, 0x09},
	{0x5C, 0x4C},
	{0x5D, 0xF4},
	{0x5E, 0x1E},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x66, 0x04},
	{0x67, 0x30},
	{0x6A, 0x12},
	{0x7A, 0xA0},
	{0x9D, 0x10},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x47, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x8F, 0x80},
	{0x90, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0x69, 0x74},
	{0x65, 0x02},
	{0x81, 0x74},
	{0x19, 0x20},
	{0x12, 0x03},

	{JXF35_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf35_init_regs_1920_1080_30fps_mipi[] = {
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x0D, 0xF0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x18},
	{0x57, 0x60},
	{0x64, 0xE0},
	{0x20, 0x00},
	{0x21, 0x05},
	{0x22, 0x46},
	{0x23, 0x05},
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0x0F},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0x00},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x14},
	{0x2F, 0x44},
	{0x41, 0xC8},
	{0x42, 0x13},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x80, 0x06},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x70, 0x6D},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x36},
	{0x74, 0x02},
	{0x78, 0x9E},
	{0x89, 0x81},
	{0x6E, 0x2C},
	{0x32, 0x4F},
	{0x33, 0x58},
	{0x34, 0x5F},
	{0x35, 0x5F},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0x70},
	{0x3D, 0x8F},
	{0x3E, 0xFF},
	{0x3F, 0x85},
	{0x40, 0xFF},
	{0x56, 0x32},
	{0x59, 0x67},
	{0x85, 0x3C},
	{0x8A, 0x04},
	{0x91, 0x10},
	{0x9C, 0xE1},
	{0x5A, 0x09},
	{0x5C, 0x4C},
	{0x5D, 0xF4},
	{0x5E, 0x1E},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x66, 0x04},
	{0x67, 0x30},
	{0x6A, 0x12},
	{0x7A, 0xA0},
	{0x9D, 0x10},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x47, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x8F, 0x80},
	{0x90, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0x69, 0x74},
	{0x65, 0x02},
	{0x81, 0x74},
	{0x19, 0x20},
	{0x12, 0x00},
	{JXF35_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf35_init_regs_1728_972_15fps_mipi[] = {
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x0D, 0xF0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x18},
	{0x57, 0x60},
	{0x64, 0xE0},
	{0x20, 0x00},
	{0x21, 0x0A},
	{0x22, 0x65},
	{0x23, 0x04},
	{0x24, 0x60},
	{0x25, 0xCC},
	{0x26, 0x33},
	{0x27, 0x0C},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0x00},
	{0x2B, 0x12},
	{0x2C, 0x34},
	{0x2D, 0x0E},
	{0x2E, 0x07},
	{0x2F, 0x44},
	{0x41, 0x64},
	{0x42, 0x13},
	{0x46, 0x01},
	{0x76, 0x70},
	{0x77, 0x08},
	{0x80, 0x06},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x70, 0x6D},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x36},
	{0x74, 0x02},
	{0x78, 0x9E},
	{0x89, 0x81},
	{0x6E, 0x2C},
	{0x32, 0x4F},
	{0x33, 0x58},
	{0x34, 0x5F},
	{0x35, 0x5F},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0x70},
	{0x3D, 0x8F},
	{0x3E, 0xFF},
	{0x3F, 0x85},
	{0x40, 0xFF},
	{0x56, 0x32},
	{0x59, 0x67},
	{0x85, 0x3C},
	{0x8A, 0x04},
	{0x91, 0x10},
	{0x9C, 0xE1},
	{0x5A, 0x09},
	{0x5C, 0x4C},
	{0x5D, 0xF4},
	{0x5E, 0x1E},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x66, 0x04},
	{0x67, 0x30},
	{0x6A, 0x12},
	{0x7A, 0xA0},
	{0x9D, 0x10},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x47, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x8F, 0x80},
	{0x90, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0x69, 0x74},
	{0x65, 0x02},
	{0x81, 0x74},
	{0x19, 0x20},
	{0x12, 0x00},
	{JXF35_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf35_init_regs_1728_972_55fps_mipi[] = {
	{0x12, 0x40},
	{0x48, 0x96},
	{0x48, 0x16},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x42},
	{0x11, 0x80},
	{0x0D, 0xF0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x18},
	{0x57, 0x60},
	{0x64, 0xE0},
	{0x20, 0x80},
	{0x21, 0x02},
	{0x22, 0x65},
	{0x23, 0x04},
	{0x24, 0xB0},
	{0x25, 0xCC},
	{0x26, 0x31},
	{0x27, 0x07},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0x00},
	{0x2B, 0x12},
	{0x2C, 0x32},
	{0x2D, 0x0E},
	{0x2E, 0x07},
	{0x2F, 0x44},
	{0x41, 0x68},
	{0x42, 0x13},
	{0x46, 0x01},
	{0x76, 0x70},
	{0x77, 0x08},
	{0x80, 0x06},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x70, 0xDD},
	{0x71, 0xCB},
	{0x72, 0xD5},
	{0x73, 0x59},
	{0x74, 0x02},
	{0x78, 0x94},
	{0x89, 0x81},
	{0x6E, 0x2C},
	{0x84, 0x20},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x32, 0x4F},
	{0x33, 0x58},
	{0x34, 0x5F},
	{0x35, 0x5F},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0x70},
	{0x3D, 0x8F},
	{0x3E, 0xFF},
	{0x3F, 0x85},
	{0x40, 0xFF},
	{0x56, 0x32},
	{0x59, 0x67},
	{0x85, 0x3C},
	{0x8A, 0x04},
	{0x91, 0x10},
	{0x9C, 0xE1},
	{0x5A, 0x09},
	{0x5C, 0x4C},
	{0x5D, 0xF4},
	{0x5E, 0x1E},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x66, 0x04},
	{0x67, 0x30},
	{0x6A, 0x12},
	{0x7A, 0xA0},
	{0x9D, 0x10},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x47, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x8F, 0x80},
	{0x90, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0x69, 0x74},
	{0x65, 0x02},
	{0x81, 0x74},
	{0x19, 0x20},
	{0x12, 0x00},
	{JXF35_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf35_init_regs_1920_1080_30fps_dvp[] = {
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x0D, 0xF0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x18},
	{0x57, 0x60},
	{0x64, 0xE0},
	{0x20, 0x00},
	{0x21, 0x05},
	{0x22, 0x65},
	{0x23, 0x04},
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0x0F},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0x00},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x14},
	{0x2F, 0x44},
	{0x41, 0xC8},
	{0x42, 0x13},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x80, 0x06},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x70, 0x6D},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x36},
	{0x74, 0x02},
	{0x78, 0x9E},
	{0x89, 0x81},
	{0x6E, 0x2C},
	{0x32, 0x4F},
	{0x33, 0x58},
	{0x34, 0x5F},
	{0x35, 0x5F},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0x70},
	{0x3D, 0x8F},
	{0x3E, 0xFF},
	{0x3F, 0x85},
	{0x40, 0xFF},
	{0x56, 0x32},
	{0x59, 0x67},
	{0x85, 0x3C},
	{0x8A, 0x04},
	{0x91, 0x10},
	{0x9C, 0xE1},
	{0x5A, 0x09},
	{0x5C, 0x4C},
	{0x5D, 0xF4},
	{0x5E, 0x1E},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x66, 0x04},
	{0x67, 0x30},
	{0x6A, 0x12},
	{0x7A, 0xA0},
	{0x9D, 0x10},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x47, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x8F, 0x80},
	{0x90, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0x69, 0x74},
	{0x65, 0x02},
	{0x81, 0x74},
	{0x19, 0x20},
	{0x12, 0x00},
	{JXF35_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf35_init_regs_1920_1080_60fps_mipi[] = {
	{0x12, 0x40},
	{0x48, 0x96},
	{0x48, 0x16},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x42},
	{0x11, 0x80},
	{0x0D, 0xF0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x18},
	{0x57, 0x60},
	{0x64, 0xE0},
	{0x20, 0x53},
	{0x21, 0x02},
	{0x22, 0x55},
	{0x23, 0x04},
	{0x24, 0xE0},
	{0x25, 0x38},
	{0x26, 0x41},
	{0x27, 0x07},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0x00},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x14},
	{0x2F, 0x44},
	{0x41, 0xC8},
	{0x42, 0x13},
	{0x46, 0x01},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x80, 0x06},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x70, 0xDD},
	{0x71, 0xCB},
	{0x72, 0xD5},
	{0x73, 0x59},
	{0x74, 0x02},
	{0x78, 0x94},
	{0x89, 0x81},
	{0x6E, 0x2C},
	{0x84, 0x20},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x32, 0x4F},
	{0x33, 0x58},
	{0x34, 0x5F},
	{0x35, 0x5F},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0x70},
	{0x3D, 0x8F},
	{0x3E, 0xFF},
	{0x3F, 0x85},
	{0x40, 0xFF},
	{0x56, 0x32},
	{0x59, 0x67},
	{0x85, 0x3C},
	{0x8A, 0x04},
	{0x91, 0x10},
	{0x9C, 0xE1},
	{0x5A, 0x09},
	{0x5C, 0x4C},
	{0x5D, 0xF4},
	{0x5E, 0x1E},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x66, 0x04},
	{0x67, 0x30},
	{0x6A, 0x12},
	{0x7A, 0xA0},
	{0x9D, 0x10},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x47, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x8F, 0x80},
	{0x90, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0x69, 0x74},
	{0x65, 0x02},
	{0x81, 0x74},
	{0x19, 0x20},
	{0x12, 0x00},

	{JXF35_REG_END, 0x00},	/* END MARKER */
};

struct tx_isp_sensor_attribute jxf35_attr;
unsigned int jxf35_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxf35_again_lut;
	while(lut->gain <= jxf35_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == jxf35_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int jxf35_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxf35_again_lut;
	while(lut->gain <= jxf35_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == jxf35_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int jxf35_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute jxf35_attr={
	.name = "jxf35",
	.chip_id = 0xf35,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x40,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 259142,
	.max_again_short = 0,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_short = 2,
	.max_integration_time_short = 512,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1350 - 4,
	.integration_time_limit = 1350 - 4,
	.total_width = 2560,
	.total_height = 1350,
	.max_integration_time = 1350 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.one_line_expr_in_us = 30,
	.sensor_ctrl.alloc_again = jxf35_alloc_again,
	.sensor_ctrl.alloc_again_short = jxf35_alloc_again_short,
	.sensor_ctrl.alloc_dgain = jxf35_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

struct tx_isp_mipi_bus jxf35_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 430,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.image_twidth = 1920,
	.image_theight = 1080,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus jxf35_mipi_vga={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 430,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.image_twidth = 640,
	.image_theight = 480,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_dvp_bus jxf35_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};
/*
 * the order of the jxf35_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxf35_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxf35_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxf35_init_regs_1920_1080_30fps_dvp,
	},
	{
		.width		= 640,
		.height		= 480,
		.fps		= 120 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxf35_init_regs_vga_120fps_mipi,
	},
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 60 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxf35_init_regs_1920_1080_60fps_mipi,
	},
	{
		.width		= 1728,
		.height		= 972,
		.fps		= 55 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxf35_init_regs_1728_972_55fps_mipi,
	},
	{
		.width		= 1728,
		.height		= 972,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxf35_init_regs_1728_972_15fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &jxf35_win_sizes[0];

static struct regval_list jxf35_stream_on_dvp[] = {
	{0x12, 0x00},
	{JXF35_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf35_stream_off_dvp[] = {
	{0x12, 0x40},
	{JXF35_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf35_stream_on_mipi[] = {

	{JXF35_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf35_stream_off_mipi[] = {
	{JXF35_REG_END, 0x00},	/* END MARKER */
};

int jxf35_read(struct tx_isp_subdev *sd, unsigned char reg,
		   unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= 0x40,
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

int jxf35_write(struct tx_isp_subdev *sd, unsigned char reg,
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

static int jxf35_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != JXF35_REG_END) {
		if (vals->reg_num == JXF35_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxf35_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("{0x%02x, 0x%02x},\n",vals->reg_num, val);
		vals++;
	}

	return 0;
}

static int jxf35_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != JXF35_REG_END) {
		if (vals->reg_num == JXF35_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxf35_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int jxf35_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = jxf35_read(sd, 0x0a, &v);
	ISP_WARNING("-----%s : %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != JXF35_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxf35_read(sd, 0x0b, &v);
	ISP_WARNING("-----%s : %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != JXF35_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int jxf35_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int jxf35_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf35_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf35_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf35_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = jxf35_write(sd,  0x01, (unsigned char)(value & 0xff));
	ret += jxf35_write(sd, 0x02, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		pr_debug("set integration time failure!!!\n");

	return ret;
}

static int jxf35_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	expo = expo / 2;
	ret = jxf35_write(sd,  0x05, (unsigned char)(expo & 0xfe));
	if (ret < 0)
		pr_debug("set integration time failure!!!\n");

	return ret;
}

static int jxf35_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += jxf35_write(sd, 0x00, (unsigned char)(value & 0x7f));

	if(value < 0x20) {
		ret += jxf35_write(sd, 0x2f, r2f_val | 0x20);
		ret += jxf35_write(sd, 0x0c, r0c_val | 0x40);
		ret += jxf35_write(sd, 0x80, r80_val | 0x01);
	} else {
		ret += jxf35_write(sd, 0x2f, r2f_val & 0xdf);
		ret += jxf35_write(sd, 0x0c, r0c_val & 0xbf);
		ret += jxf35_write(sd, 0x80, r80_val & 0xfe);
	}

	if (ret < 0)
		pr_debug("set analog gain!!!\n");

	return ret;
}

static int jxf35_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int jxf35_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = SENSOR_OUTPUT_MAX_FPS;

	if(data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
		switch (sensor_max_fps) {
		case PS5270_SENSOR_MAX_FPS_30:
			sclk = JXF35_SUPPORT_30FPS_SCLK;
			max_fps = SENSOR_OUTPUT_MAX_FPS;
			break;
		case PS5270_SENSOR_MAX_FPS_15:
			sclk = JXF35_SUPPORT_15FPS_SCLK;
			max_fps = PS5270_SENSOR_MAX_FPS_15;
			break;
		default:
			ret = -1;
			ISP_ERROR("Now we do not support this framerate!!!\n");
		}

	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
		switch (sensor_max_fps) {
		case PS5270_SENSOR_MAX_FPS_15:
			sclk = JXF35_SUPPORT_15FPS_SCLK;
			max_fps = PS5270_SENSOR_MAX_FPS_15;
			break;
		case PS5270_SENSOR_MAX_FPS_30:
			sclk = JXF35_SUPPORT_30FPS_MIPI_SCLK;
			max_fps = PS5270_SENSOR_MAX_FPS_30;
			break;
		case PS5270_SENSOR_MAX_FPS_55:
			sclk = JXF35_SUPPORT_55FPS_SCLK;
			max_fps = PS5270_SENSOR_MAX_FPS_55;
			break;
		case PS5270_SENSOR_MAX_FPS_60:
			sclk = JXF35_SUPPORT_60FPS_MIPI_SCLK;
			max_fps = PS5270_SENSOR_MAX_FPS_60;
			break;
		case PS5270_SENSOR_MAX_FPS_120:
			sclk = JXF35_SUPPORT_VGA_SCLK;
			max_fps = PS5270_SENSOR_MAX_FPS_120;
			break;
		default:
			ret = -1;
			ISP_ERROR("Now we do not support this framerate!!!\n");
		}

	} else {
		pr_debug("%s:%d:Can not support this mode!!!\n", __func__, __LINE__);
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret += jxf35_read(sd, 0x21, &val);
	hts = val<<8;
	val = 0;
	ret += jxf35_read(sd, 0x20, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: jxf35 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	jxf35_write(sd, 0xc0, 0x22);
	jxf35_write(sd, 0xc1, (unsigned char)(vts & 0xff));
	jxf35_write(sd, 0xc2, 0x23);
	jxf35_write(sd, 0xc3, (unsigned char)(vts >> 8));
	ret = jxf35_read(sd, 0x1f, &val);
//	pr_debug("before register 0x1f value : 0x%02x\n", val);
	if(ret < 0)
		return -1;
	val |= (1 << 7); //set bit[7],  register group writefunction,  auto clean
	jxf35_write(sd, 0x1f, val);
//	pr_debug("after register 0x1f value : 0x%02x\n", val);

	if (0 != ret) {
		ISP_ERROR("err: jxf35_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int jxf35_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	ret = jxf35_write_array(sd, wsize->regs);
	ret += jxf35_read(sd, 0x2f, &r2f_val);
	ret += jxf35_read(sd, 0x0c, &r0c_val);
	ret += jxf35_read(sd, 0x80, &r80_val);

	if (ret)
		return ret;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int jxf35_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf35_write_array(sd, jxf35_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf35_write_array(sd, jxf35_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("jxf35 stream on\n");
	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf35_write_array(sd, jxf35_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf35_write_array(sd, jxf35_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("jxf35 stream off\n");
	}

	return ret;
}

static int jxf35_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = jxf35_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return ret;
}

static int jxf35_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = jxf35_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int jxf35_g_register_list(struct tx_isp_subdev *sd, struct tx_isp_dbg_register_list *reg)
{
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	pr_debug("sensor init setting:\n");
	jxf35_read_array(sd, wsize->regs);

	return ret;
}

static int jxf35_g_register_all(struct tx_isp_subdev *sd, struct tx_isp_dbg_register_list *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;
	int i = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	pr_debug("sensor all setting:\n");
	for(i = 0; i <= 0xff; i++){
		ret = jxf35_read(sd, i, &val);
		pr_debug("{0x%x, 0x%x},\n", i, val);
	}

	return ret;
}

static int jxf35_g_chip_ident(struct tx_isp_subdev *sd,
				  struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"jxf35_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(15);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"jxf35_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}

	ret = jxf35_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxf35 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("jxf35 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "jxf35", sizeof("jxf35"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int jxf35_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = jxf35_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = jxf35_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = jxf35_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = jxf35_set_analog_gain_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = jxf35_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = jxf35_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = jxf35_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf35_write_array(sd, jxf35_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf35_write_array(sd, jxf35_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf35_write_array(sd, jxf35_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf35_write_array(sd, jxf35_stream_on_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = jxf35_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static struct tx_isp_subdev_core_ops jxf35_core_ops = {
	.g_chip_ident = jxf35_g_chip_ident,
	.reset = jxf35_reset,
	.init = jxf35_init,
	/*.ioctl = jxf35_ops_ioctl,*/
	.g_register_list = jxf35_g_register_list,
	.g_register_all = jxf35_g_register_all,
	.g_register = jxf35_g_register,
	.s_register = jxf35_s_register,
};

static struct tx_isp_subdev_video_ops jxf35_video_ops = {
	.s_stream = jxf35_s_stream,
};

static struct tx_isp_subdev_sensor_ops	jxf35_sensor_ops = {
	.ioctl	= jxf35_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops jxf35_ops = {
	.core = &jxf35_core_ops,
	.video = &jxf35_video_ops,
	.sensor = &jxf35_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "jxf35",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int jxf35_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	if(data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_max_fps == PS5270_SENSOR_MAX_FPS_30) && (sensor_resolution == SENSOR_RES_200)){
			wsize = &jxf35_win_sizes[0];
			jxf35_attr.total_width = 0x500 * 2;//2560
			jxf35_attr.total_height = 0x546; //1350
			jxf35_attr.max_integration_time_native = 0x465 - 4;
			jxf35_attr.integration_time_limit = 0x465 - 4;
			jxf35_attr.max_integration_time = 0x465 - 4;
			jxf35_mipi.clk = 430;
			memcpy((void*)(&(jxf35_attr.mipi)),(void*)(&jxf35_mipi),sizeof(jxf35_mipi));
		} else if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_max_fps == PS5270_SENSOR_MAX_FPS_60) && (sensor_resolution == SENSOR_RES_200)){
			wsize = &jxf35_win_sizes[3];
			jxf35_attr.total_width = 0x253 * 4;//2380
			jxf35_attr.total_height = 0x455; //1109
			jxf35_attr.max_integration_time_native = 0x455 - 4;
			jxf35_attr.integration_time_limit = 0x455 - 4;
			jxf35_attr.max_integration_time = 0x455 - 4;
			jxf35_mipi.clk = 792;
			memcpy((void*)(&(jxf35_attr.mipi)),(void*)(&jxf35_mipi),sizeof(jxf35_mipi));
		} else if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_max_fps == PS5270_SENSOR_MAX_FPS_15 && sensor_resolution == SENSOR_RES_180)){
			wsize = &jxf35_win_sizes[5];
			jxf35_attr.total_width = 0xA00;//2560
			jxf35_attr.total_height = 0x465; //1350
			jxf35_attr.max_integration_time_native = 0x465 - 4;
			jxf35_attr.integration_time_limit = 0x465 - 4;
			jxf35_attr.max_integration_time = 0x465 - 4;
			jxf35_mipi.clk = 430;
			jxf35_mipi.image_twidth = 1728;
			jxf35_mipi.image_theight = 972;
			memcpy((void*)(&(jxf35_attr.mipi)),(void*)(&jxf35_mipi),sizeof(jxf35_mipi));
		} else if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_max_fps == PS5270_SENSOR_MAX_FPS_55 && sensor_resolution == SENSOR_RES_180)){
			wsize = &jxf35_win_sizes[4];
			jxf35_attr.total_width = 0x280 * 4;//2560
			jxf35_attr.total_height = 0x465; //1125
			jxf35_attr.max_integration_time_native = 0x465 - 4;
			jxf35_attr.integration_time_limit = 0x465 - 4;
			jxf35_attr.max_integration_time = 0x465 - 4;
			jxf35_mipi.clk = 792;
			jxf35_mipi.image_twidth = 1728;
			jxf35_mipi.image_theight = 972;
			memcpy((void*)(&(jxf35_attr.mipi)),(void*)(&jxf35_mipi),sizeof(jxf35_mipi));
		}else if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_resolution == SENSOR_RES_30)){
			wsize = &jxf35_win_sizes[2];
			jxf35_attr.total_width = 0x2a2;//674
			jxf35_attr.total_height = 0x216;//534
			jxf35_attr.max_integration_time_native = 0x216 - 4;
			jxf35_attr.integration_time_limit = 0x216 - 4;
			jxf35_attr.max_integration_time = 0x216 - 4;
			memcpy((void*)(&(jxf35_attr.mipi)),(void*)(&jxf35_mipi_vga),sizeof(jxf35_mipi_vga));
		} else if((data_interface == TX_SENSOR_DATA_INTERFACE_DVP) && (sensor_max_fps == PS5270_SENSOR_MAX_FPS_30)){
			wsize = &jxf35_win_sizes[1];
			memcpy((void*)(&(jxf35_attr.dvp)),(void*)(&jxf35_dvp),sizeof(jxf35_dvp));
			ret = set_sensor_gpio_function(sensor_gpio_func);
			if (ret < 0)
				goto err_set_sensor_gpio;
			jxf35_attr.dvp.gpio = sensor_gpio_func;
		} else {
			ISP_ERROR("Can not support this data interface and fps!!!\n");
			goto err_set_sensor_data_interface;
		}
	} else {
		ISP_ERROR("Can not support this data type!!!\n");
	}
	/*
	  convert sensor-gain into isp-gain,
	*/
	jxf35_attr.dbus_type = data_interface;
	jxf35_attr.data_type = data_type;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &jxf35_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &jxf35_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->jxf35\n");

	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int jxf35_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id jxf35_id[] = {
	{ "jxf35", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jxf35_id);

static struct i2c_driver jxf35_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "jxf35",
	},
	.probe		= jxf35_probe,
	.remove		= jxf35_remove,
	.id_table	= jxf35_id,
};

static __init int init_jxf35(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init jxf35 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&jxf35_driver);
}

static __exit void exit_jxf35(void)
{
	private_i2c_del_driver(&jxf35_driver);
}

module_init(init_jxf35);
module_exit(exit_jxf35);

MODULE_DESCRIPTION("A low-level driver for Sonic jxf35 sensors");
MODULE_LICENSE("GPL");
