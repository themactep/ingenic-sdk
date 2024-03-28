// SPDX-License-Identifier: GPL-2.0+
/*
 * jxf23.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps     interface       mode
 *   0          1920x1080       25        dvp           linear
 *   1          1920x1080       15        dvp           linear
 *   2          1920x1080       25        mipi          linear
 *   3          1920x1080       15        mipi          linear
 *   4          1920x1080       15        mipi          wdr
 *   5           640x480        30        dvp           linear
 *   5           640x480        15        dvp           linear
 */

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

#define SENSOR_NAME "jxf23"
#define SENSOR_CHIP_ID_H (0x0f)
#define SENSOR_CHIP_ID_L (0x23)
#define SENSOR_REG_END 0xff
#define SENSOR_REG_DELAY 0xfe
#define SENSOR_OUTPUT_MIN_FPS 5
#define MCLK 24000000
#define SENSOR_VERSION "H20220527a"

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0,  0 },
	{0x1,  5731 },
	{0x2,  11136},
	{0x3,  16248},
	{0x4,  21097},
	{0x5,  25710},
	{0x6,  30109},
	{0x7,  34312},
	{0x8,  38336},
	{0x9,  42195},
	{0xa,  45904},
	{0xb,  49472},
	{0xc,  52910},
	{0xd,  56228},
	{0xe,  59433},
	{0xf,  62534},
	{0x10,  65536},
	{0x11, 71267},
	{0x12, 76672},
	{0x13, 81784},
	{0x14, 86633},
	{0x15, 91246},
	{0x16, 95645},
	{0x17, 99848},
	{0x18,  103872},
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
	{0x40, 262144},
	{0x41, 267875},
	{0x42, 273280},
	{0x43, 278392},
	{0x44, 283241},
	{0x45, 287854},
	{0x46, 292253},
	{0x47, 296456},
	{0x48, 300480},
	{0x49, 304339},
	{0x4a, 308048},
	{0x4b, 311616},
	{0x4c, 315054},
	{0x4d, 318372},
	{0x4e, 321577},
	{0x4f, 324678},
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
#if 0
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x40},
	{0x11, 0x80},
	{0x48, 0x05},
	{0x96, 0xAA},
	{0x94, 0xC0},
	{0x97, 0x8D},
	{0x96, 0x00},
	{0x12, 0x40},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x0D, 0xA0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x12},
	{0x57, 0x60},
	{0x9D, 0x00},
	{0x20, 0x00},
	{0x21, 0x05},
	{0x22, 0x46},
	{0x23, 0x05},
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0xC3},
	{0x28, 0x19},
	{0x29, 0x04},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x18},
	{0x2F, 0x44},
	{0x41, 0xC9},
	{0x42, 0x13},
	{0x46, 0x00},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x6E, 0x2C},
	{0x70, 0x6C},
	{0x71, 0x8a},/*ths-zero,tck-zero*/
	{0x72, 0x88},/*ths-prepare,tck-post*/
	{0x73, 0x36},
	{0x74, 0x02},
	{0x78, 0x9E},
	{0x89, 0x01},
	{0x2A, 0xB1},
	{0x2B, 0x24},
	{0x31, 0x08},
	{0x32, 0x4F},
	{0x33, 0x20},
	{0x34, 0x5E},
	{0x35, 0x5E},
	{0x3A, 0xAF},
	{0x56, 0x32},
	{0x59, 0xBF},
	{0x5A, 0x04},
	{0x85, 0x5A},
	{0x8A, 0x04},
	{0x8F, 0x90},
	{0x91, 0x13},
	{0x5B, 0xA0},
	{0x5C, 0xF0},
	{0x5D, 0xF4},
	{0x5E, 0x1F},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x64, 0xC0},
	{0x66, 0x44},
	{0x67, 0x73},
	{0x69, 0x7C},
	{0x6A, 0x28},
	{0x7A, 0xC0},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x49, 0x10},
	{0x50, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x90, 0x00},
	{0x8E, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8B, 0x01},
	{0x0C, 0x40},
	{0x65, 0x02},
	{0x80, 0x1A},
	{0x81, 0xC0},
	{0x19, 0x20},
	{0x12, 0x00},
	{0x48, 0x8A},
	{SENSOR_REG_DELAY, 250},
	{0x48, 0x0A},
	{0x99, 0x0F},
	{0x9B, 0x0F},
	{0x17, 0x00},
	{0x16, 0x4F},
#else

	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x40},
	{0x11, 0x80},
	{0x48, 0x05},
	{0x96, 0xAA},
	{0x94, 0xC0},
	{0x97, 0x8D},
	{0x96, 0x00},
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x0D, 0xA0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x12},
	{0x57, 0x60},
	{0x9D, 0x00},
	{0x20, 0x00},//1280
	{0x21, 0x05},
	{0x22, 0x46},//1125
	{0x23, 0x05},
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0xC3},
	{0x28, 0x19},
	{0x29, 0x04},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x18},
	{0x2F, 0x44},
	{0x41, 0xC9},
	{0x42, 0x13},
	{0x46, 0x00},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x6E, 0x2C},
	{0x70, 0x6C},
	{0x71, 0x8a},
	{0x72, 0x88},
	{0x73, 0x36},
	{0x74, 0x02},
	{0x78, 0x9E},
	{0x89, 0x01},
	{0x2A, 0xB1},
	{0x2B, 0x24},
	{0x31, 0x08},
	{0x32, 0x4F},
	{0x33, 0x20},
	{0x34, 0x5E},
	{0x35, 0x5E},
	{0x3A, 0xAF},
	{0x56, 0x32},
	{0x59, 0xBF},
	{0x5A, 0x04},
	{0x85, 0x5A},
	{0x8A, 0x04},
	{0x8F, 0x90},
	{0x91, 0x13},
	{0x5B, 0xA0},
	{0x5C, 0xF0},
	{0x5D, 0xFC},
	{0x5E, 0x1F},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x64, 0xC0},
	{0x66, 0x44},
	{0x67, 0x73},
	{0x69, 0x7C},
	{0x6A, 0x28},
	{0x7A, 0xC0},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x49, 0x10},
	{0x50, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x90, 0x00},
	{0x8E, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8B, 0x01},
	{0x0C, 0x40},
	{0x65, 0x02},
	{0x80, 0x1A},
	{0x81, 0xC0},
	{0x19, 0x20},
	{0x99, 0x0F},
	{0x9B, 0x0F},
	{0x12, 0x00},
	{0x48, 0x8A},
	{SENSOR_REG_DELAY, 250},
	{0x48, 0x0A},

