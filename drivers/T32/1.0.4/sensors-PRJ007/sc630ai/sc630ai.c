/*
 * sc630ai.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           2560*1440       30        mipi_2lane    linear  12        30
 *  0           2560*1440       30        mipi_2lane    dag     12        30
 *
 * @I2C addr:0x30, 0x36
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
#define SENSOR_VERSION  "H20241108a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xce)
#define SENSOR_CHIP_ID_L    (0x47)
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

struct tx_isp_sensor_attribute sc630ai_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc630ai_again_lut[] = {
    {0x0040, 0},
	{0x0041, 1501},
	{0x0042, 2886},
	{0x0043, 4343},
	{0x0044, 5776},
	{0x0045, 7101},
	{0x0046, 8494},
	{0x0047, 9782},
	{0x0048, 11136},
	{0x0049, 12471},
	{0x004a, 13706},
	{0x004b, 15006},
	{0x004c, 16288},
	{0x004d, 17474},
	{0x004e, 18724},
	{0x004f, 19880},
	{0x0050, 21098},
	{0x0051, 22300},
	{0x0052, 23414},
	{0x0053, 24588},
	{0x0054, 25747},
	{0x0055, 26821},
	{0x0056, 27953},
	{0x0057, 29003},
	{0x0058, 30109},
	{0x0059, 31203},
	{0x005a, 32217},
	{0x005b, 33287},
	{0x005c, 34345},
	{0x005d, 35326},
	{0x005e, 36362},
	{0x005f, 37322},
	{0x0060, 38336},
	{0x0061, 39339},
	{0x0062, 40270},
	{0x0063, 41253},
	{0x0064, 42226},
	{0x0065, 43129},
	{0x0066, 44083},
	{0x0067, 44968},
	{0x0068, 45904},
	{0x0069, 46830},
	{0x006a, 47691},
	{0x006b, 48600},
	{0x006c, 49500},
	{0x006d, 50337},
	{0x006e, 51221},
	{0x006f, 52042},
	{0x0070, 52911},
	{0x0071, 53771},
	{0x0072, 54571},
	{0x0073, 55417},
	{0x0074, 56255},
	{0x0075, 57034},
	{0x0076, 57858},
	{0x0077, 58624},
	{0x0078, 59434},
	{0x0079, 60237},
	{0x007a, 60984},
	{0x007b, 61775},
	{0x007c, 62559},
	{0x007d, 63288},
	{0x007e, 64059},
	{0x007f, 64777},
	{0x0140, 65536},
	{0x0141, 66990},
	{0x0142, 68468},
	{0x0143, 69879},
	{0x0144, 71268},
	{0x0145, 72637},
	{0x0146, 74030},
	{0x0147, 75360},
	{0x0148, 76672},
	{0x0149, 77966},
	{0x014a, 79283},
	{0x014b, 80542},
	{0x014c, 81784},
	{0x014d, 83010},
	{0x014e, 84260},
	{0x014f, 85454},
	{0x0150, 86634},
	{0x0151, 87799},
	{0x0152, 88987},
	{0x0153, 90124},
	{0x0154, 91247},
	{0x0155, 92357},
	{0x0156, 93489},
	{0x0157, 94573},
	{0x0158, 95645},
	{0x0159, 96705},
	{0x015a, 97787},
	{0x015b, 98823},
	{0x015c, 99848},
	{0x015d, 100862},
	{0x015e, 101898},
	{0x015f, 102890},
	{0x0160, 103872},
	{0x0161, 104844},
	{0x0162, 105837},
	{0x0163, 106789},
	{0x0164, 107732},
	{0x0165, 108665},
	{0x0166, 109619},
	{0x0167, 110534},
	{0x0168, 111440},
	{0x0169, 112338},
	{0x016a, 113255},
	{0x016b, 114136},
	{0x016c, 115008},
	{0x016d, 115873},
	{0x016e, 116757},
	{0x016f, 117606},
	{0x0170, 118447},
	{0x0171, 119281},
	{0x0172, 120134},
	{0x0173, 120953},
	{0x0174, 121765},
	{0x0175, 122570},
	{0x0176, 123394},
	{0x0177, 124185},
	{0x0178, 124970},
	{0x0179, 125748},
	{0x017a, 126545},
	{0x017b, 127311},
	{0x017c, 128070},
	{0x017d, 128824},
	{0x017e, 129595},
	{0x017f, 130336},
	{0x0340, 131072},
	{0x0341, 132550},
	{0x0342, 133981},
	{0x0343, 135415},
	{0x0344, 136804},
	{0x0345, 138195},
	{0x0346, 139545},
	{0x0347, 140896},
	{0x0348, 142208},
	{0x0349, 143523},
	{0x034a, 144799},
	{0x034b, 146078},
	{0x034c, 147320},
	{0x034d, 148566},
	{0x034e, 149776},
	{0x034f, 150990},
	{0x0350, 152170},
	{0x0351, 153354},
	{0x0352, 154505},
	{0x0353, 155660},
	{0x0354, 156783},
	{0x0355, 157911},
	{0x0356, 159008},
	{0x0357, 160109},
	{0x0358, 161181},
	{0x0359, 162258},
	{0x035a, 163306},
	{0x035b, 164359},
	{0x035c, 165384},
	{0x035d, 166414},
	{0x035e, 167418},
	{0x035f, 168426},
	{0x0360, 169408},
	{0x0361, 170396},
	{0x0362, 171358},
	{0x0363, 172325},
	{0x0364, 173268},
	{0x0365, 174216},
	{0x0366, 175140},
	{0x0367, 176070},
	{0x0368, 176976},
	{0x0369, 177888},
	{0x036a, 178777},
	{0x8040, 179133},
	{0x8041, 180600},
	{0x8042, 182045},
	{0x8043, 183468},
	{0x8044, 184870},
	{0x8045, 186252},
	{0x8046, 187600},
	{0x8047, 188943},
	{0x8048, 190266},
	{0x8049, 191572},
	{0x804a, 192859},
	{0x804b, 194129},
	{0x804c, 195383},
	{0x804d, 196620},
	{0x804e, 197841},
	{0x804f, 199046},
	{0x8050, 200237},
	{0x8051, 201401},
	{0x8052, 202562},
	{0x8053, 203709},
	{0x8054, 204843},
	{0x8055, 205963},
	{0x8056, 207069},
	{0x8057, 208163},
	{0x8058, 209245},
	{0x8059, 210314},
	{0x805a, 211372},
	{0x805b, 212407},
	{0x805c, 213442},
	{0x805d, 214465},
	{0x805e, 215477},
	{0x805f, 216478},
	{0x8060, 217469},
	{0x8061, 218450},
	{0x8062, 219420},
	{0x8063, 220381},
	{0x8064, 221332},
	{0x8065, 222274},
	{0x8066, 223197},
	{0x8067, 224121},
	{0x8068, 225035},
	{0x8069, 225941},
	{0x806a, 226838},
	{0x806b, 227726},
	{0x806c, 228606},
	{0x806d, 229479},
	{0x806e, 230343},
	{0x806f, 231199},
	{0x8070, 232048},
	{0x8071, 232881},
	{0x8072, 233715},
	{0x8073, 234541},
	{0x8074, 235361},
	{0x8075, 236173},
	{0x8076, 236978},
	{0x8077, 237777},
	{0x8078, 238569},
	{0x8079, 239354},
	{0x807a, 240133},
	{0x807b, 240898},
	{0x807c, 241665},
	{0x807d, 242425},
	{0x807e, 243179},
	{0x807f, 243927},
	{0x8140, 244669},
	{0x8141, 246136},
	{0x8142, 247581},
	{0x8143, 248997},
	{0x8144, 250399},
	{0x8145, 251781},
	{0x8146, 253143},
	{0x8147, 254485},
	{0x8148, 255808},
	{0x8149, 257108},
	{0x814a, 258395},
	{0x814b, 259665},
	{0x814c, 260919},
	{0x814d, 262156},
	{0x814e, 263371},
	{0x814f, 264577},
	{0x8150, 265767},
	{0x8151, 266943},
	{0x8152, 268104},
	{0x8153, 269245},
	{0x8154, 270379},
	{0x8155, 271499},
	{0x8156, 272605},
	{0x8157, 273700},
	{0x8158, 274781},
	{0x8159, 275845},
	{0x815a, 276903},
	{0x815b, 277948},
	{0x815c, 278982},
	{0x815d, 280006},
	{0x815e, 281013},
	{0x815f, 282014},
	{0x8160, 283005},
	{0x8161, 283986},
	{0x8162, 284956},
	{0x8163, 285913},
	{0x8164, 286864},
	{0x8165, 287805},
	{0x8166, 288738},
	{0x8167, 289661},
	{0x8168, 290575},
	{0x8169, 291477},
	{0x816a, 292374},
	{0x816b, 293262},
	{0x816c, 294142},
	{0x816d, 295015},
	{0x816e, 295875},
	{0x816f, 296731},
	{0x8170, 297580},
	{0x8171, 298421},
	{0x8172, 299255},
	{0x8173, 300077},
	{0x8174, 300897},
	{0x8175, 301709},
	{0x8176, 302514},
	{0x8177, 303313},
	{0x8178, 304105},
	{0x8179, 304887},
	{0x817a, 305665},
	{0x817b, 306438},
	{0x817c, 307204},
	{0x817d, 307964},
	{0x817e, 308715},
	{0x817f, 309463},
	{0x8340, 310205},
	{0x8341, 311672},
	{0x8342, 313114},
	{0x8343, 314537},
	{0x8344, 315939},
	{0x8345, 317317},
	{0x8346, 318679},
	{0x8347, 320018},
	{0x8348, 321341},
	{0x8349, 322647},
	{0x834a, 323931},
	{0x834b, 325201},
	{0x834c, 326455},
	{0x834d, 327689},
	{0x834e, 328910},
	{0x834f, 330113},
	{0x8350, 331303},
	{0x8351, 332479},
	{0x8352, 333637},
	{0x8353, 334784},
	{0x8354, 335917},
	{0x8355, 337035},
	{0x8356, 338141},
	{0x8357, 339233},
	{0x8358, 340314},
	{0x8359, 341384},
	{0x835a, 342439},
	{0x835b, 343484},
	{0x835c, 344518},
	{0x835d, 345539},
	{0x835e, 346551},
	{0x835f, 347550},
	{0x8360, 348541},
	{0x8361, 349522},
	{0x8362, 350490},
	{0x8363, 351451},
	{0x8364, 352402},
	{0x8365, 353341},
	{0x8366, 354274},
	{0x8367, 355195},
	{0x8368, 356109},
	{0x8369, 357015},
	{0x836a, 357910},
	{0x836b, 358798},
	{0x836c, 359678},
	{0x836d, 360549},
	{0x836e, 361413},
	{0x836f, 362267},
	{0x8370, 363116},
	{0x8371, 363957},
	{0x8372, 364789},
	{0x8373, 365615},
	{0x8374, 366435},
	{0x8375, 367245},
	{0x8376, 368050},
	{0x8377, 368847},
	{0x8378, 369639},
	{0x8379, 370424},
	{0x837a, 371201},
	{0x837b, 371974},
	{0x837c, 372740},
	{0x837d, 373499},
	{0x837e, 374253},
	{0x837f, 374999},
	{0x8740, 375741},
	{0x8741, 377207},
	{0x8742, 378651},
	{0x8743, 380073},
	{0x8744, 381473},
	{0x8745, 382853},
	{0x8746, 384215},
	{0x8747, 385555},
	{0x8748, 386877},
	{0x8749, 388181},
	{0x874a, 389469},
	{0x874b, 390737},
	{0x874c, 391989},
	{0x874d, 393225},
	{0x874e, 394446},
	{0x874f, 395650},
	{0x8750, 396839},
	{0x8751, 398013},
	{0x8752, 399174},
	{0x8753, 400320},
	{0x8754, 401452},
	{0x8755, 402571},
	{0x8756, 403677},
	{0x8757, 404770},
	{0x8758, 405850},
	{0x8759, 406918},
	{0x875a, 407976},
	{0x875b, 409020},
	{0x875c, 410053},
	{0x875d, 411075},
	{0x875e, 412087},
	{0x875f, 413087},
	{0x8760, 414077},
	{0x8761, 415057},
	{0x8762, 416027},
	{0x8763, 416987},
	{0x8764, 417937},
	{0x8765, 418877},
	{0x8766, 419810},
	{0x8767, 420732},
	{0x8768, 421645},
	{0x8769, 422550},
	{0x876a, 423447},
	{0x876b, 424334},
	{0x876c, 425213},
	{0x876d, 426085},
	{0x876e, 426949},
	{0x876f, 427804},
	{0x8770, 428652},
	{0x8771, 429492},
	{0x8772, 430326},
	{0x8773, 431151},
	{0x8774, 431970},
	{0x8775, 432781},
	{0x8776, 433586},
	{0x8777, 434384},
	{0x8778, 435175},
	{0x8779, 435960},
	{0x877a, 436738},
	{0x877b, 437510},
	{0x877c, 438275},
	{0x877d, 439035},
	{0x877e, 439789},
	{0x877f, 440536},
};

struct again_lut sc630ai_again_dag_lut[] = {
    {0x0040, 0},
	{0x0041, 1501},
	{0x0042, 2886},
	{0x0043, 4343},
	{0x0044, 5776},
	{0x0045, 7101},
	{0x0046, 8494},
	{0x0047, 9782},
	{0x0048, 11136},
	{0x0049, 12471},
	{0x004a, 13706},
	{0x004b, 15006},
	{0x004c, 16288},
	{0x004d, 17474},
	{0x004e, 18724},
	{0x004f, 19880},
	{0x0050, 21098},
	{0x0051, 22300},
	{0x0052, 23414},
	{0x0053, 24588},
	{0x0054, 25747},
	{0x0055, 26821},
	{0x0056, 27953},
	{0x0057, 29003},
	{0x0058, 30109},
	{0x0059, 31203},
	{0x005a, 32217},
	{0x005b, 33287},
	{0x005c, 34345},
	{0x005d, 35326},
	{0x005e, 36362},
	{0x005f, 37322},
	{0x0060, 38336},
	{0x0061, 39339},
	{0x0062, 40270},
	{0x0063, 41253},
	{0x0064, 42226},
	{0x0065, 43129},
	{0x0066, 44083},
	{0x0067, 44968},
	{0x0068, 45904},
	{0x0069, 46830},
	{0x006a, 47691},
	{0x006b, 48600},
	{0x006c, 49500},
	{0x006d, 50337},
	{0x006e, 51221},
	{0x006f, 52042},
	{0x0070, 52911},
	{0x0071, 53771},
	{0x0072, 54571},
	{0x0073, 55417},
	{0x0074, 56255},
	{0x0075, 57034},
	{0x0076, 57858},
	{0x0077, 58624},
	{0x0078, 59434},
	{0x0079, 60237},
	{0x007a, 60984},
	{0x007b, 61775},
	{0x007c, 62559},
	{0x007d, 63288},
	{0x007e, 64059},
	{0x007f, 64777},
	{0x0140, 65536},
	{0x0141, 66990},
	{0x0142, 68468},
	{0x0143, 69879},
	{0x0144, 71268},
	{0x0145, 72637},
	{0x0146, 74030},
	{0x0147, 75360},
	{0x0148, 76672},
	{0x0149, 77966},
	{0x014a, 79283},
	{0x014b, 80542},
	{0x014c, 81784},
	{0x014d, 83010},
	{0x014e, 84260},
	{0x014f, 85454},
	{0x0150, 86634},
	{0x0151, 87799},
	{0x0152, 88987},
	{0x0153, 90124},
	{0x0154, 91247},
	{0x0155, 92357},
	{0x0156, 93489},
	{0x0157, 94573},
	{0x0158, 95645},
	{0x0159, 96705},
	{0x015a, 97787},
	{0x015b, 98823},
	{0x015c, 99848},
	{0x015d, 100862},
	{0x015e, 101898},
	{0x015f, 102890},
	{0x0160, 103872},
	{0x0161, 104844},
	{0x0162, 105837},
	{0x0163, 106789},
	{0x0164, 107732},
	{0x0165, 108665},
	{0x0166, 109619},
	{0x0167, 110534},
	{0x0168, 111440},
	{0x0169, 112338},
	{0x016a, 113255},
	{0x016b, 114136},
	{0x016c, 115008},
	{0x016d, 115873},
	{0x016e, 116757},
	{0x016f, 117606},
	{0x0170, 118447},
	{0x0171, 119281},
	{0x0172, 120134},
	{0x0173, 120953},
	{0x0174, 121765},
	{0x0175, 122570},
	{0x0176, 123394},
	{0x0177, 124185},
	{0x0178, 124970},
	{0x0179, 125748},
	{0x017a, 126545},
	{0x017b, 127311},
	{0x017c, 128070},
	{0x017d, 128824},
	{0x017e, 129595},
	{0x017f, 130336},
	{0x0340, 131072},
	{0x0341, 132550},
	{0x0342, 133981},
	{0x0343, 135415},
	{0x0344, 136804},
	{0x0345, 138195},
	{0x0346, 139545},
	{0x0347, 140896},
	{0x0348, 142208},
	{0x0349, 143523},
	{0x034a, 144799},
	{0x034b, 146078},
	{0x034c, 147320},
	{0x034d, 148566},
	{0x034e, 149776},
	{0x034f, 150990},
	{0x0350, 152170},
	{0x0351, 153354},
	{0x0352, 154505},
	{0x0353, 155660},
	{0x0354, 156783},
	{0x0355, 157911},
	{0x0356, 159008},
	{0x0357, 160109},
	{0x0358, 161181},
	{0x0359, 162258},
	{0x035a, 163306},
	{0x035b, 164359},
	{0x035c, 165384},
	{0x035d, 166414},
	{0x035e, 167418},
	{0x035f, 168426},
	{0x0360, 169408},
	{0x0361, 170396},
	{0x0362, 171358},
	{0x0363, 172325},
	{0x0364, 173268},
	{0x0365, 174216},
	{0x0366, 175140},
	{0x0367, 176070},
	{0x0368, 176976},
	{0x0369, 177888},
	{0x036a, 178777},
	{0x036b, 179672},
	{0x036c, 180544},
	{0x036d, 181423},
	{0x036e, 182279},
	{0x036f, 183142},
	{0x0370, 183983},
	{0x0371, 184830},
	{0x0372, 185656},
	{0x0373, 186489},
	{0x0374, 187301},
	{0x0375, 188119},
	{0x0376, 188917},
	{0x0377, 189721},
	{0x0378, 190506},
	{0x0379, 191297},
	{0x037a, 192069},
	{0x037b, 192847},
	{0x037f, 195872},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc630ai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc630ai_again_lut;
	while (lut->gain <= sc630ai_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc630ai_attr.again.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc630ai_alloc_again_dag(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc630ai_again_dag_lut;
	while (lut->gain <= sc630ai_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc630ai_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc630ai_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc630ai_again_lut;
	while(lut->gain <= sc630ai_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc630ai_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc630ai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc630ai_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc630ai_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc630ai_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1024,
	.lans = 2,
	.image_twidth = 2560,
	.image_theight = 1440,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sc630ai_mipi_DAG = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1275,
	.lans = 2,
	.image_twidth = 2560,
	.image_theight = 1440,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_dvp_bus sc630ai_dvp = {
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

struct tx_isp_sensor_attribute sc630ai_attr = {
	.name = "sc630ai",
	.chip_id = 0xce47,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.sensor_ctrl.alloc_again = sc630ai_alloc_again,
	.sensor_ctrl.alloc_dgain = sc630ai_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc630ai_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc630ai_init_regs_2560_1440_30fps_mipi[] = {
    //Cleaned_0x61_SC630AI_MIPI_24Minput_2lane_12bit_1024Mbps_2560x1440_30fps
    //VTS=1600.000000,HTS=3555.555556,SCLK=108.000000,PCLK=170.666667,MipiCLK=1024.000000,Tline=20.833333us,TExp_step=1.0*Tline=20.833333us,TExp_offset=2.703704us
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x37f9,0x80},
    {0x2340,0xfc},
    {0x2341,0xcc},
    {0x2345,0x06},
    {0x2346,0xcc},
    {0x2374,0xa0},
    {0x2375,0xe0},
    {0x3018,0x3a},
    {0x3019,0x0c},
    {0x301f,0x61},
    {0x3031,0x0c},
    {0x3200,0x00},
    {0x3201,0x00},
    {0x3202,0x02},
    {0x3203,0x30},
    {0x3204,0x0a},
    {0x3205,0x0f},
    {0x3206,0x07},
    {0x3207,0xdf},
    {0x3208,0x0a},
    {0x3209,0x00},
    {0x320a,0x05},
    {0x320b,0xa0},
    {0x320c,0x08},//hts = 0x8ca = 2250
    {0x320d,0xca},//
    {0x320e,0x06},//vts = 0x640 = 1600
    {0x320f,0x40},//
    {0x3210,0x00},
    {0x3211,0x08},
    {0x3212,0x00},
    {0x3213,0x08},
    {0x3280,0x07},
    {0x3301,0x08},
    {0x3304,0x30},
    {0x3305,0x00},
    {0x3306,0x80},
    {0x3309,0x50},
    {0x330a,0x00},
    {0x330b,0xd0},
    {0x331e,0x21},
    {0x331f,0x41},
    {0x3333,0x10},
    {0x3334,0x40},
    {0x3338,0x40},
    {0x3364,0x5e},
    {0x336d,0x23},
    {0x3390,0x20},
    {0x3391,0x60},
    {0x3392,0x60},
    {0x3393,0x08},
    {0x3394,0x08},
    {0x3395,0x08},
    {0x3396,0xa0},
    {0x3397,0xe0},
    {0x3398,0xe0},
    {0x3399,0x0d},
    {0x339a,0x0d},
    {0x339b,0x24},
    {0x339c,0x24},
    {0x33ae,0x20},
    {0x33af,0x41},
    {0x33b2,0x60},
    {0x33b3,0x20},
    {0x33f8,0x00},
    {0x33f9,0x90},
    {0x33fa,0x00},
    {0x33fb,0x90},
    {0x33fc,0xa0},
    {0x33fd,0xe0},
    {0x349f,0x03},
    {0x34a6,0xa0},
    {0x34a7,0xe0},
    {0x34a8,0x14},
    {0x34a9,0x14},
    {0x34aa,0x00},
    {0x34ab,0xe0},
    {0x34ac,0x00},
    {0x34ad,0xe0},
    {0x34f8,0xe0},
    {0x34f9,0x14},
    {0x3630,0x77},
    {0x3633,0x43},
    {0x3636,0x80},
    {0x3637,0x55},
    {0x363a,0x06},
    {0x363c,0xc1},
    {0x363d,0x41},
    {0x363f,0x07},
    {0x3648,0xb9},
    {0x364d,0x3a},
    {0x3670,0x48},
    {0x3690,0x43},
    {0x3691,0x44},
    {0x3692,0x54},
    {0x369c,0x80},
    {0x369d,0xe8},
    {0x36b1,0x98},
    {0x36b2,0x01},
    {0x36d0,0x20},
    {0x36d1,0x20},
    {0x36d2,0x60},
    {0x36d3,0x80},
    {0x36d4,0xa0},
    {0x36d5,0xe0},
    {0x36d6,0xe8},
    {0x36d7,0x01},
    {0x36d8,0x04},
    {0x36d9,0x09},
    {0x36da,0x05},
    {0x36db,0x0b},
    {0x36dc,0x14},
    {0x36dd,0x29},
    {0x36e0,0x10},
    {0x36e1,0x14},
    {0x36e2,0x18},
    {0x36e3,0xa0},
    {0x36e4,0xe0},
    {0x36ea,0x10},
    {0x36eb,0x0d},
    {0x36ec,0x8b},
    {0x36ed,0x98},
    {0x370f,0x21},
    {0x3722,0x40},
    {0x3725,0xa4},
    {0x3728,0x80},
    {0x3771,0x80},
    {0x3772,0x86},
    {0x3773,0x86},
    {0x377a,0x80},
    {0x377b,0xa0},
    {0x37b0,0xaf},
    {0x37b1,0xaf},
    {0x37b2,0xaf},
    {0x37b3,0x80},
    {0x37b4,0xa0},
    {0x37fa,0x09},
    {0x37fb,0x31},
    {0x37fc,0x00},
    {0x37fd,0x18},
    {0x3903,0x40},
    {0x3905,0x8d},
    {0x3907,0x01},
    {0x3908,0x11},
    {0x391f,0x64},
    {0x3933,0x80},
    {0x3934,0x12},
    {0x3937,0x68},
    {0x3939,0x00},
    {0x393a,0x00},
    {0x3e00,0x00},
    {0x3e01,0x63},
    {0x3e02,0xa0},
    {0x3e25,0x80},
    {0x3e26,0x7f},
    {0x3f09,0x28},
    {0x4424,0x02},
    {0x4503,0x10},
    {0x450d,0x20},
    {0x4837,0x1f},
    {0x5007,0x80},
    {0x5799,0x46},
    {0x57d9,0x46},
    {0x5ae0,0xfe},
    {0x5ae1,0x40},
    {0x5ae2,0x38},
    {0x5ae3,0x30},
    {0x5ae4,0x0c},
    {0x5ae5,0x38},
    {0x5ae6,0x30},
    {0x5ae7,0x28},
    {0x5ae8,0x3f},
    {0x5ae9,0x34},
    {0x5aea,0x2c},
    {0x5aeb,0x3f},
    {0x5aec,0x34},
    {0x5aed,0x2c},
    {0x5aee,0xfe},
    {0x5aef,0x40},
    {0x5af4,0x38},
    {0x5af5,0x30},
    {0x5af6,0x28},
    {0x5af7,0x38},
    {0x5af8,0x30},
    {0x5af9,0x28},
    {0x5afa,0x3f},
    {0x5afb,0x34},
    {0x5afc,0x2c},
    {0x5afd,0x3f},
    {0x5afe,0x34},
    {0x5aff,0x2c},
    {0x36e9,0x44},
    {0x37f9,0x24},
    {0x0100,0x01},
    {SENSOR_REG_DELAY,0x05},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc630ai_init_regs_2560_1440_30fps_mipi_dag[] = {
    //Cleaned_0x62_SC630AI_MIPI_24Minput_2lane_12bit_1440Mbps_2560x1440_30fps_PG_HDR_combin_TM12bit
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x37f9,0x80},
    {0x2304,0x50},
    {0x2306,0x40},
    {0x2308,0x18},
    {0x2309,0x60},
    {0x230b,0x88},
    {0x2312,0xb0},
    {0x2315,0x4c},
    {0x231e,0x41},
    {0x231f,0x51},
    {0x2335,0x2c},
    {0x2337,0x30},
    {0x233d,0x12},
    {0x233f,0x10},
    {0x2370,0x30},
    {0x2371,0x18},
    {0x3018,0x3a},
    {0x3019,0x0c},
    {0x301f,0x62},
    {0x3031,0x0c},
    {0x3200,0x00},
    {0x3201,0x00},
    {0x3202,0x02},
    {0x3203,0x30},
    {0x3204,0x0a},
    {0x3205,0x0f},
    {0x3206,0x07},
    {0x3207,0xdf},
    {0x3208,0x0a},
    {0x3209,0x00},
    {0x320a,0x05},
    {0x320b,0xa0},
    {0x320c,0x04},//hts = 0x4b0 = 1200
    {0x320d,0xb0},//
    {0x320e,0x0b},//vts = 0xbb8 = 3000
    {0x320f,0xb8},//
    {0x3210,0x00},
    {0x3211,0x08},
    {0x3212,0x00},
    {0x3213,0x08},
    {0x3251,0x90},
    {0x3281,0x80},
    {0x3301,0x16},
    {0x3302,0x10},
    {0x3303,0x10},
    {0x3304,0x50},
    {0x3306,0x28},
    {0x3307,0x03},
    {0x3308,0x18},
    {0x3309,0x60},
    {0x330b,0x50},
    {0x330d,0x0c},
    {0x330e,0x04},
    {0x330f,0x03},
    {0x3314,0x01},
    {0x3317,0x03},
    {0x331e,0x41},
    {0x331f,0x51},
    {0x3333,0x10},
    {0x3338,0x10},
    {0x3356,0x08},
    {0x3362,0x22},
    {0x3399,0x28},
    {0x33ad,0x2c},
    {0x33af,0x14},
    {0x33b2,0x30},
    {0x33b3,0x14},
    {0x33c0,0x12},
    {0x33c8,0x03},
    {0x33c9,0xf8},
    {0x33ca,0x00},
    {0x33cb,0x05},
    {0x3630,0x78},
    {0x3633,0x88},
    {0x3636,0x80},
    {0x3637,0x38},
    {0x363a,0x00},
    {0x363c,0xc1},
    {0x363d,0x41},
    {0x363f,0x09},
    {0x3648,0xb9},
    {0x364d,0x58},
    {0x36b1,0x4c},
    {0x36b2,0xd8},
    {0x36b3,0x01},
    {0x36d0,0x80},
    {0x36ea,0x0a},
    {0x36eb,0x0d},
    {0x36ec,0x8b},
    {0x36ed,0xd8},
    {0x3722,0x87},
    {0x3725,0xa4},
    {0x3728,0xa0},
    {0x3729,0x13},
    {0x37fa,0x09},
    {0x37fb,0x31},
    {0x37fc,0x00},
    {0x37fd,0x18},
    {0x3903,0x41},
    {0x3907,0x00},
    {0x3908,0x48},
    {0x391d,0x04},
    {0x391f,0x44},
    {0x3926,0x60},
    {0x3933,0x80},
    {0x3934,0x15},
    {0x3935,0x01},
    {0x3936,0x3a},
    {0x3937,0x61},
    {0x3938,0x67},
    {0x3939,0x00},
    {0x393a,0x00},
    {0x3963,0x08},
    {0x397b,0x10},
    {0x3993,0x20},
    {0x39dd,0x21},
    {0x39de,0x64},
    {0x3e00,0x00},
    {0x3e01,0x5d},
    {0x3e02,0x60},
    {0x3e25,0x00},
    {0x3e26,0x7f},
    {0x3e82,0x00},
    {0x3e83,0x40},
    {0x3f09,0x28},
    {0x4402,0x03},
    {0x4403,0x0b},
    {0x4404,0x21},
    {0x4405,0x2b},
    {0x440c,0x36},
    {0x440d,0x36},
    {0x440e,0x29},
    {0x440f,0x44},
    {0x4424,0x01},
    {0x4503,0x10},
    {0x450d,0x20},
    {0x4520,0x10},
    {0x4522,0xa0},
    {0x4837,0x16},
    {0x4853,0xf8},
    {0x5000,0x00},
    {0x5003,0x08},
    {0x5006,0x06},
    {0x5007,0x80},
    {0x5799,0x00},
    {0x57d9,0x00},
    {0x5ae0,0xfe},
    {0x5ae1,0x40},
    {0x5ae2,0x38},
    {0x5ae3,0x30},
    {0x5ae4,0x0c},
    {0x5ae5,0x38},
    {0x5ae6,0x30},
    {0x5ae7,0x28},
    {0x5ae8,0x3f},
    {0x5ae9,0x34},
    {0x5aea,0x2c},
    {0x5aeb,0x3f},
    {0x5aec,0x34},
    {0x5aed,0x2c},
    {0x5aee,0xfe},
    {0x5aef,0x40},
    {0x5af4,0x38},
    {0x5af5,0x30},
    {0x5af6,0x28},
    {0x5af7,0x38},
    {0x5af8,0x30},
    {0x5af9,0x28},
    {0x5afa,0x3f},
    {0x5afb,0x34},
    {0x5afc,0x2c},
    {0x5afd,0x3f},
    {0x5afe,0x34},
    {0x5aff,0x2c},
    {0x5b80,0x88},
    {0x5b83,0x01},
    {0x5b84,0x64},
    {0x5b8a,0x00},
    {0x36e9,0x53},
    {0x37f9,0x24},
    {0x0100,0x01},
    {SENSOR_REG_DELAY,0x05},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};
/*
 * the order of the sc630ai_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc630ai_win_sizes[] = {
	/* 2560*1440 [0] */
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR12_1X12,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc630ai_init_regs_2560_1440_30fps_mipi,
	},
	/* 2560*1440 [1] */
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR12_1X12,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc630ai_init_regs_2560_1440_30fps_mipi_dag,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sc630ai_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc630ai_stream_on_mipi[] = {
	{0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc630ai_stream_off_mipi[] = {
	{0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc630ai_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc630ai_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc630ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc630ai_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc630ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc630ai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc630ai_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc630ai_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc630ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc630ai_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc630ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc630ai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc630ai_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc630ai_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc630ai_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc630ai_win_sizes[0];
		sc630ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc630ai_attr.again.value = 65536;
		sc630ai_attr.again.max = 440536;
		sc630ai_attr.again.min = 0;
		sc630ai_attr.again.apply_delay = 2;

		sc630ai_attr.integration_time.value = 0xb60;
		sc630ai_attr.integration_time.max = 800 - 4;
		sc630ai_attr.integration_time.min = 1;
		sc630ai_attr.integration_time.apply_delay = 2;

		sc630ai_attr.total_width = 2250;
		sc630ai_attr.total_height = 1600;

		sc630ai_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc630ai_attr.hcg.base_gain = ;
		sc630ai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc630ai_attr.again_short.value = ;
		sc630ai_attr.again_short.max = ;
		sc630ai_attr.again_short.min = ;
		sc630ai_attr.again_short.apply_delay = ;

		sc630ai_attr.integration_time_short.value = ;
		sc630ai_attr.integration_time_short.max = ;
		sc630ai_attr.integration_time_short.min = ;
		sc630ai_attr.integration_time_short.apply_delay = ;

		sc630ai_attr.wdr_cache = wdr_line * sc630ai_attr.total_width;

#ifdef SENSOR_HCG
		sc630ai_attr.hcg_short.base_gain = ;
		sc630ai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc630ai_attr.mipi)), (void *)(&sc630ai_mipi_linear), sizeof(sc630ai_attr.mipi));
		break;
	case 1:
		wsize = &sc630ai_win_sizes[1];
		sc630ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc630ai_attr.again.value = 65536;
		sc630ai_attr.again.max = 195872;
		sc630ai_attr.again.min = 0;
		sc630ai_attr.again.apply_delay = 2;

		sc630ai_attr.integration_time.value = 0xb60;
		sc630ai_attr.integration_time.max = 1500 - 4;
		sc630ai_attr.integration_time.min = 1;
		sc630ai_attr.integration_time.apply_delay = 2;

		sc630ai_attr.total_width = 1200;
		sc630ai_attr.total_height = 3000;

		sc630ai_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc630ai_attr.hcg.base_gain = ;
		sc630ai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc630ai_attr.again_short.value = ;
		sc630ai_attr.again_short.max = ;
		sc630ai_attr.again_short.min = ;
		sc630ai_attr.again_short.apply_delay = ;

		sc630ai_attr.integration_time_short.value = ;
		sc630ai_attr.integration_time_short.max = ;
		sc630ai_attr.integration_time_short.min = ;
		sc630ai_attr.integration_time_short.apply_delay = ;

		sc630ai_attr.wdr_cache = wdr_line * sc630ai_attr.total_width;

#ifdef SENSOR_HCG
		sc630ai_attr.hcg_short.base_gain = ;
		sc630ai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc630ai_attr.mipi)), (void *)(&sc630ai_mipi_DAG), sizeof(sc630ai_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc630ai_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc630ai_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc630ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc630ai_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc630ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc630ai_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc630ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc630ai_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	sc630ai_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc630ai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc630ai_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc630ai_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc630ai_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc630ai_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc630ai_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "sc630ai_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = sc630ai_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc630ai chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc630ai_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc630ai", sizeof("sc630ai"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc630ai_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc630ai_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc630ai_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc630ai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc630ai_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc630ai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc630ai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc630ai_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc630ai_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc630ai_write_array(sd, sc630ai_stream_on_mipi);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc630ai stream on\n");
		}

	} else {
		ret = sc630ai_write_array(sd, sc630ai_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc630ai stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc630ai_set_expo(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += sc630ai_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc630ai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc630ai_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

	switch (info->default_boot) {
	case 0:
		ret += sc630ai_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
		ret += sc630ai_write(sd, 0x3e09, (unsigned char)(again & 0xff));
		break;
	case 1:
		/* high gain */
		ret += sc630ai_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
		ret += sc630ai_write(sd, 0x3e09, (unsigned char)(again & 0xff));
		/* low gain */
		ret += sc630ai_write(sd, 0x3e82, (unsigned char)((again >> 8) & 0xff));
		ret += sc630ai_write(sd, 0x3e83, (unsigned char)(again & 0xff));
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this sboot!!!\n");
    }

    return ret;
}
#else
static int sc630ai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc630ai_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc630ai_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc630ai_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

	return ret;
}

static int sc630ai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	switch (info->default_boot) {
	case 0:
		ret += sc630ai_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
		ret += sc630ai_write(sd, 0x3e09, (unsigned char)(value & 0xff));
		break;
	case 1:
		ret += sc630ai_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
		ret += sc630ai_write(sd, 0x3e09, (unsigned char)(value & 0xff));
		ret += sc630ai_write(sd, 0x3e82, (unsigned char)((value >> 8) & 0xff));
		ret += sc630ai_write(sd, 0x3e83, (unsigned char)(value & 0xff));
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this sboot!!!\n");
    }

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc630ai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc630ai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc630ai_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc630ai_attr_set(sd, wsize);
	}

	return ret;
}

