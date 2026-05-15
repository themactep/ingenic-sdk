/*
 * cv2005s1.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution       fps       interface     mode    raw     max_fps    dvdd    fd_stream_off
 *  0           1920*1080        15        mipi_2lane    linear  10        30       null      support
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

#define SENSOR_CHIP_ID_H    (0x20)
#define SENSOR_CHIP_ID_L    (0x05)
#define SENSOR_OUTPUT_MIN_FPS 1
#define SENSOR_MCLK 27000000

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

struct tx_isp_sensor_attribute cv2005s1_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	uint16_t value;
	unsigned int gain;
};

struct again_lut cv2005s1_again_lut[] = {
	{0x00, 0},
	{0x01, 377},
	{0x02, 753},
	{0x03, 1127},
	{0x04, 1500},
	{0x05, 1872},
	{0x06, 2242},
	{0x07, 2610},
	{0x08, 2978},
	{0x09, 3343},
	{0x0a, 3799},
	{0x0b, 4161},
	{0x0c, 4522},
	{0x0f, 4882},
	{0x0d, 5330},
	{0x0e, 5687},
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
	{0x1b, 10546},
	{0x1c, 10967},
	{0x1d, 11388},
	{0x1e, 11806},
	{0x1f, 12222},
	{0x20, 12637},
	{0x21, 13049},
	{0x22, 13460},
	{0x23, 13869},
	{0x24, 14358},
	{0x25, 14763},
	{0x26, 15167},
	{0x27, 15649},
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
	{0x33, 21022},
	{0x34, 21475},
	{0x35, 21926},
	{0x36, 22375},
	{0x37, 22896},
	{0x38, 23340},
	{0x39, 23782},
	{0x3a, 24295},
	{0x3b, 24733},
	{0x3c, 25241},
	{0x3d, 25746},
	{0x3e, 26249},
	{0x3f, 26678},
	{0x40, 27176},
	{0x41, 27671},
	{0x42, 28164},
	{0x43, 28654},
	{0x44, 29211},
	{0x45, 29695},
	{0x46, 30178},
	{0x47, 30726},
	{0x48, 31203},
	{0x49, 31745},
	{0x4a, 32284},
	{0x4b, 32753},
	{0x4c, 33287},
	{0x4d, 33817},
	{0x4e, 34345},
	{0x4f, 34869},
	{0x50, 35456},
	{0x51, 35974},
	{0x52, 36490},
	{0x53, 37066},
	{0x54, 37576},
	{0x55, 38146},
	{0x56, 38713},
	{0x57, 39276},
	{0x58, 39836},
	{0x59, 40393},
	{0x5a, 40947},
	{0x5b, 41558},
	{0x5c, 42104},
	{0x5d, 42708},
	{0x5e, 43248},
	{0x5f, 43845},
	{0x60, 44438},
	{0x61, 45027},
	{0x62, 45612},
	{0x63, 46252},
	{0x64, 46830},
	{0x65, 47462},
	{0x66, 48032},
	{0x67, 48656},
	{0x68, 49276},
	{0x69, 49891},
	{0x6a, 50558},
	{0x6b, 51165},
	{0x6c, 51824},
	{0x6d, 52423},
	{0x6e, 53072},
	{0x6f, 53771},
	{0x70, 54411},
	{0x71, 55047},
	{0x72, 55731},
	{0x73, 56411},
	{0x74, 57085},
	{0x75, 57755},
	{0x76, 58420},
	{0x77, 59130},
	{0x78, 59786},
	{0x79, 60487},
	{0x7a, 61182},
	{0x7b, 61922},
	{0x7c, 62607},
	{0x7d, 63335},
	{0x7e, 64059},
	{0x7f, 64776},
	{0x80, 65536},
	{0x81, 66289},
	{0x82, 67036},
	{0x83, 67778},
	{0x84, 68559},
	{0x85, 69289},
	{0x86, 70058},
	{0x87, 70866},
	{0x88, 71623},
	{0x89, 72417},
	{0x8a, 73205},
	{0x8b, 74030},
	{0x8c, 74847},
	{0x8d, 75658},
	{0x8e, 76503},
	{0x8f, 77300},
	{0x90, 78173},
	{0x91, 78996},
	{0x92, 79853},
	{0x93, 80743},
	{0x94, 81584},
	{0x95, 82498},
	{0x96, 83363},
	{0x97, 84259},
	{0x98, 85185},
	{0x99, 86064},
	{0x9a, 87011},
	{0x9b, 87948},
	{0x9c, 88876},
	{0x9d, 89831},
	{0x9e, 90777},
	{0x9f, 91749},
	{0xa0, 92747},
	{0xa1, 93735},
	{0xa2, 94712},
	{0xa3, 95748},
	{0xa4, 96773},
	{0xa5, 97786},
	{0xa6, 98823},
	{0xa7, 99881},
	{0xa8, 100959},
	{0xa9, 102058},
	{0xaa, 103144},
	{0xab, 104249},
	{0xac, 105372},
	{0xad, 106483},
	{0xae, 107640},
	{0xaf, 108784},
	{0xb0, 109974},
	{0xb1, 111177},
	{0xb2, 112366},
	{0xb3, 113597},
	{0xb4, 114812},
	{0xb5, 116066},
	{0xb6, 117332},
	{0xb7, 118635},
	{0xb8, 119947},
	{0xb9, 121267},
	{0xba, 122595},
	{0xbb, 123956},
	{0xbc, 125347},
	{0xbd, 126743},
	{0xbe, 128167},
	{0xbf, 129595},
	{0xc0, 131072},
	{0xc1, 132549},
	{0xc2, 134073},
	{0xc3, 135617},
	{0xc4, 137181},
	{0xc5, 138763},
	{0xc6, 140383},
	{0xc7, 142018},
	{0xc8, 143688},
	{0xc9, 145410},
	{0xca, 147140},
	{0xcb, 148899},
	{0xcc, 150702},
	{0xcd, 152547},
	{0xce, 154412},
	{0xcf, 156313},
	{0xd0, 158265},
	{0xd1, 160265},
	{0xd2, 162292},
	{0xd3, 164375},
	{0xd4, 166495},
	{0xd5, 168664},
	{0xd6, 170893},
	{0xd7, 173176},
	{0xd8, 175510},
	{0xd9, 177902},
	{0xda, 180362},
	{0xdb, 182882},
	{0xdc, 185470},
	{0xdd, 188131},
	{0xde, 190870},
	{0xdf, 193703},
	{0xe0, 196608},
	{0xe1, 199609},
	{0xe2, 202706},
	{0xe3, 205919},
	{0xe4, 209234},
	{0xe5, 212666},
	{0xe6, 216238},
	{0xe7, 219948},
	{0xe8, 223810},
	{0xe9, 227828},
	{0xea, 232031},
	{0xeb, 236429},
	{0xec, 241046},
	{0xed, 245898},
	{0xee, 251006},
	{0xef, 256413},
	{0xf0, 262144},
	{0xf1, 268247},
	{0xf2, 274770},
	{0xf3, 281774},
	{0xf4, 289342},
	{0xf5, 297571},
	{0xf6, 306582},
	{0xf7, 316542},
	{0xf8, 327680},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int cv2005s1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = cv2005s1_again_lut;
	while (lut->gain <= cv2005s1_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == cv2005s1_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int cv2005s1_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = cv2005s1_again_lut;
	while(lut->gain <= cv2005s1_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == cv2005s1_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int cv2005s1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int cv2005s1_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int cv2005s1_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus cv2005s1_mipi_15fps_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1200,
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

struct tx_isp_dvp_bus cv2005s1_dvp = {
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

struct tx_isp_sensor_attribute cv2005s1_attr = {
	.name = "cv2005s1",
	.chip_id = 0x2005,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x35,
	.sensor_ctrl.alloc_again = cv2005s1_alloc_again,
	.sensor_ctrl.alloc_dgain = cv2005s1_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = cv2005s1_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list cv2005s1_init_regs_1920_1080_15fps_mipi_master[] = {
	{0x3031, 0x00}, //OB OFF Def
	{0x3204, 0x40}, //BLK LEVEL Def

	{0x359d, 0x00}, //BLK LEVEL Def
			//    {0x3000, 0x00}, //Streaming
	{0x3109 /* CV2005s1_SPLIT_GAIN_EN */ ,0x01},
	//   {0x3000, 0x00}, //Streaming
	{0x389D, 0x0A},
	{0x389C, 0x6A},
	{0x38A0, 0x2B},
	{0x3878, 0x01},
	{0x3879, 0x15},
	{0x356F, 0x02},

	{0x366C, 0x0f},//灯管收光
		       //mipi global timing.
	{0x3420, 0x3f},//THSPREPARE: 61 ns ; min limit: 45 ; max limt: 92.6923076923077
	{0x3422, 0xC7},//THSZERO   : 174 ns ; min limit: 158 ; max limt: 1000
	{0x3424, 0x5f},//THSTRAIL  : 82 ns ; min limit: 69 ; max limt: 1000
	{0x3426, 0x87},//THSEXIT   : 112 ns ; min limit: 100 ; max limt: 1000
	{0x3428, 0x47},//TLPX      : 61 ns ; min limit: 50 ; max limt: 1000
		       // {0x312C, 0x01},//first_frame_fast_mode enable
		       // {0x3814, 0x06},
		       // {0x394A, 0x15},
		       //15fps 低功耗配置
		       //68mW @0dB 15fps LD0 OFF状态下
	{0x3403, 0x10},
	{0x3538, 0x01},
	{0x3628, 0x66},//M HBLK
	{0x3629, 0x7e},
	{0x3510, 0x7d},
	{0x3512, 0x80},
	{0x3513, 0x01},
	{0x3021, 0x04},//HMAX=1242
	{0x3020, 0xda},
	{0x3808, 0x64},//PLL=1.2G
	{0x380a, 0x02},
	{0x301d, 0x0e},//VMAX 3624
	{0x301c, 0x28},
	{0x3834, 0x00},//LDO ON
	{0x3400, 0x11},
	{0x3068, 0x22}, // VSYNC输出
	{0x3072, 0x14}, // master输出的VSYNC delay配置，max, 0xFF
	{0x3073, 0x07}, // master输出的VSYNC delay配置，max, 0x07
	{SENSOR_REG_END, 0x00},/* END MARKER */
};


