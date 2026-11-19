// SPDX-License-Identifier: GPL-2.0
/*
 * Test fixture for tools/check_sensor_modes.py. Not a driver: never built.
 *
 * Preprocessor conditions with C operators the first version of the script
 * got wrong: & and / choose the right total_height here. One condition uses
 * a function-like macro, which the script does not read and must say so.
 * The mode is consistent (1200 lines); the init table's name states another
 * size than the window, which is the only finding, a low one.
 */
#define SENSOR_NAME "fx_ppif"
#define SENSOR_REG_END 0xffff
#define SENSOR_SUPPORT_SCLK (81000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define FX_FLAGS 3
#define FX_LINES (FX_FLAGS + 1197)

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

static struct regval_list sensor_init_regs_1280_720_30fps_mipi[] = {
	{0x320c, 0x08},
	{0x320d, 0xca},
	{0x320e, 0x04},
	{0x320f, 0xb0},
#if FX_VERSION(4, 4, 0) > 1
	{0x320e, 0x7f},
#endif
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1280_720_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.total_width = 2250,
#if (FX_FLAGS & 1) && FX_LINES / 2 == 600
	.total_height = 1200,
#else
	.total_height = 1500,
#endif
#if FX_FLAGS % 2 == 0 || defined(FX_NOT_DEFINED)
	.max_integration_time = 2999,
#elif (FX_FLAGS ^ 1) == 2
	.max_integration_time = 1196,
#endif
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
	sensor->video.attr = &sensor_attr;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.fps = wsize->fps;
	return 0;
}