#endif
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_15fps_mipi_fs[] = {
#if 1
	{0x12,0x40},
	{SENSOR_REG_DELAY, 250},
	{0x48,0x8A},
	{SENSOR_REG_DELAY, 250},
	{0x48,0x0A},

	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x40},
	{0x11, 0x80},
	{0x48, 0x05},
	{0x96, 0xAA},
	{0x94, 0xC0},
	{0x97, 0x8D},
	{0x96, 0x00},
	{0x12, 0x48},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x23},
	{0x11, 0x80},
	{0x0D, 0xA0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x12},
	{0x57, 0x60},
	{0x9D, 0x00},

	{0x20, 0xB8},
	{0x21, 0x09},
	{0x22, 0x94},
	{0x23, 0x11},

	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0x98},
	{0x28, 0x29},
	{0x29, 0x07},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x18},
	{0x2F, 0x44},
	{0x41, 0xC8},
	{0x42, 0x13},
	{0x46, 0x00},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x8A, 0x24},
	{0x37, 0x40},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x6E, 0x2C},

	{0x70, 0x6C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x36},
	{0x74, 0x02},
	{0x78, 0x9E},

	{0x89, 0x81},
	{0x2A, 0x86},
	{0x2B, 0x27},
	{0x31, 0x08},
	{0x32, 0x4F},
	{0x33, 0x20},
	{0x34, 0x5E},
	{0x35, 0x5E},
	{0x3A, 0xAF},
	{0x56, 0x32},
	{0x59, 0xBF},
	{0x5A, 0x04},
	{0x85, 0x5A},
	{0x8F, 0x90},
	{0x91, 0x13},
	{0x5B, 0xA0},
	{0x5C, 0xF0},
	{0x5D, 0xFC},
	{0x5E, 0x1F},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x64, 0xC0},
	{0x66, 0x44},
	{0x67, 0x73},
	{0x69, 0x7C},
	{0x6A, 0x28},
	{0x7A, 0xC0},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x49, 0x10},
	{0x50, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x90, 0x00},
	{0x8E, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8B, 0x01},
	{0x0C, 0x40},
	{0x65, 0x02},
	{0x80, 0x1A},
	{0x81, 0xC0},
	{0x19, 0x20},
	{0x99, 0x0F},
	{0x9B, 0x0F},
	{0x12, 0x08},
	{0x48, 0x8A},
	{SENSOR_REG_DELAY, 250},
	{SENSOR_REG_DELAY, 250},
	{0x48, 0x0A},
	{SENSOR_REG_DELAY, 250},
#endif

#if 0 // fpga ok
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x40},
	{0x11, 0x80},
	{0x48, 0x05},
	{0x96, 0xAA},
	{0x94, 0xC0},
	{0x97, 0x8D},
	{0x96, 0x00},
	{0x12, 0x48},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x0D, 0xA0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x12},
	{0x57, 0x60},
	{0x9D, 0x00},
	{0x20, 0x00},
	{0x21, 0x05},
	{0x22, 0x94},
	{0x23, 0x11},
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0xC3},
	{0x28, 0x29},
	{0x29, 0x04},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x18},
	{0x2F, 0x44},
	{0x41, 0xC8},
	{0x42, 0x13},
	{0x46, 0x00},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x8A, 0x24},
	{0x37, 0x40},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x6E, 0x2C},

	{0x70, 0x6C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x36},
	{0x74, 0x02},
	{0x78, 0x9E},

#if 0
	{0x70, 0x4C},
	{0x71, 0x6D},
	{0x72, 0xEA},
	{0x73, 0x36},
	{0x74, 0x02},
	{0x78, 0x9E},
#endif

	{0x89, 0x81},
	{0x2A, 0xB1},
	{0x2B, 0x24},
	{0x31, 0x08},
	{0x32, 0x4F},
	{0x33, 0x20},
	{0x34, 0x5E},
	{0x35, 0x5E},
	{0x3A, 0xAF},
	{0x56, 0x32},
	{0x59, 0xBF},
	{0x5A, 0x04},
	{0x85, 0x5A},
	{0x8F, 0x90},
	{0x91, 0x13},
	{0x5B, 0xA0},
	{0x5C, 0xF0},
	{0x5D, 0xFC},
	{0x5E, 0x1F},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x64, 0xC0},
	{0x66, 0x44},
	{0x67, 0x73},
	{0x69, 0x7C},
	{0x6A, 0x28},
	{0x7A, 0xC0},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x49, 0x10},
	{0x50, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x90, 0x00},
	{0x8E, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8B, 0x01},
	{0x0C, 0x40},
	{0x65, 0x02},
	{0x80, 0x1A},
	{0x81, 0xC0},
	{0x19, 0x20},
	{0x99, 0x0F},
	{0x9B, 0x0F},
	{0x12, 0x08},
	{0x48, 0x8A},
	{0x48, 0x0A},
