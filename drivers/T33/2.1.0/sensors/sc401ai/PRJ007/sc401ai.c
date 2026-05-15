/*
 * sc401ai.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           2560*1440       30        mipi_2lane    linear  10        30
 *
 * @I2C addr:0x30, 0x32
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

#define TVERSION "V20241105a"
#define SENSOR_VERSION  "H20250610a"

/* #define SENSOR_TEST */

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
/* #define SENSOR_HCG */
#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xcd)
#define SENSOR_CHIP_ID_L    (0x2e)
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

struct tx_isp_sensor_attribute sc401ai_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
    unsigned int value;
    unsigned int gain;
};

struct again_lut sc401ai_again_lut[] = {
    {0x340, 0},
    {0x341, 1500},
    {0x342, 2886},
    {0x343, 4342},
    {0x344, 5776},
    {0x345, 7101},
    {0x346, 8494},
    {0x347, 9781},
    {0x348, 11136},
    {0x349, 12471},
    {0x34a, 13706},
    {0x34b, 15005},
    {0x34c, 16287},
    {0x34d, 17474},
    {0x34e, 18723},
    {0x34f, 19879},
    {0x350, 21097},
    {0x351, 22300},
    {0x352, 23413},
    {0x353, 24587},
    {0x354, 25746},
    {0x355, 26820},
    {0x356, 27952},
    {0x357, 29002},
    {0x358, 30108},
    {0x359, 31202},
    {0x35a, 32216},
    {0x35b, 33286},
    {0x35c, 34344},
    {0x35d, 35325},
    {0x2340, 36361},
    {0x2341, 37829},
    {0x2342, 39276},
    {0x2343, 40700},
    {0x2344, 42104},
    {0x2345, 43487},
    {0x2346, 44850},
    {0x2347, 46193},
    {0x2348, 47518},
    {0x2349, 48825},
    {0x234a, 50113},
    {0x234b, 51330},
    {0x234c, 52585},
    {0x234d, 53824},
    {0x234e, 55046},
    {0x234f, 56253},
    {0x2350, 57445},
    {0x2351, 58622},
    {0x2352, 59785},
    {0x2353, 60933},
    {0x2354, 62068},
    {0x2355, 63189},
    {0x2356, 64297},
    {0x2357, 65393},
    {0x2358, 66475},
    {0x2359, 67546},
    {0x235a, 68604},
    {0x235b, 69651},
    {0x235c, 70686},
    {0x235d, 71710},
    {0x235e, 72723},
    {0x235f, 73726},
    {0x2360, 74718},
    {0x2361, 75657},
    {0x2362, 76629},
    {0x2363, 77591},
    {0x2364, 78543},
    {0x2365, 79486},
    {0x2366, 80419},
    {0x2367, 81344},
    {0x2368, 82259},
    {0x2369, 83166},
    {0x236a, 84064},
    {0x236b, 84953},
    {0x236c, 85835},
    {0x236d, 86708},
    {0x236e, 87573},
    {0x236f, 88430},
    {0x2370, 89280},
    {0x2371, 90122},
    {0x2372, 90956},
    {0x2373, 91784},
    {0x2374, 92604},
    {0x2375, 93417},
    {0x2376, 94188},
    {0x2377, 94988},
    {0x2378, 95781},
    {0x2379, 96567},
    {0x237a, 97347},
    {0x237b, 98120},
    {0x237c, 98888},
    {0x237d, 99649},
    {0x237e, 100404},
    {0x237f, 101153},
    {0x2740, 101896},
    {0x2741, 103364},
    {0x2742, 104811},
    {0x2743, 106235},
    {0x2744, 107639},
    {0x2745, 109022},
    {0x2746, 110355},
    {0x2747, 111699},
    {0x2748, 113024},
    {0x2749, 114331},
    {0x274a, 115620},
    {0x274b, 116892},
    {0x274c, 118147},
    {0x274d, 119385},
    {0x274e, 120608},
    {0x274f, 121814},
    {0x2750, 123006},
    {0x2751, 124157},
    {0x2752, 125320},
    {0x2753, 126468},
    {0x2754, 127603},
    {0x2755, 128724},
    {0x2756, 129832},
    {0x2757, 130928},
    {0x2758, 132010},
    {0x2759, 133081},
    {0x275a, 134139},
    {0x275b, 135163},
    {0x275c, 136199},
    {0x275d, 137223},
    {0x275e, 138236},
    {0x275f, 139239},
    {0x2760, 140231},
    {0x2761, 141213},
    {0x2762, 142185},
    {0x2763, 143146},
    {0x2764, 144098},
    {0x2765, 145041},
    {0x2766, 145954},
    {0x2767, 146879},
    {0x2768, 147794},
    {0x2769, 148701},
    {0x276a, 149599},
    {0x276b, 150488},
    {0x276c, 151370},
    {0x276d, 152243},
    {0x276e, 153108},
    {0x276f, 153965},
    {0x2770, 154815},
    {0x2771, 155639},
    {0x2772, 156473},
    {0x2773, 157301},
    {0x2774, 158121},
    {0x2775, 158934},
    {0x2776, 159741},
    {0x2777, 160540},
    {0x2778, 161333},
    {0x2779, 162119},
    {0x277a, 162899},
    {0x277b, 163655},
    {0x277c, 164423},
    {0x277d, 165184},
    {0x277e, 165939},
    {0x277f, 166688},
    {0x2f40, 167431},
    {0x2f41, 168899},
    {0x2f42, 170346},
    {0x2f43, 171755},
    {0x2f44, 173159},
    {0x2f45, 174542},
    {0x2f46, 175905},
    {0x2f47, 177249},
    {0x2f48, 178574},
    {0x2f49, 179866},
    {0x2f4a, 181155},
    {0x2f4b, 182427},
    {0x2f4c, 183682},
    {0x2f4d, 184920},
    {0x2f4e, 186129},
    {0x2f4f, 187336},
    {0x2f50, 188528},
    {0x2f51, 189705},
    {0x2f52, 190867},
    {0x2f53, 192003},
    {0x2f54, 193138},
    {0x2f55, 194259},
    {0x2f56, 195367},
    {0x2f57, 196463},
    {0x2f58, 197545},
    {0x2f59, 198604},
    {0x2f5a, 199663},
    {0x2f5b, 200710},
    {0x2f5c, 201745},
    {0x2f5d, 202769},
    {0x2f5e, 203771},
    {0x2f5f, 204774},
    {0x2f60, 205766},
    {0x2f61, 206748},
    {0x2f62, 207720},
    {0x2f63, 208671},
    {0x2f64, 209623},
    {0x2f65, 210566},
    {0x2f66, 211499},
    {0x2f67, 212424},
    {0x2f68, 213339},
    {0x2f69, 214236},
    {0x2f6a, 215134},
    {0x2f6b, 216023},
    {0x2f6c, 216905},
    {0x2f6d, 217778},
    {0x2f6e, 218633},
    {0x2f6f, 219491},
    {0x2f70, 220341},
    {0x2f71, 221183},
    {0x2f72, 222017},
    {0x2f73, 222836},
    {0x2f74, 223656},
    {0x2f75, 224469},
    {0x2f76, 225276},
    {0x2f77, 226075},
    {0x2f78, 226868},
    {0x2f79, 227646},
    {0x2f7a, 228425},
    {0x2f7b, 229199},
    {0x2f7c, 229966},
    {0x2f7d, 230727},
    {0x2f7e, 231474},
    {0x2f7f, 232223},
    {0x3f40, 232966},
    {0x3f41, 234434},
    {0x3f42, 235873},
    {0x3f43, 237298},
    {0x3f44, 238701},
    {0x3f45, 240077},
    {0x3f46, 241440},
    {0x3f47, 242777},
    {0x3f48, 244102},
    {0x3f49, 245408},
    {0x3f4a, 246690},
    {0x3f4b, 247962},
    {0x3f4c, 249217},
    {0x3f4d, 250449},
    {0x3f4e, 251671},
    {0x3f4f, 252871},
    {0x3f50, 254063},
    {0x3f51, 255240},
    {0x3f52, 256396},
    {0x3f53, 257545},
    {0x3f54, 258679},
    {0x3f55, 259794},
    {0x3f56, 260902},
    {0x3f57, 261992},
    {0x3f58, 263074},
    {0x3f59, 264145},
    {0x3f5a, 265198},
    {0x3f5b, 266245},
    {0x3f5c, 267280},
    {0x3f5d, 268299},
    {0x3f5e, 269312},
    {0x3f5f, 270309},
    {0x3f60, 271301},
    {0x3f61, 272283},
    {0x3f62, 273249},
    {0x3f63, 274211},
    {0x3f64, 275163},
    {0x3f65, 276101},
    {0x3f66, 277034},
    {0x3f67, 277954},
    {0x3f68, 278869},
    {0x3f69, 279775},
    {0x3f6a, 280669},
    {0x3f6b, 281558},
    {0x3f6c, 282440},
    {0x3f6d, 283308},
    {0x3f6e, 284173},
    {0x3f6f, 285026},
    {0x3f70, 285876},
    {0x3f71, 286718},
    {0x3f72, 287548},
    {0x3f73, 288375},
    {0x3f74, 289196},
    {0x3f75, 290004},
    {0x3f76, 290811},
    {0x3f77, 291606},
    {0x3f78, 292399},
    {0x3f79, 293185},
    {0x3f7a, 293960},
    {0x3f7b, 294734},
    {0x3f7c, 295501},
    {0x3f7d, 296258},
    {0x3f7e, 297013},
    {0x3f7f, 297758},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc401ai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = sc401ai_again_lut;
    while (lut->gain <= sc401ai_attr.again.max) {
        if (isp_gain == 0) {
            *sensor_again = lut[0].value;
            return 0;
        } else if (isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        } else {
            if ((lut->gain == sc401ai_attr.again.max) && (isp_gain >= lut->gain)) {
                *sensor_again = lut->value;
                return lut->gain;
            }
        }

        lut++;
    }

#else
    /* Non analog gain table */
    ...;
#endif  /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

