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

#define SENSOR_MIN_FPS      5
#define CV3001_CHIP_ID_L	0x01
#define CV3001_CHIP_ID_H	0x30
#define CV3001_REG_END		0xffff
#define CV3001_REG_DELAY	0xfffe
#define CV3001_AGAIN_MAX	0xB4
#define SENSOR_VERSION	"H20230512a"

static int reset_gpio = GPIO_PA(18);
static int pwdn_gpio = -1;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct tx_isp_sensor_attribute cv3001_attr;


unsigned int cv3001_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	uint16_t again=(isp_gain*20)>>shift;
	if(again>CV3001_AGAIN_MAX) again=CV3001_AGAIN_MAX;
	*sensor_again = again;
	isp_gain= (((int32_t)again)<<shift)/20;

	return isp_gain;
}

unsigned int cv3001_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

unsigned int cv3001_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	*sensor_it = it;

	return it;
}


struct tx_isp_mipi_bus cv3001_mipi_linear = {
   .mode = SENSOR_MIPI_OTHER_MODE,
   .clk = 1080,
   .lans = 2,
   .settle_time_apative_en = 0,
   .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,	//RAW
   .mipi_sc.hcrop_diff_en = 0,
   .mipi_sc.mipi_vcomp_en = 0,
   .mipi_sc.mipi_hcomp_en = 0,
   .mipi_sc.line_sync_mode = 0,
   .mipi_sc.work_start_flag = 0,
   .image_twidth = 2304,
   .image_theight = 1296,
   .mipi_sc.mipi_crop_start0x = 0,
   .mipi_sc.mipi_crop_start0y = 0,
   .mipi_sc.mipi_crop_start1x = 0,
   .mipi_sc.mipi_crop_start1y = 0,
   .mipi_sc.mipi_crop_start2x = 0,
   .mipi_sc.mipi_crop_start2y = 0,
   .mipi_sc.mipi_crop_start3x = 0,
   .mipi_sc.mipi_crop_start3y = 0,
   .mipi_sc.data_type_en = 0,
   .mipi_sc.data_type_value = RAW10,	//RAW
   .mipi_sc.del_start = 0,
   .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
   .mipi_sc.sensor_fid_mode = 0,
   .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute cv3001_attr={
	.name = "cv3001",
	.chip_id = 0x3001,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x35,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 589824,
	.max_dgain = 0,
	.min_integration_time = 4,
	.max_integration_time = 2772, /* 2780 - 8 */
	.min_integration_time_native = 4,
	.max_integration_time_native = 2772,
	.integration_time_limit = 2772,
	.total_width = 648,
	.total_height = 2780,
	.one_line_expr_in_us = 33,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = cv3001_alloc_again,
	.sensor_ctrl.alloc_again_short = cv3001_alloc_again,
	.sensor_ctrl.alloc_dgain = cv3001_alloc_dgain,
	.sensor_ctrl.alloc_integration_time = cv3001_alloc_integration_time,
};

static struct regval_list cv3001_init_regs_mipi[] = {
	{0x301D, 0x00},
	{0x3024, 0xDC},//vts 0xADC = 2780
	{0x3025, 0x0A},
	{0x3026, 0x00},
	{0x3028, 0x88},// hts 0x288 = 648
	{0x3029, 0x02},//
	{0x3032, 0x00},
	{0x3033, 0x00},
	{0x3300, 0x00},
	{0x3401, 0x01},
	{0x343E, 0x00},
	{0x3806, 0x01},
	{0x3908, 0x2D},
	{0x3909, 0x00},
	{0x390A, 0x02},
	{0x3034, 0x01},
	{0x3038, 0x04},
	{0x3039, 0x00},
	{0x303A, 0x10},
	{0x303B, 0x05},
	{0x303C, 0x04},
	{0x303D, 0x00},
	{0x303E, 0x00},
	{0x303F, 0x09},
	{0x3338, 0x00},
	{0x35B3, 0x0A},
	{0x3168, 0x64},
	{0x3871, 0x00},
	{CV3001_REG_END, 0x00},	/* END MARKER */
};


static struct tx_isp_sensor_win_setting cv3001_win_sizes[] = {
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10, //RAW
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= cv3001_init_regs_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &cv3001_win_sizes[0]; //默认线性模式

static struct regval_list cv3001_stream_on_mipi[] = {
	{0x3000, 0x00},
	{CV3001_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list cv3001_stream_off_mipi[] = {
	{0x3000, 0x01},
	{CV3001_REG_END, 0x00},	/* END MARKER */
};

int cv3001_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
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

int cv3001_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int cv3001_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != CV3001_REG_END) {
		if (vals->reg_num == CV3001_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv3001_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int cv3001_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != CV3001_REG_END) {
		if (vals->reg_num == CV3001_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv3001_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int cv3001_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int cv3001_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = cv3001_read(sd, 0x3002, &v);
	ISP_WARNING("%s: ret = %d, v = 0x%02x\n", __func__, ret, v);
	if (ret < 0)
		return ret;
	if (v != CV3001_CHIP_ID_L)
		return -ENODEV;

	ret += cv3001_read(sd, 0x3003, &v);
	ISP_WARNING("%s: ret = %d, v = 0x%02x\n", __func__, ret, v);
	if (ret < 0)
		return ret;
	if (v != CV3001_CHIP_ID_H)
		return -ENODEV;

	*ident = v;

	return ret;
}

#if 0
static int cv3001_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = value & 0xffff;
	unsigned short exp;
	unsigned int vmax = 1125;
	int again = (value >> 16) & 0x0000ffff;

	vmax = cv3001_attr.total_height;
	exp = vmax - it;
	exp0 = (exp0 >> 1) << 1;
	ret += cv3001_write(sd, 0x3058, (unsigned char)(exp & 0xff));
	ret += cv3001_write(sd, 0x3059, (unsigned char)((exp >> 8) & 0xff));
	ret += cv3001_write(sd, 0x305A, (unsigned char)((exp >> 16) & 0x0f));
	ret += cv3001_write(sd, 0x3160, (unsigned char)(again & 0xff));

	ISP_WARNING("cv3001 set exp=0x%04x(%4d line) gain=0x%02x\n", exp, it, again);

	return ret;
}
#endif

static int cv3001_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = value & 0xffff;
	unsigned short exp;
	unsigned int vmax = 1125;

	vmax = cv3001_attr.total_height;
	exp = vmax - it;
	exp = (exp >> 1) << 1;
	ret += cv3001_write(sd, 0x3058, (unsigned char)(exp & 0xff));
	ret += cv3001_write(sd, 0x3059, (unsigned char)((exp >> 8) & 0xff));
	ret += cv3001_write(sd, 0x305A, (unsigned char)((exp >> 16) & 0x0f));

	//ISP_WARNING("cv3001 set exp=0x%04x(%4d line)\n", exp0, it);

	return ret;
}


static int cv3001_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned char again = value & 0xff;

