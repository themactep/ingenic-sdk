/*
 * os05l10.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps     dvdd
 *  0           2880*1620       30        mipi_2lane    linear  10        30
 *  1           2880*1620       15        mipi_2lane    wdr     10        15
 * @I2C addr:0x3c, 0x3d
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

#define TVERSION "V20241023a"
#define SENSOR_VERSION  "H20250211a"

// #define SENSOR_TEST

#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE	  /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
#define SENSOR_WDR_2_FRAME	/**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP			/**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x53)
#define SENSOR_CHIP_ID_M    (0x05)
#define SENSOR_CHIP_ID_L    (0x4C)
#define SENSOR_OUTPUT_MIN_FPS 1
#define SENSOR_MCLK 24000000

#ifdef CONFIG_ZERATUL
#define ZRT_SENSOR_WITHOUT_INIT
#endif

#ifdef SENSOR_WDR_2_FRAME
static int wdr_line = 657;
#endif

#ifndef SENSOR_I2C_REG_8BIT
#define SENSOR_I2C_REG_16BIT
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_8BIT
#define SENSOR_REG_END	  0xffff
#define SENSOR_REG_DELAY  0xfffe
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_16BIT
#define SENSOR_REG_END	  0xffff
#define SENSOR_REG_DELAY  0xfffe
#endif /* SENSOR_I2C_REG_16BIT */

struct regval_list {
#ifdef SENSOR_I2C_REG_8BIT
    uint16_t reg_num;
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_16BIT
    uint16_t reg_num;
#endif /* SENSOR_I2C_REG_16BIT */
    uint8_t value;
};

struct tx_isp_sensor_attribute os05l10_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
    unsigned int value;
    unsigned int gain;
};

struct again_lut os05l10_again_lut[] = {
    {0x10, 0},
    {0x11, 5732},
    {0x12, 11136},
    {0x13, 16248},
    {0x14, 21098},
    {0x15, 25711},
    {0x16, 30109},
    {0x17, 34312},
    {0x18, 38336},
    {0x19, 42196},
    {0x1a, 45904},
    {0x1b, 49472},
    {0x1c, 52911},
    {0x1d, 56229},
    {0x1e, 59434},
    {0x1f, 62534},
    {0x21, 65536},
    {0x23, 71268},
    {0x25, 76672},
    {0x27, 81784},
    {0x29, 86634},
    {0x2b, 91247},
    {0x2d, 95645},
    {0x2f, 99848},
    {0x31, 103872},
    {0x33, 107732},
    {0x35, 111440},
    {0x37, 115008},
    {0x39, 118447},
    {0x3b, 121765},
    {0x3d, 124970},
    {0x3f, 128070},
    {0x43, 131072},
    {0x47, 136804},
    {0x4b, 142208},
    {0x4f, 147320},
    {0x53, 152170},
    {0x57, 156783},
    {0x5b, 161181},
    {0x5f, 165384},
    {0x63, 169408},
    {0x67, 173268},
    {0x6b, 176976},
    {0x6f, 180544},
    {0x73, 183983},
    {0x77, 187301},
    {0x7b, 190506},
    {0x7f, 193606},
    {0x87, 196608},
    {0x8f, 202340},
    {0x97, 207744},
    {0x9f, 212856},
    {0xa7, 217706},
    {0xaf, 222319},
    {0xb7, 226717},
    {0xbf, 230920},
    {0xc7, 234944},
    {0xcf, 238804},
    {0xd7, 242512},
    {0xdf, 246080},
    {0xe7, 249519},
    {0xef, 252837},
    {0xf7, 256042},
    {0xff, 259142},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int os05l10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = os05l10_again_lut;
    while (lut->gain <= os05l10_attr.again.max) {
        if (isp_gain == 0) {
            *sensor_again = lut[0].value;
            return 0;
        } else if (isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        } else {
            if ((lut->gain == os05l10_attr.again.max) && (isp_gain >= lut->gain)) {
                *sensor_again = lut->value;
                return lut->gain;
            }
        }

        lut++;
    }

#else
    /* Non analog gain table */
    ...;
#endif	/* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

    return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int os05l10_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = os05l10_again_lut;
    while(lut->gain <= os05l10_attr.again_short.max) {
        if(isp_gain == 0) {
            *sensor_again = 0;
            return 0;
        }
        else if(isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        }
        else{
            if((lut->gain == os05l10_attr.again_short.max) && (isp_gain >= lut->gain)) {
                *sensor_again = lut->value;
                return lut->gain;
            }
        }

        lut++;
    }

#else
    /* Non analog gain table */

    ...;
#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

    return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int os05l10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
    return 0;
}

#ifdef SENSOR_HCG
unsigned int os05l10_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int os05l10_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus os05l10_mipi_linear = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 840,
    .lans = 2,
    .image_twidth = 2880,
    .image_theight = 1620,
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

