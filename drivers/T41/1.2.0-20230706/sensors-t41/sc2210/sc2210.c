/*
 * sc2210.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       30        mipi_2lane           linear
 *   1          1280*720         30        mipi_2lane           linear
 *   2          960*540           30        mipi_2lane           linear
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

#define SC2210_CHIP_ID_H        (0x22)
#define SC2210_CHIP_ID_L        (0x10)
#define SC2210_REG_END          0xffff
#define SC2210_REG_DELAY        0xfffe
#define SC2210_SUPPORT_25FPS_SCLK (81000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION  "H20230522a"

static int reset_gpio = GPIO_PA(18);
static int pwdn_gpio = -1;

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list {
        uint16_t reg_num;
        unsigned char value;
};
/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
        unsigned int value;
        unsigned int gain;
};

struct again_lut sc2210_again_lut[] = {
        {0x340, 0},
        {0x344, 5731},
        {0x348, 11135},
        {0x34c, 16247},
        {0x350, 21097},
        {0x354, 25710},
        {0x358, 30108},
        {0x35c, 34311},
        {0x360, 38335},
        {0x364, 42195},
        {0x368, 45903},
        {0x36c, 49472},
        {0x370, 52910},
        {0x374, 56228},
        {0x378, 56228},
        {0x37c, 62534},
        {0x740, 65536},
        {0x744, 71267},
        {0x748, 76671},
        {0x74c, 81783},
        {0x750, 86633},
        {0x754, 91246},
        {0x758, 95644},
        {0x75c, 99847},
        {0x760, 103871},
        {0x764, 107731},
        {0x768, 111439},
        {0x76c, 115008},
        {0x2343, 120119},
        {0x2347, 124969},
        {0x234b, 129582},
        {0x234f, 135402},
        {0x2353, 139543},
        {0x2357, 144798},
        {0x235b, 148555},
        {0x235f, 152169},
        {0x2363, 156782},
        {0x2367, 160100},
        {0x236b, 164349},
        {0x236f, 167416},
        {0x2373, 170387},
        {0x2377, 174207},
        {0x237b, 176975},
        {0x237f, 180544},
        {0x2743, 185655},
        {0x2747, 190505},
        {0x274b, 195865},
        {0x274f, 200938},
        {0x2753, 205752},
        {0x2757, 210334},
        {0x275b, 214091},
        {0x275f, 218294},
        {0x2763, 222318},
        {0x2767, 226178},
        {0x276b, 229885},
        {0x276f, 233454},
        {0x2773, 236409},
        {0x2777, 239743},
        {0x277b, 242964},
        {0x277f, 246080},
        {0x2f43, 251191},
        {0x2f47, 256434},
        {0x2f4b, 261773},
        {0x2f4f, 266826},
        {0x2f53, 271288},
        {0x2f57, 275870},
        {0x2f5b, 279933},
        {0x2f5f, 284123},
        {0x2f63, 288134},
        {0x2f67, 291714},
        {0x2f6b, 295421},
        {0x2f6f, 298990},
        {0x2f73, 302187},
        {0x2f77, 305513},
        {0x2f7b, 308500},
        {0x2f7f, 311616},
        {0x3f43, 316727},
        {0x3f47, 322166},
        {0x3f4b, 327309},
        {0x3f4f, 332362},
        {0x3f53, 336992},
        {0x3f57, 341406},
        {0x3f5b, 345622},
        {0x3f5f, 349805},
        {0x3f63, 353670},
        {0x3f67, 357384},
        {0x3f6b, 360957},
        {0x3f6f, 364526},
        {0x3f73, 367844},
        {0x3f77, 371049},
        {0x3f7b, 374149},
        {0x3f7f, 377261},
};

struct tx_isp_sensor_attribute sc2210_attr;

unsigned int sc2210_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        struct again_lut *lut = sc2210_again_lut;
        while(lut->gain <= sc2210_attr.max_again) {
                if(isp_gain == 0) {
                        *sensor_again = lut[0].value;
                        return lut[0].gain;
                }
                else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                }
                else{
                        if((lut->gain == sc2210_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
                                return lut->gain;
                        }
                }

                lut++;
        }

        return isp_gain;
}

unsigned int sc2210_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
        return 0;
}

struct tx_isp_mipi_bus mipi_binning = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 480,
        .lans = 2,
        .settle_time_apative_en = 0,
        .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
        .mipi_sc.hcrop_diff_en = 0,
        .mipi_sc.mipi_vcomp_en = 0,
        .mipi_sc.mipi_hcomp_en = 0,
        .mipi_sc.line_sync_mode = 0,
        .mipi_sc.work_start_flag = 0,
        .image_twidth = 960,
        .image_theight = 540,
        .mipi_sc.mipi_crop_start0x = 0,
        .mipi_sc.mipi_crop_start0y = 0,
        .mipi_sc.mipi_crop_start1x = 0,
        .mipi_sc.mipi_crop_start1y = 0,
        .mipi_sc.mipi_crop_start2x = 0,
        .mipi_sc.mipi_crop_start2y = 0,
        .mipi_sc.mipi_crop_start3x = 0,
        .mipi_sc.mipi_crop_start3y = 0,
        .mipi_sc.data_type_en = 0,
        .mipi_sc.data_type_value = RAW12,
        .mipi_sc.del_start = 0,
        .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
        .mipi_sc.sensor_fid_mode = 0,
        .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus mipi_crop_720p = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 480,
        .lans = 2,
        .settle_time_apative_en = 0,
        .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
        .mipi_sc.hcrop_diff_en = 0,
        .mipi_sc.mipi_vcomp_en = 0,
        .mipi_sc.mipi_hcomp_en = 0,
        .mipi_sc.line_sync_mode = 0,
        .mipi_sc.work_start_flag = 0,
        .image_twidth = 1920,
        .image_theight = 1080,
        .mipi_sc.mipi_crop_start0x = 320,
        .mipi_sc.mipi_crop_start0y = 180,
        .mipi_sc.mipi_crop_start1x = 0,
        .mipi_sc.mipi_crop_start1y = 0,
        .mipi_sc.mipi_crop_start2x = 0,
        .mipi_sc.mipi_crop_start2y = 0,
        .mipi_sc.mipi_crop_start3x = 0,
        .mipi_sc.mipi_crop_start3y = 0,
        .mipi_sc.data_type_en = 0,
        .mipi_sc.data_type_value = RAW12,
        .mipi_sc.del_start = 0,
        .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
        .mipi_sc.sensor_fid_mode = 0,
        .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sc2210_attr={
        .name = "sc2210",
        .chip_id = 0x2210,
        .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
        .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
        .cbus_device = 0x30,
        .dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
        .mipi = {
                .mode = SENSOR_MIPI_OTHER_MODE,
                .clk = 800,
                .lans = 2,
                .settle_time_apative_en = 1,
                .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
                .mipi_sc.data_type_value = RAW12,
                .mipi_sc.del_start = 0,
                .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
                .mipi_sc.sensor_fid_mode = 0,
                .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
        },
        .data_type = TX_SENSOR_DATA_TYPE_LINEAR,
        .max_again = 377261,
        .max_dgain = 0,
        .min_integration_time = 1,
        .min_integration_time_native = 1,
        .max_integration_time_native = 1440 - 4,
        .integration_time_limit = 1440 - 4,
        .total_width = 2250,
        .total_height = 1440,
        .max_integration_time = 1440 - 4,
        .integration_time_apply_delay = 2,
        .again_apply_delay = 2,
        .dgain_apply_delay = 0,
        .sensor_ctrl.alloc_again = sc2210_alloc_again,
        .sensor_ctrl.alloc_dgain = sc2210_alloc_dgain,
};

static struct regval_list sc2210_init_regs_960_540_25fps_mipi[] = {
        {0x0103,0x01},
        {0x0100,0x00},
        {0x36e9,0x80},
        {0x36f9,0x80},
        {0x3001,0x07},
        {0x3002,0xc0},
        {0x300a,0x2c},
        {0x300f,0x00},
        {0x3018,0x33},
        {0x3019,0x0c},
        {0x301f,0xb5},
        {0x3031,0x0c},
        {0x3033,0x20},
        {0x3038,0x22},
        {0x3106,0x01},
        {0x3201,0x04},
        {0x3203,0x04},
        {0x3204,0x07},
        {0x3205,0x8b},
        {0x3206,0x04},
        {0x3207,0x43},
        {0x320c,0x04},
        {0x320d,0x65},
        {0x320e,0x04},
        {0x320f,0xb0},
        {0x3211,0x04},
        {0x3213,0x04},
        {0x3231,0x02},
        {0x3253,0x04},
        {0x3301,0x0a},
        {0x3302,0x10},
        {0x3304,0x48},
        {0x3305,0x00},
        {0x3306,0x68},
        {0x3308,0x20},
        {0x3309,0x98},
        {0x330a,0x00},
        {0x330b,0xe8},
        {0x330e,0x68},
        {0x3314,0x92},
        {0x3000,0xc0},
        {0x331e,0x41},
        {0x331f,0x91},
        {0x334c,0x10},
        {0x335d,0x60},
        {0x335e,0x02},
        {0x335f,0x06},
        {0x3364,0x16},
        {0x3366,0x92},
        {0x3367,0x10},
        {0x3368,0x04},
        {0x3369,0x00},
        {0x336a,0x00},
        {0x336b,0x00},
        {0x336d,0x03},
        {0x337c,0x08},
        {0x337d,0x0e},
        {0x337f,0x33},
        {0x3390,0x10},
        {0x3391,0x30},
        {0x3392,0x40},
        {0x3393,0x0a},
        {0x3394,0x0a},
        {0x3395,0x1a},
        {0x3396,0x08},
        {0x3397,0x30},
        {0x3398,0x3f},
        {0x3399,0x50},
        {0x339a,0x50},
        {0x339b,0x50},
        {0x339c,0x50},
        {0x33a2,0x0a},
        {0x33b9,0x0e},
        {0x33e1,0x08},
        {0x33e2,0x18},
        {0x33e3,0x18},
        {0x33e4,0x18},
        {0x33e5,0x10},
        {0x33e6,0x06},
        {0x33e7,0x02},
        {0x33e8,0x18},
        {0x33e9,0x10},
        {0x33ea,0x0c},
        {0x33eb,0x10},
        {0x33ec,0x04},
        {0x33ed,0x02},
        {0x33ee,0xa0},
        {0x33ef,0x08},
        {0x33f4,0x18},
        {0x33f5,0x10},
        {0x33f6,0x0c},
        {0x33f7,0x10},
        {0x33f8,0x06},
        {0x33f9,0x02},
        {0x33fa,0x18},
        {0x33fb,0x10},
        {0x33fc,0x0c},
        {0x33fd,0x10},
        {0x33fe,0x04},
        {0x33ff,0x02},
        {0x360f,0x01},
        {0x3622,0xf7},
        {0x3625,0x0a},
        {0x3627,0x02},
        {0x3630,0xa2},
        {0x3631,0x00},
        {0x3632,0xd8},
        {0x3633,0x33},
        {0x3635,0x20},
        {0x3638,0x24},
        {0x363a,0x80},
        {0x363b,0x02},
        {0x363e,0x22},
        {0x3670,0x40},
        {0x3671,0xf7},
        {0x3672,0xf7},
        {0x3673,0x07},
        {0x367a,0x40},
        {0x367b,0x7f},
        {0x36b5,0x40},
        {0x36b6,0x7f},
        {0x36c0,0x80},
        {0x36c1,0x9f},
        {0x36c2,0x9f},
        {0x36cc,0x22},
        {0x36cd,0x23},
        {0x36ce,0x30},
        {0x36d0,0x20},
        {0x36d1,0x40},
        {0x36d2,0x7f},
        {0x36ea,0x34},
        {0x36eb,0x0d},
        {0x36ec,0x13},
        {0x36ed,0x24},
        {0x36fa,0xf7},
        {0x36fb,0x15},
        {0x36fc,0x00},
        {0x36fd,0x07},
        {0x3905,0xd8},
        {0x3907,0x01},
        {0x3908,0x11},
        {0x391b,0x83},
        {0x391d,0x0c},
        {0x391f,0x00},
        {0x3933,0x28},
        {0x3934,0xa6},
        {0x3940,0x70},
        {0x3942,0x08},
        {0x3943,0xbc},
        {0x3958,0x02},
        {0x3959,0x04},
        {0x3980,0x61},
        {0x3987,0x0b},
        {0x3990,0x00},
        {0x3991,0x00},
        {0x3992,0x00},
        {0x3993,0x00},
        {0x3994,0x00},
        {0x3995,0x00},
        {0x3996,0x00},
        {0x3997,0x00},
        {0x3998,0x00},
        {0x3999,0x00},
        {0x399a,0x00},
        {0x399b,0x00},
        {0x399c,0x00},
        {0x399d,0x00},
        {0x399e,0x00},
        {0x399f,0x00},
        {0x39a0,0x00},
        {0x39a1,0x00},
        {0x39a2,0x03},
        {0x39a3,0x30},
        {0x39a4,0x03},
        {0x39a5,0x60},
        {0x39a6,0x03},
        {0x39a7,0xa0},
        {0x39a8,0x03},
        {0x39a9,0xb0},
        {0x39aa,0x00},
        {0x39ab,0x00},
        {0x39ac,0x00},
        {0x39ad,0x20},
        {0x39ae,0x00},
        {0x39af,0x40},
        {0x39b0,0x00},
        {0x39b1,0x60},
        {0x39b2,0x00},
        {0x39b3,0x00},
        {0x39b4,0x08},
        {0x39b5,0x14},
        {0x39b6,0x20},
        {0x39b7,0x38},
        {0x39b8,0x38},
        {0x39b9,0x20},
        {0x39ba,0x14},
        {0x39bb,0x08},
        {0x39bc,0x08},
        {0x39bd,0x10},
        {0x39be,0x20},
        {0x39bf,0x30},
        {0x39c0,0x30},
        {0x39c1,0x20},
        {0x39c2,0x10},
        {0x39c3,0x08},
        {0x39c4,0x00},
        {0x39c5,0x80},
        {0x39c6,0x00},
        {0x39c7,0x80},
        {0x39c8,0x00},
        {0x39c9,0x00},
        {0x39ca,0x80},
        {0x39cb,0x00},
        {0x39cc,0x00},
        {0x39cd,0x00},
        {0x39ce,0x00},
        {0x39cf,0x00},
        {0x39d0,0x00},
        {0x39d1,0x00},
        {0x39e2,0x05},
        {0x39e3,0xeb},
        {0x39e4,0x07},
        {0x39e5,0xb6},
        {0x39e6,0x00},
        {0x39e7,0x3a},
        {0x39e8,0x3f},
        {0x39e9,0xb7},
        {0x39ea,0x02},
        {0x39eb,0x4f},
        {0x39ec,0x08},
        {0x39ed,0x00},
        {0x3e01,0x4a},
        {0x3e02,0xc0},
        {0x3e09,0x40},
        {0x3e14,0x31},
        {0x3e1b,0x3a},
        {0x3e26,0x40},
        {0x4401,0x1a},
        {0x4407,0xc0},
        {0x4418,0x34},
        {0x4500,0x18},
        {0x4501,0xb4},
        {0x4509,0x20},
        {0x4603,0x00},
        {0x4800,0x24},
        {0x4837,0x21},
        {0x5000,0x0e},
        {0x550f,0x20},
        {0x320a,0x02},
        {0x320b,0x1c},
        {0x3213,0x02},
        {0x3220,0x17},
        {0x3215,0x31},
        {0x3208,0x03},
        {0x3209,0xc0},
        {0x3211,0x04},
        {0x5000,0x46},
        {0x5901,0x04},
        {0x36e9,0x44},
        {0x36f9,0x53},
        {0x0100,0x01},
        {SC2210_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list sc2210_init_regs_1920_1080_25fps_mipi[] = {
        {0x0103, 0x01},
        {0x0100, 0x00},
        {0x36e9, 0x80},
        {0x36f9, 0x80},
        {0x3001, 0x07},
        {0x3002, 0xc0},
        {0x300a, 0x2c},
        {0x300f, 0x00},
        {0x3018, 0x33},
        {0x3019, 0x0c},
        {0x301f, 0xb5},
        {0x3031, 0x0c},
        {0x3033, 0x20},
        {0x3038, 0x22},
        {0x3106, 0x01},
        {0x3201, 0x04},
        {0x3203, 0x04},
        {0x3204, 0x07},
        {0x3205, 0x8b},
        {0x3206, 0x04},
        {0x3207, 0x43},
        {0x320c, 0x04},
        {0x320d, 0x65},
        {0x320e, 0x05},
        {0x320f, 0xa0},
        {0x3211, 0x04},
        {0x3213, 0x04},
        {0x3231, 0x02},
        {0x3253, 0x04},
        {0x3301, 0x0a},
        {0x3302, 0x10},
        {0x3304, 0x48},
        {0x3305, 0x00},
        {0x3306, 0x68},
        {0x3308, 0x20},
        {0x3309, 0x98},
        {0x330a, 0x00},
        {0x330b, 0xe8},
        {0x330e, 0x68},
        {0x3314, 0x92},
        {0x3000, 0xc0},
        {0x331e, 0x41},
        {0x331f, 0x91},
        {0x334c, 0x10},
        {0x335d, 0x60},
        {0x335e, 0x02},
        {0x335f, 0x06},
        {0x3364, 0x16},
        {0x3366, 0x92},
        {0x3367, 0x10},
        {0x3368, 0x04},
        {0x3369, 0x00},
        {0x336a, 0x00},
        {0x336b, 0x00},
        {0x336d, 0x03},
        {0x337c, 0x08},
        {0x337d, 0x0e},
        {0x337f, 0x33},
        {0x3390, 0x10},
        {0x3391, 0x30},
        {0x3392, 0x40},
        {0x3393, 0x0a},
        {0x3394, 0x0a},
        {0x3395, 0x1a},
        {0x3396, 0x08},
        {0x3397, 0x30},
        {0x3398, 0x3f},
        {0x3399, 0x50},
        {0x339a, 0x50},
        {0x339b, 0x50},
        {0x339c, 0x50},
        {0x33a2, 0x0a},
        {0x33b9, 0x0e},
        {0x33e1, 0x08},
        {0x33e2, 0x18},
        {0x33e3, 0x18},
        {0x33e4, 0x18},
        {0x33e5, 0x10},
        {0x33e6, 0x06},
        {0x33e7, 0x02},
        {0x33e8, 0x18},
        {0x33e9, 0x10},
        {0x33ea, 0x0c},
        {0x33eb, 0x10},
        {0x33ec, 0x04},
        {0x33ed, 0x02},
        {0x33ee, 0xa0},
        {0x33ef, 0x08},
        {0x33f4, 0x18},
        {0x33f5, 0x10},
        {0x33f6, 0x0c},
        {0x33f7, 0x10},
        {0x33f8, 0x06},
        {0x33f9, 0x02},
        {0x33fa, 0x18},
        {0x33fb, 0x10},
        {0x33fc, 0x0c},
        {0x33fd, 0x10},
        {0x33fe, 0x04},
        {0x33ff, 0x02},
        {0x360f, 0x01},
        {0x3622, 0xf7},
        {0x3625, 0x0a},
        {0x3627, 0x02},
        {0x3630, 0xa2},
        {0x3631, 0x00},
        {0x3632, 0xd8},
        {0x3633, 0x33},
        {0x3635, 0x20},
        {0x3638, 0x24},
        {0x363a, 0x80},
        {0x363b, 0x02},
        {0x363e, 0x22},
        {0x3670, 0x40},
        {0x3671, 0xf7},
        {0x3672, 0xf7},
        {0x3673, 0x07},
        {0x367a, 0x40},
        {0x367b, 0x7f},
        {0x36b5, 0x40},
        {0x36b6, 0x7f},
        {0x36c0, 0x80},
        {0x36c1, 0x9f},
        {0x36c2, 0x9f},
        {0x36cc, 0x22},
        {0x36cd, 0x23},
        {0x36ce, 0x30},
        {0x36d0, 0x20},
        {0x36d1, 0x40},
        {0x36d2, 0x7f},
        {0x36ea, 0x34},
        {0x36eb, 0x0d},
        {0x36ec, 0x13},
        {0x36ed, 0x24},
        {0x36fa, 0xf7},
        {0x36fb, 0x15},
        {0x36fc, 0x00},
        {0x36fd, 0x07},
        {0x3905, 0xd8},
        {0x3907, 0x01},
        {0x3908, 0x11},
        {0x391b, 0x83},
        {0x391d, 0x0c},
        {0x391f, 0x00},
        {0x3933, 0x28},
        {0x3934, 0xa6},
        {0x3940, 0x70},
        {0x3942, 0x08},
        {0x3943, 0xbc},
        {0x3958, 0x02},
        {0x3959, 0x04},
        {0x3980, 0x61},
        {0x3987, 0x0b},
        {0x3990, 0x00},
        {0x3991, 0x00},
        {0x3992, 0x00},
        {0x3993, 0x00},
        {0x3994, 0x00},
        {0x3995, 0x00},
        {0x3996, 0x00},
        {0x3997, 0x00},
        {0x3998, 0x00},
        {0x3999, 0x00},
        {0x399a, 0x00},
        {0x399b, 0x00},
        {0x399c, 0x00},
        {0x399d, 0x00},
        {0x399e, 0x00},
        {0x399f, 0x00},
        {0x39a0, 0x00},
        {0x39a1, 0x00},
        {0x39a2, 0x03},
        {0x39a3, 0x30},
        {0x39a4, 0x03},
        {0x39a5, 0x60},
        {0x39a6, 0x03},
        {0x39a7, 0xa0},
        {0x39a8, 0x03},
        {0x39a9, 0xb0},
        {0x39aa, 0x00},
        {0x39ab, 0x00},
        {0x39ac, 0x00},
        {0x39ad, 0x20},
        {0x39ae, 0x00},
        {0x39af, 0x40},
        {0x39b0, 0x00},
        {0x39b1, 0x60},
        {0x39b2, 0x00},
        {0x39b3, 0x00},
        {0x39b4, 0x08},
        {0x39b5, 0x14},
        {0x39b6, 0x20},
        {0x39b7, 0x38},
        {0x39b8, 0x38},
        {0x39b9, 0x20},
        {0x39ba, 0x14},
        {0x39bb, 0x08},
        {0x39bc, 0x08},
        {0x39bd, 0x10},
        {0x39be, 0x20},
        {0x39bf, 0x30},
        {0x39c0, 0x30},
        {0x39c1, 0x20},
        {0x39c2, 0x10},
        {0x39c3, 0x08},
        {0x39c4, 0x00},
        {0x39c5, 0x80},
        {0x39c6, 0x00},
        {0x39c7, 0x80},
        {0x39c8, 0x00},
        {0x39c9, 0x00},
        {0x39ca, 0x80},
        {0x39cb, 0x00},
        {0x39cc, 0x00},
        {0x39cd, 0x00},
        {0x39ce, 0x00},
        {0x39cf, 0x00},
        {0x39d0, 0x00},
        {0x39d1, 0x00},
        {0x39e2, 0x05},
        {0x39e3, 0xeb},
        {0x39e4, 0x07},
        {0x39e5, 0xb6},
        {0x39e6, 0x00},
        {0x39e7, 0x3a},
        {0x39e8, 0x3f},
        {0x39e9, 0xb7},
        {0x39ea, 0x02},
        {0x39eb, 0x4f},
        {0x39ec, 0x08},
        {0x39ed, 0x00},
        {0x3e01, 0x4a},
        {0x3e02, 0xc0},
        {0x3e09, 0x40},
        {0x3e14, 0x31},
        {0x3e1b, 0x3a},
        {0x3e26, 0x40},
        {0x4401, 0x1a},
        {0x4407, 0xc0},
        {0x4418, 0x34},
        {0x4500, 0x18},
        {0x4501, 0xb4},
        {0x4509, 0x20},
        {0x4603, 0x00},
        {0x4800, 0x24},
        {0x4837, 0x21},
        {0x5000, 0x0e},
        {0x550f, 0x20},
        {0x36e9, 0x44},
        {0x36f9, 0x53},
        {0x0100, 0x01},

        {SC2210_REG_END, 0x00}, /* END MARKER */
};