	ret += cv3001_write(sd, 0x3160, (unsigned char)(again & 0xff));

	//ISP_WARNING("cv3001 set gain0=0x%02x(%03d)\n", again, again);

	return ret;
}


static int cv3001_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv3001_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv3001_init(struct tx_isp_subdev *sd, int enable)
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
	ret = cv3001_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	ISP_WARNING("cv3001 init\n");

	return 0;
}

static int cv3001_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = cv3001_write_array(sd, cv3001_stream_on_mipi);
		ISP_WARNING("cv3001 stream on\n");

	}
	else {
		ret = cv3001_write_array(sd, cv3001_stream_off_mipi);
		ISP_WARNING("cv3001 stream off\n");
	}

	return ret;
}

#if 1
static int cv3001_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	printk("\nfps = 0x%x\n",fps);
	sclk = 54043200; /* 648 * 2780 * 30 */
	max_fps = TX_SENSOR_MAX_FPS_30;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret += cv3001_read(sd, 0x3029, &val);
	hts = val;
	val = 0;
	ret += cv3001_read(sd, 0x3028, &val);
	hts = ((hts << 8) | val);

	printk("\nhts = %d\n",hts);
	if (0 != ret) {
		ISP_ERROR("err: cv3001 read err\n");
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	printk("\nvts = %d\n",vts);
	ret = cv3001_write(sd, 0x3024, (unsigned char)(vts & 0xff));
	ret += cv3001_write(sd, 0x3025, (unsigned char)((vts >> 8)& 0xff));
	ret += cv3001_write(sd, 0x3026, (unsigned char)((vts >> 16)& 0x0f));

	if (0 != ret) {
		ISP_ERROR("err: cv3001_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif


#if 1
static int cv3001_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;

	ret = cv3001_read(sd, 0x3030, &val);
	switch(enable) {
	case 0:
		val &= 0xFC;
		break;
	case 1:
		val &= 0xFC;
		val |= 0x02;
		break;
	};
	ret = cv3001_write(sd, 0x3030, val);

	return 0;
}
#endif


static int cv3001_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize)
	{
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	ISP_WARNING("cv3001 set mode\n");

	return ret;
}

static int cv3001_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if (reset_gpio != -1)
	{
		ret = private_gpio_request(reset_gpio, "cv3001_reset");
		if (!ret)
		{
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1)
	{
		ret = private_gpio_request(pwdn_gpio, "cv3001_pwdn");
		if (!ret)
		{
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
		}
	}
	ret = cv3001_detect(sd, &ident);
	if (ret)
	{
		ISP_ERROR("chip found @ 0x%x (%s) is not an cv3001 chip.\n", client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("cv3001 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if (chip)
	{
		memcpy(chip->name, "cv3001", sizeof("cv3001"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}


static int cv3001_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		// if(arg)
	    //  	ret = cv3001_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = cv3001_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = cv3001_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = cv3001_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = cv3001_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = cv3001_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = cv3001_write_array(sd, cv3001_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = cv3001_write_array(sd, cv3001_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		 if(arg)
		 	ret = cv3001_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		 if(arg)
		 	ret = cv3001_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int cv3001_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = cv3001_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int cv3001_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	cv3001_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops cv3001_core_ops = {
	.g_chip_ident = cv3001_g_chip_ident,
	.reset = cv3001_reset,
	.init = cv3001_init,
	/*.ioctl = cv3001_ops_ioctl,*/
	.g_register = cv3001_g_register,
	.s_register = cv3001_s_register,
};

static struct tx_isp_subdev_video_ops cv3001_video_ops = {
	.s_stream = cv3001_s_stream,
};

static struct tx_isp_subdev_sensor_ops	cv3001_sensor_ops = {
	.ioctl	= cv3001_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops cv3001_ops = {
	.core = &cv3001_core_ops,
	.video = &cv3001_video_ops,
	.sensor = &cv3001_sensor_ops,
};

static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "cv3001",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int cv3001_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
	{
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk))
	{
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);	//MCLK 24MHz
	private_clk_enable(sensor->mclk);

	wsize = &cv3001_win_sizes[0];
	memcpy(&(cv3001_attr.mipi), &cv3001_mipi_linear, sizeof(cv3001_mipi_linear));

	sd = &sensor->sd;
	video = &sensor->video;
	cv3001_attr.expo_fs = 1;
	cv3001_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
	sensor->video.attr = &cv3001_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &cv3001_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("---- cv3001 probe ok ----\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int cv3001_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id cv3001_id[] = {
	{ "cv3001", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cv3001_id);

static struct i2c_driver cv3001_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "cv3001",
	},
	.probe		= cv3001_probe,
	.remove		= cv3001_remove,
	.id_table	= cv3001_id,
};

static __init int init_cv3001(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init cv3001 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&cv3001_driver);
}

static __exit void exit_cv3001(void)
{
	private_i2c_del_driver(&cv3001_driver);
}

module_init(init_cv3001);
module_exit(exit_cv3001);

MODULE_DESCRIPTION("A low-level driver for CV3001 sensors");
MODULE_LICENSE("GPL");