struct tx_isp_mipi_bus os05l10_mipi_wdr = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 840,
    .lans = 2,
    .image_twidth = 2880,
    .image_theight = 1620,
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
    .mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
    .mipi_sc.sensor_fid_mode = 0,
    .mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_dvp_bus os05l10_dvp = {
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

struct tx_isp_sensor_attribute os05l10_attr = {
    .name = "os05l10",
    .chip_id = 0x53054c,
    .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
    .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
    .cbus_device = 0x3c,
    .sensor_ctrl.alloc_again = os05l10_alloc_again,
    .sensor_ctrl.alloc_dgain = os05l10_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
    .sensor_ctrl.alloc_again_short = os05l10_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

    .dgain = {0},
    .dgain_short = {0},
};

static struct regval_list os05l10_init_regs_2880_1620_30fps_mipi[] = {
    //OS05L10_mclk24M_2880x1620_mipi_2lane_840Mbps_linear_row10_30fps_v14_ross.luo_20241204
    {0xfd, 0x00},
    {0x20, 0x00},
    {0xfd, 0x00},
    {0x20, 0x2b},
    {0xe7, 0x03},
    {0xe7, 0x00},
    {0xfd, 0x00},
    {0x21, 0x06},
    {0x14, 0x8c},
    {0x18, 0x61},
    {0x19, 0x80},
    {0x1a, 0x06},
    {0x1b, 0x69},
    {0x1c, 0x01},
    {0x1d, 0x02},
    {0xfd, 0x00},
    {0x21, 0x00},
    {SENSOR_REG_DELAY, 0x02},
    {0xfd, 0x0f},
    {0x01, 0xe6},
    {0x07, 0x0f},
    {0x08, 0xf0},
    {0x0f, 0x08},
    {0x15, 0x28},
    {0x16, 0x11},
    {0x20, 0x06},
    {0x2e, 0x1c},
    {0x2f, 0x3d},
    {0x30, 0x77},
    {0x31, 0xe5},
    {0x0b, 0x01},
    {0x13, 0x22},
    {0x14, 0xbc},
    {0x2d, 0x0a},
    {0x17, 0x3e},
    {0x1b, 0xe6},
    {0x1c, 0x99},
    {0x1d, 0x99},
    {0x1e, 0x55},
    {0xfd, 0x01},
    {0x02, 0x00},
    {0x03, 0x00},
    {0x04, 0x04},
    {0x06, 0x5c},
    {0x24, 0x10},
    {0x21, 0x00},
    {0x22, 0x40},
    {0x31, 0x00},
    {0x33, 0x03},
    {0x40, 0x30},
    {0x41, 0x0c},
    {0x43, 0x44},
    {0x44, 0x0b},
    {0x46, 0x01},
    {0x48, 0x08},
    {0x4c, 0x10},
    {0x50, 0x0a},
    {0x51, 0x08},
    {0x52, 0x08},
    {0x53, 0x0a},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x57, 0x92},
    {0x58, 0x00},
    {0x59, 0x05},
    {0x5a, 0x05},
    {0x5b, 0x00},
    {0x5c, 0x0b},
    {0x5e, 0x08},
    {0x66, 0x0d},
    {0x69, 0x0d},
    {0x76, 0x0e},
    {0x7a, 0x00},
    {0x7c, 0x03},
    {0x83, 0x03},
    {0x84, 0x25},
    {0x85, 0x03},
    {0x87, 0x03},
    {0x90, 0x33},
    {0x91, 0x1e},
    {0x92, 0x15},
    {0x93, 0x16},
    {0x94, 0x08},
    {0x95, 0x33},
    {0x9c, 0x03},
    {0x9d, 0x13},
    {0x9e, 0x03},
    {0xa0, 0x13},
    {0xa4, 0x01},
    {0xa5, 0x01},
    {0xa7, 0x01},
    {0xc0, 0x09},
    {0xc1, 0x38},
    {0xc8, 0x1f},
    {0xd7, 0x11},
    {0xd9, 0x1c},
    {0xd8, 0x1f},
    {0xda, 0x2f},
    {0xdb, 0x10},
    {0xdd, 0x1b},
    {0xdc, 0x1e},
    {0xde, 0x2d},
    {0xed, 0x33},
    {0xee, 0x33},
    {0x01, 0x01},
    {0xfd, 0x02},
    {0x9a, 0x03},
    {0x05, 0x01},
    {0x0b, 0x0c},
    {0x0c, 0x0f},
    {0x03, 0x24},
    {0x04, 0x12},
    {0x0d, 0x0f},
    {0x0e, 0xff},
    {0x0f, 0xff},
    {0x10, 0xff},
    {0xfd, 0x01},
    {0x01, 0x01},
    {0xfd, 0x04},
    {0x19, 0x3f},
    {0x12, 0x00},
    {0xf3, 0x00},
    {0xfd, 0x07},
    {0xb0, 0x00},
    {0x10, 0xf0},
    {0x42, 0x00},
    {0x43, 0x76},
    {0x44, 0x00},
    {0x45, 0x76},
    {0x46, 0x00},
    {0x47, 0x76},
    {0x48, 0x00},
    {0x49, 0x76},
    {0xb3, 0x02},
    {0xb4, 0x20},
    {0xb7, 0x02},
    {0xb8, 0x20},
    {0xfd, 0x07},
    {0xc5, 0x1c},
    {0xc9, 0x1c},
    {0xcd, 0x1c},
    {0xc6, 0x1a},
    {0xca, 0x1a},
    {0xce, 0x1a},
    {0xc3, 0x07},
    {0xc7, 0x07},
    {0xcb, 0x07},
    {0xc4, 0x09},
    {0xc8, 0x09},
    {0xcc, 0x09},
    {0xcf, 0x0a},
    {0xd3, 0x0a},
    {0xd7, 0x0a},
    {0xd0, 0x08},
    {0xd4, 0x08},
    {0xd8, 0x08},
    {0xbb, 0x7f},
    {0xd1, 0x06},
    {0xd5, 0x06},
    {0xd9, 0x06},
    {0xd2, 0x04},
    {0xd6, 0x04},
    {0xda, 0x04},
    {0xbc, 0x3f},
    {0xfd, 0x03},
    {0x85, 0x03},
    {0x9d, 0x0f},
    {0xba, 0x06},
    {0xfd, 0x01},
    {0xfd, 0x02},
    {0xa1, 0x04},
    {0xa3, 0x54},
    {0xa5, 0x04},
    {0xa7, 0x40},
    {0xfd, 0x00},
    {0x8e, 0x0b},
    {0x8f, 0x40},
    {0x90, 0x06},
    {0x91, 0x54},
    {0x94, 0x08},
    {0x95, 0x09},
    {0x99, 0x08},
    {0x9c, 0x20},
    {0xa4, 0x0c},
    {0x9d, 0x01},
    {0xa5, 0xff},
    {0xa1, 0x04},
    {0xc1, 0xee},
    {0xc5, 0x50},
    {0xc4, 0x01},
    {0xa0, 0x01},
    {0xfd, 0x01},
    {0xfd, 0x01},
    {0x01, 0x31},
    {0xfd, 0x00},
    {0x20, 0x1f},
    {0xfd, 0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list os05l10_init_regs_2880_1620_30fps_mipi_wdr[] = {
    //OS05L10_mclk24M_2880x1620_mipi_2lane_840Mbps_HDR_raw10_15fps_ross.luo_20241204 ;HTS*VTS: 800x1750
    {0xfd, 0x00},
    {0x20, 0x00},
    {0xfd, 0x00},
    {0x20, 0x2b},
    {0xe7, 0x03},
    {0xe7, 0x00},
    {0xfd, 0x00},
    {0x21, 0x06},
    {0x14, 0x8c},
    {0x18, 0x61},
    {0x19, 0x80},
    {0x1a, 0x06},
    {0x1b, 0x69},
    {0x1c, 0x04},
    {0x1d, 0x02},
    {0xfd, 0x00},
    {0x21, 0x00},
    {SENSOR_REG_DELAY, 0x02},
    {0xfd, 0x0f},
    {0x01, 0xe6},
    {0x07, 0x0f},
    {0x08, 0xf0},
    {0x0f, 0x08},
    {0x15, 0x28},
    {0x16, 0x11},
    {0x20, 0x06},
    {0x2e, 0x1c},
    {0x2f, 0x3d},
    {0x30, 0x77},
    {0x31, 0xe5},
    {0x0b, 0x01},
    {0x13, 0x22},
    {0x14, 0xbc},
    {0x2d, 0x0a},
    {0x17, 0x3e},
    {0x1b, 0xe6},
    {0x1c, 0x99},
    {0x1d, 0x99},
    {0x1e, 0x55},
    {0xfd, 0x01},
    {0x02, 0x00},
    {0x03, 0x00},
    {0x04, 0x02},
    {0x06, 0x60},
    {0x24, 0xff},
    {0x21, 0x00},
    {0x22, 0x40},
    {0x33, 0x03},
    {0x40, 0x30},
    {0x41, 0x0c},
    {0x43, 0x44},
    {0x44, 0x0b},
    {0x46, 0x01},
    {0x48, 0x08},
    {0x4c, 0x10},
    {0x50, 0x0a},
    {0x51, 0x08},
    {0x52, 0x08},
    {0x53, 0x0a},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x57, 0x92},
    {0x58, 0x00},
    {0x59, 0x05},
    {0x5a, 0x05},
    {0x5b, 0x00},
    {0x5c, 0x0b},
    {0x5e, 0x08},
    {0x66, 0x0d},
    {0x69, 0x0d},
    {0x76, 0x0e},
    {0x7a, 0x00},
    {0x7c, 0x03},
    {0x83, 0x03},
    {0x84, 0x25},
    {0x85, 0x03},
    {0x87, 0x03},
    {0x90, 0x33},
    {0x91, 0x1e},
    {0x92, 0x15},
    {0x93, 0x16},
    {0x94, 0x08},
    {0x95, 0x33},
    {0x9c, 0x03},
    {0x9d, 0x13},
    {0x9e, 0x03},
    {0xa0, 0x13},
    {0xa4, 0x01},
    {0xa5, 0x01},
    {0xa7, 0x01},
    {0xc0, 0x09},
    {0xc1, 0x38},
    {0xc8, 0x1f},
    {0xd7, 0x11},
    {0xd9, 0x1c},
    {0xd8, 0x1f},
    {0xda, 0x2f},
    {0xdb, 0x10},
    {0xdd, 0x1b},
    {0xdc, 0x1e},
    {0xde, 0x2d},
    {0xed, 0x33},
    {0xee, 0x33},
    {0x01, 0x01},
    {0xfd, 0x02},
    {0x9a, 0x03},
    {0x05, 0x01},
    {0x0b, 0x0c},
    {0x0c, 0x0f},
    {0xfd, 0x01},
    {0x01, 0x01},
    {0xfd, 0x04},
    {0x19, 0x3f},
    {0x12, 0x00},
    {0xf3, 0x00},
    {0xfd, 0x07},
    {0x10, 0xf0},
    {0x42, 0x00},
    {0x43, 0x76},
    {0x44, 0x00},
    {0x45, 0x76},
    {0x46, 0x00},
    {0x47, 0x76},
    {0x48, 0x00},
    {0x49, 0x76},
    {0xb3, 0x02},
    {0xb4, 0x20},
    {0xb7, 0x02},
    {0xb8, 0x20},
    {0xc5, 0x3a},
    {0xc9, 0x3a},
    {0xcd, 0x3a},
    {0xc6, 0x2b},
    {0xca, 0x2b},
    {0xce, 0x2b},
    {0xc3, 0x0a},
    {0xc7, 0x0a},
    {0xcb, 0x0a},
    {0xc4, 0x12},
    {0xc8, 0x12},
    {0xcc, 0x12},
    {0xcf, 0x11},
    {0xd3, 0x11},
    {0xd7, 0x11},
    {0xd0, 0x0e},
    {0xd4, 0x0e},
    {0xd8, 0x0e},
    {0xbb, 0x7f},
    {0xd1, 0x0f},
    {0xd5, 0x0f},
    {0xd9, 0x0f},
    {0xd2, 0x0a},
    {0xd6, 0x0a},
    {0xda, 0x0a},
    {0xbc, 0x3f},
    {0xfd, 0x03},
    {0x85, 0x03},
    {0x9d, 0x0f},
    {0xba, 0x06},
    {0xfd, 0x01},
    {0x31, 0x21},
    {0x27, 0x02},
    {0x2c, 0x00},
    {0x2d, 0x00},
    {0x2e, 0x06},
    {0x2f, 0x54},
    {0xfd, 0x02},
    {0xa5, 0x04},
    {0xa7, 0x40},
    {0xfd, 0x01},
    {0x01, 0x01},
    {0xfd, 0x00},
    {0x8e, 0x0b},
    {0x8f, 0x40},
    {0x90, 0x06},
    {0x91, 0x54},
    {0x94, 0x08},
    {0x95, 0x09},
    {0x99, 0x08},
    {0x9c, 0x20},
    {0xa4, 0x0c},
    {0x9d, 0x01},
    {0xa5, 0xff},
    {0xa1, 0x04},
    {0xc1, 0xee},
    {0xc5, 0x50},
    {0xc4, 0x01},
    {0xa0, 0x01},
    {0xfd, 0x01},
    {0xfd, 0x01},
    {0x01, 0x31},
    {0xfd, 0x00},
    {0x20, 0x1f},
    {0xfd, 0x01},
    {0xfd, 0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the os05l10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os05l10_win_sizes[] = {
    {
        .width          = 2880,
        .height         = 1620,
        .fps            = 30 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = os05l10_init_regs_2880_1620_30fps_mipi,
    },
    {
        .width          = 2880,
        .height         = 1620,
        .fps            = 15 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = os05l10_init_regs_2880_1620_30fps_mipi_wdr,
    },
};

static struct tx_isp_sensor_win_setting *wsize = &os05l10_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list os05l10_stream_on_mipi[] = {
    /* {0xfd, 0x01}, */
    /* {0x01, 0x31}, */
    /* {0xfd, 0x00}, */
    /* {0x20, 0x1f}, */
    /* {0xfd, 0x01}, */
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list os05l10_stream_off_mipi[] = {
    /* {0xfd, 0x01}, */
    /* {0x01, 0x21}, */
    /* {0xfd, 0x00}, */
    /* {0x20, 0x2b}, */
    /* {0xfd, 0x01}, */
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int os05l10_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int os05l10_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int os05l10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = os05l10_read(sd, vals->reg_num, &val);
            /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif

static int os05l10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = os05l10_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif	/* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int os05l10_read(struct tx_isp_subdev *sd, uint16_t reg,
                 unsigned char *value) {
    int ret;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
    struct i2c_msg msg[2] = {
        [0] = {
            .addr	= client->addr,
            .flags	= 0,
            .len	= 2,
            .buf	= buf,
        },
        [1] = {
            .addr	= client->addr,
            .flags	= I2C_M_RD,
            .len	= 1,
            .buf	= value,
        }
    };

    ret = private_i2c_transfer(client->adapter, msg, 2);
    if (ret > 0)
        ret = 0;

    return ret;
}

int os05l10_write(struct tx_isp_subdev *sd, uint16_t reg,
                  unsigned char value)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
    struct i2c_msg msg = {
        .addr	= client->addr,
        .flags	= 0,
        .len	= 3,
        .buf	= buf,
    };
    int ret;
    ret = private_i2c_transfer(client->adapter, &msg, 1);
    if (ret > 0)
        ret = 0;

    return ret;
}

#if 0
static int os05l10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = os05l10_read(sd, vals->reg_num, &val);
            if (ret < 0)
                return ret;
        }
        vals++;
    }
    return 0;
}
#endif

static int os05l10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = os05l10_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif	/* SENSOR_I2C_REG_16BIT */

static int os05l10_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int os05l10_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int os05l10_setting_select(struct tx_isp_subdev *sd, int deboot)
{
    int ret = ISP_SUCCESS;

    switch (deboot) {
    case 0:
        wsize = &os05l10_win_sizes[0];
        os05l10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        os05l10_attr.again.value = 65536;
        os05l10_attr.again.max = 259142;
        os05l10_attr.again.min = 0;
        os05l10_attr.again.apply_delay = 2;

        os05l10_attr.integration_time.value = 0x7b6;
        os05l10_attr.integration_time.max = 1750 - 5;
        os05l10_attr.integration_time.min = 2;
        os05l10_attr.integration_time.apply_delay = 2;

        os05l10_attr.total_width = 400;
        os05l10_attr.total_height = 1750;

        os05l10_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
        os05l10_attr.hcg.base_gain = ;
        os05l10_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        /* os05l10_attr.again_short.value = ; */
        /* os05l10_attr.again_short.max = ; */
        /* os05l10_attr.again_short.min = ; */
        /* os05l10_attr.again_short.apply_delay = ; */

        /* os05l10_attr.integration_time_short.value = ; */
        /* os05l10_attr.integration_time_short.max = ; */
        /* os05l10_attr.integration_time_short.min = ; */
        /* os05l10_attr.integration_time_short.apply_delay = ; */

        /* os05l10_attr.wdr_cache = wdr_line * os05l10_attr.total_width; */

#ifdef SENSOR_HCG
        os05l10_attr.hcg_short.base_gain = ;
        os05l10_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(os05l10_attr.mipi)), (void *)(&os05l10_mipi_linear), sizeof(os05l10_attr.mipi));
        break;
    case 1:
        wsize = &os05l10_win_sizes[1];
        os05l10_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;

        os05l10_attr.again.value = 65536;
        os05l10_attr.again.max = 259142;
        os05l10_attr.again.min = 0;
        os05l10_attr.again.apply_delay = 2;

        os05l10_attr.integration_time.value = 0x600;
        os05l10_attr.integration_time.max = 1750 - 102 - 5;
        os05l10_attr.integration_time.min = 2;
        os05l10_attr.integration_time.apply_delay = 2;

        os05l10_attr.total_width = 800;
        os05l10_attr.total_height = 1750;

        os05l10_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
        os05l10_attr.hcg.base_gain = ;
        os05l10_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        os05l10_attr.again_short.value = 65536;
        os05l10_attr.again_short.max = 259142;
        os05l10_attr.again_short.min = 0;
        os05l10_attr.again_short.apply_delay = 2;

        os05l10_attr.integration_time_short.value = 0;
        os05l10_attr.integration_time_short.max = 102;
        os05l10_attr.integration_time_short.min = 2;
        os05l10_attr.integration_time_short.apply_delay = 2;

        os05l10_attr.wdr_cache = wdr_line * os05l10_attr.total_width;

#ifdef SENSOR_HCG
        os05l10_attr.hcg_short.base_gain = ;
        os05l10_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(os05l10_attr.mipi)), (void *)(&os05l10_mipi_wdr), sizeof(os05l10_attr.mipi));
        break;
    default:
        ISP_ERROR("Have no this Setting Source!!!\n");
        break;
    }

    return ret;
}

static int os05l10_attr_check(struct tx_isp_subdev *sd)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct clk *sclka;
    int ret = ISP_SUCCESS;

    os05l10_setting_select(sd, info->default_boot);

    switch (info->video_interface) {
    case TISP_SENSOR_VI_MIPI_CSI0:
        os05l10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        os05l10_attr.mipi.index = 0;
        break;
    case TISP_SENSOR_VI_MIPI_CSI1:
        os05l10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        os05l10_attr.mipi.index = 1;
        break;
    case TISP_SENSOR_VI_DVP:
        os05l10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

    ret = os05l10_clk_set(sd, sclka, SENSOR_MCLK);
    if (ret) {
        ISP_ERROR("MCLK configuration failed!!!\n");
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
    os05l10_attr_set(sd, wsize);
    sensor->priv = wsize;

    return 0;

err_get_mclk:
    return -1;
}

static int os05l10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
    unsigned char v;
    int ret;

    ret = os05l10_read(sd, 0x03, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_H)
        return -ENODEV;
    *ident = v;

    ret = os05l10_read(sd, 0x02, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_M)
        return -ENODEV;
    *ident = (*ident << 8) | v;

    ret = os05l10_read(sd, 0x01, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_L)
        return -ENODEV;
    *ident = (*ident << 8) | v;

    return 0;
}

static int os05l10_g_chip_ident(struct tx_isp_subdev *sd,
                                struct tx_isp_chip_ident *chip)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    unsigned int ident = 0;
    int ret = ISP_SUCCESS;

    os05l10_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
    if (info->rst_gpio != -1) {
        ret = private_gpio_request(info->rst_gpio, "os05l10_reset");
        if (!ret) {
            private_gpio_direction_output(info->rst_gpio, 1);
            private_msleep(5);
            private_gpio_direction_output(info->rst_gpio, 0);
            private_msleep(10);
            private_gpio_direction_output(info->rst_gpio, 1);
            private_msleep(10);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
        }
    }
    if (info->pwdn_gpio != -1) {
        ret = private_gpio_request(info->pwdn_gpio, "os05l10_pwdn");
        if (!ret) {
            private_gpio_direction_output(info->pwdn_gpio, 0);
            private_msleep(5);
            private_gpio_direction_output(info->pwdn_gpio, 1);
            private_msleep(5);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
        }
    }
    ret = os05l10_detect(sd, &ident);
    if (ret) {
        ISP_ERROR("chip found @ 0x%x (%s) is not an os05l10 chip.\n",
                  client->addr, client->adapter->name);
        return ret;
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    ISP_WARNING("===================================================\n");
    ISP_WARNING("OS05L10 version is %s\n", TVERSION);
    ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
    ISP_WARNING("Sensor name is %s\n", os05l10_attr.name);
    ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
    ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
    ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
    ISP_WARNING("===================================================\n");

    if (chip) {
        memcpy(chip->name, "os05l10", sizeof("os05l10"));
        chip->ident = ident;
        chip->revision = SENSOR_VERSION;
    }

    return 0;
}

static int os05l10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    return 0;
}