    return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int sc401ai_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = sc401ai_again_lut;
    while(lut->gain <= sc401ai_attr.again_short.max) {
        if(isp_gain == 0) {
            *sensor_again = 0;
            return 0;
        }
        else if(isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        }
        else{
            if((lut->gain == sc401ai_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc401ai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
    return 0;
}

#ifdef SENSOR_HCG
unsigned int sc401ai_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc401ai_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc401ai_mipi_linear = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 630,
    .lans = 2,
    .image_twidth = 2560,
    .image_theight = 1440,
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

struct tx_isp_dvp_bus sc401ai_dvp = {
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

struct tx_isp_sensor_attribute sc401ai_attr = {
    .name = "sc401ai",
    .chip_id = 0xcd2e,
    .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
    .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
    .cbus_device = 0x30,
    .sensor_ctrl.alloc_again = sc401ai_alloc_again,
    .sensor_ctrl.alloc_dgain = sc401ai_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
    .sensor_ctrl.alloc_again_short = sc401ai_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

    .dgain = {0},
    .dgain_short = {0},
};

static struct regval_list sc401ai_init_regs_2560_1440_30fps_mipi[] = {
    {0x0103, 0x01},
    {0x0100, 0x00},
    {0x36e9, 0x80},
    {0x36f9, 0x80},
    {0x3018, 0x3a},
    {0x3019, 0x0c},
    {0x301c, 0x78},
    {0x301f, 0x08},
    {0x3208, 0x0a},
    {0x3209, 0x00},
    {0x320a, 0x05},
    {0x320b, 0xa0},
    {0x320e, 0x05},// 1500:30fps
    {0x320f, 0xdc},//vts:1800:25fps
    {0x3214, 0x11},
    {0x3215, 0x11},
    {0x3223, 0x80},
    {0x3250, 0x00},
    {0x3253, 0x08},
    {0x3274, 0x01},
    {0x3301, 0x20},
    {0x3302, 0x18},
    {0x3303, 0x10},
    {0x3304, 0x50},
    {0x3306, 0x38},
    {0x3308, 0x18},
    {0x3309, 0x60},
    {0x330b, 0xc0},
    {0x330d, 0x10},
    {0x330e, 0x18},
    {0x330f, 0x04},
    {0x3310, 0x02},
    {0x331c, 0x04},
    {0x331e, 0x41},
    {0x331f, 0x51},
    {0x3320, 0x09},
    {0x3333, 0x10},
    {0x334c, 0x08},
    {0x3356, 0x09},
    {0x3364, 0x17},
    {0x338e, 0xfd},
    {0x3390, 0x08},
    {0x3391, 0x18},
    {0x3392, 0x38},
    {0x3393, 0x20},
    {0x3394, 0x20},
    {0x3395, 0x20},
    {0x3396, 0x08},
    {0x3397, 0x18},
    {0x3398, 0x38},
    {0x3399, 0x20},
    {0x339a, 0x20},
    {0x339b, 0x20},
    {0x339c, 0x20},
    {0x33ac, 0x10},
    {0x33ae, 0x18},
    {0x33af, 0x19},
    {0x360f, 0x01},
    {0x3620, 0x08},
    {0x3637, 0x25},
    {0x363a, 0x12},/*auto logic enable*/
    {0x3670, 0x0a},
    {0x3671, 0x07},
    {0x3672, 0x57},
    {0x3673, 0x5e},
    {0x3674, 0x84},
    {0x3675, 0x88},
    {0x3676, 0x8a},
    {0x367a, 0x58},
    {0x367b, 0x78},
    {0x367c, 0x58},
    {0x367d, 0x78},
    {0x3690, 0x33},
    {0x3691, 0x43},
    {0x3692, 0x34},
    {0x369c, 0x40},
    {0x369d, 0x78},
    {0x36ea, 0x39},
    {0x36eb, 0x0d},
    {0x36ec, 0x1c},
    {0x36ed, 0x24},
    {0x36fa, 0x39},
    {0x36fb, 0x33},
    {0x36fc, 0x10},
    {0x36fd, 0x14},
    {0x3908, 0x41},
    {0x396c, 0x0e},
    {0x3e00, 0x00},//
    {0x3e01, 0xb6},//
    {0x3e02, 0x00},//
    {0x3e03, 0x0b},
    {0x3e08, 0x03},
    {0x3e09, 0x40},
    {0x3e1b, 0x2a},
    {0x4509, 0x30},
    {0x4819, 0x08},
    {0x481b, 0x05},
    {0x481d, 0x11},
    {0x481f, 0x04},
    {0x4821, 0x09},
    {0x4823, 0x04},
    {0x4825, 0x04},
    {0x4827, 0x04},
    {0x4829, 0x07},
    {0x57a8, 0xd0},
    {0x36e9, 0x23},
    {0x36f9, 0x23},
    {0x0100, 0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc401ai_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc401ai_win_sizes[] = {
    {
        .width          = 2560,
        .height         = 1440,
        .fps            = 30 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = sc401ai_init_regs_2560_1440_30fps_mipi,
    },
};

static struct tx_isp_sensor_win_setting *wsize = &sc401ai_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc401ai_stream_on_mipi[] = {
    {0x0100, 0x01},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc401ai_stream_off_mipi[] = {
    {0x0100, 0x00},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc401ai_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc401ai_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc401ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = sc401ai_read(sd, vals->reg_num, &val);
            /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif

static int sc401ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = sc401ai_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc401ai_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc401ai_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc401ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = sc401ai_read(sd, vals->reg_num, &val);
            if (ret < 0)
                return ret;
        }
        vals++;
    }
    return 0;
}
#endif

static int sc401ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = sc401ai_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc401ai_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc401ai_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc401ai_setting_select(struct tx_isp_subdev *sd, int deboot)
{
    int ret = ISP_SUCCESS;

    switch (deboot) {
    case 0:
        wsize = &sc401ai_win_sizes[0];
        sc401ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        sc401ai_attr.again.value = 65536;
        sc401ai_attr.again.max = 297758;
        sc401ai_attr.again.min = 0;
        sc401ai_attr.again.apply_delay = 2;

        sc401ai_attr.integration_time.value = 0xb60;
        sc401ai_attr.integration_time.max = 1500 - 4;
        sc401ai_attr.integration_time.min = 2;
        sc401ai_attr.integration_time.apply_delay = 2;

        sc401ai_attr.total_width = 1400;
        sc401ai_attr.total_height = 1500;

        sc401ai_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
        sc401ai_attr.hcg.base_gain = ;
        sc401ai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        sc401ai_attr.again_short.value = ;
        sc401ai_attr.again_short.max = ;
        sc401ai_attr.again_short.min = ;
        sc401ai_attr.again_short.apply_delay = ;

        sc401ai_attr.integration_time_short.value = ;
        sc401ai_attr.integration_time_short.max = ;
        sc401ai_attr.integration_time_short.min = ;
        sc401ai_attr.integration_time_short.apply_delay = ;

        sc401ai_attr.wdr_cache = wdr_line * sc401ai_attr.total_width;

#ifdef SENSOR_HCG
        sc401ai_attr.hcg_short.base_gain = ;
        sc401ai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(sc401ai_attr.mipi)), (void *)(&sc401ai_mipi_linear), sizeof(sc401ai_attr.mipi));
        break;
    default:
        ISP_ERROR("Have no this Setting Source!!!\n");
        break;
    }

    return ret;
}

static int sc401ai_attr_check(struct tx_isp_subdev *sd)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct clk *sclka;
    int ret = ISP_SUCCESS;

    sc401ai_setting_select(sd, info->default_boot);

    switch (info->video_interface) {
    case TISP_SENSOR_VI_MIPI_CSI0:
        sc401ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        sc401ai_attr.mipi.index = 0;
        break;
    case TISP_SENSOR_VI_MIPI_CSI1:
        sc401ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        sc401ai_attr.mipi.index = 1;
        break;
    case TISP_SENSOR_VI_DVP:
        sc401ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

    ret = sc401ai_clk_set(sd, sclka, SENSOR_MCLK);
    if (ret) {
        ISP_ERROR("MCLK configuration failed!!!\n");
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
    sc401ai_attr_set(sd, wsize);
    sensor->priv = wsize;

    return 0;

err_get_mclk:
    return -1;
}

static int sc401ai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
    unsigned char v;
    int ret;

    ret = sc401ai_read(sd, 0x3107, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_H)
        return -ENODEV;
    *ident = v;

    ret = sc401ai_read(sd, 0x3108, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_L)
        return -ENODEV;
    *ident = (*ident << 8) | v;

    return 0;
}

static int sc401ai_g_chip_ident(struct tx_isp_subdev *sd,
                                struct tx_isp_chip_ident *chip)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    unsigned int ident = 0;
    int ret = ISP_SUCCESS;

    sc401ai_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
    if (info->rst_gpio != -1) {
        ret = private_gpio_request(info->rst_gpio, "sc401ai_reset");
        if (!ret) {
            private_gpio_direction_output(info->rst_gpio, 1);
            private_msleep(5);
            private_gpio_direction_output(info->rst_gpio, 0);
            private_msleep(5);
            private_gpio_direction_output(info->rst_gpio, 1);
            private_msleep(5);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
        }
    }
    if (info->pwdn_gpio != -1) {
        ret = private_gpio_request(info->pwdn_gpio, "sc401ai_pwdn");
        if (!ret) {
            private_gpio_direction_output(info->pwdn_gpio, 0);
            private_msleep(5);
            private_gpio_direction_output(info->pwdn_gpio, 1);
            private_msleep(5);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
        }
    }
    ret = sc401ai_detect(sd, &ident);
    if (ret) {
        ISP_ERROR("chip found @ 0x%x (%s) is not an sc401ai chip.\n",
                  client->addr, client->adapter->name);
        return ret;
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    ISP_WARNING("===================================================\n");
    ISP_WARNING("Template version is %s\n", TVERSION);
    ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
    ISP_WARNING("Sensor name is %s\n", sc401ai_attr.name);
    ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
    ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
    ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
    ISP_WARNING("===================================================\n");

    if (chip) {
        memcpy(chip->name, "sc401ai", sizeof("sc401ai"));
        chip->ident = ident;
        chip->revision = SENSOR_VERSION;
    }

    return 0;
}

static int sc401ai_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    return 0;
}

static int sc401ai_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
        ret = sc401ai_attr_set(sd, wsize);
        sensor->video.state = TX_ISP_MODULE_DEINIT;
    }

    return ret;
}

static int sc401ai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
    ret = sc401ai_read(sd, reg->reg & 0xffff, &val);
    reg->val = val;
    reg->size = 2;

