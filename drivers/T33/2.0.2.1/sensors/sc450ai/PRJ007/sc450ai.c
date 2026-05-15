/*
 * sc450ai.c
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
 *  1           2592*1520       30        mipi_2lane    linear  10        30
 *  2           2688*1520       30        mipi_2lane    linear  10        30
 *  3           1280*720        60        mipi_2lane    linear  10        120
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

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xbd)
#define SENSOR_CHIP_ID_L    (0x2f)
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

struct tx_isp_sensor_attribute sc450ai_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
    unsigned int value;
    unsigned int gain;
};

struct again_lut sc450ai_again_lut[] = {
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
    {0x352, 23414},
    {0x353, 24587},
    {0x354, 25746},
    {0x355, 26820},
    {0x356, 27953},
    {0x357, 29002},
    {0x358, 30109},
    {0x359, 31203},
    {0x35a, 32217},
    {0x35b, 33287},
    {0x35c, 34345},
    {0x35d, 35326},
    {0x35e, 36361},
    {0x35f, 37322},
    {0x360, 38336},
    {0x361, 39339},
    {0x362, 40270},
    {0x363, 41253},
    {0x364, 42226},
    {0x365, 43129},
    {0x366, 44082},
    {0x367, 44968},
    {0x368, 45904},
    {0x369, 46830},
    {0x36a, 47690},
    {0x36b, 48599},
    {0x36c, 49500},
    {0x36d, 50336},
    {0x36e, 51220},
    {0x36f, 52042},
    {0x370, 52910},
    {0x371, 53771},
    {0x372, 54571},
    {0x373, 55416},
    {0x374, 56254},
    {0x375, 57033},
    {0x376, 57857},
    {0x377, 58623},
    {0x378, 59433},
    {0x379, 60237},
    {0x37a, 60984},
    {0x37b, 61774},
    {0x37c, 62558},
    {0x37d, 63287},
    {0x37e, 64059},
    {0x37f, 64776},
    {0x740, 65536},
    {0x741, 66990},
    {0x742, 68468},
    {0x743, 69878},
    {0x744, 71267},
    {0x745, 72637},
    {0x746, 74030},
    {0x747, 75360},
    {0x748, 76672},
    {0x749, 77965},
    {0x74a, 79283},
    {0x74b, 80541},
    {0x74c, 81784},
    {0x74d, 83010},
    {0x74e, 84259},
    {0x74f, 85454},
    {0x750, 86633},
    {0x751, 87799},
    {0x752, 88986},
    {0x753, 90123},
    {0x754, 91246},
    {0x755, 92356},
    {0x756, 93489},
    {0x757, 94573},
    {0x758, 95645},
    {0x759, 96705},
    {0x75a, 97786},
    {0x75b, 98823},
    {0x75c, 99848},
    {0x75d, 100862},
    {0x75e, 101897},
    {0x75f, 102890},
    {0x760, 103872},
    {0x761, 104844},
    {0x762, 105837},
    {0x763, 106789},
    {0x764, 107731},
    {0x765, 108665},
    {0x766, 109618},
    {0x767, 110533},
    {0x768, 111440},
    {0x769, 112337},
    {0x76a, 113255},
    {0x76b, 114135},
    {0x76c, 115008},
    {0x76d, 115872},
    {0x76e, 116756},
    {0x76f, 117605},
    {0x770, 118446},
    {0x771, 119280},
    {0x772, 120133},
    {0x773, 120952},
    {0x774, 121764},
    {0x775, 122569},
    {0x776, 123393},
    {0x777, 124185},
    {0x778, 124969},
    {0x779, 125748},
    {0x2340, 126545},
    {0x2341, 127996},
    {0x2342, 129450},
    {0x2343, 130859},
    {0x2344, 132269},
    {0x2345, 133636},
    {0x2346, 135007},
    {0x2347, 136335},
    {0x2348, 137667},
    {0x2349, 138981},
    {0x234a, 140255},
    {0x234b, 141533},
    {0x234c, 142773},
    {0x234d, 144018},
    {0x234e, 145227},
    {0x234f, 146440},
    {0x2350, 147638},
    {0x2351, 148801},
    {0x2352, 149969},
    {0x2353, 151104},
    {0x2354, 152245},
    {0x2355, 153353},
    {0x2356, 154467},
    {0x2357, 155568},
    {0x2358, 156638},
    {0x2359, 157714},
    {0x235a, 158761},
    {0x235b, 159813},
    {0x235c, 160836},
    {0x235d, 161866},
    {0x235e, 162884},
    {0x235f, 163875},
    {0x2360, 164873},
    {0x2361, 165843},
    {0x2362, 166820},
    {0x2363, 167770},
    {0x2364, 168728},
    {0x2365, 169675},
    {0x2366, 170598},
    {0x2367, 171527},
    {0x2368, 172432},
    {0x2369, 173343},
    {0x236a, 174231},
    {0x236b, 175125},
    {0x236c, 176011},
    {0x236d, 176874},
    {0x236e, 177743},
    {0x236f, 178591},
    {0x2370, 179445},
    {0x2371, 180277},
    {0x2372, 181116},
    {0x2373, 181948},
    {0x2374, 182759},
    {0x2375, 183576},
    {0x2376, 184373},
    {0x2377, 185177},
    {0x2378, 185961},
    {0x2379, 186751},
    {0x237a, 187535},
    {0x237b, 188299},
    {0x237c, 189070},
    {0x237d, 189822},
    {0x237e, 190581},
    {0x237f, 191321},
    {0x2740, 192068},
    {0x2741, 193532},
    {0x2742, 194974},
    {0x2743, 196395},
    {0x2744, 197805},
    {0x2745, 199184},
    {0x2746, 200543},
    {0x2747, 201882},
    {0x2748, 203203},
    {0x2749, 204506},
    {0x274a, 205791},
    {0x274b, 207069},
    {0x274c, 208320},
    {0x274d, 209554},
    {0x274e, 210773},
    {0x274f, 211976},
    {0x2750, 213164},
    {0x2751, 214337},
    {0x2752, 215505},
    {0x2753, 216650},
    {0x2754, 217781},
    {0x2755, 218899},
    {0x2756, 220003},
    {0x2757, 221095},
    {0x2758, 222174},
    {0x2759, 223250},
    {0x275a, 224305},
    {0x275b, 225349},
    {0x275c, 226381},
    {0x275d, 227402},
    {0x275e, 228412},
    {0x275f, 229411},
    {0x2760, 230409},
    {0x2761, 231387},
    {0x2762, 232356},
    {0x2763, 233314},
    {0x2764, 234264},
    {0x2765, 235203},
    {0x2766, 236134},
    {0x2767, 237055},
    {0x2768, 237975},
    {0x2769, 238879},
    {0x276a, 239774},
    {0x276b, 240661},
    {0x276c, 241539},
    {0x276d, 242410},
    {0x276e, 243272},
    {0x276f, 244134},
    {0x2770, 244981},
    {0x2771, 245820},
    {0x2772, 246652},
    {0x2773, 247477},
    {0x2774, 248295},
    {0x2775, 249105},
    {0x2776, 249916},
    {0x2777, 250713},
    {0x2778, 251503},
    {0x2779, 252287},
    {0x277a, 253064},
    {0x277b, 253835},
    {0x277c, 254600},
    {0x277d, 255365},
    {0x277e, 256117},
    {0x277f, 256864},
    {0x2f40, 257604},
    {0x2f41, 259068},
    {0x2f42, 260516},
    {0x2f43, 261936},
    {0x2f44, 263336},
    {0x2f45, 264714},
    {0x2f46, 266079},
    {0x2f47, 267418},
    {0x2f48, 268739},
    {0x2f49, 270047},
    {0x2f4a, 271332},
    {0x2f4b, 272600},
    {0x2f4c, 273851},
    {0x2f4d, 275090},
    {0x2f4e, 276309},
    {0x2f4f, 277512},
    {0x2f50, 278705},
    {0x2f51, 279878},
    {0x2f52, 281037},
    {0x2f53, 282181},
    {0x2f54, 283317},
    {0x2f55, 284435},
    {0x2f56, 285539},
    {0x2f57, 286631},
    {0x2f58, 287715},
    {0x2f59, 288782},
    {0x2f5a, 289837},
    {0x2f5b, 290885},
    {0x2f5c, 291917},
    {0x2f5d, 292938},
    {0x2f5e, 293948},
    {0x2f5f, 294952},
    {0x2f60, 295940},
    {0x2f61, 296919},
    {0x2f62, 297892},
    {0x2f63, 298850},
    {0x2f64, 299800},
    {0x2f65, 300739},
    {0x2f66, 301674},
    {0x2f67, 302595},
    {0x2f68, 303507},
    {0x2f69, 304415},
    {0x2f6a, 305310},
    {0x2f6b, 306197},
    {0x2f6c, 307075},
    {0x2f6d, 307949},
    {0x2f6e, 308812},
    {0x2f6f, 309666},
    {0x2f70, 310517},
    {0x2f71, 311356},
    {0x2f72, 312188},
    {0x2f73, 313013},
    {0x2f74, 313834},
    {0x2f75, 314645},
    {0x2f76, 315449},
    {0x2f77, 316246},
    {0x2f78, 317039},
    {0x2f79, 317823},
    {0x2f7a, 318600},
    {0x2f7b, 319374},
    {0x2f7c, 320139},
    {0x2f7d, 320897},
    {0x2f7e, 321650},
    {0x2f7f, 322400},
    {0x3f40, 323140},
    {0x3f41, 324608},
    {0x3f42, 326049},
    {0x3f43, 327472},
    {0x3f44, 328872},
    {0x3f45, 330253},
    {0x3f46, 331612},
    {0x3f47, 332954},
    {0x3f48, 334278},
    {0x3f49, 335580},
    {0x3f4a, 336868},
    {0x3f4b, 338136},
    {0x3f4c, 339389},
    {0x3f4d, 340624},
    {0x3f4e, 341845},
    {0x3f4f, 343048},
    {0x3f50, 344238},
    {0x3f51, 345414},
    {0x3f52, 346573},
    {0x3f53, 347720},
    {0x3f54, 348851},
    {0x3f55, 349971},
    {0x3f56, 351075},
    {0x3f57, 352169},
    {0x3f58, 353251},
    {0x3f59, 354318},
    {0x3f5a, 355375},
    {0x3f5b, 356419},
    {0x3f5c, 357453},
    {0x3f5d, 358474},
    {0x3f5e, 359486},
    {0x3f5f, 360485},
    {0x3f60, 361476},
    {0x3f61, 362457},
    {0x3f62, 363426},
    {0x3f63, 364386},
    {0x3f64, 365336},
    {0x3f65, 366277},
    {0x3f66, 367208},
    {0x3f67, 368131},
    {0x3f68, 369045},
    {0x3f69, 369949},
    {0x3f6a, 370846},
    {0x3f6b, 371733},
    {0x3f6c, 372613},
    {0x3f6d, 373483},
    {0x3f6e, 374348},
    {0x3f6f, 375202},
    {0x3f70, 376051},
    {0x3f71, 376892},
    {0x3f72, 377724},
    {0x3f73, 378551},
    {0x3f74, 379369},
    {0x3f75, 380181},
    {0x3f76, 380985},
    {0x3f77, 381783},
    {0x3f78, 382575},
    {0x3f79, 383359},
    {0x3f7a, 384138},
    {0x3f7b, 384909},
    {0x3f7c, 385675},
    {0x3f7d, 386433},
    {0x3f7e, 387188},
    {0x3f7f, 387934},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc450ai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = sc450ai_again_lut;
    while (lut->gain <= sc450ai_attr.again.max) {
        if (isp_gain == 0) {
            *sensor_again = lut[0].value;
            return 0;
        } else if (isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        } else {
            if ((lut->gain == sc450ai_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc450ai_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = sc450ai_again_lut;
    while(lut->gain <= sc450ai_attr.again_short.max) {
        if(isp_gain == 0) {
            *sensor_again = 0;
            return 0;
        }
        else if(isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        }
        else{
            if((lut->gain == sc450ai_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc450ai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
    return 0;
}

#ifdef SENSOR_HCG
unsigned int sc450ai_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc450ai_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc450ai_mipi_linear_sboot0 = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 720,
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

struct tx_isp_mipi_bus sc450ai_mipi_linear_sboot1 = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 720,
    .lans = 2,
    .image_twidth = 2592,
    .image_theight = 1520,
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

struct tx_isp_mipi_bus sc450ai_mipi_linear_sboot2 = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 720,
    .lans = 2,
    .image_twidth = 2688,
    .image_theight = 1520,
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

struct tx_isp_mipi_bus sc450ai_mipi_linear_sboot3 = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 720,
    .lans = 2,
    .image_twidth = 1280,
    .image_theight = 720,
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

struct tx_isp_dvp_bus sc450ai_dvp = {
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

struct tx_isp_sensor_attribute sc450ai_attr = {
    .name = "sc450ai",
    .chip_id = 0xcd2d,
    .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
    .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
    .cbus_device = 0x30,
    .sensor_ctrl.alloc_again = sc450ai_alloc_again,
    .sensor_ctrl.alloc_dgain = sc450ai_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
    .sensor_ctrl.alloc_again_short = sc450ai_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

    .dgain = {0},
    .dgain_short = {0},
};

static struct regval_list sc450ai_init_regs_2560_1440_30fps_mipi[] = {
    //cleaned_0x23_SC450AI_MIPI_24MInput_2lane_720Mbps_10bit_30fps_2560x1440
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x36f9,0x80},
    {0x3018,0x3a},
    {0x3019,0x0c},
    {0x301c,0x78},
    {0x301f,0x23},
    {0x3021,0x87},
    {0x302e,0x00},
    {0x3200,0x00},
    {0x3201,0x40},
    {0x3202,0x00},
    {0x3203,0x28},
    {0x3204,0x0a},
    {0x3205,0x4f},
    {0x3206,0x05},
    {0x3207,0xd7},
    {0x3208,0x0a},
    {0x3209,0x00},
    {0x320a,0x05},
    {0x320b,0xa0},
    {0x320c,0x03},//hts = 0x384 = 900
    {0x320d,0x84},//
    {0x320e,0x06},//vts = 0x640 = 1600
    {0x320f,0x40},//
    {0x3210,0x00},
    {0x3211,0x08},
    {0x3212,0x00},
    {0x3213,0x08},
    {0x3214,0x11},
    {0x3215,0x11},
    {0x3220,0x00},
    {0x3223,0xc0},
    {0x3253,0x10},
    {0x325f,0x44},
    {0x3274,0x09},
    {0x3280,0x01},
    {0x3301,0x08},
    {0x3306,0x24},
    {0x3309,0x60},
    {0x330b,0x64},
    {0x330d,0x30},
    {0x3315,0x00},
    {0x331f,0x59},
    {0x335d,0x60},
    {0x3364,0x56},
    {0x338f,0x80},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0x38},
    {0x3393,0x0a},
    {0x3394,0x10},
    {0x3395,0x18},
    {0x3396,0x08},
    {0x3397,0x18},
    {0x3398,0x38},
    {0x3399,0x0f},
    {0x339a,0x12},
    {0x339b,0x14},
    {0x339c,0x18},
    {0x33af,0x18},
    {0x360f,0x13},
    {0x3621,0xec},
    {0x3627,0xa0},
    {0x3630,0x90},
    {0x3633,0x56},
    {0x3637,0x1d},
    {0x3638,0x0a},
    {0x363c,0x0f},
    {0x363d,0x0f},
    {0x363e,0x08},
    {0x3670,0x4a},
    {0x3671,0xe0},
    {0x3672,0xe0},
    {0x3673,0xe0},
    {0x3674,0xb0},
    {0x3675,0x88},
    {0x3676,0x8c},
    {0x367a,0x48},
    {0x367b,0x58},
    {0x367c,0x48},
    {0x367d,0x58},
    {0x3690,0x34},
    {0x3691,0x43},
    {0x3692,0x44},
    {0x3699,0x03},
    {0x369a,0x0f},
    {0x369b,0x1f},
    {0x369c,0x40},
    {0x369d,0x48},
    {0x36a2,0x48},
    {0x36a3,0x78},
    {0x36b0,0x54},
    {0x36b1,0x75},
    {0x36b2,0x35},
    {0x36b3,0x48},
    {0x36b4,0x78},
    {0x36b7,0xa0},
    {0x36b8,0xa0},
    {0x36b9,0x20},
    {0x36bd,0x40},
    {0x36be,0x48},
    {0x36d0,0x20},
    {0x36e0,0x08},
    {0x36e1,0x08},
    {0x36e2,0x12},
    {0x36e3,0x48},
    {0x36e4,0x78},
    {0x36ea,0x0a},
    {0x36eb,0x05},
    {0x36ec,0x43},
    {0x36ed,0x34},
    {0x36fa,0x06},
    {0x36fb,0xa4},
    {0x36fc,0x00},
    {0x36fd,0x34},
    {0x3907,0x00},
    {0x3908,0x41},
    {0x391e,0x01},
    {0x391f,0x11},
    {0x3933,0x82},
    {0x3934,0x0b},
    {0x3935,0x02},
    {0x3936,0x5e},
    {0x3937,0x76},
    {0x3938,0x78},
    {0x3939,0x00},
    {0x393a,0x28},
    {0x393b,0x00},
    {0x393c,0x1d},
    {0x3e00,0x00},
    {0x3e01,0xc7},
    {0x3e02,0x80},
    {0x3e03,0x0b},
    {0x3e08,0x03},
    {0x3e1b,0x2a},
    {0x440e,0x02},
    {0x4509,0x20},
    {0x4837,0x16},
    {0x5000,0x0e},
    {0x5001,0x44},
    {0x5780,0x76},
    {0x5784,0x08},
    {0x5785,0x04},
    {0x5787,0x0a},
    {0x5788,0x0a},
    {0x5789,0x0a},
    {0x578a,0x0a},
    {0x578b,0x0a},
    {0x578c,0x0a},
    {0x578d,0x40},
    {0x5790,0x08},
    {0x5791,0x04},
    {0x5792,0x04},
    {0x5793,0x08},
    {0x5794,0x04},
    {0x5795,0x04},
    {0x5799,0x46},
    {0x579a,0x77},
    {0x57a1,0x04},
    {0x57a8,0xd0},
    {0x57aa,0x2a},
    {0x57ab,0x7f},
    {0x57ac,0x00},
    {0x57ad,0x00},
    {0x59e0,0xfe},
    {0x59e1,0x40},
    {0x59e2,0x3f},
    {0x59e3,0x38},
    {0x59e4,0x30},
    {0x59e5,0x3f},
    {0x59e6,0x38},
    {0x59e7,0x30},
    {0x59e8,0x3f},
    {0x59e9,0x3c},
    {0x59ea,0x38},
    {0x59eb,0x3f},
    {0x59ec,0x3c},
    {0x59ed,0x38},
    {0x59ee,0xfe},
    {0x59ef,0x40},
    {0x59f4,0x3f},
    {0x59f5,0x38},
    {0x59f6,0x30},
    {0x59f7,0x3f},
    {0x59f8,0x38},
    {0x59f9,0x30},
    {0x59fa,0x3f},
    {0x59fb,0x3c},
    {0x59fc,0x38},
    {0x59fd,0x3f},
    {0x59fe,0x3c},
    {0x59ff,0x38},
    {0x36e9,0x20},
    {0x36f9,0x20},
    {0x0100,0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};


static struct regval_list sc450ai_init_regs_2592_1520_30fps_mipi[] = {
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x36f9,0x80},
    {0x3018,0x3a},
    {0x3019,0x0c},
    {0x301c,0x78},
    {0x301f,0x0b},
    {0x3021,0x87},
    {0x302e,0x00},
    {0x3200,0x00},
    {0x3201,0x30},
    {0x3202,0x00},
    {0x3203,0x00},
    {0x3204,0x0a},
    {0x3205,0x5f},
    {0x3206,0x05},
    {0x3207,0xff},
    {0x3208,0x0a},
    {0x3209,0x20},
    {0x320a,0x05},
    {0x320b,0xf0},
    {0x320c,0x02},//hts 0x2ee = 750
    {0x320d,0xee},//
    {0x320e,0x06},//vts 25fps@0x750=1872 30fps@0x618=1560
    {0x320f,0x18},//
    {0x3210,0x00},
    {0x3211,0x08},
    {0x3212,0x00},
    {0x3213,0x08},
    {0x3214,0x11},
    {0x3215,0x11},
    {0x3220,0x00},
    {0x3223,0xc0},
    {0x3253,0x10},
    {0x325f,0x44},
    {0x3274,0x09},
    {0x3280,0x01},
    {0x3301,0x07},
    {0x3306,0x20},
    {0x3308,0x08},
    {0x330b,0x58},
    {0x330e,0x18},
    {0x3315,0x00},
    {0x335d,0x60},
    {0x3364,0x56},
    {0x338f,0x80},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0x38},
    {0x3393,0x07},
    {0x3394,0x10},
    {0x3395,0x18},
    {0x3396,0x08},
    {0x3397,0x18},
    {0x3398,0x38},
    {0x3399,0x10},
    {0x339a,0x13},
    {0x339b,0x15},
    {0x339c,0x18},
    {0x33af,0x18},
    {0x360f,0x13},
    {0x3621,0xec},
    {0x3622,0x00},
    {0x3625,0x0b},
    {0x3627,0x20},
    {0x3630,0x90},
    {0x3633,0x56},
    {0x3637,0x1d},
    {0x3638,0x12},
    {0x363c,0x0f},
    {0x363d,0x0f},
    {0x363e,0x08},
    {0x3670,0x4a},
    {0x3671,0xe0},
    {0x3672,0xe0},
    {0x3673,0xe0},
    {0x3674,0xc0},
    {0x3675,0x87},
    {0x3676,0x8c},
    {0x367a,0x48},
    {0x367b,0x58},
    {0x367c,0x48},
    {0x367d,0x58},
    {0x3690,0x22},
    {0x3691,0x33},
    {0x3692,0x44},
    {0x3699,0x03},
    {0x369a,0x0f},
    {0x369b,0x1f},
    {0x369c,0x40},
    {0x369d,0x78},
    {0x36a2,0x48},
    {0x36a3,0x78},
    {0x36b0,0x53},
    {0x36b1,0x74},
    {0x36b2,0x34},
    {0x36b3,0x40},
    {0x36b4,0x78},
    {0x36b7,0xa0},
    {0x36b8,0xa0},
    {0x36b9,0x20},
    {0x36bd,0x40},
    {0x36be,0x48},
    {0x36d0,0x20},
    {0x36e0,0x08},
    {0x36e1,0x08},
    {0x36e2,0x12},
    {0x36e3,0x48},
    {0x36e4,0x78},
    {0x36ea,0x0f},
    {0x36eb,0x05},
    {0x36ec,0x43},
    {0x36ed,0x14},
    {0x36fa,0xcd},
    {0x36fb,0xb4},
    {0x36fc,0x00},
    {0x36fd,0x04},
    {0x3907,0x00},
    {0x3908,0x41},
    {0x391e,0xf1},
    {0x391f,0x11},
    {0x3933,0x82},
    {0x3934,0x30},
    {0x3935,0x02},
    {0x3936,0xc7},
    {0x3937,0x76},
    {0x3938,0x76},
    {0x3939,0x00},
    {0x393a,0x28},
    {0x393b,0x00},
    {0x393c,0x23},
    {0x3e01,0xe9},
    {0x3e02,0x80},
    {0x3e03,0x0b},
    {0x3e08,0x03},
    {0x3e1b,0x2a},
    {0x440e,0x02},
    {0x4509,0x20},
    {0x4837,0x16},
    {0x5000,0x0e},
    {0x5001,0x44},
    {0x5799,0x06},
    {0x59e0,0xfe},
    {0x59e1,0x40},
    {0x59e2,0x3f},
    {0x59e3,0x38},
    {0x59e4,0x30},
    {0x59e5,0x3f},
    {0x59e6,0x38},
    {0x59e7,0x30},
    {0x59e8,0x3f},
    {0x59e9,0x3c},
    {0x59ea,0x38},
    {0x59eb,0x3f},
    {0x59ec,0x3c},
    {0x59ed,0x38},
    {0x59ee,0xfe},
    {0x59ef,0x40},
    {0x59f4,0x3f},
    {0x59f5,0x38},
    {0x59f6,0x30},
    {0x59f7,0x3f},
    {0x59f8,0x38},
    {0x59f9,0x30},
    {0x59fa,0x3f},
    {0x59fb,0x3c},
    {0x59fc,0x38},
    {0x59fd,0x3f},
    {0x59fe,0x3c},
    {0x59ff,0x38},
    {0x36e9,0x20},
    {0x36f9,0x53},
    {0x0100,0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc450ai_init_regs_2688_1520_30fps_mipi[] = {
    //cleaned_0x15_SC450AI_MIPI_24MInput_2lane_720Mbps_10bit_2688x1520_30fps
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x36f9,0x80},
    {0x3018,0x3a},
    {0x3019,0x0c},
    {0x301c,0x78},
    {0x301f,0x15},
    {0x3021,0x87},
    {0x302e,0x00},
    {0x3200,0x00},
    {0x3201,0x00},
    {0x3202,0x00},
    {0x3203,0x00},
    {0x3204,0x0a},
    {0x3205,0x8f},
    {0x3206,0x05},
    {0x3207,0xff},
    {0x3208,0x0a},
    {0x3209,0x80},
    {0x320a,0x05},
    {0x320b,0xf0},
    {0x320c,0x02},//hts = 0x2ee = 750
    {0x320d,0xee},//
    {0x320e,0x06},//vts = 0x618 = 1560
    {0x320f,0x18},//
    {0x3210,0x00},
    {0x3211,0x08},
    {0x3212,0x00},
    {0x3213,0x08},
    {0x3214,0x11},
    {0x3215,0x11},
    {0x3220,0x00},
    {0x3223,0xc0},
    {0x3253,0x10},
    {0x325f,0x44},
    {0x3274,0x09},
    {0x3280,0x01},
    {0x3301,0x07},
    {0x3306,0x20},
    {0x3308,0x08},
    {0x330b,0x58},
    {0x330e,0x18},
    {0x3315,0x00},
    {0x335d,0x60},
    {0x3364,0x56},
    {0x338f,0x80},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0x38},
    {0x3393,0x07},
    {0x3394,0x10},
    {0x3395,0x18},
    {0x3396,0x08},
    {0x3397,0x18},
    {0x3398,0x38},
    {0x3399,0x10},
    {0x339a,0x13},
    {0x339b,0x15},
    {0x339c,0x18},
    {0x33af,0x18},
    {0x360f,0x13},
    {0x3621,0xec},
    {0x3622,0x00},
    {0x3625,0x0b},
    {0x3627,0x20},
    {0x3630,0x90},
    {0x3633,0x56},
    {0x3637,0x1d},
    {0x3638,0x12},
    {0x363c,0x0f},
    {0x363d,0x0f},
    {0x363e,0x08},
    {0x3670,0x4a},
    {0x3671,0xe0},
    {0x3672,0xe0},
    {0x3673,0xe0},
    {0x3674,0xc0},
    {0x3675,0x87},
    {0x3676,0x8c},
    {0x367a,0x48},
    {0x367b,0x58},
    {0x367c,0x48},
    {0x367d,0x58},
    {0x3690,0x22},
    {0x3691,0x33},
    {0x3692,0x44},
    {0x3699,0x03},
    {0x369a,0x0f},
    {0x369b,0x1f},
    {0x369c,0x40},
    {0x369d,0x78},
    {0x36a2,0x48},
    {0x36a3,0x78},
    {0x36b0,0x53},
    {0x36b1,0x74},
    {0x36b2,0x34},
    {0x36b3,0x40},
    {0x36b4,0x78},
    {0x36b7,0xa0},
    {0x36b8,0xa0},
    {0x36b9,0x20},
    {0x36bd,0x40},
    {0x36be,0x48},
    {0x36d0,0x20},
    {0x36e0,0x08},
    {0x36e1,0x08},
    {0x36e2,0x12},
    {0x36e3,0x48},
    {0x36e4,0x78},
    {0x36ea,0x0f},
    {0x36eb,0x05},
    {0x36ec,0x43},
    {0x36ed,0x14},
    {0x36fa,0xcd},
    {0x36fb,0xb4},
    {0x36fc,0x00},
    {0x36fd,0x04},
    {0x3907,0x00},
    {0x3908,0x41},
    {0x391e,0xf1},
    {0x391f,0x11},
    {0x3933,0x82},
    {0x3934,0x30},
    {0x3935,0x02},
    {0x3936,0xc7},
    {0x3937,0x76},
    {0x3938,0x76},
    {0x3939,0x00},
    {0x393a,0x28},
    {0x393b,0x00},
    {0x393c,0x23},
    {0x3e01,0xc2},
    {0x3e02,0x60},
    {0x3e03,0x0b},
    {0x3e08,0x03},
    {0x3e1b,0x2a},
    {0x440e,0x02},
    {0x4509,0x20},
    {0x4837,0x16},
    {0x5000,0x0e},
    {0x5001,0x44},
    {0x5780,0x76},
    {0x5784,0x08},
    {0x5785,0x04},
    {0x5787,0x0a},
    {0x5788,0x0a},
    {0x5789,0x0a},
    {0x578a,0x0a},
    {0x578b,0x0a},
    {0x578c,0x0a},
    {0x578d,0x40},
    {0x5790,0x08},
    {0x5791,0x04},
    {0x5792,0x04},
    {0x5793,0x08},
    {0x5794,0x04},
    {0x5795,0x04},
    {0x5799,0x46},
    {0x579a,0x77},
    {0x57a1,0x04},
    {0x57a8,0xd0},
    {0x57aa,0x2a},
    {0x57ab,0x7f},
    {0x57ac,0x00},
    {0x57ad,0x00},
    {0x59e0,0xfe},
    {0x59e1,0x40},
    {0x59e2,0x3f},
    {0x59e3,0x38},
    {0x59e4,0x30},
    {0x59e5,0x3f},
    {0x59e6,0x38},
    {0x59e7,0x30},
    {0x59e8,0x3f},
    {0x59e9,0x3c},
    {0x59ea,0x38},
    {0x59eb,0x3f},
    {0x59ec,0x3c},
    {0x59ed,0x38},
    {0x59ee,0xfe},
    {0x59ef,0x40},
    {0x59f4,0x3f},
    {0x59f5,0x38},
    {0x59f6,0x30},
    {0x59f7,0x3f},
    {0x59f8,0x38},
    {0x59f9,0x30},
    {0x59fa,0x3f},
    {0x59fb,0x3c},
    {0x59fc,0x38},
    {0x59fd,0x3f},
    {0x59fe,0x3c},
    {0x59ff,0x38},
    {0x36e9,0x20},
    {0x36f9,0x53},
    {0x0100,0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc450ai_init_regs_1280_720_120fps_mipi[] = {
    //Cleaned_0x96_SC450AI_MIPI_24MInput_2lane_720Mbps_10bit_120.192fps_1280x720_hsub_vbin(1)
    //VTS=780.000000,HTS=1536.000000,SCLK=87.750000,PCLK=144.000000,MipiCLK=720.000000,Tline=10.666667us,TExp_step=0.5*Tline=5.333333us,TExp_offset=2.256410us
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x36f9,0x80},
    {0x3018,0x3a},
    {0x3019,0x0c},
    {0x301c,0x78},
    {0x301f,0x96},
    {0x3021,0x87},
    {0x302d,0xa0},
    {0x302e,0x00},
    {0x3200,0x00},
    {0x3201,0x40},
    {0x3202,0x00},
    {0x3203,0x28},
    {0x3204,0x0a},
    {0x3205,0x4f},
    {0x3206,0x05},
    {0x3207,0xd7},
    {0x3208,0x05},
    {0x3209,0x00},
    {0x320a,0x02},
    {0x320b,0xd0},
    {0x320c,0x03},//hts = 0x3a8 = 936
    {0x320d,0xa8},//
    {0x320e,0x03},//120fps@vts=0x30c=780  60fps@vts=0x618=1560
    {0x320f,0x0c},//
    {0x3210,0x00},
    {0x3211,0x04},
    {0x3212,0x00},
    {0x3213,0x04},
    {0x3214,0x11},
    {0x3215,0x31},
    {0x3220,0x01},
    {0x3223,0xc0},
    {0x3253,0x10},
    {0x325f,0x44},
    {0x3274,0x09},
    {0x3280,0x01},
    {0x3301,0x08},
    {0x3306,0x24},
    {0x3309,0x60},
    {0x330b,0x64},
    {0x330d,0x30},
    {0x3315,0x00},
    {0x331f,0x59},
    {0x335d,0x60},
    {0x3364,0x56},
    {0x338f,0x80},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0x38},
    {0x3393,0x0a},
    {0x3394,0x10},
    {0x3395,0x18},
    {0x3396,0x08},
    {0x3397,0x18},
    {0x3398,0x38},
    {0x3399,0x0f},
    {0x339a,0x12},
    {0x339b,0x14},
    {0x339c,0x18},
    {0x33af,0x18},
    {0x3400,0x16},
    {0x360f,0x13},
    {0x3621,0xec},
    {0x3627,0xa0},
    {0x3630,0x90},
    {0x3633,0x56},
    {0x3637,0x1d},
    {0x3638,0x0a},
    {0x363c,0x0f},
    {0x363d,0x0f},
    {0x363e,0x08},
    {0x3670,0x4a},
    {0x3671,0xe0},
    {0x3672,0xe0},
    {0x3673,0xe0},
    {0x3674,0xb0},
    {0x3675,0x88},
    {0x3676,0x8c},
    {0x367a,0x48},
    {0x367b,0x58},
    {0x367c,0x48},
    {0x367d,0x58},
    {0x3690,0x34},
    {0x3691,0x43},
    {0x3692,0x44},
    {0x3699,0x03},
    {0x369a,0x0f},
    {0x369b,0x1f},
    {0x369c,0x40},
    {0x369d,0x48},
    {0x36a2,0x48},
    {0x36a3,0x78},
    {0x36b0,0x54},
    {0x36b1,0x75},
    {0x36b2,0x35},
    {0x36b3,0x48},
    {0x36b4,0x78},
    {0x36b7,0xa0},
    {0x36b8,0xa0},
    {0x36b9,0x20},
    {0x36bd,0x40},
    {0x36be,0x48},
    {0x36d0,0x20},
    {0x36e0,0x08},
    {0x36e1,0x08},
    {0x36e2,0x12},
    {0x36e3,0x48},
    {0x36e4,0x78},
    {0x36ea,0x0a},
    {0x36eb,0x05},
    {0x36ec,0x43},
    {0x36ed,0x34},
    {0x36fa,0x4d},
    {0x36fb,0xa4},
    {0x36fc,0x00},
    {0x36fd,0x24},
    {0x3907,0x00},
    {0x3908,0x41},
    {0x391e,0x01},
    {0x391f,0x11},
    {0x3921,0x10},
    {0x3933,0x82},
    {0x3934,0x0b},
    {0x3935,0x02},
    {0x3936,0x5e},
    {0x3937,0x76},
    {0x3938,0x78},
    {0x3939,0x00},
    {0x393a,0x28},
    {0x393b,0x00},
    {0x393c,0x1d},
    {0x3e00,0x00},
    {0x3e01,0x61},
    {0x3e02,0x00},
    {0x3e03,0x0b},
    {0x3e08,0x03},
    {0x3e1b,0x2a},
    {0x440e,0x02},
    {0x4509,0x20},
    {0x4837,0x16},
    {0x5000,0x4e},
    {0x5001,0x44},
    {0x5780,0x76},
    {0x5784,0x08},
    {0x5785,0x04},
    {0x5787,0x0a},
    {0x5788,0x0a},
    {0x5789,0x0a},
    {0x578a,0x0a},
    {0x578b,0x0a},
    {0x578c,0x0a},
    {0x578d,0x40},
    {0x5790,0x08},
    {0x5791,0x04},
    {0x5792,0x04},
    {0x5793,0x08},
    {0x5794,0x04},
    {0x5795,0x04},
    {0x5799,0x46},
    {0x579a,0x77},
    {0x57a1,0x04},
    {0x57a8,0xd0},
    {0x57aa,0x2a},
    {0x57ab,0x7f},
    {0x57ac,0x00},
    {0x57ad,0x00},
    {0x5900,0x01},
    {0x5901,0x04},
    {0x59e0,0xfe},
    {0x59e1,0x40},
    {0x59e2,0x3f},
    {0x59e3,0x38},
    {0x59e4,0x30},
    {0x59e5,0x3f},
    {0x59e6,0x38},
    {0x59e7,0x30},
    {0x59e8,0x3f},
    {0x59e9,0x3c},
    {0x59ea,0x38},
    {0x59eb,0x3f},
    {0x59ec,0x3c},
    {0x59ed,0x38},
    {0x59ee,0xfe},
    {0x59ef,0x40},
    {0x59f4,0x3f},
    {0x59f5,0x38},
    {0x59f6,0x30},
    {0x59f7,0x3f},
    {0x59f8,0x38},
    {0x59f9,0x30},
    {0x59fa,0x3f},
    {0x59fb,0x3c},
    {0x59fc,0x38},
    {0x59fd,0x3f},
    {0x59fe,0x3c},
    {0x59ff,0x38},
    {0x36e9,0x20},
    {0x36f9,0x51},
    {0x0100,0x01},
    {SENSOR_REG_DELAY,0x10},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc450ai_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc450ai_win_sizes[] = {
    /* 2560*1440 [0] */
    {
        .width          = 2560,
        .height         = 1440,
        .fps            = 30 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = sc450ai_init_regs_2560_1440_30fps_mipi,
    },
    /* 2592*1520 [1] */
    {
        .width          = 2592,
        .height         = 1520,
        .fps            = 30 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = sc450ai_init_regs_2592_1520_30fps_mipi,
    },
    /* 2688*1520 [2] */
    {
        .width          = 2688,
        .height         = 1520,
        .fps            = 30 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = sc450ai_init_regs_2688_1520_30fps_mipi,
    },
    /* 1280*720 [3] */
    {
        .width          = 1280,
        .height         = 720,
        .fps            = 120 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = sc450ai_init_regs_1280_720_120fps_mipi,
    },
};