/*
 * the order of the cv2005s1_reboot
 _sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting cv2005s1_win_sizes[] = {
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 15 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = cv2005s1_init_regs_1920_1080_15fps_mipi_master,
	}
};

static struct tx_isp_sensor_win_setting *wsize = &cv2005s1_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list cv2005s1_stream_on_mipi[] = {
	{0x3000, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list cv2005s1_stream_off_mipi[] = {
	{0x3403, 0x10},
	{0x3000, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int cv2005s1_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int cv2005s1_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int cv2005s1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv2005s1_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int cv2005s1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv2005s1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int cv2005s1_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int cv2005s1_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int cv2005s1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv2005s1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int cv2005s1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv2005s1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int cv2005s1_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int cv2005s1_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int cv2005s1_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &cv2005s1_win_sizes[0];
		cv2005s1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		cv2005s1_attr.again.value = 0;
		cv2005s1_attr.again.max = 327680;
		cv2005s1_attr.again.min = 0;
		cv2005s1_attr.again.apply_delay = 2;

		cv2005s1_attr.integration_time.value = 2000;
		cv2005s1_attr.integration_time.max = 3624 - 5;
		cv2005s1_attr.integration_time.min = 1;
		cv2005s1_attr.integration_time.apply_delay = 2;

		cv2005s1_attr.total_width = 1242;
		cv2005s1_attr.total_height = 3624;

		cv2005s1_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		cv2005s1_attr.hcg.base_gain = ;
		cv2005s1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		cv2005s1_attr.again_short.value = ;
		cv2005s1_attr.again_short.max = ;
		cv2005s1_attr.again_short.min = ;
		cv2005s1_attr.again_short.apply_delay = ;

		cv2005s1_attr.integration_time_short.value = ;
		cv2005s1_attr.integration_time_short.max = ;
		cv2005s1_attr.integration_time_short.min = ;
		cv2005s1_attr.integration_time_short.apply_delay = ;

		cv2005s1_attr.wdr_cache = wdr_line * cv2005s1_attr.total_width;

#ifdef SENSOR_HCG
		cv2005s1_attr.hcg_short.base_gain = ;
		cv2005s1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(cv2005s1_attr.mipi)), (void *)(&cv2005s1_mipi_15fps_linear), sizeof(cv2005s1_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int cv2005s1_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	cv2005s1_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		cv2005s1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		cv2005s1_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		cv2005s1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		cv2005s1_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		cv2005s1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = cv2005s1_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	cv2005s1_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int cv2005s1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = cv2005s1_read(sd, 0x3003, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = cv2005s1_read(sd, 0x3002, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int cv2005s1_g_chip_ident(struct tx_isp_subdev *sd,
				 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	cv2005s1_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "cv2005s1_reset");
		// printk("-----cv2005s1_reset:%d",info->rst_gpio);
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
		ret = private_gpio_request(info->pwdn_gpio, "cv2005s1_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = cv2005s1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an cv2005s1 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", cv2005s1_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "cv2005s1", sizeof("cv2005s1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int cv2005s1_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int cv2005s1_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = cv2005s1_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int cv2005s1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = cv2005s1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int cv2005s1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	cv2005s1_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int cv2005s1_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = cv2005s1_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = cv2005s1_write_array(sd, cv2005s1_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("cv2005s1 stream on\n");
		}

	} else {
		ret = cv2005s1_write_array(sd, cv2005s1_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("cv2005s1 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int cv2005s1_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	int shr0 = 0;

	if(it > (cv2005s1_attr.total_height - 1))
	{
		pr_err("s0 it(0x%X),total_height(0x%X)\n",it,cv2005s1_attr.total_height);
		it = cv2005s1_attr.total_height - 1;
	}
	cv2005s1_attr.integration_time.value = it;
	shr0 = cv2005s1_attr.total_height - it;
	// shr0 = ((shr0 >> 1) << 1);
	// ret += cv2005s1_write(sd, 0x3141, 0x01);
	ret += cv2005s1_write(sd, 0x304a, (unsigned char)((shr0 >> 16) & 0xff));
	ret += cv2005s1_write(sd, 0x3049, (unsigned char)((shr0 >> 8) & 0xff));
	ret += cv2005s1_write(sd, 0x3048, (unsigned char)(shr0 & 0xff));

	ret += cv2005s1_write(sd, 0x3118, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int cv2005s1_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	return ret;
}

static int cv2005s1_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	return ret;
}
#endif /* SENSOR_EXPO */

