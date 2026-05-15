/*
 * imx482.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       30        mipi_2lane            linear
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

#define IMX482_CHIP_ID_H        (0x4C)
#define IMX482_CHIP_ID_L        (0x01)
#define IMX482_REG_END          0xffff
#define IMX482_REG_DELAY        0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION  "H20230704a"


static int reset_gpio = -1;
static int pwdn_gpio = -1;

struct regval_list {
        uint16_t reg_num;
        unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
        unsigned int value;
        unsigned int gain;
};

struct tx_isp_sensor_attribute imx482_attr;

unsigned int imx482_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
        // Limit Max gain
        if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

        /* p_ctx->again=again; */
        *sensor_again=again;
        isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

        return isp_gain;
}

unsigned int imx482_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
        return 0;
}

struct tx_isp_mipi_bus imx482_mipi={
        .mode = SENSOR_MIPI_SONY_MODE,
        .clk = 1440,
        .lans = 2,
        .settle_time_apative_en = 0,
        .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
        .mipi_sc.hcrop_diff_en = 0,
        .mipi_sc.mipi_vcomp_en = 0,
        .mipi_sc.mipi_hcomp_en = 0,
        .mipi_sc.line_sync_mode = 0,
        .mipi_sc.work_start_flag = 0,
        .image_twidth = 1932,
        .image_theight = 1080,
        .mipi_sc.mipi_crop_start0x = 0,
        .mipi_sc.mipi_crop_start0y = 0,
        .mipi_sc.mipi_crop_start1x = 0,
        .mipi_sc.mipi_crop_start1y = 0,
        .mipi_sc.mipi_crop_start2x = 0,
        .mipi_sc.mipi_crop_start2y = 0,
        .mipi_sc.mipi_crop_start3x = 0,
        .mipi_sc.mipi_crop_start3y = 0,
        .mipi_sc.data_type_en = 1,
        .mipi_sc.data_type_value = RAW12,
        .mipi_sc.del_start = 0,
        .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
        .mipi_sc.sensor_fid_mode = 0,
        .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute imx482_attr={
        .name = "imx482",
        .chip_id = 0x4C01,
        .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
        .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
        .cbus_device = 0x1a,
        .max_again = 404346,
        .max_dgain = 0,
        .min_integration_time = 2,
        .min_integration_time_native = 2,
        .integration_time_apply_delay = 2,
        .again_apply_delay = 2,
        .dgain_apply_delay = 0,
        .sensor_ctrl.alloc_again = imx482_alloc_again,
        .sensor_ctrl.alloc_dgain = imx482_alloc_dgain,
};

static struct regval_list imx482_init_regs_1920_1080_30fps_mipi[] = {
	{0x3008,0x7F},// BCWAIT_TIME[9:0]
	{0x300A,0x5B},// CPWAIT_TIME[9:0]
	{0x300B,0x50},
	{0x301D,0x05},// CFMODE[3:0]
	{0x3020,0x01},// HADD
	{0x3021,0x01},// VADD
	{0x3022,0x02},// ADDMODE[1:0]
	{0x3024,0xCA},// VMAX[15:0] -> 0x8ca = 2250
	{0x3025,0x08},
	{0x3028,0x4C},// HMAX[15:0] -> 0x44c = 1100
	{0x3029,0x04},
	{0x3031,0x00},// ADBIT
	{0x30A5,0x00},// XVS_DRV[1:0]
	{0x30D5,0x02},// DIG_CLP_VSTART
	{0x3114,0x02},// INCKSEL1[1:0]
	{0x311C,0x9B},// INCKSEL3[8:0]
	{0x3160,0xC4},
	{0x3260,0x22},
	{0x3262,0x02},
	{0x3278,0xA2},
	{0x3324,0x00},
	{0x3366,0x31},
	{0x340C,0x4D},
	{0x3416,0x10},
	{0x3417,0x13},
	{0x3432,0x93},
	{0x34CE,0x1E},
	{0x34CF,0x1E},
	{0x34DC,0x80},
	{0x351C,0x03},
	{0x359E,0x70},
	{0x35A2,0x9C},
	{0x35AC,0x08},
	{0x35C0,0xFA},
	{0x35C2,0x4E},
	{0x35DC,0x05},
	{0x35DE,0x05},
	{0x3608,0x41},
	{0x360A,0x47},
	{0x361E,0x4A},
	{0x3630,0x43},
	{0x3632,0x47},
	{0x363C,0x41},
	{0x363E,0x4A},
	{0x3648,0x41},
	{0x364A,0x47},
	{0x3660,0x04},
	{0x3676,0x3F},
	{0x367A,0x3F},
	{0x36A4,0x41},
	{0x3798,0x8C},
	{0x379A,0x8C},
	{0x379C,0x8C},
	{0x379E,0x8C},
	{0x3804,0x22},// INCKSEL4[1:0]
	{0x3888,0xA8},
	{0x388C,0xA6},
	{0x3914,0x15},
	{0x3915,0x15},
	{0x3916,0x15},
	{0x3917,0x14},
	{0x3918,0x14},
	{0x3919,0x14},
	{0x391A,0x13},
	{0x391B,0x13},
	{0x391C,0x13},
	{0x391E,0x00},
	{0x391F,0xA5},
	{0x3920,0xDE},
	{0x3921,0x0E},
	{0x39A2,0x0C},
	{0x39A4,0x16},
	{0x39A6,0x2B},
	{0x39A7,0x01},
	{0x39D2,0x2D},
	{0x39D3,0x00},
	{0x39D8,0x37},
	{0x39D9,0x00},
	{0x39DA,0x9B},
	{0x39DB,0x01},
	{0x39E0,0x28},
	{0x39E1,0x00},
	{0x39E2,0x2C},
	{0x39E3,0x00},
	{0x39E8,0x96},
	{0x39EA,0x9A},
	{0x39EB,0x01},
	{0x39F2,0x27},
	{0x39F3,0x00},
	{0x3A00,0x38},
	{0x3A01,0x00},
	{0x3A02,0x95},
	{0x3A03,0x01},
	{0x3A18,0x9B},
	{0x3A2A,0x0C},
	{0x3A30,0x15},
	{0x3A32,0x31},
	{0x3A33,0x01},
	{0x3A36,0x4D},
	{0x3A3E,0x11},
	{0x3A40,0x31},
	{0x3A42,0x4C},
	{0x3A43,0x01},
	{0x3A44,0x47},
	{0x3A46,0x4B},
	{0x3A4E,0x11},
	{0x3A50,0x32},
	{0x3A52,0x46},
	{0x3A53,0x01},
	{0x3D01,0x01},// LANEMODE[2:0]
	{0x3D04,0x48},// TXCLKESC_FREQ[15:0]
	{0x3D05,0x09},
	{0x3D18,0x9F},// TCLKPOST[15:0]
	{0x3D1A,0x57},// TCLKPREPARE[15:0]
	{0x3D1C,0x57},// TCLKTRAIL[15:0]
	{0x3D1E,0x87},// TCLKZERO[15:0]
	{0x3D20,0x5F},// THSPREPARE[15:0]
	{0x3D22,0xA7},// THSZERO[15:0]
	{0x3D24,0x5F},// THSTRAIL [15:0]
	{0x3D26,0x97},// THSEXIT [15:0]
	{0x3D28,0x4F},// TLPX[15:0]
	{0x3000,0x00},
	{IMX482_REG_DELAY,0x18},
	{0x3002,0x00},
	{0x30A5,0x00},
    {IMX482_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting imx482_win_sizes[] = {
        {
                .width          = 1920,
                .height         = 1080,
                .fps            = 30 << 16 | 1,
                .mbus_code      = TISP_VI_FMT_SRGGB12_1X12,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = imx482_init_regs_1920_1080_30fps_mipi,
        },
};
struct tx_isp_sensor_win_setting *wsize = &imx482_win_sizes[0];

static struct regval_list imx482_stream_on_mipi[] = {
        {IMX482_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list imx482_stream_off_mipi[] = {
        {IMX482_REG_END, 0x00}, /* END MARKER */
};

int imx482_read(struct tx_isp_subdev *sd, uint16_t reg,
                unsigned char *value)
{
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
        int ret;
        ret = private_i2c_transfer(client->adapter, msg, 2);
        if (ret > 0)
                ret = 0;

        return ret;
}

int imx482_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int imx482_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != IMX482_REG_END) {
                if (vals->reg_num == IMX482_REG_DELAY) {
                        msleep(vals->value);
                } else {
                        ret = imx482_read(sd, vals->reg_num, &val);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif

static int imx482_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;

        while (vals->reg_num != IMX482_REG_END) {
                if (vals->reg_num == IMX482_REG_DELAY) {
                        msleep(vals->value);
                } else {
                        ret = imx482_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}

static int imx482_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        return 0;
}

static int imx482_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
        int ret;
        unsigned char v;
		return 0;
        ret = imx482_read(sd, 0x3A42, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
        if (ret < 0)
                return ret;
        if (v != IMX482_CHIP_ID_H)
                return -ENODEV;
        *ident = v;

        ret = imx482_read(sd, 0x3A43, &v);
        ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
        if (ret < 0)
                return ret;
        if (v != IMX482_CHIP_ID_L)
                return -ENODEV;
        *ident = (*ident << 8) | v;

        return 0;
}

static int imx482_set_integration_time(struct tx_isp_subdev *sd, int value)
{
        int ret = 0;
        unsigned short shs = 0;
        unsigned short vmax = 0;

        vmax = imx482_attr.total_height;
        shs = vmax - value;

        ret = imx482_write(sd, 0x3050, (unsigned char)(shs & 0xff));
        ret += imx482_write(sd, 0x3051, (unsigned char)((shs >> 8) & 0xff));
        ret += imx482_write(sd, 0x3052, (unsigned char)((shs >> 16) & 0x0f));
        if (ret < 0)
                return ret;

        return 0;
}

static int imx482_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
        int ret = 0;

        ret += imx482_write(sd, 0x3084, (unsigned char)(value & 0xff));
		ret += imx482_write(sd, 0x3085, (unsigned char)((value >> 8) & 0x0f));
        if (ret < 0)
                return ret;

        return 0;
}

static int imx482_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int imx482_get_black_pedestal(struct tx_isp_subdev *sd, int value)
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
        sensor->video.max_fps = wsize->fps;
        sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

        return 0;
}

static int imx482_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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

static int imx482_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = 0;

        if (init->enable) {
                if(sensor->video.state == TX_ISP_MODULE_INIT){
                        ret = imx482_write_array(sd, wsize->regs);
                        if (ret)
                                return ret;
                        sensor->video.state = TX_ISP_MODULE_RUNNING;
                }
                if(sensor->video.state == TX_ISP_MODULE_RUNNING){

                        ret = imx482_write_array(sd, imx482_stream_on_mipi);
                        ISP_WARNING("imx482 stream on\n");
                }
        }
        else {
                ret = imx482_write_array(sd, imx482_stream_off_mipi);
                ISP_WARNING("imx482 stream off\n");
        }

        return ret;
}

static int imx482_set_fps(struct tx_isp_subdev *sd, int fps)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        unsigned int sclk = 0;
        unsigned int hts = 0;
        unsigned int vts = 0;
        unsigned int max_fps;
        unsigned char val = 0;
        unsigned int newformat = 0; //the format is 24.8
        int ret = 0;

        switch(sensor->info.default_boot){
        case 0:
                sclk = 74250000;
                max_fps = TX_SENSOR_MAX_FPS_30;
                break;
        default:
                ISP_ERROR("Now we do not support this framerate!!!\n");
        }

        newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
        if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
                ISP_ERROR("warn: fps(%x) no in range\n", fps);
                return -1;
        }

