// SPDX-License-Identifier: GPL-2.0+
/*
 * n5.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
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
#include <txx-funcs.h>

#define SENSOR_NAME "n5"
#define SENSOR_CHIP_ID_H (0x00)
#define SENSOR_CHIP_ID_L (0x00)
#define SENSOR_REG_END 0xfe
#define SENSOR_REG_DELAY 0x00
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20221025a"

static int reset_gpio = GPIO_PB(6);
static int pwr_gpio = GPIO_PA(24);
static int sensor_gpio_func = DVP_PA_LOW_8BIT;
static int data_interface = TX_SENSOR_DATA_INTERFACE_DVP;
static int sensor_max_fps = 5; //TX_SENSOR_MAX_FPS_20;

static int shvflip = 0;
struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

#if 1
struct tx_isp_dvp_bus sensor_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
	.polar = {
		.hsync_polar = 0,
		.vsync_polar = 0,
	},
	.dvp_hcomp_en = 1,
};
#endif

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x2145,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x32,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 444864,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
};

static struct regval_list sensor_init_regs_1920_1080_25fps_dvp[] = {
	{0xff, 0x00},
	{0x00, 0x10},
	{0x01, 0x10},
	{0x18, 0x3f},
	{0x19, 0x3f},
	{0x22, 0x0b},
	{0x23, 0x71},
	{0x26, 0x0b},
	{0x27, 0x71},
	{0x54, 0x00},
	{0xa0, 0x05},
	{0xa1, 0x05},
	{0xff, 0x05},
	{0x00, 0xf0},
	{0x01, 0x22},
	{0x05, 0x04},
	{0x08, 0x55},
	{0x1b, 0x08},
	{0x25, 0xdc},
	{0x28, 0x80},
	{0x2f, 0x00},
	{0x30, 0xe0},
	{0x31, 0x43},
	{0x32, 0xa2},
	{0x57, 0x00},
	{0x58, 0x77},
	{0x5b, 0x41},
	{0x5c, 0x78},
	{0x5f, 0x00},
	{0x7b, 0x11},
	{0x7c, 0x01},
	{0x7d, 0x80},
	{0x80, 0x00},
	{0x90, 0x01},
	{0xa9, 0x00},
	{0xb5, 0x00},
	{0xb9, 0x72},
	{0xd1, 0x00},
	{0xd5, 0x80},
	{0xff, 0x06},
	{0x00, 0xf0},
	{0x01, 0x22},
	{0x05, 0x04},
	{0x08, 0x55},
	{0x1b, 0x08},
	{0x25, 0xdc},
	{0x28, 0x80},
	{0x2f, 0x00},
	{0x30, 0xe0},
	{0x31, 0x43},
	{0x32, 0xa2},
	{0x57, 0x00},
	{0x58, 0x77},
	{0x5b, 0x41},
	{0x5c, 0x78},
	{0x5f, 0x00},
	{0x7b, 0x11},
	{0x7c, 0x01},
	{0x7d, 0x80},
	{0x80, 0x00},
	{0x90, 0x01},
	{0xa9, 0x00},
	{0xb5, 0x00},
	{0xb9, 0x72},
	{0xd1, 0x00},
	{0xd5, 0x80},
	{0xff, 0x09},
	{0x50, 0x30},
	{0x51, 0x6f},
	{0x52, 0x67},
	{0x53, 0x48},
	{0x54, 0x30},
	{0x55, 0x6f},
	{0x56, 0x67},
	{0x57, 0x48},
	{0x96, 0x00},
	{0x9e, 0x00},
	{0xb6, 0x00},
	{0xbe, 0x00},
	{0xff, 0x0a},
	{0x25, 0x10},
	{0x27, 0x1e},
	{0x30, 0xac},
	{0x31, 0x78},
	{0x32, 0x17},
	{0x33, 0xc1},
	{0x34, 0x40},
	{0x35, 0x00},
	{0x36, 0xc3},
	{0x37, 0x0a},
	{0x38, 0x00},
	{0x39, 0x02},
	{0x3a, 0x00},
	{0x3b, 0xb2},
	{0xa5, 0x10},
	{0xa7, 0x1e},
	{0xb0, 0xac},
	{0xb1, 0x78},
	{0xb2, 0x17},
	{0xb3, 0xc1},
	{0xb4, 0x40},
	{0xb5, 0x00},
	{0xb6, 0xc3},
	{0xb7, 0x0a},
	{0xb8, 0x00},
	{0xb9, 0x02},
	{0xba, 0x00},
	{0xbb, 0xb2},
	{0x77, 0x8F},
	{0xF7, 0x8F},
	{0xff, 0x13},
	{0x07, 0x47},
	{0x12, 0x04},
	{0x1e, 0x1f},
	{0x1f, 0x27},
	{0x2e, 0x10},
	{0x2f, 0xc8},
	{0x30, 0x00},
    {0x31, 0xff},
    {0x32, 0x00},
    {0x33, 0x00},
    {0x3a, 0xff},
    {0x3b, 0xff},
    {0x3c, 0xff},
    {0x3d, 0xff},
    {0x3e, 0xff},
    {0x3f, 0x0f},
    {0x70, 0x00},
    {0x72, 0x05},
    {0x7A, 0xf0},
	{0xff, 0x01},
	{0x97, 0x00},
	{0x97, 0x0f},
	{0x7A, 0x0f},
	{0xff, 0x00}, //8x8 color block test pattern
	{0x78, 0xba},
	{0xff, 0x05},
	{0x2c, 0x08},
	{0x6a, 0x80},
	{0xff, 0x06},
	{0x2c, 0x08},
	{0x6a, 0x80},
};


int sensor_write(struct tx_isp_subdev *sd, unsigned char reg, unsigned char value);
int sensor_read(struct tx_isp_subdev *sd, unsigned char reg,unsigned char *value);

void sensor_set_chnmode_1080p_25(struct tx_isp_subdev *sd, unsigned char chn)
{
    unsigned char reg_0x54;
    unsigned char reg_0xED;
	printk("=====> sensor_set_chnmode_1080p_25\n");
	sensor_write(sd, 0xff, 0x00);
	sensor_write(sd, 0x08+chn, 0x00);
	sensor_write(sd, 0x34+chn, 0x00);
	sensor_write(sd, 0x81+chn, 0x03);
	sensor_write(sd, 0x85+chn, 0x00);
	sensor_read(sd, 0x54, &reg_0x54);
	sensor_write(sd, 0x54, reg_0x54 &(~(0x10<<chn)));
	sensor_write(sd, 0x18+chn, 0x3f);
	sensor_write(sd, 0x58+chn, 0x78);
	sensor_write(sd, 0x5c+chn, 0x80);
	sensor_write(sd, 0x64+chn, 0x3f);
	sensor_write(sd, 0x89+chn, 0x10);
	sensor_write(sd, chn+0x8e, 0x00);
	sensor_write(sd, 0x30+chn, 0x12);
	sensor_write(sd, 0xa0+chn, 0x05);

	sensor_write(sd, 0xff, 0x01);
	sensor_write(sd, 0x84+chn, 0x00);
	sensor_write(sd, 0x8c+chn, 0x40);
	sensor_read(sd, 0xed, &reg_0xED);
	sensor_write(sd, 0xed, reg_0xED &(~(0x01<<chn)));

	sensor_write(sd, 0xff, 0x05+chn);
	sensor_write(sd, 0x20, 0x84);
	sensor_write(sd, 0x25, 0xdc);
	sensor_write(sd, 0x28, 0x80);
	sensor_write(sd, 0x2b, 0xa8);
	sensor_write(sd, 0x47, 0x04);
	sensor_write(sd, 0x50, 0x84);
	sensor_write(sd, 0x58, 0x77);
	sensor_write(sd, 0x69, 0x00);
	sensor_write(sd, 0x7b, 0x11);
	sensor_write(sd, 0xb8, 0x39);
	sensor_write(sd, 0xff, 0x09);
	sensor_write(sd, 0x96+chn*0x20, 0x00);
	sensor_write(sd, 0x97+chn*0x20, 0x00);
	sensor_write(sd, 0x98+chn*0x20, 0x00);
}

static void my_set_port_mode_1mux(struct tx_isp_subdev *sd, unsigned char port, unsigned char regCC) {
    unsigned char reg_0x54;
    unsigned char reg_1xC8;
    unsigned char reg_1xCA;

	printk("=================> my_set_port_mode_1mux\n");
    sensor_write(sd, 0xFF, 0x00); //bank
    sensor_read(sd, 0x54, &reg_0x54);
    sensor_write(sd, 0x54, reg_0x54 & 0xFE);
    sensor_write(sd, 0xFF, 0x01); //bank 1
    sensor_read(sd, 0xC8, &reg_1xC8);
    sensor_write(sd, 0xA0, 0x00);
    sensor_write(sd, 0xC0, 0x00);
    sensor_write(sd, 0xC1, 0x00);
    reg_1xC8 &= ( ==port?0x0F:0xF0);
    sensor_write(sd, 0xC8, reg_1xC8); //
    sensor_write(sd, 0xCC, regCC);

    sensor_write(sd, 0xA8+port, 0x90+(port*0x10)); //h/v0 sync enabled
	sensor_write(sd, 0xB3, 0x01);

    sensor_write(sd, 0xE4, 0x00);
    sensor_write(sd, 0xE5, 0x00);

    sensor_read(sd, 0xCA, &reg_1xCA);
    reg_1xCA |= (0x11<<port);
    sensor_write(sd, 0xCA, reg_1xCA);
}

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* [0] 1920*1080 @ max 25fps dvp*/
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_YUYV8_2X8,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_dvp,
	}
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char *value)
{
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

//	printk("	{0x%x,0x%x}\n",*(msg[0].buf), *(msg[1].buf));
//	private_msleep(5);
	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, unsigned char reg,
		 unsigned char value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;
//	printk("	{0x%x,0x%x}\n",buf[0], buf[1]);
//	private_msleep(5);
	return ret;
}

