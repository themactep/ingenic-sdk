/*
 * gc2607.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution       fps     interface              mode
 *   0          1920*1080        30     mipi_2lane             linear
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

#define GC2607_CHIP_ID_H        (0x26)
#define GC2607_CHIP_ID_L        (0x07)
#define GC2607_REG_END          0xff
#define GC2607_REG_DELAY        0x00
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION  "H20220818a"

static int reset_gpio = GPIO_PC(27);
static int pwdn_gpio = -1;
static int shvflip = 0;

struct regval_list {
        unsigned int reg_num;
        unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
        int index;
        unsigned int reg2b3;
        unsigned int reg2b4;
        unsigned int reg20c;
        unsigned int reg20d;
        unsigned int gain;
};

struct again_lut gc2607_again_lut[] = {
        //0x02b3 0x02b4 0x020c 0x020d
        {0x00, 0x00, 0x00, 0x00, 0x40, 0},                //1.000000
        {0x01, 0x05, 0x00, 0x00, 0x4b, 16247},            //1.187500
        {0x02, 0x00, 0x01, 0x00, 0x59, 35333},            //1.453125
        {0x03, 0x05, 0x01, 0x00, 0x6a, 52062},            //1.734375
        {0x04, 0x00, 0x02, 0x00, 0x80, 67001},            //2.031250
        {0x05, 0x05, 0x02, 0x00, 0x97, 84239},            //2.437500
        {0x06, 0x00, 0x03, 0x00, 0xb3, 99847},            //2.875000
        {0x07, 0x05, 0x03, 0x00, 0xd4, 117171},            //3.453125
        {0x08, 0x00, 0x04, 0x01, 0x00, 129957},            //3.953125
        {0x09, 0x05, 0x04, 0x01, 0x2f, 147319},            //4.750000
        {0x0a, 0x00, 0x05, 0x01, 0x66, 165125},            //5.734375
        {0x0b, 0x05, 0x05, 0x01, 0xa8, 180980},            //6.781250
        {0x0c, 0x00, 0x06, 0x02, 0x00, 196237},            //7.968750
        {0x0d, 0x05, 0x06, 0x02, 0x5e, 212700},            //9.484375
        {0x0e, 0x09, 0x26, 0x02, 0xcc, 228446},            //11.203125
        {0x0f, 0x0c, 0xb6, 0x03, 0x50, 244200},            //13.234375
        {0x10, 0x10, 0x06, 0x04, 0x00, 261029},            //15.812500

};

struct tx_isp_sensor_attribute gc2607_attr;

unsigned int gc2607_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        struct again_lut *lut = gc2607_again_lut;

        while(lut->gain <= gc2607_attr.max_again) {
                if(isp_gain == 0) {
                        *sensor_again = lut[0].index;
                        return lut[0].gain;
                } else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->index;
                        return (lut - 1)->gain;
                } else {
                        if((lut->gain == gc2607_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->index;
                                return lut->gain;
                        }
                }

                lut++;
        }

        return isp_gain;
}

unsigned int gc2607_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
        return 0;
}

struct tx_isp_mipi_bus gc2607_mipi={
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 672,
        .lans = 2,
        .settle_time_apative_en = 0,
        .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
        .mipi_sc.hcrop_diff_en = 0,
        .mipi_sc.mipi_vcomp_en = 0,
        .mipi_sc.mipi_hcomp_en = 0,
        .mipi_sc.line_sync_mode = 0,
        .mipi_sc.work_start_flag = 0,
        .image_twidth = 1920,
        .image_theight = 1080,
        .mipi_sc.mipi_crop_start0x = 0,
        .mipi_sc.mipi_crop_start0y = 0,
        .mipi_sc.mipi_crop_start1x = 0,
        .mipi_sc.mipi_crop_start1y = 0,
        .mipi_sc.mipi_crop_start2x = 0,
        .mipi_sc.mipi_crop_start2y = 0,
        .mipi_sc.mipi_crop_start3x = 0,
        .mipi_sc.mipi_crop_start3y = 0,
        .mipi_sc.data_type_en = 0,
        .mipi_sc.data_type_value = RAW10,
        .mipi_sc.del_start = 0,
        .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
        .mipi_sc.sensor_fid_mode = 0,
        .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute gc2607_attr={
        .name = "gc2607",
        .chip_id = 0x2607,
        .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
        .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
        .cbus_device = 0x37,
        .dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
        .max_again = 261029,
        .max_dgain = 0,
        .min_integration_time = 2,
        .min_integration_time_native = 2,
        .integration_time_apply_delay = 2,
        .again_apply_delay = 2,
        .dgain_apply_delay = 0,
        .sensor_ctrl.alloc_again = gc2607_alloc_again,
        .sensor_ctrl.alloc_dgain = gc2607_alloc_dgain,
};

static struct regval_list gc2607_init_regs_1920_1080_30fps_mipi[] = {
        {0x03fe,0xf0},
        {0x03fe,0xf0},
        {0x03fe,0x00},
        {0x03fe,0x00},
        {0x03fe,0x00},
        {0x03fe,0x00},
        {0x0d06,0x01},
        {0x0315,0xd4},
        {0x0d82,0x14},
        {0x0a70,0x80},
        {0x0134,0x5b},
        {0x0110,0x01},
        {0x0dd1,0x56},
        {0x0137,0x03},
        {0x0135,0x01},
        {0x0136,0x2a},
        {0x0130,0x08},
        {0x0132,0x01},
        {0x031c,0x93},
        {0x0218,0x00},
        {0x0340,0x0a},
        {0x0341,0x6e},
        {0x0342,0x08},//
        {0x0343,0x00},//hts=2048
        {0x0220,0x05},//
        {0x0221,0x37},//vts =1335
        {0x0af4,0x2b},
        {0x0002,0x30},
        {0x00c3,0x3c},
        {0x0101,0x00},
        {0x0d05,0xcc},
        {0x0218,0x00},
        {0x005e,0x84},
        {0x0007,0x15},
        {0x0350,0x01},
        {0x00c0,0x07},
        {0x00c1,0x90},
        {0x0346,0x00},
        {0x0347,0x02},
        {0x034a,0x04},
        {0x034b,0x40},
        {0x021f,0x12},
        {0x034c,0x07},
        {0x034d,0x80},
        {0x0353,0x00},
        {0x0354,0x04},
        {0x0d11,0x10},
        {0x0d22,0x00},
        {0x03f6,0x4d},
        {0x03f5,0x3c},
        {0x03f3,0x54},
        {0x0d07,0xdd},
        {0x0e71,0x00},
        {0x0e72,0x10},
        {0x0e17,0x26},
        {0x0e22,0x0d},
        {0x0e23,0x20},
        {0x0e1b,0x30},
        {0x0e3a,0x15},
        {0x0e0a,0x00},
        {0x0e0b,0x00},
        {0x0e0e,0x00},
        {0x0e2a,0x08},
        {0x0e2b,0x08},
        {0x0d02,0x73},
        {0x0d22,0x38},
        {0x0d25,0x00},
        {0x0e6a,0x39},
        {0x0050,0x05},
        {0x0089,0x03},
        {0x0070,0x40},
        {0x0071,0x40},
        {0x0072,0x40},
        {0x0073,0x40},
        {0x0040,0x82},
        {0x0030,0x80},
        {0x0031,0x80},
        {0x0032,0x80},
        {0x0033,0x80},
        {0x0202,0x04},//
        {0x0203,0x38},//1080
        {0x02b3,0x00},
        {0x02b3,0x00},
        {0x02b4,0x00},
        {0x0208,0x04},
        {0x0209,0x00},
        {0x009e,0x01},
        {0x009f,0xa0},
        {0x0db8,0x08},
        {0x0db6,0x02},
        {0x0db4,0x05},
        {0x0db5,0x16},
        {0x0db9,0x09},
        {0x0d93,0x05},
        {0x0d94,0x06},
        {0x0d95,0x0b},
        {0x0d99,0x10},
        {0x0082,0x03},
        {0x0107,0x05},
        {0x0117,0x01},
        {0x0d80,0x07},
        {0x0d81,0x02},
        {0x0d84,0x09},
        {0x0d85,0x60},
        {0x0d86,0x04},
        {0x0d87,0xb1},
        {0x0222,0x00},
        {0x0223,0x01},
        {0x0117,0x91},
        {0x03f4,0x38},
        {0x0e69,0x00},
        {0x00d6,0x00},
        {0x00d0,0x0d},
        {0x00e0,0x18},
        {0x00e1,0x18},
        {0x00e2,0x18},
        {0x00e3,0x18},
        {0x00e4,0x18},
        {0x00e5,0x18},
        {0x00e6,0x18},
        {0x00e7,0x18},
        {GC2607_REG_END, 0x00}, /* END MARKER */
};

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc2607_win_sizes[] = {
        /* [0] 1920*1080 @ max 30fps dvp*/
        {
                .width          = 1920,
                .height         = 1080,
                .fps                    = 30 << 16 | 1,
                .mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = gc2607_init_regs_1920_1080_30fps_mipi,
        }
};

