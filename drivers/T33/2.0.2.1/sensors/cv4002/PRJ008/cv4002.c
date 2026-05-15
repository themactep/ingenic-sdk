/*
 * cv4002.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution       fps       interface     mode    raw     max_fps    dvdd
 *  0           2560*1440        30        mipi_2lane    linear  10        30       null
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
#define SENSOR_VERSION  "H20251101a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x40)
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

struct tx_isp_sensor_attribute cv4002_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	uint16_t value;
	unsigned int gain;
};

struct again_lut cv4002_again_lut[] = {
	{0x0, 0},
	{0x1, 377},
	{0x2, 753},
	{0x3, 1127},
	{0x4, 1500},
	{0x5, 1872},
	{0x6, 2242},
	{0x7, 2610},
	{0x8, 2978},
	{0x9, 3343},
	{0xa, 3799},
	{0xb, 4161},
	{0xc, 4522},
	{0xd, 4882},
	{0xe, 5330},
	{0xf, 5687},
	{0x10, 6131},
	{0x11, 6485},
	{0x12, 6925},
	{0x13, 7276},
	{0x14, 7713},
	{0x15, 8061},
	{0x16, 8494},
	{0x17, 8925},
	{0x18, 9268},
	{0x19, 9696},
	{0x1a, 10122},
	{0x1b, 10545},
	{0x1c, 10967},
	{0x1d, 11387},
	{0x1e, 11805},
	{0x1f, 12222},
	{0x20, 12636},
	{0x21, 13049},
	{0x22, 13460},
	{0x23, 13869},
	{0x24, 14358},
	{0x25, 14763},
	{0x26, 15166},
	{0x27, 15648},
	{0x28, 16048},
	{0x29, 16526},
	{0x2a, 16922},
	{0x2b, 17395},
	{0x2c, 17866},
	{0x2d, 18256},
	{0x2e, 18723},
	{0x2f, 19187},
	{0x30, 19649},
	{0x31, 20109},
	{0x32, 20566},
	{0x33, 21021},
	{0x34, 21474},
	{0x35, 21925},
	{0x36, 22374},
	{0x37, 22895},
	{0x38, 23339},
	{0x39, 23782},
	{0x3a, 24295},
	{0x3b, 24733},
	{0x3c, 25241},
	{0x3d, 25746},
	{0x3e, 26249},
	{0x3f, 26678},
	{0x40, 27175},
	{0x41, 27671},
	{0x42, 28163},
	{0x43, 28653},
	{0x44, 29210},
	{0x45, 29695},
	{0x46, 30177},
	{0x47, 30725},
	{0x48, 31202},
	{0x49, 31744},
	{0x4a, 32284},
	{0x4b, 32753},
	{0x4c, 33286},
	{0x4d, 33817},
	{0x4e, 34344},
	{0x4f, 34869},
	{0x50, 35455},
	{0x51, 35974},
	{0x52, 36489},
	{0x53, 37066},
	{0x54, 37576},
	{0x55, 38146},
	{0x56, 38712},
	{0x57, 39276},
	{0x58, 39836},
	{0x59, 40393},
	{0x5a, 40946},
	{0x5b, 41557},
	{0x5c, 42104},
	{0x5d, 42708},
	{0x5e, 43248},
	{0x5f, 43844},
	{0x60, 44437},
	{0x61, 45026},
	{0x62, 45611},
	{0x63, 46251},
	{0x64, 46829},
	{0x65, 47461},
	{0x66, 48031},
	{0x67, 48655},
	{0x68, 49275},
	{0x69, 49890},
	{0x6a, 50557},
	{0x6b, 51165},
	{0x6c, 51823},
	{0x6d, 52422},
	{0x6e, 53071},
	{0x6f, 53770},
	{0x70, 54410},
	{0x71, 55046},
	{0x72, 55730},
	{0x73, 56410},
	{0x74, 57084},
	{0x75, 57754},
	{0x76, 58419},
	{0x77, 59130},
	{0x78, 59785},
	{0x79, 60486},
	{0x7a, 61181},
	{0x7b, 61921},
	{0x7c, 62606},
	{0x7d, 63335},
	{0x7e, 64058},
	{0x7f, 64775},
	{0x80, 65535},
	{0x81, 66288},
	{0x82, 67035},
	{0x83, 67777},
	{0x84, 68558},
	{0x85, 69288},
	{0x86, 70057},
	{0x87, 70865},
	{0x88, 71622},
	{0x89, 72416},
	{0x8a, 73204},
	{0x8b, 74029},
	{0x8c, 74846},
	{0x8d, 75657},
	{0x8e, 76502},
	{0x8f, 77299},
	{0x90, 78171},
	{0x91, 78995},
	{0x92, 79852},
	{0x93, 80742},
	{0x94, 81583},
	{0x95, 82496},
	{0x96, 83362},
	{0x97, 84258},
	{0x98, 85184},
	{0x99, 86063},
	{0x9a, 87009},
	{0x9b, 87947},
	{0x9c, 88874},
	{0x9d, 89830},
	{0x9e, 90776},
	{0x9f, 91748},
	{0xa0, 92746},
	{0xa1, 93733},
	{0xa2, 94710},
	{0xa3, 95746},
	{0xa4, 96771},
	{0xa5, 97785},
	{0xa6, 98821},
	{0xa7, 99879},
	{0xa8, 100958},
	{0xa9, 102056},
	{0xaa, 103142},
	{0xab, 104247},
	{0xac, 105371},
	{0xad, 106481},
	{0xae, 107639},
	{0xaf, 108783},
	{0xb0, 109972},
	{0xb1, 111176},
	{0xb2, 112364},
	{0xb3, 113595},
	{0xb4, 114810},
	{0xb5, 116065},
	{0xb6, 117330},
	{0xb7, 118633},
	{0xb8, 119945},
	{0xb9, 121265},
	{0xba, 122593},
	{0xbb, 123954},
	{0xbc, 125345},
	{0xbd, 126741},
	{0xbe, 128165},
	{0xbf, 129593},
	{0xc0, 131070},
	{0xc1, 132547},
	{0xc2, 134071},
	{0xc3, 135615},
	{0xc4, 137179},
	{0xc5, 138761},
	{0xc6, 140381},
	{0xc7, 142016},
	{0xc8, 143686},
	{0xc9, 145407},
	{0xca, 147138},
	{0xcb, 148897},
	{0xcc, 150700},
	{0xcd, 152544},
	{0xce, 154409},
	{0xcf, 156311},
	{0xd0, 158263},
	{0xd1, 160263},
	{0xd2, 162289},
	{0xd3, 164373},
	{0xd4, 166493},
	{0xd5, 168661},
	{0xd6, 170890},
	{0xd7, 173174},
	{0xd8, 175507},
	{0xd9, 177899},
	{0xda, 180359},
	{0xdb, 182879},
	{0xdc, 185467},
	{0xdd, 188128},
	{0xde, 190867},
	{0xdf, 193700},
	{0xe0, 196605},
	{0xe1, 199606},
	{0xe2, 202703},
	{0xe3, 205916},
	{0xe4, 209231},
	{0xe5, 212663},
	{0xe6, 216235},
	{0xe7, 219944},
	{0xe8, 223807},
	{0xe9, 227824},
	{0xea, 232028},
	{0xeb, 236425},
	{0xec, 241042},
	{0xed, 245894},
	{0xee, 251002},
	{0xef, 256409},
	{0xf0, 262140},
	{0xf1, 268243},
	{0xf2, 274766},
	{0xf3, 281770},
	{0xf4, 289338},
	{0xf5, 297567},
	{0xf6, 306577},
	{0xf7, 316537},
	{0xf8, 327675},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int cv4002_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = cv4002_again_lut;
	while (lut->gain <= cv4002_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == cv4002_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int cv4002_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = cv4002_again_lut;
	while(lut->gain <= cv4002_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == cv4002_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int cv4002_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int cv4002_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int cv4002_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus cv4002_mipi_15fps_linear = {
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

struct tx_isp_dvp_bus cv4002_dvp = {
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

struct tx_isp_sensor_attribute cv4002_attr = {
	.name = "cv4002",
	.chip_id = 0x2005,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x35,
	.sensor_ctrl.alloc_again = cv4002_alloc_again,
	.sensor_ctrl.alloc_dgain = cv4002_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = cv4002_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list cv4002_init_regs_2560_1440_30fps_mipi[] = {
	{0x3020,0x80},
	{0x3021,0x0C},
	{0x3022,0x00},
	{0x3024,0xCC},
	{0x3025,0x05},
	{0x3029,0x00},
	{0x302A,0x00},
	{0x3300,0x00},
	{0x3401,0x01},
	{0x3418,0x97},
	{0x3419,0x00},
	{0x341A,0x4F},
	{0x341B,0x00},
	{0x341C,0x4F},
	{0x341D,0x00},
	{0x341E,0x4F},
	{0x341F,0x01},
	{0x3420,0x4F},
	{0x3421,0x00},
	{0x3422,0x97},
	{0x3423,0x00},
	{0x3424,0x4F},
	{0x3425,0x00},
	{0x3426,0x7F},
	{0x3427,0x00},
	{0x3428,0x3F},
	{0x3429,0x00},
	{0x3440,0x01},
	{0x3442,0x00},
	{0x3806,0x02},
	{0x3908,0x5F},
	{0x3909,0x00},
	{0x3486,0x2B},
	{0x3031,0x00},
	{0x3118,0x01},
	{0x3119,0x06},
	{0x301c,0x00},
	{0x3030,0x01},
	{0x3038,0x08},
	{0x3039,0x00},
	{0x303A,0x00},
	{0x303B,0x0A},
	{0x3034,0x08},
	{0x3035,0x00},
	{0x3036,0xA0},
	{0x3037,0x05},
	{0x35B3,0x11},
	{0x3330,0x00},
	{0x3148,0x64},
	{0x3670,0x00},
	{0x35b3,0x10},
	{0x320e,0x03},
	{0x35ab,0x08},
	{0x3804,0x15},
	{0x397a,0x1d},
	{0x3aa5,0x14},
	{0x3aa4,0x00},
	{0x3a98,0xb4},
	{0x3a99,0x0d},
	{0x3679,0x02},
	{0x3120,0x03},
	{0x3121,0x01},
	{0x3109,0x04},
	{0x313A,0x04},
	{0x3124,0x6C},
	{0x3125,0x0B},
	{0x3126,0x10},
	{0x3127,0x00},
	{0x327C,0x68},
	{0x327D,0x0B},
	{0x327E,0x64},
	{0x327F,0x0B},
	{0x3284,0x6A},
	{0x3285,0x0B},
	{0x3286,0x66},
	{0x3287,0x0B},
	{0x3282,0x60},
	{0x3283,0x0B},
	{0x328A,0x62},
	{0x328B,0x0B},
	{0x3B55,0x01},
	{0x3B56,0x01},
	{0x3141,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};


/*
 * the order of the cv4002_reboot
 _sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting cv4002_win_sizes[] = {
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = cv4002_init_regs_2560_1440_30fps_mipi,
	}
};

static struct tx_isp_sensor_win_setting *wsize = &cv4002_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list cv4002_stream_on_mipi[] = {
	{0x3000, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list cv4002_stream_off_mipi[] = {
	{0x3000, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int cv4002_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int cv4002_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int cv4002_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4002_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int cv4002_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4002_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int cv4002_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int cv4002_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int cv4002_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4002_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int cv4002_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4002_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int cv4002_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = ISP_SUCCESS;

	rate = private_clk_get_rate(sensor->mclk);
	if(((rate / 1000) % (mclk / 1000)) != 0) {
		switch(mclk) {
		case 24000000:
			ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
			break;
		case 27000000:
		case 37125000:
			ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK1));
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

	private_clk_set_rate(sensor->mclk, mclk);
	private_clk_prepare_enable(sensor->mclk);

error:
	return ret;
}

static int cv4002_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int cv4002_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &cv4002_win_sizes[0];
		cv4002_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		cv4002_attr.again.value = 0;
		cv4002_attr.again.max = 327675;
		cv4002_attr.again.min = 0;
		cv4002_attr.again.apply_delay = 2;

		cv4002_attr.integration_time.value = 2000;
		cv4002_attr.integration_time.max = 3200 - 8;
		cv4002_attr.integration_time.min = 4;
		cv4002_attr.integration_time.apply_delay = 2;

		cv4002_attr.total_width = 1484;
		cv4002_attr.total_height = 3200;

		cv4002_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		cv4002_attr.hcg.base_gain = ;
		cv4002_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		cv4002_attr.again_short.value = ;
		cv4002_attr.again_short.max = ;
		cv4002_attr.again_short.min = ;
		cv4002_attr.again_short.apply_delay = ;

		cv4002_attr.integration_time_short.value = ;
		cv4002_attr.integration_time_short.max = ;
		cv4002_attr.integration_time_short.min = ;
		cv4002_attr.integration_time_short.apply_delay = ;

		cv4002_attr.wdr_cache = wdr_line * cv4002_attr.total_width;

#ifdef SENSOR_HCG
		cv4002_attr.hcg_short.base_gain = ;
		cv4002_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(cv4002_attr.mipi)), (void *)(&cv4002_mipi_15fps_linear), sizeof(cv4002_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int cv4002_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	cv4002_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		cv4002_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		cv4002_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		cv4002_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		cv4002_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		cv4002_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = cv4002_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	cv4002_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int cv4002_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = cv4002_read(sd, 0x3003, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = cv4002_read(sd, 0x3002, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int cv4002_g_chip_ident(struct tx_isp_subdev *sd,
							   struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	cv4002_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "cv4002_reset");
		// printk("-----cv4002_reset:%d",info->rst_gpio);
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(100);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "cv4002_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = cv4002_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an cv4002 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", cv4002_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "cv4002", sizeof("cv4002"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int cv4002_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int cv4002_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = cv4002_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int cv4002_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = cv4002_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int cv4002_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	cv4002_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int cv4002_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = cv4002_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = cv4002_write_array(sd, cv4002_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("cv4002 stream on\n");
		}

	} else {
		ret = cv4002_write_array(sd, cv4002_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("cv4002 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int cv4002_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	int shr0 = 0;

	shr0 = cv4002_attr.total_height - it;
	shr0 = ((shr0 >> 1) << 1);
	ret += cv4002_write(sd, 0x304a, (unsigned char)((shr0 >> 16) & 0xff));
	ret += cv4002_write(sd, 0x3049, (unsigned char)((shr0 >> 8) & 0xff));
	ret += cv4002_write(sd, 0x3048, (unsigned char)(shr0 & 0xff));

	ret += cv4002_write(sd, 0x3154, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int cv4002_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	return ret;
}

static int cv4002_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	return ret;
}
#endif /* SENSOR_EXPO */

