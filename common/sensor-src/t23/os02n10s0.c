/*
 * os02n10.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @fsync Sync hardware connection: Primary Sensor EVSYNC is directly connected to secondary Sensor EVSYNC.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920x1080       15       MIPI_2lane             linear
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

#define TVERSION "V20231121a"
#define SENSOR_VERSION  "H20240219a"

//#define SENSOR_TEST

/* 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_I2C_REG_8BIT

/* 选择Sensor AGain匹配方式(AGain表/非AGain表) */
#define SENSOR_AGAIN_TABLE

#define SENSOR_EXPO

/* 镜像翻转功能开关 */
//#define SENSOR_MIR_FLIP

static int rst_gpio = GPIO_PA(18);
module_param(rst_gpio, int, S_IRUGO);
MODULE_PARM_DESC(rst_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int default_boot = 0;
module_param(default_boot, int, S_IRUGO);
MODULE_PARM_DESC(default_boot, "Sensor default boot");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int fsync_mode = 3;
module_param(fsync_mode, int, S_IRUGO);
MODULE_PARM_DESC(fsync_mode, "Sensor Indicates the frame synchronization mode");

#define SENSOR_CHIP_ID_HH    (0x53)
#define SENSOR_CHIP_ID_HL    (0x02)
#define SENSOR_CHIP_ID_LH    (0x4e)
#define SENSOR_CHIP_ID_LL    (0x10)
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_MCLK 27000000

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

struct tx_isp_sensor_attribute os02n10_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
        unsigned int value;
        unsigned int gain;
};

