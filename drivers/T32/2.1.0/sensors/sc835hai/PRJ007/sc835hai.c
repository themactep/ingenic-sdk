/*
 * sc835hai.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           3840*2160       20        mipi_4lane   linear  	10      20
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
#define SENSOR_VERSION  "H20250730a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xc1)
#define SENSOR_CHIP_ID_L    (0x70)
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

struct tx_isp_sensor_attribute sc835hai_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc835hai_again_lut[] = {
	{0x0020,0},
	{0x0021,2886},
	{0x0022,5776},
	{0x0023,8494},
	{0x0024,11136},
	{0x0025,13706},
	{0x0026,16288},
	{0x0027,18724},
	{0x0028,21098},
	{0x0029,23414},
	{0x002A,25747},
	{0x002B,27953},
	{0x002C,30109},
	{0x002D,32217},
	{0x002E,34345},
	{0x002F,36362},
	{0x8020,38336},
	{0x8021,41253},
	{0x8022,44083},
	{0x8023,46830},
	{0x8024,49500},
	{0x8025,52042},
	{0x8026,54571},
	{0x8027,57034},
	{0x8028,59434},
	{0x8029,61775},
	{0x802A,64059},
	{0x802B,66289},
	{0x802C,68468},
	{0x802D,70553},
	{0x802E,72637},
	{0x802F,74676},
	{0x8030,76672},
	{0x8031,78627},
	{0x8032,80542},
	{0x8033,82419},
	{0x8034,84260},
	{0x8035,86027},
	{0x8036,87799},
	{0x8037,89539},
	{0x8038,91247},
	{0x8039,92925},
	{0x803A,94573},
	{0x803B,96194},
	{0x803C,97787},
	{0x803D,99321},
	{0x803E,100862},
	{0x803F,102379},
	{0x8120,103872},
	{0x8121,106789},
	{0x8122,109619},
	{0x8123,112338},
	{0x8124,115008},
	{0x8125,117606},
	{0x8126,120134},
	{0x8127,122570},
	{0x8128,124970},
	{0x8129,127311},
	{0x812A,129595},
	{0x812B,131802},
	{0x812C,133981},
	{0x812D,136112},
	{0x812E,138195},
	{0x812F,140212},
	{0x8130,142208},
	{0x8131,144163},
	{0x8132,146078},
	{0x8133,147935},
	{0x8134,149776},
	{0x8135,151582},
	{0x8136,153354},
	{0x8137,155075},
	{0x8138,156783},
	{0x8139,158461},
	{0x813A,160109},
	{0x813B,161713},
	{0x813C,163306},
	{0x813D,164873},
	{0x813E,166414},
	{0x813F,167915},
	{0x8320,169408},
	{0x8321,172325},
	{0x8322,175140},
	{0x8323,177888},
	{0x8324,180544},
	{0x8325,183142},
	{0x8326,185656},
	{0x8327,188119},
	{0x8328,190506},
	{0x8329,192847},
	{0x832A,195119},
	{0x832B,197350},
	{0x832C,199517},
	{0x832D,201648},
	{0x832E,203720},
	{0x832F,205759},
	{0x8330,207744},
	{0x8331,209699},
	{0x8332,211604},
	{0x8333,213481},
	{0x8334,215312},
	{0x8335,217118},
	{0x8336,218880},
	{0x8337,220620},
	{0x8338,222319},
	{0x8339,223997},
	{0x833A,225637},
	{0x833B,227257},
	{0x833C,228842},
	{0x833D,230409},
	{0x833E,231942},
	{0x833F,233459},
	{0x8720,234944},
	{0x8721,237854},
	{0x8722,240676},
	{0x8723,243417},
	{0x8724,246080},
	{0x8725,248671},
	{0x8726,251192},
	{0x8727,253648},
	{0x8728,256042},
	{0x8729,258377},
	{0x872A,260655},
	{0x872B,262880},
	{0x872C,265053},
	{0x872D,267178},
	{0x872E,269256},
	{0x872F,271290},
	{0x8730,273280},
	{0x8731,275230},
	{0x8732,277140},
	{0x8733,279012},
	{0x8734,280848},
	{0x8735,282649},
	{0x8736,284416},
	{0x8737,286151},
	{0x8738,287855},
	{0x8739,289528},
	{0x873A,291173},
	{0x873B,292789},
	{0x873C,294378},
	{0x873D,295941},
	{0x873E,297478},
	{0x873F,298991},
	{0x8f20,300480},
	{0x8f21,303390},
	{0x8f22,306212},
	{0x8f23,308953},
	{0x8f32,342676},
	{0x8f33,344548},
	{0x8f34,346384},
	{0x8f35,348185},
	{0x8f36,349952},
	{0x8f37,351687},
	{0x8f38,353391},
	{0x8f39,355064},
	{0x8f3A,356709},
	{0x8f3B,358325},
	{0x8f3C,359914},
	{0x8f3D,361477},
	{0x8f3E,363014},
	{0x8f3F,364527},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc835hai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc835hai_again_lut;
	while (lut->gain <= sc835hai_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc835hai_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc835hai_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc835hai_again_lut;
	while(lut->gain <= sc835hai_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc835hai_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc835hai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc835hai_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc835hai_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc835hai_mipi_linear_4lane_clk_560 = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 560,
	.lans = 4,
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

struct tx_isp_dvp_bus sc835hai_dvp = {
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

struct tx_isp_sensor_attribute sc835hai_attr = {
	.name = "sc835hai",
	.chip_id = 0xc170,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.sensor_ctrl.alloc_again = sc835hai_alloc_again,
	.sensor_ctrl.alloc_dgain = sc835hai_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc835hai_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc835hai_init_regs_3840_2160_20fps_mipi_560Mbps[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x58},
	{0x30b8,0x44},
	{0x320c,0x08},
	{0x320d,0x34},
	{0x320e,0x0a},
	{0x320f,0x3b},
	{0x3301,0x0a},
	{0x3302,0x10},
	{0x3303,0x10},
	{0x3304,0x68},
	{0x3305,0x00},
	{0x3306,0x70},
	{0x3307,0x08},
	{0x3308,0x14},
	{0x3309,0x98},
	{0x330a,0x00},
	{0x330b,0xf8},
	{0x330c,0x10},
	{0x330d,0x08},
	{0x330e,0x3c},
	{0x331e,0x41},
	{0x331f,0x71},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x334c,0x10},
	{0x335d,0x60},
	{0x3364,0x5e},
	{0x3366,0x01},
	{0x3367,0x04},
	{0x338f,0x80},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x07},
	{0x3393,0x28},
	{0x3394,0x4c},
	{0x3395,0x4c},
	{0x3396,0x01},
	{0x3397,0x03},
	{0x3398,0x07},
	{0x3399,0x0e},
	{0x339a,0x12},
	{0x339b,0x4c},
	{0x339c,0x4c},
	{0x33ad,0x24},
	{0x33ae,0x58},
	{0x33af,0x88},
	{0x33b2,0x50},
	{0x33b3,0x20},
	{0x33f8,0x00},
	{0x33f9,0x88},
	{0x33fa,0x00},
	{0x33fb,0xa0},
	{0x33fc,0x43},
	{0x33fd,0x47},
	{0x349f,0x03},
	{0x34a6,0x43},
	{0x34a7,0x47},
	{0x34a8,0x20},
	{0x34a9,0x20},
	{0x34aa,0x01},
	{0x34ab,0x10},
	{0x34ac,0x01},
	{0x34ad,0x28},
	{0x34f8,0x43},
	{0x34f9,0x08},
	{0x3632,0x64},
	{0x363b,0x16},
	{0x363c,0x0e},
	{0x363d,0x8e},
	{0x363e,0x6c},
	{0x3654,0x00},
	{0x3674,0x94},
	{0x3675,0x84},
	{0x3676,0x68},
	{0x367c,0x41},
	{0x367d,0x43},
	{0x3690,0x35},
	{0x3691,0x35},
	{0x3692,0x45},
	{0x3693,0x40},
	{0x3694,0x41},
	{0x3696,0x81},
	{0x3697,0x80},
	{0x3698,0x80},
	{0x3699,0x83},
	{0x369a,0x81},
	{0x369b,0xff},
	{0x369c,0xff},
	{0x369d,0xff},
	{0x36a2,0x40},
	{0x36a3,0x41},
	{0x36a4,0x43},
	{0x36a5,0x47},
	{0x36a6,0x4f},
	{0x36a7,0x4f},
	{0x36a8,0x4f},
	{0x36d0,0x15},
	{0x36ea,0x0e},
	{0x36eb,0x04},
	{0x36ec,0x53},
	{0x36ed,0x2a},
	{0x370f,0x01},
	{0x3721,0x6c},
	{0x3724,0xe5},
	{0x3725,0xa8},
	{0x3727,0x14},
	{0x37b0,0x17},
	{0x37b1,0x9b},
	{0x37b2,0xfb},
	{0x37b3,0x41},
	{0x37b4,0x43},
	{0x37fa,0x0b},
	{0x37fb,0x31},
	{0x37fc,0x00},
	{0x37fd,0x26},
	{0x3905,0x0f},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0xd3},
	{0x3937,0x70},
	{0x3939,0x0f},
	{0x393a,0xf8},
	{0x3e00,0x01},
	{0x3e01,0x46},
	{0x3e02,0x90},
	{0x3e16,0x00},
	{0x3e17,0xbc},
	{0x3e18,0x00},
	{0x3e19,0xbc},
	{0x4424,0x02},
	{0x4509,0x1a},
	{0x450d,0x0b},
	{0x4800,0x24},
	{0x4837,0x1c},
	{0x5000,0x0e},
	{0x550e,0x02},
	{0x550f,0x1c},
	{0x5510,0x28},
	{0x575c,0x10},
	{0x575d,0x08},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x08},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x36e9,0x44},
	{0x37f9,0x44},
	/* {SENSOR_REG_DELAY,0x5}, */
	/* {0x0100,0x01}, */
	{SENSOR_REG_END, 0x00},/* END MARKER */
};
/*
 * the order of the sc835hai_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc835hai_win_sizes[] = {
	/* 3840*2160 [0] */
	{
		.width          = 3840,
		.height         = 2160,
		.fps            = 20 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc835hai_init_regs_3840_2160_20fps_mipi_560Mbps,
	},

};

