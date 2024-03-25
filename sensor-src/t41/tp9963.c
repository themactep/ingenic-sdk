// SPDX-License-Identifier: GPL-2.0+
/*
 * tp9963.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       30        mipi_2lane            linear
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
#include <txx-funcs.h>

#define SENSOR_NAME "tp9963"
#define SENSOR_CHIP_ID_H (0x28)
#define SENSOR_CHIP_ID_L (0x63)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230110a"

static int reset_gpio = -1;
static int pwdn_gpio = -1;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
};

struct tx_isp_sensor_attribute sensor_attr;

struct tx_isp_mipi_bus sensor_mipi_n = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 594,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_YUV422,
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
	.mipi_sc.data_type_value = YUV422_8BIT,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_p = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 594,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_YUV422,
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
	.mipi_sc.data_type_value = YUV422_8BIT,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x2863,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.max_again = 327680,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_YUYV8_2X8,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_YUYV8_2X8,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi,
	}
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char *value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, unsigned char reg,
		 unsigned char value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg & 0xff), value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
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

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals, unsigned char cnt)
{
	int ret;
	unsigned char val;
	while (cnt--) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			ret += sensor_read(sd, vals->reg_num, &val);
			printk("	{0x%x,0x%x}\n",vals->reg_num, val);
		}
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}
#endif

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0xfe, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0xff, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

//static int vic_num = 0;
static int sensor_set_logic(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
#if 0
	if (vic_num < 1) {
//		if (vic_num % 2 == 0)
			*((u32 *)0xb3380000) = 0x5;
		vic_num++;
	}
#endif
	return ret;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise) {
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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

enum {
	CH_ = 0,   //
	CH_ = 1,    //
	CH_ALL = 4,    //
	SENSOR_PAGE = 8,
};

enum {
	STD_TVI, //TVI
	STD_HDA, //AHD
};

enum {
	FHD25,
	FHD30, //1920x1080
};

enum {
	MIPI_2CH2LANE_594M, //up to 2x1080p25/30
	MIPI_1CH2LANE_594M,
};

void sensor_decoder_init(struct tx_isp_subdev *sd, unsigned char ch, unsigned char fmt, unsigned char std) {

	unsigned char tmp;
	const unsigned char MASK42_43[] = {0xfe, 0xfd, 0xff, 0xff, 0xfc};

	sensor_write(sd, 0x40, ch);
	sensor_write(sd, 0x06, 0x12); //default value
	sensor_write(sd, 0x50, 0x00); //VIN1/3
	sensor_write(sd, 0x51, 0x00); //
	sensor_write(sd, 0x54, 0x03);


	if (FHD25 == fmt) {
		sensor_read(sd, 0x42, &tmp);
		tmp &= MASK42_43[ch];
		sensor_write(sd, 0x42, tmp);

		sensor_read(sd, 0x43, &tmp);
		tmp &= MASK42_43[ch];
		sensor_write(sd, 0x43, tmp);

		sensor_write(sd, 0x02, 0x40);
		sensor_write(sd, 0x07, 0xc0);
		sensor_write(sd, 0x0b, 0xc0);
		sensor_write(sd, 0x0c, 0x03);
		sensor_write(sd, 0x0d, 0x50);

		sensor_write(sd, 0x15, 0x03);
		sensor_write(sd, 0x16, 0xd2);
		sensor_write(sd, 0x17, 0x80);
		sensor_write(sd, 0x18, 0x29);
		sensor_write(sd, 0x19, 0x38);
		sensor_write(sd, 0x1a, 0x47);

		sensor_write(sd, 0x1c, 0x0a);  //1920*1080, 25fps
		sensor_write(sd, 0x1d, 0x50);  //

		sensor_write(sd, 0x20, 0x30);
		sensor_write(sd, 0x21, 0x84);
		sensor_write(sd, 0x22, 0x36);
		sensor_write(sd, 0x23, 0x3c);

		sensor_write(sd, 0x2b, 0x60);
		sensor_write(sd, 0x2c, 0x0a);
		sensor_write(sd, 0x2d, 0x30);
		sensor_write(sd, 0x2e, 0x70);

		sensor_write(sd, 0x30, 0x48);
		sensor_write(sd, 0x31, 0xbb);
		sensor_write(sd, 0x32, 0x2e);
		sensor_write(sd, 0x33, 0x90);

		sensor_write(sd, 0x35, 0x05);
		sensor_write(sd, 0x39, 0x0C);

		if (STD_HDA == std) {
			sensor_write(sd, 0x02, 0x44);

			sensor_write(sd, 0x0d, 0x73);
			sensor_write(sd, 0x15, 0x01);
			sensor_write(sd, 0x16, 0xf0);
			sensor_write(sd, 0x18, 0x2a);
			sensor_write(sd, 0x20, 0x3c);
			sensor_write(sd, 0x21, 0x46);

			sensor_write(sd, 0x25, 0xfe);
			sensor_write(sd, 0x26, 0x0d);

			sensor_write(sd, 0x2c, 0x3a);
			sensor_write(sd, 0x2d, 0x54);
			sensor_write(sd, 0x2e, 0x40);

			sensor_write(sd, 0x30, 0xa5);
			sensor_write(sd, 0x31, 0x86);
			sensor_write(sd, 0x32, 0xfb);
			sensor_write(sd, 0x33, 0x60);
		}

	} else if (FHD30 == fmt) {

		sensor_read(sd, 0x42, &tmp);
		tmp &= MASK42_43[ch];
		sensor_write(sd, 0x42, tmp);

		tmp = sensor_read(sd, 0x43, &tmp);
		tmp &= MASK42_43[ch];
		sensor_write(sd, 0x43, tmp);

		sensor_write(sd, 0x02, 0x40);
		sensor_write(sd, 0x07, 0xc0);
		sensor_write(sd, 0x0b, 0xc0);
		sensor_write(sd, 0x0c, 0x03);
		sensor_write(sd, 0x0d, 0x50);

		sensor_write(sd, 0x15, 0x03);
		sensor_write(sd, 0x16, 0xd2);
		sensor_write(sd, 0x17, 0x80);
		sensor_write(sd, 0x18, 0x29);
		sensor_write(sd, 0x19, 0x38);
		sensor_write(sd, 0x1a, 0x47);
		sensor_write(sd, 0x1c, 0x08);  //1920*1080, 30fps
		sensor_write(sd, 0x1d, 0x98);  //

		sensor_write(sd, 0x20, 0x30);
		sensor_write(sd, 0x21, 0x84);
		sensor_write(sd, 0x22, 0x36);
		sensor_write(sd, 0x23, 0x3c);

		sensor_write(sd, 0x2b, 0x60);
		sensor_write(sd, 0x2c, 0x0a);
		sensor_write(sd, 0x2d, 0x30);
		sensor_write(sd, 0x2e, 0x70);

		sensor_write(sd, 0x30, 0x48);
		sensor_write(sd, 0x31, 0xbb);
		sensor_write(sd, 0x32, 0x2e);
		sensor_write(sd, 0x33, 0x90);

		sensor_write(sd, 0x35, 0x05);
		sensor_write(sd, 0x39, 0x0C);

		if (STD_HDA == std) {
			sensor_write(sd, 0x02, 0x44);

			sensor_write(sd, 0x0d, 0x72);

			sensor_write(sd, 0x15, 0x01);
			sensor_write(sd, 0x16, 0xf0);
			sensor_write(sd, 0x18, 0x2a);

			sensor_write(sd, 0x20, 0x38);
			sensor_write(sd, 0x21, 0x46);

			sensor_write(sd, 0x25, 0xfe);
			sensor_write(sd, 0x26, 0x0d);

			sensor_write(sd, 0x2c, 0x3a);
			sensor_write(sd, 0x2d, 0x54);
			sensor_write(sd, 0x2e, 0x40);

			sensor_write(sd, 0x30, 0xa5);
			sensor_write(sd, 0x31, 0x95);
			sensor_write(sd, 0x32, 0xe0);
			sensor_write(sd, 0x33, 0x60);
		}
	}
}

void sensor_mipi_out(struct tx_isp_subdev *sd, unsigned char output) {
	//mipi setting
	sensor_write(sd, 0x40, SENSOR_PAGE); //MIPI page
	sensor_write(sd, 0x02, 0x78);
	sensor_write(sd, 0x03, 0x70);
	sensor_write(sd, 0x04, 0x70);
	sensor_write(sd, 0x05, 0x70);
	sensor_write(sd, 0x06, 0x70);
	sensor_write(sd, 0x13, 0xef);

	sensor_write(sd, 0x20, 0x00);
	sensor_write(sd, 0x21, 0x22);
	sensor_write(sd, 0x22, 0x20);
	sensor_write(sd, 0x23, 0x9e);

	if (MIPI_1CH2LANE_594M == output) {
		sensor_write(sd, 0x21, 0x12);

		sensor_write(sd, 0x14, 0x00);
		sensor_write(sd, 0x15, 0x01);

		sensor_write(sd, 0x2a, 0x08);
		sensor_write(sd, 0x2b, 0x08);
		sensor_write(sd, 0x2c, 0x10);
		sensor_write(sd, 0x2e, 0x0a);

		sensor_write(sd, 0x10, 0xa0);
		sensor_write(sd, 0x10, 0x20);

	}

	if (MIPI_2CH2LANE_594M == output) {
		sensor_write(sd, 0x21, 0x22);

		sensor_write(sd, 0x14, 0x00);
		sensor_write(sd, 0x15, 0x01);

		sensor_write(sd, 0x2a, 0x08);
		sensor_write(sd, 0x2b, 0x08);
		sensor_write(sd, 0x2c, 0x10);
		sensor_write(sd, 0x2e, 0x0a);

		sensor_write(sd, 0x10, 0xa0);
		sensor_write(sd, 0x10, 0x20);

	}

	/* Enable MIPI CSI2 output */
	sensor_write(sd, 0x28, 0x02);
	sensor_write(sd, 0x28, 0x00);

}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char reg3 = 0;
	unsigned char reg1 = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
