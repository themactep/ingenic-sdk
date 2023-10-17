/*
 * tp2850.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#define TP2850_CHIP_ID_H	(0x28)
#define TP2850_CHIP_ID_L	(0x50)
#define TP2850_REG_END		0x66
#define TP2850_REG_DELAY	0x00
#define TP2850_SUPPORT_30FPS_MIPI_SCLK (78000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20220108a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int default_boot = 0;
module_param(default_boot, int, S_IRUGO);
MODULE_PARM_DESC(default_boot, "Mode");

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct tx_isp_sensor_attribute tp2850_attr;

unsigned int tp2850_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	return isp_gain;
}

unsigned int tp2850_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute tp2850_attr={
	.name = "tp2850",
	.chip_id = 0x2850,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	/* i2c */
	.cbus_device = 0x44,
	/* mipi */
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
        .mipi = {
                .mode = SENSOR_MIPI_OTHER_MODE,
                .clk = 500,
                .lans = 2,
                .settle_time_apative_en = 0,
                .mipi_sc.sensor_csi_fmt = TX_SENSOR_YUV422,//RAW10
                .mipi_sc.hcrop_diff_en = 0,
                .mipi_sc.mipi_vcomp_en = 0,
                .mipi_sc.mipi_hcomp_en = 0,
                .mipi_sc.line_sync_mode = 0,
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
                .mipi_sc.work_start_flag = 0,
                .mipi_sc.data_type_en = 0,
                .mipi_sc.data_type_value = YUV422_8BIT,
                .mipi_sc.del_start = 0,
                .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
                .mipi_sc.sensor_fid_mode = 0,
                .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
        },
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.total_width = 0x44c * 2,
	.total_height = 0x58a,
	.sensor_ctrl.alloc_again = tp2850_alloc_again,
	.sensor_ctrl.alloc_dgain = tp2850_alloc_dgain,
};

static struct regval_list tp2850_init_regs_1920_1080_30fps_mipi[] = {
        {0xfe, 0x28},
        {0xff, 0x50},
        {0x40, 0x00},
        {0x41, 0x00},
        {0x4c, 0x40},
        {0x4e, 0x00},
        {0x35, 0x25},
        {0xf5, 0x10},
        {0xfd, 0x80},
        {0x38, 0x40},
        {0x3d, 0x60},
        {0x41, 0x00},
        {0x40, 0x08},
        {0x23, 0x02},
        {0x40, 0x00},
        {0x07, 0xc0},
        {0x0b, 0xc0},
        {0x26, 0x0c},
        {0xa7, 0x00},
        {0x06, 0x32},

        {0x15, 0x03},
        {0x16, 0xd3},
        {0x17, 0x80},
        {0x18, 0x29},
        {0x19, 0x38},
        {0x1a, 0x47},
        {0x1b, 0x00},
        {0x1c, 0x08},
        {0x1d, 0x98},