    return ret;
}

static int sc401ai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
    int len = 0;

    len = strlen(sd->chip.name);
    if (len && strncmp(sd->chip.name, reg->name, len)) {
        return -EINVAL;
    }
    if (!private_capable(CAP_SYS_ADMIN))
        return -EPERM;
    sc401ai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

    return 0;
}

static int sc401ai_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = ISP_SUCCESS;

    if (init->enable) {
        if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
            ret = sc401ai_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
            if (ret)
                return ret;
            sensor->video.state = TX_ISP_MODULE_INIT;
        }
        if (sensor->video.state == TX_ISP_MODULE_INIT) {
            ret = sc401ai_write_array(sd, sc401ai_stream_on_mipi);
            sensor->video.state = TX_ISP_MODULE_RUNNING;
            pr_debug("sc401ai stream on\n");
        }

    } else {
        ret = sc401ai_write_array(sd, sc401ai_stream_off_mipi);
        sensor->video.state = TX_ISP_MODULE_INIT;
        pr_debug("sc401ai stream off\n");
    }

    return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc401ai_set_expo(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;
    int it = value & 0xffff;
    int again = (value & 0xffff0000) >> 16;

    it = it << 1;
    ret += sc401ai_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
    ret += sc401ai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
    ret += sc401ai_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

    ret += sc401ai_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
    ret += sc401ai_write(sd, 0x3e09, (unsigned char)(again & 0xff));

    return ret;
}
#else
static int sc401ai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    value = value << 1;
    ret += sc401ai_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
    ret += sc401ai_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
    ret += sc401ai_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

    return ret;
}

