/*
 * gc02m1.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps        dvdd
 *  0           1600*1200       30        mipi_1lane    linear  10        30           null
 *  0           1280*720        30        mipi_1lane    linear  10        30           null
 *
 * @I2C addr:0x37,0x3f
 *
 * @FSync:none
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

#define TVERSION "VH20241105a"
#define SENSOR_VERSION  "H20250417a"

// #define SENSOR_TEST

#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x02)
#define SENSOR_CHIP_ID_L    (0xe0)
#define SENSOR_OUTPUT_MIN_FPS 1
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
#define SENSOR_REG_DELAY  0x00
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

struct tx_isp_sensor_attribute gc02m1_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int index;
	unsigned int reg_00b6;
	unsigned int reg_00b1;
	unsigned int reg_00b2;
	unsigned int gain;
};

struct again_lut gc02m1_again_lut[] = {
	//index  0xb6 0xb1 0xb2  gain
	{0x00, 0x00, 0x04, 0x00, 0},             //1.000000
	{0x01, 0x00, 0x04, 0x10, 1465},             //1.015625
	{0x02, 0x00, 0x04, 0x20, 2909},             //1.031250
	{0x03, 0x00, 0x04, 0x30, 4330},             //1.046875
	{0x04, 0x00, 0x04, 0x40, 5731},             //1.062500
	{0x05, 0x00, 0x04, 0x50, 7111},             //1.078125
	{0x06, 0x00, 0x04, 0x60, 8471},             //1.093750
	{0x07, 0x00, 0x04, 0x70, 9813},             //1.109375
	{0x08, 0x00, 0x04, 0x80, 11135},             //1.125000
	{0x09, 0x00, 0x04, 0x90, 12439},             //1.140625
	{0x0a, 0x00, 0x04, 0xa0, 13726},             //1.156250
	{0x0b, 0x00, 0x04, 0xb0, 14995},             //1.171875
	{0x0c, 0x00, 0x04, 0xc0, 16247},             //1.187500
	{0x0d, 0x00, 0x04, 0xd0, 17483},             //1.203125
	{0x0e, 0x00, 0x04, 0xe0, 18703},             //1.218750
	{0x0f, 0x00, 0x04, 0xf0, 19908},             //1.234375
	{0x10, 0x00, 0x05, 0x00, 21097},             //1.250000
	{0x11, 0x00, 0x05, 0x10, 22271},             //1.265625
	{0x12, 0x00, 0x05, 0x20, 23431},             //1.281250
	{0x13, 0x00, 0x05, 0x30, 24578},             //1.296875
	{0x14, 0x00, 0x05, 0x40, 25710},             //1.312500
	{0x15, 0x00, 0x05, 0x50, 26829},             //1.328125
	{0x16, 0x00, 0x05, 0x60, 27935},             //1.343750
	{0x17, 0x00, 0x05, 0x70, 29028},             //1.359375
	{0x18, 0x00, 0x05, 0x80, 30108},             //1.375000
	{0x19, 0x00, 0x05, 0x90, 31177},             //1.390625
	{0x1a, 0x00, 0x05, 0xa0, 32233},             //1.406250
	{0x1b, 0x00, 0x05, 0xb0, 33277},             //1.421875
	{0x1c, 0x00, 0x05, 0xc0, 34311},             //1.437500
	{0x1d, 0x00, 0x05, 0xd0, 35333},             //1.453125
	{0x1e, 0x00, 0x05, 0xe0, 36344},             //1.468750
	{0x1f, 0x00, 0x05, 0xf0, 37345},             //1.484375
	{0x20, 0x01, 0x04, 0x00, 38335},             //1.500000
	{0x21, 0x01, 0x04, 0x10, 39315},             //1.515625
	{0x22, 0x01, 0x04, 0x20, 41245},             //1.546875
	{0x23, 0x01, 0x04, 0x30, 42195},             //1.562500
	{0x24, 0x01, 0x04, 0x40, 44067},             //1.593750
	{0x25, 0x01, 0x04, 0x50, 44990},             //1.609375
	{0x26, 0x01, 0x04, 0x60, 46808},             //1.640625
	{0x27, 0x01, 0x04, 0x70, 47704},             //1.656250
	{0x28, 0x01, 0x04, 0x80, 49472},             //1.687500
	{0x29, 0x01, 0x04, 0x90, 50343},             //1.703125
	{0x2a, 0x01, 0x04, 0xa0, 52062},             //1.734375
	{0x2b, 0x01, 0x04, 0xb0, 52910},             //1.750000
	{0x2c, 0x01, 0x04, 0xc0, 54583},             //1.781250
	{0x2d, 0x01, 0x04, 0xd0, 55409},             //1.796875
	{0x2e, 0x01, 0x04, 0xe0, 57039},             //1.828125
	{0x2f, 0x01, 0x04, 0xf0, 57844},             //1.843750
	{0x30, 0x01, 0x05, 0x00, 59433},             //1.875000
	{0x31, 0x01, 0x05, 0x10, 60217},             //1.890625
	{0x32, 0x01, 0x05, 0x20, 61768},             //1.921875
	{0x33, 0x01, 0x05, 0x30, 62534},             //1.937500
	{0x34, 0x01, 0x05, 0x40, 64046},             //1.968750
	{0x35, 0x02, 0x04, 0x00, 64793},             //1.984375
	{0x36, 0x02, 0x04, 0x10, 65536},             //2.000000
	{0x37, 0x02, 0x04, 0x20, 67001},             //2.031250
	{0x38, 0x02, 0x04, 0x30, 68445},             //2.062500
	{0x39, 0x02, 0x04, 0x40, 69866},             //2.093750
	{0x3a, 0x02, 0x04, 0x50, 71267},             //2.125000
	{0x3b, 0x02, 0x04, 0x60, 72647},             //2.156250
	{0x3c, 0x02, 0x04, 0x70, 74007},             //2.187500
	{0x3d, 0x02, 0x04, 0x80, 75349},             //2.218750
	{0x3e, 0x02, 0x04, 0x90, 76671},             //2.250000
	{0x3f, 0x02, 0x04, 0xa0, 77975},             //2.281250
	{0x40, 0x02, 0x04, 0xb0, 79262},             //2.312500
	{0x41, 0x02, 0x04, 0xc0, 80531},             //2.343750
	{0x42, 0x02, 0x04, 0xd0, 81783},             //2.375000
	{0x43, 0x02, 0x04, 0xe0, 83019},             //2.406250
	{0x44, 0x02, 0x04, 0xf0, 84239},             //2.437500
	{0x45, 0x03, 0x04, 0x00, 84843},             //2.453125
	{0x46, 0x03, 0x04, 0x10, 86040},             //2.484375
	{0x47, 0x03, 0x04, 0x20, 87222},             //2.515625
	{0x48, 0x03, 0x04, 0x30, 88967},             //2.562500
	{0x49, 0x03, 0x04, 0x40, 90114},             //2.593750
	{0x4a, 0x03, 0x04, 0x50, 91807},             //2.640625
	{0x4b, 0x03, 0x04, 0x60, 92920},             //2.671875
	{0x4c, 0x03, 0x04, 0x70, 94564},             //2.718750
	{0x4d, 0x03, 0x04, 0x80, 95644},             //2.750000
	{0x4e, 0x03, 0x04, 0x90, 97243},             //2.796875
	{0x4f, 0x03, 0x04, 0xa0, 98293},             //2.828125
	{0x50, 0x03, 0x04, 0xb0, 99331},             //2.859375
	{0x51, 0x03, 0x04, 0xc0, 100869},             //2.906250
	{0x52, 0x03, 0x04, 0xd0, 101880},             //2.937500
	{0x53, 0x03, 0x04, 0xe0, 103377},             //2.984375
	{0x54, 0x03, 0x04, 0xf0, 104362},             //3.015625
	{0x55, 0x03, 0x05, 0x00, 105821},             //3.062500
	{0x56, 0x04, 0x04, 0x00, 106302},             //3.078125
	{0x57, 0x04, 0x04, 0x10, 107731},             //3.125000
	{0x58, 0x04, 0x04, 0x20, 109138},             //3.171875
	{0x59, 0x04, 0x04, 0x30, 110526},             //3.218750
	{0x5a, 0x04, 0x04, 0x40, 111892},             //3.265625
	{0x5b, 0x04, 0x04, 0x50, 113240},             //3.312500
	{0x5c, 0x04, 0x04, 0x60, 114569},             //3.359375
	{0x5d, 0x04, 0x04, 0x70, 115879},             //3.406250
	{0x5e, 0x04, 0x04, 0x80, 117171},             //3.453125
	{0x5f, 0x04, 0x04, 0x90, 118446},             //3.500000
	{0x60, 0x05, 0x04, 0x00, 119286},             //3.531250
	{0x61, 0x05, 0x04, 0x10, 120533},             //3.578125
	{0x62, 0x05, 0x04, 0x20, 122171},             //3.640625
	{0x63, 0x05, 0x04, 0x30, 123380},             //3.687500
	{0x64, 0x05, 0x04, 0x40, 124969},             //3.750000
	{0x65, 0x05, 0x04, 0x50, 126144},             //3.796875
	{0x66, 0x05, 0x04, 0x60, 127687},             //3.859375
	{0x67, 0x05, 0x04, 0x70, 128829},             //3.906250
	{0x68, 0x05, 0x04, 0x80, 130329},             //3.968750
	{0x69, 0x05, 0x04, 0x90, 131439},             //4.015625
	{0x6a, 0x06, 0x04, 0x00, 132172},             //4.046875
	{0x6b, 0x06, 0x04, 0x10, 133621},             //4.109375
	{0x6c, 0x06, 0x04, 0x20, 135048},             //4.171875
	{0x6d, 0x06, 0x04, 0x30, 136454},             //4.234375
	{0x6e, 0x06, 0x04, 0x40, 137839},             //4.296875
	{0x6f, 0x06, 0x04, 0x50, 139205},             //4.359375
	{0x70, 0x06, 0x04, 0x60, 140551},             //4.421875
	{0x71, 0x07, 0x04, 0x00, 141878},             //4.484375
	{0x72, 0x07, 0x04, 0x10, 143187},             //4.546875
	{0x73, 0x07, 0x04, 0x20, 144477},             //4.609375
	{0x74, 0x07, 0x04, 0x30, 146067},             //4.687500
	{0x75, 0x07, 0x04, 0x40, 147319},             //4.750000
	{0x76, 0x07, 0x04, 0x50, 148861},             //4.828125
	{0x77, 0x07, 0x04, 0x60, 150077},             //4.890625
	{0x78, 0x08, 0x04, 0x00, 151576},             //4.968750
	{0x79, 0x08, 0x04, 0x10, 152758},             //5.031250
	{0x7a, 0x08, 0x04, 0x20, 154214},             //5.109375
	{0x7b, 0x08, 0x04, 0x30, 155650},             //5.187500
	{0x7c, 0x08, 0x04, 0x40, 157062},             //5.265625
	{0x7d, 0x08, 0x04, 0x50, 158456},             //5.343750
	{0x7e, 0x08, 0x04, 0x60, 159827},             //5.421875
	{0x7f, 0x08, 0x04, 0x70, 161180},             //5.500000
	{0x80, 0x09, 0x04, 0x00, 162249},             //5.562500
	{0x81, 0x09, 0x04, 0x10, 163567},             //5.640625
	{0x82, 0x09, 0x04, 0x20, 165125},             //5.734375
	{0x83, 0x09, 0x04, 0x30, 166405},             //5.812500
	{0x84, 0x09, 0x04, 0x40, 167918},             //5.906250
	{0x85, 0x09, 0x04, 0x50, 169160},             //5.984375
	{0x86, 0x09, 0x04, 0x60, 170630},             //6.078125
	{0x87, 0x0a, 0x04, 0x00, 171115},             //6.109375
	{0x88, 0x0a, 0x04, 0x10, 172555},             //6.203125
	{0x89, 0x0a, 0x04, 0x20, 173973},             //6.296875
	{0x8a, 0x0a, 0x04, 0x30, 175370},             //6.390625
	{0x8b, 0x0a, 0x04, 0x40, 176747},             //6.484375
	{0x8c, 0x0b, 0x04, 0x00, 177654},             //6.546875
	{0x8d, 0x0b, 0x04, 0x10, 178999},             //6.640625
	{0x8e, 0x0b, 0x04, 0x20, 180544},             //6.750000
	{0x8f, 0x0b, 0x04, 0x30, 181848},             //6.843750
	{0x90, 0x0b, 0x04, 0x40, 183346},             //6.953125
	{0x91, 0x0c, 0x04, 0x00, 184403},             //7.031250
	{0x92, 0x0c, 0x04, 0x10, 185862},             //7.140625
	{0x93, 0x0c, 0x04, 0x20, 187300},             //7.250000
	{0x94, 0x0c, 0x04, 0x30, 188715},             //7.359375
	{0x95, 0x0c, 0x04, 0x40, 190110},             //7.468750
	{0x96, 0x0d, 0x04, 0x00, 190505},             //7.500000
	{0x97, 0x0d, 0x04, 0x10, 191873},             //7.609375
	{0x98, 0x0d, 0x04, 0x20, 193414},             //7.734375
	{0x99, 0x0d, 0x04, 0x30, 194742},             //7.843750
	{0x9a, 0x0d, 0x04, 0x40, 196237},             //7.968750
	{0x9b, 0x0e, 0x04, 0x00, 196791},             //8.015625
	{0x9c, 0x0e, 0x04, 0x10, 198254},             //8.140625
	{0x9d, 0x0e, 0x04, 0x20, 199695},             //8.265625
	{0x9e, 0x0e, 0x04, 0x30, 201114},             //8.390625
	{0x9f, 0x0e, 0x04, 0x40, 202513},             //8.515625
	{0xa0, 0x0e, 0x04, 0x50, 203890},             //8.640625
	{0xa1, 0x0e, 0x04, 0x60, 205248},             //8.765625
	{0xa2, 0x0e, 0x04, 0x70, 206587},             //8.890625
	{0xa3, 0x0e, 0x04, 0x80, 207907},             //9.015625
	{0xa4, 0x0e, 0x04, 0x90, 209209},             //9.140625
	{0xa5, 0x0e, 0x04, 0xa0, 210493},             //9.265625
	{0xa6, 0x0e, 0x04, 0xb0, 211760},             //9.390625
	{0xa7, 0x0e, 0x04, 0xc0, 213010},             //9.515625
	{0xa8, 0x0e, 0x04, 0xd0, 214244},             //9.640625
	{0xa9, 0x0e, 0x04, 0xe0, 215462},             //9.765625
	{0xaa, 0x0e, 0x04, 0xf0, 216665},             //9.890625
	{0xab, 0x0e, 0x05, 0x00, 217852},             //10.015625
	{0xac, 0x0f, 0x04, 0x00, 218587},             //10.093750
	{0xad, 0x0f, 0x04, 0x10, 220039},             //10.250000
	{0xae, 0x0f, 0x04, 0x20, 221469},             //10.406250
	{0xaf, 0x0f, 0x04, 0x30, 222879},             //10.562500
	{0xb0, 0x0f, 0x04, 0x40, 224267},             //10.718750
	{0xb1, 0x0f, 0x04, 0x50, 225636},             //10.875000
	{0xb2, 0x0f, 0x04, 0x60, 226984},             //11.031250
	{0xb3, 0x0f, 0x04, 0x70, 228315},             //11.187500
	{0xb4, 0x0f, 0x04, 0x80, 229625},             //11.343750
	{0xb5, 0x0f, 0x04, 0x90, 230919},             //11.500000
	{0xb6, 0x0f, 0x04, 0xa0, 232194},             //11.656250
	{0xb7, 0x0f, 0x04, 0xb0, 233578},             //11.828125
	{0xb8, 0x0f, 0x04, 0xc0, 234820},             //11.984375
	{0xb9, 0x0f, 0x04, 0xd0, 236045},             //12.140625
	{0xba, 0x0f, 0x04, 0xe0, 237253},             //12.296875
	{0xbb, 0x0f, 0x04, 0xf0, 238447},             //12.453125
	{0xbc, 0x0f, 0x05, 0x00, 239626},             //12.609375
	{0xbd, 0x0f, 0x05, 0x10, 240791},             //12.765625
	{0xbe, 0x0f, 0x05, 0x20, 241940},             //12.921875
	{0xbf, 0x0f, 0x05, 0x30, 243077},             //13.078125
	{0xc0, 0x0f, 0x05, 0x40, 244200},             //13.234375
	{0xc1, 0x0f, 0x05, 0x50, 245310},             //13.390625
	{0xc2, 0x0f, 0x05, 0x60, 246516},             //13.562500
	{0xc3, 0x0f, 0x05, 0x70, 247599},             //13.718750
	{0xc4, 0x0f, 0x05, 0x80, 248670},             //13.875000
	{0xc5, 0x0f, 0x05, 0x90, 249728},             //14.031250
	{0xc6, 0x0f, 0x05, 0xa0, 250776},             //14.187500
	{0xc7, 0x0f, 0x05, 0xb0, 251811},             //14.343750
	{0xc8, 0x0f, 0x05, 0xc0, 252836},             //14.500000
	{0xc9, 0x0f, 0x05, 0xd0, 253849},             //14.656250
	{0xca, 0x0f, 0x05, 0xe0, 254851},             //14.812500
	{0xcb, 0x0f, 0x05, 0xf0, 255844},             //14.968750
	{0xcc, 0x0f, 0x06, 0x00, 256923},             //15.140625
	{0xcd, 0x0f, 0x06, 0x10, 257894},             //15.296875
	{0xce, 0x0f, 0x06, 0x20, 258854},             //15.453125
	{0xcf, 0x0f, 0x06, 0x30, 259806},             //15.609375
	{0xd0, 0x0f, 0x06, 0x40, 260748},             //15.765625
	{0xd1, 0x0f, 0x06, 0x50, 261680},             //15.921875
	{0xd2, 0x0f, 0x06, 0x60, 262602},             //16.078125
	{0xd3, 0x0f, 0x06, 0x70, 263518},             //16.234375
	{0xd4, 0x0f, 0x06, 0x80, 264422},             //16.390625
	{0xd5, 0x0f, 0x06, 0x90, 265320},             //16.546875
	{0xd6, 0x0f, 0x06, 0xa0, 266209},             //16.703125
	{0xd7, 0x0f, 0x06, 0xb0, 267177},             //16.875000
	{0xd8, 0x0f, 0x06, 0xc0, 268049},             //17.031250
	{0xd9, 0x0f, 0x06, 0xd0, 268911},             //17.187500
	{0xda, 0x0f, 0x06, 0xe0, 269767},             //17.343750
	{0xdb, 0x0f, 0x06, 0xf0, 270615},             //17.500000
	{0xdc, 0x0f, 0x07, 0x00, 271456},             //17.656250
	{0xdd, 0x0f, 0x07, 0x10, 272288},             //17.812500
	{0xde, 0x0f, 0x07, 0x20, 273115},             //17.968750
	{0xdf, 0x0f, 0x07, 0x30, 273934},             //18.125000
	{0xe0, 0x0f, 0x07, 0x40, 274745},             //18.281250
	{0xe1, 0x0f, 0x07, 0x50, 275549},             //18.437500
	{0xe2, 0x0f, 0x07, 0x60, 276427},             //18.609375
	{0xe3, 0x0f, 0x07, 0x70, 277218},             //18.765625
	{0xe4, 0x0f, 0x07, 0x80, 278001},             //18.921875
	{0xe5, 0x0f, 0x07, 0x90, 278778},             //19.078125
	{0xe6, 0x0f, 0x07, 0xa0, 279550},             //19.234375
	{0xe7, 0x0f, 0x07, 0xb0, 280315},             //19.390625
	{0xe8, 0x0f, 0x07, 0xc0, 281073},             //19.546875
	{0xe9, 0x0f, 0x07, 0xd0, 281826},             //19.703125
	{0xea, 0x0f, 0x07, 0xe0, 282573},             //19.859375
	{0xeb, 0x0f, 0x07, 0xf0, 283315},             //20.015625
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc02m1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc02m1_again_lut;
	while (lut->gain <= gc02m1_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc02m1_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}
#else
	/* Non analog gain table */
