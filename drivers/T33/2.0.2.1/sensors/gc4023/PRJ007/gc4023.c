/*
 * gc4023.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps        dvdd
 *  0           2560*1440       30        mipi_2lane    linear  10        30           null
 *
 * @I2C addr:0x29,0x21
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
#define SENSOR_VERSION  "H20250211a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x40)
#define SENSOR_CHIP_ID_L    (0x23)
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

struct tx_isp_sensor_attribute gc4023_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
    unsigned int index;
    unsigned char reg614;
    unsigned char reg615;
    unsigned char reg218;
    unsigned char reg1467;
    unsigned char reg1468;
    unsigned char regb8;
    unsigned char regb9;
    unsigned int gain;
};

struct again_lut gc4023_again_lut[] = {
    {0x00, 0x00,0x00,0x00,0x07,0x0f,0x01,0x00,     0 },
    {0x01, 0x80,0x02,0x00,0x07,0x0f,0x01,0x0B, 16208 },
    {0x02, 0x01,0x00,0x00,0x07,0x0f,0x01,0x19, 32217 },
    {0x03, 0x81,0x02,0x00,0x07,0x0f,0x01,0x2A, 47690 },
    {0x04, 0x02,0x00,0x00,0x08,0x10,0x02,0x00, 65536 },
    {0x05, 0x82,0x02,0x00,0x08,0x10,0x02,0x17, 81784 },
    {0x06, 0x03,0x00,0x00,0x08,0x10,0x02,0x33, 97213 },
    {0x07, 0x83,0x02,0x00,0x09,0x11,0x03,0x14,113226 },
    {0x08, 0x04,0x00,0x00,0x0A,0x12,0x04,0x00,131072 },
    {0x09, 0x80,0x02,0x20,0x0A,0x12,0x04,0x2F,147001 },
    {0x0A, 0x01,0x00,0x20,0x0B,0x13,0x05,0x26,162766 },
    {0x0B, 0x81,0x02,0x20,0x0C,0x14,0x06,0x28,178990 },
    {0x0C, 0x02,0x00,0x20,0x0D,0x15,0x08,0x00,196608 },
    {0x0D, 0x82,0x02,0x20,0x0D,0x15,0x09,0x1E,212696 },
    {0x0E, 0x03,0x00,0x20,0x0E,0x16,0x0B,0x0C,228446 },
    {0x0F, 0x83,0x02,0x20,0x0E,0x16,0x0D,0x11,244419 },
    {0x10, 0x04,0x00,0x20,0x0F,0x17,0x10,0x00,262144 },
    {0x11, 0x84,0x02,0x20,0x0F,0x17,0x12,0x3D,278158 },
    {0x12, 0x05,0x00,0x20,0x10,0x18,0x16,0x19,293982 },
    {0x13, 0x85,0x02,0x20,0x10,0x18,0x1A,0x22,310012 },
    {0x14, 0xb5,0x04,0x20,0x11,0x19,0x20,0x00,327680 },
    {0x15, 0x85,0x05,0x20,0x11,0x19,0x25,0x3A,343731 },
    {0x16, 0x05,0x08,0x20,0x12,0x1a,0x2C,0x33,359484 },
    {0x17, 0x45,0x09,0x20,0x15,0x1d,0x35,0x05,375550 },
    {0x18, 0x55,0x0a,0x20,0x16,0x1e,0x40,0x00,393216 },
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc4023_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = gc4023_again_lut;
    while (lut->gain <= gc4023_attr.again.max) {
        if (isp_gain == 0) {
            *sensor_again = lut[0].index;
            return 0;
        } else if (isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->index;
            return (lut - 1)->gain;
        } else {
            if ((lut->gain == gc4023_attr.again.max) && (isp_gain >= lut->gain)) {
                *sensor_again = lut->index;
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
unsigned int gc4023_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = gc4023_again_lut;
    while(lut->gain <= gc4023_attr.again_short.max) {
        if(isp_gain == 0) {
            *sensor_again = 0;
            return 0;
        }
        else if(isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        }
        else{
            if((lut->gain == gc4023_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int gc4023_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
    return 0;
}

#ifdef SENSOR_HCG
unsigned int gc4023_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc4023_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc4023_mipi_linear = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 864,
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


struct tx_isp_dvp_bus gc4023_dvp = {
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

struct tx_isp_sensor_attribute gc4023_attr = {
    .name = "gc4023",
    .chip_id = 0x4023,
    .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
    .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
    .cbus_device = 0x29,
    .sensor_ctrl.alloc_again = gc4023_alloc_again,
    .sensor_ctrl.alloc_dgain = gc4023_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
    .sensor_ctrl.alloc_again_short = gc4023_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

    .dgain = {0},
    .dgain_short = {0},
};

static struct regval_list gc4023_init_regs_2560_1440_30fps_mipi[] = {
    /*
     * version 0.2 mclk 27Mhz wpclk 216Mhz rpclk 172.2Mhz mipi 864Mbps/lane
     * cpclk 27Mhz vts = 1500 window 2560 1440
     */
    /*SYSTEM*/
    {0x03fe, 0xf0},
    {0x03fe, 0x00},
    {0x03fe, 0x10},
    {0x03fe, 0x00},
    {0x0a38, 0x00},
    {0x0a38, 0x01},
    {0x0a20, 0x07},
    {0x061c, 0x50},
    {0x061d, 0x22},
    {0x061e, 0x78},
    {0x061f, 0x06},
    {0x0a21, 0x10},
    {0x0a34, 0x40},
    {0x0a35, 0x01},
    {0x0a36, 0x60},
    {0x0a37, 0x06},
    {0x0314, 0x50},
    {0x0315, 0x00},
    {0x031c, 0xce},
    {0x0219, 0x47},
    {0x0342, 0x04},
    {0x0343, 0xb0},
    {0x0259, 0x05},
    {0x025a, 0xa0},
    {0x0340, 0x05},
    {0x0341, 0xdc},
    {0x0347, 0x02},
    {0x0348, 0x0a},
    {0x0349, 0x08},
    {0x034a, 0x05},
    {0x034b, 0xa8},
    {0x0094, 0x0a},
    {0x0095, 0x00},
    {0x0096, 0x05},
    {0x0097, 0xa0},
    {0x0099, 0x04},
    {0x009b, 0x04},
    {0x060c, 0x01},
    {0x060e, 0x08},
    {0x060f, 0x05},
    {0x070c, 0x01},
    {0x070e, 0x08},
    {0x070f, 0x05},
    {0x0909, 0x03},
    {0x0902, 0x04},
    {0x0904, 0x0b},
    {0x0907, 0x54},
    {0x0908, 0x06},
    {0x0903, 0x9d},
    {0x072a, 0x18},
    {0x0724, 0x0a},
    {0x0727, 0x0a},
    {0x072a, 0x18},
    {0x072b, 0x08},
    {0x1466, 0x10},
    {0x1468, 0x0f},
    {0x1467, 0x07},
    {0x1469, 0x80},
    {0x146a, 0xbc},
    {0x0707, 0x07},
    {0x0737, 0x0f},
    {0x061a, 0x00},
    {0x1430, 0x80},
    {0x1407, 0x10},
    {0x1408, 0x16},
    {0x1409, 0x03},
    {0x146d, 0x0e},
    {0x146e, 0x32},
    {0x146f, 0x33},
    {0x1470, 0x2c},
    {0x1471, 0x2d},
    {0x1472, 0x3a},
    {0x1473, 0x3a},
    {0x1474, 0x40},
    {0x1475, 0x36},
    {0x1420, 0x14},
    {0x1464, 0x15},
    {0x146c, 0x40},
    {0x146d, 0x40},
    {0x1423, 0x08},
    {0x1428, 0x10},
    {0x1462, 0x08},
    {0x02ce, 0x04},
    {0x143a, 0x0b},
    {0x142b, 0x88},
    {0x0245, 0xc9},
    {0x023a, 0x08},
    {0x02cd, 0x88},
    {0x0612, 0x02},
    {0x0613, 0xc7},
    {0x0243, 0x03},
    {0x021b, 0x09},
    {0x0089, 0x03},
    {0x0040, 0xa3},
    {0x0075, 0x64},
    {0x0004, 0x0f},
    {0x0002, 0xa9},
    {0x0053, 0x0a},
    {0x0205, 0x0c},
    {0x0202, 0x06},
    {0x0203, 0x27},
    {0x0614, 0x00},
    {0x0615, 0x00},
    {0x0181, 0x0c},
    {0x0182, 0x05},
    {0x0185, 0x01},
    {0x0180, 0x46},
    {0x0100, 0x08},
    {0x0106, 0x38},
    {0x010d, 0x80},
    {0x010e, 0x0c},
    {0x0113, 0x02},
    {0x0114, 0x01},
    {0x0115, 0x10},
    {0x0100, 0x09},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the gc4023_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc4023_win_sizes[] = {
    {
        .width          = 2560,
        .height         = 1440,
        .fps            = 30 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = gc4023_init_regs_2560_1440_30fps_mipi,
    },
};