struct again_lut os02n10_again_lut[] = {
        {0x10, 0},
        {0x11, 5731},
        {0x12, 11135},
        {0x13, 16247},
        {0x14, 21097},
        {0x15, 25710},
        {0x16, 30108},
        {0x17, 34311},
        {0x18, 38335},
        {0x19, 42195},
        {0x1a, 45903},
        {0x1b, 49472},
        {0x1c, 52910},
        {0x1d, 56228},
        {0x1e, 59433},
        {0x1f, 62534},
        {0x20, 65536},
        {0x22, 71267},
        {0x24, 76671},
        {0x26, 81783},
        {0x28, 86633},
        {0x2a, 91246},
        {0x2c, 95644},
        {0x2e, 99847},
        {0x30, 103871},
        {0x32, 107731},
        {0x34, 111439},
        {0x36, 115008},
        {0x38, 118446},
        {0x3a, 121764},
        {0x3c, 124969},
        {0x3e, 128070},
        {0x40, 131072},
        {0x44, 136803},
        {0x48, 142207},
        {0x4c, 147319},
        {0x50, 152169},
        {0x54, 156782},
        {0x58, 161180},
        {0x5c, 165383},
        {0x60, 169407},
        {0x64, 173267},
        {0x68, 176975},
        {0x6c, 180544},
        {0x70, 183982},
        {0x74, 187300},
        {0x78, 190505},
        {0x7c, 193606},
        {0x80, 196608},
        {0x88, 202339},
        {0x90, 207743},
        {0x98, 212855},
        {0xa0, 217705},
        {0xa8, 222318},
        {0xb0, 226716},
        {0xb8, 230919},
        {0xc0, 234943},
        {0xc8, 238803},
        {0xd0, 242511},
        {0xd8, 246080},
        {0xe0, 249518},
        {0xe8, 252836},
        {0xf0, 256041},
        {0xf8, 259142},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int os02n10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
        /* Analog gain table */
        struct again_lut *lut = os02n10_again_lut;
        while (lut->gain <= os02n10_attr.max_again) {
                if (isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if (isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else {
                        if ((lut->gain == os02n10_attr.max_again) && (isp_gain >= lut->gain)) {
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

unsigned int os02n10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
        return 0;
}

struct tx_isp_mipi_bus os02n10_mipi_linear = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 783,
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

struct tx_isp_dvp_bus os02n10_dvp = {
        .gpio = DVP_PA_LOW_10BIT,
        .mode = SENSOR_DVP_HREF_MODE,
        .blanking = {
                .hblanking = 0,
                .vblanking = 0,
        },
        .polar = {
                .hsync_polar = 0,
                .vsync_polar = 0,
                .pclk_polar = 0, /**< reserved */
        },
        .dvp_hcomp_en = 0,
};

struct tx_isp_sensor_attribute os02n10_attr = {
        .name = "os02n10",
        .chip_id = 0x53024e10,
        .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
        .cbus_device = 0x3c,
        .sensor_ctrl.alloc_again = os02n10_alloc_again,
        .sensor_ctrl.alloc_dgain = os02n10_alloc_dgain,
	.fsync_attr = {
		.mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE,
		.call_times = 1,
		.sdelay = 100,
	}
};

static struct regval_list os02n10_init_regs_1920_1080_15fps_mipi[] = {
        {0xfc, 0x01},
        {0xfd, 0x00},
        {0xba, 0x02},
        {0xfd, 0x00},
        {0xb1, 0x14},
        {0xba, 0x00},
        {0x1a, 0x00},
        {0xfd, 0x01},
        {0x0e, 0x01},
        {0x0f, 0x00},
        {0x24, 0x10},
        {0x14, 0x04},
        {0x15, 0x76},
        {0x2f, 0x30},
        {0xfe, 0x02},
        {0x2b, 0xff},
        {0x30, 0x00},
        {0x31, 0x16},
        {0x32, 0x25},
        {0x33, 0xfb},
        {0xfd, 0x01},
        {0x50, 0x03},
        {0x51, 0x07},
        {0x52, 0x04},
        {0x53, 0x05},
        {0x57, 0x52},
        {0x66, 0x04},
        {0x6d, 0x58},
        {0x77, 0x01},
        {0x79, 0x44},
        {0x7c, 0x01},
        {0x90, 0x3b},
        {0x91, 0x0b},
        {0x92, 0x18},
        {0x95, 0x40},
        {0x99, 0x05},
        {0xaa, 0x0e},
        {0xab, 0x0c},
        {0xac, 0x10},
        {0xad, 0x10},
        {0xae, 0x20},
        {0xb0, 0x0e},
        {0xb1, 0x0f},
        {0xb2, 0x1a},
        {0xb3, 0x1c},
        {0xfd, 0x00},
        {0xb0, 0x00},
        {0xb1, 0x14},
        {0xb2, 0x00},
        {0xb3, 0x10},
        {0xfd, 0x03},
        {0x08, 0x00},
        {0x09, 0x20},
        {0x0a, 0x02},
        {0x0b, 0x80},
        {0x11, 0x41},
        {0x12, 0x41},
        {0x13, 0x41},
        {0x14, 0x41},
        {0x17, 0x68},
        {0x18, 0x68},
        {0x19, 0x68},
        {0x1a, 0x68},
        {0x1b, 0xc0},
        {0x1d, 0x01},
        {0x1f, 0x80},
        {0x20, 0x40},
        {0x21, 0x80},
        {0x22, 0x40},
        {0x23, 0x88},
        {0x4b, 0x06},
        {0x58, 0x7b},
        {0x59, 0x17},
        {0x5a, 0x32},
        {0xfd, 0x00},
        {0x08, 0x1d},
        {0x09, 0x00},
        {0xfd, 0x00},
        {0x13, 0xbe},
        {0x14, 0x02},
        {0x4c, 0x24},
        {0xb6, 0x00},
        {0xb7, 0x08},
        {0xb9, 0xd6},
        {0xc6, 0xa5},
        {0xc7, 0x77},
        {0xc9, 0x22},
        {0xca, 0x52},
        {0xd7, 0xaa},
        {0xbc, 0x1f},
        {0xbd, 0x60},
        {0xbe, 0x78},
        {0xbf, 0xa5},
        {0xcb, 0x00},
        {0xcc, 0x00},
        {0xce, 0x20},
        {0xcf, 0x3f},
        {0xd0, 0x76},
        {0xd1, 0xec},
        {0xfd, 0x04},
        {0x1b, 0x01},
        {0xfd, 0x03},
        {0x01, 0x04},
        {0x02, 0x07},
        {0x03, 0x80},
        {0x05, 0x04},
        {0x06, 0x04},
        {0x07, 0x38},
        {0xfd, 0x00},
        {0x1e, 0x0f},
        {0x1d, 0xa1},
        {0x21, 0x04},
        {0x24, 0x02},
        {0x27, 0x07},
        {0x28, 0x80},
        {0x29, 0x04},
        {0x2a, 0x38},
        {0x2d, 0x04},
        {0x2e, 0x03},
        {0x2f, 0x0c},
        {0x31, 0x04},
        {0x32, 0x1a},
        {0x33, 0x04},
        {0x34, 0x05},
        {0x3f, 0x40},
        {0x40, 0xf3},
        {0xfd, 0x00},
        {0x1a, 0x12},
        {0x52, 0x01},
        {0xe7, 0x01},
        {0xfd, 0x01},
        {0xd0, 0x80},
        {0xd3, 0x01},
        /* {0xd6, 0x04}, */
        /* {0xd7, 0x00}, */
        {0xd8, 0x00},
        {0xd9, 0x01},
        {0xda, 0x01},
        {0xdb, 0x00},
        {0xfd, 0x00},
        {0xd0, 0x76},
        {0xd3, 0x04},
        {0xd4, 0x04},
        {0xd5, 0x04},
        {0xd6, 0xaa},
        {0xd7, 0xaa},
        {0xd8, 0x00},
        {0xd9, 0x00},
        {0xda, 0x00},
        {0xdb, 0x00},
        {0xfd, 0x00},
        {0x23, 0x01},
        {0xfb, 0x03},
        {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the os02n10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os02n10_win_sizes[] = {
        /* 1920*1080 [0] */
        {
                .width          = 1920,
                .height         = 1080,
                .fps            = 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
                .regs           = os02n10_init_regs_1920_1080_15fps_mipi,
        },
};

static struct tx_isp_sensor_win_setting *wsize = &os02n10_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list os02n10_stream_on_mipi[] = {
        {0xfd, 0x00},
        {0x23, 0x01},
        {SENSOR_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list os02n10_stream_off_mipi[] = {
        {0xfd, 0x00},
        {0x23, 0x00},
        {SENSOR_REG_END, 0x00}, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int os02n10_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int os02n10_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int os02n10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os02n10_read(sd, vals->reg_num, &val);
                        /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif

static int os02n10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os02n10_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int os02n10_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int os02n10_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int os02n10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os02n10_read(sd, vals->reg_num, &val);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }
        return 0;
}
#endif

static int os02n10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os02n10_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sensor_mclk_config(struct tx_isp_sensor *sensor, unsigned long want_rate)
{
        unsigned long rate = 0;
        struct clk *pll = NULL;
        char *plls[] = {"mpll", "sclka"};
        int psize = sizeof(plls) / sizeof(char *);
        char *ppll = plls[psize - 1];
        int ret = 0, i = 0;

        pll = clk_get_parent(sensor->mclk);
        rate = clk_get_rate(pll);
        if (rate % want_rate) {
                for (i = 0; i < psize; i++) {
                        pll = clk_get(NULL, plls[i]);
                        rate = clk_get_rate(pll);
                        if (!(rate % want_rate)) {
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, plls[i]);
                                        continue;
                                } else {
                                        break;
                                }
                        }
                }
                if (i == psize) {
                        if (!ret) {
                                pll = clk_get(NULL, ppll);
                                rate = clk_get_rate(pll);
                                if(want_rate == 37125000){
                                        if((rate >= 1188000000)) {
                                                rate = 1188000000;
                                        } else if (rate >= 891000000) {
                                                rate = 891000000;
                                        } else {
                                                ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
                                                          __func__, __LINE__, ppll);
                                                ret = -1;
                                                goto error;
                                        }
                                } else if (want_rate == 24000000 || want_rate == 27000000) {
                                        rate -= rate % want_rate;
                                } else {
                                        ret = -1;
                                        goto error;
                                }
                                ret = private_clk_set_rate(pll, rate);
                                if (ret) {
                                        ISP_WARNING("[%s %d] Failed to set %s !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                } else {
                                        ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld !!!\n",
                                                    __func__, __LINE__, ppll, rate);
                                }
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                }
                        } else {
                                goto error;
                        }
                }
        }
        private_clk_set_rate(sensor->mclk, want_rate);
        private_clk_enable(sensor->mclk);

        rate = clk_get_rate(sensor->mclk);
        if (rate % want_rate) {
                ret = -1;
                goto error;
        }

        return ret;

error:
        ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
                  __func__, __LINE__, want_rate);
        return ret;
}

static int os02n10_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        sensor->video.vi_max_width = wsize->width;
        sensor->video.vi_max_height = wsize->height;

        sensor->video.mbus.width = wsize->width;
        sensor->video.mbus.height = wsize->height;
        sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
        sensor->video.mbus.colorspace = wsize->colorspace;

        sensor->video.fps = wsize->fps;

#ifdef SENSOR_MIR_FLIP
	sensor->video.shvflip = 1;
#endif

        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

        return ret;
}

static int os02n10_setting_select(struct tx_isp_subdev *sd, int deboot)
{
        int ret = ISP_SUCCESS;

        switch (deboot) {
        case 0:
                wsize = &os02n10_win_sizes[0];
                os02n10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                os02n10_attr.max_dgain = 0;
                os02n10_attr.max_again = 265053;
                os02n10_attr.min_integration_time = 2;
                os02n10_attr.max_integration_time = 1142 - 9;
                os02n10_attr.total_width = 290;
                os02n10_attr.total_height = 1142;
                os02n10_attr.integration_time_apply_delay = 2;
                os02n10_attr.again_apply_delay = 2;
                os02n10_attr.dgain_apply_delay = 0;
                os02n10_attr.integration_time_limit = os02n10_attr.max_integration_time;
                os02n10_attr.max_integration_time_native = os02n10_attr.max_integration_time;
                os02n10_attr.min_integration_time_native = os02n10_attr.min_integration_time;
                os02n10_attr.expo_fs = 1;
                os02n10_attr.fsync_attr.mode = fsync_mode;
                memcpy((void *)(&(os02n10_attr.mipi)), (void *)(&os02n10_mipi_linear), sizeof(os02n10_attr.mipi));
                break;
        default:
                ISP_ERROR("Have no this Setting Source!!!\n");
        }

        return ret;
}

static int os02n10_attr_check(struct tx_isp_subdev *sd)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        os02n10_setting_select(sd, default_boot);

        switch (data_interface) {
        case TX_SENSOR_DATA_INTERFACE_MIPI:
        case TX_SENSOR_DATA_INTERFACE_DVP:
                os02n10_attr.dbus_type = data_interface;
                break;
        default:
                ISP_ERROR("Have no this interface!!!\n");
        }

#ifdef CONFIG_KERNEL_4_4_94
	sensor->mclk = clk_get(NULL, "div_cim");
#else
	sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
    sensor_mclk_config(sensor, SENSOR_MCLK);
        if (ret) {
                ISP_ERROR("MCLK configuration failed!!!\n");
        }

        os02n10_attr_set(sd, wsize);
        sensor->priv = wsize;

        return 0;

err_get_mclk:
        return -1;
}

static int os02n10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
        unsigned char v;
        int ret;