static struct tx_isp_sensor_win_setting *wsize = &sc450ai_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc450ai_stream_on_mipi[] = {
    {0x0100, 0x01},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc450ai_stream_off_mipi[] = {
    {0x0100, 0x00},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc450ai_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc450ai_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc450ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = sc450ai_read(sd, vals->reg_num, &val);
            /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif

static int sc450ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = sc450ai_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc450ai_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc450ai_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc450ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = sc450ai_read(sd, vals->reg_num, &val);
            if (ret < 0)
                return ret;
        }
        vals++;
    }
    return 0;
}
#endif

static int sc450ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = sc450ai_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc450ai_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc450ai_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc450ai_setting_select(struct tx_isp_subdev *sd, int deboot)
{
    int ret = ISP_SUCCESS;

    switch (deboot) {
    case 0:
        wsize = &sc450ai_win_sizes[0];
        sc450ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        sc450ai_attr.again.value = 0;
        sc450ai_attr.again.max = 387934;
        sc450ai_attr.again.min = 0;
        sc450ai_attr.again.apply_delay = 2;

        sc450ai_attr.integration_time.value = 0x9c;
        sc450ai_attr.integration_time.max = 1600 -4;
        sc450ai_attr.integration_time.min = 1;
        sc450ai_attr.integration_time.apply_delay = 2;

        sc450ai_attr.total_width = 900;
        sc450ai_attr.total_height = 1600;

        sc450ai_attr.expo_fs = 1;

#ifdef SENSOR_HCG
        sc450ai_attr.hcg.base_gain = ;
        sc450ai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        sc450ai_attr.again_short.value = ;
        sc450ai_attr.again_short.max = ;
        sc450ai_attr.again_short.min = ;
        sc450ai_attr.again_short.apply_delay = ;

        sc450ai_attr.integration_time_short.value = ;
        sc450ai_attr.integration_time_short.max = ;
        sc450ai_attr.integration_time_short.min = ;
        sc450ai_attr.integration_time_short.apply_delay = ;

        sc450ai_attr.wdr_cache = wdr_line * sc450ai_attr.total_width;

#ifdef SENSOR_HCG
        sc450ai_attr.hcg_short.base_gain = ;
        sc450ai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(sc450ai_attr.mipi)), (void *)(&sc450ai_mipi_linear_sboot0), sizeof(sc450ai_attr.mipi));
        break;
    case 1:
        wsize = &sc450ai_win_sizes[1];
        sc450ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        sc450ai_attr.again.value = 0;
        sc450ai_attr.again.max = 387934;
        sc450ai_attr.again.min = 0;
        sc450ai_attr.again.apply_delay = 2;

        sc450ai_attr.integration_time.value = 0x9c;
        sc450ai_attr.integration_time.max = 1560 -4;
        sc450ai_attr.integration_time.min = 1;
        sc450ai_attr.integration_time.apply_delay = 2;

        sc450ai_attr.total_width = 750;
        sc450ai_attr.total_height = 1560;

        sc450ai_attr.expo_fs = 1;

