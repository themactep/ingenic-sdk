// SPDX-License-Identifier: GPL-2.0
/*
 * Test fixture for tools/check_sensor_modes.py. Not a driver: never built.
 *
 * Both functions leave through a goto. The probe ends in a "goto out" that
 * does nothing, and set_fps() jumps over its error path. The mode has the
 * pull request 13 pattern: the table programs 1200 lines (0x04b0), the
 * attributes report 1500, and sclk / hts / fps gives 1200.
 */
#define SENSOR_NAME "fx_goto"
#define SENSOR_REG_END 0xffff
#define SENSOR_SUPPORT_SCLK (81000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5

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

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.total_width = 2250,
	.total_height = 1500,
	.max_integration_time = 1496,
	.max_integration_time_native = 1496,
	.integration_time_limit = 1496,
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
	goto done;

fail:
	ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
	return ret;

done:
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
	goto out;

out:
	return 0;
}