#endif

#if 0
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x40},
	{0x11, 0x80},
	{0x48, 0x05},
	{0x96, 0xAA},
	{0x94, 0xC0},
	{0x97, 0x8D},
	{0x96, 0x00},
	{0x12, 0x48},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x14},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x0D, 0xA0},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x58, 0x12},
	{0x57, 0x60},
	{0x9D, 0x00},
	{0x20, 0x00},
	{0x21, 0x05},
	{0x22, 0xCA},
	{0x23, 0x08},
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0xC3},
	{0x28, 0x29},
	{0x29, 0x04},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x18},
	{0x2F, 0x44},
	{0x41, 0xC8},
	{0x42, 0x13},
	{0x46, 0x00},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x89, 0x80},
	{0x8A, 0x24},
	{0x37, 0x40},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x68, 0x00},
	{0x6E, 0x2C},
	{0x70, 0x6C},
	{0x71, 0x6D},
	{0x72, 0xEA},
	{0x73, 0x36},
	{0x74, 0x02},
	{0x78, 0x9E},
	{0x89, 0x01},
	{0x2A, 0xB1},
	{0x2B, 0x24},
	{0x31, 0x08},
	{0x32, 0x4F},
	{0x33, 0x20},
	{0x34, 0x5E},
	{0x35, 0x5E},
	{0x3A, 0xAF},
	{0x56, 0x32},
	{0x59, 0xBF},
	{0x5A, 0x04},
	{0x85, 0x5A},
	{0x8F, 0x90},
	{0x91, 0x13},
	{0x5B, 0xA0},
	{0x5C, 0xF0},
	{0x5D, 0xFC},
	{0x5E, 0x1F},
	{0x62, 0x04},
	{0x63, 0x0F},
	{0x64, 0xC0},
	{0x66, 0x44},
	{0x67, 0x73},
	{0x69, 0x7C},
	{0x6A, 0x28},
	{0x7A, 0xC0},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0x49, 0x10},
	{0x50, 0x02},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x7F, 0x57},
	{0x90, 0x00},
	{0x8E, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8B, 0x01},
	{0x0C, 0x40},
	{0x65, 0x02},
	{0x80, 0x1A},
	{0x81, 0xC0},
	{0x19, 0x20},
	{0x99, 0x0F},
	{0x9B, 0x0F},
	{0x12, 0x08},
	{0x48, 0x8A},
	{0x48, 0x0A},