static int sc401ai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret += sc401ai_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
    ret += sc401ai_write(sd, 0x3e09, (unsigned char)(value & 0xff));

    return ret;
}
#endif /* SENSOR_EXPO */

static int sc401ai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int sc401ai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int sc401ai_set_mode(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    if (wsize) {
        ret = sc401ai_attr_set(sd, wsize);
    }

    return ret;
}

static int sc401ai_set_fps(struct tx_isp_subdev *sd, int fps)
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
        sclk = 1400 * 1500 * 30;  /**< HTS * VTS * FPS */
        max_fps = 30;
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

    val = 0;
    ret += sc401ai_read(sd, 0x320c, &val);
    hts = val << 8;
    val = 0;
    ret += sc401ai_read(sd, 0x320d, &val);
    hts |= val;
    if (0 != ret) {
        ISP_ERROR("err: sc401ai read err\n");
        return ret;
    }

    vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

    sc401ai_write(sd, 0x320f, (unsigned char) (vts & 0xff));
    sc401ai_write(sd, 0x320e, (unsigned char) (vts >> 8));

    if (0 != ret) {
        ISP_ERROR("err: sc401ai_write err\n");
        return ret;
    }

    /* sensor->video.fps = fps; */
    sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
    /* Linear mode */
    sensor->video.attr->total_height = vts;
    sensor->video.attr->integration_time.max = vts - 4;
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