#endif  /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int gc02m1_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc02m1_again_lut;
	while(lut->gain <= gc02m1_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc02m1_attr.again_short.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}
		lut++;
	}
#else
	/* Non analog gain table */
#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int gc02m1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int gc02m1_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc02m1_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc02m1_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 672,
	.lans = 1,
	.image_twidth = 1600,
	.image_theight = 1200,
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

struct tx_isp_dvp_bus gc02m1_dvp = {
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

struct tx_isp_sensor_attribute gc02m1_attr = {
	.name = "gc02m1",
	.chip_id = 0x08a8,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x37,
	.sensor_ctrl.alloc_again = gc02m1_alloc_again,
	.sensor_ctrl.alloc_dgain = gc02m1_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = gc02m1_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list gc02m1_init_regs_1600_1200_30fps_mipi[] = {
	{0xfc,0x01},
	{0xf4,0x41},
	{0xf5,0xe3},//c0->e3 for YH&YJ&YK&YC
	{0xf6,0x44},
	{0xf8,0x38},
	{0xf9,0x82},
	{0xfa,0x00},
	{0xfd,0x80},
	{0xfc,0x81},
	{0xfe,0x03},
	{0x01,0x0b},
	{0xf7,0x01},
	{0xfc,0x80},
	{0xfc,0x80},
	{0xfc,0x80},
	{0xfc,0x8e},
	/*CISCTL*/
	{0xfe,0x00},
	{0x87,0x09},
	{0xee,0x72},
	{0xfe,0x01},
	{0x8c,0x90},
	{0xfe,0x00},
	{0x90,0x00},
	{0x03,0x04},
	{0x04,0x7d},
	{0x41,0x04},
	{0x42,0xf4},
	{0x05,0x04},
	{0x06,0x48},
	{0x07,0x00},
	{0x08,0x18},
	{0x9d,0x18},
	{0x09,0x00},
	{0x0a,0x02},
	{0x0d,0x04},
	{0x0e,0xbc},
	{0x17,0x80},
	{0x19,0x04},
	{0x24,0x00},
	{0x56,0x20},
	{0x5b,0x00},
	{0x5e,0x01},
	/*analogRegister width*/
	{0x21,0x3c},
	{0x44,0x20},
	{0xcc,0x01},
	/*analog mode*/
	{0x1a,0x04},
	{0x1f,0x11},
	{0x27,0x30},
	{0x2b,0x00},
	{0x33,0x00},
	{0x53,0x90},
	{0xe6,0x50},
	/*analog voltage*/
	{0x39,0x07},
	{0x43,0x04},
	{0x46,0x4a},//2a->4a for YH&YJ&YK&YC
	{0x7c,0xa0},
	{0xd0,0xbe},
	{0xd1,0x60},
	{0xd2,0x40},
	{0xd3,0xf3},
	{0xde,0x1d},
	/*analog current*/
	{0xcd,0x05},
	{0xce,0x6f},
	/*CISCTL RESET*/
	{0xfc,0x88},
	{0xfe,0x10},
	{0xfe,0x00},
	{0xfc,0x8e},
	{0xfe,0x00},
	{0xfe,0x00},
	{0xfe,0x00},
	{0xfe,0x00},
	{0xfc,0x88},
	{0xfe,0x10},
	{0xfe,0x00},
	{0xfc,0x8e},
	{0xfe,0x04},
	{0xe0,0x01},
	{0xfe,0x00},
	/*ISP*/
	{0xfe,0x01},
	{0x53,0x54},//44->54 for YH&YJ&YK&YC
	{0x87,0x53},
	{0x89,0x03},
	/*Gain*/
	{0xfe,0x00},
	{0xb0,0x74},
	{0xb1,0x04},
	{0xb2,0x00},
	{0xb6,0x00},
	{0xfe,0x04},
	{0xd8,0x00},
	{0xc0,0x40},
	{0xc0,0x00},
	{0xc0,0x00},
	{0xc0,0x00},
	{0xc0,0x60},
	{0xc0,0x00},
	{0xc0,0xc0},
	{0xc0,0x2a},
	{0xc0,0x80},
	{0xc0,0x00},
	{0xc0,0x00},
	{0xc0,0x40},
	{0xc0,0xa0},
	{0xc0,0x00},
	{0xc0,0x90},
	{0xc0,0x19},
	{0xc0,0xc0},
	{0xc0,0x00},
	{0xc0,0xD0},
	{0xc0,0x2F},
	{0xc0,0xe0},
	{0xc0,0x00},
	{0xc0,0x90},
	{0xc0,0x39},
	{0xc0,0x00},
	{0xc0,0x01},
	{0xc0,0x20},
	{0xc0,0x04},
	{0xc0,0x20},
	{0xc0,0x01},
	{0xc0,0xe0},
	{0xc0,0x0f},
	{0xc0,0x40},
	{0xc0,0x01},
	{0xc0,0xe0},
	{0xc0,0x1a},
	{0xc0,0x60},
	{0xc0,0x01},
	{0xc0,0x20},
	{0xc0,0x25},
	{0xc0,0x80},
	{0xc0,0x01},
	{0xc0,0xa0},
	{0xc0,0x2c},
	{0xc0,0xa0},
	{0xc0,0x01},
	{0xc0,0xe0},
	{0xc0,0x32},
	{0xc0,0xc0},
	{0xc0,0x01},
	{0xc0,0x20},
	{0xc0,0x38},
	{0xc0,0xe0},
	{0xc0,0x01},
	{0xc0,0x60},
	{0xc0,0x3c},
	{0xc0,0x00},
	{0xc0,0x02},
	{0xc0,0xa0},
	{0xc0,0x40},
	{0xc0,0x80},
	{0xc0,0x02},
	{0xc0,0x18},
	{0xc0,0x5c},
	{0xfe,0x00},
	{0x9f,0x10},
	/*BLK*/
	{0xfe,0x00},
	{0x26,0x20},
	{0xfe,0x01},
	{0x40,0x22},
	{0x46,0x7f},
	{0x49,0x0f},
	{0x4a,0xf0},
	{0xfe,0x04},
	{0x14,0x80},
	{0x15,0x80},
	{0x16,0x80},
	{0x17,0x80},
	/*anti_blooming*/
	{0xfe,0x01},
	{0x41,0x20},
	{0x4c,0x00},
	{0x4d,0x0c},
	{0x44,0x08},
	{0x48,0x03},
	/*Window 1600X1200*/
	{0xfe,0x01},
	{0x90,0x01},
	{0x91,0x00},
	{0x92,0x06},
	{0x93,0x00},
	{0x94,0x06},
	{0x95,0x04},
	{0x96,0xb0},
	{0x97,0x06},
	{0x98,0x40},
	/*mipi*/
	{0xfe,0x03},
	{0x01,0x23},
	{0x03,0xce},
	{0x04,0x48},
	{0x15,0x00},
	{0x21,0x10},
	{0x22,0x05},
	{0x23,0x20},
	{0x25,0x20},
	{0x26,0x08},
	{0x29,0x06},
	{0x2a,0x0a},
	{0x2b,0x08},
	/*out*/
	{0xfe,0x01},
	{0x8c,0x10},
	{0xfe,0x00},
	{0x3e,0x90},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list gc02m1_init_regs_1280_720_30fps_mipi[] = {
	//system
	{0xfc,0x01},
	{0xf4,0x41},
	{0xf5,0xe3},//c0->e3forYH&YJ&YK&YC
	{0xf6,0x44},
	{0xf8,0x38},
	{0xf9,0x82},
	{0xfa,0x00},
	{0xfd,0x80},
	{0xfc,0x81},
	{0xfe,0x03},
	{0x01,0x0b},
	{0xf7,0x01},
	{0xfc,0x80},
	{0xfc,0x80},
	{0xfc,0x80},
	{0xfc,0x8e},
	/*CISCTL*/
	{0xfe,0x00},
	{0x87,0x09},
	{0xee,0x72},
	{0xfe,0x01},
	{0x8c,0x90},
	{0xfe,0x00},
	{0x90,0x01},
	{0x03,0x04},
	{0x04,0x7d},
	{0x41,0x04},
	{0x42,0xf4},
	{0x05,0x04},
	{0x06,0x48},
	{0x07,0x00},
	{0x08,0x18},
	{0x9d,0x18},
	{0x09,0x00},
	{0x0a,0x02},
	{0x0d,0x04},
	{0x0e,0xbc},
	{0x17,0x80},
	{0x19,0x04},
	{0x24,0x00},
	{0x56,0x20},
	{0x5b,0x00},
	{0x5e,0x01},
	/*analogRegisterwidth*/
	{0x21,0x3c},
	{0x44,0x20},
	{0xcc,0x01},
	/*analogmode*/
	{0x1a,0x04},
	{0x1f,0x11},
	{0x27,0x30},
	{0x2b,0x00},
	{0x33,0x00},
	{0x53,0x90},
	{0xe6,0x50},
	/*analogvoltage*/
	{0x39,0x07},
	{0x43,0x04},
	{0x46,0x4a},//2a->4aforYH&YJ&YK&YC
	{0x7c,0xa0},
	{0xd0,0xbe},
	{0xd1,0x60},
	{0xd2,0x40},
	{0xd3,0xf3},
	{0xde,0x1d},
	/*analogcurrent*/
	{0xcd,0x05},
	{0xce,0x6f},
	/*CISCTLRESET*/
	{0xfc,0x88},
	{0xfe,0x10},
	{0xfe,0x00},
	{0xfc,0x8e},
	{0xfe,0x00},
	{0xfe,0x00},
	{0xfe,0x00},
	{0xfe,0x00},
	{0xfc,0x88},
	{0xfe,0x10},
	{0xfe,0x00},
	{0xfc,0x8e},
	{0xfe,0x04},
	{0xe0,0x01},
	{0xfe,0x00},
	/*ISP*/
	{0xfe,0x01},
	{0x53,0x54},//44->54forYH&YJ&YK&YC
	{0x87,0x53},
	{0x89,0x03},
	/*Gain*/
	{0xfe,0x00},
	{0xb0,0x74},
	{0xb1,0x04},
	{0xb2,0x00},
	{0xb6,0x00},
	{0xfe,0x04},
	{0xd8,0x00},
	{0xc0,0x40},
	{0xc0,0x00},
	{0xc0,0x00},
	{0xc0,0x00},
	{0xc0,0x60},
	{0xc0,0x00},
	{0xc0,0xc0},
	{0xc0,0x2a},
	{0xc0,0x80},
	{0xc0,0x00},
	{0xc0,0x00},
	{0xc0,0x40},
	{0xc0,0xa0},
	{0xc0,0x00},
	{0xc0,0x90},
	{0xc0,0x19},
	{0xc0,0xc0},
	{0xc0,0x00},
	{0xc0,0xD0},
	{0xc0,0x2F},
	{0xc0,0xe0},
	{0xc0,0x00},
	{0xc0,0x90},
	{0xc0,0x39},
	{0xc0,0x00},
	{0xc0,0x01},
	{0xc0,0x20},
	{0xc0,0x04},
	{0xc0,0x20},
	{0xc0,0x01},
	{0xc0,0xe0},
	{0xc0,0x0f},
	{0xc0,0x40},
	{0xc0,0x01},
	{0xc0,0xe0},
	{0xc0,0x1a},
	{0xc0,0x60},
	{0xc0,0x01},
	{0xc0,0x20},
	{0xc0,0x25},
	{0xc0,0x80},
	{0xc0,0x01},
	{0xc0,0xa0},
	{0xc0,0x2c},
	{0xc0,0xa0},
	{0xc0,0x01},
	{0xc0,0xe0},
	{0xc0,0x32},
	{0xc0,0xc0},
	{0xc0,0x01},
	{0xc0,0x20},
	{0xc0,0x38},
	{0xc0,0xe0},
	{0xc0,0x01},
	{0xc0,0x60},
	{0xc0,0x3c},
	{0xc0,0x00},
	{0xc0,0x02},
	{0xc0,0xa0},
	{0xc0,0x40},
	{0xc0,0x80},
	{0xc0,0x02},
	{0xc0,0x18},
	{0xc0,0x5c},
	{0xfe,0x00},
	{0x9f,0x10},
	/*BLK*/
	{0xfe,0x00},
	{0x26,0x20},
	{0xfe,0x01},
	{0x40,0x22},
	{0x46,0x7f},
	{0x49,0x0f},
	{0x4a,0xf0},
	{0xfe,0x04},
	{0x14,0x80},
	{0x15,0x80},
	{0x16,0x80},
	{0x17,0x80},
	/*anti_blooming*/
	{0xfe,0x01},
	{0x41,0x20},
	{0x4c,0x00},
	{0x4d,0x0c},
	{0x44,0x08},
	{0x48,0x03},
	/*Window1280X720*/
	{0xfe,0x01},
	{0x90,0x01},
	{0x91,0x00},
	{0x92,0xf6},
	{0x93,0x00},
	{0x94,0xa6},
	{0x95,0x02},
	{0x96,0xd0},//4b0
	{0x97,0x05},
	{0x98,0x00},//640
	/*mipi*/
	{0xfe,0x03},
	{0x01,0x23},
	{0x03,0xce},
	{0x04,0x48},
	{0x15,0x00},
	{0x21,0x10},
	{0x22,0x05},
	{0x23,0x20},
	{0x25,0x20},
	{0x26,0x08},
	{0x29,0x06},
	{0x2a,0x0a},
	{0x2b,0x08},
	/*out*/
	{0xfe,0x01},
	{0x8c,0x10},
	{0xfe,0x00},
	{0x3e,0x90},
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the gc02m1_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc02m1_win_sizes[] = {
	{
		.width          = 1600,
		.height         = 1200,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc02m1_init_regs_1600_1200_30fps_mipi,
	},
	{
		.width          = 1280,
		.height         = 720,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc02m1_init_regs_1280_720_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &gc02m1_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc02m1_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list gc02m1_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc02m1_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc02m1_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc02m1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc02m1_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc02m1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc02m1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc02m1_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int gc02m1_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc02m1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc02m1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc02m1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	// unsigned char val = 0;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc02m1_write(sd, vals->reg_num, vals->value);
			// ret = gc02m1_read(sd, vals->reg_num, &val);
			// printk("	{0x%x 0x%x}\n", vals->reg_num, val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int gc02m1_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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
				if ((mclk != 24000000) && (mclk != 27000000) && (mclk != 37125000)) {
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

static int gc02m1_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int gc02m1_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &gc02m1_win_sizes[0];
		gc02m1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc02m1_attr.again.value = 65536;
		gc02m1_attr.again.max = 283315;
		gc02m1_attr.again.min = 0;
		gc02m1_attr.again.apply_delay = 2;

		gc02m1_attr.integration_time.value = 0x5b;
		gc02m1_attr.integration_time.max = 1252 - 16;
		gc02m1_attr.integration_time.min = 2;
		gc02m1_attr.integration_time.apply_delay = 2;

		gc02m1_attr.total_width = 2192;
		gc02m1_attr.total_height = 1252;

		gc02m1_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		gc02m1_attr.hcg.base_gain = ;
		gc02m1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc02m1_attr.again_short.value = ;
		gc02m1_attr.again_short.max = ;
		gc02m1_attr.again_short.min = ;
		gc02m1_attr.again_short.apply_delay = ;

		gc02m1_attr.integration_time_short.value = ;
		gc02m1_attr.integration_time_short.max = ;
		gc02m1_attr.integration_time_short.min = ;
		gc02m1_attr.integration_time_short.apply_delay = ;

		gc02m1_attr.wdr_cache = wdr_line * gc02m1_attr.total_width;

#ifdef SENSOR_HCG
		gc02m1_attr.hcg_short.base_gain = ;
		gc02m1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc02m1_attr.mipi)), (void *)(&gc02m1_mipi_linear), sizeof(gc02m1_attr.mipi));
		break;
	case 1:
		wsize = &gc02m1_win_sizes[0];
		gc02m1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc02m1_attr.again.value = 65536;
		gc02m1_attr.again.max = 283315;
		gc02m1_attr.again.min = 0;
		gc02m1_attr.again.apply_delay = 2;

		gc02m1_attr.integration_time.value = 0x5b;
		gc02m1_attr.integration_time.max = 1252 - 16;
		gc02m1_attr.integration_time.min = 2;
		gc02m1_attr.integration_time.apply_delay = 2;

		gc02m1_attr.total_width = 2192;
		gc02m1_attr.total_height = 1252;

		gc02m1_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		gc02m1_attr.hcg.base_gain = ;
		gc02m1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc02m1_attr.again_short.value = ;
		gc02m1_attr.again_short.max = ;
		gc02m1_attr.again_short.min = ;
		gc02m1_attr.again_short.apply_delay = ;

		gc02m1_attr.integration_time_short.value = ;
		gc02m1_attr.integration_time_short.max = ;
		gc02m1_attr.integration_time_short.min = ;
		gc02m1_attr.integration_time_short.apply_delay = ;

		gc02m1_attr.wdr_cache = wdr_line * gc02m1_attr.total_width;

#ifdef SENSOR_HCG
		gc02m1_attr.hcg_short.base_gain = ;
		gc02m1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc02m1_attr.mipi)), (void *)(&gc02m1_mipi_linear), sizeof(gc02m1_attr.mipi));
		gc02m1_attr.mipi.image_twidth = 1280;
		gc02m1_attr.mipi.image_theight = 720;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int gc02m1_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	gc02m1_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc02m1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc02m1_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc02m1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc02m1_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc02m1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

#ifndef ZRT_SENSOR_WITHOUT_INIT
	switch (info->mclk) {
	case TISP_SENSOR_MCLK0:
	case TISP_SENSOR_MCLK1:
	case TISP_SENSOR_MCLK2:
		sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
		sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
		set_sensor_mclk_function(0);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	ret = gc02m1_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	gc02m1_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc02m1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc02m1_read(sd, 0xf0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc02m1_read(sd, 0xf1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc02m1_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	gc02m1_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "gc02m1_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "gc02m1_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = gc02m1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc02m1 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", gc02m1_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "gc02m1", sizeof("gc02m1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc02m1_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc02m1_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = gc02m1_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int gc02m1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc02m1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc02m1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc02m1_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int gc02m1_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = gc02m1_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc02m1_write_array(sd, gc02m1_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc02m1 stream on\n");
		}

	} else {
		ret = gc02m1_write_array(sd, gc02m1_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("gc02m1 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc02m1_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *lut = gc02m1_again_lut;
	printk("------it = %d again = %d------\n", it ,again);
	ret += gc02m1_write(sd, 0xfe, 0x00);
	ret += gc02m1_write(sd, 0x03, (unsigned char)((it >> 8) & 0xff));
	ret += gc02m1_write(sd, 0x04, (unsigned char)(it & 0xff));

	ret += gc02m1_write(sd, 0xb6, (unsigned char)(lut[again].reg_00b6));
	ret += gc02m1_write(sd, 0xb1, (unsigned char)(lut[again].reg_00b1));
	ret += gc02m1_write(sd, 0xb2, (unsigned char)(lut[again].reg_00b2));

	return ret;
}
#else
static int gc02m1_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	/*set integration time*/
	ret = gc5603_write(sd, 0x0d04, value & 0xff);
	ret += gc5603_write(sd, 0x0d03, value >> 8);

	return ret;
}

static int gc02m1_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	struct again_lut *val_lut = gc02m1_again_lut;

	/*set analog gain*/
	ret += gc02m1_write(sd, 0xd0, val_lut[again].again_pga);
	ret += gc02m1_write(sd, 0x31d, 0x2e);
	ret += gc02m1_write(sd, 0xdc1, val_lut[again].again_shift);
	ret += gc02m1_write(sd, 0x31d, 0x28);
	ret += gc02m1_write(sd, 0xb8, val_lut[again].col_gain0);
	ret += gc02m1_write(sd, 0xb9, val_lut[again].col_gain1);
	/*set blc*/
	ret += gc02m1_write(sd, 0x0155, val_lut[again].exp_rate_darkc);
	ret += gc02m1_write(sd, 0x0410, val_lut[again].darks_g1);
	ret += gc02m1_write(sd, 0x0411, val_lut[again].darks_r1);
	ret += gc02m1_write(sd, 0x0412, val_lut[again].darks_b1);
	ret += gc02m1_write(sd, 0x0413, val_lut[again].darks_g2);
	ret += gc02m1_write(sd, 0x0414, val_lut[again].darkn_g1);
	ret += gc02m1_write(sd, 0x0415, val_lut[again].darkn_r1);
	ret += gc02m1_write(sd, 0x0416, val_lut[again].darkn_b1);
	ret += gc02m1_write(sd, 0x0417, val_lut[again].darkn_g2);

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc02m1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc02m1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc02m1_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = gc02m1_attr_set(sd, wsize);
	}

	return ret;
}

static int gc02m1_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;
	unsigned char tmp;
	int ret = ISP_SUCCESS;

	switch (info->default_boot) {
	case 0:
		sclk = 0x448 * 2 * 0x4f4 * 30;  /**< HTS * VTS * FPS */
		max_fps = 30;
		break;
	case 1:
		sclk = 1096 * 2 * 1252 * 30;  /**< HTS * VTS * FPS */
		max_fps = 30;
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

	ret =  gc02m1_write(sd, 0xfe, 0x00);
	ret += gc02m1_read(sd, 0x05, &tmp);
	hts = tmp;
	ret += gc02m1_read(sd, 0x06, &tmp);
	if (ret < 0)
		return -1;
	hts = ((hts << 8) | tmp) << 1;
	if (0 != ret) {
		ISP_ERROR("err: gc02m1 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	gc02m1_write(sd, 0x41, (unsigned char) ((vts >> 8) & 0x3f));
	gc02m1_write(sd, 0x42, (unsigned char) (vts & 0xff));

	if (0 != ret) {
		ISP_ERROR("err: gc02m1_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 16;
#else
	/* WDR mode */
	switch (info->default_boot) {
	case 0:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - 8;
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

static int gc02m1_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	ret =  gc02m1_write(sd, 0xfe, 0x00);
	/* 2'b01:mirror,2'b10:filp */
	ret = gc02m1_read(sd, 0x17, &val);
	// printk("\n==> [%s %d] par->hvflip %d\n", __func__, __LINE__, par->hvflip);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val = 0x80;
		// sensor->video.mbus.code = TISP_VI_FMT_SRGGB10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val = 0x81;
		// sensor->video.mbus.code = TISP_VI_FMT_SGRBG10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val = 0x82;
		// sensor->video.mbus.code = TISP_VI_FMT_SGBRG10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val = 0x83;
		// sensor->video.mbus.code = TISP_VI_FMT_SBGGR10_1X10;
		break;
	}
	ret += gc02m1_write(sd, 0x17, val);

	// sensor->video.mbus_change = 1;
	sensor->video.hvflip_mode = par->hvflip;
	gc02m1_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int gc02m1_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int gc02m1_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int gc02m1_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc02m1_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = gc02m1_write_array(sd, gc02m1_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		gc02m1_setting_select(sd, 1);
		gc02m1_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		gc02m1_setting_select(sd, 0);
		gc02m1_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int gc02m1_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = gc02m1_write_array(sd, wsize->regs);
	ret = gc02m1_write_array(sd, gc02m1_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc02m1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = gc02m1_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = gc02m1_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = gc02m1_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc02m1_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc02m1_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc02m1_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = gc02m1_write_array(sd, gc02m1_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = gc02m1_write_array(sd, gc02m1_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc02m1_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = gc02m1_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = gc02m1_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = gc02m1_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = gc02m1_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = gc02m1_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = gc02m1_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc02m1_core_ops = {
	.g_chip_ident = gc02m1_g_chip_ident,
	.reset = gc02m1_reset,
	.init = gc02m1_init,
	.g_register = gc02m1_g_register,
	.s_register = gc02m1_s_register,
};

static struct tx_isp_subdev_video_ops gc02m1_video_ops = {
	.s_stream = gc02m1_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc02m1_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = gc02m1_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc02m1_ops = {
	.core = &gc02m1_core_ops,
	.video = &gc02m1_video_ops,
	.sensor = &gc02m1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "gc02m1",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc02m1_probe(struct i2c_client *client,
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
	sensor->video.attr = &gc02m1_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc02m1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc02m1\n");

	return 0;
}

static int gc02m1_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc02m1_id[] = {
	{"gc02m1", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc02m1_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver gc02m1_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "gc02m1",
	},
	.probe          = gc02m1_probe,
	.remove         = gc02m1_remove,
	.id_table       = gc02m1_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc02m1(void) {
	return private_i2c_add_driver(&gc02m1_driver);
}

static __exit void exit_gc02m1(void) {
	private_i2c_del_driver(&gc02m1_driver);
}

module_init(init_gc02m1);
module_exit(exit_gc02m1);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc02m1(void) {
	return private_i2c_add_driver(&gc02m1_driver);
}

static void exit_first_gc02m1(void) {
	private_i2c_del_driver(&gc02m1_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "gc02m1",
	.i2c_addr = 0x37,
	.width = 1600,
	.height = 1200,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_gc02m1,
	.exit_sensor = exit_first_gc02m1
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for gc02m1 sensor");
MODULE_LICENSE("GPL");