static int cv4002_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv4002_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv4002_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = cv4002_attr_set(sd, wsize);
	}

	return ret;
}

static int cv4002_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1484 * 3200 * 30;  /**< HTS * VTS * FPS */
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
	ret += cv4002_read(sd, 0x3025, &val);
	hts = val;
	val = 0;
	ret += cv4002_read(sd, 0x3024, &val);
	hts = (hts << 8) | val;
	if (0 != ret) {
		ISP_ERROR("err: cv4002 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	cv4002_write(sd, 0x3020, (unsigned char) (vts & 0xff));
	cv4002_write(sd, 0x3031, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: cv4002_write err\n");
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

static int cv4002_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;
	// return 0;
	/* 2'b01:mirror,2'b10:filp */
	// ret = cv4002_read(sd, 0x3028, &val);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val &= 0xFC;
		sensor->video.mbus.code = TISP_VI_FMT_SRGGB10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val = ((val & 0xFD) | 0x01);
		sensor->video.mbus.code = TISP_VI_FMT_SGRBG10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val = ((val & 0xFE) | 0x02);
		sensor->video.mbus.code = TISP_VI_FMT_SGBRG10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val |= 0x03;
		sensor->video.mbus.code = TISP_VI_FMT_SBGGR10_1X10;
		break;
	}
	ret += cv4002_write(sd, 0x3028, val);

	sensor->video.hvflip_mode = par->hvflip;
	sensor->video.mbus_change = 1;
	cv4002_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int cv4002_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int cv4002_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int cv4002_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int cv4002_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = cv4002_write_array(sd, cv4002_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		cv4002_setting_select(sd, 1);
		cv4002_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		cv4002_setting_select(sd, 0);
		cv4002_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int cv4002_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = cv4002_write_array(sd, wsize->regs);
	ret = cv4002_write_array(sd, cv4002_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int cv4002_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	// return 0;
	switch (cmd) {
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = cv4002_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = cv4002_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = cv4002_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = cv4002_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = cv4002_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = cv4002_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = cv4002_write_array(sd, cv4002_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = cv4002_write_array(sd, cv4002_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = cv4002_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = cv4002_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = cv4002_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = cv4002_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = cv4002_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = cv4002_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = cv4002_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops cv4002_core_ops = {
	.g_chip_ident = cv4002_g_chip_ident,
	.reset = cv4002_reset,
	.init = cv4002_init,
	.g_register = cv4002_g_register,
	.s_register = cv4002_s_register,
};

static struct tx_isp_subdev_video_ops cv4002_video_ops = {
	.s_stream = cv4002_s_stream,
};

static struct tx_isp_subdev_sensor_ops cv4002_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = cv4002_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops cv4002_ops = {
	.core = &cv4002_core_ops,
	.video = &cv4002_video_ops,
	.sensor = &cv4002_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "cv4002",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int cv4002_probe(struct i2c_client *client,
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
	sensor->video.attr = &cv4002_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &cv4002_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->cv4002\n");

	return 0;
}

static int cv4002_remove(struct i2c_client *client)
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

static const struct i2c_device_id cv4002_id[] = {
	{"cv4002", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, cv4002_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver cv4002_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "cv4002",
	},
	.probe          = cv4002_probe,
	.remove         = cv4002_remove,
	.id_table       = cv4002_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_cv4002(void) {
	return private_i2c_add_driver(&cv4002_driver);
}

static __exit void exit_cv4002(void) {
	private_i2c_del_driver(&cv4002_driver);
}

module_init(init_cv4002);
module_exit(exit_cv4002);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_cv4002(void) {
	return private_i2c_add_driver(&cv4002_driver);
}

static void exit_first_cv4002(void) {
	private_i2c_del_driver(&cv4002_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "cv4002",
	.i2c_addr = 0x35,
	.width = 2560,
	.height = 1440,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_cv4002,
	.exit_sensor = exit_first_cv4002
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for cv4002 sensor");
MODULE_LICENSE("GPL");
