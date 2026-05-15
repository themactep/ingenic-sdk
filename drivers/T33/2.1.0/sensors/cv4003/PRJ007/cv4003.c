/*
 * cv4003.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps    dvdd
 *  0           2560*1440       30        mipi_2lane    linear  10        30       null
 *
 * @I2C addr:0x35,0x36
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
#define SENSOR_VERSION  "H20250326a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x5a)
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

struct tx_isp_sensor_attribute cv4003_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut cv4003_again_lut[] = {
	{0x00, 0},//1.0
	{0x01, 370},//1.004
	{0x02, 741},//1.008
	{0x03, 1114},//1.012
	{0x04, 1488},//1.016
	{0x05, 1864},//1.02
	{0x06, 2242},//1.024
	{0x07, 2621},//1.028
	{0x08, 3001},//1.032
	{0x09, 3383},//1.036
	{0x0a, 3767},//1.041
	{0x0b, 4152},//1.045
	{0x0c, 4539},//1.049
	{0x0d, 4927},//1.053
	{0x0e, 5317},//1.058
	{0x0f, 5708},//1.062
	{0x10, 6102},//1.067
	{0x11, 6496},//1.071
	{0x12, 6893},//1.076
	{0x13, 7291},//1.08
	{0x14, 7691},//1.085
	{0x15, 8092},//1.089
	{0x16, 8495},//1.094
	{0x17, 8900},//1.099
	{0x18, 9307},//1.103
	{0x19, 9715},//1.108
	{0x1a, 10125},//1.113
	{0x1b, 10537},//1.118
	{0x1c, 10951},//1.123
	{0x1d, 11367},//1.128
	{0x1e, 11784},//1.133
	{0x1f, 12204},//1.138
	{0x20, 12625},//1.143
	{0x21, 13048},//1.148
	{0x22, 13473},//1.153
	{0x23, 13900},//1.158
	{0x24, 14328},//1.164
	{0x25, 14759},//1.169
	{0x26, 15192},//1.174
	{0x27, 15626},//1.18
	{0x28, 16063},//1.185
	{0x29, 16502},//1.191
	{0x2a, 16943},//1.196
	{0x2b, 17386},//1.202
	{0x2c, 17831},//1.208
	{0x2d, 18278},//1.213
	{0x2e, 18727},//1.219
	{0x2f, 19178},//1.225
	{0x30, 19631},//1.231
	{0x31, 20087},//1.237
	{0x32, 20545},//1.243
	{0x33, 21005},//1.249
	{0x34, 21467},//1.255
	{0x35, 21932},//1.261
	{0x36, 22399},//1.267
	{0x37, 22868},//1.274
	{0x38, 23340},//1.28
	{0x39, 23814},//1.286
	{0x3a, 24290},//1.293
	{0x3b, 24769},//1.299
	{0x3c, 25250},//1.306
	{0x3d, 25734},//1.313
	{0x3e, 26220},//1.32
	{0x3f, 26708},//1.326
	{0x40, 27199},//1.333
	{0x41, 27693},//1.34
	{0x42, 28189},//1.347
	{0x43, 28688},//1.354
	{0x44, 29190},//1.362
	{0x45, 29694},//1.369
	{0x46, 30201},//1.376
	{0x47, 30711},//1.384
	{0x48, 31223},//1.391
	{0x49, 31739},//1.399
	{0x4a, 32257},//1.407
	{0x4b, 32778},//1.414
	{0x4c, 33301},//1.422
	{0x4d, 33828},//1.43
	{0x4e, 34358},//1.438
	{0x4f, 34891},//1.446
	{0x50, 35426},//1.455
	{0x51, 35965},//1.463
	{0x52, 36507},//1.471
	{0x53, 37052},//1.48
	{0x54, 37600},//1.488
	{0x55, 38151},//1.497
	{0x56, 38706},//1.506
	{0x57, 39263},//1.515
	{0x58, 39825},//1.524
	{0x59, 40389},//1.533
	{0x5a, 40957},//1.542
	{0x5b, 41528},//1.552
	{0x5c, 42103},//1.561
	{0x5d, 42681},//1.571
	{0x5e, 43263},//1.58
	{0x5f, 43849},//1.59
	{0x60, 44438},//1.6
	{0x61, 45030},//1.61
	{0x62, 45627},//1.62
	{0x63, 46227},//1.631
	{0x64, 46831},//1.641
	{0x65, 47439},//1.652
	{0x66, 48051},//1.662
	{0x67, 48667},//1.673
	{0x68, 49287},//1.684
	{0x69, 49911},//1.695
	{0x6a, 50540},//1.707
	{0x6b, 51172},//1.718
	{0x6c, 51809},//1.73
	{0x6d, 52450},//1.741
	{0x6e, 53095},//1.753
	{0x6f, 53745},//1.766
	{0x70, 54399},//1.778
	{0x71, 55058},//1.79
	{0x72, 55722},//1.803
	{0x73, 56390},//1.816
	{0x74, 57063},//1.829
	{0x75, 57741},//1.842
	{0x76, 58423},//1.855
	{0x77, 59111},//1.869
	{0x78, 59804},//1.882
	{0x79, 60501},//1.896
	{0x7a, 61204},//1.91
	{0x7b, 61913},//1.925
	{0x7c, 62626},//1.939
	{0x7d, 63345},//1.954
	{0x7e, 64070},//1.969
	{0x7f, 64800},//1.984
	{0x80, 65536},//2.0
	{0x81, 66277},//2.016
	{0x82, 67024},//2.032
	{0x83, 67778},//2.048
	{0x84, 68537},//2.065
	{0x85, 69303},//2.081
	{0x86, 70075},//2.098
	{0x87, 70853},//2.116
	{0x88, 71638},//2.133
	{0x89, 72429},//2.151
	{0x8a, 73227},//2.169
	{0x8b, 74031},//2.188
	{0x8c, 74843},//2.207
	{0x8d, 75661},//2.226
	{0x8e, 76487},//2.246
	{0x8f, 77320},//2.265
	{0x90, 78161},//2.286
	{0x91, 79009},//2.306
	{0x92, 79864},//2.327
	{0x93, 80728},//2.349
	{0x94, 81599},//2.37
	{0x95, 82479},//2.393
	{0x96, 83367},//2.415
	{0x97, 84263},//2.438
	{0x98, 85167},//2.462
	{0x99, 86081},//2.485
	{0x9a, 87003},//2.51
	{0x9b, 87935},//2.535
	{0x9c, 88876},//2.56
	{0x9d, 89826},//2.586
	{0x9e, 90786},//2.612
	{0x9f, 91756},//2.639
	{0xa0, 92735},//2.667
	{0xa1, 93725},//2.695
	{0xa2, 94726},//2.723
	{0xa3, 95737},//2.753
	{0xa4, 96759},//2.783
	{0xa5, 97793},//2.813
	{0xa6, 98837},//2.844
	{0xa7, 99894},//2.876
	{0xa8, 100962},//2.909
	{0xa9, 102043},//2.943
	{0xaa, 103136},//2.977
	{0xab, 104242},//3.012
	{0xac, 105361},//3.048
	{0xad, 106493},//3.084
	{0xae, 107639},//3.122
	{0xaf, 108799},//3.16
	{0xb0, 109974},//3.2
	{0xb1, 111163},//3.241
	{0xb2, 112367},//3.282
	{0xb3, 113587},//3.325
	{0xb4, 114823},//3.368
	{0xb5, 116076},//3.413
	{0xb6, 117345},//3.459
	{0xb7, 118631},//3.507
	{0xb8, 119935},//3.556
	{0xb9, 121258},//3.606
	{0xba, 122599},//3.657
	{0xbb, 123959},//3.71
	{0xbc, 125340},//3.765
	{0xbd, 126740},//3.821
	{0xbe, 128162},//3.879
	{0xbf, 129606},//3.938
	{0xc0, 131072},//4.0
	{0xc1, 132560},//4.063
	{0xc2, 134073},//4.129
	{0xc3, 135611},//4.197
	{0xc4, 137174},//4.267
	{0xc5, 138763},//4.339
	{0xc6, 140379},//4.414
	{0xc7, 142023},//4.491
	{0xc8, 143697},//4.571
	{0xc9, 145400},//4.655
	{0xca, 147135},//4.741
	{0xcb, 148903},//4.83
	{0xcc, 150703},//4.923
	{0xcd, 152539},//5.02
	{0xce, 154412},//5.12
	{0xcf, 156322},//5.224
	{0xd0, 158271},//5.333
	{0xd1, 160262},//5.447
	{0xd2, 162295},//5.565
	{0xd3, 164373},//5.689
	{0xd4, 166498},//5.818
	{0xd5, 168672},//5.953
	{0xd6, 170897},//6.095
	{0xd7, 173175},//6.244
	{0xd8, 175510},//6.4
	{0xd9, 177903},//6.564
	{0xda, 180359},//6.737
	{0xdb, 182881},//6.919
	{0xdc, 185471},//7.111
	{0xdd, 188135},//7.314
	{0xde, 190876},//7.529
	{0xdf, 193698},//7.758
	{0xe0, 196608},//8.0
	{0xe1, 199609},//8.258
	{0xe2, 202710},//8.533
	{0xe3, 205915},//8.828
	{0xe4, 209233},//9.143
	{0xe5, 212671},//9.481
	{0xe6, 216239},//9.846
	{0xe7, 219948},//10.24
	{0xe8, 223807},//10.667
	{0xe9, 227831},//11.13
	{0xea, 232034},//11.636
	{0xeb, 236433},//12.19
	{0xec, 241046},//12.8
	{0xed, 245895},//13.474
	{0xee, 251007},//14.222
	{0xef, 256412},//15.059
	{0xf0, 262144},//16.0
	{0xf1, 268246},//17.067
	{0xf2, 274769},//18.286
	{0xf3, 281775},//19.692
	{0xf4, 289343},//21.333
	{0xf5, 297570},//23.273
	{0xf6, 306582},//25.6
	{0xf7, 316543},//28.444
	{0xf8, 327680},//32.0
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int cv4003_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = cv4003_again_lut;
	while (lut->gain <= cv4003_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == cv4003_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int cv4003_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = cv4003_again_lut;
	while(lut->gain <= cv4003_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == cv4003_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int cv4003_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int cv4003_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int cv4003_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus cv4003_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1317,
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

struct tx_isp_dvp_bus cv4003_dvp = {
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

struct tx_isp_sensor_attribute cv4003_attr = {
	.name = "cv4003",
	.chip_id = 0x8001,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x35,
	.sensor_ctrl.alloc_again = cv4003_alloc_again,
	.sensor_ctrl.alloc_dgain = cv4003_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = cv4003_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list cv4003_init_regs_2560_1440_30fps_mipi[] = {
	{0x30C0, 0x02},      // 默认拉低vsync引脚
	{0x300D, 0x01},
	{0x300E, 0x00},
	{0x30A8, 0x01},
	{0x3204, 0x40},
	{0x3478, 0x00},
	{0x385A, 0x03},
	{0x359f, 0x13},
	{0x3048, 0x08},
	{0x3134, 0x01},
	{0x326c, 0xdc},
	{0x326e, 0x24},
	{0x3154, 0x11},
	{0x31BC, 0x40},
	{0x31D5, 0x03},
	{0x31D6, 0x04},
	{0x3164, 0x09},
	{0x351F, 0x32},
	{0x3527, 0x05},
	{0x38AD, 0x10},
	{0x3638, 0x03},
	{0x3639, 0x00},
	{0x301c, 0xe0},//vts = 0x15e0 = 5600
	{0x301d, 0x15},//
	{0x3020, 0x89},//vts = 0x189 = 393
	{0x3021, 0x01},//
	{0x3109, 0x01},
	{0x3014, 0x04},
	{0x303C, 0x56},
	{0x303D, 0x00},
	{0x303E, 0xb0},
	{0x303F, 0x05},
	{0x3030, 0x01},
	{0x3038, 0xa4},
	{0x3039, 0x00},
	{0x303A, 0x00},
	{0x303B, 0x0a},
	{0x3034, 0x08},
	{0x3035, 0x00},
	{0x3036, 0xa0},
	{0x3037, 0x05},
	{0x3403, 0x00}, //帧中standby允许出现坏帧(写0x3000,0x01立即停流)
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the cv4003_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting cv4003_win_sizes[] = {
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = cv4003_init_regs_2560_1440_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &cv4003_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list cv4003_stream_on_mipi[] = {
	{0x3000, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list cv4003_stream_off_mipi[] = {
	{0x3000, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int cv4003_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int cv4003_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int cv4003_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4003_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int cv4003_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4003_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int cv4003_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int cv4003_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int cv4003_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4003_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int cv4003_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4003_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int cv4003_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int cv4003_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int cv4003_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &cv4003_win_sizes[0];
		cv4003_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		cv4003_attr.again.value = 65536;
		cv4003_attr.again.max = 327680;
		cv4003_attr.again.min = 0;
		cv4003_attr.again.apply_delay = 2;

		cv4003_attr.integration_time.value = 0xb60;
		cv4003_attr.integration_time.max = 5600 - 8;
		cv4003_attr.integration_time.min = 4;
		cv4003_attr.integration_time.apply_delay = 2;

		cv4003_attr.total_width = 393;
		cv4003_attr.total_height = 5600;

		cv4003_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		cv4003_attr.hcg.base_gain = ;
		cv4003_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		cv4003_attr.again_short.value = ;
		cv4003_attr.again_short.max = ;
		cv4003_attr.again_short.min = ;
		cv4003_attr.again_short.apply_delay = ;

		cv4003_attr.integration_time_short.value = ;
		cv4003_attr.integration_time_short.max = ;
		cv4003_attr.integration_time_short.min = ;
		cv4003_attr.integration_time_short.apply_delay = ;

		cv4003_attr.wdr_cache = wdr_line * cv4003_attr.total_width;

#ifdef SENSOR_HCG
		cv4003_attr.hcg_short.base_gain = ;
		cv4003_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(cv4003_attr.mipi)), (void *)(&cv4003_mipi_linear), sizeof(cv4003_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int cv4003_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	cv4003_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		cv4003_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		cv4003_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		cv4003_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		cv4003_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		cv4003_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = cv4003_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	cv4003_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int cv4003_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = cv4003_read(sd, 0x3003, &v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = cv4003_read(sd, 0x3002, &v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int cv4003_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	cv4003_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "cv4003_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(info->rst_gpio, 0);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "cv4003_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = cv4003_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an cv4003 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", cv4003_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "cv4003", sizeof("cv4003"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int cv4003_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int cv4003_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = cv4003_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int cv4003_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = cv4003_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int cv4003_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	cv4003_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int cv4003_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = cv4003_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = cv4003_write_array(sd, cv4003_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("cv4003 stream on\n");
		}

	} else {
		ret = cv4003_write_array(sd, cv4003_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("cv4003 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int cv4003_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	int shr0 = 0;

	/* shr0 = cv4003_attr.integration_time.max - it; */
	shr0 = cv4003_attr.total_height - it;
	shr0 = ((shr0 >> 1) << 1);
	ret += cv4003_write(sd, 0x3049, (unsigned char)((shr0 >> 8) & 0xff));
	ret += cv4003_write(sd, 0x3048, (unsigned char)(shr0 & 0xff));

	ret += cv4003_write(sd, 0x3118, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int cv4003_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int shr0 = 0;

	shr0 = cv4003_attr.integration_time.max - value;
	shr0 = ((shr0 >> 1) << 1);
	ret += cv4003_write(sd, 0x3049, (unsigned char)((shr0 >> 8) & 0xff));
	ret += cv4003_write(sd, 0x3048, (unsigned char)(shr0 & 0xff));

	return ret;
}

