/*
 * gc8602.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           3840*2160       25        mipi_2lane    linear  10      25              null
 * @I2C addr:0x31
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

#define SENSOR_VERSION  "H20260119a"

//#define SENSOR_TEST
//#define SENSOR_SLAVE
//#define SENSOR_PM

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
//#define SENSOR_HCG
//#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x86)
#define SENSOR_CHIP_ID_L    (0x02)
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

struct tx_isp_sensor_attribute gc8602_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut gc8602_again_lut[] = {
	{0x0040, 0},             //1.000000
	{0x0041, 1465},          //1.015625
	{0x0042, 2909},          //1.031250
	{0x0043, 4330},          //1.046875
	{0x0044, 5731},          //1.062500
	{0x0045, 7111},          //1.078125
	{0x0046, 8471},          //1.093750
	{0x0047, 9813},          //1.109375
	{0x0048, 11135},         //1.125000
	{0x0049, 12439},         //1.140625
	{0x004a, 13726},         //1.156250
	{0x004b, 14995},         //1.171875
	{0x004c, 16247},         //1.187500
	{0x004d, 17483},         //1.203125
	{0x004e, 18703},         //1.218750
	{0x004f, 19908},         //1.234375
	{0x0050, 21097},         //1.250000
	{0x0051, 22271},         //1.265625
	{0x0052, 23431},         //1.281250
	{0x0053, 24578},         //1.296875
	{0x0054, 25710},         //1.312500
	{0x0055, 26829},         //1.328125
	{0x0056, 27935},         //1.343750
	{0x0057, 29028},         //1.359375
	{0x0058, 30108},         //1.375000
	{0x0059, 31177},         //1.390625
	{0x005a, 32233},         //1.406250
	{0x005b, 33277},         //1.421875
	{0x005c, 34311},         //1.437500
	{0x005d, 35333},         //1.453125
	{0x005e, 36344},         //1.468750
	{0x005f, 37345},         //1.484375
	{0x0060, 38335},         //1.500000
	{0x0061, 39315},         //1.515625
	{0x0062, 40285},         //1.531250
	{0x0063, 41245},         //1.546875
	{0x0064, 42195},         //1.562500
	{0x0065, 43135},         //1.578125
	{0x0066, 44067},         //1.593750
	{0x0067, 44990},         //1.609375
	{0x0068, 45903},         //1.625000
	{0x0069, 46808},         //1.640625
	{0x006a, 47704},         //1.656250
	{0x006b, 48592},         //1.671875
	{0x006c, 49472},         //1.687500
	{0x006d, 50343},         //1.703125
	{0x006e, 51206},         //1.718750
	{0x006f, 52062},         //1.734375
	{0x0070, 52910},         //1.750000
	{0x0071, 53750},         //1.765625
	{0x0072, 54583},         //1.781250
	{0x0073, 55409},         //1.796875
	{0x0074, 56228},         //1.812500
	{0x0075, 57039},         //1.828125
	{0x0076, 57844},         //1.843750
	{0x0077, 58642},         //1.859375
	{0x0078, 59433},         //1.875000
	{0x0079, 60217},         //1.890625
	{0x007a, 60996},         //1.906250
	{0x007b, 61768},         //1.921875
	{0x007c, 62534},         //1.937500
	{0x007d, 63293},         //1.953125
	{0x007e, 64046},         //1.968750
	{0x007f, 64793},         //1.984375
	{0x0080, 65536},         //2.000000
	{0x0082, 67001},         //2.031250
	{0x0084, 68445},         //2.062500
	{0x0086, 69866},         //2.093750
	{0x0088, 71267},         //2.125000
	{0x008a, 72647},         //2.156250
	{0x008c, 74007},         //2.187500
	{0x008e, 75349},         //2.218750
	{0x0090, 76671},         //2.250000
	{0x0092, 77975},         //2.281250
	{0x0094, 79262},         //2.312500
	{0x0096, 80531},         //2.343750
	{0x0098, 81783},         //2.375000
	{0x009a, 83019},         //2.406250
	{0x009c, 84239},         //2.437500
	{0x009e, 85444},         //2.468750
	{0x00a0, 86633},         //2.500000
	{0x00a2, 87807},         //2.531250
	{0x00a4, 88967},         //2.562500
	{0x00a6, 90114},         //2.593750
	{0x00a8, 91246},         //2.625000
	{0x00aa, 92365},         //2.656250
	{0x00ac, 93471},         //2.687500
	{0x00ae, 94564},         //2.718750
	{0x00b0, 95644},         //2.750000
	{0x00b2, 96713},         //2.781250
	{0x00b4, 97769},         //2.812500
	{0x00b6, 98813},         //2.843750
	{0x00b8, 99847},         //2.875000
	{0x00ba, 100869},        //2.906250
	{0x00bc, 101880},        //2.937500
	{0x00be, 102881},        //2.968750
	{0x00c0, 103871},        //3.000000
	{0x00c3, 105337},        //3.046875
	{0x00c6, 106781},        //3.093750
	{0x00c9, 108202},        //3.140625
	{0x00cc, 109603},        //3.187500
	{0x00cf, 110983},        //3.234375
	{0x00d2, 112344},        //3.281250
	{0x00d5, 113685},        //3.328125
	{0x00d8, 115008},        //3.375000
	{0x00db, 116312},        //3.421875
	{0x00de, 117598},        //3.468750
	{0x00e1, 118867},        //3.515625
	{0x00e4, 120119},        //3.562500
	{0x00e7, 121355},        //3.609375
	{0x00ea, 122575},        //3.656250
	{0x00ed, 123779},        //3.703125
	{0x00f0, 124969},        //3.750000
	{0x00f3, 126144},        //3.796875
	{0x00f6, 127304},        //3.843750
	{0x00f9, 128450},        //3.890625
	{0x00fc, 129582},        //3.937500
	{0x00ff, 130701},        //3.984375
	{0x0102, 131807},        //4.031250
	{0x0106, 133261},        //4.093750
	{0x010a, 134694},        //4.156250
	{0x010e, 136105},        //4.218750
	{0x0112, 137496},        //4.281250
	{0x0116, 138866},        //4.343750
	{0x011a, 140216},        //4.406250
	{0x011e, 141549},        //4.468750
	{0x0122, 142862},        //4.531250
	{0x0126, 144157},        //4.593750
	{0x012a, 145434},        //4.656250
	{0x012e, 146695},        //4.718750
	{0x0132, 147939},        //4.781250
	{0x0136, 149167},        //4.843750
	{0x013a, 150379},        //4.906250
	{0x013e, 151576},        //4.968750
	{0x0142, 152758},        //5.031250
	{0x0147, 154214},        //5.109375
	{0x014c, 155650},        //5.187500
	{0x0151, 157062},        //5.265625
	{0x0156, 158456},        //5.343750
	{0x015b, 159827},        //5.421875
	{0x0160, 161180},        //5.500000
	{0x0165, 162514},        //5.578125
	{0x016a, 163829},        //5.656250
	{0x016f, 165125},        //5.734375
	{0x0174, 166405},        //5.812500
	{0x0179, 167667},        //5.890625
	{0x017e, 168913},        //5.968750
	{0x0183, 170143},        //6.046875
	{0x0189, 171597},        //6.140625
	{0x018f, 173030},        //6.234375
	{0x0195, 174441},        //6.328125
	{0x019b, 175832},        //6.421875
	{0x01a1, 177202},        //6.515625
	{0x01a7, 178553},        //6.609375
	{0x01ad, 179884},        //6.703125
	{0x01b3, 181197},        //6.796875
	{0x01b9, 182493},        //6.890625
	{0x01bf, 183770},        //6.984375
	{0x01c5, 185031},        //7.078125
	{0x01cc, 186481},        //7.187500
	{0x01d3, 187909},        //7.296875
	{0x01da, 189315},        //7.406250
	{0x01e1, 190701},        //7.515625
	{0x01e8, 192068},        //7.625000
	{0x01ef, 193414},        //7.734375
	{0x01f6, 194742},        //7.843750
	{0x01fd, 196051},        //7.953125
	{0x0204, 197343},        //8.062500
	{0x020c, 198797},        //8.187500
	{0x0214, 200230},        //8.312500
	{0x021c, 201641},        //8.437500
	{0x0224, 203032},        //8.562500
	{0x022c, 204402},        //8.687500
	{0x0234, 205752},        //8.812500
	{0x023c, 207085},        //8.937500
	{0x0244, 208398},        //9.062500
	{0x024d, 209853},        //9.203125
	{0x0256, 211287},        //9.343750
	{0x025f, 212700},        //9.484375
	{0x0268, 214091},        //9.625000
	{0x0271, 215462},        //9.765625
	{0x027a, 216814},        //9.906250
	{0x0283, 218147},        //10.046875
	{0x028d, 219606},        //10.203125
	{0x0297, 221043},        //10.359375
	{0x02a1, 222458},        //10.515625
	{0x02ab, 223853},        //10.671875
	{0x02b5, 225227},        //10.828125
	{0x02bf, 226582},        //10.984375
	{0x02c9, 227917},        //11.140625
	{0x02d4, 229365},        //11.312500
	{0x02df, 230790},        //11.484375
	{0x02ea, 232194},        //11.656250
	{0x02f5, 233578},        //11.828125
	{0x0300, 234943},        //12.000000
	{0x030c, 236409},        //12.187500
	{0x0318, 237853},        //12.375000
	{0x0324, 239274},        //12.562500
	{0x0330, 240675},        //12.750000
	{0x033c, 242055},        //12.937500
	{0x0348, 243416},        //13.125000
	{0x0355, 244867},        //13.328125
	{0x0362, 246298},        //13.531250
	{0x036f, 247707},        //13.734375
	{0x037c, 249095},        //13.937500
	{0x0389, 250463},        //14.140625
	{0x0397, 251914},        //14.359375
	{0x03a5, 253343},        //14.578125
	{0x03b3, 254752},        //14.796875
	{0x03c1, 256140},        //15.015625
	{0x03d0, 257604},        //15.250000
	{0x03df, 259046},        //15.484375
	{0x03ee, 260466},        //15.718750
	{0x03fd, 261865},        //15.953125
	{0x040c, 263244},        //16.187500
	{0x041c, 264693},        //16.437500
	{0x042c, 266120},        //16.687500
	{0x043c, 267526},        //16.937500
	{0x044c, 268911},        //17.187500
	{0x045d, 270362},        //17.453125
	{0x046e, 271790},        //17.718750
	{0x047f, 273197},        //17.984375
	{0x0490, 274583},        //18.250000
	{0x04a2, 276029},        //18.531250
	{0x04b4, 277453},        //18.812500
	{0x04c6, 278856},        //19.093750
	{0x04d9, 280315},        //19.390625
	{0x04ec, 281751},        //19.687500
	{0x04ff, 283167},        //19.984375
	{0x0512, 284561},        //20.281250
	{0x0526, 286007},        //20.593750
	{0x053a, 287431},        //20.906250
	{0x054e, 288833},        //21.218750
	{0x0563, 290284},        //21.546875
	{0x0578, 291714},        //21.875000
	{0x058d, 293121},        //22.203125
	{0x05a3, 294574},        //22.546875
	{0x05b9, 296003},        //22.890625
	{0x05cf, 297413},        //23.234375
	{0x05e6, 298864},        //23.593750
	{0x05fd, 300294},        //23.953125
	{0x0614, 301702},        //24.312500
	{0x062c, 303149},        //24.687500
	{0x0644, 304574},        //25.062500
	{0x065d, 306037},        //25.453125
	{0x0676, 307476},        //25.843750
	{0x068f, 308895},        //26.234375
	{0x06a9, 310348},        //26.640625
	{0x06c3, 311779},        //27.046875
	{0x06de, 313243},        //27.468750
	{0x06f9, 314683},        //27.890625
	{0x0714, 316103},        //28.312500
	{0x0730, 317553},        //28.750000
	{0x074c, 318981},        //29.187500
	{0x0769, 320437},        //29.640625
	{0x0786, 321871},        //30.093750
	{0x07a4, 323333},        //30.562500
	{0x07c2, 324773},        //31.031250
	{0x07e1, 326237},        //31.515625
	{0x0800, 327680},        //32.000000
	{0x0820, 329145},        //32.500000
	{0x0840, 330589},        //33.000000
	{0x0861, 332054},        //33.515625
	{0x0882, 333498},        //34.031250
	{0x08a4, 334962},        //34.562500
	{0x08c6, 336404},        //35.093750
	{0x08e9, 337866},        //35.640625
	{0x090c, 339306},        //36.187500
	{0x0930, 340765},        //36.750000
	{0x0954, 342201},        //37.312500
	{0x0979, 343654},        //37.890625
	{0x099e, 345086},        //38.468750
	{0x09c4, 346534},        //39.062500
	{0x09eb, 347997},        //39.671875
	{0x0a12, 349439},        //40.281250
	{0x0a3a, 350895},        //40.906250
	{0x0a62, 352329},        //41.531250
	{0x0a8b, 353775},        //42.171875
	{0x0ab5, 355236},        //42.828125
	{0x0adf, 356673},        //43.484375
	{0x0b0a, 358123},        //44.156250
	{0x0b36, 359584},        //44.843750
	{0x0b62, 361022},        //45.531250
	{0x0b8f, 362470},        //46.234375
	{0x0bbd, 363929},        //46.953125
	{0x0beb, 365365},        //47.671875
	{0x0c1a, 366811},        //48.406250
	{0x0c4a, 368265},        //49.156250
	{0x0c7b, 369727},        //49.921875
	{0x0cac, 371166},        //50.687500
	{0x0cde, 372611},        //51.468750
	{0x0d11, 374065},        //52.265625
	{0x0d45, 375523},        //53.078125
	{0x0d7a, 376986},        //53.906250
	{0x0daf, 378428},        //54.734375
	{0x0de5, 379874},        //55.578125
	{0x0e1c, 381325},        //56.437500
	{0x0e54, 382780},        //57.312500
	{0x0e8d, 384238},        //58.203125
	{0x0ec7, 385698},        //59.109375
	{0x0f02, 387162},        //60.031250
	{0x0f3e, 388627},        //60.968750
	{0x0f7a, 390070},        //61.906250
	{0x0fb7, 391514},        //62.859375
	{0x0ff5, 392960},        //63.828125
	{0x1034, 394407},        //64.812500
	{0x1074, 395855},        //65.812500
	{0x10b5, 397302},        //66.828125
	{0x10f7, 398751},        //67.859375
	{0x113a, 400198},        //68.906250
	{0x117e, 401645},        //69.968750
	{0x11c3, 403091},        //71.046875
	{0x120a, 404556},        //72.156250
	{0x1252, 406019},        //73.281250
	{0x129b, 407478},        //74.421875
	{0x12e5, 408937},        //75.578125
	{0x1330, 410391},        //76.750000
	{0x137c, 411843},        //77.937500
	{0x13c9, 413291},        //79.140625
	{0x1418, 414755},        //80.375000
	{0x1468, 416214},        //81.625000
	{0x14b9, 417668},        //82.890625
	{0x150b, 419119},        //84.171875
	{0x155f, 420582},        //85.484375
	{0x15b4, 422039},        //86.812500
	{0x160a, 423492},        //88.156250
	{0x1662, 424955},        //89.531250
	{0x16bb, 426412},        //90.921875
	{0x1715, 427862},        //92.328125
	{0x1771, 429323},        //93.765625
	{0x17ce, 430778},        //95.218750
	{0x182d, 432240},        //96.703125
	{0x188d, 433696},        //98.203125
	{0x18ef, 435158},        //99.734375
	{0x1952, 436614},        //101.281250
	{0x19b7, 438076},        //102.859375
	{0x1a1d, 439529},        //104.453125
	{0x1a85, 440990},        //106.078125
	{0x1aef, 442454},        //107.734375
	{0x1b5a, 443909},        //109.406250
	{0x1bc7, 445371},        //111.109375
	{0x1c36, 446835},        //112.843750
	{0x1ca6, 448290},        //114.593750
	{0x1d18, 449748},        //116.375000
	{0x1d8c, 451210},        //118.187500
	{0x1e02, 452673},        //120.031250
	{0x1e7a, 454139},        //121.906250
	{0x1ef3, 455594},        //123.796875
	{0x1f6e, 457050},        //125.718750
	{0x1feb, 458508},        //127.671875
	{0x206a, 459966},        //129.656250
	{0x20eb, 461423},        //131.671875
	{0x216e, 462883},        //133.718750
	{0x21f3, 464341},        //135.796875
	{0x227a, 465798},        //137.906250
	{0x2303, 467255},        //140.046875
	{0x238f, 468721},        //142.234375
	{0x241d, 470184},        //144.453125
	{0x24ad, 471645},        //146.703125
	{0x253f, 473104},        //148.984375
	{0x25d3, 474560},        //151.296875
	{0x266a, 476023},        //153.656250
	{0x2703, 477483},        //156.046875
	{0x279f, 478948},        //158.484375
	{0x283d, 480410},        //160.953125
	{0x28dd, 481867},        //163.453125
	{0x2980, 483330},        //166.000000
	{0x2a26, 484795},        //168.593750
	{0x2ace, 486256},        //171.218750
	{0x2b79, 487720},        //173.890625
	{0x2c26, 489178},        //176.593750
	{0x2cd6, 490640},        //179.343750
	{0x2d89, 492103},        //182.140625
	{0x2e3f, 493568},        //184.984375
	{0x2ef7, 495026},        //187.859375
	{0x2fb2, 496484},        //190.781250
	{0x3070, 497945},        //193.750000
	{0x3131, 499404},        //196.765625
	{0x31f5, 500865},        //199.828125
	{0x32bc, 502325},        //202.937500
	{0x3386, 503784},        //206.093750
	{0x3454, 505249},        //209.312500
	{0x3525, 506713},        //212.578125
	{0x35f9, 508174},        //215.890625
	{0x36d0, 509635},        //219.250000
	{0x37ab, 511099},        //222.671875
	{0x3889, 512560},        //226.140625
	{0x396b, 514026},        //229.671875
	{0x3a50, 515487},        //233.250000
	{0x3b39, 516952},        //236.890625
	{0x3c25, 518412},        //240.578125
	{0x3d15, 519875},        //244.328125
	{0x3e09, 521338},        //248.140625
	{0x3f01, 522803},        //252.015625
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc8602_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc8602_again_lut;
	while (lut->gain <= gc8602_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc8602_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int gc8602_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc8602_again_lut;
	while(lut->gain <= gc8602_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc8602_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int gc8602_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int gc8602_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc8602_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc8602_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1296,
	.lans = 2,
	.image_twidth = 3840,
	.image_theight = 2160,
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

struct tx_isp_dvp_bus gc8602_dvp = {
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

struct tx_isp_sensor_attribute gc8602_attr = {
	.name = "gc8602",
	.chip_id = 0x8602,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x31,
	.sensor_ctrl.alloc_again = gc8602_alloc_again,
	.sensor_ctrl.alloc_dgain = gc8602_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = gc8602_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list gc8602_init_regs_3840_2160_25fps_mipi[] = {
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03fe,0x10},
	{0x0331,0x04},
	{0x0a38,0x01},
	{0x0a39,0x72},
	{0x0a34,0x03},
	{0x0a23,0x39},
	{0x0a21,0x33},
	{0x0a39,0x73},
	{0x0a3a,0x7d},
	{0x0a35,0x96},
	{0x0a3b,0x40},
	{0x0a3c,0xa2},
	{0x0a36,0x03},
	{0x0a32,0x0c},
	{0x0a33,0x50},
	{0x0a3d,0x95},
	{0x0a3e,0xe1},
	{0x0a22,0x46},
	{0x0a20,0x04},
	{0x0a24,0x04},
	{0x0a25,0x00},
	{0x0a26,0x87},
	{0x0a21,0x37},
	{0x0a21,0x77},
	{0x0ac2,0x33},
	{0x0a33,0x51},
	{0x0a33,0x71},
	{0x0314,0x10},
	{0x0320,0x00},
	{0x0327,0xc3},
	{0x031c,0x46},
	{0x0331,0x07},
	{0x0ab2,0x7a},
	{0x029b,0x00},
	{0x0ab3,0x0e},
	{0x0e04,0x03},
	{0x0e0e,0x03},
	{0x0e0c,0x03},
	{0x0ab4,0x09},
	{0x0aba,0xa5},
	{0x0081,0xdf},
	{0x0abb,0x05},
	{0x0245,0x02},
	{0x0006,0xbb},
	{0x0e10,0x08},
	{0x0e11,0xfc},
	{0x0e0e,0x23},
	{0x0e0d,0x02},
	{0x02ce,0x03},
	{0x02cc,0x03},
	{0x0e16,0x06},
	{0x02cd,0x35},
	{0x0275,0x35},
	{0x0238,0x00},
	{0x0239,0x06},
	{0x023a,0x06},
	{0x02c1,0xbf},
	{0x02c2,0xbf},
	{0x0e68,0x00},
	{0x0e69,0x66},
	{0x0e35,0x22},
	{0x0e5d,0x36},
	{0x0e66,0x00},
	{0x0e67,0x5d},
	{0x0e52,0x22},
	{0x0e59,0x22},
	{0x021b,0x52},
	{0x021c,0x08},
	{0x0242,0x06},
	{0x0243,0x06},
	{0x0331,0x07},
	{0x0ab4,0x09},
	{0x0ab6,0x24},
	{0x0e60,0x40},
	{0x0e64,0x00},
	{0x0e65,0xf0},
	{0x0e44,0x1e},
	{0x0e1a,0xc1},
	{0x0e8b,0x01},
	{0x0c18,0x40},
	{0x0276,0x01},
	{0x0357,0x06},
	{0x0e1a,0x01},
	{0x0abb,0x05},
	{0x0e11,0x0c},
	{0x0e45,0x0c},
	{0x0e0c,0xe3},
	{0x0e4d,0x00},
	{0x0e70,0x90},
	{0x029b,0x08},
	{0x029e,0x23},
	{0x029a,0x10},
	{0x029d,0x00},
	{0x0130,0x50},
	{0x0131,0x01},
	{0x0274,0x0b},
	{0x0ab2,0x7b},
	{0x0212,0x30},
	{0x0213,0x04},
	{0x0219,0x4f},
	{0x0259,0x08},
	{0x025a,0x9e},
	{0x0340,0x08},	//HTS:4312
	{0x0341,0xca},
	{0x0342,0x04},	//VTS:2260
	{0x0343,0xb0},
	{0x0217,0xa0},
	{0x0087,0x50},
	{0x0346,0x00},
	{0x0347,0x28},
	{0x0348,0x0f},
	{0x0349,0x10},
	{0x034a,0x08},
	{0x034b,0x80},
	{0x0c22,0x00},
	{0x0c23,0x04},
	{0x0c24,0x07},
	{0x0c25,0x8a},
	{0x034e,0x0f},
	{0x034f,0x50},
	{0x0c12,0x43},
	{0x0c13,0x43},
	{0x0089,0x00},
	{0x0004,0x0f},
	{0x0004,0x0f},
	{0x0036,0x00},
	{0x0037,0x00},
	{0x0038,0x00},
	{0x0039,0x00},
	{0x003a,0x00},
	{0x0082,0x00},
	{0x0083,0x00},
	{0x0060,0x20},
	{0x0002,0x09},
	{0x000f,0x00},
	{0x0094,0x0f},
	{0x0095,0x00},
	{0x0096,0x08},
	{0x0097,0x70},
	{0x0099,0x08},
	{0x009b,0x08},
	{0x0001,0x0c},
	{0x00b0,0x06},
	{0x00b1,0x10},
	{0x00c0,0x02},
	{0x00c1,0x20},
	{0x00d0,0x06},
	{0x00d1,0x10},
	{0x00b2,0x08},
	{0x00b3,0x0e},
	{0x00c2,0x04},
	{0x00c3,0x60},
	{0x00d2,0x06},
	{0x00d3,0x10},
	{0x00b4,0x0a},
	{0x00b5,0x10},
	{0x00c4,0x04},
	{0x00c5,0x30},
	{0x00d4,0x06},
	{0x00d5,0x10},
	{0x00b6,0x06},
	{0x00b7,0x10},
	{0x00c6,0x04},
	{0x00c7,0x10},
	{0x00d6,0x06},
	{0x00d7,0x10},
	{0x00b8,0x06},
	{0x00b9,0x10},
	{0x00c8,0x06},
	{0x00c9,0x20},
	{0x00d8,0x06},
	{0x00d9,0x10},
	{0x00ba,0x06},
	{0x00bb,0x10},
	{0x00ca,0x06},
	{0x00cb,0x00},
	{0x00da,0x06},
	{0x00db,0x10},
	{0x00bc,0x06},
	{0x00bd,0x10},
	{0x00cc,0x08},
	{0x00cd,0x10},
	{0x00dc,0x06},
	{0x00dd,0x10},
	{0x00be,0x06},
	{0x00bf,0x10},
	{0x00ce,0x08},
	{0x00cf,0x08},
	{0x00de,0x06},
	{0x00df,0x10},
	{0x0008,0x66},
	{0x0009,0x58},
	{0x000a,0x47},
	{0x000b,0x36},
	{0x0051,0x02},
	{0x0076,0x01},
	{0x021a,0x10},
	{0x044c,0x76},
	{0x044d,0x76},
	{0x044e,0x76},
	{0x044f,0x76},
	{0x0448,0x09},
	{0x0449,0x09},
	{0x044a,0x09},
	{0x044b,0x09},
	{0x0468,0x00},
	{0x0046,0x20},
	{0x0c22,0x18},
	{0x0c19,0x04},
	{0x0c1a,0x4c},
	{0x0c1b,0xd8},
	{0x0c20,0x01},
	{0x0c21,0x00},
	{0x0202,0x04},
	{0x0203,0x00},
	{0x005d,0x80},
	{0x0089,0x00},
	{0x02ab,0x00},
	{0x02a9,0x00},
	{0x0e5c,0x24},
	{0x0e56,0x24},
	{0x0e76,0x00},
	{0x0075,0x48},
	{0x0800,0x01},
	{0x0810,0x01},
	{0x0810,0x00},
	{0x0801,0xff},
	{0x0803,0xc0},
	{0x080b,0x78},
	{0x0880,0x00},
	{0x0881,0x00},
	{0x0885,0x00},
	{0x0886,0x40},
	{0x0885,0x00},
	{0x0886,0x59},
	{0x0885,0x00},
	{0x0886,0x8b},
	{0x0885,0x00},
	{0x0886,0xc2},
	{0x0885,0x01},
	{0x0886,0x14},
	{0x0885,0x01},
	{0x0886,0x7d},
	{0x0885,0x02},
	{0x0886,0x19},
	{0x0885,0x02},
	{0x0886,0xdc},
	{0x0885,0x03},
	{0x0886,0xfe},
	{0x0885,0x05},
	{0x0886,0x5c},
	{0x0885,0x07},
	{0x0886,0xc5},
	{0x0885,0x0a},
	{0x0886,0xa8},
	{0x0885,0x0e},
	{0x0886,0xc7},
	{0x0885,0x13},
	{0x0886,0xfd},
	{0x0885,0x1c},
	{0x0886,0x90},
	{0x0880,0x00},
	{0x0881,0x0f},
	{0x0885,0x02},
	{0x0886,0xab},
	{0x0885,0x02},
	{0x0886,0xa9},
	{0x0885,0x0e},
	{0x0886,0x76},
	{0x0885,0x0e},
	{0x0886,0x16},
	{0x0885,0x0e},
	{0x0886,0x5c},
	{0x0885,0x0e},
	{0x0886,0x56},
	{0x0885,0x02},
	{0x0886,0xcd},
	{0x0885,0x00},
	{0x0886,0x46},
	{0x0880,0x00},
	{0x0881,0x17},
	{0x0885,0x00},
	{0x0886,0x00},
	{0x0885,0x06},
	{0x0886,0x03},
	{0x0885,0x25},
	{0x0886,0x25},
	{0x0885,0x20},
	{0x0886,0x35},
	{0x0880,0x00},
	{0x0881,0x1b},
	{0x0885,0x00},
	{0x0886,0x01},
	{0x0885,0x06},
	{0x0886,0x03},
	{0x0885,0x27},
	{0x0886,0x27},
	{0x0885,0x20},
	{0x0886,0x35},
	{0x0880,0x00},
	{0x0881,0x1f},
	{0x0885,0x00},
	{0x0886,0x00},
	{0x0885,0x06},
	{0x0886,0x00},
	{0x0885,0x25},
	{0x0886,0x25},
	{0x0885,0x20},
	{0x0886,0x35},
	{0x0880,0x00},
	{0x0881,0x23},
	{0x0885,0x00},
	{0x0886,0x01},
	{0x0885,0x06},
	{0x0886,0x00},
	{0x0885,0x27},
	{0x0886,0x27},
	{0x0885,0x20},
	{0x0886,0x35},
	{0x0880,0x00},
	{0x0881,0x27},
	{0x0885,0x00},
	{0x0886,0x02},
	{0x0885,0x06},
	{0x0886,0x00},
	{0x0885,0x2a},
	{0x0886,0x2a},
	{0x0885,0x20},
	{0x0886,0x35},
	{0x0880,0x00},
	{0x0881,0x2b},
	{0x0885,0x00},
	{0x0886,0x03},
	{0x0885,0x06},
	{0x0886,0x00},
	{0x0885,0x2d},
	{0x0886,0x2d},
	{0x0885,0x20},
	{0x0886,0x35},
	{0x0880,0x00},
	{0x0881,0x2f},
	{0x0885,0x00},
	{0x0886,0x04},
	{0x0885,0x06},
	{0x0886,0x00},
	{0x0885,0x31},
	{0x0886,0x31},
	{0x0885,0x20},
	{0x0886,0x35},
	{0x0880,0x00},
	{0x0881,0x33},
	{0x0885,0x00},
	{0x0886,0x05},
	{0x0885,0x06},
	{0x0886,0x00},
	{0x0885,0x34},
	{0x0886,0x34},
	{0x0885,0x20},
	{0x0886,0x35},
	{0x0880,0x00},
	{0x0881,0x37},
	{0x0885,0x00},
	{0x0886,0x06},
	{0x0885,0x06},
	{0x0886,0x00},
	{0x0885,0x35},
	{0x0886,0x35},
	{0x0885,0x20},
	{0x0886,0x55},
	{0x0880,0x00},
	{0x0881,0x3b},
	{0x0885,0x00},
	{0x0886,0x07},
	{0x0885,0x06},
	{0x0886,0x00},
	{0x0885,0x38},
	{0x0886,0x38},
	{0x0885,0x20},
	{0x0886,0x55},
	{0x0880,0x00},
	{0x0881,0x3f},
	{0x0885,0x01},
	{0x0886,0x04},
	{0x0885,0x06},
	{0x0886,0x00},
	{0x0885,0x3c},
	{0x0886,0x3c},
	{0x0885,0x30},
	{0x0886,0x55},
	{0x0880,0x00},
	{0x0881,0x43},
	{0x0885,0x01},
	{0x0886,0x05},
	{0x0885,0x03},
	{0x0886,0x00},
	{0x0885,0x31},
	{0x0886,0x31},
	{0x0885,0x40},
	{0x0886,0x75},
	{0x0880,0x00},
	{0x0881,0x47},
	{0x0885,0x01},
	{0x0886,0x06},
	{0x0885,0x02},
	{0x0886,0x00},
	{0x0885,0x2f},
	{0x0886,0x2f},
	{0x0885,0x50},
	{0x0886,0x75},
	{0x0880,0x00},
	{0x0881,0x4b},
	{0x0885,0x01},
	{0x0886,0x07},
	{0x0885,0x01},
	{0x0886,0x00},
	{0x0885,0x2c},
	{0x0886,0x2c},
	{0x0885,0x60},
	{0x0886,0x75},
	{0x0880,0x00},
	{0x0881,0x4f},
	{0x0885,0x01},
	{0x0886,0x0f},
	{0x0885,0x01},
	{0x0886,0x00},
	{0x0885,0x2d},
	{0x0886,0x2d},
	{0x0885,0x80},
	{0x0886,0x85},
	{0x0803,0xc1},
	{0x0807,0x00},
	{0x0808,0x40},
	{0x0a67,0x80},
	{0x0a4e,0x0c},
	{0x0a4f,0x0c},
	{0x0a54,0x36},
	{0x0a55,0x36},
	{0x0a7f,0x17},
	{0x0a7e,0x10},
	{0x0a81,0x10},
	{0x0a4e,0x00},
	{0x0a4f,0x00},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x002d,0x10},
	{0x0021,0x44},
	{0x0a90,0x83},
	{0x0a91,0x02},
	{0x0a93,0xe0},
	{0x0a97,0x01},
	{0x0a98,0x08},
	{0x0028,0x0f},
	{0x0029,0x10},
	{0x002a,0x08},
	{0x002b,0x80},
	{0x0a9e,0x0f},
	{0x0a9f,0x10},
	{0x0a9b,0x08},
	{0x0a9c,0x80},
	{0x0022,0x00},
	{0x0023,0x00},
	{0x0024,0x00},
	{0x0025,0x00},
	{0x0a5a,0x80},
	{SENSOR_REG_DELAY, 0x20},
	{0x05be,0x01},
	{0x0080,0x02},
	{0x0020,0x8c},
	{0x039c,0x02},
	{0x08e9,0x01},
	{0x039a,0x0f},
	{0x0391,0x3c},
	{0x0392,0x3c},
	{0x0393,0x3c},
	{0x0394,0x3c},
	{0x08ea,0x00},
	{0x08ea,0x3c},
	{0x08eb,0x01},
	{0x08ec,0x38},
	{0x08ed,0x0a},
	{0x08eb,0x07},
	{0x08ec,0x31},
	{0x08ed,0x03},
	{0x08eb,0x03},
	{0x08ec,0x34},
	{0x08ed,0x0a},
	{0x08eb,0x71},
	{0x08ec,0x33},
	{0x08ed,0x0a},
	{0x08eb,0x76},
	{0x08ec,0x21},
	{0x08ed,0x0a},
	{0x08eb,0x77},
	{0x08ec,0x21},
	{0x08ed,0x0a},
	{0x08eb,0x01},
	{0x08ec,0x36},
	{0x08ed,0x03},
	{0x08eb,0x00},
	{0x08ec,0x36},
	{0x08ed,0x03},
	{0x08eb,0x09},
	{0x08ec,0x00},
	{0x08ed,0x01},
	{0x08ea,0x78},
	{0x08ea,0xb4},
	{0x08eb,0x08},
	{0x08ec,0x00},
	{0x08ed,0x01},
	{0x08eb,0x73},
	{0x08ec,0x21},
	{0x08ed,0x0a},
	{0x08eb,0x72},
	{0x08ec,0x21},
	{0x08ed,0x0a},
	{0x08eb,0x70},
	{0x08ec,0x33},
	{0x08ed,0x0a},
	{0x08eb,0x02},
	{0x08ec,0x34},
	{0x08ed,0x0a},
	{0x08eb,0x00},
	{0x08ec,0x31},
	{0x08ed,0x03},
	{0x08eb,0x00},
	{0x08ec,0x38},
	{0x08ed,0x0a},
	{0x039c,0x03},
	{0x0100,0x09},
	{0x0107,0x89},
	{0x0114,0x01},
	{0x015a,0x82},
	{0x015b,0x42},
	{0x0111,0x2b},
	{0x0112,0x01},
	{0x010d,0x12},
	{0x010e,0xc0},
	{0x0125,0x25},
	{0x0124,0x04},
	{0x0122,0x0c},
	{0x0123,0x34},
	{0x0126,0x0f},
	{0x0121,0x10},
	{0x0129,0x0a},
	{0x012a,0x18},
	{0x012b,0x0c},
	{0x0327,0x46},
	{0x0336,0x01},
	{0x0336,0x00},
	{0x03fe,0x00},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the gc8602_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc8602_win_sizes[] = {
	/* 3840*2160 [0] */
	{
		.width          = 3840,
		.height         = 2160,
		.fps            = 25 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc8602_init_regs_3840_2160_25fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &gc8602_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc8602_stream_on_mipi[] = {
	{0x03fe, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list gc8602_stream_off_mipi[] = {
	{0x03fe, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc8602_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc8602_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc8602_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc8602_read(sd, vals->reg_num, &val);
			/* ISP_WARNING("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc8602_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc8602_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc8602_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int gc8602_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc8602_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc8602_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc8602_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc8602_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int gc8602_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int gc8602_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int gc8602_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &gc8602_win_sizes[0];
		gc8602_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc8602_attr.again.value = 65536;
		gc8602_attr.again.max = 522803;
		gc8602_attr.again.min = 0;
		gc8602_attr.again.apply_delay = 2;

		gc8602_attr.integration_time.value = 0x400;
		gc8602_attr.integration_time.max = 2260 - 8;
		gc8602_attr.integration_time.min = 1;
		gc8602_attr.integration_time.apply_delay = 2;

		gc8602_attr.total_width = 4312;
		gc8602_attr.total_height = 2260;

		gc8602_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		gc8602_attr.hcg.base_gain = ;
		gc8602_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc8602_attr.again_short.value = ;
		gc8602_attr.again_short.max = ;
		gc8602_attr.again_short.min = ;
		gc8602_attr.again_short.apply_delay = ;

		gc8602_attr.integration_time_short.value = ;
		gc8602_attr.integration_time_short.max = ;
		gc8602_attr.integration_time_short.min = ;
		gc8602_attr.integration_time_short.apply_delay = ;

		gc8602_attr.wdr_cache = wdr_line * gc8602_attr.total_width;

#ifdef SENSOR_HCG
		gc8602_attr.hcg_short.base_gain = ;
		gc8602_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc8602_attr.mipi)), (void *)(&gc8602_mipi_linear), sizeof(gc8602_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int gc8602_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	gc8602_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc8602_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc8602_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc8602_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc8602_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc8602_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = gc8602_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.attr->fsync_attr.call_times = 1;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	gc8602_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc8602_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc8602_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc8602_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc8602_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	gc8602_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "gc8602_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "gc8602_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = gc8602_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc8602 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", gc8602_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "gc8602", sizeof("gc8602"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc8602_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc8602_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = gc8602_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int gc8602_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc8602_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc8602_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc8602_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int gc8602_fsync(struct tx_isp_subdev *sd, struct tx_isp_frame_sync *sync)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int sensor_num = (sync->enable >> 16) & 0xf;
	int sensor_id = (sync->enable >> 12) & 0xf;
	int mode = (sync->enable >> 8) & 0xf;
	int ret = 0;
#ifndef SENSOR_SLAVE
	/* master */
#else
	/* slave */
#endif /* SENSOR_SLAVE */

	return 0;
}

static int gc8602_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = gc8602_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc8602_write_array(sd, gc8602_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc8602 stream on\n");
		}

	} else {
		ret = gc8602_write_array(sd, gc8602_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("gc8602 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc8602_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	/*set integration time*/
	gc8602_write(sd, 0x0202, (it >> 8));
	gc8602_write(sd, 0x0203, (it & 0xff));

	gc8602_write(sd, 0x0807, (again >> 8));
	gc8602_write(sd, 0x0808, (again & 0xff));

	return ret;
}
#else
static int gc8602_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int gc8602_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc8602_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc8602_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc8602_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = gc8602_attr_set(sd, wsize);
	}

	return ret;
}

static int gc8602_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 2260 * 4312 * 25;  /**< HTS * VTS * FPS */
		max_fps = 25;
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
	ret += gc8602_read(sd, 0x0342, &val);
	hts = val << 8;
	val = 0;
	ret += gc8602_read(sd, 0x0343, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: gc8602 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	gc8602_write(sd, 0x0340, (unsigned char) (vts & 0xff));
	gc8602_write(sd, 0x0341, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: gc8602_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;
#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 8;
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

static int gc8602_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		ret = gc8602_write(sd, 0x022c, 0x00);
		sensor->video.mbus.code = TISP_VI_FMT_SGRBG10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		ret = gc8602_write(sd, 0x022c, 0x01);
		sensor->video.mbus.code = TISP_VI_FMT_SRGGB10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		ret = gc8602_write(sd, 0x022c, 0x02);
		sensor->video.mbus.code = TISP_VI_FMT_SBGGR10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		ret = gc8602_write(sd, 0x022c, 0x03);
		sensor->video.mbus.code = TISP_VI_FMT_SGBRG10_1X10;
		break;
	}

	sensor->video.hvflip_mode = par->hvflip;
	gc8602_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_PM
static int gc8602_pm(struct tx_isp_subdev *sd, void *arg)
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
static int gc8602_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int gc8602_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int gc8602_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc8602_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = gc8602_write_array(sd, gc8602_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		gc8602_setting_select(sd, 1);
		gc8602_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		gc8602_setting_select(sd, 0);
		gc8602_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int gc8602_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = gc8602_write_array(sd, wsize->regs);
	ret = gc8602_write_array(sd, gc8602_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc8602_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = gc8602_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = gc8602_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = gc8602_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc8602_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc8602_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc8602_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = gc8602_write_array(sd, gc8602_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = gc8602_write_array(sd, gc8602_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc8602_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = gc8602_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_PM
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret =  gc8602_pm(sd, arg);
		break;
#endif /* SENSOR_PM */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = gc8602_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = gc8602_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = gc8602_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = gc8602_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = gc8602_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc8602_core_ops = {
	.g_chip_ident = gc8602_g_chip_ident,
	.reset = gc8602_reset,
	.init = gc8602_init,
	.g_register = gc8602_g_register,
	.s_register = gc8602_s_register,
	.fsync = gc8602_fsync,
};

static struct tx_isp_subdev_video_ops gc8602_video_ops = {
	.s_stream = gc8602_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc8602_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = gc8602_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc8602_ops = {
	.core = &gc8602_core_ops,
	.video = &gc8602_video_ops,
	.sensor = &gc8602_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "gc8602",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc8602_probe(struct i2c_client *client,
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
	sensor->video.attr = &gc8602_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc8602_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc8602\n");

	return 0;
}

static int gc8602_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc8602_id[] = {
	{"gc8602", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc8602_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver gc8602_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "gc8602",
	},
	.probe          = gc8602_probe,
	.remove         = gc8602_remove,
	.id_table       = gc8602_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc8602(void) {
	return private_i2c_add_driver(&gc8602_driver);
}

static __exit void exit_gc8602(void) {
	private_i2c_del_driver(&gc8602_driver);
}

module_init(init_gc8602);
module_exit(exit_gc8602);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc8602(void) {
	return private_i2c_add_driver(&gc8602_driver);
}

static void exit_first_gc8602(void) {
	private_i2c_del_driver(&gc8602_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "gc8602",
	.i2c_addr = 0x31,
	.width = 3840,
	.height = 2160,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_gc8602,
	.exit_sensor = exit_first_gc8602
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A gc8602 low-level CIS driver");
MODULE_LICENSE("GPL");