static struct tx_isp_sensor_win_setting *wsize = &gc4023_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc4023_stream_on_mipi[] = {
	{0x03fe, 0x30},
	{0x03fe, 0x00},
	{0x0100, 0x08},
	{0x0a38, 0x01}, //apwd
	{0x0a34, 0x40}, // pllmp en
	{0x061c, 0x50}, // plldg en
	{0x031c, 0xce}, //not use pll
	{0x0180, 0x46},
	{0x0181, 0x0c},
	{0x0087, 0x51},
	{0x061c, 0x50},
	{0x061d, 0x21},
	{0x061e, 0x6c},
	{0x061f, 0x06},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x060c, 0x01},
	{0x060e, 0x08},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0x08},
	{0x070f, 0x05},
	{0x072a, 0x18},
	{0x0724, 0x0a},
	{0x0727, 0x0a},
	{0x072a, 0x1c},
	{0x072b, 0x0a},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x061a, 0x00},
	{0x0612, 0x02},
	{0x0613, 0xc7},
	{0x031d, 0x2d},
	{0x0100, 0x09},
	{0x031d, 0x28},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list gc4023_stream_off_mipi[] = {
	{0x031d, 0x2d},
	{0x0180, 0x00},
	{0x0181, 0x00},
	{0x0100, 0x00},
	{0x0a34, 0x00}, // pllmp en
	{0x061c, 0x10}, // plldg en
	{0x031c, 0x01}, //not use pll
	{0x0a38, 0x00}, //apwd
	{0x031d, 0x28},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc4023_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc4023_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc4023_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = gc4023_read(sd, vals->reg_num, &val);
            /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif

static int gc4023_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = gc4023_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc4023_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int gc4023_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc4023_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = gc4023_read(sd, vals->reg_num, &val);
            if (ret < 0)
                return ret;
        }
        vals++;
    }
    return 0;
}
#endif