#ifdef SENSOR_HCG
        sc450ai_attr.hcg.base_gain = ;
        sc450ai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        sc450ai_attr.again_short.value = ;
        sc450ai_attr.again_short.max = ;
        sc450ai_attr.again_short.min = ;
        sc450ai_attr.again_short.apply_delay = ;

        sc450ai_attr.integration_time_short.value = ;
        sc450ai_attr.integration_time_short.max = ;
        sc450ai_attr.integration_time_short.min = ;
        sc450ai_attr.integration_time_short.apply_delay = ;

        sc450ai_attr.wdr_cache = wdr_line * sc450ai_attr.total_width;

#ifdef SENSOR_HCG
        sc450ai_attr.hcg_short.base_gain = ;
        sc450ai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(sc450ai_attr.mipi)), (void *)(&sc450ai_mipi_linear_sboot1), sizeof(sc450ai_attr.mipi));
        break;
    case 2:
        wsize = &sc450ai_win_sizes[2];
        sc450ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        sc450ai_attr.again.value = 0;
        sc450ai_attr.again.max = 387934;
        sc450ai_attr.again.min = 0;
        sc450ai_attr.again.apply_delay = 2;

        sc450ai_attr.integration_time.value = 0x9c;
        sc450ai_attr.integration_time.max = 1560 -4;
        sc450ai_attr.integration_time.min = 1;
        sc450ai_attr.integration_time.apply_delay = 2;

        sc450ai_attr.total_width = 750;
        sc450ai_attr.total_height = 1560;

        sc450ai_attr.expo_fs = 1;