struct tx_isp_sensor_win_setting *wsize = &gc2607_win_sizes[0];

/*
 * the part of driver was fixed.
 */
static struct regval_list gc2607_stream_on_mipi[] = {
        {GC2607_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list gc2607_stream_off_mipi[] = {
        {GC2607_REG_END, 0x00}, /* END MARKER */
};

int gc2607_read(struct tx_isp_subdev *sd,  uint16_t reg,
                unsigned char *value)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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
        int ret;
        ret = private_i2c_transfer(client->adapter, msg, 2);
        if (ret > 0)
                ret = 0;

        return ret;
}

int gc2607_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc2607_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != GC2607_REG_END) {
                if (vals->reg_num == GC2607_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = gc2607_read(sd, vals->reg_num, &val);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }
        return 0;
}
#endif

static int gc2607_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != GC2607_REG_END) {
                if (vals->reg_num == GC2607_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = gc2607_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}

static int gc2607_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        return 0;
}

static int gc2607_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
        unsigned char v;
        int ret;
        ret = gc2607_read(sd, 0x03f0, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
        if (ret < 0)
                return ret;
        if (v != GC2607_CHIP_ID_H)
                return -ENODEV;
        ret = gc2607_read(sd, 0x03f1, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
        if (ret < 0)
                return ret;
        if (v != GC2607_CHIP_ID_L)
                return -ENODEV;
        *ident = (*ident << 8) | v;

        return 0;
}

static int gc2607_set_expo(struct tx_isp_subdev *sd, int value)
{
        int ret = 0;
        int expo = (value & 0xffff);
        int again = (value & 0xffff0000) >> 16;
        struct again_lut *val_lut = gc2607_again_lut;

        /*set integration time*/
        ret = gc2607_write(sd, 0x0203, expo& 0xff);
        ret += gc2607_write(sd, 0x0202, expo >> 8);
        /*set sensor analog gain*/
        ret = gc2607_write(sd, 0x02b3, val_lut[again].reg2b3);
        ret = gc2607_write(sd, 0x02b4, val_lut[again].reg2b4);
        ret = gc2607_write(sd, 0x020c, val_lut[again].reg20c);
        ret = gc2607_write(sd, 0x020d, val_lut[again].reg20d);

        if (ret < 0) {
                ISP_ERROR("gc2607_write error  %d" ,__LINE__ );
                return ret;
        }

        return 0;
}

# if  0
static int gc2607_set_integration_time(struct tx_isp_subdev *sd, int value)
{
        int ret = 0;

        ret = gc2607_write(sd, 0x0203, value & 0xff);
        ret += gc2607_write(sd, 0x0202, value >> 8);
        if (ret < 0) {
                ISP_ERROR("gc2607_write error  %d\n" ,__LINE__ );
                return ret;
        }

        return 0;
}

static int gc2607_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
        int ret = 0;
        struct again_lut *val_lut = gc2607_again_lut;


        ret = gc2607_write(sd, 0x02b3, val_lut[again].reg2b3);
        ret = gc2607_write(sd, 0x02b4, val_lut[again].reg2b4);
        ret = gc2607_write(sd, 0x020c, val_lut[again].reg20c);
        ret = gc2607_write(sd, 0x020d, val_lut[again].reg20d);
        if (ret < 0) {
                ISP_ERROR("gc2607_write error  %d" ,__LINE__ );
                return ret;
        }

        return 0;
}
#endif