static int gc4023_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = gc4023_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int gc4023_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int gc4023_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int gc4023_setting_select(struct tx_isp_subdev *sd, int deboot)
{
    int ret = ISP_SUCCESS;

    switch (deboot) {
    case 0:
        wsize = &gc4023_win_sizes[0];
        gc4023_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        gc4023_attr.again.value = 65536;
        gc4023_attr.again.max = 393216;
        gc4023_attr.again.min = 0;
        gc4023_attr.again.apply_delay = 2;

        gc4023_attr.integration_time.value = 0x5b8;
        gc4023_attr.integration_time.max = 1500 - 8;
        gc4023_attr.integration_time.min = 2;
        gc4023_attr.integration_time.apply_delay = 2;

        gc4023_attr.total_width = 1200;
        gc4023_attr.total_height = 1500;

        gc4023_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
        gc4023_attr.hcg.base_gain = ;
        gc4023_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        gc4023_attr.again_short.value = ;
        gc4023_attr.again_short.max = ;
        gc4023_attr.again_short.min = ;
        gc4023_attr.again_short.apply_delay = ;

        gc4023_attr.integration_time_short.value = ;
        gc4023_attr.integration_time_short.max = ;
        gc4023_attr.integration_time_short.min = ;
        gc4023_attr.integration_time_short.apply_delay = ;

        gc4023_attr.wdr_cache = wdr_line * gc4023_attr.total_width;

#ifdef SENSOR_HCG
        gc4023_attr.hcg_short.base_gain = ;
        gc4023_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(gc4023_attr.mipi)), (void *)(&gc4023_mipi_linear), sizeof(gc4023_attr.mipi));
        break;
    default:
        ISP_ERROR("Have no this Setting Source!!!\n");
        break;
    }

    return ret;
}