        ret += imx482_read(sd, 0x3029, &val);
        hts = val & 0x0f;
        ret += imx482_read(sd, 0x3028, &val);
        hts = (hts << 8) + val;
        if (0 != ret) {
                ISP_ERROR("err: imx482 read err\n");
                return -1;
        }

        vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
        ret = imx482_write(sd, 0x3026, (unsigned char)((vts & 0xf0000) >> 16));
        ret += imx482_write(sd, 0x3025, (unsigned char)((vts & 0xff00) >> 8));
        ret += imx482_write(sd, 0x3024, (unsigned char)(vts & 0xff));
        if (0 != ret) {
                ISP_ERROR("err: imx482_write err\n");
                return ret;
        }
        sensor->video.fps = fps;
        sensor->video.attr->max_integration_time_native = vts -6;
        sensor->video.attr->integration_time_limit = vts -6;
        sensor->video.attr->total_height = vts;
        sensor->video.attr->max_integration_time = vts -6;
        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

        return ret;
}

static int imx482_set_mode(struct tx_isp_subdev *sd, int value)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if(wsize){
                sensor_set_attr(sd, wsize);
                ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        }

        return ret;
}

static int imx482_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;

	/* 2'b01:mirror,2'b10:filp */
	val = imx482_read(sd, 0x3030, &val);
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
	ret = imx482_write(sd, 0x3030, val);

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct clk *sclka;
        unsigned long rate;
        int ret;

        switch(info->default_boot){
        case 0:
                wsize = &imx482_win_sizes[0];
                memcpy(&(imx482_attr.mipi), &imx482_mipi, sizeof(imx482_mipi));
                imx482_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                imx482_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                imx482_attr.max_integration_time_native = 2244;
                imx482_attr.integration_time_limit = 2244;
                imx482_attr.total_width = 1100;
                imx482_attr.total_height = 2250;
                imx482_attr.max_integration_time = 2244;
                imx482_attr.again = 0;
                imx482_attr.integration_time = 1125;
                break;
        default:
                ISP_ERROR("Have no this Setting Source!!!\n");
        }

        switch(info->video_interface){
        case TISP_SENSOR_VI_MIPI_CSI0:
                imx482_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                imx482_attr.mipi.index = 0;
                break;
        case TISP_SENSOR_VI_DVP:
                imx482_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
                break;
        default:
                ISP_ERROR("Have no this Interface Source!!!\n");
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
        switch(info->default_boot){
        case 0:
                if (((rate / 1000) % 37125) != 0) {
                        ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                        sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
                        if (IS_ERR(sclka)) {
                                pr_err("get sclka failed\n");
                        } else {
                                rate = private_clk_get_rate(sclka);
                                if (((rate / 1000) % 37125) != 0) {
                                        private_clk_set_rate(sclka, 891000000);
                                }
                        }
                }
                private_clk_set_rate(sensor->mclk, 37125000);
                private_clk_prepare_enable(sensor->mclk);
                break;
        }

        ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot, wsize->width, wsize->height, info->video_interface, info->mclk);
        reset_gpio = info->rst_gpio;
        pwdn_gpio = info->pwdn_gpio;

        sensor_set_attr(sd, wsize);
        sensor->priv = wsize;
        sensor->video.max_fps = wsize->fps;
        sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

        return 0;

}