static int sc401ai_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
    int ret = ISP_SUCCESS;
    unsigned char val = 0;

    par->drop_frame = 0;
    par->reset = 0;

    /* 2'b01:mirror,2'b10:filp */
    ret = sc401ai_read(sd, 0x3221, &val);
    switch(par->hvflip) {
    case TX_ISP_SENSOR_HVFLIP_NOMAL:
        val &= 0x99;
        break;
    case TX_ISP_SENSOR_HVFLIP_HFLIP:
        val = ((val & 0x99) | 0x06);
        break;
    case TX_ISP_SENSOR_HVFLIP_VFLIP:
        val = ((val & 0x99) | 0x60);
        break;
    case TX_ISP_SENSOR_HVFLIP_HVFLIP:
        val |= 0x66;
        break;
    }
    ret += sc401ai_write(sd, 0x3221, val);

    sensor->video.hvflip_mode = par->hvflip;
    sc401ai_attr_set(sd, wsize);

    return ret;
#else
    return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc401ai_set_expo_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#else
static int sc401ai_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}

static int sc401ai_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#endif /* SENSOR_EXPO */

static int sc401ai_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    int ret = ISP_SUCCESS;

    ret = sc401ai_write_array(sd, sc401ai_stream_off_mipi);
    if (par->wdr_en == 1) {
        if (par->boot == -1) {
            info->default_boot = 1;
        }
        sc401ai_setting_select(sd, 1);
        sc401ai_attr_set(sd, wsize);
    } else if (par->wdr_en == 0) {
        if (par->boot == -1) {
            info->default_boot = 0;
        }
        sc401ai_setting_select(sd, 0);
        sc401ai_attr_set(sd, wsize);
    } else {
        ISP_ERROR("Can not support this data type!!!");
        return -1;
    }

    return 0;
}

