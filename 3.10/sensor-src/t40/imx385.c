// SPDX-License-Identifier: GPL-2.0+
/*
 * imx385.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       30        mipi_2lane           linear
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
#include <txx-funcs.h>

#define SENSOR_NAME "imx385"
#define SENSOR_CHIP_ID_H (0x77)
#define SENSOR_CHIP_ID_L (0x27)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (123750000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define MCLK 24000000
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION "H20230725a"

static int shvflip = 0;

static int reset_gpio = GPIO_PC(28);
static int pwdn_gpio = -1;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0a, 0},
	{0x0b, 9011},
	{0x0c, 17238},
	{0x0d, 24806},
	{0x0e, 31812},
	{0x0f, 38336},
	{0x10, 44438},
	{0x11, 50170},
	{0x12, 55574},
	{0x13, 60686},
	{0x14, 65536},
	{0x15, 70149},
	{0x16, 74547},
	{0x17, 78750},
	{0x18, 82774},
	{0x19, 86633},
	{0x1a, 90342},
	{0x1b, 93910},
	{0x1c, 97348},
	{0x1d, 100666},
	{0x1e, 103872},
	{0x1f, 106972},
	{0x20, 109974},
	{0x21, 112883},
	{0x22, 115706},
	{0x23, 118446},
	{0x24, 121110},
	{0x25, 123700},
	{0x26, 126222},
	{0x27, 128678},
	{0x28, 131072},
	{0x29, 133406},
	{0x2a, 135685},
	{0x2b, 137909},
	{0x2c, 140083},
	{0x2d, 142208},
	{0x2e, 144286},
	{0x2f, 146319},
	{0x30, 148310},
	{0x31, 150259},
	{0x32, 152169},
	{0x33, 154042},
	{0x34, 155878},
	{0x35, 157679},
	{0x36, 159446},
	{0x37, 161181},
	{0x38, 162884},
	{0x39, 164558},
	{0x3a, 166202},
	{0x3b, 167819},
	{0x3c, 169408},
	{0x3d, 170970},
	{0x3e, 172508},
	{0x3f, 174021},
	{0x40, 175510},
	{0x41, 176976},
	{0x42, 178419},
	{0x43, 179841},
	{0x44, 181242},
	{0x45, 182622},
	{0x46, 183982},
	{0x47, 185323},
	{0x48, 186646},
	{0x49, 187950},
	{0x4a, 189236},
	{0x4b, 190505},
	{0x4c, 191758},
	{0x4d, 192994},
	{0x4e, 194214},
	{0x4f, 195418},
	{0x50, 196608},
	{0x51, 197782},
	{0x52, 198942},
	{0x53, 200088},
	{0x54, 201221},
	{0x55, 202339},
	{0x56, 203445},
	{0x57, 204538},
	{0x58, 205619},
	{0x59, 206687},
	{0x5a, 207744},
	{0x5b, 208788},
	{0x5c, 209822},
	{0x5d, 210844},
	{0x5e, 211855},
	{0x5f, 212856},
	{0x60, 213846},
	{0x61, 214826},
	{0x62, 215795},
	{0x63, 216755},
	{0x64, 217705},
	{0x65, 218646},
	{0x66, 219578},
	{0x67, 220500},
	{0x68, 221414},
	{0x69, 222318},
	{0x6a, 223215},
	{0x6b, 224102},
	{0x6c, 224982},
	{0x6d, 225853},
	{0x6e, 226717},
	{0x6f, 227572},
	{0x70, 228420},
	{0x71, 229261},
	{0x72, 230094},
	{0x73, 230920},
	{0x74, 231738},
	{0x75, 232550},
	{0x76, 233355},
	{0x77, 234152},
	{0x78, 234944},
	{0x79, 235728},
	{0x7a, 236506},
	{0x7b, 237278},
	{0x7c, 238044},
	{0x7d, 238803},
	{0x7e, 239557},
	{0x7f, 240304},
	{0x80, 241046},
	{0x81, 241781},
	{0x82, 242512},
	{0x83, 243236},
	{0x84, 243955},
	{0x85, 244669},
	{0x86, 245377},
	{0x87, 246080},
	{0x88, 246778},
	{0x89, 247470},
	{0x8a, 248158},
	{0x8b, 248841},
	{0x8c, 249518},
	{0x8d, 250191},
	{0x8e, 250859},
	{0x8f, 251523},
	{0x90, 252182},
	{0x91, 252836},
	{0x92, 253486},
	{0x93, 254131},
	{0x94, 254772},
	{0x95, 255409},
	{0x96, 256041},
	{0x97, 256670},
	{0x98, 257294},
	{0x99, 257914},
	{0x9a, 258530},
	{0x9b, 259142},
	{0x9c, 259750},
	{0x9d, 260354},
	{0x9e, 260954},
	{0x9f, 261551},
	{0xa0, 262144},
	{0xa1, 262733},
	{0xa2, 263318},
	{0xa3, 263900},
	{0xa4, 264478},
	{0xa5, 265053},
	{0xa6, 265624},
	{0xa7, 266192},
	{0xa8, 266757},
	{0xa9, 267318},
	{0xaa, 267875},
	{0xab, 268430},
	{0xac, 268981},
	{0xad, 269529},
	{0xae, 270074},
	{0xaf, 270616},
	{0xb0, 271155},
	{0xb1, 271691},
	{0xb2, 272223},
	{0xb3, 272753},
	{0xb4, 273280},
	{0xb5, 273804},
	{0xb6, 274324},
	{0xb7, 274843},
	{0xb8, 275358},
	{0xb9, 275870},
	{0xba, 276380},
	{0xbb, 276887},
	{0xbc, 277391},
	{0xbd, 277893},
	{0xbe, 278392},
	{0xbf, 278888},
	{0xc0, 279382},
	{0xc1, 279873},
	{0xc2, 280362},
	{0xc3, 280848},
	{0xc4, 281331},
	{0xc5, 281812},
	{0xc6, 282291},
	{0xc7, 282767},
	{0xc8, 283241},
	{0xc9, 283713},
	{0xca, 284182},
	{0xcb, 284649},
	{0xcc, 285114},
	{0xcd, 285576},
	{0xce, 286036},
	{0xcf, 286494},
	{0xd0, 286950},
	{0xd1, 287403},
	{0xd2, 287854},
	{0xd3, 288304},
	{0xd4, 288751},
	{0xd5, 289196},
	{0xd6, 289638},
	{0xd7, 290079},
	{0xd8, 290518},
	{0xd9, 290955},
	{0xda, 291389},
	{0xdb, 291822},
	{0xdc, 292253},
	{0xdd, 292682},
	{0xde, 293108},
	{0xdf, 293533},
	{0xe0, 293956},
	{0xe1, 294378},
	{0xe2, 294797},
	{0xe3, 295214},
	{0xe4, 295630},
	{0xe5, 296044},
	{0xe6, 296456},
	{0xe7, 296866},
	{0xe8, 297274},
	{0xe9, 297681},
	{0xea, 298086},
	{0xeb, 298489},
	{0xec, 298891},
	{0xed, 299290},
	{0xee, 299688},
	{0xef, 300085},
	{0xf0, 300480},
	{0xf1, 300873},
	{0xf2, 301264},
	{0xf3, 301654},
	{0xf4, 302042},
	{0xf5, 302429},
	{0xf6, 302814},
	{0xf7, 303198},
	{0xf8, 303580},
	{0xf9, 303960},
	{0xfa, 304339},
	{0xfb, 304717},
	{0xfc, 305093},
	{0xfd, 305467},
	{0xfe, 305840},
	{0xff, 306212},
	{0x100, 306582},
	{0x101, 306950},
	{0x102, 307317},
	{0x103, 307683},
	{0x104, 308048},
	{0x105, 308410},
	{0x106, 308772},
	{0x107, 309132},
	{0x108, 309491},
	{0x109, 309849},
	{0x10a, 310205},
	{0x10b, 310559},
	{0x10c, 310913},
	{0x10d, 311265},
	{0x10e, 311616},
	{0x10f, 311965},
	{0x110, 312314},
	{0x111, 312661},
	{0x112, 313006},
	{0x113, 313351},
	{0x114, 313694},
	{0x115, 314036},
	{0x116, 314377},
	{0x117, 314716},
	{0x118, 315054},
	{0x119, 315391},
	{0x11a, 315727},
	{0x11b, 316062},
	{0x11c, 316395},
	{0x11d, 316728},
	{0x11e, 317059},
	{0x11f, 317389},
	{0x120, 317718},
	{0x121, 318046},
	{0x122, 318372},
	{0x123, 318698},
	{0x124, 319022},
	{0x125, 319345},
	{0x126, 319667},
	{0x127, 319988},
	{0x128, 320308},
	{0x129, 320627},
	{0x12a, 320945},
	{0x12b, 321262},
	{0x12c, 321577},
	{0x12d, 321892},
	{0x12e, 322206},
	{0x12f, 322518},
	{0x130, 322830},
	{0x131, 323140},
	{0x132, 323450},
	{0x133, 323758},
	{0x134, 324066},
	{0x135, 324372},
	{0x136, 324678},
	{0x137, 324982},
	{0x138, 325286},
	{0x139, 325588},
	{0x13a, 325890},
	{0x13b, 326191},
	{0x13c, 326490},
	{0x13d, 326789},
	{0x13e, 327087},
	{0x13f, 327384},
	{0x140, 327680},
	{0x141, 327975},
	{0x142, 328269},
	{0x143, 328562},
	{0x144, 328854},
	{0x145, 329145},
	{0x146, 329436},
	{0x147, 329725},
	{0x148, 330014},
	{0x149, 330302},
	{0x14a, 330589},
	{0x14b, 330875},
	{0x14c, 331160},
	{0x14d, 331445},
	{0x14e, 331728},
	{0x14f, 332011},
	{0x150, 332293},
	{0x151, 332574},
	{0x152, 332854},
	{0x153, 333133},
	{0x154, 333411},
	{0x155, 333689},
	{0x156, 333966},
	{0x157, 334242},
	{0x158, 334517},
	{0x159, 334792},
	{0x15a, 335065},
	{0x15b, 335338},
	{0x15c, 335610},
	{0x15d, 335882},
	{0x15e, 336152},
	{0x15f, 336422},
	{0x160, 336691},
	{0x161, 336959},
	{0x162, 337227},
	{0x163, 337493},
	{0x164, 337759},
	{0x165, 338025},
	{0x166, 338289},
	{0x167, 338553},
	{0x168, 338816},
	{0x169, 339078},
	{0x16a, 339340},
	{0x16b, 339600},
	{0x16c, 339860},
	{0x16d, 340120},
	{0x16e, 340379},
	{0x16f, 340637},
	{0x170, 340894},
	{0x171, 341150},
	{0x172, 341406},
	{0x173, 341661},
	{0x174, 341916},
	{0x175, 342170},
	{0x176, 342423},
	{0x177, 342675},
	{0x178, 342927},
	{0x179, 343178},
	{0x17a, 343429},
	{0x17b, 343679},
	{0x17c, 343928},
	{0x17d, 344176},
	{0x17e, 344424},
	{0x17f, 344671},
	{0x180, 344918},
	{0x181, 345164},
	{0x182, 345409},
	{0x183, 345654},
	{0x184, 345898},
	{0x185, 346141},
	{0x186, 346384},
	{0x187, 346626},
	{0x188, 346867},
	{0x189, 347108},
	{0x18a, 347348},
	{0x18b, 347588},
	{0x18c, 347827},
	{0x18d, 348066},
	{0x18e, 348303},
	{0x18f, 348541},
	{0x190, 348777},
	{0x191, 349013},
	{0x192, 349249},
	{0x193, 349484},
	{0x194, 349718},
	{0x195, 349952},
	{0x196, 350185},
	{0x197, 350418},
	{0x198, 350650},
	{0x199, 350881},
	{0x19a, 351112},
	{0x19b, 351342},
	{0x19c, 351572},
	{0x19d, 351801},
	{0x19e, 352030},
	{0x19f, 352258},
	{0x1a0, 352486},
	{0x1a1, 352713},
	{0x1a2, 352939},
	{0x1a3, 353165},
	{0x1a4, 353390},
	{0x1a5, 353615},
	{0x1a6, 353840},
	{0x1a7, 354063},
	{0x1a8, 354287},
	{0x1a9, 354509},
	{0x1aa, 354732},
	{0x1ab, 354953},
	{0x1ac, 355174},
	{0x1ad, 355395},
	{0x1ae, 355615},
	{0x1af, 355835},
	{0x1b0, 356054},
	{0x1b1, 356273},
	{0x1b2, 356491},
	{0x1b3, 356708},
	{0x1b4, 356925},
	{0x1b5, 357142},
	{0x1b6, 357358},
	{0x1b7, 357574},
	{0x1b8, 357789},
	{0x1b9, 358003},
	{0x1ba, 358218},
	{0x1bb, 358431},
	{0x1bc, 358644},
	{0x1bd, 358857},
	{0x1be, 359069},
	{0x1bf, 359281},
	{0x1c0, 359492},
	{0x1c1, 359703},
	{0x1c2, 359914},
	{0x1c3, 360123},
	{0x1c4, 360333},
	{0x1c5, 360542},
	{0x1c6, 360750},
	{0x1c7, 360958},
	{0x1c8, 361166},
	{0x1c9, 361373},
	{0x1ca, 361580},
	{0x1cb, 361786},
	{0x1cc, 361992},
	{0x1cd, 362197},
	{0x1ce, 362402},
	{0x1cf, 362606},
	{0x1d0, 362810},
	{0x1d1, 363014},
	{0x1d2, 363217},
	{0x1d3, 363420},
	{0x1d4, 363622},
	{0x1d5, 363824},
	{0x1d6, 364025},
	{0x1d7, 364226},
	{0x1d8, 364427},
	{0x1d9, 364627},
	{0x1da, 364826},
	{0x1db, 365026},
	{0x1dc, 365224},
	{0x1dd, 365423},
	{0x1de, 365621},
	{0x1df, 365818},
	{0x1e0, 366016},
	{0x1e1, 366212},
	{0x1e2, 366409},
	{0x1e3, 366605},
	{0x1e4, 366800},
	{0x1e5, 366995},
	{0x1e6, 367190},
	{0x1e7, 367384},
	{0x1e8, 367578},
	{0x1e9, 367772},
	{0x1ea, 367965},
	{0x1eb, 368158},
	{0x1ec, 368350},
	{0x1ed, 368542},
	{0x1ee, 368734},
	{0x1ef, 368925},
	{0x1f0, 369116},
	{0x1f1, 369306},
	{0x1f2, 369496},
	{0x1f3, 369686},
	{0x1f4, 369875},
	{0x1f5, 370064},
	{0x1f6, 370253},
	{0x1f7, 370441},
	{0x1f8, 370629},
	{0x1f9, 370816},
	{0x1fa, 371003},
	{0x1fb, 371190},
	{0x1fc, 371376},
	{0x1fd, 371562},
	{0x1fe, 371748},
	{0x1ff, 371933},
	{0x200, 372118},
	{0x201, 372302},
	{0x202, 372486},
	{0x203, 372670},
	{0x204, 372853},
	{0x205, 373036},
	{0x206, 373219},
	{0x207, 373402},
	{0x208, 373584},
	{0x209, 373765},
	{0x20a, 373946},
	{0x20b, 374127},
	{0x20c, 374308},
	{0x20d, 374488},
	{0x20e, 374668},
	{0x20f, 374848},
	{0x210, 375027},
	{0x211, 375206},
	{0x212, 375385},
	{0x213, 375563},
	{0x214, 375741},
	{0x215, 375918},
	{0x216, 376095},
	{0x217, 376272},
	{0x218, 376449},
	{0x219, 376625},
	{0x21a, 376801},
	{0x21b, 376977},
	{0x21c, 377152},
	{0x21d, 377327},
	{0x21e, 377501},
	{0x21f, 377676},
	{0x220, 377850},
	{0x221, 378023},
	{0x222, 378197},
	{0x223, 378370},
	{0x224, 378542},
	{0x225, 378715},
	{0x226, 378887},
	{0x227, 379058},
	{0x228, 379230},
	{0x229, 379401},
	{0x22a, 379572},
	{0x22b, 379742},
	{0x22c, 379913},
	{0x22d, 380082},
	{0x22e, 380252},
	{0x22f, 380421},
	{0x230, 380590},
	{0x231, 380759},
	{0x232, 380927},
	{0x233, 381095},
	{0x234, 381263},
	{0x235, 381431},
	{0x236, 381598},
	{0x237, 381765},
	{0x238, 381931},
	{0x239, 382098},
	{0x23a, 382264},
	{0x23b, 382430},
	{0x23c, 382595},
	{0x23d, 382760},
	{0x23e, 382925},
	{0x23f, 383090},
	{0x240, 383254},
	{0x241, 383418},
	{0x242, 383582},
	{0x243, 383745},
	{0x244, 383908},
	{0x245, 384071},
	{0x246, 384234},
	{0x247, 384396},
	{0x248, 384558},
	{0x249, 384720},
	{0x24a, 384881},
	{0x24b, 385042},
	{0x24c, 385203},
	{0x24d, 385364},
	{0x24e, 385524},
	{0x24f, 385685},
	{0x250, 385844},
	{0x251, 386004},
	{0x252, 386163},
	{0x253, 386322},
	{0x254, 386481},
	{0x255, 386640},
	{0x256, 386798},
	{0x257, 386956},
	{0x258, 387113},
	{0x259, 387271},
	{0x25a, 387428},
	{0x25b, 387585},
	{0x25c, 387742},
	{0x25d, 387898},
	{0x25e, 388054},
	{0x25f, 388210},
	{0x260, 388366},
	{0x261, 388521},
	{0x262, 388676},
	{0x263, 388831},
	{0x264, 388986},
	{0x265, 389140},
	{0x266, 389294},
	{0x267, 389448},
	{0x268, 389602},
	{0x269, 389755},
	{0x26a, 389908},
	{0x26b, 390061},
	{0x26c, 390214},
	{0x26d, 390366},
	{0x26e, 390518},
	{0x26f, 390670},
	{0x270, 390822},
	{0x271, 390973},
	{0x272, 391124},
	{0x273, 391275},
	{0x274, 391426},
	{0x275, 391576},
	{0x276, 391727},
	{0x277, 391876},
	{0x278, 392026},
	{0x279, 392176},
	{0x27a, 392325},
	{0x27b, 392474},
	{0x27c, 392623},
	{0x27d, 392771},
	{0x27e, 392920},
	{0x27f, 393068},
	{0x280, 393216},
	{0x281, 393363},
	{0x282, 393511},
	{0x283, 393658},
	{0x284, 393805},
	{0x285, 393951},
	{0x286, 394098},
	{0x287, 394244},
	{0x288, 394390},
	{0x289, 394536},
	{0x28a, 394681},
	{0x28b, 394827},
	{0x28c, 394972},
	{0x28d, 395117},
	{0x28e, 395261},
	{0x28f, 395406},
	{0x290, 395550},
	{0x291, 395694},
	{0x292, 395838},
	{0x293, 395982},
	{0x294, 396125},
	{0x295, 396268},
	{0x296, 396411},
	{0x297, 396554},
	{0x298, 396696},
	{0x299, 396839},
	{0x29a, 396981},
	{0x29b, 397122},
	{0x29c, 397264},
	{0x29d, 397406},
	{0x29e, 397547},
	{0x29f, 397688},
	{0x2a0, 397829},
	{0x2a1, 397969},
	{0x2a2, 398110},
	{0x2a3, 398250},
	{0x2a4, 398390},
	{0x2a5, 398529},
	{0x2a6, 398669},
	{0x2a7, 398808},
	{0x2a8, 398947},
	{0x2a9, 399086},
	{0x2aa, 399225},
	{0x2ab, 399364},
	{0x2ac, 399502},
	{0x2ad, 399640},
	{0x2ae, 399778},
	{0x2af, 399916},
	{0x2b0, 400053},
	{0x2b1, 400191},
	{0x2b2, 400328},
	{0x2b3, 400465},
	{0x2b4, 400601},
	{0x2b5, 400738},
	{0x2b6, 400874},
	{0x2b7, 401010},
	{0x2b8, 401146},
	{0x2b9, 401282},
	{0x2ba, 401418},
	{0x2bb, 401553},
	{0x2bc, 401688},
	{0x2bd, 401823},
	{0x2be, 401958},
	{0x2bf, 402093},
	{0x2c0, 402227},
	{0x2c1, 402361},
	{0x2c2, 402495},
	{0x2c3, 402629},
	{0x2c4, 402763},
	{0x2c5, 402896},
	{0x2c6, 403029},
	{0x2c7, 403162},
	{0x2c8, 403295},
	{0x2c9, 403428},
	{0x2ca, 403561},
	{0x2cb, 403693},
	{0x2cc, 403825},
	{0x2cd, 403957},
	{0x2ce, 404089},
	{0x2cf, 404220},
	{0x2d0, 404352},
};


struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
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

struct tx_isp_mipi_bus sensor_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,//SENSOR_MIPI_SONY_MODE
	.clk = 800,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 1952,
	.image_theight = 1080,
	.mipi_sc.mipi_crop_start0x = 4,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.data_type_en = 1,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x7727,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 404352,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1123,
	.integration_time_limit = 1123,
	.total_width = 4400,
	.total_height = 1125,
	.max_integration_time = 1123,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	{0x3000,0x01},
	{0x3001,0x00},
	{0x3002,0x01},
	{0x3005,0x00},
	{0x3007,0x00},
	{0x3009,0x02},
	{0x300A,0xF0},
	{0x300B,0x00},
	{0x3012,0x2C},
	{0x3013,0x01},
	{0x3014,0x00},
	{0x3015,0x00},
	{0x3016,0x08},
	{0x3018,0x65},//vts 0x465 = 1125
	{0x3019,0x04},
	{0x301A,0x00},
	{0x301B,0x30},//hts 0x1130 = 4400
	{0x301C,0x11},
	{0x3020,0x00},
	{0x3021,0x00},
	{0x3022,0x00},
	{0x3036,0x10},
	{0x303A,0xD1},
	{0x303B,0x03},
	{0x3044,0x01},
	{0x3046,0x00},
	{0x3047,0x08},
	{0x3049,0x00},
	{0x3054,0x66},
	{0x305C,0x28},
	{0x305D,0x00},
	{0x305E,0x20},
	{0x305F,0x00},
	{0x310B,0x07},
	{0x3110,0x12},
	{0x31ED,0x38},
	{0x3338,0xD4},
	{0x3339,0x40},
	{0x333A,0x10},
	{0x333B,0x00},
	{0x333C,0xD4},
	{0x333D,0x40},
	{0x333E,0x10},
	{0x333F,0x00},
	{0x3344,0x10},
	{0x3346,0x01},
	{0x3353,0x0E},
	{0x3357,0x49},
	{0x3358,0x04},
	{0x336B,0x37},
	{0x336C,0x1F},
	{0x337D,0x0A},
	{0x337E,0x0A},
	{0x337F,0x01},
	{0x3380,0x20},
	{0x3381,0x25},
	{0x3382,0x5F},
	{0x3383,0x1F},
	{0x3384,0x37},
	{0x3385,0x1F},
	{0x3386,0x1F},
	{0x3387,0x17},
	{0x3388,0x67},
	{0x3389,0x27},
	{0x338D,0xB4},
	{0x338E,0x01},
	{0x3000,0x00},
	{0x3002,0x00},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
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

#if 0
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
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

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3388, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3389, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;
	vmax = sensor_attr.total_height;
	shs = vmax - value;
	ret = sensor_write(sd, 0x3020, (unsigned char)(shs & 0xff));
	ret += sensor_write(sd, 0x3021, (unsigned char)((shs >> 8) & 0xff));
	ret += sensor_write(sd, 0x3022, (unsigned char)((shs >> 16) & 0x01));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret += sensor_write(sd, 0x3014, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3015, (unsigned char)((value >> 8) & 0x03));
	if (ret < 0)
		return ret;

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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}

	} else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot) {
	case 0:
		wpclk = SENSOR_SUPPORT_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}
	ret += sensor_read(sd, 0x301c, &tmp);
	hts = tmp & 0x3f;
	ret += sensor_read(sd, 0x301b, &tmp);
	if (ret < 0)
		return -1;
	hts = (hts << 8) + tmp;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x301A, (unsigned char)((vts & 0x10000) >> 16));
	ret += sensor_write(sd, 0x3019, (unsigned char)((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x3018, (unsigned char)(vts & 0xff));
	if (ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts- 2;
	sensor->video.attr->integration_time_limit = vts - 2;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 2;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;

	/* 2'b01:mirror,2'b10:filp */
	val = sensor_read(sd, 0x3007, &val);
	switch(enable) {
	case 0://normal
		val &= 0xfc;
		break;
	case 1://sensor mirror
		val = ((val & 0xFD) | 0x01);
		break;
	case 2://sensor flip
		val = ((val & 0xFE) | 0x02);
		break;
	case 3://sensor mirror&flip
		val |= 0x03;
		break;
	}
	ret = sensor_write(sd, 0x3007, val);

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

	memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.min_integration_time = 2;
		sensor_attr.min_integration_time_native = 2;
		sensor_attr.max_integration_time_native = 1123l;
		sensor_attr.integration_time_limit = 1123;
		sensor_attr.total_width = 4400;
		sensor_attr.total_height = 1125;
		sensor_attr.max_integration_time = 1123;
		sensor_attr.integration_time = 800;
		sensor_attr.again = 0;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 0;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->mclk) {
	case TISP_SENSOR_MCLK0:
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
		set_sensor_mclk_function(1);
		break;
	case TISP_SENSOR_MCLK2:
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
		set_sensor_mclk_function(2);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	if (((rate / 1000) % 31725) != 0) {
		ret = clk_set_parent(sclka, clk_get(NULL, "epll"));
		sclka = private_devm_clk_get(&client->dev, "epll");
		if (IS_ERR(sclka)) {
			pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclka);
			if (((rate / 1000) % 31725) != 0) {
				private_clk_set_rate(sclka, 891000000);
			}
		}
	}

	private_clk_set_rate(sensor->mclk, 31725000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(100);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
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

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{

	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
		/*	case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
			ret = sensor_set_expo(sd, sensor_val->value);
			break;   */
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
			ret = sensor_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

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


static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

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
