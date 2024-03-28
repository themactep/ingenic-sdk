#ifndef SENSOR_INFO_H
#define SENSOR_INFO_H

struct sensor_info {
	const char *name;
	unsigned int chip_id;
	const char *version;
	int min_fps;
	int max_fps;
	unsigned int chip_i2c_addr;
	int width;
	int height;
};

void sensor_common_init(struct sensor_info *info);
void sensor_common_exit(void);

#endif // SENSOR_INFO_H