        os02n10_write(sd, 0xfd, 0x00);
        ret = os02n10_read(sd, 0x02, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_HH)
                return -ENODEV;
        *ident = v;

        ret = os02n10_read(sd, 0x03, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_HL)
                return -ENODEV;
        *ident = (*ident << 8) | v;

        ret = os02n10_read(sd, 0x04, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_LH)
                return -ENODEV;
        *ident = (*ident << 16) | v;

        ret = os02n10_read(sd, 0x05, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_LL)
                return -ENODEV;
        *ident = (*ident << 24) | v;

        return 0;
}

static int os02n10_g_chip_ident(struct tx_isp_subdev *sd,
                                 struct tx_isp_chip_ident *chip)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        unsigned int ident = 0;
        int ret = ISP_SUCCESS;

        if (rst_gpio != -1) {
                ret = private_gpio_request(rst_gpio, "os02n10_reset");
                if (!ret) {
                        private_gpio_direction_output(rst_gpio, 0);
                        private_msleep(100);
                        private_gpio_direction_output(rst_gpio, 1);
                        private_msleep(100);
                } else {
                        ISP_ERROR("gpio requrest fail %d\n", rst_gpio);
                }
        }
        if (pwdn_gpio != -1) {
                ret = private_gpio_request(pwdn_gpio, "os02n10_pwdn");
                if (!ret) {
                        private_gpio_direction_output(pwdn_gpio, 1);
                        private_msleep(10);
                        private_gpio_direction_output(pwdn_gpio, 0);
                        private_msleep(10);
                } else {
                        ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
                }
        }
        ret = os02n10_detect(sd, &ident);
        if (ret) {
                ISP_ERROR("chip found @ 0x%x (%s) is not an os02n10 chip.\n",
                          client->addr, client->adapter->name);
                return ret;
        }

        ISP_WARNING("===================================================\n");
        ISP_WARNING("Template version is %s\n", TVERSION);
        ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
        ISP_WARNING("Sensor name is %s\n", os02n10_attr.name);
        ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
        ISP_WARNING("Sensor video interface is %d\n", data_interface);
        ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                    default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
        ISP_WARNING("===================================================\n");

        if (chip) {
                memcpy(chip->name, "os02n10", sizeof("os02n10"));
                chip->ident = ident;
                chip->revision = SENSOR_VERSION;
        }

        return 0;
}