static int imx482_g_chip_ident(struct tx_isp_subdev *sd,
                               struct tx_isp_chip_ident *chip)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        unsigned int ident = 0;
        int ret = ISP_SUCCESS;

        sensor_attr_check(sd);
        if(reset_gpio != -1){
                ret = private_gpio_request(reset_gpio,"imx482_reset");
                if(!ret){
                        private_gpio_direction_output(reset_gpio, 1);
                        private_msleep(5);
                        private_gpio_direction_output(reset_gpio, 0);
                        private_msleep(5);
                        private_gpio_direction_output(reset_gpio, 1);
                        private_msleep(5);
                }else{
                        ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
                }
        }
        if(pwdn_gpio != -1){
                ret = private_gpio_request(pwdn_gpio,"imx482_pwdn");
                if(!ret){
                        private_gpio_direction_output(pwdn_gpio, 0);
                        private_msleep(5);
                        private_gpio_direction_output(pwdn_gpio, 1);
                        private_msleep(5);
                }else{
                        ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
                }
        }
        ret = imx482_detect(sd, &ident);
        if (ret) {
                ISP_ERROR("chip found @ 0x%x (%s) is not an imx482 chip.\n",
                          client->addr, client->adapter->name);
                return ret;
        }
        ISP_WARNING("imx482 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
        ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
        if(chip){
                memcpy(chip->name, "imx482", sizeof("imx482"));
                chip->ident = ident;
                chip->revision = SENSOR_VERSION;
        }

        return 0;
}