static struct tx_isp_sensor_win_setting sc2210_win_sizes[] = {
        {
                .width          = 1920,
                .height         = 1080,
                .fps              = 25 << 16 | 1,
                .mbus_code  = TISP_VI_FMT_SBGGR12_1X12,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = sc2210_init_regs_1920_1080_25fps_mipi,
        },
        {
                .width          = 1280,
                .height         = 720,
                .fps              = 25 << 16 | 1,
                .mbus_code  = TISP_VI_FMT_SBGGR12_1X12,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = sc2210_init_regs_1920_1080_25fps_mipi,
        },
        {
                .width          = 960,
                .height         = 540,
                .fps              = 25 << 16 | 1,
                .mbus_code  = TISP_VI_FMT_SBGGR12_1X12,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = sc2210_init_regs_960_540_25fps_mipi,
        },
};
struct tx_isp_sensor_win_setting *wsize = &sc2210_win_sizes[0];

static struct regval_list sc2210_stream_on_mipi[] = {
        {0x0100, 0x01},
        {SC2210_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list sc2210_stream_off_mipi[] = {
        {0x0100, 0x00},
        {SC2210_REG_END, 0x00}, /* END MARKER */
};

int sc2210_read(struct tx_isp_subdev *sd, uint16_t reg,
                unsigned char *value)
{
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
        int ret;
        ret = private_i2c_transfer(client->adapter, msg, 2);
        if (ret > 0)
                ret = 0;

        return ret;
}

int sc2210_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc2210_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != SC2210_REG_END) {
                if (vals->reg_num == SC2210_REG_DELAY) {
                        msleep(vals->value);
                } else {
                        ret = sc2210_read(sd, vals->reg_num, &val);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif

static int sc2210_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != SC2210_REG_END) {
                if (vals->reg_num == SC2210_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = sc2210_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}

static int sc2210_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        return 0;
}

static int sc2210_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
        int ret;
        unsigned char v;

        ret = sc2210_read(sd, 0x3107, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
        if (ret < 0)
                return ret;
        if (v != SC2210_CHIP_ID_H)
                return -ENODEV;
        *ident = v;

        ret = sc2210_read(sd, 0x3108, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
        if (ret < 0)
                return ret;
        if (v != SC2210_CHIP_ID_L)
                return -ENODEV;
        *ident = (*ident << 8) | v;

        return 0;
}

static int sc2210_set_expo(struct tx_isp_subdev *sd, int value)
{
        int ret = 0;
        int it = value & 0xffff;
        int again = (value & 0xffff0000) >> 16;

        ret = sc2210_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
        ret += sc2210_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
        ret += sc2210_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

        ret += sc2210_write(sd, 0x3e09, (unsigned char)(again & 0xff));
        ret += sc2210_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));

        return ret;
}

#if 0
static int sc2210_set_integration_time(struct tx_isp_subdev *sd, int value)
{
        int ret = 0;

        value *= 2;
        ret = sc2210_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
        ret += sc2210_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
        ret += sc2210_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
        if (ret < 0)
                return ret;

        return 0;
}

static int sc2210_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
        int ret = 0;

