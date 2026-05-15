/*
 * cv8001.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps    dvdd
 *  0           3840*2160       15        mipi_2lane    linear  10        15       null
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
#define SENSOR_VERSION  "H20241108b"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x80)
#define SENSOR_CHIP_ID_L    (0x01)
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

struct tx_isp_sensor_attribute cv8001_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut cv8001_again_lut[] = {
	{0x0, 0},//1.0
	{0x1, 3265},//1.035
	{0x2, 6531},//1.072
	{0x3, 9796},//1.109
	{0x4, 13062},//1.148
	{0x5, 16327},//1.189
	{0x6, 19593},//1.23
	{0x7, 22859},//1.274
	{0x8, 26124},//1.318
	{0x9, 29390},//1.365
	{0xa, 32655},//1.413
	{0xb, 35921},//1.462
	{0xc, 39187},//1.514
	{0xd, 42452},//1.567
	{0xe, 45718},//1.622
	{0xf, 48983},//1.679
	{0x10, 52249},//1.738
	{0x11, 55514},//1.799
	{0x12, 58780},//1.862
	{0x13, 62046},//1.928
	{0x14, 65311},//1.995
	{0x15, 68577},//2.065
	{0x16, 71842},//2.138
	{0x17, 75108},//2.213
	{0x18, 78374},//2.291
	{0x19, 81639},//2.371
	{0x1a, 84905},//2.455
	{0x1b, 88170},//2.541
	{0x1c, 91436},//2.63
	{0x1d, 94702},//2.723
	{0x1e, 97967},//2.818
	{0x1f, 101233},//2.917
	{0x20, 104498},//3.02
	{0x21, 107764},//3.126
	{0x22, 111029},//3.236
	{0x23, 114295},//3.35
	{0x24, 117561},//3.467
	{0x25, 120826},//3.589
	{0x26, 124092},//3.715
	{0x27, 127357},//3.846
	{0x28, 130623},//3.981
	{0x29, 133889},//4.121
	{0x2a, 137154},//4.266
	{0x2b, 140420},//4.416
	{0x2c, 143685},//4.571
	{0x2d, 146951},//4.732
	{0x2e, 150217},//4.898
	{0x2f, 153482},//5.07
	{0x30, 156748},//5.248
	{0x31, 160013},//5.433
	{0x32, 163279},//5.623
	{0x33, 166544},//5.821
	{0x34, 169810},//6.026
	{0x35, 173076},//6.237
	{0x36, 176341},//6.457
	{0x37, 179607},//6.683
	{0x38, 182872},//6.918
	{0x39, 186138},//7.161
	{0x3a, 189404},//7.413
	{0x3b, 192669},//7.674
	{0x3c, 195935},//7.943
	{0x3d, 199200},//8.222
	{0x3e, 202466},//8.511
	{0x3f, 205732},//8.81
	{0x40, 208997},//9.12
	{0x41, 212263},//9.441
	{0x42, 215528},//9.772
	{0x43, 218794},//10.116
	{0x44, 222059},//10.471
	{0x45, 225325},//10.839
	{0x46, 228591},//11.22
	{0x47, 231856},//11.614
	{0x48, 235122},//12.023
	{0x49, 238387},//12.445
	{0x4a, 241653},//12.882
	{0x4b, 244919},//13.335
	{0x4c, 248184},//13.804
	{0x4d, 251450},//14.289
	{0x4e, 254715},//14.791
	{0x4f, 257981},//15.311
	{0x50, 261247},//15.849
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int cv8001_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = cv8001_again_lut;
	while (lut->gain <= cv8001_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == cv8001_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int cv8001_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = cv8001_again_lut;
	while(lut->gain <= cv8001_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == cv8001_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int cv8001_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int cv8001_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int cv8001_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus cv8001_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1000,
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

struct tx_isp_dvp_bus cv8001_dvp = {
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

struct tx_isp_sensor_attribute cv8001_attr = {
	.name = "cv8001",
	.chip_id = 0x8001,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x35,
	.sensor_ctrl.alloc_again = cv8001_alloc_again,
	.sensor_ctrl.alloc_dgain = cv8001_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = cv8001_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list cv8001_init_regs_3840_2160_15fps_mipi[] = {
	{0x3401, 0x01},
	{0x3915, 0x00},
	{0x3220, 0x08},
	{0x3185, 0x03},
	{0x318A, 0x03},
	{0x3020, 0x04},
	{0x3040, 0x01},
	{0x3048, 0x08},
	{0x3049, 0x00},
	{0x304A, 0x00},
	{0x304B, 0x0F},
	{0x3044, 0x04},
	{0x3045, 0x00},
	{0x3046, 0x70},
	{0x3047, 0x08},
	{0x3041, 0x00},
	{0x3b68, 0x00},
	{0x3b69, 0x00},
	{0x3b6a, 0xff},
	{0x3b6b, 0x01},
	{0x387D, 0x3F},
	{0x397a, 0x08},
	{0x3804, 0x14},
	{0x3587, 0x2a},
	{0x3348, 0x00},
	{0x31AC, 0xC8},
	{0x35EC, 0x08},
	{0x3220, 0x08},
	{0x3b75, 0x00},
	{0x3b5E, 0x01},
	{0x3a10, 0x0A},
	{0x3a11, 0x0A},
	{0x3a92, 0x56},
	{0x3914, 0x2E},
	{0x3908, 0x4A},
	{0x302C, 0x8a},
	{0x302D, 0x01},
	{0x3028, 0x40},
	{0x3029, 0x12},
	{0x316C, 0x02},
	{0x3686, 0x03},
	{0x36b8, 0x00},
	{0x36b6, 0x00},
	{0x36C0, 0x00},
	{0x36C5, 0x02},
	{0x36B6, 0x02},
	{0x35DF, 0x00},
	{0x35C4, 0x09},
	{0x3685, 0x01},
	{0x3A86, 0x01},
	{0x3A88, 0x07},
	{0x3A8C, 0x3C},
	{0x3B19, 0x0A},
	{0x3B1C, 0x2B},
	{0x3518, 0x6A},
	{0x3519, 0x00},
	{0x351A, 0x08},
	{0x3852, 0x00},
	{0x36AA, 0x00},
	{0x3000, 0x00},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the cv8001_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting cv8001_win_sizes[] = {
	{
		.width          = 3840,
		.height         = 2160,
		.fps            = 15 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = cv8001_init_regs_3840_2160_15fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &cv8001_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list cv8001_stream_on_mipi[] = {
	{0x3000, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list cv8001_stream_off_mipi[] = {
	{0x3000, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int cv8001_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int cv8001_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int cv8001_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv8001_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int cv8001_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv8001_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int cv8001_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int cv8001_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int cv8001_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv8001_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int cv8001_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv8001_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int cv8001_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int cv8001_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int cv8001_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &cv8001_win_sizes[0];
		cv8001_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		cv8001_attr.again.value = 65536;
		cv8001_attr.again.max = 261247;
		cv8001_attr.again.min = 0;
		cv8001_attr.again.apply_delay = 2;

		cv8001_attr.integration_time.value = 0xb60;
		cv8001_attr.integration_time.max = 4672 - 8;
		cv8001_attr.integration_time.min = 2;
		cv8001_attr.integration_time.apply_delay = 2;

		cv8001_attr.total_width = 394;
		cv8001_attr.total_height = 4672;

		cv8001_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		cv8001_attr.hcg.base_gain = ;
		cv8001_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		cv8001_attr.again_short.value = ;
		cv8001_attr.again_short.max = ;
		cv8001_attr.again_short.min = ;
		cv8001_attr.again_short.apply_delay = ;

		cv8001_attr.integration_time_short.value = ;
		cv8001_attr.integration_time_short.max = ;
		cv8001_attr.integration_time_short.min = ;
		cv8001_attr.integration_time_short.apply_delay = ;

		cv8001_attr.wdr_cache = wdr_line * cv8001_attr.total_width;

#ifdef SENSOR_HCG
		cv8001_attr.hcg_short.base_gain = ;
		cv8001_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(cv8001_attr.mipi)), (void *)(&cv8001_mipi_linear), sizeof(cv8001_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int cv8001_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	cv8001_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		cv8001_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		cv8001_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		cv8001_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		cv8001_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		cv8001_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = cv8001_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
    cv8001_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int cv8001_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = cv8001_read(sd, 0x3003, &v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = cv8001_read(sd, 0x3002, &v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int cv8001_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	cv8001_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "cv8001_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "cv8001_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = cv8001_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an cv8001 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", cv8001_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "cv8001", sizeof("cv8001"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int cv8001_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int cv8001_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = cv8001_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int cv8001_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = cv8001_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int cv8001_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	cv8001_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int cv8001_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = cv8001_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = cv8001_write_array(sd, cv8001_stream_on_mipi);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("cv8001 stream on\n");
		}

	} else {
		ret = cv8001_write_array(sd, cv8001_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("cv8001 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int cv8001_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	int shr0 = 0;

	/* shr0 = cv8001_attr.integration_time.max - it; */
	shr0 = cv8001_attr.total_height - it;
	shr0 = ((shr0 >> 1) << 1);
	ret += cv8001_write(sd, 0x3006, 0x01);
	ret += cv8001_write(sd, 0x3061, (unsigned char)((shr0 >> 8) & 0xff));
	ret += cv8001_write(sd, 0x3060, (unsigned char)(shr0 & 0xff));

	ret += cv8001_write(sd, 0x3164, (unsigned char)(again & 0xff));
	ret += cv8001_write(sd, 0x3006, 0x00);

	return ret;
}
#else
static int cv8001_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int shr0 = 0;

	shr0 = cv8001_attr.integration_time.max - value;
	shr0 = ((shr0 >> 1) << 1);
	ret += cv8001_write(sd, 0x3006, 0x01);
	ret += cv8001_write(sd, 0x3061, (unsigned char)((shr0 >> 8) & 0xff));
	ret += cv8001_write(sd, 0x3060, (unsigned char)(shr0 & 0xff));
	ret += cv8001_write(sd, 0x3006, 0x00);

	return ret;
}