static int gc4023_attr_check(struct tx_isp_subdev *sd)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct clk *sclka;
    int ret = ISP_SUCCESS;

    gc4023_setting_select(sd, info->default_boot);

    switch (info->video_interface) {
    case TISP_SENSOR_VI_MIPI_CSI0:
        gc4023_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        gc4023_attr.mipi.index = 0;
        break;
    case TISP_SENSOR_VI_MIPI_CSI1:
        gc4023_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        gc4023_attr.mipi.index = 1;
        break;
    case TISP_SENSOR_VI_DVP:
        gc4023_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

    ret = gc4023_clk_set(sd, sclka, SENSOR_MCLK);
    if (ret) {
        ISP_ERROR("MCLK configuration failed!!!\n");
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
    gc4023_attr_set(sd, wsize);
    sensor->priv = wsize;

    return 0;

err_get_mclk:
    return -1;
}

static int gc4023_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
    unsigned char v;
    int ret;

    ret = gc4023_read(sd, 0x03f0, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_H)
        return -ENODEV;
    *ident = v;

    ret = gc4023_read(sd, 0x03f1, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_L)
        return -ENODEV;
    *ident = (*ident << 8) | v;

    return 0;
}

static int gc4023_g_chip_ident(struct tx_isp_subdev *sd,
                               struct tx_isp_chip_ident *chip)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    unsigned int ident = 0;
    int ret = ISP_SUCCESS;

    gc4023_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
    if (info->rst_gpio != -1) {
        ret = private_gpio_request(info->rst_gpio, "gc4023_reset");
        if (!ret) {
            private_gpio_direction_output(info->rst_gpio, 0);
            private_msleep(20);
            private_gpio_direction_output(info->rst_gpio, 1);
            private_msleep(10);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
        }
    }
    if (info->pwdn_gpio != -1) {
        ret = private_gpio_request(info->pwdn_gpio, "gc4023_pwdn");
        if (!ret) {
            private_gpio_direction_output(info->pwdn_gpio, 1);
            private_msleep(10);
            private_gpio_direction_output(info->pwdn_gpio, 0);
            private_msleep(10);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
        }
    }
    ret = gc4023_detect(sd, &ident);
    if (ret) {
        ISP_ERROR("chip found @ 0x%x (%s) is not an gc4023 chip.\n",
                  client->addr, client->adapter->name);
        return ret;
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    ISP_WARNING("===================================================\n");
    ISP_WARNING("Template version is %s\n", TVERSION);
    ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
    ISP_WARNING("Sensor name is %s\n", gc4023_attr.name);
    ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
    ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
    ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
    ISP_WARNING("===================================================\n");

    if (chip) {
        memcpy(chip->name, "gc4023", sizeof("gc4023"));
        chip->ident = ident;
        chip->revision = SENSOR_VERSION;
    }

    return 0;
}

static int gc4023_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    return 0;
}

static int gc4023_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
        ret = gc4023_attr_set(sd, wsize);
        sensor->video.state = TX_ISP_MODULE_DEINIT;
    }

    return ret;
}