static int os05l10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
        ret = os05l10_attr_set(sd, wsize);
        sensor->video.state = TX_ISP_MODULE_DEINIT;
    }


    return ret;
}

static int os05l10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
    ret = os05l10_read(sd, reg->reg & 0xffff, &val);
    reg->val = val;
    reg->size = 2;

    return ret;
}

static int os05l10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
    int len = 0;

    len = strlen(sd->chip.name);
    if (len && strncmp(sd->chip.name, reg->name, len)) {
        return -EINVAL;
    }
    if (!private_capable(CAP_SYS_ADMIN))
        return -EPERM;
    os05l10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

    return 0;
}

static int os05l10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = ISP_SUCCESS;

    if (init->enable) {
        if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
            ret = os05l10_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
            if (ret)
                return ret;
            sensor->video.state = TX_ISP_MODULE_INIT;
        }
        if (sensor->video.state == TX_ISP_MODULE_INIT) {
            ret = os05l10_write_array(sd, os05l10_stream_on_mipi);
            sensor->video.state = TX_ISP_MODULE_RUNNING;
            pr_debug("os05l10 stream on\n");
        }

    } else {
        ret = os05l10_write_array(sd, os05l10_stream_off_mipi);
        sensor->video.state = TX_ISP_MODULE_INIT;
        pr_debug("os05l10 stream off\n");
    }

    return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int os05l10_set_expo(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;
    int it = value & 0xffff;
    int again = (value & 0xffff0000) >> 16;

    ret = os05l10_write(sd, 0xfd, 0x01);
    ret += os05l10_write(sd, 0x03, (unsigned char)((it >> 8) & 0xff));
    ret += os05l10_write(sd, 0x04, (unsigned char)(it & 0xff));

    ret += os05l10_write(sd, 0x24, (unsigned char)(again & 0xff));
    ret += os05l10_write(sd, 0x01, 0x01);

    return ret;
}
#else
static int os05l10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret = os05l10_write(sd, 0xfd, 0x01);
    ret += os05l10_write(sd, 0x03, (unsigned char)((value >> 8) & 0xff));
    ret += os05l10_write(sd, 0x04, (unsigned char)(value & 0xff));
    ret += os05l10_write(sd, 0x01, 0x01);

    return ret;
}