static int cv2005s1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv2005s1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv2005s1_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = cv2005s1_attr_set(sd, wsize);
	}

	return ret;
}

static int cv2005s1_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1242 * 3624 * 15;  /**< HTS * VTS * FPS */
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
	ret += cv2005s1_read(sd, 0x3021, &val);
	hts = val;
	val = 0;
	ret += cv2005s1_read(sd, 0x3020, &val);
	hts = (hts << 8) | val;
	if (0 != ret) {
		ISP_ERROR("err: cv2005s1 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	cv2005s1_write(sd, 0x301c, (unsigned char) (vts & 0xff));
	cv2005s1_write(sd, 0x301d, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: cv2005s1_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 5;
#else
	/* WDR mode */
	switch (info->default_boot) {
	case 0:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - 5;
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

static int cv2005s1_set_hvflip(struct tx_isp_subdev *sd, void *arg)
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
	// ret = cv2005s1_read(sd, 0x3028, &val);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		ret += cv2005s1_write(sd, 0x3038, 0x05);
		ret += cv2005s1_write(sd, 0x3034, 0x05);
		val = 0x03;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		ret += cv2005s1_write(sd, 0x3038, 0x04);
		ret += cv2005s1_write(sd, 0x3034, 0x05);
		val = 0x02;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		ret += cv2005s1_write(sd, 0x3038, 0x05);
		ret += cv2005s1_write(sd, 0x3034, 0x04);
		val = 0x01;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		ret +=cv2005s1_write(sd, 0x3038, 0x04);
		ret +=cv2005s1_write(sd, 0x3034, 0x04);
		val = 0x00;
		break;
	}
	ret += cv2005s1_write(sd, 0x3028, val);

	sensor->video.hvflip_mode = par->hvflip;
	sensor->video.mbus_change = 1;
	cv2005s1_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

static int cv2005s1_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;
	char *name = (char *)cv2005s1_attr.name;
	// unsigned char v;
	if(data->cmd == TX_SENSOR_PM_RESUME){
		ret = cv2005s1_write(sd, 0x3261, 0x1);//aov
		ret = cv2005s1_write_array(sd, cv2005s1_stream_on_mipi);

		// ret = cv2005s1_read(sd, 0x301c, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x301d, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x3020, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x3021, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x3808, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x380a, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		if(ret != 0){
			printk("%s streamon failed\n",name);
			return ret;
		}
		printk("%s TX_SENSOR_PM_RESUME\n",name);
	}else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
		ret = cv2005s1_write_array(sd, cv2005s1_stream_off_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
			return ret;
		}
		// ret = cv2005s1_read(sd, 0x301c, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x301d, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x3020, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x3021, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x3808, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
		// ret = cv2005s1_read(sd, 0x380a, &v);
		// printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);

		printk("%s TX_SENSOR_PM_SUSPEND.\n",name);
	}else if(data->cmd == TX_SENSOR_PM_PREPARE) {
		printk("%s TX_SENSOR_PM_PREPARE.\n",name);
	}else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
	}else if(data->cmd == TX_SENSOR_PM_STREAMON) {
		ret = cv2005s1_write(sd, 0x3261, 0x0);//continue
		ret = cv2005s1_write_array(sd, cv2005s1_stream_on_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMON.\n",name);
	}else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
		ret = cv2005s1_write_array(sd, cv2005s1_stream_off_mipi);
		if (ret < 0) {
			printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMOFF.\n",name);
	}else {
		printk("[%s]Don't Support this function.\n",__func__);
		return -EINVAL;
	}

	return ret;
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int cv2005s1_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int cv2005s1_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int cv2005s1_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int cv2005s1_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = cv2005s1_write_array(sd, cv2005s1_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		cv2005s1_setting_select(sd, 1);
		cv2005s1_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		cv2005s1_setting_select(sd, 0);
		cv2005s1_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int cv2005s1_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = cv2005s1_write_array(sd, wsize->regs);
	ret = cv2005s1_write_array(sd, cv2005s1_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int cv2005s1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = cv2005s1_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = cv2005s1_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = cv2005s1_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = cv2005s1_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = cv2005s1_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = cv2005s1_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = cv2005s1_write_array(sd, cv2005s1_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = cv2005s1_write_array(sd, cv2005s1_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = cv2005s1_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = cv2005s1_set_hvflip(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret =  cv2005s1_pm_s_stream_ctrl(sd, arg);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = cv2005s1_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = cv2005s1_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = cv2005s1_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = cv2005s1_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = cv2005s1_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops cv2005s1_core_ops = {
	.g_chip_ident = cv2005s1_g_chip_ident,
	.reset = cv2005s1_reset,
	.init = cv2005s1_init,
	.g_register = cv2005s1_g_register,
	.s_register = cv2005s1_s_register,
};

static struct tx_isp_subdev_video_ops cv2005s1_video_ops = {
	.s_stream = cv2005s1_s_stream,
};

static struct tx_isp_subdev_sensor_ops cv2005s1_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = cv2005s1_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops cv2005s1_ops = {
	.core = &cv2005s1_core_ops,
	.video = &cv2005s1_video_ops,
	.sensor = &cv2005s1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "cv2005s1",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int cv2005s1_probe(struct i2c_client *client,
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
	sensor->video.attr = &cv2005s1_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &cv2005s1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->cv2005s1\n");

	return 0;
}

static int cv2005s1_remove(struct i2c_client *client)
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

static const struct i2c_device_id cv2005s1_id[] = {
	{"cv2005s1", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, cv2005s1_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver cv2005s1_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "cv2005s1",
	},
	.probe          = cv2005s1_probe,
	.remove         = cv2005s1_remove,
	.id_table       = cv2005s1_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_cv2005s1(void) {
	return private_i2c_add_driver(&cv2005s1_driver);
}

static __exit void exit_cv2005s1(void) {
	private_i2c_del_driver(&cv2005s1_driver);
}

module_init(init_cv2005s1);
module_exit(exit_cv2005s1);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_cv2005s1(void) {
	return private_i2c_add_driver(&cv2005s1_driver);
}

static void exit_first_cv2005s1(void) {
	private_i2c_del_driver(&cv2005s1_driver);
}

struct tx_isp_sensor_fast_attr sensor1 = {
	.name = "cv2005s1",
	.i2c_addr = 0x35,
	.width = 1920,
	.height = 1080,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_cv2005s1,
	.exit_sensor = exit_first_cv2005s1
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for cv2005s1 sensor");
MODULE_LICENSE("GPL");