static int os02n10_reset(struct tx_isp_subdev *sd, int val)
{
        return 0;
}

static int os02n10_init(struct tx_isp_subdev *sd, int enable)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if (!enable) {
                sensor->video.state = TX_ISP_MODULE_DEINIT;
                return ISP_SUCCESS;
        } else {
                ret = os02n10_attr_set(sd, wsize);
                ret = os02n10_write_array(sd, wsize->regs);
                if (ret)
                        return ret;
                sensor->video.state = TX_ISP_MODULE_DEINIT;
        }

        return ret;
}

static int os02n10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
        ret = os02n10_read(sd, reg->reg & 0xffff, &val);
        reg->val = val;
        reg->size = 2;

        return ret;
}

static int os02n10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
        int len = 0;

        len = strlen(sd->chip.name);
        if (len && strncmp(sd->chip.name, reg->name, len)) {
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        os02n10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

        return 0;
}

static int os02n10_s_stream(struct tx_isp_subdev *sd, int enable)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if (enable) {
                if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
                        sensor->video.state = TX_ISP_MODULE_INIT;
                }
                if (sensor->video.state == TX_ISP_MODULE_INIT) {
                        ret = os02n10_write_array(sd, os02n10_stream_on_mipi);
                        sensor->video.state = TX_ISP_MODULE_RUNNING;
                        pr_debug("os02n10 stream on\n");
                }

        } else {
                ret = os02n10_write_array(sd, os02n10_stream_off_mipi);
                sensor->video.state = TX_ISP_MODULE_INIT;
                pr_debug("os02n10 stream off\n");
        }

        return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int os02n10_set_expo(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

        os02n10_write(sd, 0xfd, 0x01);

        os02n10_write(sd, 0x0e, it >> 8);
        os02n10_write(sd, 0x0f, it & 0xff);

        os02n10_write(sd, 0x24, again & 0xff);
        os02n10_write(sd, 0xfe, 0x02);

        return ret;
}
#else
static int os02n10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

        value &= 0xffff;
        os02n10_write(sd, 0xfd, 0x01);
        os02n10_write(sd, 0x0e, value >> 8);
        os02n10_write(sd, 0x0f, value & 0xff);
        os02n10_write(sd, 0xfe, 0x02);

        return ret;
}