#ifdef SENSOR_HCG
        sc450ai_attr.hcg.base_gain = ;
        sc450ai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        sc450ai_attr.again_short.value = ;
        sc450ai_attr.again_short.max = ;
        sc450ai_attr.again_short.min = ;
        sc450ai_attr.again_short.apply_delay = ;

        sc450ai_attr.integration_time_short.value = ;
        sc450ai_attr.integration_time_short.max = ;
        sc450ai_attr.integration_time_short.min = ;
        sc450ai_attr.integration_time_short.apply_delay = ;

        sc450ai_attr.wdr_cache = wdr_line * sc450ai_attr.total_width;

#ifdef SENSOR_HCG
        sc450ai_attr.hcg_short.base_gain = ;
        sc450ai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(sc450ai_attr.mipi)), (void *)(&sc450ai_mipi_linear_sboot2), sizeof(sc450ai_attr.mipi));
        break;
    case 3:
        wsize = &sc450ai_win_sizes[3];
        sc450ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        sc450ai_attr.again.value = 0;
        sc450ai_attr.again.max = 387934;
        sc450ai_attr.again.min = 0;
        sc450ai_attr.again.apply_delay = 2;

        sc450ai_attr.integration_time.value = 0x9c;
        sc450ai_attr.integration_time.max = 780 - 4;
        sc450ai_attr.integration_time.min = 1;
        sc450ai_attr.integration_time.apply_delay = 2;

        sc450ai_attr.total_width = 936;
        sc450ai_attr.total_height = 780;

        sc450ai_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
        sc450ai_attr.hcg.base_gain = ;
        sc450ai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        sc450ai_attr.again_short.value = ;
        sc450ai_attr.again_short.max = ;
        sc450ai_attr.again_short.min = ;
        sc450ai_attr.again_short.apply_delay = ;

        sc450ai_attr.integration_time_short.value = ;
        sc450ai_attr.integration_time_short.max = ;
        sc450ai_attr.integration_time_short.min = ;
        sc450ai_attr.integration_time_short.apply_delay = ;

        sc450ai_attr.wdr_cache = wdr_line * sc450ai_attr.total_width;