        {0x02, 0x4c},
        {0x0b, 0xc0},
        {0x0c, 0x03},
        {0x0d, 0x72},
        {0x20, 0x38},
        {0x21, 0x46},
        {0x22, 0x36},
        {0x23, 0x3c},
        {0x25, 0xfe},
        {0x26, 0x0d},
        {0x27, 0x2d},
        {0x28, 0x00},
        {0x2b, 0x60},
        {0x2c, 0x3a},
        {0x2d, 0x54},
        {0x2e, 0x40},
        {0x30, 0xa5},
        {0x31, 0x95},
        {0x32, 0xe0},
        {0x33, 0x60},
        {0x39, 0x1c},
        {0x3a, 0x32},
        {0x3b, 0x26},
        {0x13, 0x00},
        {0x14, 0x00},
        {0x16, 0xee},
        {0x40, 0x08},
        {0x01, 0xf8},
        {0x02, 0x01},
        {0x13, 0x04},
        {0x14, 0x46},
        {0x15, 0x08},
        {0x20, 0x12},
        {0x34, 0x1b},
        {0x40, 0x00},
        {0x35, 0x05},
        {0xfa, 0x08},
        {0x40, 0x08},
        {0x08, 0x0f},
        {0x23, 0x00},
        {0x40, 0x00},
        {0x06, 0xb2},
	{TP2850_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list tp2850_init_regs_1920_1080_30fps_mipi_test[] = {
        {0xfe, 0x28},
        {0xff, 0x50},
        {0x40, 0x00},
        {0x41, 0x00},
        {0x4c, 0x40},
        {0x4e, 0x00},
        {0x35, 0x25},
        {0xf5, 0x10},
        {0xfd, 0x80},
        {0x38, 0x40},
        {0x3d, 0x60},
        {0x41, 0x00},
        {0x40, 0x08},
        {0x23, 0x02},
        {0x40, 0x00},
        {0x07, 0xc0},
        {0x0b, 0xc0},
        {0x26, 0x0c},
        {0xa7, 0x00},
        {0x06, 0x32},

        {0x2a, 0x3c},
        {0x15, 0x03},
        {0x16, 0xd3},
        {0x17, 0x80},
        {0x18, 0x29},
        {0x19, 0x38},
        {0x1a, 0x47},
        {0x1b, 0x00},
        {0x1c, 0x08},
        {0x1d, 0x98},

        {0x02, 0x4c},
        {0x0b, 0xc0},
        {0x0c, 0x03},
        {0x0d, 0x72},
        {0x20, 0x38},
        {0x21, 0x46},
        {0x22, 0x36},
        {0x23, 0x3c},
        {0x25, 0xfe},
        {0x26, 0x0d},
        {0x27, 0x2d},
        {0x28, 0x00},
        {0x2b, 0x60},
        {0x2c, 0x3a},
        {0x2d, 0x54},
        {0x2e, 0x40},
        {0x30, 0xa5},
        {0x31, 0x95},
        {0x32, 0xe0},
        {0x33, 0x60},
        {0x39, 0x1c},
        {0x3a, 0x32},
        {0x3b, 0x26},
        {0x13, 0x00},
        {0x14, 0x00},
        {0x16, 0xee},
        {0x40, 0x08},
        {0x01, 0xf8},
        {0x02, 0x01},
        {0x13, 0x04},
        {0x14, 0x46},
        {0x15, 0x08},
        {0x20, 0x12},
        {0x34, 0x1b},
        {0x40, 0x00},
        {0x35, 0x05},
        {0xfa, 0x08},
        {0x40, 0x08},
        {0x08, 0x0f},
        {0x23, 0x00},
        {0x40, 0x00},
        {0x06, 0xb2},
	{TP2850_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting tp2850_win_sizes[] = {
	/* 1920*1080 @ max 30fps mipi*/
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_1X16,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= tp2850_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_1X16,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= tp2850_init_regs_1920_1080_30fps_mipi_test,
	},
};
struct tx_isp_sensor_win_setting *wsize = &tp2850_win_sizes[0];

int tp2850_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int tp2850_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int tp2850_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != TP2850_REG_END) {
		if (vals->reg_num == TP2850_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = tp2850_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int tp2850_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != TP2850_REG_END) {
		if (vals->reg_num == TP2850_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = tp2850_write(sd, vals->reg_num, vals->value);

                        /* { */
                        /*         unsigned char val; */
                        /*         ret = tp2850_read(sd, vals->reg_num, &val); */
                        /*         printk("----------->> tp2850 reg[0x%x] = 0x%x <<-----------\n", vals->reg_num, val); */
                        /* } */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int tp2850_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int tp2850_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = tp2850_read(sd, 0xfe, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != TP2850_CHIP_ID_H)
		return -ENODEV;
	ret = tp2850_read(sd, 0xff, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != TP2850_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int tp2850_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = tp2850_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int tp2850_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		pr_debug("tp2850 stream on\n");
	} else {
		pr_debug("tp2850 stream off\n");
	}

	return ret;
}

static int tp2850_set_fps(struct tx_isp_subdev *sd, int fps)
{
	return 0;
}

static int tp2850_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

static int tp2850_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"tp2850_reset");
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
		ret = private_gpio_request(pwdn_gpio,"tp2850_pwdn");
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
	ret = tp2850_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an tp2850 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("tp2850 chip found @ 0x%02x (%s) version %s\n", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "tp2850", sizeof("tp2850"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int tp2850_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = tp2850_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = tp2850_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int tp2850_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = tp2850_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int tp2850_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	tp2850_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops tp2850_core_ops = {
	.g_chip_ident = tp2850_g_chip_ident,
	.reset = tp2850_reset,
	.init = tp2850_init,
	/*.ioctl = tp2850_ops_ioctl,*/
	.g_register = tp2850_g_register,
	.s_register = tp2850_s_register,
};

static struct tx_isp_subdev_video_ops tp2850_video_ops = {
	.s_stream = tp2850_s_stream,
};

static struct tx_isp_subdev_sensor_ops	tp2850_sensor_ops = {
	.ioctl	= tp2850_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops tp2850_ops = {
	.core = &tp2850_core_ops,
	.video = &tp2850_video_ops,
	.sensor = &tp2850_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "tp2850",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int tp2850_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	int ret;
	unsigned long rate=0;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

        printk("------>> mode: %d\n", default_boot);
        wsize = &tp2850_win_sizes[default_boot];

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 27000) != 0) {
		struct clk *vpll;
		vpll = clk_get(NULL,"vpll");
		if (IS_ERR(vpll)) {
			pr_err("get vpll failed\n");
		} else {
			rate = clk_get_rate(vpll);
			if (((rate / 1000) % 27000) != 0) {
                                printk("------------>> %s:%d <<-------------\n", __func__, __LINE__);
				clk_set_rate(vpll,10800000);
			}
			ret = clk_set_parent(sensor->mclk, vpll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}
	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/
	tp2850_attr.max_again = 444864;
	tp2850_attr.max_dgain = 0;
	tp2850_attr.expo_fs = 1;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &tp2850_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &tp2850_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->tp2850\n");
	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int tp2850_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id tp2850_id[] = {
	{ "tp2850", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tp2850_id);

static struct i2c_driver tp2850_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tp2850",
	},
	.probe		= tp2850_probe,
	.remove		= tp2850_remove,
	.id_table	= tp2850_id,
};

static __init int init_tp2850(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init tp2850 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&tp2850_driver);
}

static __exit void exit_tp2850(void)
{
	private_i2c_del_driver(&tp2850_driver);
}

module_init(init_tp2850);
module_exit(exit_tp2850);

MODULE_DESCRIPTION("A low-level driver for galaxycore tp2850 sensors");
MODULE_LICENSE("GPL");