static int sc630ai_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 2250 * 1600 * 30;  /**< HTS * VTS * FPS */
		max_fps = wsize->fps;
		break;
	case 1:
		sclk = 1200 * 3000 * 30;  /**< HTS * VTS * FPS */
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
	ret += sc630ai_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc630ai_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc630ai read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc630ai_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc630ai_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc630ai_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts/2 - 4;
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

static int sc630ai_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = sc630ai_read(sd, 0x3221, &val);
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
	ret += sc630ai_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc630ai_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc630ai_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc630ai_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc630ai_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc630ai_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc630ai_write_array(sd, sc630ai_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc630ai_setting_select(sd, 1);
		sc630ai_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc630ai_setting_select(sd, 0);
		sc630ai_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc630ai_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc630ai_write_array(sd, wsize->regs);
	ret = sc630ai_write_array(sd, sc630ai_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc630ai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = sc630ai_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc630ai_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc630ai_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc630ai_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc630ai_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc630ai_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc630ai_write_array(sd, sc630ai_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc630ai_write_array(sd, sc630ai_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc630ai_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc630ai_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc630ai_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc630ai_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc630ai_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc630ai_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc630ai_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc630ai_core_ops = {
	.g_chip_ident = sc630ai_g_chip_ident,
	.reset = sc630ai_reset,
	.init = sc630ai_init,
	.g_register = sc630ai_g_register,
	.s_register = sc630ai_s_register,
};

static struct tx_isp_subdev_video_ops sc630ai_video_ops = {
	.s_stream = sc630ai_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc630ai_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc630ai_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc630ai_ops = {
	.core = &sc630ai_core_ops,
	.video = &sc630ai_video_ops,
	.sensor = &sc630ai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "sc630ai",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc630ai_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc630ai_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc630ai_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc630ai\n");

	return 0;
}

static int sc630ai_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc630ai_id[] = {
	{"sc630ai", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc630ai_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc630ai_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc630ai",
	},
	.probe          = sc630ai_probe,
	.remove         = sc630ai_remove,
	.id_table       = sc630ai_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc630ai(void) {
	return private_i2c_add_driver(&sc630ai_driver);
}

static __exit void exit_sc630ai(void) {
	private_i2c_del_driver(&sc630ai_driver);
}

module_init(init_sc630ai);
module_exit(exit_sc630ai);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc630ai(void) {
	return private_i2c_add_driver(&sc630ai_driver);
}

static void exit_first_sc630ai(void) {
	private_i2c_del_driver(&sc630ai_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "sc630ai",
	.i2c_addr = 0x30,
	.width = 0,
	.height = 0,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc630ai,
	.exit_sensor = exit_first_sc630ai
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for sc630ai sensor");
MODULE_LICENSE("GPL");
