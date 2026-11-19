// SPDX-License-Identifier: GPL-2.0
/*
 * Test fixture for tools/check_sensor_modes.py. Not a driver: never built.
 *
 * Two kinds of default: branch in the probe. The one on sensor_max_fps does
 * real work (every rate but 30 runs the 15 fps table), so it is a mode. The
 * one on data_interface only logs, so the DVP setting is a value the driver
 * refuses. Both modes are consistent: 1200 lines at 30 fps, 2400 at 15.
 */
#define SENSOR_NAME "fx_default"
#define SENSOR_REG_END 0xffff
#define SENSOR_SUPPORT_SCLK (81000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	{0x320c, 0x08},
	{0x320d, 0xca},
	{0x320e, 0x04},
	{0x320f, 0xb0},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_15fps_mipi[] = {
	{0x320c, 0x08},
	{0x320d, 0xca},
	{0x320e, 0x09},
	{0x320f, 0x60},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 15 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_15fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.total_width = 2250,
	.total_height = 1200,
	.max_integration_time = 1196,
	.max_integration_time_native = 1196,
	.integration_time_limit = 1196,
};

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int clk = SENSOR_SUPPORT_SCLK;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0;
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
		return -1;

	ret = sensor_read(sd, 0x320c, &val);
	hts = val;
	ret += sensor_read(sd, 0x320d, &val);
	hts = ((hts << 8) + val);
	vts = clk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	return 0;
}

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_30:
		wsize = &sensor_win_sizes[0];
		break;
	default:
		wsize = &sensor_win_sizes[1];
		sensor_attr.total_height = 2400;
		sensor_attr.max_integration_time = 2396;
		sensor_attr.max_integration_time_native = 2396;
		sensor_attr.integration_time_limit = 2396;
		break;
	}

	switch (data_interface) {
	case TX_SENSOR_DATA_INTERFACE_MIPI:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		break;
	default:
		ISP_ERROR("%s: this interface is not supported\n", SENSOR_NAME);
	}

	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP)
		ISP_WARNING("%s: DVP was asked for\n", SENSOR_NAME);

	sensor->video.attr = &sensor_attr;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.fps = wsize->fps;
	return 0;
}