static int os05l10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret = os05l10_write(sd, 0xfd, 0x01);
    ret += os05l10_write(sd, 0x24, (unsigned char)(value & 0xff));
    ret += os05l10_write(sd, 0x01, 0x01);

    return ret;
}
#endif /* SENSOR_EXPO */

static int os05l10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int os05l10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int os05l10_set_mode(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    if (wsize) {
        ret = os05l10_attr_set(sd, wsize);
    }

    return ret;
}

static int os05l10_set_fps(struct tx_isp_subdev *sd, int fps)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    unsigned int sclk = 0;
    unsigned int hts = 0;
    unsigned int vts = 0;
    unsigned int vts_base = 0;
    unsigned int vb = 0;
    unsigned char val = 0;
    unsigned int newformat = 0; //the format is 24.8
    unsigned int max_fps = 0;
    int ret = ISP_SUCCESS;

    switch (info->default_boot) {
    case 0:
        sclk = 400 * 1750 * 30;  /**< HTS * VTS * FPS */
        max_fps = 30;
        vts_base = 1658;
        break;
    case 1:
        sclk = 800 * 1750 * 15;  /**< HTS * VTS * FPS */
        max_fps = 15;
        vts_base = 1654;
        break;
    default:
        ret = -1;
        ISP_ERROR("Now we do not support this framerate!!!\n");
    }

    /* max_fps = wsize->fps; */
    newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
    if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
        ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
        return -1;
    }

    ret += os05l10_write(sd, 0xfd, 0x01);
    val = 0;
    ret += os05l10_read(sd, 0x37, &val);
    hts = val << 8;
    val = 0;
    ret += os05l10_read(sd, 0x38, &val);
    hts |= val;
    if (0 != ret) {
        ISP_ERROR("err: os05l10 read err\n");
        return ret;
    }

    vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
    vb = vts - vts_base;

    ret += os05l10_write(sd, 0xfd, 0x01);
    os05l10_write(sd, 0x06, (unsigned char) (vb & 0xff));
    os05l10_write(sd, 0x05, (unsigned char) (vb >> 8));
    ret += os05l10_write(sd, 0x01, 0x01);

    if (0 != ret) {
        ISP_ERROR("err: os05l10_write err\n");
        return ret;
    }

    sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
    /* Linear mode */
    sensor->video.attr->total_height = vts;
    sensor->video.attr->integration_time.max = vts - 5;
