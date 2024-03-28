// SPDX-License-Identifier: GPL-2.0+
/*
 * imx385.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
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

#define SENSOR_NAME "imx385"
#define SENSOR_CHIP_ID_H (0xf0)
#define SENSOR_CHIP_ID_L (0x00)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_PCLK (74250*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define AGAIN_MAX_DB 0x1e
#define DGAIN_MAX_DB 0x2a
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION "H20190531a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	/* Analog */
	{0x0, 0},
	{0x1, 1088},
	{0x2, 2177},
	{0x3, 3265},
	{0x4, 4354},
	{0x5, 5442},
	{0x6, 6531},
	{0x7, 7619},
	{0x8, 8708},
	{0x9, 9796},
	{0xa, 10885},
	{0xb, 11973},
	{0xc, 13062},
	{0xd, 14150},
	{0xe, 15239},
	{0xf, 16327},
	{0x10, 17416},
	{0x11, 18504},
	{0x12, 19593},
	{0x13, 20682},
	{0x14, 21770},
	{0x15, 22859},
	{0x16, 23947},
	{0x17, 25036},
	{0x18, 26124},
	{0x19, 27213},
	{0x1a, 28301},
	{0x1b, 29390},
	{0x1c, 30478},
	{0x1d, 31567},
	{0x1e, 32655},
	{0x1f, 33744},
	{0x20, 34832},
	{0x21, 35921},
	{0x22, 37009},
	{0x23, 38098},
	{0x24, 39187},
	{0x25, 40275},
	{0x26, 41364},
	{0x27, 42452},
	{0x28, 43541},
	{0x29, 44629},
	{0x2a, 45718},
	{0x2b, 46806},
	{0x2c, 47895},
	{0x2d, 48983},
	{0x2e, 50072},
	{0x2f, 51160},
	{0x30, 52249},
	{0x31, 53337},
	{0x32, 54426},
	{0x33, 55514},
	{0x34, 56603},
	{0x35, 57692},
	{0x36, 58780},
	{0x37, 59869},
	{0x38, 60957},
	{0x39, 62046},
	{0x3a, 63134},
	{0x3b, 64223},
	{0x3c, 65311},
	{0x3d, 66400},
	{0x3e, 67488},
	{0x3f, 68577},
	{0x40, 69665},
	{0x41, 70754},
	{0x42, 71842},
	{0x43, 72931},
	{0x44, 74019},
	{0x45, 75108},
	{0x46, 76197},
	{0x47, 77285},
	{0x48, 78374},
	{0x49, 79462},
	{0x4a, 80551},
	{0x4b, 81639},
	{0x4c, 82728},
	{0x4d, 83816},
	{0x4e, 84905},
	{0x4f, 85993},
	{0x50, 87082},
	{0x51, 88170},
	{0x52, 89259},
	{0x53, 90347},
	{0x54, 91436},
	{0x55, 92524},
	{0x56, 93613},
	{0x57, 94702},
	{0x58, 95790},
	{0x59, 96879},
	{0x5a, 97967},
	{0x5b, 99056},
	{0x5c, 100144},
	{0x5d, 101233},
	{0x5e, 102321},
	{0x5f, 103410},
	{0x60, 104498},
	{0x61, 105587},
	{0x62, 106675},
	{0x63, 107764},
	{0x64, 108852},
	{0x65, 109941},
	{0x66, 111029},
	{0x67, 112118},
	{0x68, 113207},
	{0x69, 114295},
	{0x6a, 115384},
	{0x6b, 116472},
	{0x6c, 117561},
	{0x6d, 118649},
	{0x6e, 119738},
	{0x6f, 120826},
	{0x70, 121915},
	{0x71, 123003},
	{0x72, 124092},
	{0x73, 125180},
	{0x74, 126269},
	{0x75, 127357},
	{0x76, 128446},
	{0x77, 129534},
	{0x78, 130623},
	{0x79, 131712},
	{0x7a, 132800},
	{0x7b, 133889},
	{0x7c, 134977},
	{0x7d, 136066},
	{0x7e, 137154},
	{0x7f, 138243},
	{0x80, 139331},
	{0x81, 140420},
	{0x82, 141508},
	{0x83, 142597},
	{0x84, 143685},
	{0x85, 144774},
	{0x86, 145862},
	{0x87, 146951},
	{0x88, 148039},
	{0x89, 149128},
	{0x8a, 150217},
	{0x8b, 151305},
	{0x8c, 152394},
	{0x8d, 153482},
	{0x8e, 154571},
	{0x8f, 155659},
	{0x90, 156748},
	{0x91, 157836},
	{0x92, 158925},
	{0x93, 160013},
	{0x94, 161102},
	{0x95, 162190},
	{0x96, 163279},
	{0x97, 164367},
	{0x98, 165456},
	{0x99, 166544},
	{0x9a, 167633},
	{0x9b, 168722},
	{0x9c, 169810},
	{0x9d, 170899},
	{0x9e, 171987},
	{0x9f, 173076},
	{0xa0, 174164},
	{0xa1, 175253},
	{0xa2, 176341},
	{0xa3, 177430},
	{0xa4, 178518},
	{0xa5, 179607},
	{0xa6, 180695},
	{0xa7, 181784},
	{0xa8, 182872},
	{0xa9, 183961},
	{0xaa, 185049},
	{0xab, 186138},
	{0xac, 187227},
	{0xad, 188315},
	{0xae, 189404},
	{0xaf, 190492},
	{0xb0, 191581},
	{0xb1, 192669},
	{0xb2, 193758},
	{0xb3, 194846},
	{0xb4, 195935},
	{0xb5, 197023},
	{0xb6, 198112},
	{0xb7, 199200},
	{0xb8, 200289},
	{0xb9, 201377},
	{0xba, 202466},
	{0xbb, 203554},
	{0xbc, 204643},
	{0xbd, 205732},
	{0xbe, 206820},
	{0xbf, 207909},
	{0xc0, 208997},
	{0xc1, 210086},
	{0xc2, 211174},
	{0xc3, 212263},
	{0xc4, 213351},
	{0xc5, 214440},
	{0xc6, 215528},
	{0xc7, 216617},
	{0xc8, 217705},
	{0xc9, 218794},
	{0xca, 219882},
	{0xcb, 220971},
	{0xcc, 222059},
	{0xcd, 223148},
	{0xce, 224237},
	{0xcf, 225325},
	{0xd0, 226414},
	{0xd1, 227502},
	{0xd2, 228591},
	{0xd3, 229679},
	{0xd4, 230768},
	{0xd5, 231856},
	{0xd6, 232945},
	{0xd7, 234033},
	{0xd8, 235122},
	{0xd9, 236210},
	{0xda, 237299},
	{0xdb, 238387},
	{0xdc, 239476},
	{0xdd, 240564},
	{0xde, 241653},
	{0xdf, 242742},
	{0xe0, 243830},
	{0xe1, 244919},
	{0xe2, 246007},
	{0xe3, 247096},
	{0xe4, 248184},
	{0xe5, 249273},
	{0xe6, 250361},
	{0xe7, 251450},
	{0xe8, 252538},
	{0xe9, 253627},
	{0xea, 254715},
	{0xeb, 255804},
	{0xec, 256892},
	{0xed, 257981},
	{0xee, 259069},
	{0xef, 260158},
	{0xf0, 261247},
	{0xf1, 262335},
	{0xf2, 263424},
	{0xf3, 264512},
	{0xf4, 265601},
	{0xf5, 266689},
	{0xf6, 267778},
	{0xf7, 268866},
	{0xf8, 269955},
	{0xf9, 271043},
	{0xfa, 272132},
	{0xfb, 273220},
	{0xfc, 274309},
	{0xfd, 275397},
	{0xfe, 276486},
	{0xff, 277574},
	{0x100, 278663},
	{0x101, 279752},
	{0x102, 280840},
	{0x103, 281929},
	{0x104, 283017},
	{0x105, 284106},
	{0x106, 285194},
	{0x107, 286283},
	{0x108, 287371},
	{0x109, 288460},
	{0x10a, 289548},
	{0x10b, 290637},
	{0x10c, 291725},
	{0x10d, 292814},
	{0x10e, 293902},
	{0x10f, 294991},
	{0x110, 296079},
	{0x111, 297168},
	{0x112, 298257},
	{0x113, 299345},
	{0x114, 300434},
	{0x115, 301522},
	{0x116, 302611},
	{0x117, 303699},
	{0x118, 304788},
	{0x119, 305876},
	{0x11a, 306965},
	{0x11b, 308053},
	{0x11c, 309142},
	{0x11d, 310230},
	{0x11e, 311319},
	{0x11f, 312407},
	{0x120, 313496},
	{0x121, 314584},
	{0x122, 315673},
	{0x123, 316762},
	{0x124, 317850},
	{0x125, 318939},
	{0x126, 320027},
	{0x127, 321116},
	{0x128, 322204},
	{0x129, 323293},
	{0x12a, 324381},
	{0x12b, 325470},
	{0x12c, 326558},
	/* Analog + Digital */
	{0x12d, 327647},
	{0x12e, 328735},
	{0x12f, 329824},
	{0x130, 330912},
	{0x131, 332001},
	{0x132, 333089},
	{0x133, 334178},
	{0x134, 335267},
	{0x135, 336355},
	{0x136, 337444},
	{0x137, 338532},
	{0x138, 339621},
	{0x139, 340709},
	{0x13a, 341798},
	{0x13b, 342886},
	{0x13c, 343975},
	{0x13d, 345063},
	{0x13e, 346152},
	{0x13f, 347240},
	{0x140, 348329},
	{0x141, 349417},
	{0x142, 350506},
	{0x143, 351594},
	{0x144, 352683},
	{0x145, 353772},
	{0x146, 354860},
	{0x147, 355949},
	{0x148, 357037},
	{0x149, 358126},
	{0x14a, 359214},
	{0x14b, 360303},
	{0x14c, 361391},
	{0x14d, 362480},
	{0x14e, 363568},
	{0x14f, 364657},
	{0x150, 365745},
	{0x151, 366834},
	{0x152, 367922},
	{0x153, 369011},
	{0x154, 370099},
	{0x155, 371188},
	{0x156, 372277},
	{0x157, 373365},
	{0x158, 374454},
	{0x159, 375542},
	{0x15a, 376631},
	{0x15b, 377719},
	{0x15c, 378808},
	{0x15d, 379896},
	{0x15e, 380985},
	{0x15f, 382073},
	{0x160, 383162},
	{0x161, 384250},
	{0x162, 385339},
	{0x163, 386427},
	{0x164, 387516},
	{0x165, 388604},
	{0x166, 389693},
	{0x167, 390782},
	{0x168, 391870},
	{0x169, 392959},
	{0x16a, 394047},
	{0x16b, 395136},
	{0x16c, 396224},
	{0x16d, 397313},
	{0x16e, 398401},
	{0x16f, 399490},
	{0x170, 400578},
	{0x171, 401667},
	{0x172, 402755},
	{0x173, 403844},
	{0x174, 404932},
	{0x175, 406021},
	{0x176, 407109},
	{0x177, 408198},
	{0x178, 409287},
	{0x179, 410375},
	{0x17a, 411464},
	{0x17b, 412552},
	{0x17c, 413641},
	{0x17d, 414729},
	{0x17e, 415818},
	{0x17f, 416906},
	{0x180, 417995},
	{0x181, 419083},
	{0x182, 420172},
	{0x183, 421260},
	{0x184, 422349},
	{0x185, 423437},
	{0x186, 424526},
	{0x187, 425614},
	{0x188, 426703},
	{0x189, 427792},
	{0x18a, 428880},
	{0x18b, 429969},
	{0x18c, 431057},
	{0x18d, 432146},
	{0x18e, 433234},
	{0x18f, 434323},
	{0x190, 435411},
	{0x191, 436500},
	{0x192, 437588},
	{0x193, 438677},
	{0x194, 439765},
	{0x195, 440854},
	{0x196, 441942},
	{0x197, 443031},
	{0x198, 444119},
	{0x199, 445208},
	{0x19a, 446297},
	{0x19b, 447385},
	{0x19c, 448474},
	{0x19d, 449562},
	{0x19e, 450651},
	{0x19f, 451739},
	{0x1a0, 452828},
	{0x1a1, 453916},
	{0x1a2, 455005},
	{0x1a3, 456093},
	{0x1a4, 457182},
	{0x1a5, 458270},
	{0x1a6, 459359},
	{0x1a7, 460447},
	{0x1a8, 461536},
	{0x1a9, 462624},
	{0x1aa, 463713},
	{0x1ab, 464802},
	{0x1ac, 465890},
	{0x1ad, 466979},
	{0x1ae, 468067},
	{0x1af, 469156},
	{0x1b0, 470244},
	{0x1b1, 471333},
	{0x1b2, 472421},
	{0x1b3, 473510},
	{0x1b4, 474598},
	{0x1b5, 475687},
	{0x1b6, 476775},
	{0x1b7, 477864},
	{0x1b8, 478952},
	{0x1b9, 480041},
	{0x1ba, 481129},
	{0x1bb, 482218},
	{0x1bc, 483307},
	{0x1bd, 484395},
	{0x1be, 485484},
	{0x1bf, 486572},
	{0x1c0, 487661},
	{0x1c1, 488749},
	{0x1c2, 489838},
	{0x1c3, 490926},
	{0x1c4, 492015},
	{0x1c5, 493103},
	{0x1c6, 494192},
	{0x1c7, 495280},
	{0x1c8, 496369},
	{0x1c9, 497457},
	{0x1ca, 498546},
	{0x1cb, 499634},
	{0x1cc, 500723},/*200x*/
	{0x1cd, 501812},
	{0x1ce, 502900},
	{0x1cf, 503989},
	{0x1d0, 505077},
	{0x1d1, 506166},
	{0x1d2, 507254},
	{0x1d3, 508343},
	{0x1d4, 509431},
	{0x1d5, 510520},
	{0x1d6, 511608},
	{0x1d7, 512697},
	{0x1d8, 513785},
	{0x1d9, 514874},
	{0x1da, 515962},
	{0x1db, 517051},
	{0x1dc, 518139},
	{0x1dd, 519228},
	{0x1de, 520317},
	{0x1df, 521405},
	{0x1e0, 522494},
	{0x1e1, 523582},
	{0x1e2, 524671},
	{0x1e3, 525759},
	{0x1e4, 526848},
	{0x1e5, 527936},
	{0x1e6, 529025},
	{0x1e7, 530113},
	{0x1e8, 531202},
	{0x1e9, 532290},
	{0x1ea, 533379},
	{0x1eb, 534467},
	{0x1ec, 535556},
	{0x1ed, 536644},
	{0x1ee, 537733},
	{0x1ef, 538822},
	{0x1f0, 539910},
	{0x1f1, 540999},
	{0x1f2, 542087},
	{0x1f3, 543176},
	{0x1f4, 544264},
	{0x1f5, 545353},
	{0x1f6, 546441},
	{0x1f7, 547530},
	{0x1f8, 548618},
	{0x1f9, 549707},
	{0x1fa, 550795},
	{0x1fb, 551884},
	{0x1fc, 552972},
	{0x1fd, 554061},
	{0x1fe, 555149},
	{0x1ff, 556238},
	{0x200, 557327},
	{0x201, 558415},
	{0x202, 559504},
	{0x203, 560592},
	{0x204, 561681},
	{0x205, 562769},
	{0x206, 563858},
	{0x207, 564946},
	{0x208, 566035},
	{0x209, 567123},
	{0x20a, 568212},
	{0x20b, 569300},
	{0x20c, 570389},
	{0x20d, 571477},
	{0x20e, 572566},
	{0x20f, 573654},
	{0x210, 574743},
	{0x211, 575832},
	{0x212, 576920},
	{0x213, 578009},
	{0x214, 579097},
	{0x215, 580186},
	{0x216, 581274},
	{0x217, 582363},
	{0x218, 583451},
	{0x219, 584540},
	{0x21a, 585628},
	{0x21b, 586717},
	{0x21c, 587805},
	{0x21d, 588894},
	{0x21e, 589982},
	{0x21f, 591071},
	{0x220, 592159},
	{0x221, 593248},
	{0x222, 594337},
	{0x223, 595425},
	{0x224, 596514},
	{0x225, 597602},
	{0x226, 598691},
	{0x227, 599779},
	{0x228, 600868},
	{0x229, 601956},
	{0x22a, 603045},
	{0x22b, 604133},
	{0x22c, 605222},
	{0x22d, 606310},
	{0x22e, 607399},
	{0x22f, 608487},
	{0x230, 609576},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#if 1
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain <= sensor_again_lut[0].gain) {
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
#else
	uint16_t again_reg;

	/* again_reg value = gain[dB]*10 */
	again_reg = (isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	printk("isp_gain in = %d, again_reg = %d\n",isp_gain,again_reg);
	if (again_reg > (AGAIN_MAX_DB + DGAIN_MAX_DB)*10)
		again_reg = (AGAIN_MAX_DB + DGAIN_MAX_DB)*10;
	/* p_ctx->again_reg=again_reg; */
	*sensor_again=again_reg;
	isp_gain= (((int32_t)again_reg)<<LOG2_GAIN_SHIFT)/200;
	printk("isp_gain out = %d\n",isp_gain);
#endif
	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	*sensor_dgain = 0;

	return 0;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0xf000,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_SONY_MODE,
		.clk = 445,
		.lans = 2,
	},
	.max_again = 500723,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1125-3,
	.integration_time_limit = 1125-3,
	.total_width = 2200,
	.total_height = 1125,
	.max_integration_time = 1125-3,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	{0x3000, 0x01},
	{0x3001, 0x00},
	{0x3002, 0x01},
	{0x3005, 0x01},
	{0x3007, 0x00},
	{0x3009, 0x12},
	{0x300A, 0xF0},
	{0x300B, 0x00},
	{0x3012, 0x2C},
	{0x3013, 0x01},
	{0x3014, 0x3c},
	{0x3015, 0x00},
	{0x3016, 0x08},
	{0x3018, 0x65},
	{0x3019, 0x04},
	{0x301A, 0x00},
	{0x301B, 0x30},
	{0x301C, 0x11},
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3036, 0x10},
	{0x303A, 0xD1},
	{0x303B, 0x03},
	{0x3044, 0x01},
	{0x3046, 0x00},
	{0x3047, 0x08},
	{0x3049, 0x00},
	{0x3054, 0x66},
	{0x305C, 0x18},
	{0x305D, 0x00},
	{0x305E, 0x20},
	{0x305F, 0x00},
	{0x310B, 0x07},
	{0x3110, 0x12},
	{0x31ED, 0x38},
	{0x3338, 0xD4},
	{0x3339, 0x40},
	{0x333A, 0x10},
	{0x333B, 0x00},
	{0x333C, 0xD4},
	{0x333D, 0x40},
	{0x333E, 0x10},
	{0x333F, 0x00},
	{0x3344, 0x00},
	{0x3346, 0x01},
	{0x3353, 0x0E},
	{0x3357, 0x49},
	{0x3358, 0x04},
	{0x336B, 0x3F},
	{0x336C, 0x1F},
	{0x337D, 0x0C},
	{0x337E, 0x0C},
	{0x337F, 0x01},
	{0x3380, 0x20},
	{0x3381, 0x25},
	{0x3382, 0x67},
	{0x3383, 0x24},
	{0x3384, 0x3F},
	{0x3385, 0x27},
	{0x3386, 0x1F},
	{0x3387, 0x17},
	{0x3388, 0x77},
	{0x3389, 0x27},
	{0x338D, 0xB4},
	{0x338E, 0x01},
	//{SENSOR_REG_DELAY, 0x18},
	{0x3002, 0x00},
	{0x3000, 0x00},

	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1952*1113 */
	{
		.width = 1952,
		.height = 1113,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SRGGB12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_mipi[] = {
	{0x3000, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x3000, 0x01},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
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

static int sensor_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x3012, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3013, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
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
	shs = vmax - value - 1;
	ret = sensor_write(sd, 0x3020, (unsigned char)(shs & 0xff));
	ret += sensor_write(sd, 0x3021, (unsigned char)((shs >> 8) & 0xff));
	ret += sensor_write(sd, 0x3022, (unsigned char)((shs >> 16) & 0x01));
	if (0 != ret) {
		printk("err: sensor_write err\n");
		return ret;
	}

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x3014, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3015, (unsigned char)((value >> 8) & 0x03));
	if (ret < 0)
		return ret;

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

static int sensor_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret = 0;
	if (!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = sensor_write_array(sd, wsize->regs);
	printk("sensor init \n");
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		pr_debug("%s stream on\n", SENSOR_NAME);

	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int pclk = 0;
	unsigned short hmax = 0;
	unsigned short vmax = 0;
	unsigned short cur_int = 0;
	unsigned short shs = 0;
	unsigned char value = 0;
	unsigned int newformat = 0; //the format is 24.8

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	pclk = SENSOR_SUPPORT_PCLK;

#if 0
	/*method 1 change hts*/
	ret = sensor_read(sd, 0x3018, &value);
	vmax = value;
	ret += sensor_read(sd, 0x3019, &value);
	vmax |= value << 8;
	ret += sensor_read(sd, 0x301a, &value);
	vmax |= (value|0x3) << 16;

	hmax = ((pclk << 4) / (vmax * (newformat >> 4))) << 1;
	ret += sensor_write(sd, 0x301c, hmax & 0xff);
	ret += sensor_write(sd, 0x301d, (hmax >> 8) & 0xff);
	if (0 != ret) {
		printk("err: sensor_write err\n");
		return ret;
	}
	sensor->video.attr->total_width = hmax >> 1;
#endif

	/*method 2 change vts*/
	ret = sensor_read(sd, 0x301b, &value);
	hmax = value;
	ret += sensor_read(sd, 0x301c, &value);
	hmax = (((value & 0x3f) << 8) | hmax) >> 1;

	vmax = ((pclk << 4) / (hmax * (newformat >> 4)));
	ret += sensor_write(sd, 0x3018, vmax & 0xff);
	ret += sensor_write(sd, 0x3019, (vmax >> 8) & 0xff);
	ret += sensor_write(sd, 0x301a, (vmax >> 16) & 0x01);

	/*record current integration time*/
	ret = sensor_read(sd, 0x3020, &value);
	shs = value;
	ret += sensor_read(sd, 0x3021, &value);
	shs = (value << 8) | shs;
	ret += sensor_read(sd, 0x3022, &value);
	shs = ((value & 0x01) << 16) | shs;
	cur_int = sensor->video.attr->total_height - shs -1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vmax - 3;
	sensor->video.attr->integration_time_limit = vmax - 3;
	sensor->video.attr->total_height = vmax;
	sensor->video.attr->max_integration_time = vmax - 3;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	ret = sensor_set_integration_time(sd,cur_int);
	if (ret < 0)
		return -1;

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	wsize = &sensor_win_sizes[0];
	if (wsize) {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	ret += sensor_read(sd, 0x3007, &val);
	if (enable) {
		val = val | 0x01;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SRGGB12_1X12;
	} else {
		val = val & 0xfe;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SRGGB12_1X12;
	}
	sensor->video.mbus_change = 0;
	ret += sensor_write(sd, 0x3007, val);

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			printk("gpio requrest fail %d\n",reset_gpio);
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
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an %s chip.\n",
		       client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	printk("%s chip found @ 0x%02x (%s)\n",
	       SENSOR_NAME, client->addr, client->adapter->name);
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
	if (IS_ERR_OR_NULL(sd)) {
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, *(int*)arg);
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
			ret = sensor_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return 0;
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
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret;
	unsigned long rate = 0;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 37125) != 0) {
		struct clk *epll;
		epll = clk_get(NULL,"epll");
		if (IS_ERR(epll)) {
			pr_err("get epll failed\n");
		} else {
			rate = clk_get_rate(epll);
			if (((rate / 1000) % 37125) != 0) {
				clk_set_rate(epll,891000000);
			}
			ret = clk_set_parent(sensor->mclk, epll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}
	private_clk_set_rate(sensor->mclk, 37125000);
	private_clk_enable(sensor->mclk);

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.mbus_change = 0;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
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
	int ret = 0;
	ret = private_driver_get_interface();
	if (ret) {
		printk("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}

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