static int os02n10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

        os02n10_write(sd, 0xfd, 0x01);
        os02n10_write(sd, 0x24, value & 0xff);
        os02n10_write(sd, 0xfe, 0x02);

        return ret;
}
#endif /* SENSOR_EXPO */

static int os02n10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int os02n10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int os02n10_set_mode(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

        if (wsize) {
                ret = os02n10_attr_set(sd, wsize);
        }

        return ret;
}

static int os02n10_set_fps(struct tx_isp_subdev *sd, int fps)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        unsigned int sclk = 0;
        unsigned int hts = 0;
        unsigned int vts = 0;
        unsigned char val = 0;
        unsigned int newformat = 0; //the format is 24.8
        unsigned int max_fps = 0;
        int ret = ISP_SUCCESS;

        switch (default_boot) {
        case 0:
                sclk = 290 * 1142 * 15;  /**< HTS * VTS * FPS */
                break;
        default:
                ret = -1;
                ISP_ERROR("Now we do not support this framerate!!!\n");
        }

        max_fps = wsize->fps;
        newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
        if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
                ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
                return -1;
        }

        os02n10_write(sd, 0xfd, 0x01);
        val = 0;
        ret += os02n10_read(sd, 0x25, &val);
        hts = val << 8;
        val = 0;
        ret += os02n10_read(sd, 0x26, &val);
        hts |= val;
        if (0 != ret) {
                ISP_ERROR("err: os02n10 read err\n");
                return ret;
        }

        vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

        os02n10_write(sd, 0x15, (unsigned char) (vts & 0xff));
        os02n10_write(sd, 0x14, (unsigned char) (vts >> 8));
        os02n10_write(sd, 0xfe, 0x02);

        if (0 != ret) {
                ISP_ERROR("err: os02n10_write err\n");
                return ret;
        }

        sensor->video.fps = fps;
        sensor->video.attr->total_height = vts;
        sensor->video.attr->max_integration_time = vts - 9;
        sensor->video.attr->integration_time_limit = vts - 9;
        sensor->video.attr->max_integration_time_native = vts - 9;
        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        if (ret) {
                ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
        }

        return ret;
}