#ifdef SENSOR_HCG
        sc450ai_attr.hcg_short.base_gain = ;
        sc450ai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(sc450ai_attr.mipi)), (void *)(&sc450ai_mipi_linear_sboot3), sizeof(sc450ai_attr.mipi));
        break;
    default:
        ISP_ERROR("Have no this Setting Source!!!\n");
        break;
    }

    return ret;
}

static int sc450ai_attr_check(struct tx_isp_subdev *sd)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct clk *sclka;
    int ret = ISP_SUCCESS;

    sc450ai_setting_select(sd, info->default_boot);

    switch (info->video_interface) {
    case TISP_SENSOR_VI_MIPI_CSI0:
        sc450ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        sc450ai_attr.mipi.index = 0;
        break;
    case TISP_SENSOR_VI_MIPI_CSI1:
        sc450ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        sc450ai_attr.mipi.index = 1;
        break;
    case TISP_SENSOR_VI_DVP:
        sc450ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

    ret = sc450ai_clk_set(sd, sclka, SENSOR_MCLK);
    if (ret) {
        ISP_ERROR("MCLK configuration failed!!!\n");
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
    sc450ai_attr_set(sd, wsize);
    sensor->priv = wsize;

    return 0;

err_get_mclk:
    return -1;
}

static int sc450ai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
    unsigned char v;
    int ret;

    ret = sc450ai_read(sd, 0x3107, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_H)
        return -ENODEV;
    *ident = v;

    ret = sc450ai_read(sd, 0x3108, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_L)
        return -ENODEV;
    *ident = (*ident << 8) | v;

    return 0;
}

