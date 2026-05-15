/*
 * imx291.c
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

#define IMX291_CHIP_ID_H	(0xA0)
#define IMX291_CHIP_ID_L	(0xB2)

#define IMX291_REG_END		0xffff
#define IMX291_REG_DELAY	0xfffe

#define IMX291_SUPPORT_PCLK (74250*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 15
#define AGAIN_MAX_DB 0x50
#define DGAIN_MAX_DB 0x3c
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION	"H20180416a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_12BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */

unsigned int imx291_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{

	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;
	return isp_gain;

}

unsigned int imx291_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	*sensor_dgain = 0;
	return 0;
}

struct tx_isp_sensor_attribute imx291_attr = {
	.name = "imx291",
	.chip_id = 0xa0b2,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_SONY_MODE,
		.blanking = {
			.vblanking = 17,
			.hblanking = 11,
		},
	},
	.max_again = 458752,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1125-2,
	.integration_time_limit = 1125-2,
	.total_width = 2640,
	.total_height = 1125,
	.max_integration_time = 1125-2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = imx291_alloc_again,
	.sensor_ctrl.alloc_dgain = imx291_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};


static struct regval_list imx291_init_regs_1920_1080_25fps[] = {
/* inclk 37.125M clk*/
	{0x3000,0x01},
	{0x3002,0x00},//MODE
	{0x3005,0x01},
	{0x3007,0x00},//1080p
	{0x3009,0x02},
	{0x300a,0xf0},
	{0x300f,0x00},
	{0x3010,0x21},
	{0x3012,0x64},
	{0x3013,0x00},
	{0x3016,0x09},
	{0x3018,0x65},
	{0x3019,0x04},
	{0x301c,0xa0},
	{0x301d,0x14},
	{0x3046,0x01},
	{0x304B,0x0A},
	{0x305c,0x18},
	{0x305d,0x00},
	{0x305e,0x20},
	{0x305f,0x01},
	{0x3129,0x00},
	{0x3070,0x02},
	{0x3071,0x11},
	{0x309b,0x10},
	{0x309c,0x22},
	{0x30a2,0x02},
	{0x30a6,0x20},
	{0x30a8,0x20},
	{0x30aa,0x20},
	{0x30ac,0x20},
	{0x30b0,0x43},
	{0x3119,0x9e},
	{0x311c,0x1e},
	{0x311e,0x08},
	{0x3128,0x05},
	{0x3129,0x00},
	{0x313d,0x83},
	{0x3150,0x13},
	{0x315e,0x1A},
	{0x3164,0x1A},
	{0x317c,0x00},
	{0x317e,0x00},
	{0x31ec,0x0e},
	{0x3418,0xd9},
	{0x3419,0x02},
	{0x3480,0x49},
	{0x332c,0xd3},
	{0x332d,0x10},
	{0x332d,0x0d},
	{0x3358,0x06},
	{0x3359,0xe1},
	{0x335a,0x11},
	{0x3360,0x1e},
	{0x3362,0x61},
	{0x3363,0x10},
	{0x33b0,0x50},
	{0x33b2,0x1a},
	{0x33b3,0x04},
	{0x32b8,0x50},
	{0x32b9,0x10},
	{0x32ba,0x00},
	{0x32bb,0x04},
	{0x32c8,0x50},
	{0x32c9,0x10},
	{0x32ca,0x00},
	{0x32cb,0x04},
	{0x3480,0x49},
	{IMX291_REG_DELAY, 0x14},	/* END MARKER */
	{IMX291_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the imx291_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting imx291_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SGRBG12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= imx291_init_regs_1920_1080_25fps,
	}
};

