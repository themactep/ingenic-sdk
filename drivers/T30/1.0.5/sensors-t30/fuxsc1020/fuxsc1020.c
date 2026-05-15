/*
 * fuxsc1020.c
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
#include <apical-isp/apical_math.h>

#define FUXSC1020_REG_END		0xff
#define FUXSC1020_REG_DELAY		0xfe

#define FUXSC1020_SUPPORT_PCLK (74*1000*1000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H201807011a"

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

struct tx_isp_sensor_attribute fuxsc1020_attr;

unsigned int fuxsc1020_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	return 0;
}

unsigned int fuxsc1020_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}
struct tx_isp_sensor_attribute fuxsc1020_attr={
	.name = "fuxsc1020",
	.chip_id = 0xa042,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 0xff << (TX_ISP_GAIN_FIXED_POINT - 4),
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 745,
	.integration_time_limit = 745,
	.total_width = 1980,
	.total_height = 750,
	.max_integration_time = 745,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 1,
	.dgain_apply_delay = 1,
	.sensor_ctrl.alloc_again = fuxsc1020_alloc_again,
	.sensor_ctrl.alloc_dgain = fuxsc1020_alloc_dgain,
	.one_line_expr_in_us = 44,
	//void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list fuxsc1020_init_regs_1280_720_25fps[] = {
	{FUXSC1020_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the fuxsc1020_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting fuxsc1020_win_sizes[] = {
	/* 1280*800 */
	{
		.width		= 1280,
		.height		= 720,
		.fps		= 25 << 16 | 1, /* 12.5 fps */
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_1X16,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= fuxsc1020_init_regs_1280_720_25fps,
	}
};

/*
 * the part of driver was fixed.
 */

static struct regval_list fuxsc1020_stream_on[] = {
	{FUXSC1020_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list fuxsc1020_stream_off[] = {
	{FUXSC1020_REG_END, 0x00},	/* END MARKER */
};

int fuxsc1020_read(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char *value)
{
	return 0;
}

int fuxsc1020_write(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char value)
{
	return 0;
}

static int fuxsc1020_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	return 0;
}

static int fuxsc1020_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int fuxsc1020_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	return 0;
}

static int fuxsc1020_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	return 0;
}
static int fuxsc1020_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}
static int fuxsc1020_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int fuxsc1020_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int fuxsc1020_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = (container_of(sd, struct tx_isp_sensor, sd));
	struct tx_isp_sensor_win_setting *wsize = &fuxsc1020_win_sizes[0];
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return ret;
}

static int fuxsc1020_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = fuxsc1020_write_array(sd, fuxsc1020_stream_on);
		pr_debug("fuxsc1020 stream on\n");
	}
	else {
		ret = fuxsc1020_write_array(sd, fuxsc1020_stream_off);
		pr_debug("fuxsc1020 stream off\n");
	}
	return ret;
}

static int fuxsc1020_set_fps(struct tx_isp_subdev *sd, int fps)
{
	return 0;
}

static int fuxsc1020_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;
	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &fuxsc1020_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &fuxsc1020_win_sizes[0];
	}
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

static int fuxsc1020_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	return 0;
}

static int fuxsc1020_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = gpio_request(reset_gpio,"fuxsc1020_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = gpio_request(pwdn_gpio, "fuxsc1020_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			printk("gpio requrest fail %d\n", pwdn_gpio);
		}
	}
	ret = fuxsc1020_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an fuxsc1020 chip.\n",
		       client->addr, client->adapter->name);
		return ret;
	}
	printk("fuxsc1020 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "fuxsc1020", sizeof("fuxsc1020"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int fuxsc1020_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = fuxsc1020_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = fuxsc1020_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = fuxsc1020_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = fuxsc1020_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = fuxsc1020_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(arg)
			ret = fuxsc1020_write_array(sd, fuxsc1020_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(arg)
			ret = fuxsc1020_write_array(sd, fuxsc1020_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = fuxsc1020_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = fuxsc1020_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;;
	}
	return 0;
}

static int fuxsc1020_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = fuxsc1020_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int fuxsc1020_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	fuxsc1020_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops fuxsc1020_core_ops = {
	.g_chip_ident = fuxsc1020_g_chip_ident,
	.reset = fuxsc1020_reset,
	.init = fuxsc1020_init,
	.g_register = fuxsc1020_g_register,
	.s_register = fuxsc1020_s_register,
};

static struct tx_isp_subdev_video_ops fuxsc1020_video_ops = {
	.s_stream = fuxsc1020_s_stream,
};

static struct tx_isp_subdev_sensor_ops	fuxsc1020_sensor_ops = {
	.ioctl	= fuxsc1020_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops fuxsc1020_ops = {
	.core = &fuxsc1020_core_ops,
	.video = &fuxsc1020_video_ops,
	.sensor = &fuxsc1020_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "fuxsc1020",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int fuxsc1020_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &fuxsc1020_win_sizes[0];
	int ret = -1;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 37125000);
	private_clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	fuxsc1020_attr.dvp.gpio = sensor_gpio_func;
	 /*
		convert sensor-gain into isp-gain,
	 */
	fuxsc1020_attr.max_again = 	log2_fixed_to_fixed(fuxsc1020_attr.max_again, TX_ISP_GAIN_FIXED_POINT, LOG2_GAIN_SHIFT);
	fuxsc1020_attr.max_dgain = fuxsc1020_attr.max_dgain;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &fuxsc1020_attr;
	sensor->video.mbus_change = 0;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &fuxsc1020_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->fuxsc1020\n");

	return 0;
err_set_sensor_gpio:
	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int fuxsc1020_remove(struct i2c_client *client)
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

static const struct i2c_device_id fuxsc1020_id[] = {
	{ "fuxsc1020", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fuxsc1020_id);

static struct i2c_driver fuxsc1020_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "fuxsc1020",
	},
	.probe		= fuxsc1020_probe,
	.remove		= fuxsc1020_remove,
	.id_table	= fuxsc1020_id,
};

static __init int init_fuxsc1020(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to init fuxsc1020 dirver.\n");
		return -1;
	}

	return private_i2c_add_driver(&fuxsc1020_driver);
}

static __exit void exit_fuxsc1020(void)
{
	i2c_del_driver(&fuxsc1020_driver);
}

module_init(init_fuxsc1020);
module_exit(exit_fuxsc1020);

MODULE_DESCRIPTION("A low-level driver for fuxsc1020 sensors");
MODULE_LICENSE("GPL");