static int cv8001_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += cv8001_write(sd, 0x3006, 0x01);
	ret += cv8001_write(sd, 0x3164, (unsigned char)(value & 0xff));
	ret += cv8001_write(sd, 0x3006, 0x00);

	return ret;
}
#endif /* SENSOR_EXPO */

static int cv8001_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv8001_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv8001_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = cv8001_attr_set(sd, wsize);
	}

	return ret;
}

static int cv8001_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 394 * 4672 * 15;  /**< HTS * VTS * FPS */
		max_fps = 15;
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
	ret += cv8001_read(sd, 0x302d, &val);
	hts = val;
	val = 0;
	ret += cv8001_read(sd, 0x302c, &val);
	hts = (hts << 8) | val;
	if (0 != ret) {
		ISP_ERROR("err: cv8001 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	cv8001_write(sd, 0x3028, (unsigned char) (vts & 0xff));
	cv8001_write(sd, 0x3029, (unsigned char) (vts >> 8));
	cv8001_write(sd, 0x302a, (unsigned char) (vts >> 16));

	if (0 != ret) {
		ISP_ERROR("err: cv8001_write err\n");
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

static int cv8001_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

    par->drop_frame = 0;
    par->reset = 0;

    /* 2'b01:mirror,2'b10:filp */
    ret = cv8001_read(sd, 0x3034, &val);
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
    ret += cv8001_write(sd, 0x3034, val);

    sensor->video.hvflip_mode = par->hvflip;
    sensor->video.mbus_change = 1;
    cv8001_attr_set(sd, wsize);


    return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int cv8001_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int cv8001_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int cv8001_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int cv8001_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = cv8001_write_array(sd, cv8001_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		cv8001_setting_select(sd, 1);
		cv8001_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		cv8001_setting_select(sd, 0);
		cv8001_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int cv8001_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = cv8001_write_array(sd, wsize->regs);
	ret = cv8001_write_array(sd, cv8001_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int cv8001_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = cv8001_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = cv8001_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = cv8001_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = cv8001_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = cv8001_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = cv8001_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = cv8001_write_array(sd, cv8001_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = cv8001_write_array(sd, cv8001_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = cv8001_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = cv8001_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = cv8001_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = cv8001_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = cv8001_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = cv8001_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = cv8001_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops cv8001_core_ops = {
	.g_chip_ident = cv8001_g_chip_ident,
	.reset = cv8001_reset,
	.init = cv8001_init,
	.g_register = cv8001_g_register,
	.s_register = cv8001_s_register,
};

static struct tx_isp_subdev_video_ops cv8001_video_ops = {
	.s_stream = cv8001_s_stream,
};

static struct tx_isp_subdev_sensor_ops cv8001_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = cv8001_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops cv8001_ops = {
	.core = &cv8001_core_ops,
	.video = &cv8001_video_ops,
	.sensor = &cv8001_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "cv8001",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int cv8001_probe(struct i2c_client *client,
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
	sensor->video.attr = &cv8001_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &cv8001_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->cv8001\n");

	return 0;
}

static int cv8001_remove(struct i2c_client *client)
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

static const struct i2c_device_id cv8001_id[] = {
	{"cv8001", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, cv8001_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver cv8001_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "cv8001",
	},
	.probe          = cv8001_probe,
	.remove         = cv8001_remove,
	.id_table       = cv8001_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_cv8001(void) {
	return private_i2c_add_driver(&cv8001_driver);
}

static __exit void exit_cv8001(void) {
	private_i2c_del_driver(&cv8001_driver);
}

module_init(init_cv8001);
module_exit(exit_cv8001);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_cv8001(void) {
	return private_i2c_add_driver(&cv8001_driver);
}

static void exit_first_cv8001(void) {
	private_i2c_del_driver(&cv8001_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "cv8001",
	.i2c_addr = 0x35,
	.width = 3840,
	.height = 2160,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_cv8001,
	.exit_sensor = exit_first_cv8001
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for cv8001 sensor");
MODULE_LICENSE("GPL");