static int imx482_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
        long ret = 0;
        struct tx_isp_sensor_value *sensor_val = arg;

        if(IS_ERR_OR_NULL(sd)){
                ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
                return -EINVAL;
        }
        switch(cmd){
        case TX_ISP_EVENT_SENSOR_EXPO:
                //      if(arg)
                //              ret = imx482_set_expo(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_INT_TIME:
                if(arg)
                        ret = imx482_set_integration_time(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN:
                if(arg)
                        ret = imx482_set_analog_gain(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_DGAIN:
                if(arg)
                        ret = imx482_set_digital_gain(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
                if(arg)
                        ret = imx482_get_black_pedestal(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_RESIZE:
                if(arg)
                        ret = imx482_set_mode(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
                ret = imx482_write_array(sd, imx482_stream_off_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
                ret = imx482_write_array(sd, imx482_stream_on_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FPS:
                if(arg)
                        ret = imx482_set_fps(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_VFLIP:
              if(arg)
                      ret = imx482_set_vflip(sd, sensor_val->value);
                break;
        default:
                break;
        }

        return ret;
}

static int imx482_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
        ret = imx482_read(sd, reg->reg & 0xffff, &val);
        reg->val = val;
        reg->size = 2;

        return ret;
}

static int imx482_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
        int len = 0;

        len = strlen(sd->chip.name);
        if(len && strncmp(sd->chip.name, reg->name, len)){
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        imx482_write(sd, reg->reg & 0xffff, reg->val & 0xff);

        return 0;
}

static struct tx_isp_subdev_core_ops imx482_core_ops = {
        .g_chip_ident = imx482_g_chip_ident,
        .reset = imx482_reset,
        .init = imx482_init,
        .g_register = imx482_g_register,
        .s_register = imx482_s_register,
};

static struct tx_isp_subdev_video_ops imx482_video_ops = {
        .s_stream = imx482_s_stream,
};

static struct tx_isp_subdev_sensor_ops  imx482_sensor_ops = {
        .ioctl  = imx482_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops imx482_ops = {
        .core = &imx482_core_ops,
        .video = &imx482_video_ops,
        .sensor = &imx482_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
        .name = "imx482",
        .id = -1,
        .dev = {
                .dma_mask = &tx_isp_module_dma_mask,
                .coherent_dma_mask = 0xffffffff,
                .platform_data = NULL,
        },
        .num_resources = 0,
};

static int imx482_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
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

        sd = &sensor->sd;
        video = &sensor->video;
        sensor->dev = &client->dev;
        imx482_attr.expo_fs = 1;
        sensor->video.shvflip = 1;
        sensor->video.attr = &imx482_attr;
        tx_isp_subdev_init(&sensor_platform_device, sd, &imx482_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->imx482\n");

        return 0;
}

static int imx482_remove(struct i2c_client *client)
{
        struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
        struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

        if(reset_gpio != -1)
                private_gpio_free(reset_gpio);
        if(pwdn_gpio != -1)
                private_gpio_free(pwdn_gpio);

        private_clk_disable_unprepare(sensor->mclk);
        private_devm_clk_put(&client->dev, sensor->mclk);
        tx_isp_subdev_deinit(sd);
        kfree(sensor);

        return 0;
}

static const struct i2c_device_id imx482_id[] = {
        { "imx482", 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, imx482_id);

static struct i2c_driver imx482_driver = {
        .driver = {
                .owner  = THIS_MODULE,
                .name   = "imx482",
        },
        .probe          = imx482_probe,
        .remove         = imx482_remove,
        .id_table       = imx482_id,
};

static __init int init_imx482(void)
{
        return private_i2c_add_driver(&imx482_driver);
}

static __exit void exit_imx482(void)
{
        private_i2c_del_driver(&imx482_driver);
}

module_init(init_imx482);
module_exit(exit_imx482);

MODULE_DESCRIPTION("A low-level driver for imx482 sensors");
MODULE_LICENSE("GPL");