static int sc401ai_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    struct tx_isp_sensor_register_info *info = &sensor->info;
    int ret = ISP_SUCCESS;

    private_gpio_direction_output(info->rst_gpio, 0);
    private_msleep(1);
    private_gpio_direction_output(info->rst_gpio, 1);
    private_msleep(1);

    ret = sc401ai_write_array(sd, wsize->regs);
    ret = sc401ai_write_array(sd, sc401ai_stream_on_mipi);

    return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc401ai_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;
	if(data->cmd == TX_SENSOR_PM_RESUME){
		ret = sc401ai_write_array(sd, sc401ai_stream_on_mipi);
		if(ret != 0){
			printk("%s streamon failed\n",sc401ai_attr.name);
			return ret;
		}
		printk("%s TX_SENSOR_PM_RESUME\n",sc401ai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
		ret = sc401ai_write_array(sd, sc401ai_stream_off_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_SUSPEND.\n",sc401ai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_PREPARE) {
		printk("%s TX_SENSOR_PM_PREPARE.\n",sc401ai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
		printk("%s TX_SENSOR_PM_COMPLETE.\n",sc401ai_attr.name);

	}else if(data->cmd == TX_SENSOR_PM_STREAMON) {
		ret = sc401ai_write_array(sd, sc401ai_stream_on_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMON.\n",sc401ai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
		ret = sc401ai_write_array(sd, sc401ai_stream_off_mipi);
		if (ret < 0) {
			printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMOFF.\n",sc401ai_attr.name);
	}else {
		printk("[%s]Don't Support this function.\n",__func__);
		return -EINVAL;
	}

	return ret;
}

static int sc401ai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
            ret = sc401ai_set_expo(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME:
        if (arg)
            ret = sc401ai_set_integration_time(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN:
        if (arg)
            ret = sc401ai_set_analog_gain(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_DGAIN:
        if (arg)
            ret = sc401ai_set_digital_gain(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
        if (arg)
            ret = sc401ai_get_black_pedestal(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_RESIZE:
        if (arg)
            ret = sc401ai_set_mode(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
        if (arg)
            ret = sc401ai_write_array(sd, sc401ai_stream_off_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
        if (arg)
            ret = sc401ai_write_array(sd, sc401ai_stream_on_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FPS:
        if (arg)
            ret = sc401ai_set_fps(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_HVFLIP:
        if(arg)
            ret = sc401ai_set_hvflip(sd, (void *)sensor_val->value);
        break;
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret = sc401ai_pm_s_stream_ctrl(sd, arg);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
    case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
        if (arg)
            ret = sc401ai_set_expo_short(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
        if(arg)
            ret = sc401ai_set_integration_time_short(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
        if(arg)
            ret = sc401ai_set_analog_gain_short(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_WDR:
        if(arg)
            ret = sc401ai_set_wdr(sd, (void *)sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_WDR_STOP:
        if(arg)
            ret = sc401ai_set_wdr_stop(sd, (void *)sensor_val->value);
        break;
#endif /* SENSOR_WDR_2_FRAME */
    default:
        break;
    }

    return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc401ai_core_ops = {
    .g_chip_ident = sc401ai_g_chip_ident,
    .reset = sc401ai_reset,
    .init = sc401ai_init,
    .g_register = sc401ai_g_register,
    .s_register = sc401ai_s_register,
};

static struct tx_isp_subdev_video_ops sc401ai_video_ops = {
    .s_stream = sc401ai_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc401ai_sensor_ops = {
#ifndef SENSOR_TEST
    .ioctl  = sc401ai_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc401ai_ops = {
    .core = &sc401ai_core_ops,
    .video = &sc401ai_video_ops,
    .sensor = &sc401ai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
    .name = "sc401ai",
    .id = -1,
    .dev = {
        .dma_mask = &tx_isp_module_dma_mask,
        .coherent_dma_mask = 0xffffffff,
        .platform_data = NULL,
    },
    .num_resources = 0,
};

static int sc401ai_probe(struct i2c_client *client,
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
    sensor->video.attr = &sc401ai_attr;
    sensor->dev = &client->dev;
    tx_isp_subdev_init(&sensor_platform_device, sd, &sc401ai_ops);
    tx_isp_set_subdevdata(sd, client);
    tx_isp_set_subdev_hostdata(sd, sensor);
    private_i2c_set_clientdata(client, sd);

    pr_debug("probe ok ------->sc401ai\n");

    return 0;
}

static int sc401ai_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc401ai_id[] = {
    {"sc401ai", 0},
    {}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc401ai_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc401ai_driver = {
    .driver = {
#ifdef CONFIG_ZERATUL
        .owner  = NULL,
#else
        .owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
        .name   = "sc401ai",
    },
    .probe          = sc401ai_probe,
    .remove         = sc401ai_remove,
    .id_table       = sc401ai_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc401ai(void) {
    return private_i2c_add_driver(&sc401ai_driver);
}

static __exit void exit_sc401ai(void) {
    private_i2c_del_driver(&sc401ai_driver);
}

module_init(init_sc401ai);
module_exit(exit_sc401ai);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc401ai(void) {
    return private_i2c_add_driver(&sc401ai_driver);
}

static void exit_first_sc401ai(void) {
    private_i2c_del_driver(&sc401ai_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
    .name = "sc401ai",
    .i2c_addr = 0x30,
    .width = 2560,
    .height = 1440,
#ifdef SENSOR_WDR_2_FRAME
    .wdr = 1,
#else
    .wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
    .init_sensor = init_first_sc401ai,
    .exit_sensor = exit_first_sc401ai
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for Sony sc401ai sensor");
MODULE_LICENSE("GPL");