static int gc4023_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
    ret = gc4023_read(sd, reg->reg & 0xffff, &val);
    reg->val = val;
    reg->size = 2;

    return ret;
}

static int gc4023_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
    int len = 0;

    len = strlen(sd->chip.name);
    if (len && strncmp(sd->chip.name, reg->name, len)) {
        return -EINVAL;
    }
    if (!private_capable(CAP_SYS_ADMIN))
        return -EPERM;
    gc4023_write(sd, reg->reg & 0xffff, reg->val & 0xff);

    return 0;
}

static int gc4023_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = ISP_SUCCESS;

    if (init->enable) {
        if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
            ret = gc4023_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
            if (ret)
                return ret;
            sensor->video.state = TX_ISP_MODULE_INIT;
        }
        if (sensor->video.state == TX_ISP_MODULE_INIT) {
            ret = gc4023_write_array(sd, gc4023_stream_on_mipi);
            sensor->video.state = TX_ISP_MODULE_RUNNING;
            pr_debug("gc4023 stream on\n");
        }

    } else {
        ret = gc4023_write_array(sd, gc4023_stream_off_mipi);
        sensor->video.state = TX_ISP_MODULE_INIT;
        pr_debug("gc4023 stream off\n");
    }

    return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc4023_set_expo(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;
    int it = value & 0xffff;
    int again = (value & 0xffff0000) >> 16;
    struct again_lut *val_lut = gc4023_again_lut;

    /*set integration time*/
    ret = gc4023_write(sd, 0x0203, it & 0xff);
    ret += gc4023_write(sd, 0x0202, it >> 8);

    /*set sensor analog gain*/
    ret = gc4023_write(sd, 0x0614, val_lut[again].reg614);
    ret = gc4023_write(sd, 0x0615, val_lut[again].reg615);
    ret = gc4023_write(sd, 0x0218, val_lut[again].reg218);
    ret = gc4023_write(sd, 0x1467, val_lut[again].reg1467);
    ret = gc4023_write(sd, 0x1468, val_lut[again].reg1468);
    ret = gc4023_write(sd, 0x00b8, val_lut[again].regb8);
    ret = gc4023_write(sd, 0x00b9, val_lut[again].regb9);

    return ret;
}
#else
static int gc4023_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    /*set integration time*/
    ret = gc5603_write(sd, 0x0203, value & 0xff);
    ret += gc5603_write(sd, 0x0202, value >> 8);

    return ret;
}

static int gc4023_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;
    struct again_lut *val_lut = gc4023_again_lut;


    /*set sensor analog gain*/
    ret = gc4023_write(sd, 0x0614, val_lut[value].reg614);
    ret = gc4023_write(sd, 0x0615, val_lut[value].reg615);
    ret = gc4023_write(sd, 0x0218, val_lut[value].reg218);
    ret = gc4023_write(sd, 0x1467, val_lut[value].reg1467);
    ret = gc4023_write(sd, 0x1468, val_lut[value].reg1468);
    ret = gc4023_write(sd, 0x00b8, val_lut[value].regb8);
    ret = gc4023_write(sd, 0x00b9, val_lut[value].regb9);

    return ret;
}
#endif /* SENSOR_EXPO */

static int gc4023_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int gc4023_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int gc4023_set_mode(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    if (wsize) {
        ret = gc4023_attr_set(sd, wsize);
    }

    return ret;
}