static int sc450ai_g_chip_ident(struct tx_isp_subdev *sd,
                                struct tx_isp_chip_ident *chip)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    unsigned int ident = 0;
    int ret = ISP_SUCCESS;

    sc450ai_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
    if (info->rst_gpio != -1) {
        ret = private_gpio_request(info->rst_gpio, "sc450ai_reset");
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
        ret = private_gpio_request(info->pwdn_gpio, "sc450ai_pwdn");
        if (!ret) {
            private_gpio_direction_output(info->pwdn_gpio, 0);
            private_msleep(5);
            private_gpio_direction_output(info->pwdn_gpio, 1);
            private_msleep(5);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
        }
    }
    ret = sc450ai_detect(sd, &ident);
    if (ret) {
        ISP_ERROR("chip found @ 0x%x (%s) is not an sc450ai chip.\n",
                  client->addr, client->adapter->name);
        return ret;
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    ISP_WARNING("===================================================\n");
    ISP_WARNING("Template version is %s\n", TVERSION);
    ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
    ISP_WARNING("Sensor name is %s\n", sc450ai_attr.name);
    ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
    ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
    ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
    ISP_WARNING("===================================================\n");

    if (chip) {
        memcpy(chip->name, "sc450ai", sizeof("sc450ai"));
        chip->ident = ident;
        chip->revision = SENSOR_VERSION;
    }

    return 0;
}