        ret += sc2210_write(sd, 0x3e09, (unsigned char)(value & 0xff));
        ret += sc2210_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
        if (ret < 0)
                return ret;

        return 0;
}
#endif

static int sc2210_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int sc2210_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

        sensor->video.vi_max_width = wsize->width;
        sensor->video.vi_max_height = wsize->height;
        sensor->video.mbus.width = wsize->width;
        sensor->video.mbus.height = wsize->height;
        sensor->video.mbus.code = wsize->mbus_code;
        sensor->video.mbus.field = TISP_FIELD_NONE;
        sensor->video.mbus.colorspace = wsize->colorspace;
        sensor->video.fps = wsize->fps;
        sensor->video.max_fps = wsize->fps;
        sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

        return 0;
}

static int sc2210_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = 0;

        if(!init->enable){
                sensor->video.state = TX_ISP_MODULE_DEINIT;
                return ISP_SUCCESS;
        } else {
                sensor_set_attr(sd, wsize);
                sensor->video.state = TX_ISP_MODULE_DEINIT;
                ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
                sensor->priv = wsize;
        }

        return 0;
}

static int sc2210_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = 0;

        if (init->enable) {
                if(sensor->video.state == TX_ISP_MODULE_DEINIT){
                        ret = sc2210_write_array(sd, wsize->regs);
                        if (ret)
                                return ret;
                        sensor->video.state = TX_ISP_MODULE_INIT;
                }
                if(sensor->video.state == TX_ISP_MODULE_INIT){
                        ret = sc2210_write_array(sd, sc2210_stream_on_mipi);
                        sensor->video.state = TX_ISP_MODULE_RUNNING;
                        pr_debug("sc2210 stream on\n");
                }

        } else {
                ret = sc2210_write_array(sd, sc2210_stream_off_mipi);
                sensor->video.state = TX_ISP_MODULE_INIT;
                pr_debug("sc2210 stream off\n");
        }

        return ret;
}