static struct tx_isp_sensor_win_setting *wsize = &sc835hai_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc835hai_stream_on_mipi[] = {
	{SENSOR_REG_DELAY,0x5},
	{0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc835hai_stream_off_mipi[] = {
	{SENSOR_REG_DELAY,0x5},
	{0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc835hai_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc835hai_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc835hai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc835hai_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc835hai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc835hai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc835hai_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc835hai_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc835hai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc835hai_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc835hai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc835hai_write(sd, vals->reg_num, vals->value);
			printk("{0x%04x,0x%02x},\n",vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc835hai_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc835hai_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc835hai_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc835hai_win_sizes[0];
		sc835hai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc835hai_attr.again.value = 65536;
		sc835hai_attr.again.max = 364527;
		sc835hai_attr.again.min = 0;
		sc835hai_attr.again.apply_delay = 2;

		sc835hai_attr.integration_time.value = 0xb60;
		sc835hai_attr.integration_time.max = 2619 - 7;//(2619*2 - 13)/2
		sc835hai_attr.integration_time.min = 4;
		sc835hai_attr.integration_time.apply_delay = 2;

		sc835hai_attr.total_width = 2100;
		sc835hai_attr.total_height = 2619;

		sc835hai_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc835hai_attr.hcg.base_gain = ;
		sc835hai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc835hai_attr.again_short.value = ;
		sc835hai_attr.again_short.max = ;
		sc835hai_attr.again_short.min = ;
		sc835hai_attr.again_short.apply_delay = ;

		sc835hai_attr.integration_time_short.value = ;
		sc835hai_attr.integration_time_short.max = ;
		sc835hai_attr.integration_time_short.min = ;
		sc835hai_attr.integration_time_short.apply_delay = ;

		sc835hai_attr.wdr_cache = wdr_line * sc835hai_attr.total_width;

#ifdef SENSOR_HCG
		sc835hai_attr.hcg_short.base_gain = ;
		sc835hai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc835hai_attr.mipi)), (void *)(&sc835hai_mipi_linear_4lane_clk_560), sizeof(sc835hai_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc835hai_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc835hai_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc835hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc835hai_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc835hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc835hai_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc835hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	if(info->default_boot == 4)
		ret = sc835hai_clk_set(sd, sclka, 27000000);
	else
		ret = sc835hai_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	sc835hai_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc835hai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc835hai_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc835hai_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc835hai_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc835hai_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc835hai_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "sc835hai_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = sc835hai_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc835hai chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc835hai_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc835hai", sizeof("sc835hai"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc835hai_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc835hai_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc835hai_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc835hai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc835hai_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc835hai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc835hai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc835hai_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc835hai_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc835hai_write_array(sd, sc835hai_stream_on_mipi);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc835hai stream on\n");
		}

	} else {
		ret = sc835hai_write_array(sd, sc835hai_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc835hai stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc835hai_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	it <<= 1;
	ret += sc835hai_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc835hai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc835hai_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

	ret += sc835hai_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
	ret += sc835hai_write(sd, 0x3e09, (unsigned char)(again & 0xff));

    return ret;
}
#else
static int sc835hai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc835hai_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc835hai_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc835hai_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

	return ret;
}

static int sc835hai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	switch (info->default_boot) {
	case 0:
		ret += sc835hai_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
		ret += sc835hai_write(sd, 0x3e09, (unsigned char)(value & 0xff));
		break;
	case 1:
		ret += sc835hai_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
		ret += sc835hai_write(sd, 0x3e09, (unsigned char)(value & 0xff));
		ret += sc835hai_write(sd, 0x3e82, (unsigned char)((value >> 8) & 0xff));
		ret += sc835hai_write(sd, 0x3e83, (unsigned char)(value & 0xff));
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this sboot!!!\n");
    }

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc835hai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc835hai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc835hai_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc835hai_attr_set(sd, wsize);
	}

	return ret;
}

static int sc835hai_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 2100 * 2619 * 20;  /**< HTS * VTS * FPS */
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
	ret += sc835hai_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc835hai_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc835hai read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc835hai_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc835hai_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc835hai_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 7;
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

static int sc835hai_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = sc835hai_read(sd, 0x3221, &val);
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
	ret += sc835hai_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc835hai_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc835hai_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc835hai_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc835hai_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc835hai_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc835hai_write_array(sd, sc835hai_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc835hai_setting_select(sd, 1);
		sc835hai_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc835hai_setting_select(sd, 0);
		sc835hai_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc835hai_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc835hai_write_array(sd, wsize->regs);
	ret = sc835hai_write_array(sd, sc835hai_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc835hai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = sc835hai_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc835hai_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc835hai_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc835hai_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc835hai_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc835hai_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc835hai_write_array(sd, sc835hai_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc835hai_write_array(sd, sc835hai_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc835hai_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc835hai_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc835hai_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc835hai_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc835hai_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc835hai_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc835hai_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc835hai_core_ops = {
	.g_chip_ident = sc835hai_g_chip_ident,
	.reset = sc835hai_reset,
	.init = sc835hai_init,
	.g_register = sc835hai_g_register,
	.s_register = sc835hai_s_register,
};

static struct tx_isp_subdev_video_ops sc835hai_video_ops = {
	.s_stream = sc835hai_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc835hai_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc835hai_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc835hai_ops = {
	.core = &sc835hai_core_ops,
	.video = &sc835hai_video_ops,
	.sensor = &sc835hai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "sc835hai",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc835hai_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc835hai_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc835hai_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc835hai\n");

	return 0;
}

static int sc835hai_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc835hai_id[] = {
	{"sc835hai", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc835hai_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc835hai_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc835hai",
	},
	.probe          = sc835hai_probe,
	.remove         = sc835hai_remove,
	.id_table       = sc835hai_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc835hai(void) {
	return private_i2c_add_driver(&sc835hai_driver);
}

static __exit void exit_sc835hai(void) {
	private_i2c_del_driver(&sc835hai_driver);
}

module_init(init_sc835hai);
module_exit(exit_sc835hai);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc835hai(void) {
	return private_i2c_add_driver(&sc835hai_driver);
}

static void exit_first_sc835hai(void) {
	private_i2c_del_driver(&sc835hai_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "sc835hai",
	.i2c_addr = 0x30,
	.width = 3840,
	.height = 2160,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc835hai,
	.exit_sensor = exit_first_sc835hai
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for sc835hai sensor");
MODULE_LICENSE("GPL");