#ifdef SENSOR_MIR_FLIP
static int os02n10_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = ISP_SUCCESS;

	/* 2'b01:mirror,2'b10:filp */
	switch(enable) {
	case 0:
                ...;
		break;
	case 1:
                ...;
		break;
	case 2:
                ...;
		break;
	case 3:
                ...;
		break;
	}

	return ret;
}
#endif /* SENSOR_MIR_FLIP */

static int os02n10_fsync(struct tx_isp_subdev *sd, struct tx_isp_sensor_fsync *fsync)
{
        if (fsync->place != TX_ISP_SENSOR_FSYNC_PLACE_STREAMON_AFTER)
                return 0;
        switch (fsync->call_index) {
        case 0:
                switch (fsync_mode) {
                case 2:
                        os02n10_write(sd, 0xfd, 0x01);
                        os02n10_write(sd, 0xd6, 0x00);
                        os02n10_write(sd, 0xd7, 0x00);
                        break;
                case 3:
                        os02n10_write(sd, 0xfd, 0x01);
                        os02n10_write(sd, 0xd6, 0x04);
                        os02n10_write(sd, 0xd7, 0x70);
                        break;
                }
                break;
        }

        return 0;
}

static int os02n10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
        long ret = 0;

        if (IS_ERR_OR_NULL(sd)) {
                ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
                return -EINVAL;
        }

        switch (cmd) {
#ifdef SENSOR_EXPO
        case TX_ISP_EVENT_SENSOR_EXPO:
                if (arg)
                        ret = os02n10_set_expo(sd, *(int*)arg);
                break;
#else
        case TX_ISP_EVENT_SENSOR_INT_TIME:
                if (arg)
                        ret = os02n10_set_integration_time(sd, *(int*)arg);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN:
                if (arg)
                        ret = os02n10_set_analog_gain(sd, *(int*)arg);
                break;
#endif /* SENSOR_EXPO */
        case TX_ISP_EVENT_SENSOR_DGAIN:
                if (arg)
                        ret = os02n10_set_digital_gain(sd, *(int*)arg);
                break;
        case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
                if (arg)
                        ret = os02n10_get_black_pedestal(sd, *(int*)arg);
                break;
        case TX_ISP_EVENT_SENSOR_RESIZE:
                if (arg)
                        ret = os02n10_set_mode(sd, *(int*)arg);
                break;
        case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
                if (arg)
                        ret = os02n10_write_array(sd, os02n10_stream_off_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
                if (arg)
                        ret = os02n10_write_array(sd, os02n10_stream_on_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FPS:
                if (arg)
                        ret = os02n10_set_fps(sd, *(int*)arg);
                break;
#ifdef SENSOR_MIR_FLIP
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = os02n10_set_vflip(sd, *(int*)arg);
		break;
#endif /* SENSOR_MIR_FLIP */
        default:
                break;
        }

        return 0;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops os02n10_core_ops = {
        .g_chip_ident = os02n10_g_chip_ident,
        .reset = os02n10_reset,
        .init = os02n10_init,
        .g_register = os02n10_g_register,
        .s_register = os02n10_s_register,
};

static struct tx_isp_subdev_video_ops os02n10_video_ops = {
        .s_stream = os02n10_s_stream,
};

static struct tx_isp_subdev_sensor_ops os02n10_sensor_ops = {
#ifndef SENSOR_TEST
        .ioctl  = os02n10_sensor_ops_ioctl,
        .fsync = os02n10_fsync,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops os02n10_ops = {
        .core = &os02n10_core_ops,
        .video = &os02n10_video_ops,
        .sensor = &os02n10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
        .name = "os02n10",
        .id = -1,
        .dev = {
                .dma_mask = &tx_isp_module_dma_mask,
                .coherent_dma_mask = 0xffffffff,
                .platform_data = NULL,
        },
        .num_resources = 0,
};

static int os02n10_probe(struct i2c_client *client,
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

        os02n10_attr_check(sd);

        sensor->video.attr = &os02n10_attr;
        tx_isp_subdev_init(&sensor_platform_device, sd, &os02n10_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->os02n10\n");

        return 0;
}

static int os02n10_remove(struct i2c_client *client)
{
        struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
        struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(rst_gpio != -1)
		private_gpio_free(rst_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);

        kfree(sensor);

        return 0;
}

static const struct i2c_device_id os02n10_id[] = {
        {"os02n10", 0},
        {}
};
MODULE_DEVICE_TABLE(i2c, os02n10_id
                   );

static struct i2c_driver os02n10_driver = {
        .driver = {
                .owner  = THIS_MODULE,
                .name   = "os02n10",
        },
        .probe          = os02n10_probe,
        .remove         = os02n10_remove,
        .id_table       = os02n10_id,
};

static __init int init_os02n10(void) {
        return private_i2c_add_driver(&os02n10_driver);
}

static __exit void exit_os02n10(void) {
        private_i2c_del_driver(&os02n10_driver);
}

module_init(init_os02n10);
module_exit(exit_os02n10);

MODULE_DESCRIPTION("A low-level driver for OV os02n10 sensor");
MODULE_LICENSE("GPL");