static int sc450ai_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    return 0;
}

static int sc450ai_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
        ret = sc450ai_attr_set(sd, wsize);
        sensor->video.state = TX_ISP_MODULE_DEINIT;
    }

    return ret;
}

static int sc450ai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
    ret = sc450ai_read(sd, reg->reg & 0xffff, &val);
    reg->val = val;
    reg->size = 2;

    return ret;
}

static int sc450ai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
    int len = 0;

    len = strlen(sd->chip.name);
    if (len && strncmp(sd->chip.name, reg->name, len)) {
        return -EINVAL;
    }
    if (!private_capable(CAP_SYS_ADMIN))
        return -EPERM;
    sc450ai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

    return 0;
}

static int sc450ai_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = ISP_SUCCESS;

    if (init->enable) {
        if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
            ret = sc450ai_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
            if (ret)
                return ret;
            sensor->video.state = TX_ISP_MODULE_INIT;
        }
        if (sensor->video.state == TX_ISP_MODULE_INIT) {
            ret = sc450ai_write_array(sd, sc450ai_stream_on_mipi);
            sensor->video.state = TX_ISP_MODULE_RUNNING;
            pr_debug("sc450ai stream on\n");
        }

    } else {
        ret = sc450ai_write_array(sd, sc450ai_stream_off_mipi);
        sensor->video.state = TX_ISP_MODULE_INIT;
        pr_debug("sc450ai stream off\n");
    }

    return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc450ai_set_expo(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;
    int it = value & 0xffff;
    int again = (value & 0xffff0000) >> 16;

    it = it << 1;
    ret += sc450ai_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
    ret += sc450ai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
    ret += sc450ai_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

    ret += sc450ai_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
    ret += sc450ai_write(sd, 0x3e09, (unsigned char)(again & 0xff));

    return ret;
}
#else
static int sc450ai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    value = value << 1;
    ret += sc450ai_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
    ret += sc450ai_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
    ret += sc450ai_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

    return ret;
}

static int sc450ai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret += sc450ai_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
    ret += sc450ai_write(sd, 0x3e09, (unsigned char)(value & 0xff));

    return ret;
}
#endif /* SENSOR_EXPO */

static int sc450ai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int sc450ai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int sc450ai_set_mode(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    if (wsize) {
        ret = sc450ai_attr_set(sd, wsize);
    }

    return ret;
}

static int sc450ai_set_fps(struct tx_isp_subdev *sd, int fps)
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
        sclk = 900 * 1600 * 30;  /**< HTS * VTS * FPS */
        max_fps = wsize->fps;
        break;
    case 1:
        sclk = 750 * 1560 * 30;  /**< HTS * VTS * FPS */
        max_fps = wsize->fps;
        break;
    case 2:
        sclk = 750 * 1560 * 30;  /**< HTS * VTS * FPS */
        max_fps = wsize->fps;
        break;
    case 3:
        sclk = 936 * 780 * 120;  /**< HTS * VTS * FPS */
        max_fps = wsize->fps;
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
    ret += sc450ai_read(sd, 0x320c, &val);
    hts = val << 8;
    val = 0;
    ret += sc450ai_read(sd, 0x320d, &val);
    hts |= val;
    if (0 != ret) {
        ISP_ERROR("err: sc450ai read err\n");
        return ret;
    }

    vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

    sc450ai_write(sd, 0x320f, (unsigned char) (vts & 0xff));
    sc450ai_write(sd, 0x320e, (unsigned char) (vts >> 8));

    if (0 != ret) {
        ISP_ERROR("err: sc450ai_write err\n");
        return ret;
    }

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