#endif
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_15fps_mipi[] = {
	{0x12,0x40},
	{SENSOR_REG_DELAY, 250},
	{0x48,0x8A},
	{SENSOR_REG_DELAY, 250},
	{0x48,0x0A},
	{0x0E,0x12},
	{0x0F,0x1C},
	{0x10,0x36},
	{0x11,0x80},
	{0x0D,0xF0},
	{0x5F,0x41},
	{0x60,0x20},
	{0x58,0x12},
	{0x57,0x60},
	{0x9D,0x00},
	{0x20,0x00},
	{0x21,0x05},
	{0x22,0x65},
	{0x23,0x04},
	{0x24,0xC0},
	{0x25,0x38},
	{0x26,0x43},
	{0x27,0x58},
	{0x28,0x19},
	{0x29,0x04},
	{0x2C,0x00},
	{0x2D,0x00},
	{0x2E,0x18},
	{0x2F,0x44},
	{0x41,0xC9},
	{0x42,0x13},
	{0x46,0x00},
	{0x76,0x60},
	{0x77,0x09},
	{0x1D,0x00},
	{0x1E,0x04},
	{0x6C,0x40},
	{0x68,0x00},
	{0x6E,0x2C},
	/* {0x70,0x6C}, */
	/* {0x71,0x6D}, */
	/* {0x72,0x6A}, */

	/* {0x70,0x68}, */
	/* {0x71,0x6D}, */
	/* {0x72,0x2A}, */

	{0x70,0xE8},
	{0x71,0x6D},
	{0x72,0x2A},

	/* {0x71,0x8a}, */
	/* {0x72,0x88}, */
	/* {0x73,0x36}, */
	{0x73,0x36},
	{0x74,0x02},
	{0x78,0x9E},
	{0x89,0x01},
	{0x2A,0x46},
	{0x2B,0x24},
	{0x30,0x88},
	{0x31,0x05},
	{0x32,0x2A},
	{0x33,0x14},
	{0x34,0x32},
	{0x35,0x32},
	{0x3A,0xAF},
	{0x56,0x32},
	{0x59,0x66},
	{0x5A,0x04},
	{0x85,0x2C},
	{0x8A,0x04},
	{0x8F,0x90},
	{0x91,0x13},
	{0x5B,0xA0},
	{0x5C,0xF0},
	{0x5D,0xFC},
	{0x5E,0x1F},
	{0x62,0x04},
	{0x63,0x0F},
	{0x64,0xC0},
	{0x66,0x44},
	{0x67,0x73},
	{0x69,0x7C},
	{0x6A,0x28},
	{0x7A,0xC0},
	{0x4A,0x05},
	{0x7E,0xCD},
	{0x49,0x10},
	{0x50,0x02},
	{0x7B,0x4A},
	{0x7C,0x0C},
	{0x7F,0x57},
	{0x90,0x00},
	{0x8E,0x00},
	{0x8C,0xFF},
	{0x8D,0xC7},
	{0x8B,0x01},
	{0x0C,0x40},
	{0x65,0x02},
	{0x80,0x1A},
	{0x81,0xC0},
	{0x19,0x20},
	{0x99,0x0F},
	{0x9B,0x0F},
	{0x12,0x00},
	{0x48,0x8A},
	{SENSOR_REG_DELAY, 250},
	{SENSOR_REG_DELAY, 250},
	{0x48,0x0A},
	{SENSOR_REG_DELAY, 250},
	/* {0x99, 0x0F}, */
	/* {0x9B, 0x0F}, */
	/* {0x17, 0x00}, */
	/* {0x16, 0x4F}, */
	{0x00,0x00},
	{0x01,0x0a},
	{0x02,0x00},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_25fps_dvp[] = {
	{0x0E,0x11},
	{0x0F,0x14},
	{0x10,0x40},
	{0x11,0x80},
	{0x48,0x05},
	{0x96,0xAA},
	{0x94,0xC0},
	{0x97,0x8D},
	{0x96,0x00},
	{0x12,0x40},
	{0x48,0x85},
	{0x48,0x05},
	{0x0E,0x11},
	{0x0F,0x14},
	{0x10,0x48},
	{0x11,0x80},
	{0x0D,0x50},
	{0x5F,0x41},
	{0x60,0x20},
	{0x58,0x12},
	{0x57,0x60},
	{0x9D,0x00},
	{0x20,0x00},
	{0x21,0x05},
	{0x22,0x46},
	{0x23,0x05},
	{0x24,0xC0},
	{0x25,0x38},
	{0x26,0x43},
	{0x27,0xC3},
	{0x28,0x19},
	{0x29,0x04},
	{0x2C,0x00},
	{0x2D,0x00},
	{0x2E,0x18},
	{0x2F,0x44},
	{0x41,0xC9},
	{0x42,0x13},
	{0x46,0x00},
	{0x76,0x60},
	{0x77,0x09},
	{0x1D,0xFF},
	{0x1E,0x9F},
	{0x6C,0xC0},
	{0x2A,0xB1},
	{0x2B,0x24},
	{0x31,0x08},
	{0x32,0x4F},
	{0x33,0x20},
	{0x34,0x5E},
	{0x35,0x5E},
	{0x3A,0xAF},
	{0x56,0x32},
	{0x59,0xBF},
	{0x5A,0x04},
	{0x85,0x5A},
	{0x8A,0x04},
	{0x8F,0x90},
	{0x91,0x13},
	{0x5B,0xA0},
	{0x5C,0xF0},
	{0x5D,0xF4},
	{0x5E,0x1F},
	{0x62,0x04},
	{0x63,0x0F},
	{0x64,0xC0},
	{0x66,0x44},
	{0x67,0x73},
	{0x69,0x7C},
	{0x6A,0x28},
	{0x7A,0xC0},
	{0x4A,0x05},
	{0x7E,0xCD},
	{0x49,0x10},
	{0x50,0x02},
	{0x7B,0x4A},
	{0x7C,0x0C},
	{0x7F,0x57},
	{0x90,0x00},
	{0x8E,0x00},
	{0x8C,0xFF},
	{0x8D,0xC7},
	{0x8B,0x01},
	{0x0C,0x40},
	{0x65,0x02},
	{0x80,0x1A},
	{0x81,0xC0},
	{0x19,0x20},
	{0x12,0x00},
	{0x48,0x85},
	{SENSOR_REG_DELAY, 250},
	{SENSOR_REG_DELAY, 250},
	{0x48,0x05},
	{0x1F,0x01},
	//	{0x99,0x0F},
	//	{0x9b,0x0F},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_15fps_dvp[] = {
	{0x0E,0x11},
	{0x0F,0x14},
	{0x10,0x40},
	{0x11,0x80},
	{0x48,0x05},
	{0x96,0xAA},
	{0x94,0xC0},
	{0x97,0x8D},
	{0x96,0x00},
	{0x12,0x40},
	{0x0E,0x11},
	{0x0F,0x14},
	{0x10,0x24},
	{0x11,0x80},
	{0x0D,0xA0},
	{0x5F,0x41},
	{0x60,0x20},
	{0x58,0x12},
	{0x57,0x60},
	{0x9D,0x00},
	{0x20,0x00},
	{0x21,0x05},
	{0x22,0x65},
	{0x23,0x04},
	{0x24,0xC0},
	{0x25,0x38},
	{0x26,0x43},
	{0x27,0x58},
	{0x28,0x19},
	{0x29,0x04},
	{0x2C,0x00},
	{0x2D,0x00},
	{0x2E,0x18},
	{0x2F,0x44},
	{0x41,0xC9},
	{0x42,0x13},
	{0x46,0x00},
	{0x76,0x60},
	{0x77,0x09},
	{0x1D,0xFF},
	{0x1E,0x9F},
	{0x6C,0xC0},
	{0x2A,0x46},
	{0x2B,0x24},
	{0x30,0x88},
	{0x31,0x05},
	{0x32,0x2A},
	{0x33,0x14},
	{0x34,0x32},
	{0x35,0x32},
	{0x3A,0xAF},
	{0x56,0x32},
	{0x59,0x66},
	{0x5A,0x04},
	{0x85,0x2C},
	{0x8A,0x04},
	{0x8F,0x90},
	{0x91,0x13},
	{0x5B,0xA0},
	{0x5C,0xF0},
	{0x5D,0xF4},
	{0x5E,0x1F},
	{0x62,0x04},
	{0x63,0x0F},
	{0x64,0xC0},
	{0x66,0x44},
	{0x67,0x73},
	{0x69,0x7C},
	{0x6A,0x28},
	{0x7A,0xC0},
	{0x4A,0x05},
	{0x7E,0xCD},
	{0x49,0x10},
	{0x50,0x02},
	{0x7B,0x4A},
	{0x7C,0x0C},
	{0x7F,0x57},
	{0x90,0x00},
	{0x8E,0x00},
	{0x8C,0xFF},
	{0x8D,0xC7},
	{0x8B,0x01},
	{0x0C,0x40},
	{0x65,0x02},
	{0x80,0x1A},
	{0x81,0xC0},
	{0x19,0x20},
	{0x12,0x40},
	{0x48,0x85},
	{SENSOR_REG_DELAY, 250},
	{SENSOR_REG_DELAY, 250},
	{0x48,0x05},
	{0x99,0x0F},
	{0x9B,0x0F},

	{0x00,0x00},
	{0x01,0x0a},
	{0x02,0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_640_480_30fps_dvp[] = {
        {0x0E, 0x11},
        {0x0F, 0x14},
        {0x10, 0x40},
        {0x11, 0x80},
        {0x48, 0x05},
        {0x96, 0xAA},
        {0x94, 0xC0},
        {0x97, 0x8D},
        {0x96, 0x00},
        {0x12, 0x47},
        {0x48, 0x85},
        {0x48, 0x05},
        {0x0E, 0x11},
        {0x0F, 0x14},
        {0x10, 0x48},
        {0x11, 0x80},
        {0x0D, 0x50},
        {0x5F, 0x41},
        {0x60, 0x20},
        {0x58, 0x12},
        {0x57, 0x60},
        {0x9D, 0x00},
        {0x20, 0x60},
        {0x21, 0x09},
        {0x22, 0x58},
        {0x23, 0x02},
        {0x24, 0x40},
        {0x25, 0xE0},
        {0x26, 0x11},
        {0x27, 0xBE},
        {0x28, 0x15},
        {0x29, 0x06},
        {0x2C, 0x96},
        {0x2D, 0x0B},
        {0x2E, 0x09},
        {0x2F, 0x44},
        {0x41, 0x9C},
        {0x42, 0x12},
        {0x46, 0x02},
        {0x76, 0x20},
        {0x77, 0x03},
        {0x1D, 0xFF},
        {0x1E, 0x9F},
        {0x6C, 0xC0},
        {0x2A, 0xAA},
        {0x2B, 0x26},
        {0x31, 0x08},
        {0x32, 0x4F},
        {0x33, 0x20},
        {0x34, 0x5E},
        {0x35, 0x5E},
        {0x3A, 0xAF},
        {0x56, 0x32},
        {0x59, 0xBF},
        {0x5A, 0x04},
        {0x85, 0x5A},
        {0x8A, 0x04},
        {0x8F, 0x90},
        {0x91, 0x13},
        {0x5B, 0xA0},
        {0x5C, 0xF0},
        {0x5D, 0xFC},
        {0x5E, 0x1F},
        {0x62, 0x04},
        {0x63, 0x0F},
        {0x64, 0xC0},
        {0x66, 0x44},
        {0x67, 0x73},
        {0x69, 0x7C},
        {0x6A, 0x28},
        {0x7A, 0xC0},
        {0x4A, 0x05},
        {0x7E, 0xCD},
        {0x49, 0x10},
        {0x50, 0x02},
        {0x7B, 0x4A},
        {0x7C, 0x0C},
        {0x7F, 0x57},
        {0x90, 0x00},
        {0x8E, 0x00},
        {0x8C, 0xFF},
        {0x8D, 0xC7},
        {0x8B, 0x01},
        {0x0C, 0x40},
        {0x65, 0x02},
        {0x80, 0x1A},
        {0x81, 0xC0},
        {0x19, 0x20},
        {0x99, 0x0F},
        {0x9B, 0x0F},
        {0x12, 0x07},
        {0x48, 0x85},
        {0x48, 0x05},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_640_480_15fps_dvp[] = {
        {0x0E, 0x11},
        {0x0F, 0x14},
        {0x10, 0x40},
        {0x11, 0x80},
        {0x48, 0x05},
        {0x96, 0xAA},
        {0x94, 0xC0},
        {0x97, 0x8D},
        {0x96, 0x00},
        {0x12, 0x47},
        {0x48, 0x85},
        {0x48, 0x05},
        {0x0E, 0x11},
        {0x0F, 0x14},
        {0x10, 0x24},
        {0x11, 0x80},
        {0x0D, 0xA0},
        {0x5F, 0x41},
        {0x60, 0x20},
        {0x58, 0x12},
        {0x57, 0x60},
        {0x9D, 0x00},
        {0x20, 0x60},
        {0x21, 0x09},
        {0x22, 0x58},
        {0x23, 0x02},
        {0x24, 0x40},
        {0x25, 0xE0},
        {0x26, 0x11},
        {0x27, 0xDE},
        {0x28, 0x15},
        {0x29, 0x05},
        {0x2C, 0x96},
        {0x2D, 0x0B},
        {0x2E, 0x09},
        {0x2F, 0x44},
        {0x41, 0x9C},
        {0x42, 0x12},
        {0x46, 0x02},
        {0x76, 0x20},
        {0x77, 0x03},
        {0x1D, 0xFF},
        {0x1E, 0x9F},
        {0x6C, 0xC0},
        {0x2A, 0xCA},
        {0x2B, 0x25},
        {0x30, 0x88},
        {0x31, 0x05},
        {0x32, 0x2A},
        {0x33, 0x14},
        {0x34, 0x32},
        {0x35, 0x32},
        {0x3A, 0xAF},
        {0x56, 0x32},
        {0x59, 0x66},
        {0x5A, 0x04},
        {0x85, 0x2C},
        {0x8A, 0x04},
        {0x8F, 0x90},
        {0x91, 0x13},
        {0x5B, 0xA0},
        {0x5C, 0xF0},
        {0x5D, 0xFC},
        {0x5E, 0x1F},
        {0x62, 0x04},
        {0x63, 0x0F},
        {0x64, 0xC0},
        {0x66, 0x44},
        {0x67, 0x73},
        {0x69, 0x7C},
        {0x6A, 0x28},
        {0x7A, 0xC0},
        {0x4A, 0x05},
        {0x7E, 0xCD},
        {0x49, 0x10},
        {0x50, 0x02},
        {0x7B, 0x4A},
        {0x7C, 0x0C},
        {0x7F, 0x57},
        {0x90, 0x00},
        {0x8E, 0x00},
        {0x8C, 0xFF},
        {0x8D, 0xC7},
        {0x8B, 0x01},
        {0x0C, 0x40},
        {0x65, 0x02},
        {0x80, 0x1A},
        {0x81, 0xC0},
        {0x19, 0x20},
        {0x99, 0x0F},
        {0x9B, 0x0F},
        {0x12, 0x07},
        {0x48, 0x85},
        {0x48, 0x05},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_dvp,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_15fps_dvp,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_15fps_mipi,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_15fps_mipi_fs,
	},
	{
		.width = 640,
		.height = 480,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_640_480_30fps_dvp,
	},
	{
		.width = 640,
		.height = 480,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_640_480_15fps_dvp,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

struct tx_isp_sensor_attribute sensor_attr;
unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi ={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 800,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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

struct tx_isp_mipi_bus sensor_mipi ={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 300,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
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
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_fs={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 300,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 1,
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
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_dvp_bus sensor_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
        .polar = {
                .hsync_polar = 0,
                .vsync_polar = 0,
                .pclk_polar = 0,
        },
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0xf23,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x40,
	.max_again = 259142,
	.max_again_short = 0,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_short = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.one_line_expr_in_us = 30,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_stream_on_dvp[] = {
	{0x12, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_dvp[] = {
	{0x12, 0x40},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg,
	       unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

char* __attribute__((weak)) sclk_name[4];

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
	int sensor_gpio_func = DVP_PA_LOW_10BIT;
        struct clk *tclk;
        struct clk *sclka;
	unsigned long rate;
        uint8_t i;
	int ret;

	switch(info->default_boot) {
	case 0:
                wsize = &sensor_win_sizes[0];

                sensor_attr.total_width = 2560;
                sensor_attr.total_height = 1350;
                sensor_attr.max_integration_time = 1346;
                sensor_attr.integration_time_limit = 1346;
                sensor_attr.max_integration_time_native = 1346;
                sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		sensor_attr.again = 0;
                sensor_attr.integration_time = 0xFF;
                memcpy((void*)(&(sensor_attr.dvp)),(void*)(&sensor_dvp),sizeof(sensor_dvp));
                ret = set_sensor_gpio_function(sensor_gpio_func);
                if (ret < 0)
                        goto err_set_sensor_gpio;
                sensor_attr.dvp.gpio = sensor_gpio_func;
		break;
	case 1:
                wsize = &sensor_win_sizes[1];

                sensor_attr.total_width = 2560;
                sensor_attr.total_height = 1125;
                sensor_attr.max_integration_time = 1121;
                sensor_attr.integration_time_limit = 1121;
                sensor_attr.max_integration_time_native = 1121;
                sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		sensor_attr.again = 0;
                sensor_attr.integration_time = 0xa;
                memcpy((void*)(&(sensor_attr.dvp)),(void*)(&sensor_dvp),sizeof(sensor_dvp));
                ret = set_sensor_gpio_function(sensor_gpio_func);
                if (ret < 0)
                        goto err_set_sensor_gpio;
                sensor_attr.dvp.gpio = sensor_gpio_func;
                break;
        case 2:
                wsize = &sensor_win_sizes[2];

                sensor_attr.total_width = 2560;
                sensor_attr.total_height = 1350;
                sensor_attr.max_integration_time = 1346;
                sensor_attr.integration_time_limit = 1346;
                sensor_attr.max_integration_time_native = 1346;
                sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.again = 0;
                sensor_attr.integration_time = 0xFF;
                memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi1),sizeof(sensor_mipi1));
                break;
        case 3:
                wsize = &sensor_win_sizes[3];

                sensor_attr.total_width = 2560;
                sensor_attr.total_height = 1125;
                sensor_attr.max_integration_time = 1121;
                sensor_attr.integration_time_limit = 1121;
                sensor_attr.max_integration_time_native = 1121;
                sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.again = 0;
                sensor_attr.integration_time = 0xa;
                memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi2),sizeof(sensor_mipi2));
                break;
        case 4:
                wsize = &sensor_win_sizes[4];

                sensor_attr.total_width = 0x9b8;
                sensor_attr.total_height = 0x1194;
                sensor_attr.max_integration_time = 0x1194 - 4;
                sensor_attr.integration_time_limit = 0x1194 -4;
                sensor_attr.max_integration_time_native = 0x1194 - 4;
                sensor_attr.max_integration_time_short = 112;
                sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_FS;
                sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.again = 0;
                sensor_attr.integration_time = 0xFF;
                memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_fs),sizeof(sensor_mipi_fs));
                break;
        case 5:
                wsize = &sensor_win_sizes[5];

                sensor_attr.total_width = 0x960;
                sensor_attr.total_height = 0x258;
                sensor_attr.max_integration_time = 0x258 - 4;
                sensor_attr.integration_time_limit = 0x258 - 4;
                sensor_attr.max_integration_time_native = 0x258 - 4;
                sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		sensor_attr.again = 0;
                sensor_attr.integration_time = 0xFF;
                memcpy((void*)(&(sensor_attr.dvp)),(void*)(&sensor_dvp),sizeof(sensor_dvp));
                ret = set_sensor_gpio_function(sensor_gpio_func);
                if (ret < 0)
                        goto err_set_sensor_gpio;
                sensor_attr.dvp.gpio = sensor_gpio_func;
                break;
        case 6:
                wsize = &sensor_win_sizes[6];

                sensor_attr.total_width = 0x960;
                sensor_attr.total_height = 0x258;
                sensor_attr.max_integration_time = 0x258 - 4;
                sensor_attr.integration_time_limit = 0x258 - 4;
                sensor_attr.max_integration_time_native = 0x258 - 4;
                sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		sensor_attr.again = 0;
                sensor_attr.integration_time = 0xFF;
                memcpy((void*)(&(sensor_attr.dvp)),(void*)(&sensor_dvp),sizeof(sensor_dvp));
                ret = set_sensor_gpio_function(sensor_gpio_func);
                if (ret < 0)
                        goto err_set_sensor_gpio;
                sensor_attr.dvp.gpio = sensor_gpio_func;
                break;
        default:
                ISP_ERROR("Have no this setting!!!\n");
                break;
        }

        switch(info->video_interface) {
        case TISP_SENSOR_VI_MIPI_CSI0:
                sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                sensor_attr.mipi.index = 0;
                break;
        case TISP_SENSOR_VI_MIPI_CSI1:
                sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                sensor_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

        switch(info->mclk) {
        case TISP_SENSOR_MCLK0:
                sclka = private_devm_clk_get(&client->dev, "mux_cim0");
                sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
                set_sensor_mclk_function(0);
                break;
        case TISP_SENSOR_MCLK1:
                sclka = private_devm_clk_get(&client->dev, "mux_cim1");
                sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
                set_sensor_mclk_function(1);
                break;
        case TISP_SENSOR_MCLK2:
                sclka = private_devm_clk_get(&client->dev, "mux_cim2");
                sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
                set_sensor_mclk_function(2);
                break;
        default:
                ISP_ERROR("Have no this MCLK Source!!!\n");
        }

        rate = private_clk_get_rate(sensor->mclk);
        if (((rate / 1000) % (MCLK / 1000)) != 0) {
                uint8_t sclk_name_num = sizeof(sclk_name)/sizeof(sclk_name[0]);
                for (i=0; i < sclk_name_num; i++) {
                        tclk = private_devm_clk_get(&client->dev, sclk_name[i]);
                        ret = clk_set_parent(sclka, clk_get(NULL, sclk_name[i]));
                        if (IS_ERR(tclk)) {
                                pr_err("get sclka failed\n");
                        } else {
                                rate = private_clk_get_rate(tclk);
                                if (i == sclk_name_num - 1 && ((rate / 1000) % (MCLK / 1000)) != 0) {
                                        if (((MCLK / 1000) % 27000) != 0 || ((MCLK / 1000) % 37125) != 0)
                                                private_clk_set_rate(tclk, 891000000);
                                        else if (((MCLK / 1000) % 24000) != 0)
                                                private_clk_set_rate(tclk, 1200000000);
                                } else if (((rate / 1000) % (MCLK / 1000)) == 0) {
                                        break;
                                }
                        }
                }
        }
        private_clk_set_rate(sensor->mclk, MCLK);
        private_clk_prepare_enable(sensor->mclk);

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        if (ret) {
                ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
        }

	return 0;

err_set_sensor_gpio:
	return -1;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x0a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x0b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(35);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(35);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",info->pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = ISP_SUCCESS;

	ISP_INFO("[ %s:%d ] enable is %d\n", __func__, __LINE__, init->enable);

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sensor_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sensor_core_ops = {
	.g_chip_ident = sensor_g_chip_ident,
	.reset = sensor_reset,
	.init = sensor_init,
	/*.ioctl = sensor_ops_ioctl,*/
	.g_register = sensor_g_register,
	.s_register = sensor_s_register,
};

static unsigned char val_99 = 0x0F;
static unsigned char val_9b = 0x0F;
static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
                        sensor->video.state = TX_ISP_MODULE_INIT;
                } else {
                        ISP_WARNING("init failed, state error!\n");
                }
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
                        ret = sensor_write_array(sd, wsize->regs);
                        ret += sensor_read(sd, 0x99, &val_99);
                        ret += sensor_read(sd, 0x9b, &val_9b);
			if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_DVP) {
				/* ret += sensor_write_array(sd, sensor_stream_on_dvp); */
			} else if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI) {
				/* ret += sensor_write_array(sd, sensor_stream_on_mipi); */
			} else {
				ISP_ERROR("[%s %d] Don't support this Sensor Data interface\n", __func__, __LINE__);
			}
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			ISP_WARNING("[%s %d] %s stream on\n", __func__, __LINE__, SENSOR_NAME);
                } else {
                        ISP_WARNING("[%s %d] Failed the open stream, state error!\n", __func__, __LINE__);
		}
	}
        else {
                if (sensor->video.state == TX_ISP_MODULE_RUNNING) {
                        if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_DVP) {
                                ret = sensor_write_array(sd, sensor_stream_off_dvp);
                        } else if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI) {
                                ret = sensor_write_array(sd, sensor_stream_off_mipi);
                        } else {
                                ISP_ERROR("[%s %d] Don't support this Sensor Data interface\n", __func__, __LINE__);
                        }
                        sensor->video.state = TX_ISP_MODULE_INIT;
                        ISP_WARNING("[%s %d] %s stream off\n", __func__, __LINE__, SENSOR_NAME);
                } else {
                        ISP_WARNING("[%s %d] Failed the close stream, state error!\n", __func__, __LINE__);
                }
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			sensor->video.state = TX_ISP_MODULE_DEINIT;
                } else {
                        ISP_WARNING("deinit failed, state error!\n");
                }
        }

	return ret;
}

