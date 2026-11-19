// SPDX-License-Identifier: GPL-2.0
/*
 * Test fixture for tools/check_sensor_modes.py. Not a driver: never built.
 *
 * A file that does not compile: the condition in sensor_probe() is missing
 * its closing parenthesis. The script must report it as not analysed, and a
 * run with --fail-on must fail on it.
 */
#define SENSOR_NAME "fx_broken"

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.total_height = 1200,
};

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if ((!sensor) {
		return -ENOMEM;
	}
	sensor->video.attr = &sensor_attr;
	return 0;
}