#if 0
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals, int len)
{
	int ret;
//	unsigned char val;
	while (len --) {
		ret = sensor_write(sd, vals->reg_num, vals->value);
//		ret = sensor_read(sd, vals->reg_num, &val);
//		printk("	{0x%x,0x%x}\n", vals->reg_num, val);
		if (ret < 0)
			return ret;
		vals++;
	}

	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x08, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	ret = sensor_read(sd, 0x09, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			ret = sensor_write_array(sd, wsize->regs, sizeof(sensor_init_regs_1920_1080_25fps_dvp) / sizeof(sensor_init_regs_1920_1080_25fps_dvp[0]));
			sensor_set_chnmode_1080p_25(sd, 0);
			my_set_port_mode_1mux(sd, 0 , 0x43);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			printk("%s stream on\n", SENSOR_NAME));
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		printk("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned long rate;
	int ret = 0;

	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		memcpy((void*)(&(sensor_attr.dvp)),(void*)(&sensor_dvp),sizeof(sensor_dvp));
//		memcpy((void*)(&(sensor_attr.bt601bus)),(void*)(&sensor_bt601),sizeof(sensor_bt601));
		sensor_max_fps = 5; //TX_SENSOR_MAX_FPS_25;
		ret = set_sensor_gpio_function(sensor_gpio_func);
		if (ret < 0)
		goto err_set_sensor_gpio;
		sensor_attr.dvp.gpio = sensor_gpio_func;
//		sensor_attr.bt601bus.gpio = SENSOR_BT_8BIT,
		sensor_attr.max_integration_time_native = 1080;
		sensor_attr.integration_time_limit = 1080;
		sensor_attr.total_width = 1920;
		sensor_attr.total_height = 1080;
		sensor_attr.max_integration_time = 1080;
		printk("---------------> default_boot 0 is ok <--------------\n");
		break;
	default:
		ISP_ERROR("this init boot is not supported yet!!!\n");
	}

	switch(info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		data_interface = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("this data interface is not supported yet!!!\n");
	}

	switch(info->mclk) {
	case TISP_SENSOR_MCLK0:
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
		set_sensor_mclk_function(1);
		break;
	case TISP_SENSOR_MCLK2:
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
		set_sensor_mclk_function(2);
		break;
	default:
		ISP_ERROR("this MCLK Source is not supported yet!!!\n");
	}

	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
//	pwr_gpio = info->pwr_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_set_sensor_gpio:
err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
//	private_jzgpio_set_func(GPIO_PORT_A, GPIO_OUTPUT1, 0x01000000);
#if 1
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
#endif
#if 0
	if (pwr_gpio != -1) {
		ret = private_gpio_request(pwr_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwr_gpio, 1);
			private_msleep(20);
			private_gpio_direction_output(pwr_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(pwr_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",pwr_gpio);
		}
	}
#endif
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
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

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
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
static u64 tx_isp_module_dma_mask = ~(u64)0;
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}

	private_gpio_direction_output(pwr_gpio, 1);

	memset(sensor, 0 ,sizeof(*sensor));
	sensor_attr.expo_fs = 1;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwr_gpio != -1)
		private_gpio_free(pwr_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);

	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
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

static __init int init_sensor(void)
{
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