static struct tx_isp_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
};

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;
	/* printk("s0 it is %d\n", value); */

	ret = sensor_write(sd,  0x01, (unsigned char)(expo & 0xff));
	ret += sensor_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;
	expo = expo / 2;
	ret = sensor_write(sd,  0x05, (unsigned char)(expo & 0xfe));
//	ISP_INFO("#############It short is 0x%x\n",value);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	unsigned char tmp1;
	unsigned char tmp2;
	unsigned char tmp99;
	unsigned char tmp9b;
	int ret = 0;
	/* printk("s0 ag is %d\n", value); */

	if (value < 0x10) {
		tmp1 = 0x00;
		tmp2 = 0x04;
	} else if (value >= 0x10 && value < 0x30) {
		tmp1 = 0x00;
		tmp2 = 0x24;
	} else {
		tmp1 = 0x40;
		tmp2 = 0x24;
	}

	ret += sensor_write(sd, 0x00, (unsigned char)(value & 0x7f));

	//complement the steak
	if (value <= 0x40) {
		tmp99 = (unsigned char)(val_99 & 0x0f);
		tmp9b = (unsigned char)(val_9b & 0x0f);
	} else {
		tmp99 = (unsigned char)(val_99);
		tmp9b = (unsigned char)(val_9b);
	}


	sensor_write(sd, 0x0c, tmp1);
	sensor_write(sd, 0x66, tmp2);
	sensor_write(sd, 0x99, tmp99);
	sensor_write(sd, 0x9b, tmp9b);

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{

	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;

        switch (info->default_boot) {
        case 0:
		sclk = 0x500 * 0x546 * 25;
		max_fps = TX_SENSOR_MAX_FPS_25;
                break;
        case 1:
		sclk = 0x500 * 0x546 * 15;
		max_fps = TX_SENSOR_MAX_FPS_15;
                break;
        case 2:
		sclk = 0x500 * 0x546 * 25;
		max_fps = TX_SENSOR_MAX_FPS_25;
                break;
        case 3:
		sclk = 0x500 * 0x546 * 15;
		max_fps = TX_SENSOR_MAX_FPS_15;
                break;
        case 4:
		sclk = 0x9b8 * 0x1194 * 15;
		max_fps = TX_SENSOR_MAX_FPS_15;
                break;
        case 5:
		sclk = 0x960 * 0x258 * 30;
		max_fps = TX_SENSOR_MAX_FPS_30;
                break;
        case 6:
		sclk = 0x960 * 0x258 * 30;
		max_fps = TX_SENSOR_MAX_FPS_15;
                break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
        }

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	val = 0;
	ret += sensor_read(sd, 0x21, &val);
	hts = val<<8;
	val = 0;
	ret += sensor_read(sd, 0x20, &val);
	hts |= val;
	hts *= 2;
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sensor_write(sd, 0xc0, 0x22);
	sensor_write(sd, 0xc1, (unsigned char)(vts & 0xff));
	sensor_write(sd, 0xc2, 0x23);
	sensor_write(sd, 0xc3, (unsigned char)(vts >> 8));
	ret = sensor_read(sd, 0x1f, &val);
//	pr_debug("before register 0x1f value : 0x%02x\n", val);
	if (ret < 0)
		return -1;
	val |= (1 << 7); //set bit[7],  register group write function,  auto clean
	sensor_write(sd, 0x1f, val);
//	pr_debug("after register 0x1f value : 0x%02x\n", val);

	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        if (ret) {
                ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
        }

	return ret;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if (arg)
			ret = sensor_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if (arg)
			ret = sensor_set_analog_gain_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (info->video_interface == TISP_SENSOR_VI_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (info->video_interface == TISP_SENSOR_VI_MIPI_CSI0
                           || info->video_interface == TISP_SENSOR_VI_MIPI_CSI1) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (info->video_interface == TISP_SENSOR_VI_DVP) {
			ret = sensor_write_array(sd, sensor_stream_on_dvp);
		} else if (info->video_interface == TISP_SENSOR_VI_MIPI_CSI0
                           || info->video_interface == TISP_SENSOR_VI_MIPI_CSI1) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static struct tx_isp_subdev_sensor_ops sensor_sensor_ops = {
	.ioctl = sensor_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.sensor = &sensor_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = SENSOR_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	sensor->dev = &client->dev;
	sensor->video.attr = &sensor_attr;
        sensor->video.state = TX_ISP_MODULE_DEINIT;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	printk("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;

	if (info->rst_gpio != -1)
		private_gpio_free(info->rst_gpio);
	if (info->pwdn_gpio != -1)
		private_gpio_free(info->pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
    	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void)
{
	/* ret = private_driver_get_interface(); */
	/* if (ret) { */
	/* 	ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME); */
	/* 	return -1; */
	/* } */
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
