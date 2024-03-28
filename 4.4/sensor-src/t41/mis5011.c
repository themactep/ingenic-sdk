// SPDX-License-Identifier: GPL-2.0+
/*
 * mis5011.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps     interface              mode
 *   0          2690*1632       30       mipi_2lane           linear
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
#include <sensor-info.h>

#define SENSOR_NAME "mis5011"
#define SENSOR_CHIP_ID_H (0x50)
#define SENSOR_CHIP_ID_L (0x03)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (217260000)
#define SENSOR_SUPPORT_SCLK_90fps (222750000)
#define SENSOR_SUPPORT_SCLK_120fps (216000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230928a"

static int reset_gpio = GPIO_PC(18);
static int pwdn_gpio = -1;//GPIO_PA(19);

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0, 0},
	{0x1, 94},
	{0x2, 188},
	{0x3, 283},
	{0x4, 377},
	{0x5, 471},
	{0x6, 565},
	{0x7, 659},
	{0x8, 753},
	{0x9, 847},
	{0xa, 940},
	{0xb, 1034},
	{0xc, 1127},
	{0xd, 1221},
	{0xe, 1314},
	{0xf, 1407},
	{0x10, 1500},
	{0x11, 1593},
	{0x12, 1686},
	{0x13, 1779},
	{0x14, 1872},
	{0x15, 1964},
	{0x16, 2057},
	{0x17, 2149},
	{0x18, 2242},
	{0x19, 2334},
	{0x1a, 2426},
	{0x1b, 2518},
	{0x1c, 2610},
	{0x1d, 2702},
	{0x1e, 2794},
	{0x1f, 2886},
	{0x20, 2978},
	{0x21, 3069},
	{0x22, 3161},
	{0x23, 3252},
	{0x24, 3343},
	{0x25, 3435},
	{0x26, 3617},
	{0x27, 3708},
	{0x28, 3799},
	{0x29, 3889},
	{0x2a, 3980},
	{0x2b, 4071},
	{0x2c, 4161},
	{0x2d, 4252},
	{0x2e, 4342},
	{0x2f, 4432},
	{0x30, 4522},
	{0x31, 4612},
	{0x32, 4702},
	{0x33, 4792},
	{0x34, 4882},
	{0x35, 5062},
	{0x36, 5151},
	{0x37, 5241},
	{0x38, 5330},
	{0x39, 5419},
	{0x3a, 5509},
	{0x3b, 5598},
	{0x3c, 5687},
	{0x3d, 5776},
	{0x3e, 5865},
	{0x3f, 6042},
	{0x40, 6131},
	{0x41, 6220},
	{0x42, 6308},
	{0x43, 6396},
	{0x44, 6485},
	{0x45, 6573},
	{0x46, 6661},
	{0x47, 6837},
	{0x48, 6925},
	{0x49, 7013},
	{0x4a, 7101},
	{0x4b, 7188},
	{0x4c, 7276},
	{0x4d, 7363},
	{0x4e, 7451},
	{0x4f, 7625},
	{0x50, 7713},
	{0x51, 7800},
	{0x52, 7887},
	{0x53, 7974},
	{0x54, 8061},
	{0x55, 8234},
	{0x56, 8321},
	{0x57, 8407},
	{0x58, 8494},
	{0x59, 8580},
	{0x5a, 8666},
	{0x5b, 8839},
	{0x5c, 8925},
	{0x5d, 9011},
	{0x5e, 9097},
	{0x5f, 9183},
	{0x60, 9268},
	{0x61, 9440},
	{0x62, 9525},
	{0x63, 9611},
	{0x64, 9696},
	{0x65, 9781},
	{0x66, 9952},
	{0x67, 10037},
	{0x68, 10122},
	{0x69, 10207},
	{0x6a, 10291},
	{0x6b, 10461},
	{0x6c, 10545},
	{0x6d, 10630},
	{0x6e, 10714},
	{0x6f, 10883},
	{0x70, 10967},
	{0x71, 11051},
	{0x72, 11136},
	{0x73, 11303},
	{0x74, 11387},
	{0x75, 11471},
	{0x76, 11555},
	{0x77, 11638},
	{0x78, 11805},
	{0x79, 11889},
	{0x7a, 11972},
	{0x7b, 12139},
	{0x7c, 12222},
	{0x7d, 12305},
	{0x7e, 12388},
	{0x7f, 12554},
	{0x80, 12636},
	{0x81, 12719},
	{0x82, 12802},
	{0x83, 12967},
	{0x84, 13049},
	{0x85, 13131},
	{0x86, 13296},
	{0x87, 13378},
	{0x88, 13460},
	{0x89, 13542},
	{0x8a, 13706},
	{0x8b, 13787},
	{0x8c, 13869},
	{0x8d, 14032},
	{0x8e, 14114},
	{0x8f, 14195},
	{0x90, 14358},
	{0x91, 14439},
	{0x92, 14520},
	{0x93, 14682},
	{0x94, 14763},
	{0x95, 14844},
	{0x96, 15005},
	{0x97, 15086},
	{0x98, 15166},
	{0x99, 15327},
	{0x9a, 15408},
	{0x9b, 15488},
	{0x9c, 15648},
	{0x9d, 15728},
	{0x9e, 15809},
	{0x9f, 15968},
	{0xa0, 16048},
	{0xa1, 16208},
	{0xa2, 16287},
	{0xa3, 16367},
	{0xa4, 16526},
	{0xa5, 16605},
	{0xa6, 16684},
	{0xa7, 16843},
	{0xa8, 16922},
	{0xa9, 17080},
	{0xaa, 17159},
	{0xab, 17237},
	{0xac, 17395},
	{0xad, 17474},
	{0xae, 17631},
	{0xaf, 17709},
	{0xb0, 17866},
	{0xb1, 17944},
	{0xb2, 18022},
	{0xb3, 18178},
	{0xb4, 18256},
	{0xb5, 18412},
	{0xb6, 18490},
	{0xb7, 18645},
	{0xb8, 18723},
	{0xb9, 18878},
	{0xba, 18955},
	{0xbb, 19032},
	{0xbc, 19187},
	{0xbd, 19264},
	{0xbe, 19418},
	{0xbf, 19495},
	{0xc0, 19649},
	{0xc1, 19726},
	{0xc2, 19879},
	{0xc3, 19956},
	{0xc4, 20109},
	{0xc5, 20185},
	{0xc6, 20338},
	{0xc7, 20414},
	{0xc8, 20566},
	{0xc9, 20642},
	{0xca, 20794},
	{0xcb, 20870},
	{0xcc, 21021},
	{0xcd, 21097},
	{0xce, 21248},
	{0xcf, 21324},
	{0xd0, 21474},
	{0xd1, 21550},
	{0xd2, 21700},
	{0xd3, 21850},
	{0xd4, 21925},
	{0xd5, 22075},
	{0xd6, 22150},
	{0xd7, 22300},
	{0xd8, 22374},
	{0xd9, 22523},
	{0xda, 22598},
	{0xdb, 22747},
	{0xdc, 22895},
	{0xdd, 22969},
	{0xde, 23118},
	{0xdf, 23192},
	{0xe0, 23339},
	{0xe1, 23487},
	{0xe2, 23561},
	{0xe3, 23708},
	{0xe4, 23782},
	{0xe5, 23928},
	{0xe6, 24075},
	{0xe7, 24148},
	{0xe8, 24295},
	{0xe9, 24441},
	{0xea, 24514},
	{0xeb, 24660},
	{0xec, 24733},
	{0xed, 24878},
	{0xee, 25023},
	{0xef, 25096},
	{0xf0, 25241},
	{0xf1, 25385},
	{0xf2, 25458},
	{0xf3, 25602},
	{0xf4, 25746},
	{0xf5, 25890},
	{0xf6, 25962},
	{0xf7, 26105},
	{0xf8, 26249},
	{0xf9, 26320},
	{0xfa, 26463},
	{0xfb, 26606},
	{0xfc, 26678},
	{0xfd, 26820},
	{0xfe, 26962},
	{0xff, 27104},
	{0x100, 27175},
	{0x101, 27317},
	{0x102, 27459},
	{0x103, 27600},
	{0x104, 27671},
	{0x105, 27812},
	{0x106, 27952},
	{0x107, 28093},
	{0x108, 28163},
	{0x109, 28303},
	{0x10a, 28444},
	{0x10b, 28583},
	{0x10c, 28653},
	{0x10d, 28793},
	{0x10e, 28932},
	{0x10f, 29071},
	{0x110, 29210},
	{0x111, 29349},
	{0x112, 29418},
	{0x113, 29557},
	{0x114, 29695},
	{0x115, 29833},
	{0x116, 29971},
	{0x117, 30040},
	{0x118, 30177},
	{0x119, 30314},
	{0x11a, 30452},
	{0x11b, 30588},
	{0x11c, 30725},
	{0x11d, 30862},
	{0x11e, 30998},
	{0x11f, 31066},
	{0x120, 31202},
	{0x121, 31338},
	{0x122, 31474},
	{0x123, 31609},
	{0x124, 31744},
	{0x125, 31879},
	{0x126, 32014},
	{0x127, 32149},
	{0x128, 32284},
	{0x129, 32418},
	{0x12a, 32485},
	{0x12b, 32619},
	{0x12c, 32753},
	{0x12d, 32886},
	{0x12e, 33020},
	{0x12f, 33153},
	{0x130, 33286},
	{0x131, 33419},
	{0x132, 33552},
	{0x133, 33684},
	{0x134, 33817},
	{0x135, 33949},
	{0x136, 34081},
	{0x137, 34212},
	{0x138, 34344},
	{0x139, 34475},
	{0x13a, 34607},
	{0x13b, 34738},
	{0x13c, 34869},
	{0x13d, 34999},
	{0x13e, 35130},
	{0x13f, 35260},
	{0x140, 35455},
	{0x141, 35585},
	{0x142, 35715},
	{0x143, 35844},
	{0x144, 35974},
	{0x145, 36103},
	{0x146, 36232},
	{0x147, 36361},
	{0x148, 36489},
	{0x149, 36618},
	{0x14a, 36810},
	{0x14b, 36938},
	{0x14c, 37066},
	{0x14d, 37194},
	{0x14e, 37321},
	{0x14f, 37448},
	{0x150, 37576},
	{0x151, 37766},
	{0x152, 37893},
	{0x153, 38019},
	{0x154, 38146},
	{0x155, 38272},
	{0x156, 38398},
	{0x157, 38587},
	{0x158, 38712},
	{0x159, 38838},
	{0x15a, 38963},
	{0x15b, 39151},
	{0x15c, 39276},
	{0x15d, 39401},
	{0x15e, 39525},
	{0x15f, 39712},
	{0x160, 39836},
	{0x161, 39960},
	{0x162, 40084},
	{0x163, 40269},
	{0x164, 40393},
	{0x165, 40516},
	{0x166, 40700},
	{0x167, 40823},
	{0x168, 40946},
	{0x169, 41069},
	{0x16a, 41252},
	{0x16b, 41374},
	{0x16c, 41557},
	{0x16d, 41679},
	{0x16e, 41800},
	{0x16f, 41983},
	{0x170, 42104},
	{0x171, 42225},
	{0x172, 42406},
	{0x173, 42527},
	{0x174, 42708},
	{0x175, 42828},
	{0x176, 42948},
	{0x177, 43128},
	{0x178, 43248},
	{0x179, 43427},
	{0x17a, 43546},
	{0x17b, 43725},
	{0x17c, 43844},
	{0x17d, 44022},
	{0x17e, 44141},
	{0x17f, 44319},
	{0x180, 44437},
	{0x181, 44614},
	{0x182, 44732},
	{0x183, 44909},
	{0x184, 45026},
	{0x185, 45202},
	{0x186, 45319},
	{0x187, 45495},
	{0x188, 45611},
	{0x189, 45786},
	{0x18a, 45903},
	{0x18b, 46077},
	{0x18c, 46251},
	{0x18d, 46367},
	{0x18e, 46541},
	{0x18f, 46656},
	{0x190, 46829},
	{0x191, 47002},
	{0x192, 47117},
	{0x193, 47289},
	{0x194, 47461},
	{0x195, 47575},
	{0x196, 47747},
	{0x197, 47918},
	{0x198, 48031},
	{0x199, 48202},
	{0x19a, 48372},
	{0x19b, 48485},
	{0x19c, 48655},
	{0x19d, 48825},
	{0x19e, 48994},
	{0x19f, 49106},
	{0x1a0, 49275},
	{0x1a1, 49443},
	{0x1a2, 49611},
	{0x1a3, 49779},
	{0x1a4, 49890},
	{0x1a5, 50058},
	{0x1a6, 50224},
	{0x1a7, 50391},
	{0x1a8, 50557},
	{0x1a9, 50723},
	{0x1aa, 50834},
	{0x1ab, 50999},
	{0x1ac, 51165},
	{0x1ad, 51330},
	{0x1ae, 51494},
	{0x1af, 51659},
	{0x1b0, 51823},
	{0x1b1, 51987},
	{0x1b2, 52150},
	{0x1b3, 52313},
	{0x1b4, 52422},
	{0x1b5, 52585},
	{0x1b6, 52747},
	{0x1b7, 52910},
	{0x1b8, 53071},
	{0x1b9, 53233},
	{0x1ba, 53394},
	{0x1bb, 53556},
	{0x1bc, 53770},
	{0x1bd, 53930},
	{0x1be, 54091},
	{0x1bf, 54251},
	{0x1c0, 54410},
	{0x1c1, 54570},
	{0x1c2, 54729},
	{0x1c3, 54888},
	{0x1c4, 55046},
	{0x1c5, 55205},
	{0x1c6, 55363},
	{0x1c7, 55573},
	{0x1c8, 55730},
	{0x1c9, 55888},
	{0x1ca, 56045},
	{0x1cb, 56201},
	{0x1cc, 56410},
	{0x1cd, 56566},
	{0x1ce, 56722},
	{0x1cf, 56877},
	{0x1d0, 57084},
	{0x1d1, 57239},
	{0x1d2, 57394},
	{0x1d3, 57548},
	{0x1d4, 57754},
	{0x1d5, 57908},
	{0x1d6, 58061},
	{0x1d7, 58266},
	{0x1d8, 58419},
	{0x1d9, 58571},
	{0x1da, 58775},
	{0x1db, 58927},
	{0x1dc, 59130},
	{0x1dd, 59281},
	{0x1de, 59433},
	{0x1df, 59634},
	{0x1e0, 59785},
	{0x1e1, 59986},
	{0x1e2, 60136},
	{0x1e3, 60336},
	{0x1e4, 60486},
	{0x1e5, 60685},
	{0x1e6, 60834},
	{0x1e7, 61033},
	{0x1e8, 61181},
	{0x1e9, 61379},
	{0x1ea, 61576},
	{0x1eb, 61724},
	{0x1ec, 61921},
	{0x1ed, 62068},
	{0x1ee, 62264},
	{0x1ef, 62460},
	{0x1f0, 62606},
	{0x1f1, 62801},
	{0x1f2, 62995},
	{0x1f3, 63141},
	{0x1f4, 63335},
	{0x1f5, 63528},
	{0x1f6, 63721},
	{0x1f7, 63865},
	{0x1f8, 64058},
	{0x1f9, 64249},
	{0x1fa, 64441},
	{0x1fb, 64632},
	{0x1fc, 64775},
	{0x1fd, 64966},
	{0x1fe, 65156},
	{0x1ff, 65345},
	{0x200, 65535},
	{0x201, 65723},
	{0x202, 65912},
	{0x203, 66100},
	{0x204, 66288},
	{0x205, 66475},
	{0x206, 66662},
	{0x207, 66849},
	{0x208, 67035},
	{0x209, 67221},
	{0x20a, 67407},
	{0x20b, 67592},
	{0x20c, 67777},
	{0x20d, 67961},
	{0x20e, 68145},
	{0x20f, 68329},
	{0x210, 68558},
	{0x211, 68741},
	{0x212, 68924},
	{0x213, 69106},
	{0x214, 69288},
	{0x215, 69515},
	{0x216, 69696},
	{0x217, 69877},
	{0x218, 70057},
	{0x219, 70282},
	{0x21a, 70462},
	{0x21b, 70641},
	{0x21c, 70865},
	{0x21d, 71044},
	{0x21e, 71222},
	{0x21f, 71444},
	{0x220, 71622},
	{0x221, 71843},
	{0x222, 72020},
	{0x223, 72240},
	{0x224, 72416},
	{0x225, 72636},
	{0x226, 72811},
	{0x227, 73030},
	{0x228, 73204},
	{0x229, 73422},
	{0x22a, 73639},
	{0x22b, 73812},
	{0x22c, 74029},
	{0x22d, 74244},
	{0x22e, 74417},
	{0x22f, 74632},
	{0x230, 74846},
	{0x231, 75060},
	{0x232, 75231},
	{0x233, 75444},
	{0x234, 75657},
	{0x235, 75869},
	{0x236, 76080},
	{0x237, 76292},
	{0x238, 76502},
	{0x239, 76713},
	{0x23a, 76922},
	{0x23b, 77090},
	{0x23c, 77299},
	{0x23d, 77549},
	{0x23e, 77757},
	{0x23f, 77964},
	{0x240, 78171},
	{0x241, 78378},
	{0x242, 78584},
	{0x243, 78790},
	{0x244, 78995},
	{0x245, 79241},
	{0x246, 79445},
	{0x247, 79649},
	{0x248, 79852},
	{0x249, 80095},
	{0x24a, 80298},
	{0x24b, 80500},
	{0x24c, 80742},
	{0x24d, 80943},
	{0x24e, 81143},
	{0x24f, 81383},
	{0x250, 81583},
	{0x251, 81822},
	{0x252, 82021},
	{0x253, 82259},
	{0x254, 82496},
	{0x255, 82694},
	{0x256, 82930},
	{0x257, 83126},
	{0x258, 83362},
	{0x259, 83596},
	{0x25a, 83830},
	{0x25b, 84025},
	{0x25c, 84258},
	{0x25d, 84490},
	{0x25e, 84722},
	{0x25f, 84953},
	{0x260, 85184},
	{0x261, 85376},
	{0x262, 85605},
	{0x263, 85835},
	{0x264, 86063},
	{0x265, 86291},
	{0x266, 86556},
	{0x267, 86783},
	{0x268, 87009},
	{0x269, 87235},
	{0x26a, 87460},
	{0x26b, 87685},
	{0x26c, 87947},
	{0x26d, 88170},
	{0x26e, 88393},
	{0x26f, 88653},
	{0x270, 88874},
	{0x271, 89096},
	{0x272, 89353},
	{0x273, 89574},
	{0x274, 89830},
	{0x275, 90049},
	{0x276, 90304},
	{0x277, 90558},
	{0x278, 90776},
	{0x279, 91029},
	{0x27a, 91281},
	{0x27b, 91497},
	{0x27c, 91748},
	{0x27d, 91998},
	{0x27e, 92248},
	{0x27f, 92497},
	{0x280, 92746},
	{0x281, 92994},
	{0x282, 93241},
	{0x283, 93487},
	{0x284, 93733},
	{0x285, 93979},
	{0x286, 94223},
	{0x287, 94467},
	{0x288, 94710},
	{0x289, 94988},
	{0x28a, 95230},
	{0x28b, 95471},
	{0x28c, 95746},
	{0x28d, 95987},
	{0x28e, 96260},
	{0x28f, 96499},
	{0x290, 96771},
	{0x291, 97009},
	{0x292, 97279},
	{0x293, 97516},
	{0x294, 97785},
	{0x295, 98053},
	{0x296, 98321},
	{0x297, 98588},
	{0x298, 98821},
	{0x299, 99087},
	{0x29a, 99352},
	{0x29b, 99616},
	{0x29c, 99879},
	{0x29d, 100174},
	{0x29e, 100436},
	{0x29f, 100697},
	{0x2a0, 100958},
	{0x2a1, 101217},
	{0x2a2, 101509},
	{0x2a3, 101767},
	{0x2a4, 102056},
	{0x2a5, 102313},
	{0x2a6, 102601},
	{0x2a7, 102856},
	{0x2a8, 103142},
	{0x2a9, 103396},
	{0x2aa, 103681},
	{0x2ab, 103965},
	{0x2ac, 104247},
	{0x2ad, 104530},
	{0x2ae, 104811},
	{0x2af, 105091},
	{0x2b0, 105371},
	{0x2b1, 105650},
	{0x2b2, 105928},
	{0x2b3, 106205},
	{0x2b4, 106481},
	{0x2b5, 106787},
	{0x2b6, 107062},
	{0x2b7, 107335},
	{0x2b8, 107639},
	{0x2b9, 107911},
	{0x2ba, 108212},
	{0x2bb, 108513},
	{0x2bc, 108783},
	{0x2bd, 109081},
	{0x2be, 109379},
	{0x2bf, 109676},
	{0x2c0, 109972},
	{0x2c1, 110267},
	{0x2c2, 110561},
	{0x2c3, 110854},
	{0x2c4, 111176},
	{0x2c5, 111467},
	{0x2c6, 111757},
	{0x2c7, 112076},
	{0x2c8, 112364},
	{0x2c9, 112681},
	{0x2ca, 112967},
	{0x2cb, 113282},
	{0x2cc, 113595},
	{0x2cd, 113907},
	{0x2ce, 114190},
	{0x2cf, 114500},
	{0x2d0, 114810},
	{0x2d1, 115146},
	{0x2d2, 115453},
	{0x2d3, 115759},
	{0x2d4, 116065},
	{0x2d5, 116396},
	{0x2d6, 116700},
	{0x2d7, 117029},
	{0x2d8, 117330},
	{0x2d9, 117658},
	{0x2da, 117984},
	{0x2db, 118309},
	{0x2dc, 118633},
	{0x2dd, 118956},
	{0x2de, 119278},
	{0x2df, 119599},
	{0x2e0, 119945},
	{0x2e1, 120264},
	{0x2e2, 120581},
	{0x2e3, 120924},
	{0x2e4, 121265},
	{0x2e5, 121580},
	{0x2e6, 121919},
	{0x2e7, 122257},
	{0x2e8, 122593},
	{0x2e9, 122929},
	{0x2ea, 123263},
	{0x2eb, 123622},
	{0x2ec, 123954},
	{0x2ed, 124310},
	{0x2ee, 124639},
	{0x2ef, 124993},
	{0x2f0, 125345},
	{0x2f1, 125696},
	{0x2f2, 126046},
	{0x2f3, 126394},
	{0x2f4, 126741},
	{0x2f5, 127087},
	{0x2f6, 127456},
	{0x2f7, 127799},
	{0x2f8, 128165},
	{0x2f9, 128530},
	{0x2fa, 128870},
	{0x2fb, 129232},
	{0x2fc, 129593},
	{0x2fd, 129976},
	{0x2fe, 130334},
	{0x2ff, 130691},
	{0x300, 131070},
	{0x301, 131447},
	{0x302, 131799},
	{0x303, 132174},
	{0x304, 132547},
	{0x305, 132942},
	{0x306, 133312},
	{0x307, 133680},
	{0x308, 134071},
	{0x309, 134459},
	{0x30a, 134846},
	{0x30b, 135231},
	{0x30c, 135615},
	{0x30d, 135997},
	{0x30e, 136378},
	{0x30f, 136779},
	{0x310, 137179},
	{0x311, 137577},
	{0x312, 137973},
	{0x313, 138368},
	{0x314, 138761},
	{0x315, 139152},
	{0x316, 139564},
	{0x317, 139973},
	{0x318, 140381},
	{0x319, 140787},
	{0x31a, 141192},
	{0x31b, 141615},
	{0x31c, 142016},
	{0x31d, 142436},
	{0x31e, 142855},
	{0x31f, 143271},
	{0x320, 143686},
	{0x321, 144119},
	{0x322, 144550},
	{0x323, 144959},
	{0x324, 145407},
	{0x325, 145833},
	{0x326, 146257},
	{0x327, 146698},
	{0x328, 147138},
	{0x329, 147576},
	{0x32a, 148012},
	{0x32b, 148465},
	{0x32c, 148897},
	{0x32d, 149346},
	{0x32e, 149793},
	{0x32f, 150257},
	{0x330, 150700},
	{0x331, 151160},
	{0x332, 151617},
	{0x333, 152072},
	{0x334, 152544},
	{0x335, 152995},
	{0x336, 153463},
	{0x337, 153947},
	{0x338, 154409},
	{0x339, 154888},
	{0x33a, 155365},
	{0x33b, 155839},
	{0x33c, 156311},
	{0x33d, 156798},
	{0x33e, 157283},
	{0x33f, 157783},
	{0x340, 158263},
	{0x341, 158758},
	{0x342, 159251},
	{0x343, 159758},
	{0x344, 160263},
	{0x345, 160765},
	{0x346, 161264},
	{0x347, 161778},
	{0x348, 162289},
	{0x349, 162814},
	{0x34a, 163320},
	{0x34b, 163839},
	{0x34c, 164373},
	{0x34d, 164903},
	{0x34e, 165430},
	{0x34f, 165955},
	{0x350, 166493},
	{0x351, 167027},
	{0x352, 167575},
	{0x353, 168120},
	{0x354, 168661},
	{0x355, 169216},
	{0x356, 169782},
	{0x357, 170330},
	{0x358, 170890},
	{0x359, 171463},
	{0x35a, 172031},
	{0x35b, 172597},
	{0x35c, 173174},
	{0x35d, 173747},
	{0x35e, 174333},
	{0x35f, 174914},
	{0x360, 175507},
	{0x361, 176096},
	{0x362, 176696},
	{0x363, 177292},
	{0x364, 177899},
	{0x365, 178502},
	{0x366, 179116},
	{0x367, 179739},
	{0x368, 180359},
	{0x369, 180974},
	{0x36a, 181613},
	{0x36b, 182235},
	{0x36c, 182879},
	{0x36d, 183519},
	{0x36e, 184168},
	{0x36f, 184813},
	{0x370, 185467},
	{0x371, 186129},
	{0x372, 186787},
	{0x373, 187454},
	{0x374, 188128},
	{0x375, 188811},
	{0x376, 189489},
	{0x377, 190174},
	{0x378, 190867},
	{0x379, 191568},
	{0x37a, 192276},
	{0x37b, 192979},
	{0x37c, 193700},
	{0x37d, 194417},
	{0x37e, 195140},
	{0x37f, 195869},
	{0x380, 196605},
	{0x381, 197346},
	{0x382, 198094},
	{0x383, 198847},
	{0x384, 199606},
	{0x385, 200370},
	{0x386, 201139},
	{0x387, 201924},
	{0x388, 202703},
	{0x389, 203497},
	{0x38a, 204296},
	{0x38b, 205099},
	{0x38c, 205916},
	{0x38d, 206727},
	{0x38e, 207551},
	{0x38f, 208390},
	{0x390, 209231},
	{0x391, 210075},
	{0x392, 210932},
	{0x393, 211792},
	{0x394, 212663},
	{0x395, 213547},
	{0x396, 214432},
	{0x397, 215328},
	{0x398, 216235},
	{0x399, 217152},
	{0x39a, 218070},
	{0x39b, 219007},
	{0x39c, 219944},
	{0x39d, 220891},
	{0x39e, 221855},
	{0x39f, 222827},
	{0x3a0, 223807},
	{0x3a1, 224794},
	{0x3a2, 225798},
	{0x3a3, 226808},
	{0x3a4, 227824},
	{0x3a5, 228863},
	{0x3a6, 229908},
	{0x3a7, 230965},
	{0x3a8, 232028},
	{0x3a9, 233110},
	{0x3aa, 234204},
	{0x3ab, 235310},
	{0x3ac, 236425},
	{0x3ad, 237559},
	{0x3ae, 238709},
	{0x3af, 239868},
	{0x3b0, 241042},
	{0x3b1, 242231},
	{0x3b2, 243434},
	{0x3b3, 244658},
	{0x3b4, 245894},
	{0x3b5, 247142},
	{0x3b6, 248414},
	{0x3b7, 249697},
	{0x3b8, 251002},
	{0x3b9, 252329},
	{0x3ba, 253670},
	{0x3bb, 255030},
	{0x3bc, 256409},
	{0x3bd, 257811},
	{0x3be, 259229},
	{0x3bf, 260675},
	{0x3c0, 262140},
	{0x3c1, 263629},
	{0x3c2, 265141},
	{0x3c3, 266679},
	{0x3c4, 268243},
	{0x3c5, 269831},
	{0x3c6, 271446},
	{0x3c7, 273092},
	{0x3c8, 274766},
	{0x3c9, 276467},
	{0x3ca, 278203},
	{0x3cb, 279971},
	{0x3cc, 281770},
	{0x3cd, 283605},
	{0x3ce, 285479},
	{0x3cf, 287390},
	{0x3d0, 289338},
	{0x3d1, 291329},
	{0x3d2, 293363},
	{0x3d3, 295443},
	{0x3d4, 297567},
	{0x3d5, 299739},
	{0x3d6, 301964},
	{0x3d7, 304244},
	{0x3d8, 306577},
	{0x3d9, 308969},
	{0x3da, 311425},
	{0x3db, 313949},
	{0x3dc, 316537},
	{0x3dd, 319201},
	{0x3de, 321944},
	{0x3df, 324764},
	{0x3e0, 327675},
	{0x3e1, 330676},
	{0x3e2, 333775},
	{0x3e3, 336981},
	{0x3e4, 340298},
	{0x3e5, 343738},
	{0x3e6, 347307},
	{0x3e7, 351014},
	{0x3e8, 354875},
	{0x3e9, 358898},
	{0x3ea, 363100},
	{0x3eb, 367499},
	{0x3ec, 372112},
	{0x3ed, 376962},
	{0x3ee, 382074},
	{0x3ef, 387477},
	{0x3f0, 393210},
};


struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1087,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2960,
	.image_theight = 1632,
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

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME",
	.chip_id = 0x5003,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 1087,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.image_twidth = 2960,
		.image_theight = 1632,
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
	},
	//.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 393210,                //262144 327680
	.max_again_short = 393210,          //262144 327680
	.min_integration_time = 1,
	.min_integration_time_short = 1,
	.max_integration_time_short = 0x784 - 3,
	.min_integration_time_native = 1,
	.max_integration_time_native = 0x784 - 3,
	.integration_time_limit = 0x784 - 3,
	.total_width = 0xeb4,
	.total_height = 0x784,
	.max_integration_time = 0x784 - 3,
	.integration_time_apply_delay = 2,
	.expo_fs = 1,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	//.sensor_ctrl.alloc_integration_time_short = sensor_alloc_integration_time_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_2960_1632_30fps_mipi[] = {
	//raw10

	{0x300b, 0x01},
	{0x3c70, 0xee},
	{0x3006, 0x02},
	{0x300a, 0x01},
	{0x3014, 0x00},
	{0x330e, 0x00},
	{0x310a, 0x00},
	{0x3109, 0x02},
	{0x310c, 0x00},
	{0x310b, 0x00},
	{0x310f, 0x00},
	{0x3986, 0x02},
	{0x3986, 0x02},
	{0x3900, 0x00},
	{0x3902, 0x14},
	{0x3901, 0x00},
	{0x3904, 0x45},
	{0x3903, 0x05},
	{0x3906, 0xff},
	{0x3905, 0x1f},
	{0x3908, 0xff},
	{0x3907, 0x1f},
	{0x390a, 0xd2},
	{0x3909, 0x01},
	{0x390c, 0x00},
	{0x390b, 0x02},
	{0x390e, 0xff},
	{0x390d, 0x1f},
	{0x3910, 0xff},
	{0x390f, 0x1f},
	{0x3911, 0x01},
	{0x3917, 0x00},
	{0x3916, 0x00},
	{0x3919, 0xe4},
	{0x3918, 0x00},
	{0x3913, 0x0a},
	{0x3912, 0x00},
	{0x3915, 0xdc},
	{0x3914, 0x01},
	{0x391b, 0x00},
	{0x391a, 0x00},
	{0x391d, 0x4a},
	{0x391c, 0x05},
	{0x391f, 0xff},
	{0x391e, 0x1f},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x39af, 0x0a},
	{0x39ae, 0x00},
	{0x39b1, 0x40},
	{0x39b0, 0x05},
	{0x3923, 0x00},
	{0x3922, 0x00},
	{0x3925, 0xd5},
	{0x3924, 0x01},
	{0x394c, 0x00},
	{0x394e, 0x46},
	{0x394d, 0x00},
	{0x3950, 0x50},
	{0x394f, 0x00},
	{0x3952, 0x3c},
	{0x3951, 0x00},
	{0x3954, 0xf1},
	{0x3953, 0x01},
	{0x3927, 0x28},
	{0x3926, 0x00},
	{0x3929, 0x81},
	{0x3928, 0x00},
	{0x399e, 0x2d},
	{0x399d, 0x00},
	{0x39a0, 0x86},
	{0x399f, 0x00},
	{0x392b, 0xf4},
	{0x392a, 0x00},
	{0x392d, 0xc8},
	{0x392c, 0x01},
	{0x392f, 0x70},
	{0x392e, 0x02},
	{0x3931, 0x40},
	{0x3930, 0x05},
	{0x3933, 0x40},
	{0x3932, 0x05},
	{0x3935, 0x40},
	{0x3934, 0x05},
	{0x3937, 0x40},
	{0x3936, 0x05},
	{0x3939, 0x40},
	{0x3938, 0x05},
	{0x393b, 0x40},
	{0x393a, 0x05},
	{0x39b3, 0xbe},
	{0x39b2, 0x01},
	{0x39b5, 0xc8},
	{0x39b4, 0x01},
	{0x3991, 0xee},
	{0x3990, 0x00},
	{0x3993, 0xcb},
	{0x3992, 0x01},
	{0x3995, 0x66},
	{0x3994, 0x02},
	{0x3997, 0x43},
	{0x3996, 0x05},
	{0x393d, 0x5a},
	{0x393c, 0x00},
	{0x393f, 0xf8},
	{0x393e, 0x00},
	{0x3941, 0xd2},
	{0x3940, 0x01},
	{0x3943, 0x70},
	{0x3942, 0x02},
	{0x3945, 0x00},
	{0x3944, 0x00},
	{0x3947, 0x95},
	{0x3946, 0x00},
	{0x3949, 0x95},
	{0x3948, 0x00},
	{0x394b, 0x43},
	{0x394a, 0x05},
	{0x395a, 0x00},
	{0x3959, 0x00},
	{0x395c, 0x05},
	{0x395b, 0x00},
	{0x395e, 0xc6},
	{0x395d, 0x01},
	{0x3960, 0x5c},
	{0x395f, 0x02},
	{0x3956, 0x19},
	{0x3955, 0x00},
	{0x3958, 0x43},
	{0x3957, 0x05},
	{0x3962, 0x00},
	{0x3961, 0x00},
	{0x3964, 0x50},
	{0x3963, 0x00},
	{0x3966, 0x00},
	{0x3965, 0x00},
	{0x3968, 0x50},
	{0x3967, 0x00},
	{0x39a2, 0x05},
	{0x39a1, 0x00},
	{0x39a4, 0x55},
	{0x39a3, 0x00},
	{0x399a, 0x00},
	{0x3999, 0x00},
	{0x399c, 0x50},
	{0x399b, 0x00},
	{0x3989, 0x00},
	{0x3988, 0x00},
	{0x398b, 0x63},
	{0x398a, 0x00},
	{0x398d, 0x00},
	{0x398c, 0x00},
	{0x398f, 0x5a},
	{0x398e, 0x00},
	{0x396a, 0x5e},
	{0x3969, 0x05},
	{0x39b7, 0xcd},
	{0x39b6, 0x01},
	{0x39b9, 0xe1},
	{0x39b8, 0x01},
	{0x39bb, 0xd7},
	{0x39ba, 0x01},
	{0x39bd, 0xeb},
	{0x39bc, 0x01},
	{0x39bf, 0x00},
	{0x39be, 0x00},
	{0x39c1, 0x63},
	{0x39c0, 0x00},
	{0x39a6, 0x45},
	{0x39a5, 0x05},
	{0x396d, 0x90},
	{0x396c, 0x00},
	{0x396f, 0x90},
	{0x396e, 0x00},
	{0x3971, 0x90},
	{0x3970, 0x00},
	{0x3973, 0x90},
	{0x3972, 0x00},
	{0x3975, 0x90},
	{0x3974, 0x00},
	{0x3977, 0xC0},
	{0x3976, 0x00},
	{0x3979, 0x80},
	{0x3978, 0x01},
	{0x397b, 0x80},
	{0x397a, 0x01},
	{0x397d, 0x80},
	{0x397c, 0x01},
	{0x397f, 0x80},
	{0x397e, 0x01},
	{0x3981, 0x80},
	{0x3980, 0x01},
	{0x3983, 0x80},
	{0x3982, 0x01},
	{0x3985, 0x80},
	{0x3984, 0x05},
	{0x3c42, 0x03},
	{0x3012, 0x2b},
	{0x3600, 0x63},
	{0x364e, 0x63},
	{0x3a03, 0x3f},
	{0x3a05, 0xb8},
	{0x3a06, 0x03},
	{0x3a2d, 0x45},
	{0x3a31, 0x06},
	{0x3040, 0x01},
	{0x390c, 0x00},
	{0x3a31, 0x06},
	{0x2101, 0x01},
	{0x3a03, 0x0b},
	{0x3a04, 0x00},
	{0x3a05, 0xb8},
	{0x3a06, 0x03},
	{0x3a07, 0x56},
	{0x3a08, 0x32},
	{0x3a09, 0x10},
	{0x3a0b, 0x3a},
	{0x3a2d, 0x45},
	{0x3a31, 0x00},
	{0x3a33, 0x00},
	{0x3a34, 0x34},

	{0x3c00, 0x00},
	{0x3c01, 0x04},

	{0x3c0c, 0x28},//prepare 0x428
	{0x3c0d, 0x14},//
	{0x3c0e, 0x69},//zero 0x669
	{0x3c0f, 0x16},//
	{0x3c14, 0x62},//lxp 0x0032
	{0x3c15, 0x00},//

	{0x3c18, 0x01},//

	{0x3300, 0x78},
	{0x3301, 0x02},
	{0x3302, 0x00},
	{0x3303, 0x03},
	{0x3307, 0x00},
	{0x3309, 0x01},
	{0x3201, 0x84},//
	{0x3200, 0x07},//VTS
	{0x3203, 0xb4},//
	{0x3202, 0x0e},//HTS

	{0x3205, 0x02},//
	{0x3204, 0x00},//H_ST
	{0x3207, 0x61},//
	{0x3206, 0x06},//H_END {0x3209,   0x0c},//
	{0x3208, 0x00},//V_ST
	{0x320b, 0x9b},//
	{0x320a, 0x0b},//V_END {0x3006,   0x00},
	{SENSOR_REG_END, 0x00},
};


/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution]. */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	//[0]30fps linear
	{
		.width = 2960,
		.height = 1632,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2960_1632_30fps_mipi,
	},

};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_dvp[] = {
	{0x3006, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_dvp[] = {
	{0x3006, 0x02},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_on_mipi[] = {
	{0x3006, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x3006, 0x02},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

/*
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif
*/
static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
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

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}


static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	int expo = (value & 0xffff);
//	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0x3100, (unsigned char) ((expo >> 8) & 0xff));
	ret += sensor_write(sd, 0x3101, (unsigned char) (expo & 0xff));
//	ret += sensor_write(sd, 0x3109, (unsigned char)((again >> 8)& 0x03));
//	ret += sensor_write(sd, 0x310a, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x300c, 0x01);


	if (ret < 0)
		return ret;

	return 0;
}


static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	ret = sensor_write(sd, 0x310a, (unsigned char) (value & 0xff));
	ret += sensor_write(sd, 0x3109, (unsigned char) (value >> 8 & 0x03));
	ret += sensor_write(sd, 0x300c, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}


/*
static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}
 */
static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise) {
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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable) {
		return ISP_SUCCESS;
	} else {
		ret = sensor_write_array(sd, wsize->regs);
		sensor_set_attr(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		sensor->priv = wsize;
	}

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			//ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_on_dvp);
				//ISP_WARNING("*******DVP******\n");//add
			} else if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_on_mipi);
				*((u32 *) 0xb3380000) = 0x5;
				//ISP_WARNING("*******MIPI******\n");//add
			} else {
				ISP_ERROR("Don't support this Sensor Data interface\n");
			}
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}
	} else {
		if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
		sensor->video.state = TX_ISP_MODULE_INIT;
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps = 0;
	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch (info->default_boot) {
		case 0:
		case 1:
			max_fps = SENSOR_OUTPUT_MAX_FPS;
			sclk = SENSOR_SUPPORT_SCLK;
			break;
		case 2:
			max_fps = TX_SENSOR_MAX_FPS_90;
			sclk = SENSOR_SUPPORT_SCLK_90fps;
			break;
		case 3:
			max_fps = TX_SENSOR_MAX_FPS_120;
			sclk = SENSOR_SUPPORT_SCLK_120fps;
			break;
		default:
			ISP_WARNING("default boot select err!!!\n");
			break;
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret = sensor_read(sd, 0x3202, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x3203, &tmp);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}
	hts = (hts << 8) + tmp;

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x3201, (unsigned char) (vts & 0xff));
	ret += sensor_write(sd, 0x3200, (unsigned char) (vts >> 8));
	ret += sensor_write(sd, 0x300c, 0x01);
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	*((u32 *) 0xb3380000) = 0x5;
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 3;
	sensor->video.attr->integration_time_limit = vts - 3;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 3;
	sensor_set_attr(sd, wsize);
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
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