static int cv4003_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += cv4003_write(sd, 0x3118, (unsigned char)(value & 0xff));

	return ret;
}
#endif /* SENSOR_EXPO */

static int cv4003_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv4003_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv4003_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = cv4003_attr_set(sd, wsize);
	}

	return ret;
}

static int cv4003_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 393 * 5600 * 30;  /**< HTS * VTS * FPS */
		max_fps = 30;
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
	ret += cv4003_read(sd, 0x3021, &val);
	hts = val;
	val = 0;
	ret += cv4003_read(sd, 0x3020, &val);
	hts = (hts << 8) | val;
	if (0 != ret) {
		ISP_ERROR("err: cv4003 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	cv4003_write(sd, 0x301c, (unsigned char) (vts & 0xff));
	cv4003_write(sd, 0x301d, (unsigned char) (vts >> 8));
	cv4003_write(sd, 0x301e, (unsigned char) ((vts >> 16) & 0x0f));

	if (0 != ret) {
		ISP_ERROR("err: cv4003_write err\n");
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
		sensor->video.attr->integration_time.max = vts - 8;
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

static int cv4003_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = cv4003_read(sd, 0x3028, &val);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val &= 0xFC;
		sensor->video.mbus.code = TISP_VI_FMT_SRGGB10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val = ((val & 0xFC) | 0x01);
		sensor->video.mbus.code = TISP_VI_FMT_SGRBG10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val = ((val & 0xFC) | 0x02);
		sensor->video.mbus.code = TISP_VI_FMT_SGBRG10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val |= 0x03;
		sensor->video.mbus.code = TISP_VI_FMT_SBGGR10_1X10;
		break;
	}
	ret += cv4003_write(sd, 0x3028, val);

	sensor->video.hvflip_mode = par->hvflip;
	sensor->video.mbus_change = 1;
	cv4003_attr_set(sd, wsize);


	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int cv4003_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int cv4003_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int cv4003_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int cv4003_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = cv4003_write_array(sd, cv4003_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		cv4003_setting_select(sd, 1);
		cv4003_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		cv4003_setting_select(sd, 0);
		cv4003_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int cv4003_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = cv4003_write_array(sd, wsize->regs);
	ret = cv4003_write_array(sd, cv4003_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int cv4003_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = cv4003_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = cv4003_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = cv4003_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = cv4003_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = cv4003_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = cv4003_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = cv4003_write_array(sd, cv4003_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = cv4003_write_array(sd, cv4003_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = cv4003_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = cv4003_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = cv4003_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = cv4003_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = cv4003_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = cv4003_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = cv4003_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops cv4003_core_ops = {
	.g_chip_ident = cv4003_g_chip_ident,
	.reset = cv4003_reset,
	.init = cv4003_init,
	.g_register = cv4003_g_register,
	.s_register = cv4003_s_register,
};

static struct tx_isp_subdev_video_ops cv4003_video_ops = {
	.s_stream = cv4003_s_stream,
};

static struct tx_isp_subdev_sensor_ops cv4003_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = cv4003_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops cv4003_ops = {
	.core = &cv4003_core_ops,
	.video = &cv4003_video_ops,
	.sensor = &cv4003_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "cv4003",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int cv4003_probe(struct i2c_client *client,
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
	sensor->video.attr = &cv4003_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &cv4003_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->cv4003\n");

	return 0;
}

static int cv4003_remove(struct i2c_client *client)
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

static const struct i2c_device_id cv4003_id[] = {
	{"cv4003", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, cv4003_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver cv4003_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "cv4003",
	},
	.probe          = cv4003_probe,
	.remove         = cv4003_remove,
	.id_table       = cv4003_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_cv4003(void) {
	return private_i2c_add_driver(&cv4003_driver);
}

static __exit void exit_cv4003(void) {
	private_i2c_del_driver(&cv4003_driver);
}

module_init(init_cv4003);
module_exit(exit_cv4003);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_cv4003(void) {
	return private_i2c_add_driver(&cv4003_driver);
}

static void exit_first_cv4003(void) {
	private_i2c_del_driver(&cv4003_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "cv4003",
	.i2c_addr = 0x35,
	.width = 2560,
	.height = 1440,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_cv4003,
	.exit_sensor = exit_first_cv4003
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for cv4003 sensor");
MODULE_LICENSE("GPL");