static int gc4023_set_fps(struct tx_isp_subdev *sd, int fps)
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
        sclk = 1200 * 1500 * 30;  /**< HTS * VTS * FPS */
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
    ret += gc4023_read(sd, 0x0342, &val);
    hts = (val & 0x0f) << 8;
    val = 0;
    ret += gc4023_read(sd, 0x0343, &val);
    hts  = (hts | val);
    if (0 != ret) {
        ISP_ERROR("err: gc4023 read err\n");
        return ret;
    }

    vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

    gc4023_write(sd, 0x0340, (unsigned char) ((vts >> 8) & 0x3f));
    gc4023_write(sd, 0x0341, (unsigned char) (vts & 0xff));

    gc4023_write(sd, 0x0259, (unsigned char)(((vts - 32) >> 8 ) & 0x3f));
    gc4023_write(sd, 0x025a, (unsigned char)((vts - 32) & 0xff));

    if (0 != ret) {
        ISP_ERROR("err: gc4023_write err\n");
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

static int gc4023_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
    struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    int ret = ISP_SUCCESS;
    unsigned char val = 0;
    unsigned char otp_val = 0x0;
    par->drop_frame = 0;
    par->reset = 0;

    /* 2'b01:mirror,2'b10:filp */
    ret = gc4023_read(sd, 0x022c, &val);
    switch(par->hvflip) {
    case TX_ISP_SENSOR_HVFLIP_NOMAL:
        ret = gc4023_write(sd, 0x022c, (val&0xfc) | 0x00); /*mirror*/
        break;
    case TX_ISP_SENSOR_HVFLIP_HFLIP:
        ret = gc4023_write(sd, 0x022c, (val&0xfc) | 0x01); /*mirror*/
        break;
    case TX_ISP_SENSOR_HVFLIP_VFLIP:
        ret = gc4023_write(sd, 0x022c, (val&0xfc) | 0x02); /*filp*/
        break;
    case TX_ISP_SENSOR_HVFLIP_HVFLIP:
        ret = gc4023_write(sd, 0x022c, (val&0xfc) | 0x03); /*mirror & filp*/
        break;
    }
    otp_val=0x60|val;
    ret += gc4023_write(sd,0x0a67, 0x80 );
    ret += gc4023_write(sd,0x0a54, 0x0e );
    ret += gc4023_write(sd,0x0a65, 0x10 );
    ret += gc4023_write(sd,0x0a98, 0x10 );
    ret += gc4023_write(sd,0x05be, 0x00 );
    ret += gc4023_write(sd,0x05a9, 0x01 );
    ret += gc4023_write(sd,0x0029, 0x08 );
    ret += gc4023_write(sd,0x002b, 0xa8 );
    ret += gc4023_write(sd,0x0a83, 0xe0 );
    ret += gc4023_write(sd,0x0a72, 0x02 );
    ret += gc4023_write(sd,0x0a73, otp_val );
    ret += gc4023_write(sd,0x0a75, 0x41 );
    ret += gc4023_write(sd,0x0a70, 0x03 );
    ret += gc4023_write(sd,0x0a5a, 0x80 );
    private_msleep(20);
    ret += gc4023_write(sd,0x05be, 0x01 );
    ret += gc4023_write(sd,0x0a70, 0x00 );
    ret += gc4023_write(sd,0x0080, 0x02 );
    ret += gc4023_write(sd,0x0a67, 0x00 );
    sensor->video.hvflip_mode = par->hvflip;
    gc4023_attr_set(sd, wsize);

    return ret;
#else
    return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int gc4023_set_expo_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#else
static int gc4023_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}

static int gc4023_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#endif /* SENSOR_EXPO */

static int gc4023_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    int ret = ISP_SUCCESS;

    ret = gc4023_write_array(sd, gc4023_stream_off_mipi);
    if (par->wdr_en == 1) {
        if (par->boot == -1) {
            info->default_boot = 1;
        }
        gc4023_setting_select(sd, 1);
        gc4023_attr_set(sd, wsize);
    } else if (par->wdr_en == 0) {
        if (par->boot == -1) {
            info->default_boot = 0;
        }
        gc4023_setting_select(sd, 0);
        gc4023_attr_set(sd, wsize);
    } else {
        ISP_ERROR("Can not support this data type!!!");
        return -1;
    }

    return 0;
}