struct clk *sclka;

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi_linear),
			       sizeof(sensor_mipi_linear));
			sensor_attr.mipi.clk = 1087;
			sensor_attr.max_again = 393210;
			sensor_attr.max_dgain = 0;
			sensor_attr.integration_time_limit = 0x784 - 3;
			sensor_attr.max_integration_time_native = 0x784 - 3;
			sensor_attr.total_width = 0xeb4;
			sensor_attr.total_height = 0x784;
			sensor_attr.max_integration_time = 0x784 - 3;
			//pr_debug(SENSOR_NAME init set.../n");//add
			break;

		default:
			ISP_ERROR("Have no this setting!!!\n");
			break;
	}

	switch (info->video_interface) {
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

	rate = private_clk_get_rate(sensor->mclk);
	ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
	sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
	private_clk_set_rate(sclka, 1188000000);
	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
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
	if (chip) {
		memcpy(chip->name, SENSOR_NAME", sizeof(SENSOR_NAME"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {

	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
				ret = sensor_set_integration_time(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = sensor_set_analog_gain(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = sensor_set_digital_gain(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				//ret = sensor_get_black_pedestal(sd, sensor_val->value);
				break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			if (arg)
				ret = sensor_write_array(sd, sensor_stream_off_mipi);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			if (arg)
				ret = sensor_write_array(sd, sensor_stream_on_mipi);
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

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg) {
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

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg) {
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
	.g_register = sensor_g_register,
	.s_register = sensor_s_register,
};

static struct tx_isp_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
};

static struct tx_isp_subdev_sensor_ops sensor_sensor_ops = {
	.ioctl = sensor_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.sensor = &sensor_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = SENSOR_NAME",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
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
	sensor->dev = &client->dev;
	sensor->video.attr = &sensor_attr;
	sensor_attr.expo_fs = 1;
	sensor->dev = &client->dev;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client) {
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);

	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME",
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void) {
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