#else
    /* WDR mode */
    switch (info->default_boot) {
    case 0:
        sensor->video.attr->total_height = vts;
        sensor->video.attr->integration_time.max = vts - 5;
        break;
    case 1:
        sensor->video.attr->total_height = vts;
        sensor->video.attr->integration_time.max = vts/17;
        sensor->video.attr->integration_time_short.max = vts - vts/17 - 5;
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

static int os05l10_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
    int ret = ISP_SUCCESS;
    unsigned char val = 0;

    par->drop_frame = 0;
    par->reset = 0;

    /* 2'b01:mirror,2'b10:filp */
    ret = os05l10_write(sd, 0xfd, 0x01);
    ret = os05l10_read(sd, 0x32, &val);
    switch(par->hvflip) {
    case TX_ISP_SENSOR_HVFLIP_NOMAL:
        val &= 0xFC;
        sensor->video.mbus.code = TISP_VI_FMT_SBGGR10_1X10;
        break;
    case TX_ISP_SENSOR_HVFLIP_HFLIP:
        val = ((val & 0xFC) | 0x01);
        sensor->video.mbus.code = TISP_VI_FMT_SGBRG10_1X10;
        break;
    case TX_ISP_SENSOR_HVFLIP_VFLIP:
        val = ((val & 0xFC) | 0x02);
        sensor->video.mbus.code = TISP_VI_FMT_SGRBG10_1X10;
        break;
    case TX_ISP_SENSOR_HVFLIP_HVFLIP:
        val |= 0x03;
        sensor->video.mbus.code = TISP_VI_FMT_SRGGB10_1X10;
        break;
    }
    ret = os05l10_write(sd, 0x32, val);
    ret += os05l10_write(sd, 0x01, 0x01);

    sensor->video.hvflip_mode = par->hvflip;
    sensor->video.mbus_change = 1;
    os05l10_attr_set(sd, wsize);
    return ret;
#else
    return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int os05l10_set_expo_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;
    int it = value & 0xffff;
    int again = (value & 0xffff0000) >> 16;

    ret = os05l10_write(sd, 0xfd, 0x01);
    ret += os05l10_write(sd, 0x4e, (unsigned char)((it >> 8) & 0xff));
    ret += os05l10_write(sd, 0x4f, (unsigned char)(it & 0xff));

    ret += os05l10_write(sd, 0x3b, (unsigned char)(again & 0xff));
    ret += os05l10_write(sd, 0x01, 0x01);

    return ret;
}
#else
static int os05l10_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret = os05l10_write(sd, 0xfd, 0x01);
    ret += os05l10_write(sd, 0x4e, (unsigned char)((value >> 8) & 0xff));
    ret += os05l10_write(sd, 0x4f, (unsigned char)(value & 0xff));
    ret += os05l10_write(sd, 0x01, 0x01);

    return ret;
}