static int sc2210_set_fps(struct tx_isp_subdev *sd, int fps)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        unsigned int sclk = 0;
        unsigned int hts = 0;
        unsigned int vts = 0;
        unsigned char tmp = 0;
        unsigned int max_fps = 0;
        unsigned int newformat = 0; //the format is 24.8
        int ret = 0;

        switch(sensor->info.default_boot){
        case 0:
                sclk = SC2210_SUPPORT_25FPS_SCLK;
                max_fps = 30;
                break;
        case 1:
                sclk = SC2210_SUPPORT_25FPS_SCLK;
                max_fps = 30;
                break;
        case 2:
                sclk = SC2210_SUPPORT_25FPS_SCLK;
                max_fps = 30;
                break;
        default:
                ISP_ERROR("Now we do not support this framerate!!!\n");
        }
        newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
        if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
                ISP_ERROR("warn: fps(%d) no in range\n", fps);
                return ret;
        }

        ret = sc2210_read(sd, 0x320c, &tmp);
        hts = tmp;
        ret += sc2210_read(sd, 0x320d, &tmp);
        if (0 != ret) {
                ISP_ERROR("err: sc2210 read err\n");
                return ret;
        }
        hts = ((hts << 8) + tmp) << 1;
        vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

        // ret = sc2210_write(sd,0x3812,0x00);
        ret += sc2210_write(sd, 0x320f, (unsigned char)(vts & 0xff));
        ret += sc2210_write(sd, 0x320e, (unsigned char)(vts >> 8));
        // ret += sc2210_write(sd,0x3812,0x30);
        if (0 != ret) {
                ISP_ERROR("err: sc2210_write err\n");
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

static int sc2210_set_vflip(struct tx_isp_subdev *sd, int enable)
{
        int ret = 0;
        unsigned char val;

        ret += sc2210_read(sd, 0x3221, &val);
        if(enable & 0x02){
                val |= 0x60;
        }else{
                val &= 0x9f;
        }
        ret = sc2210_write(sd, 0x3221, val);
        if(ret < 0)
                return -1;

        return ret;
}
static int sc2210_set_mode(struct tx_isp_subdev *sd, int value)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if(wsize){
                sensor_set_attr(sd, wsize);
                ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        }

        return ret;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        unsigned long rate;
        int ret = 0;

        ISP_INFO("#####default boot is %d, video interface is %d, mclk is %d, reset is %d, pwdn is %d\n", info->default_boot, info->video_interface, info->mclk, info->rst_gpio, info->pwdn_gpio);
        switch(info->default_boot){
        case 0:
                wsize = &sc2210_win_sizes[0];
                break;
        case 1:
                wsize = &sc2210_win_sizes[1];
                memcpy((void*)(&(sc2210_attr.mipi)),(void*)(&mipi_crop_720p),sizeof(mipi_crop_720p));
                break;
        case 2:
                wsize = &sc2210_win_sizes[2];
                memcpy((void*)(&(sc2210_attr.mipi)),(void*)(&mipi_binning),sizeof(mipi_binning));
                break;
        default:
                ISP_ERROR("Have no this Setting Source!!!\n");
        }

        switch(info->video_interface){
        case TISP_SENSOR_VI_MIPI_CSI0:
                sc2210_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                sc2210_attr.mipi.index = 0;
                break;
        case TISP_SENSOR_VI_MIPI_CSI1:
                sc2210_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                sc2210_attr.mipi.index = 1;
                break;
        case TISP_SENSOR_VI_DVP:
                sc2210_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
                break;
        default:
                ISP_ERROR("Have no this interface!!!\n");
        }

        switch(info->mclk){
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

        rate = private_clk_get_rate(sensor->mclk);
        if (IS_ERR(sensor->mclk)) {
                ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
                goto err_get_mclk;
        }
        if (((rate / 1000) % 24000) != 0) {
                ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
                if (IS_ERR(sclka)) {
                        pr_err("get sclka failed\n");
                } else {
                        rate = private_clk_get_rate(sclka);
                        if (((rate / 1000) % 24000) != 0) {
                                private_clk_set_rate(sclka, 1200000000);
                        }
                }
        }
        private_clk_set_rate(sensor->mclk, 24000000);
        private_clk_prepare_enable(sensor->mclk);

        reset_gpio = info->rst_gpio;
        pwdn_gpio = info->pwdn_gpio;

        sensor_set_attr(sd, wsize);
        sensor->priv = wsize;
        sensor->video.max_fps = wsize->fps;
        sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

        return 0;

err_get_mclk:
        return -1;
}

static int sc2210_g_chip_ident(struct tx_isp_subdev *sd,
                               struct tx_isp_chip_ident *chip)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        unsigned int ident = 0;
        int ret = ISP_SUCCESS;

        sensor_attr_check(sd);
        if(reset_gpio != -1){
                ret = private_gpio_request(reset_gpio,"sc2210_reset");
                if(!ret){
                        private_gpio_direction_output(reset_gpio, 1);
                        private_msleep(5);
                        private_gpio_direction_output(reset_gpio, 0);
                        private_msleep(5);
                        private_gpio_direction_output(reset_gpio, 1);
                        private_msleep(5);
                }else{
                        ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
                }
        }
        if(pwdn_gpio != -1){
                ret = private_gpio_request(pwdn_gpio,"sc2210_pwdn");
                if(!ret){
                        private_gpio_direction_output(pwdn_gpio, 0);
                        private_msleep(5);
                        private_gpio_direction_output(pwdn_gpio, 1);
                        private_msleep(5);
                }else{
                        ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
                }
        }
        ret = sc2210_detect(sd, &ident);
        if (ret) {
                ISP_ERROR("chip found @ 0x%x (%s) is not an sc2210 chip.\n",
                          client->addr, client->adapter->name);
                return ret;
        }
        ISP_WARNING("sc2210 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
        ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
        if(chip){
                memcpy(chip->name, "sc2210", sizeof("sc2210"));
                chip->ident = ident;
                chip->revision = SENSOR_VERSION;
        }

        return 0;
}
#if 0
static int tgain = -1;
static int sc2210_set_logic(struct tx_isp_subdev *sd, int value)
{
        if(value != tgain){
                if(value <= 283241){
                        sc2210_write(sd, 0x5799, 0x00);
                } else if (value >= 321577) {
                        sc2210_write(sd, 0x5799, 0x07);
                }
                tgain = value;
        }

        return 0;
}
#endif

static int sc2210_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
        long ret = 0;
        struct tx_isp_sensor_value *sensor_val = arg;

        if(IS_ERR_OR_NULL(sd)){
                ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
                return -EINVAL;
        }
        switch(cmd){
        case TX_ISP_EVENT_SENSOR_LOGIC:
                //      if(arg)
                //              ret = sc2210_set_logic(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_EXPO:
                if(arg)
                        ret = sc2210_set_expo(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_INT_TIME:
                //if(arg)
                //      ret = sc2210_set_integration_time(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN:
                //if(arg)
                //      ret = sc2210_set_analog_gain(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_DGAIN:
                if(arg)
                        ret = sc2210_set_digital_gain(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
                if(arg)
                        ret = sc2210_get_black_pedestal(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_RESIZE:
                if(arg)
                        ret = sc2210_set_mode(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
                ret = sc2210_write_array(sd, sc2210_stream_off_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
                ret = sc2210_write_array(sd, sc2210_stream_on_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FPS:
                if(arg)
                        ret = sc2210_set_fps(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_VFLIP:
                if(arg)
                        ret = sc2210_set_vflip(sd, sensor_val->value);
                break;
        default:
                break;
        }

        return ret;
}

static int sc2210_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
        ret = sc2210_read(sd, reg->reg & 0xffff, &val);
        reg->val = val;
        reg->size = 2;

        return ret;
}

static int sc2210_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
        int len = 0;

        len = strlen(sd->chip.name);
        if(len && strncmp(sd->chip.name, reg->name, len)){
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        sc2210_write(sd, reg->reg & 0xffff, reg->val & 0xff);

        return 0;
}

static struct tx_isp_subdev_core_ops sc2210_core_ops = {
        .g_chip_ident = sc2210_g_chip_ident,
        .reset = sc2210_reset,
        .init = sc2210_init,
        .g_register = sc2210_g_register,
        .s_register = sc2210_s_register,
};

static struct tx_isp_subdev_video_ops sc2210_video_ops = {
        .s_stream = sc2210_s_stream,
};

static struct tx_isp_subdev_sensor_ops  sc2210_sensor_ops = {
        .ioctl  = sc2210_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc2210_ops = {
        .core = &sc2210_core_ops,
        .video = &sc2210_video_ops,
        .sensor = &sc2210_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
        .name = "sc2210",
        .id = -1,
        .dev = {
                .dma_mask = &tx_isp_module_dma_mask,
                .coherent_dma_mask = 0xffffffff,
                .platform_data = NULL,
        },
        .num_resources = 0,
};


static int sc2210_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
        struct tx_isp_subdev *sd;
        struct tx_isp_video_in *video;
        struct tx_isp_sensor *sensor;

        sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
        if(!sensor){
                ISP_ERROR("Failed to allocate sensor subdev.\n");
                return -ENOMEM;
        }
        memset(sensor, 0 ,sizeof(*sensor));

        sd = &sensor->sd;
        video = &sensor->video;
        sensor->dev = &client->dev;
        sc2210_attr.expo_fs = 1;
        sensor->video.shvflip = shvflip;
        sensor->video.attr = &sc2210_attr;
        sensor->video.vi_max_width = wsize->width;
        sensor->video.vi_max_height = wsize->height;
        sensor->video.mbus.width = wsize->width;
        sensor->video.mbus.height = wsize->height;
        sensor->video.mbus.code = wsize->mbus_code;
        sensor->video.mbus.field = TISP_FIELD_NONE;
        sensor->video.mbus.colorspace = wsize->colorspace;
        sensor->video.fps = wsize->fps;
        tx_isp_subdev_init(&sensor_platform_device, sd, &sc2210_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        ISP_WARNING("probe ok ------->sc2210\n");

        return 0;
}

static int sc2210_remove(struct i2c_client *client)
{
        struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
        struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

        if(reset_gpio != -1)
                private_gpio_free(reset_gpio);
        if(pwdn_gpio != -1)
                private_gpio_free(pwdn_gpio);

        private_clk_disable_unprepare(sensor->mclk);
        tx_isp_subdev_deinit(sd);

        kfree(sensor);

        return 0;
}

static const struct i2c_device_id sc2210_id[] = {
        { "sc2210", 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, sc2210_id);

static struct i2c_driver sc2210_driver = {
        .driver = {
                .owner  = THIS_MODULE,
                .name   = "sc2210",
        },
        .probe          = sc2210_probe,
        .remove         = sc2210_remove,
        .id_table       = sc2210_id,
};

static __init int init_sc2210(void)
{
        return private_i2c_add_driver(&sc2210_driver);
}

static __exit void exit_sc2210(void)
{
        private_i2c_del_driver(&sc2210_driver);
}

module_init(init_sc2210);
module_exit(exit_sc2210);

MODULE_DESCRIPTION("A low-level driver for Smartsens sc2210 sensors");
MODULE_LICENSE("GPL");
