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
	int rst_gpio;
	int pwdn_gpio;        /* power-down GPIO (-1 = not used) */
	int boot;             /* boot mode (0=linear, 1=WDR, 2=60fps crop) */
	int mclk;             /* clock source (0=MCLK0, 1=MCLK1, 2=MCLK2) */
	int video_interface;  /* 0=MIPI, 1=DVP */
	int i2c_adapter;      /* I2C bus number */
	void *priv;           /* Private data for proc context */
};

void sensor_common_init(struct sensor_info *info);
void sensor_common_exit(void);

#endif // SENSOR_INFO_H