static int sc450ai_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
    struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    int ret = ISP_SUCCESS;
    unsigned char val = 0;

    par->drop_frame = 0;
    par->reset = 0;

    /* 2'b01:mirror,2'b10:filp */
    ret = sc450ai_read(sd, 0x3221, &val);
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
    ret += sc450ai_write(sd, 0x3221, val);

    sensor->video.hvflip_mode = par->hvflip;
    sc450ai_attr_set(sd, wsize);

    return ret;
#else
    return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc450ai_set_expo_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#else
static int sc450ai_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}

static int sc450ai_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#endif /* SENSOR_EXPO */

static int sc450ai_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    int ret = ISP_SUCCESS;

    ret = sc450ai_write_array(sd, sc450ai_stream_off_mipi);
    if (par->wdr_en == 1) {
        if (par->boot == -1) {
            info->default_boot = 1;
        }
        sc450ai_setting_select(sd, 1);
        sc450ai_attr_set(sd, wsize);
    } else if (par->wdr_en == 0) {
        if (par->boot == -1) {
            info->default_boot = 0;
        }
        sc450ai_setting_select(sd, 0);
        sc450ai_attr_set(sd, wsize);
    } else {
        ISP_ERROR("Can not support this data type!!!");
        return -1;
    }

    return 0;
}

static int sc450ai_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    struct tx_isp_sensor_register_info *info = &sensor->info;
    int ret = ISP_SUCCESS;

    private_gpio_direction_output(info->rst_gpio, 0);
    private_msleep(1);
    private_gpio_direction_output(info->rst_gpio, 1);
    private_msleep(1);

    ret = sc450ai_write_array(sd, wsize->regs);
    ret = sc450ai_write_array(sd, sc450ai_stream_on_mipi);

    return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc450ai_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;
	if(data->cmd == TX_SENSOR_PM_RESUME){
		ret = sc450ai_write_array(sd, sc450ai_stream_on_mipi);
		if(ret != 0){
			printk("%s streamon failed\n",sc450ai_attr.name);
			return ret;
		}
		printk("%s TX_SENSOR_PM_RESUME\n",sc450ai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
		ret = sc450ai_write_array(sd, sc450ai_stream_off_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_SUSPEND.\n",sc450ai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_PREPARE) {
		printk("%s TX_SENSOR_PM_PREPARE.\n",sc450ai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
		printk("%s TX_SENSOR_PM_COMPLETE.\n",sc450ai_attr.name);

	}else if(data->cmd == TX_SENSOR_PM_STREAMON) {
		ret = sc450ai_write_array(sd, sc450ai_stream_on_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMON.\n",sc450ai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
		ret = sc450ai_write_array(sd, sc450ai_stream_off_mipi);
		if (ret < 0) {
			printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMOFF.\n",sc450ai_attr.name);
	}else {
		printk("[%s]Don't Support this function.\n",__func__);
		return -EINVAL;
	}

	return ret;
}

static int sc450ai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
            ret = sc450ai_set_expo(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME:
        if (arg)
            ret = sc450ai_set_integration_time(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN:
        if (arg)
            ret = sc450ai_set_analog_gain(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_DGAIN:
        if (arg)
            ret = sc450ai_set_digital_gain(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
        if (arg)
            ret = sc450ai_get_black_pedestal(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_RESIZE:
        if (arg)
            ret = sc450ai_set_mode(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
        if (arg)
            ret = sc450ai_write_array(sd, sc450ai_stream_off_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
        if (arg)
            ret = sc450ai_write_array(sd, sc450ai_stream_on_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FPS:
        if (arg)
            ret = sc450ai_set_fps(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_HVFLIP:
        if(arg)
            ret = sc450ai_set_hvflip(sd, (void *)sensor_val->value);
        break;
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret = sc450ai_pm_s_stream_ctrl(sd, arg);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
    case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
        if (arg)
            ret = sc450ai_set_expo_short(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
        if(arg)
            ret = sc450ai_set_integration_time_short(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
        if(arg)
            ret = sc450ai_set_analog_gain_short(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_WDR:
        if(arg)
            ret = sc450ai_set_wdr(sd, (void *)sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_WDR_STOP:
        if(arg)
            ret = sc450ai_set_wdr_stop(sd, (void *)sensor_val->value);
        break;
#endif /* SENSOR_WDR_2_FRAME */
    default:
        break;
    }

    return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc450ai_core_ops = {
    .g_chip_ident = sc450ai_g_chip_ident,
    .reset = sc450ai_reset,
    .init = sc450ai_init,
    .g_register = sc450ai_g_register,
    .s_register = sc450ai_s_register,
};

static struct tx_isp_subdev_video_ops sc450ai_video_ops = {
    .s_stream = sc450ai_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc450ai_sensor_ops = {
#ifndef SENSOR_TEST
    .ioctl  = sc450ai_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc450ai_ops = {
    .core = &sc450ai_core_ops,
    .video = &sc450ai_video_ops,
    .sensor = &sc450ai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
    .name = "sc450ai",
    .id = -1,
    .dev = {
        .dma_mask = &tx_isp_module_dma_mask,
        .coherent_dma_mask = 0xffffffff,
        .platform_data = NULL,
    },
    .num_resources = 0,
};

static int sc450ai_probe(struct i2c_client *client,
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
    sensor->video.attr = &sc450ai_attr;
    sensor->dev = &client->dev;
    tx_isp_subdev_init(&sensor_platform_device, sd, &sc450ai_ops);
    tx_isp_set_subdevdata(sd, client);
    tx_isp_set_subdev_hostdata(sd, sensor);
    private_i2c_set_clientdata(client, sd);

    pr_debug("probe ok ------->sc450ai\n");

    return 0;
}

static int sc450ai_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc450ai_id[] = {
    {"sc450ai", 0},
    {}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc450ai_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc450ai_driver = {
    .driver = {
#ifdef CONFIG_ZERATUL
        .owner  = NULL,
#else
        .owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
        .name   = "sc450ai",
    },
    .probe          = sc450ai_probe,
    .remove         = sc450ai_remove,
    .id_table       = sc450ai_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc450ai(void) {
    return private_i2c_add_driver(&sc450ai_driver);
}

static __exit void exit_sc450ai(void) {
    private_i2c_del_driver(&sc450ai_driver);
}

module_init(init_sc450ai);
module_exit(exit_sc450ai);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc450ai(void) {
    return private_i2c_add_driver(&sc450ai_driver);
}

static void exit_first_sc450ai(void) {
    private_i2c_del_driver(&sc450ai_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
    .name = "sc450ai",
    .i2c_addr = 0x30,
    .width = 0,
    .height = 0,
#ifdef SENSOR_WDR_2_FRAME
    .wdr = 1,
#else
    .wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
    .init_sensor = init_first_sc450ai,
    .exit_sensor = exit_first_sc450ai
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for sc450ai sensor");
MODULE_LICENSE("GPL");
