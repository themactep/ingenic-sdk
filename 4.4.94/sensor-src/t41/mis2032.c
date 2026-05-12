/*
 * mis2032.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       25        mipi_2lane             linear
 *   1          1920*1080       25        mipi_2lane             wdr
 *   2          1920*1080       90        mipi_2lane             linear
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

#define TVERSION "V20231127a"
#define SENSOR_VERSION  "H20240102a"

//#define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */

#define SENSOR_CHIP_ID_H    (0x20)
#define SENSOR_CHIP_ID_L    (0x09)
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_MCLK 24000000

#ifdef SENSOR_WDR_2_FRAME
static int wdr_line = 1000;
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

struct tx_isp_sensor_attribute mis2032_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
        unsigned int value;
        unsigned int gain;
};

struct again_lut mis2032_again_lut[] = {
	{0x0, 0},
	{0x10, 1465},
	{0x20, 2998},
	{0x30, 4506},
	{0x40, 6077},
	{0x50, 7623},
	{0x60, 9228},
	{0x70, 10888},
	{0x80, 12601},
	{0x90, 14283},
	{0xa0, 16014},
	{0xb0, 17789},
	{0xc0, 19607},
	{0xd0, 21465},
	{0xe0, 23287},
	{0xf0, 25216},
	{0x100, 27175},
	{0x108, 28140},
	{0x110, 29164},
	{0x118, 30175},
	{0x120, 31177},
	{0x128, 32233},
	{0x130, 33277},
	{0x138, 34311},
	{0x140, 35396},
	{0x148, 36470},
	{0x150, 37593},
	{0x158, 38703},
	{0x160, 39801},
	{0x168, 40945},
	{0x170, 42076},
	{0x178, 43252},
	{0x180, 44413},
	{0x188, 45618},
	{0x190, 46808},
	{0x198, 48037},
	{0x1a0, 49252},
	{0x1a8, 50505},
	{0x1b0, 51794},
	{0x1b8, 53068},
	{0x1c0, 54375},
	{0x1c8, 55716},
	{0x1d0, 57039},
	{0x1d8, 58392},
	{0x1e0, 59776},
	{0x1e8, 61189},
	{0x1f0, 62581},
	{0x1f8, 64046},
	{0x200, 65536},
	{0x204, 66271},
	{0x208, 67001},
	{0x20c, 67769},
	{0x210, 68534},
	{0x214, 69290},
	{0x218, 70042},
	{0x21c, 70831},
	{0x220, 71613},
	{0x224, 72390},
	{0x228, 73201},
	{0x22c, 74007},
	{0x230, 74805},
	{0x234, 75639},
	{0x238, 76466},
	{0x23c, 77284},
	{0x240, 78137},
	{0x244, 78982},
	{0x248, 79858},
	{0x24c, 80688},
	{0x250, 81588},
	{0x254, 82441},
	{0x258, 83364},
	{0x25c, 84239},
	{0x260, 85143},
	{0x264, 86076},
	{0x268, 87001},
	{0x26c, 87916},
	{0x270, 88859},
	{0x274, 89792},
	{0x278, 90752},
	{0x27c, 91737},
	{0x280, 92711},
	{0x284, 93710},
	{0x288, 94700},
	{0x28c, 95711},
	{0x290, 96745},
	{0x294, 97769},
	{0x298, 98813},
	{0x29c, 99879},
	{0x2a0, 100932},
	{0x2a4, 102037},
	{0x2a8, 103129},
	{0x2ac, 104239},
	{0x2b0, 105337},
	{0x2b4, 106481},
	{0x2b8, 107612},
	{0x2bc, 108788},
	{0x2c0, 109949},
	{0x2c4, 111154},
	{0x2c8, 112344},
	{0x2cc, 113573},
	{0x2d0, 114815},
	{0x2d4, 116067},
	{0x2d8, 117330},
	{0x2dc, 118630},
	{0x2e0, 119911},
	{0x2e4, 121252},
	{0x2e8, 122575},
	{0x2ec, 123954},
	{0x2f0, 125337},
	{0x2f4, 126725},
	{0x2f8, 128141},
	{0x2fc, 129582},
	{0x300, 131072},
	{0x302, 131807},
	{0x304, 132559},
	{0x306, 133305},
	{0x308, 134070},
	{0x30a, 134826},
	{0x30c, 135599},
	{0x30e, 136367},
	{0x310, 137171},
	{0x312, 137947},
	{0x314, 138760},
	{0x316, 139564},
	{0x318, 140363},
	{0x31a, 141196},
	{0x31c, 142022},
	{0x31e, 142840},
	{0x320, 143693},
	{0x322, 144537},
	{0x324, 145394},
	{0x326, 146243},
	{0x328, 147124},
	{0x32a, 147996},
	{0x32c, 148900},
	{0x32e, 149793},
	{0x330, 150698},
	{0x332, 151612},
	{0x334, 152537},
	{0x336, 153452},
	{0x338, 154395},
	{0x33a, 155346},
	{0x33c, 156306},
	{0x33e, 157290},
	{0x340, 158265},
	{0x342, 159246},
	{0x344, 160252},
	{0x346, 161264},
	{0x348, 162281},
	{0x34a, 163321},
	{0x34c, 164366},
	{0x34e, 165415},
	{0x350, 166484},
	{0x352, 167573},
	{0x354, 168665},
	{0x356, 169775},
	{0x358, 170888},
	{0x35a, 172017},
	{0x35c, 173162},
	{0x35e, 174324},
	{0x360, 175500},
	{0x362, 176690},
	{0x364, 177894},
	{0x366, 179109},
	{0x368, 180351},
	{0x36a, 181603},
	{0x36c, 182866},
	{0x36e, 184166},
	{0x370, 185460},
	{0x372, 186788},
	{0x374, 188123},
	{0x376, 189490},
	{0x378, 190873},
	{0x37a, 192273},
	{0x37c, 193688},
	{0x37e, 195129},
	{0x380, 196608},
	{0x382, 198095},
	{0x384, 199606},
	{0x386, 201135},
	{0x388, 202707},
	{0x38a, 204296},
	{0x38c, 205909},
	{0x38e, 207558},
	{0x390, 209229},
	{0x392, 210930},
	{0x394, 212670},
	{0x396, 214436},
	{0x398, 216234},
	{0x39a, 218073},
	{0x39c, 219940},
	{0x39e, 221850},
	{0x3a0, 223801},
	{0x3a2, 225796},
	{0x3a4, 227826},
	{0x3a6, 229902},
	{0x3a8, 232028},
	{0x3aa, 234201},
	{0x3ac, 236432},
	{0x3ae, 238706},
	{0x3b0, 241043},
	{0x3b2, 243436},
	{0x3b4, 245894},
	{0x3b6, 248409},
	{0x3b8, 251003},
	{0x3ba, 253666},
	{0x3bc, 256409},
	{0x3be, 259230},
	{0x3c0, 262144},
	{0x3c1, 263631},
	{0x3c2, 265142},
	{0x3c3, 266678},
	{0x3c4, 268243},
	{0x3c5, 269832},
	{0x3c6, 271445},
	{0x3c7, 273094},
	{0x3c8, 274765},
	{0x3c9, 276471},
	{0x3ca, 278206},
	{0x3cb, 279972},
	{0x3cc, 281770},
	{0x3cd, 283609},
	{0x3ce, 285481},
	{0x3cf, 287391},
	{0x3d0, 289341},
	{0x3d1, 291332},
	{0x3d2, 293366},
	{0x3d3, 295442},
	{0x3d4, 297567},
	{0x3d5, 299741},
	{0x3d6, 301968},
	{0x3d7, 304245},
	{0x3d8, 306579},
	{0x3d9, 308972},
	{0x3da, 311430},
	{0x3db, 313949},
	{0x3dc, 316542},
	{0x3dd, 319205},
	{0x3de, 321945},
	{0x3df, 324769},
	{0x3e0, 327680},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int mis2032_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
        /* Analog gain table */
        struct again_lut *lut = mis2032_again_lut;
        while (lut->gain <= mis2032_attr.max_again) {
                if (isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if (isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else {
                        if ((lut->gain == mis2032_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
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
unsigned int mis2032_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
        /* Analog gain table */
        struct again_lut *lut = mis2032_again_lut;
        while(lut->gain <= mis2032_attr.max_again_short) {
                if(isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                }
                else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                }
                else{
                        if((lut->gain == mis2032_attr.max_again_short) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
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

unsigned int mis2032_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
        return 0;
}

struct tx_isp_mipi_bus mis2032_mipi_linear = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 200,
        .lans = 2,
        .image_twidth = 1920,
        .image_theight = 1080,
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

struct tx_isp_mipi_bus mis2032_mipi_dol = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 430,
        .lans = 2,
        .image_twidth = 1920,
        .image_theight = 1080,
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

struct tx_isp_mipi_bus mis2032_mipi_90fps_linear = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 560,
        .lans = 2,
        .image_twidth = 1920,
        .image_theight = 1080,
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

struct tx_isp_dvp_bus mis2032_dvp = {
        .gpio = DVP_PA_LOW_10BIT,
        .mode = SENSOR_DVP_HREF_MODE,
        .blanking = {
                .hblanking = 0,
                .vblanking = 0,
        },
        .polar = {
                .hsync_polar = 0,
                .vsync_polar = 0,
                .pclk_polar = 0, /**< reserved */
        },
        .dvp_hcomp_en = 0,
};

struct tx_isp_sensor_attribute mis2032_attr = {
        .name = "mis2032",
        .chip_id = 0x2009,
        .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
        .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
        .cbus_device = 0x30,
        .sensor_ctrl.alloc_again = mis2032_alloc_again,
        .sensor_ctrl.alloc_dgain = mis2032_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
        .sensor_ctrl.alloc_again_short = mis2032_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */
};

static struct regval_list mis2032_init_regs_1920_1080_25fps_mipi[] = {
	{0x3006, 0x01},
	{SENSOR_REG_DELAY, 50},
	{0x300b, 0x01},
	{0x3006, 0x02},
	{SENSOR_REG_DELAY, 50},
	{0x330c, 0x01},
	{0x3020, 0x00},
	{0x3021, 0x02},
	{0x3201, 0x46},
	{0x3200, 0x05},
	{0x3203, 0x98},
	{0x3202, 0x08},
	{0x3205, 0x04},
	{0x3204, 0x00},
	{0x3207, 0x43},
	{0x3206, 0x04},
	{0x3209, 0x08},
	{0x3208, 0x00},
	{0x320b, 0x87},
	{0x320a, 0x07},
	{0x3102, 0x00},
	{0x3105, 0x00},
	{0x3108, 0x00},
	{0x3007, 0x00},
	{0x300a, 0x01},
	{0x330c, 0x01},
	{0x3300, 0x7c},    //6E  27M
	{0x3301, 0x01},
	{0x3302, 0x02},
	{0x3303, 0x07},
	{0x3309, 0x01},
	{0x3307, 0x02},
	{0x330b, 0x0a},
	{0x3014, 0x00},
	{0x330f, 0x00},
	{0x310f, 0x00},
	{0x3986, 0x02},
	{0x3986, 0x02},
	{0x3900, 0x00},
	{0x3902, 0x11},
	{0x3901, 0x00},
	{0x3904, 0x44},
	{0x3903, 0x06},
	{0x3906, 0xff},
	{0x3905, 0x1f},
	{0x3908, 0xff},
	{0x3907, 0x1f},
	{0x390a, 0x42},
	{0x3909, 0x02},
	{0x390c, 0x19},
	{0x390b, 0x03},
	{0x390e, 0x30},
	{0x390d, 0x06},
	{0x3910, 0xff},
	{0x390f, 0x1f},
	{0x3911, 0x01},
	{0x3917, 0x00},
	{0x3916, 0x00},
	{0x3919, 0x90},
	{0x3918, 0x01},
	{0x3913, 0x11},
	{0x3912, 0x00},
	{0x3915, 0x52},
	{0x3914, 0x02},
	{0x391b, 0x00},
	{0x391a, 0x00},
	{0x391d, 0x41},
	{0x391c, 0x06},
	{0x391f, 0xff},
	{0x391e, 0x1f},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0x00},
	{0x3922, 0x00},
	{0x3925, 0x46},
	{0x3924, 0x02},
	{0x394c, 0x00},
	{0x394e, 0x74},
	{0x394d, 0x00},
	{0x3950, 0x84},
	{0x394f, 0x00},
	{0x3952, 0x63},
	{0x3951, 0x00},
	{0x3954, 0x71},
	{0x3953, 0x02},
	{0x3927, 0x00},
	{0x3926, 0x00},
	{0x3929, 0xc6},
	{0x3928, 0x00},
	{0x392b, 0x9d},
	{0x392a, 0x01},
	{0x392d, 0x31},
	{0x392c, 0x02},
	{0x392f, 0xcc},
	{0x392e, 0x03},
	{0x3931, 0x60},
	{0x3930, 0x06},
	{0x3933, 0x60},
	{0x3932, 0x06},
	{0x3935, 0x60},
	{0x3934, 0x06},
	{0x3937, 0x60},
	{0x3936, 0x06},
	{0x3939, 0x60},
	{0x3938, 0x06},
	{0x393b, 0x60},
	{0x393a, 0x06},
	{0x3991, 0x40},
	{0x3990, 0x00},
	{0x3993, 0x80},
	{0x3992, 0x06},
	{0x3995, 0xff},
	{0x3994, 0x1f},
	{0x3997, 0x00},
	{0x3996, 0x00},
	{0x393d, 0x74},
	{0x393c, 0x00},
	{0x393f, 0x9d},
	{0x393e, 0x01},
	{0x3941, 0x4a},
	{0x3940, 0x03},
	{0x3943, 0x9c},
	{0x3942, 0x03},
	{0x3945, 0x00},
	{0x3944, 0x00},
	{0x3947, 0xe7},
	{0x3946, 0x00},
	{0x3949, 0xe7},
	{0x3948, 0x00},
	{0x394b, 0x35},
	{0x394a, 0x06},
	{0x395a, 0x00},
	{0x3959, 0x00},
	{0x395c, 0x09},
	{0x395b, 0x00},
	{0x395e, 0x2f},
	{0x395d, 0x02},
	{0x3960, 0x39},
	{0x395f, 0x03},
	{0x3956, 0x09},
	{0x3955, 0x00},
	{0x3958, 0x35},
	{0x3957, 0x06},
	{0x3962, 0x00},
	{0x3961, 0x00},
	{0x3964, 0x84},
	{0x3963, 0x00},
	{0x3966, 0x00},
	{0x3965, 0x00},
	{0x3968, 0x74},
	{0x3967, 0x00},
	{0x3989, 0x00},
	{0x3988, 0x00},
	{0x398b, 0xa5},
	{0x398a, 0x00},
	{0x398d, 0x00},
	{0x398c, 0x00},
	{0x398f, 0x84},
	{0x398e, 0x00},
	{0x396a, 0x62},
	{0x3969, 0x06},
	{0x396d, 0x00},
	{0x396c, 0x01},
	{0x396f, 0x60},
	{0x396e, 0x00},
	{0x3971, 0x60},
	{0x3970, 0x00},
	{0x3973, 0x60},
	{0x3972, 0x00},
	{0x3975, 0x60},
	{0x3974, 0x00},
	{0x3977, 0x60},
	{0x3976, 0x00},
	{0x3979, 0xa0},
	{0x3978, 0x01},
	{0x397b, 0xa0},
	{0x397a, 0x01},
	{0x397d, 0xa0},
	{0x397c, 0x01},
	{0x397f, 0xa0},
	{0x397e, 0x01},
	{0x3981, 0xa0},
	{0x3980, 0x01},
	{0x3983, 0xa0},
	{0x3982, 0x01},
	{0x3985, 0xa0},
	{0x3984, 0x05},
	{0x3c42, 0x03},
	{0x3012, 0x2b},
	{0x3205, 0x08},
	{0x3204, 0x00},
	{0x310f, 0x00},
	{0x3600, 0x63},
	{0x3609, 0x10},
	{0x3630, 0x00},
	{0x3631, 0xFF},
	{0x3632, 0xFF},
	{0x364e, 0x63},
	{0x3657, 0x10},
	{0x367e, 0x00},
	{0x367f, 0xFF},
	{0x3680, 0xFF},
	{0x369c, 0x63},
	{0x36A5, 0x10},
	{0x36cc, 0x00},
	{0x36cd, 0xFF},
	{0x36ce, 0xFF},
	{0x3706, 0x00},
	{0x3707, 0x90},
	{0x3708, 0x00},
	{0x3709, 0x90},
	{0x370a, 0x00},
	{0x370b, 0x90},
	{0x210b, 0x00},
	{0x3021, 0x00},
	{0x3a00, 0x00},
	{0x3a04, 0x03},
	{0x3a05, 0x78},
	{0x3a0a, 0x3a},
	{0x3a0d, 0x17},
	{0x3a2a, 0x14},
	{0x3a2e, 0x10},
	{0x3a14, 0x00},
	{0x3a1c, 0x01},
	{0x3a36, 0x01},
	{0x3a07, 0x56},
	{0x3a35, 0x07},
	{0x3a30, 0x52},
	{0x3a31, 0x35},
	{0x3a32, 0x00},
	{0x3a19, 0x08},
	{0x3a1a, 0x08},
	{0x3a36, 0x01},
	{0x3006, 0x00},
	//{SENSOR_REG_DELAY,0x10},
        {SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list mis2032_init_regs_1920_1080_25fps_mipi_dol[] = {
	{0x300b, 0x01},
	{0x3006, 0x02},
	{SENSOR_REG_DELAY, 50},
	{0x330c, 0x01},
	{0x3b00, 0x03},
	{0x3b01, 0xff},
	{0x3a00, 0x80},
	{0x3300, 0x8d},   //7d   27M
	{0x3301, 0x01},
	{0x3302, 0x01},
	{0x3303, 0x03},
	{0x3309, 0x01},
	{0x3307, 0x01},
	{0x330b, 0x0a},
	{0x3021, 0x02},
	{0x3201, 0xdc},
	{0x3200, 0x05},
	{0x3203, 0x98},
	{0x3202, 0x08},
	{0x3205, 0x08},
	{0x3204, 0x00},
	{0x3207, 0x43},
	{0x3206, 0x04},
	{0x3209, 0x08},
	{0x3208, 0x00},
	{0x320b, 0x87},
	{0x320a, 0x07},
	{0x3102, 0x00},
	{0x3105, 0x00},
	{0x3108, 0x00},
	{0x3007, 0x00},
	{0x300a, 0x01},
	{0x3014, 0x00},
	{0x330f, 0x00},
	{0x310f, 0x01},
	{0x3986, 0x02},
	{0x3986, 0x02},
	{0x3900, 0x00},
	{0x3902, 0x13},
	{0x3901, 0x00},
	{0x3904, 0x4c},
	{0x3903, 0x09},
	{0x3906, 0xff},
	{0x3905, 0x1f},
	{0x3908, 0xff},
	{0x3907, 0x1f},
	{0x390a, 0xb3},
	{0x3909, 0x02},
	{0x390c, 0xf7},
	{0x390b, 0x05},
	{0x390e, 0x36},
	{0x390d, 0x09},
	{0x3910, 0xff},
	{0x390f, 0x1f},
	{0x3911, 0x01},
	{0x3917, 0x00},
	{0x3916, 0x00},
	{0x3919, 0xa1},
	{0x3918, 0x01},
	{0x3913, 0x13},
	{0x3912, 0x00},
	{0x3915, 0xc6},
	{0x3914, 0x02},
	{0x391b, 0x00},
	{0x391a, 0x00},
	{0x391d, 0x48},
	{0x391c, 0x09},
	{0x391f, 0xff},
	{0x391e, 0x1f},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0x00},
	{0x3922, 0x00},
	{0x3925, 0xb9},
	{0x3924, 0x02},
	{0x394c, 0x00},
	{0x394e, 0x82},
	{0x394d, 0x00},
	{0x3950, 0x95},
	{0x394f, 0x00},
	{0x3952, 0x70},
	{0x3951, 0x00},
	{0x3954, 0xea},
	{0x3953, 0x02},
	{0x3927, 0x00},
	{0x3926, 0x00},
	{0x3929, 0xdf},
	{0x3928, 0x00},
	{0x392b, 0xd1},
	{0x392a, 0x01},
	{0x392d, 0xa1},
	{0x392c, 0x02},
	{0x392f, 0x66},
	{0x392e, 0x06},
	{0x3931, 0x36},
	{0x3930, 0x09},
	{0x3933, 0x36},
	{0x3932, 0x09},
	{0x3935, 0x36},
	{0x3934, 0x09},
	{0x3937, 0x36},
	{0x3936, 0x09},
	{0x3939, 0x36},
	{0x3938, 0x09},
	{0x393b, 0x36},
	{0x393a, 0x09},
	{0x3991, 0x40},
	{0x3990, 0x00},
	{0x3993, 0x56},
	{0x3992, 0x09},
	{0x3995, 0xff},
	{0x3994, 0x1f},
	{0x3997, 0x00},
	{0x3996, 0x00},
	{0x393d, 0x82},
	{0x393c, 0x00},
	{0x393f, 0xd1},
	{0x393e, 0x01},
	{0x3941, 0xc6},
	{0x3940, 0x02},
	{0x3943, 0x66},
	{0x3942, 0x06},
	{0x3945, 0x00},
	{0x3944, 0x00},
	{0x3947, 0x04},
	{0x3946, 0x01},
	{0x3949, 0x04},
	{0x3948, 0x01},
	{0x394b, 0x3c},
	{0x394a, 0x09},
	{0x395a, 0x00},
	{0x3959, 0x00},
	{0x395c, 0x0a},
	{0x395b, 0x00},
	{0x395e, 0x9f},
	{0x395d, 0x02},
	{0x3960, 0xf7},
	{0x395f, 0x05},
	{0x3956, 0x0a},
	{0x3955, 0x00},
	{0x3958, 0x3c},
	{0x3957, 0x09},
	{0x3962, 0x00},
	{0x3961, 0x00},
	{0x3964, 0x95},
	{0x3963, 0x00},
	{0x3966, 0x00},
	{0x3965, 0x00},
	{0x3968, 0x82},
	{0x3967, 0x00},
	{0x3989, 0x00},
	{0x3988, 0x00},
	{0x398b, 0xba},
	{0x398a, 0x00},
	{0x398d, 0x00},
	{0x398c, 0x00},
	{0x398f, 0x95},
	{0x398e, 0x00},
	{0x396a, 0x6e},
	{0x3969, 0x09},
	{0x396d, 0x00},
	{0x396c, 0x01},
	{0x396f, 0x60},
	{0x396e, 0x00},
	{0x3971, 0x60},
	{0x3970, 0x00},
	{0x3973, 0x60},
	{0x3972, 0x00},
	{0x3975, 0x60},
	{0x3974, 0x00},
	{0x3977, 0x60},
	{0x3976, 0x00},
	{0x3979, 0xa0},
	{0x3978, 0x01},
	{0x397b, 0xa0},
	{0x397a, 0x01},
	{0x397d, 0xa0},
	{0x397c, 0x01},
	{0x397f, 0xa0},
	{0x397e, 0x01},
	{0x3981, 0xa0},
	{0x3980, 0x01},
	{0x3983, 0xa0},
	{0x3982, 0x01},
	{0x3985, 0xa0},
	{0x3984, 0x05},
	{0x3c42, 0x03},
	{0x3012, 0x2b},
	{0x3600, 0x63},
	{0x3609, 0x10},
	{0x3630, 0x00},
	{0x3631, 0xFF},
	{0x3632, 0xFF},
	{0x364e, 0x63},
	{0x3657, 0x10},
	{0x367e, 0x00},
	{0x367f, 0xFF},
	{0x3680, 0xFF},
	{0x369c, 0x63},
	{0x36A5, 0x10},
	{0x36cc, 0x00},
	{0x36cd, 0xFF},
	{0x36ce, 0xFF},
	{0x3a00, 0x00},
	{0x3706, 0x00},
	{0x3707, 0x90},
	{0x3708, 0x00},
	{0x3709, 0x90},
	{0x370a, 0x00},
	{0x370b, 0x90},
	{0x390c, 0x00},
	{0x210b, 0x00},
	{0x3021, 0x00},
	{0x3a04, 0x03},
	{0x3a05, 0x78},
	{0x3a0a, 0x3a},
	{0x3a0d, 0x17},
	{0x3a2a, 0x14},
	{0x3a2e, 0x10},
	{0x3a14, 0x00},
	{0x3a1c, 0x01},
	{0x3a36, 0x01},
	{0x3a07, 0x56},
	{0x3a35, 0x07},
	{0x3a30, 0x52},
	{0x3a31, 0x35},
	{0x3a32, 0x00},
	{0x3a19, 0x08},
	{0x3a1a, 0x08},
	{0x3a36, 0x01},
	{0x3006, 0x00},
        {SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list mis2032_init_regs_1920_1080_90fps_mipi[] = {
	{0x3006, 0x01},
	{SENSOR_REG_DELAY, 50},
	{0x3006, 0x00},
	{0x300b, 0x01},
	{0x3006, 0x02},
	{SENSOR_REG_DELAY, 50},
	{0x330c, 0x01},
	{0x3b00, 0x04},
	{0x3b01, 0xff},
	{0x330c, 0x01},
	{0x3a00, 0x80},
	{0x3300, 0xb9},   //a5  27M
	{0x3301, 0x03},
	{0x3302, 0x00},
	{0x3303, 0x00},
	{0x3309, 0x01},
	{0x3307, 0x00},
	{0x330b, 0x0a},
	{0x3021, 0x02},
	{0x3201, 0x65},
	{0x3200, 0x04},
	{0x3203, 0x98},
	{0x3202, 0x08},
	{0x3205, 0x08},
	{0x3204, 0x00},
	{0x3207, 0x43},
	{0x3206, 0x04},
	{0x3209, 0x08},
	{0x3208, 0x00},
	{0x320b, 0x87},
	{0x320a, 0x07},
	{0x3007, 0x00},
	{0x300a, 0x01},
	{0x3014, 0x00},
	{0x330f, 0x00},
	{0x3102, 0x00},
	{0x3105, 0x00},
	{0x3108, 0x00},
	{0x310f, 0x00},
	{0x3986, 0x02},
	{0x3986, 0x02},
	{0x3900, 0x00},
	{0x3902, 0x13},
	{0x3901, 0x00},
	{0x3904, 0x75},
	{0x3903, 0x06},
	{0x3906, 0xff},
	{0x3905, 0x1f},
	{0x3908, 0xff},
	{0x3907, 0x1f},
	{0x390a, 0xb1},
	{0x3909, 0x02},
	{0x390c, 0xf0},
	{0x390b, 0x02},
	{0x390e, 0x5f},
	{0x390d, 0x06},
	{0x3910, 0xff},
	{0x390f, 0x1f},
	{0x3911, 0x01},
	{0x3917, 0x00},
	{0x3916, 0x00},
	{0x3919, 0x9f},
	{0x3918, 0x01},
	{0x3913, 0x13},
	{0x3912, 0x00},
	{0x3915, 0xc4},
	{0x3914, 0x02},
	{0x391b, 0x00},
	{0x391a, 0x00},
	{0x391d, 0x72},
	{0x391c, 0x06},
	{0x391f, 0xff},
	{0x391e, 0x1f},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0x00},
	{0x3922, 0x00},
	{0x3925, 0xb7},
	{0x3924, 0x02},
	{0x394c, 0x00},
	{0x394e, 0x82},
	{0x394d, 0x00},
	{0x3950, 0x94},
	{0x394f, 0x00},
	{0x3952, 0x6f},
	{0x3951, 0x00},
	{0x3954, 0xe7},
	{0x3953, 0x02},
	{0x3927, 0x00},
	{0x3926, 0x00},
	{0x3929, 0xde},
	{0x3928, 0x00},
	{0x392b, 0xcf},
	{0x392a, 0x01},
	{0x392d, 0x9f},
	{0x392c, 0x02},
	{0x392f, 0x8f},
	{0x392e, 0x03},
	{0x3931, 0x5f},
	{0x3930, 0x06},
	{0x3933, 0x5f},
	{0x3932, 0x06},
	{0x3935, 0x5f},
	{0x3934, 0x06},
	{0x3937, 0x5f},
	{0x3936, 0x06},
	{0x3939, 0x5f},
	{0x3938, 0x06},
	{0x393b, 0x5f},
	{0x393a, 0x06},
	{0x3991, 0x40},
	{0x3990, 0x00},
	{0x3993, 0x80},
	{0x3992, 0x06},
	{0x3995, 0xff},
	{0x3994, 0x1f},
	{0x3997, 0x00},
	{0x3996, 0x00},
	{0x393d, 0x82},
	{0x393c, 0x00},
	{0x393f, 0xcf},
	{0x393e, 0x01},
	{0x3941, 0xc4},
	{0x3940, 0x02},
	{0x3943, 0x8f},
	{0x3942, 0x03},
	{0x3945, 0x00},
	{0x3944, 0x00},
	{0x3947, 0x03},
	{0x3946, 0x01},
	{0x3949, 0x03},
	{0x3948, 0x01},
	{0x394b, 0x65},
	{0x394a, 0x06},
	{0x395a, 0x00},
	{0x3959, 0x00},
	{0x395c, 0x0a},
	{0x395b, 0x00},
	{0x395e, 0x9d},
	{0x395d, 0x02},
	{0x3960, 0x6a},
	{0x395f, 0x03},
	{0x3956, 0x0a},
	{0x3955, 0x00},
	{0x3958, 0x65},
	{0x3957, 0x06},
	{0x3962, 0x00},
	{0x3961, 0x00},
	{0x3964, 0x94},
	{0x3963, 0x00},
	{0x3966, 0x00},
	{0x3965, 0x00},
	{0x3968, 0x82},
	{0x3967, 0x00},
	{0x3989, 0x00},
	{0x3988, 0x00},
	{0x398b, 0xb9},
	{0x398a, 0x00},
	{0x398d, 0x00},
	{0x398c, 0x00},
	{0x398f, 0x94},
	{0x398e, 0x00},
	{0x396a, 0x97},
	{0x3969, 0x06},
	{0x396d, 0x00},
	{0x396c, 0x01},
	{0x396f, 0x90},
	{0x396e, 0x00},
	{0x3971, 0x90},
	{0x3970, 0x00},
	{0x3973, 0x90},
	{0x3972, 0x00},
	{0x3975, 0x90},
	{0x3974, 0x00},
	{0x3977, 0x90},
	{0x3976, 0x00},
	{0x3979, 0xa0},
	{0x3978, 0x01},
	{0x397b, 0xa0},
	{0x397a, 0x01},
	{0x397d, 0xa0},
	{0x397c, 0x01},
	{0x397f, 0xa0},
	{0x397e, 0x01},
	{0x3981, 0xa0},
	{0x3980, 0x01},
	{0x3983, 0xa0},
	{0x3982, 0x01},
	{0x3985, 0xa0},
	{0x3984, 0x05},
	{0x3c18, 0x00},
	{0x3c01, 0x05},
	{0x3c0C, 0x58},
	{0x3c0E, 0x84},
	{0x3c18, 0x01},
	{0x3c42, 0x03},
	{0x3012, 0x2b},
	{0x3600, 0x63},
	{0x3609, 0x10},
	{0x3630, 0x00},
	{0x3631, 0xFF},
	{0x3632, 0xFF},
	{0x364e, 0x63},
	{0x3657, 0x10},
	{0x367e, 0x00},
	{0x367f, 0xFF},
	{0x3680, 0xFF},
	{0x369c, 0x63},
	{0x36A5, 0x10},
	{0x36cc, 0x00},
	{0x36cd, 0xFF},
	{0x36ce, 0xFF},
	{0x3a00, 0x00},
	{0x210b, 0x00},
	{0x3021, 0x00},
	{0x3706, 0x00},
	{0x3707, 0x90},
	{0x3708, 0x00},
	{0x3709, 0x90},
	{0x370a, 0x00},
	{0x370b, 0x90},
	{0x3a0d, 0x19},
	{0x3a14, 0x00},
	{0x3a1c, 0x01},
	{0x3a0e, 0x10},
	{0x3a0f, 0x10},
	{0x3a10, 0x10},
	{0x3a11, 0x10},
	{0x3a12, 0x10},
	{0x3a13, 0x20},
	{0x3a04, 0x03},
	{0x3a05, 0x78},
	{0x3a0a, 0x3a},
	{0x3a0d, 0x17},
	{0x3a2a, 0x14},
	{0x3a2e, 0x10},
	{0x3a07, 0x56},
	{0x3a35, 0x07},
	{0x3a30, 0x52},
	{0x3a31, 0x35},
	{0x3a32, 0x00},
	{0x3a19, 0x08},
	{0x3a1a, 0x08},
	{0x3a36, 0x01},
	{0x3006, 0x00},
        {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the mis2032_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting mis2032_win_sizes[] = {
        /* 1920*1080 [0] */
        {
                .width          = 1920,
                .height         = 1080,
                .fps            = 25 << 16 | 1,
                .mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = mis2032_init_regs_1920_1080_25fps_mipi,
        },
        /* 1920*1080 [1] */
        {
                .width          = 1920,
                .height         = 1080,
                .fps            = 25 << 16 | 1,
                .mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = mis2032_init_regs_1920_1080_25fps_mipi_dol,
        },
        /* 1920*1080 [2] */
        {
                .width          = 1920,
                .height         = 1080,
                .fps            = 90 << 16 | 1,
                .mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = mis2032_init_regs_1920_1080_90fps_mipi,
        },
};

static struct tx_isp_sensor_win_setting *wsize = &mis2032_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list mis2032_stream_on_mipi[] = {
        //{0x3006, 0x00},
        { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list mis2032_stream_off_mipi[] = {
        //{0x3006, 0x02},
        { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int mis2032_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int mis2032_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int mis2032_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != MIS2032_REG_END) {
                if (vals->reg_num == MIS2032_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = mis2032_read(sd, vals->reg_num, &val);
                        /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif

static int mis2032_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != MIS2032_REG_END) {
                if (vals->reg_num == MIS2032_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = mis2032_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int mis2032_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int mis2032_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int mis2032_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = mis2032_read(sd, vals->reg_num, &val);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }
        return 0;
}
#endif

static int mis2032_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = mis2032_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int mis2032_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        unsigned long rate;
        int ret = ISP_SUCCESS;

        rate = private_clk_get_rate(sensor->mclk);
        if(((rate / 1000) % mclk) != 0) {
                ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
                if (IS_ERR(sclka)) {
                        pr_err("get sclka failed\n");
                } else {
                        rate = private_clk_get_rate(sclka);
                        if(((rate / 1000) % mclk) != 0) {
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

        private_clk_set_rate(sensor->mclk, SENSOR_MCLK);
        private_clk_prepare_enable(sensor->mclk);

error:
        return ret;
}

static int mis2032_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

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

#ifdef SENSOR_MIR_FLIP
	sensor->video.shvflip = 1;
#endif

        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

        return ret;
}

static int mis2032_setting_select(struct tx_isp_subdev *sd, int deboot)
{
        int ret = ISP_SUCCESS;

        switch (deboot) {
        case 0:
                wsize = &mis2032_win_sizes[0];
                mis2032_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                mis2032_attr.max_dgain = 0;
                mis2032_attr.max_again = 327680;
                mis2032_attr.min_integration_time = 1;
                mis2032_attr.max_integration_time = 1348;
                mis2032_attr.total_width = 2200;
                mis2032_attr.total_height = 1350;
                mis2032_attr.integration_time_apply_delay = 2;
                mis2032_attr.again_apply_delay = 2;
                mis2032_attr.dgain_apply_delay = 0;
                mis2032_attr.integration_time_limit = mis2032_attr.max_integration_time;
                mis2032_attr.max_integration_time_native = mis2032_attr.max_integration_time;
                mis2032_attr.min_integration_time_native = mis2032_attr.min_integration_time;
                mis2032_attr.expo_fs = 1;
                memcpy((void *)(&(mis2032_attr.mipi)), (void *)(&mis2032_mipi_linear), sizeof(mis2032_attr.mipi));
                break;
        case 1:
                wsize = &mis2032_win_sizes[1];
                mis2032_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
                mis2032_attr.max_dgain = 0;
                mis2032_attr.max_again = 327680;
                mis2032_attr.min_integration_time = 1;
                mis2032_attr.max_integration_time = 2815;
                mis2032_attr.total_width = 2200;
                mis2032_attr.total_height = 1500 * 2;
                mis2032_attr.integration_time_apply_delay = 2;
                mis2032_attr.again_apply_delay = 2;
                mis2032_attr.dgain_apply_delay = 0;
                mis2032_attr.integration_time_limit = mis2032_attr.max_integration_time;
                mis2032_attr.max_integration_time_native = mis2032_attr.max_integration_time;
                mis2032_attr.min_integration_time_native = mis2032_attr.min_integration_time;
                mis2032_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
		mis2032_attr.wdr_cache = wdr_line * mis2032_attr.total_width * 2;
                mis2032_attr.max_again_short = 327680;
                mis2032_attr.min_integration_time_short = 1;
                mis2032_attr.max_integration_time_short = 175;
#endif /* SENSOR_WDR_2_FRAME */
                memcpy((void *)(&(mis2032_attr.mipi)), (void *)(&mis2032_mipi_dol), sizeof(mis2032_attr.mipi));
                break;
        case 2:
                wsize = &mis2032_win_sizes[2];
                mis2032_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                mis2032_attr.max_dgain = 0;
                mis2032_attr.max_again = 327680;
                mis2032_attr.min_integration_time = 1;
                mis2032_attr.max_integration_time = 1123;
                mis2032_attr.total_width = 2200;
                mis2032_attr.total_height = 1125;
                mis2032_attr.integration_time_apply_delay = 2;
                mis2032_attr.again_apply_delay = 2;
                mis2032_attr.dgain_apply_delay = 0;
                mis2032_attr.integration_time_limit = mis2032_attr.max_integration_time;
                mis2032_attr.max_integration_time_native = mis2032_attr.max_integration_time;
                mis2032_attr.min_integration_time_native = mis2032_attr.min_integration_time;
                mis2032_attr.expo_fs = 1;
                memcpy((void *)(&(mis2032_attr.mipi)), (void *)(&mis2032_mipi_90fps_linear), sizeof(mis2032_attr.mipi));
                break;
        default:
                ISP_ERROR("Have no this Setting Source!!!\n");
        }

        return ret;
}

static int mis2032_attr_check(struct tx_isp_subdev *sd)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct clk *sclka;
        int ret = ISP_SUCCESS;

        mis2032_setting_select(sd, info->default_boot);

        switch (info->video_interface) {
        case TISP_SENSOR_VI_MIPI_CSI0:
        case TISP_SENSOR_VI_MIPI_CSI1:
                mis2032_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                mis2032_attr.mipi.index = 0;
                break;
        case TISP_SENSOR_VI_DVP:
                mis2032_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
                break;
        default:
                ISP_ERROR("Have no this interface!!!\n");
        }

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

        ret = mis2032_clk_set(sd, sclka, SENSOR_MCLK);
        if (ret) {
                ISP_ERROR("MCLK configuration failed!!!\n");
        }

        mis2032_attr_set(sd, wsize);
        sensor->priv = wsize;

        return 0;

err_get_mclk:
        return -1;
}

static int mis2032_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
        unsigned char v;
        int ret;

        ret = mis2032_read(sd, 0x3000, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_H)
                return -ENODEV;
        *ident = v;

        ret = mis2032_read(sd, 0x3001, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_L)
                return -ENODEV;
        *ident = (*ident << 8) | v;

        return 0;
}

static int mis2032_g_chip_ident(struct tx_isp_subdev *sd,
                                 struct tx_isp_chip_ident *chip)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        unsigned int ident = 0;
        int ret = ISP_SUCCESS;

        mis2032_attr_check(sd);
        if (info->rst_gpio != -1) {
                ret = private_gpio_request(info->rst_gpio, "mis2032_reset");
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
                ret = private_gpio_request(info->pwdn_gpio, "mis2032_pwdn");
                if (!ret) {
                        private_gpio_direction_output(info->pwdn_gpio, 0);
                        private_msleep(5);
                        private_gpio_direction_output(info->pwdn_gpio, 1);
                        private_msleep(5);
                } else {
                        ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
                }
        }
        ret = mis2032_detect(sd, &ident);
        if (ret) {
                ISP_ERROR("chip found @ 0x%x (%s) is not an mis2032 chip.\n",
                          client->addr, client->adapter->name);
                return ret;
        }

        ISP_WARNING("===================================================\n");
        ISP_WARNING("Template version is %s\n", TVERSION);
        ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
        ISP_WARNING("Sensor name is %s\n", mis2032_attr.name);
        ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
        ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
        ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
        ISP_WARNING("===================================================\n");

        if (chip) {
                memcpy(chip->name, "mis2032", sizeof("mis2032"));
                chip->ident = ident;
                chip->revision = SENSOR_VERSION;
        }

        return 0;
}

static int mis2032_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        return 0;
}

static int mis2032_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        ret = mis2032_write_array(sd, wsize->regs);
        if (ret)
                return ret;
        sensor->video.state = TX_ISP_MODULE_INIT;
        private_msleep(280);
        *((u32 *)0xb3380000) = 0x5;

        if (!init->enable) {
                sensor->video.state = TX_ISP_MODULE_DEINIT;
                return ISP_SUCCESS;
        } else {
                ret = mis2032_attr_set(sd, wsize);
                sensor->video.state = TX_ISP_MODULE_DEINIT;
        }

        return ret;
}

static int mis2032_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
        ret = mis2032_read(sd, reg->reg & 0xffff, &val);
        reg->val = val;
        reg->size = 2;

        return ret;
}

static int mis2032_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
        int len = 0;

        len = strlen(sd->chip.name);
        if (len && strncmp(sd->chip.name, reg->name, len)) {
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        mis2032_write(sd, reg->reg & 0xffff, reg->val & 0xff);

        return 0;
}

static int mis2032_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if (init->enable) {
                if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
                        //ret = mis2032_write_array(sd, wsize->regs);
                        //if (ret)
                        //        return ret;
                        //sensor->video.state = TX_ISP_MODULE_INIT;
                }
                if (sensor->video.state == TX_ISP_MODULE_INIT) {
                        ret = mis2032_write_array(sd, mis2032_stream_on_mipi);
                        *((u32 *)0xb3380000) = 0x5;
                        sensor->video.state = TX_ISP_MODULE_RUNNING;
                        pr_debug("mis2032 stream on\n");
                }

        } else {
                ret = mis2032_write_array(sd, mis2032_stream_off_mipi);
                sensor->video.state = TX_ISP_MODULE_INIT;
                pr_debug("mis2032 stream off\n");
        }

        return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int mis2032_set_expo(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += mis2032_write(sd, 0x3100, (unsigned char)((it >> 8) & 0xff));
	ret += mis2032_write(sd, 0x3101, (unsigned char)(it & 0xff));

	ret += mis2032_write(sd, 0x3109, (unsigned char)((again >> 8) & 0x03));
	ret += mis2032_write(sd, 0x310a, (unsigned char)(again & 0xff));
	ret += mis2032_write(sd, 0x300c, 0x01);

        return ret;
}
#else
static int mis2032_set_integration_time(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

	ret += mis2032_write(sd, 0x3100, (unsigned char)((value >> 8) & 0xff));
	ret += mis2032_write(sd, 0x3101, (unsigned char)(value & 0xff));
	ret += mis2032_write(sd, 0x300c, 0x01);

        return ret;
}

static int mis2032_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

	ret += mis2032_write(sd, 0x310a, (unsigned char)((value >> 8) & 0x03));
	ret += mis2032_write(sd, 0x3509, (unsigned char)(value & 0xff));
	ret += mis2032_write(sd, 0x300c, 0x01);

        return ret;
}
#endif /* SENSOR_EXPO */

static int mis2032_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int mis2032_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int mis2032_set_mode(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

        if (wsize) {
                ret = mis2032_attr_set(sd, wsize);
        }

        return ret;
}

static int mis2032_set_fps(struct tx_isp_subdev *sd, int fps)
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
                sclk = 2200 * 1350 * 25;  /**< HTS * VTS * FPS */
                max_fps = 25;
                break;
        case 1:
                sclk = 2200 * 1500 * 25;  /**< HTS * VTS * FPS */
                max_fps = 25;
                break;
        case 2:
                sclk = 2200 * 1125 * 90;  /**< HTS * VTS * FPS */
                max_fps = 90;
                break;
        default:
                ret = -1;
                ISP_ERROR("Now we do not support this framerate!!!\n");
        }

        max_fps = wsize->fps;
        newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
        if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
                ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
                return -1;
        }

        val = 0;
        ret += mis2032_read(sd, 0x3202, &val);
        hts = val << 8;
        val = 0;
        ret += mis2032_read(sd, 0x3203, &val);
        hts = (hts | val);
        if (0 != ret) {
                ISP_ERROR("err: mis2032 read err\n");
                return ret;
        }

        vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

        ret += mis2032_write(sd, 0x3201, (unsigned char) (vts & 0xff));
        ret += mis2032_write(sd, 0x3200, (unsigned char) (vts >> 8));
	ret += mis2032_write(sd, 0x300c, 0x01);

	*((u32 *)0xb3380000) = 0x5;

        if (0 != ret) {
                ISP_ERROR("err: mis2032_write err\n");
                return ret;
        }

        sensor->video.fps = fps;

        if(info->default_boot == 1){
                sensor->video.attr->total_height = (vts << 1);
                sensor->video.attr->max_integration_time = (((vts << 1)/17) << 4);
                sensor->video.attr->integration_time_limit = (((vts << 1)/17) << 4);
                sensor->video.attr->max_integration_time_native = (((vts << 1)/17) << 4);
                sensor->video.attr->max_integration_time_short = ((vts << 1)/17);
        } else {
                sensor->video.attr->total_height = vts;
                sensor->video.attr->max_integration_time = vts - 2;
                sensor->video.attr->integration_time_limit = vts - 2;
                sensor->video.attr->max_integration_time_native = vts - 2;
        }

        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        if (ret) {
                ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
        }

        return ret;
}

#ifdef SENSOR_MIR_FLIP
static int mis2032_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = ISP_SUCCESS;

        printk("[%s,%d] -> flip = %d\n", __func__, __LINE__, enable);
	switch(enable) {
	case 0:
                mis2032_write(sd, 0x3007, 0x00);
                mis2032_write(sd, 0x3205, 0x08);
                mis2032_write(sd, 0x3207, 0x43);
                mis2032_write(sd, 0x3209, 0x08);
                mis2032_write(sd, 0x320b, 0x87);
		break;
	case 1:
                mis2032_write(sd, 0x3007, 0x01);
                mis2032_write(sd, 0x3205, 0x08);
                mis2032_write(sd, 0x3207, 0x43);
                mis2032_write(sd, 0x3209, 0x09);
                mis2032_write(sd, 0x320b, 0x88);
		break;
	case 2:
                mis2032_write(sd, 0x3007, 0x02);
                mis2032_write(sd, 0x3205, 0x09);
                mis2032_write(sd, 0x3207, 0x44);
                mis2032_write(sd, 0x3209, 0x08);
                mis2032_write(sd, 0x320b, 0x87);
		break;
	case 3:
                mis2032_write(sd, 0x3007, 0x03);
                mis2032_write(sd, 0x3205, 0x08);
                mis2032_write(sd, 0x3207, 0x44);
                mis2032_write(sd, 0x3209, 0x09);
                mis2032_write(sd, 0x320b, 0x88);
		break;
	}
	*((u32 *)0xb3380000) = 0x5;

	return ret;
}
#endif /* SENSOR_MIR_FLIP */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int mis2032_set_expo_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += mis2032_write(sd, 0x3103, (unsigned char)((it >> 8) & 0xff));
	ret += mis2032_write(sd, 0x3104, (unsigned char)(it & 0xff));

	ret += mis2032_write(sd, 0x310b, (unsigned char)((again >> 8) & 0x03));
	ret += mis2032_write(sd, 0x310c, (unsigned char)(again & 0xff));
	ret += mis2032_write(sd, 0x300c, 0x01);

        return ret;
}
#else
static int mis2032_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}

static int mis2032_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}
#endif /* SENSOR_EXPO */

static int mis2032_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
        //struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
        int ret = ISP_SUCCESS;