static int gc4023_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    struct tx_isp_sensor_register_info *info = &sensor->info;
    int ret = ISP_SUCCESS;

    private_gpio_direction_output(info->rst_gpio, 0);
    private_msleep(1);
    private_gpio_direction_output(info->rst_gpio, 1);
    private_msleep(1);

    ret = gc4023_write_array(sd, wsize->regs);
    ret = gc4023_write_array(sd, gc4023_stream_on_mipi);

    return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc4023_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
            ret = gc4023_set_expo(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME:
        if (arg)
            ret = gc4023_set_integration_time(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN:
        if (arg)
            ret = gc4023_set_analog_gain(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_DGAIN:
        if (arg)
            ret = gc4023_set_digital_gain(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
        if (arg)
            ret = gc4023_get_black_pedestal(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_RESIZE:
        if (arg)
            ret = gc4023_set_mode(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
        if (arg)
            ret = gc4023_write_array(sd, gc4023_stream_off_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
        if (arg)
            ret = gc4023_write_array(sd, gc4023_stream_on_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FPS:
        if (arg)
            ret = gc4023_set_fps(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_HVFLIP:
        if(arg)
            ret = gc4023_set_hvflip(sd, (void *)sensor_val->value);
        break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
    case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
        if (arg)
            ret = gc4023_set_expo_short(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
        if(arg)
            ret = gc4023_set_integration_time_short(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
        if(arg)
            ret = gc4023_set_analog_gain_short(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_WDR:
        if(arg)
            ret = gc4023_set_wdr(sd, (void *)sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_WDR_STOP:
        if(arg)
            ret = gc4023_set_wdr_stop(sd, (void *)sensor_val->value);
        break;
#endif /* SENSOR_WDR_2_FRAME */
    default:
        break;
    }

    return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc4023_core_ops = {
    .g_chip_ident = gc4023_g_chip_ident,
    .reset = gc4023_reset,
    .init = gc4023_init,
    .g_register = gc4023_g_register,
    .s_register = gc4023_s_register,
};

static struct tx_isp_subdev_video_ops gc4023_video_ops = {
    .s_stream = gc4023_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc4023_sensor_ops = {
#ifndef SENSOR_TEST
    .ioctl  = gc4023_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc4023_ops = {
    .core = &gc4023_core_ops,
    .video = &gc4023_video_ops,
    .sensor = &gc4023_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
    .name = "gc4023",
    .id = -1,
    .dev = {
        .dma_mask = &tx_isp_module_dma_mask,
        .coherent_dma_mask = 0xffffffff,
        .platform_data = NULL,
    },
    .num_resources = 0,
};

static int gc4023_probe(struct i2c_client *client,
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
    sensor->video.attr = &gc4023_attr;
    sensor->dev = &client->dev;
    tx_isp_subdev_init(&sensor_platform_device, sd, &gc4023_ops);
    tx_isp_set_subdevdata(sd, client);
    tx_isp_set_subdev_hostdata(sd, sensor);
    private_i2c_set_clientdata(client, sd);

    pr_debug("probe ok ------->gc4023\n");

    return 0;
}

static int gc4023_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc4023_id[] = {
    {"gc4023", 0},
    {}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc4023_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver gc4023_driver = {
    .driver = {
#ifdef CONFIG_ZERATUL
        .owner  = NULL,
#else
        .owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
        .name   = "gc4023",
    },
    .probe          = gc4023_probe,
    .remove         = gc4023_remove,
    .id_table       = gc4023_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc4023(void) {
    return private_i2c_add_driver(&gc4023_driver);
}

static __exit void exit_gc4023(void) {
    private_i2c_del_driver(&gc4023_driver);
}

module_init(init_gc4023);
module_exit(exit_gc4023);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc4023(void) {
    return private_i2c_add_driver(&gc4023_driver);
}

static void exit_first_gc4023(void) {
    private_i2c_del_driver(&gc4023_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
    .name = "gc4023",
    .i2c_addr = 0x29,
    .width = 2560,
    .height = 1440,
#ifdef SENSOR_WDR_2_FRAME
    .wdr = 1,
#else
    .wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
    .init_sensor = init_first_gc4023,
    .exit_sensor = exit_first_gc4023
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for gc4023 sensor");
MODULE_LICENSE("GPL");