static enum v4l2_mbus_pixelcode imx291_mbus_code[] = {
	V4L2_MBUS_FMT_SGRBG10_1X10,
	V4L2_MBUS_FMT_SGRBG12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list imx291_stream_on[] = {
	{0x3000,0x00},
	{IMX291_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list imx291_stream_off[] = {
	{0x3000,0x01},
	{IMX291_REG_END, 0x00},	/* END MARKER */
};

int imx291_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
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

int imx291_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int imx291_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != IMX291_REG_END) {
		if (vals->reg_num == IMX291_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx291_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
			printk("{0x%0x, 0x%02x}\n",vals->reg_num, val);
		}
		vals++;
	}
	return 0;
}
static int imx291_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != IMX291_REG_END) {
		if (vals->reg_num == IMX291_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx291_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int imx291_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int imx291_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = imx291_read(sd, 0x3008, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX291_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = imx291_read(sd, 0x301e, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX291_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;
	return 0;
}

static int imx291_set_integration_time(struct tx_isp_subdev *sd, int int_time)
{
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;

	vmax = imx291_attr.total_height;
	shs = vmax - int_time - 2;
	ret = imx291_write(sd, 0x3020, (unsigned char)(shs & 0xff));
	ret += imx291_write(sd, 0x3021, (unsigned char)((shs >> 8) & 0xff));
	ret += imx291_write(sd, 0x3022, (unsigned char)((shs >> 16) & 0x3));
	if (0 != ret) {
		printk("err: imx291_write err\n");
		return ret;
	}
	return 0;

}

static int imx291_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = imx291_write(sd, 0x3014, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;
	return 0;
}

static int imx291_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx291_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx291_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &imx291_win_sizes[0];
	int ret = 0;
	if(!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = imx291_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int imx291_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	if (enable) {
		ret = imx291_write_array(sd, imx291_stream_on);
		pr_debug("imx291 stream on\n");
	}
	else {
		ret = imx291_write_array(sd, imx291_stream_off);
		pr_debug("imx291 stream off\n");
	}
	return ret;
}

static int imx291_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int pclk = 0;
	unsigned int hmax = 0;
	unsigned int vmax = 0;
	unsigned char value = 0;
	unsigned int newformat = 0; //the format is 24.8

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	pclk = IMX291_SUPPORT_PCLK;

	ret = imx291_read(sd, 0x3018, &value);
	vmax = value;
	ret += imx291_read(sd, 0x3019, &value);
	vmax |= value << 8;
	ret += imx291_read(sd, 0x301a, &value);
	vmax |= (value&0x3) << 16;

	hmax = ((pclk << 4) / (vmax * (newformat >> 4))) << 1;
	ret += imx291_write(sd, 0x301c, hmax&0xff);
	ret += imx291_write(sd, 0x301d, (hmax>>8)&0xff);
	if (0 != ret) {
		printk("err: imx291_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->total_width = hmax >> 1;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int imx291_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;
	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &imx291_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &imx291_win_sizes[0];
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

static int imx291_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	ret += imx291_read(sd, 0x3007, &val);
	if (enable){
		val = val | 0x01;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SRGGB12_1X12;
	} else {
		val = val & 0xfe;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGRBG12_1X12;
	}
	sensor->video.mbus_change = 0;
	ret += imx291_write(sd, 0x3007, val);

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int imx291_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"imx291_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"imx291_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = imx291_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an imx291 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
	printk("imx291 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "imx291", sizeof("imx291"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}
static int imx291_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if(arg)
				ret = imx291_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if(arg)
				ret = imx291_set_analog_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if(arg)
				ret = imx291_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if(arg)
				ret = imx291_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if(arg)
				ret = imx291_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			ret = imx291_write_array(sd, imx291_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = imx291_write_array(sd, imx291_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if(arg)
				ret = imx291_set_fps(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if(arg)
				ret = imx291_set_vflip(sd, *(int*)arg);
			break;
		default:
			break;;
	}
	return 0;
}

static int imx291_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = imx291_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int imx291_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	imx291_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops imx291_core_ops = {
	.g_chip_ident = imx291_g_chip_ident,
	.reset = imx291_reset,
	.init = imx291_init,
	.g_register = imx291_g_register,
	.s_register = imx291_s_register,
};

static struct tx_isp_subdev_video_ops imx291_video_ops = {
	.s_stream = imx291_s_stream,
};

static struct tx_isp_subdev_sensor_ops	imx291_sensor_ops = {
	.ioctl	= imx291_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops imx291_ops = {
	.core = &imx291_core_ops,
	.video = &imx291_video_ops,
	.sensor = &imx291_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "imx291",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int imx291_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &imx291_win_sizes[0];
	enum v4l2_mbus_pixelcode mbus;
	int i = 0;
	int ret;
	unsigned long rate = 0;

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

	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 37125) != 0) {
		struct clk *epll;
		epll = clk_get(NULL,"epll");
		if (IS_ERR(epll)) {
			pr_err("get epll failed\n");
		} else {
			rate = clk_get_rate(epll);
			if (((rate / 1000) % 37125) != 0) {
				clk_set_rate(epll,891000000);
			}
			ret = clk_set_parent(sensor->mclk, epll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}

	clk_set_rate(sensor->mclk, 37125000);
	clk_enable(sensor->mclk);
	pr_debug("mclk=%lu\n", clk_get_rate(sensor->mclk));

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	imx291_attr.dvp.gpio = sensor_gpio_func;

	switch(sensor_gpio_func){
	case DVP_PA_LOW_10BIT:
	case DVP_PA_HIGH_10BIT:
		mbus = imx291_mbus_code[0];
		break;
	case DVP_PA_12BIT:
		mbus = imx291_mbus_code[1];
		break;
	default:
		goto err_set_sensor_gpio;
	}

	for(i = 0; i < ARRAY_SIZE(imx291_win_sizes); i++)
		imx291_win_sizes[i].mbus_code = mbus;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &imx291_attr;
	sensor->video.mbus_change = 0;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx291_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("@@@@@@@probe ok ------->imx291\n");
	return 0;
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int imx291_remove(struct i2c_client *client)
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

static const struct i2c_device_id imx291_id[] = {
	{ "imx291", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx291_id);

static struct i2c_driver imx291_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "imx291",
	},
	.probe		= imx291_probe,
	.remove		= imx291_remove,
	.id_table	= imx291_id,
};

static __init int init_imx291(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to init imx291 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&imx291_driver);
}

static __exit void exit_imx291(void)
{
	i2c_del_driver(&imx291_driver);
}

module_init(init_imx291);
module_exit(exit_imx291);

MODULE_DESCRIPTION("A low-level driver for Sony imx291 sensors");
MODULE_LICENSE("GPL");