        ret = mis2032_write_array(sd, mis2032_stream_off_mipi);
        if (wdr_en == 1) {
                mis2032_setting_select(sd, 1);
                mis2032_attr_set(sd, wsize);
        } else if (wdr_en == 0) {
                mis2032_setting_select(sd, 0);
                mis2032_attr_set(sd, wsize);
        } else {
                ISP_ERROR("Can not support this data type!!!");
                return -1;
        }

        return 0;
}

static int mis2032_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        int ret = ISP_SUCCESS;

        private_gpio_direction_output(info->rst_gpio, 0);
        private_msleep(1);
        private_gpio_direction_output(info->rst_gpio, 1);
        private_msleep(1);

        ret = mis2032_write_array(sd, wsize->regs);
        ret = mis2032_write_array(sd, mis2032_stream_on_mipi);

        return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int mis2032_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
        long ret = 0;
        struct tx_isp_sensor_value *sensor_val = arg;
#ifdef SENSOR_WDR_2_FRAME
        struct tx_isp_initarg *init = arg;
#endif /* SENSOR_WDR_2_FRAME */

        if (IS_ERR_OR_NULL(sd)) {
                ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
                return -EINVAL;
        }

        switch (cmd) {
#ifdef SENSOR_EXPO
        case TX_ISP_EVENT_SENSOR_EXPO:
                if (arg)
                        ret = mis2032_set_expo(sd, sensor_val->value);
                break;
#else
        case TX_ISP_EVENT_SENSOR_INT_TIME:
                if (arg)
                        ret = mis2032_set_integration_time(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN:
                if (arg)
                        ret = mis2032_set_analog_gain(sd, sensor_val->value);
                break;
#endif /* SENSOR_EXPO */
        case TX_ISP_EVENT_SENSOR_DGAIN:
                if (arg)
                        ret = mis2032_set_digital_gain(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
                if (arg)
                        ret = mis2032_get_black_pedestal(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_RESIZE:
                if (arg)
                        ret = mis2032_set_mode(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
                if (arg)
                        ret = mis2032_write_array(sd, mis2032_stream_off_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
                if (arg)
                        ret = mis2032_write_array(sd, mis2032_stream_on_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FPS:
                if (arg)
                        ret = mis2032_set_fps(sd, sensor_val->value);
                break;
#ifdef SENSOR_MIR_FLIP
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = mis2032_set_vflip(sd, sensor_val->value);
		break;
#endif /* SENSOR_MIR_FLIP */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
        case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
                if (arg)
                        ret = mis2032_set_expo_short(sd, sensor_val->value);
                break;
#else
        case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
                if(arg)
                        ret = mis2032_set_integration_time_short(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
                if(arg)
                        ret = mis2032_set_analog_gain_short(sd, sensor_val->value);
                break;
#endif /* SENSOR_EXPO */
        case TX_ISP_EVENT_SENSOR_WDR:
                if(arg)
                        ret = mis2032_set_wdr(sd, init->enable);
                break;
        case TX_ISP_EVENT_SENSOR_WDR_STOP:
                if(arg)
                        ret = mis2032_set_wdr_stop(sd, init->enable);
                break;
#endif /* SENSOR_WDR_2_FRAME */
        default:
                break;
        }

        return 0;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops mis2032_core_ops = {
        .g_chip_ident = mis2032_g_chip_ident,
        .reset = mis2032_reset,
        .init = mis2032_init,
        .g_register = mis2032_g_register,
        .s_register = mis2032_s_register,
};

static struct tx_isp_subdev_video_ops mis2032_video_ops = {
        .s_stream = mis2032_s_stream,
};

static struct tx_isp_subdev_sensor_ops mis2032_sensor_ops = {
#ifndef SENSOR_TEST
        .ioctl  = mis2032_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops mis2032_ops = {
        .core = &mis2032_core_ops,
        .video = &mis2032_video_ops,
        .sensor = &mis2032_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
        .name = "mis2032",
        .id = -1,
        .dev = {
                .dma_mask = &tx_isp_module_dma_mask,
                .coherent_dma_mask = 0xffffffff,
                .platform_data = NULL,
        },
        .num_resources = 0,
};

static int mis2032_probe(struct i2c_client *client,
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

        sensor->video.attr = &mis2032_attr;
        sensor->dev = &client->dev;
        tx_isp_subdev_init(&sensor_platform_device, sd, &mis2032_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->mis2032\n");

        return 0;
}

static int mis2032_remove(struct i2c_client *client)
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

static const struct i2c_device_id mis2032_id[] = {
        {"mis2032", 0},
        {}
};
MODULE_DEVICE_TABLE(i2c, mis2032_id
                   );

static struct i2c_driver mis2032_driver = {
        .driver = {
                .owner  = THIS_MODULE,
                .name   = "mis2032",
        },
        .probe          = mis2032_probe,
        .remove         = mis2032_remove,
        .id_table       = mis2032_id,
};

static __init int init_mis2032(void) {
        return private_i2c_add_driver(&mis2032_driver);
}

static __exit void exit_mis2032(void) {
        private_i2c_del_driver(&mis2032_driver);
}

module_init(init_mis2032);
module_exit(exit_mis2032);

MODULE_DESCRIPTION("A low-level driver for Sony mis2032 sensor");
MODULE_LICENSE("GPL");