static int os05l10_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret = os05l10_write(sd, 0xfd, 0x01);
    ret += os05l10_write(sd, 0x3b, (unsigned char)(value & 0xff));
    ret += os05l10_write(sd, 0x01, 0x01);

    return ret;
}
#endif /* SENSOR_EXPO */

static int os05l10_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    int ret = ISP_SUCCESS;

    ret = os05l10_write_array(sd, os05l10_stream_off_mipi);
    if (par->wdr_en == 1) {
        if (par->boot == -1) {
            info->default_boot = 1;
        }
        os05l10_setting_select(sd, 1);
        os05l10_attr_set(sd, wsize);
    } else if (par->wdr_en == 0) {
        if (par->boot == -1) {
            info->default_boot = 0;
        }
        os05l10_setting_select(sd, 0);
        os05l10_attr_set(sd, wsize);
    } else {
        ISP_ERROR("Can not support this data type!!!");
        return -1;
    }

    return 0;
}

static int os05l10_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    /* struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg; */
    struct tx_isp_sensor_register_info *info = &sensor->info;
    int ret = ISP_SUCCESS;

    private_gpio_direction_output(info->rst_gpio, 0);
    private_msleep(1);
    private_gpio_direction_output(info->rst_gpio, 1);
    private_msleep(1);

    ret = os05l10_write_array(sd, wsize->regs);
    ret = os05l10_write_array(sd, os05l10_stream_on_mipi);

    return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int os05l10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
            ret = os05l10_set_expo(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME:
        if (arg)
            ret = os05l10_set_integration_time(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN:
        if (arg)
            ret = os05l10_set_analog_gain(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_DGAIN:
        if (arg)
            ret = os05l10_set_digital_gain(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
        if (arg)
            ret = os05l10_get_black_pedestal(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_RESIZE:
        if (arg)
            ret = os05l10_set_mode(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
        if (arg)
            ret = os05l10_write_array(sd, os05l10_stream_off_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
        if (arg)
            ret = os05l10_write_array(sd, os05l10_stream_on_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FPS:
        if (arg)
            ret = os05l10_set_fps(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_HVFLIP:
        if(arg)
            ret = os05l10_set_hvflip(sd, (void *)sensor_val->value);
        break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
    case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
        if (arg)
            ret = os05l10_set_expo_short(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
        if(arg)
            ret = os05l10_set_integration_time_short(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
        if(arg)
            ret = os05l10_set_analog_gain_short(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_WDR:
        if(arg)
            ret = os05l10_set_wdr(sd, (void *)sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_WDR_STOP:
        if(arg)
            ret = os05l10_set_wdr_stop(sd, (void *)sensor_val->value);
        break;
#endif /* SENSOR_WDR_2_FRAME */
    default:
        break;
    }

    return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops os05l10_core_ops = {
    .g_chip_ident = os05l10_g_chip_ident,
    .reset = os05l10_reset,
    .init = os05l10_init,
    .g_register = os05l10_g_register,
    .s_register = os05l10_s_register,
};

static struct tx_isp_subdev_video_ops os05l10_video_ops = {
    .s_stream = os05l10_s_stream,
};

static struct tx_isp_subdev_sensor_ops os05l10_sensor_ops = {
#ifndef SENSOR_TEST
    .ioctl	= os05l10_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops os05l10_ops = {
    .core = &os05l10_core_ops,
    .video = &os05l10_video_ops,
    .sensor = &os05l10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
    .name = "os05l10",
    .id = -1,
    .dev = {
        .dma_mask = &tx_isp_module_dma_mask,
        .coherent_dma_mask = 0xffffffff,
        .platform_data = NULL,
    },
    .num_resources = 0,
};

static int os05l10_probe(struct i2c_client *client,
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
    sensor->video.attr = &os05l10_attr;
    sensor->dev = &client->dev;
    tx_isp_subdev_init(&sensor_platform_device, sd, &os05l10_ops);
    tx_isp_set_subdevdata(sd, client);
    tx_isp_set_subdev_hostdata(sd, sensor);
    private_i2c_set_clientdata(client, sd);

    pr_debug("probe ok ------->os05l10\n");

    return 0;
}

static int os05l10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os05l10_id[] = {
    {"os05l10", 0},
    {}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, os05l10_id);
#endif	/* CONFIG_ZERATUL */

static struct i2c_driver os05l10_driver = {
    .driver = {
#ifdef CONFIG_ZERATUL
        .owner	= NULL,
#else
        .owner	= THIS_MODULE,
#endif	/* CONFIG_ZERATUL */
        .name	= "os05l10",
    },
    .probe			= os05l10_probe,
    .remove			= os05l10_remove,
    .id_table		= os05l10_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_os05l10(void) {
    return private_i2c_add_driver(&os05l10_driver);
}

static __exit void exit_os05l10(void) {
    private_i2c_del_driver(&os05l10_driver);
}

module_init(init_os05l10);
module_exit(exit_os05l10);
#endif	/* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_os05l10(void) {
    return private_i2c_add_driver(&os05l10_driver);
}

static void exit_first_os05l10(void) {
    private_i2c_del_driver(&os05l10_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
    .name = "os05l10",
    .i2c_addr = 0x3c,
    .width = 2880,
    .height = 1620,
#ifdef SENSOR_WDR_2_FRAME
    .wdr = 1,
#else
    .wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
    .init_sensor = init_first_os05l10,
    .exit_sensor = exit_first_os05l10
};
#endif	/* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for Sony os05l10 sensor");
MODULE_LICENSE("GPL");