static int gc2607_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int gc2607_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
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

static int gc2607_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = 0;

        if(!init->enable)
                return ISP_SUCCESS;

        sensor_set_attr(sd, wsize);
        sensor->video.state = TX_ISP_MODULE_INIT;
        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        sensor->priv = wsize;

        return 0;
}

static int gc2607_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = 0;

        if (init->enable) {
                if(sensor->video.state == TX_ISP_MODULE_INIT){
                        ret = gc2607_write_array(sd, wsize->regs);
                        if (ret)
                                return ret;
                        sensor->video.state = TX_ISP_MODULE_RUNNING;
                }
                if(sensor->video.state == TX_ISP_MODULE_RUNNING){

                        ret = gc2607_write_array(sd, gc2607_stream_on_mipi);
                        ISP_WARNING("gc2607 stream on\n");
                }
        }
        else {
                ret = gc2607_write_array(sd, gc2607_stream_off_mipi);
                ISP_WARNING("gc2607 stream off\n");
        }

        return ret;
}

static int gc2607_set_fps(struct tx_isp_subdev *sd, int fps)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        unsigned int sclk = 0;
        unsigned int vts = 0;
        unsigned int hts=0;
        unsigned int max_fps = 0;
        unsigned char tmp;
        unsigned int newformat = 0; //the format is 24.8
        int ret = 0;

        switch(sensor->info.default_boot){
        case 0:
                sclk = 1335 * 2048 * 30;
                max_fps = TX_SENSOR_MAX_FPS_30;
                break;
        default:
                ISP_ERROR("Now we do not support this framerate!!!\n");
        }

        /* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
        newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
        if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
                ISP_ERROR("warn: fps(%x) not in range\n", fps);
                return -1;
        }

        ret += gc2607_read(sd, 0x0342, &tmp);
        hts = tmp;
        ret += gc2607_read(sd, 0x0343, &tmp);
        if(ret < 0)
                return -1;
        hts = ((hts << 8) + tmp);

        vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
        ret = gc2607_write(sd, 0x0220, (unsigned char)((vts >> 8) & 0xff));
        ret += gc2607_write(sd, 0x0221, (unsigned char)(vts & 0xff));

        sensor->video.fps = fps;
        sensor->video.attr->max_integration_time_native = vts - 1;
        sensor->video.attr->integration_time_limit = vts - 1;
        sensor->video.attr->total_height = vts;
        sensor->video.attr->max_integration_time = vts - 1;
        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

        return 0;
}

static int gc2607_set_mode(struct tx_isp_subdev *sd, int value)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if(wsize){
                sensor_set_attr(sd, wsize);
                ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        }

        return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct clk *sclka;
        unsigned long rate;
        int ret = 0;

        switch(info->default_boot){
        case 0:
                wsize = &gc2607_win_sizes[0];
                memcpy(&gc2607_attr.mipi, &gc2607_mipi, sizeof(gc2607_mipi));
                gc2607_attr.total_width = 2048;
                gc2607_attr.total_height = 1335;
                gc2607_attr.max_integration_time_native = 1335 -1;
                gc2607_attr.integration_time_limit = 1335 -1;
                gc2607_attr.max_integration_time = 1335 -1;
                gc2607_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                gc2607_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                gc2607_attr.again = 0;
                gc2607_attr.integration_time =0x438;
                break;
        default:
                ISP_ERROR("this init boot is not supported yet!!!\n");
        }

        switch(info->mclk){
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

        rate = private_clk_get_rate(sensor->mclk);
        if (IS_ERR(sensor->mclk)) {
                ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
                goto err_get_mclk;
        }
        if (((rate / 1000) % 24000) != 0) {
                ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
                if (IS_ERR(sclka)) {
                        pr_err("get sclka failed\n");
                } else {
                        rate = private_clk_get_rate(sclka);
                        if (((rate / 1000) % 24000) != 0) {
                                private_clk_set_rate(sclka, 1200000000);
                        }
                }
        }
        private_clk_set_rate(sensor->mclk, 24000000);
        private_clk_prepare_enable(sensor->mclk);

        reset_gpio = info->rst_gpio;
        pwdn_gpio = info->pwdn_gpio;

        ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot, wsize->width, wsize->height, info->video_interface, info->mclk);
        sensor_set_attr(sd, wsize);
        sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

err_get_mclk:
        return ret;
}

static int gc2607_g_chip_ident(struct tx_isp_subdev *sd,
                               struct tx_isp_chip_ident *chip)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        unsigned int ident = 0;
        int ret = ISP_SUCCESS;

        sensor_attr_check(sd);
        if(reset_gpio != -1){
                ret = private_gpio_request(reset_gpio,"gc2607_reset");
                if(!ret){
                        private_gpio_direction_output(reset_gpio, 1);
                        private_msleep(20);
                        private_gpio_direction_output(reset_gpio, 0);
                        private_msleep(20);
                        private_gpio_direction_output(reset_gpio, 1);
                        private_msleep(10);
                }else{
                        ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
                }
        }
        if(pwdn_gpio != -1){
                ret = private_gpio_request(pwdn_gpio,"gc2607_pwdn");
                if(!ret){
                        private_gpio_direction_output(pwdn_gpio, 1);
                        private_msleep(10);
                        private_gpio_direction_output(pwdn_gpio, 0);
                        private_msleep(10);
                        private_gpio_direction_output(pwdn_gpio, 1);
                        private_msleep(10);
                }else{
                        ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
                }
        }
        ret = gc2607_detect(sd, &ident);
        if (ret) {
                ISP_ERROR("chip found @ 0x%x (%s) is not an gc2607 chip.\n",
                          client->addr, client->adapter->name);
                return ret;
        }
        ISP_WARNING("gc2607 chip found @ 0x%02x (%s) version %s\n", client->addr, client->adapter->name, SENSOR_VERSION);
        if(chip){
                memcpy(chip->name, "gc2607", sizeof("gc2607"));
                chip->ident = ident;
                chip->revision = SENSOR_VERSION;
        }

        return 0;
}

static int gc2607_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
        long ret = 0;
        struct tx_isp_sensor_value *sensor_val = arg;
//      struct tx_isp_initarg *init = arg;

        if(IS_ERR_OR_NULL(sd)){
                ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
                return -EINVAL;
        }
        switch(cmd){
        case TX_ISP_EVENT_SENSOR_EXPO:
                if(arg)
                        ret = gc2607_set_expo(sd,sensor_val->value);
                break;
/*
  case TX_ISP_EVENT_SENSOR_INT_TIME:
  if(arg)
  ret = gc2607_set_integration_time(sd, sensor_val->value);
  break;
  case TX_ISP_EVENT_SENSOR_AGAIN:
  if(arg)
  ret = gc2607_set_analog_gain(sd, sensor_val->value);
  break;
*/
        case TX_ISP_EVENT_SENSOR_DGAIN:
                if(arg)
                        ret = gc2607_set_digital_gain(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
                if(arg)
                        ret = gc2607_get_black_pedestal(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_RESIZE:
                if(arg)
                        ret = gc2607_set_mode(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
                ret = gc2607_write_array(sd, gc2607_stream_off_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
                ret = gc2607_write_array(sd, gc2607_stream_on_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FPS:
                if(arg)
                        ret = gc2607_set_fps(sd, sensor_val->value);
                break;
        default:
                break;
        }

        return ret;
}

static int gc2607_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
        unsigned char val = 0;
        int len = 0;
        int ret = 0;

        len = strlen(sd->chip.name);
        if(len && strncmp(sd->chip.name, reg->name, len)){
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        ret = gc2607_read(sd, reg->reg & 0xffff, &val);
        reg->val = val;
        reg->size = 2;

        return ret;
}

static int gc2607_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
        int len = 0;

        len = strlen(sd->chip.name);
        if(len && strncmp(sd->chip.name, reg->name, len)){
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        gc2607_write(sd, reg->reg & 0xffff, reg->val & 0xff);

        return 0;
}

static struct tx_isp_subdev_core_ops gc2607_core_ops = {
        .g_chip_ident = gc2607_g_chip_ident,
        .reset = gc2607_reset,
        .init = gc2607_init,
        /*.ioctl = gc2607_ops_ioctl,*/
        .g_register = gc2607_g_register,
        .s_register = gc2607_s_register,
};

static struct tx_isp_subdev_video_ops gc2607_video_ops = {
        .s_stream = gc2607_s_stream,
};

static struct tx_isp_subdev_sensor_ops  gc2607_sensor_ops = {
        .ioctl  = gc2607_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc2607_ops = {
        .core = &gc2607_core_ops,
        .video = &gc2607_video_ops,
        .sensor = &gc2607_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
        .name = "gc2607",
        .id = -1,
        .dev = {
                .dma_mask = &tx_isp_module_dma_mask,
                .coherent_dma_mask = 0xffffffff,
                .platform_data = NULL,
        },
        .num_resources = 0,
};

static int gc2607_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct tx_isp_subdev *sd;
        struct tx_isp_video_in *video;
        struct tx_isp_sensor *sensor;

        sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
        if(!sensor){
                ISP_ERROR("Failed to allocate sensor subdev.\n");
                return -ENOMEM;
        }
        memset(sensor, 0 ,sizeof(*sensor));
        sensor->dev = &client->dev;
        sd = &sensor->sd;
        video = &sensor->video;
        gc2607_attr.expo_fs = 1;
        sensor->video.shvflip = shvflip;
        sensor->video.attr = &gc2607_attr;
        tx_isp_subdev_init(&sensor_platform_device, sd, &gc2607_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->gc2607\n");

        return 0;
}

static int gc2607_remove(struct i2c_client *client)
{
        struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
        struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

        if(reset_gpio != -1)
                private_gpio_free(reset_gpio);
        if(pwdn_gpio != -1)
                private_gpio_free(pwdn_gpio);

        private_clk_disable_unprepare(sensor->mclk);
        tx_isp_subdev_deinit(sd);
        kfree(sensor);

        return 0;
}

static const struct i2c_device_id gc2607_id[] = {
        { "gc2607", 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, gc2607_id);

static struct i2c_driver gc2607_driver = {
        .driver = {
                .owner  = THIS_MODULE,
                .name   = "gc2607",
        },
        .probe          = gc2607_probe,
        .remove         = gc2607_remove,
        .id_table       = gc2607_id,
};

static __init int init_gc2607(void)
{
        return private_i2c_add_driver(&gc2607_driver);
}

static __exit void exit_gc2607(void)
{
        private_i2c_del_driver(&gc2607_driver);
}

module_init(init_gc2607);
module_exit(exit_gc2607);

MODULE_DESCRIPTION("A low-level driver for gc2607 sensors");
MODULE_LICENSE("GPL");