//N_setting
			sensor_decoder_init(sd, CH_ALL, FHD30, STD_HDA);
			sensor_mipi_out(sd, MIPI_2CH2LANE_594M);

			private_msleep(1000);
			sensor_read(sd, 0x03, &reg3);
			if ((reg3 & 0x07) == 0x2) {
				printk("\n======> now is N <======\n");
				private_msleep(300);
				sensor_read(sd, 0x01, &reg1);
				if (((reg1 & (1 << 2)) >> 2) == 0) {
					printk("\n=====> now is AHD <======\n");
				}
			}

			if ((reg3 & 0x07) == 0x3) {
				printk("\n======> now is P <======\n");
				sensor_decoder_init(sd, CH_ALL, FHD25, STD_HDA);
				sensor_mipi_out(sd, MIPI_2CH2LANE_594M);
				private_msleep(300);
				sensor_read(sd, 0x01, &reg1);
				if (((reg1 & (1 << 2)) >> 2) == 0) {
					printk("\n=====> now is AHD <======\n");
				}
			}

			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING) {
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
			private_msleep(1000);
			*((u32 *) 0xb3380000) = 0x5;
		}
	} else {
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret;

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			memcpy(&(sensor_attr.mipi), &sensor_mipi_n, sizeof(sensor_mipi_n));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
				sensor_attr.max_integration_time_native = 1080;
			sensor_attr.integration_time_limit = 1080;
			sensor_attr.total_width = 1920;
			sensor_attr.total_height = 1080;
			sensor_attr.max_integration_time = 1080;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0;
			break;
		case 1:
			wsize = &sensor_win_sizes[1];
			memcpy(&(sensor_attr.mipi), &sensor_mipi_p, sizeof(sensor_mipi_p));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
				sensor_attr.max_integration_time_native = 1080;
			sensor_attr.integration_time_limit = 1080;
			sensor_attr.total_width = 1920;
			sensor_attr.total_height = 1080;
			sensor_attr.max_integration_time = 1080;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0;
			break;
		default:
			ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch (info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 0;
			break;
		case TISP_SENSOR_VI_DVP:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
			break;
		default:
			ISP_ERROR("Have no this Interface Source!!!\n");
	}

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

	rate = private_clk_get_rate(sensor->mclk);
	switch (info->default_boot) {
		case 0:
			if (((rate / 1000) % 27000) != 0) {
				ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
				sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
				if (IS_ERR(sclka)) {
					pr_err("get sclka failed\n");
				} else {
					rate = private_clk_get_rate(sclka);
					if (((rate / 1000) % 27000) != 0) {
						private_clk_set_rate(sclka, 1188000000);
					}
				}
			}
			private_clk_set_rate(sensor->mclk, 27000000);
			private_clk_prepare_enable(sensor->mclk);
			break;
	}

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot,
		    wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	return 0;

}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_LOGIC:
			if (arg)
				ret = sensor_set_logic(sd, sensor_val->value);
			break;
		default:
			break;
	}

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg) {
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

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg) {
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
static u64 tx_isp_module_dma_mask = ~(u64) 0;
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
			const struct i2c_device_id *id) {
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
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &sensor_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client) {
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
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

static __init int init_sensor(void) {
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
