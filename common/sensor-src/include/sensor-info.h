#ifndef SENSOR_INFO_H
#define SENSOR_INFO_H

struct sensor_info {
	const char *name;
	unsigned int chip_id;
	const char *version;
	int min_fps;
	int max_fps;
	/*
	 * Legacy actual-FPS field, still populated by the 3.10.14 sensor drivers
	 * through sensor_update_actual_fps(). Kept so one struct serves both
	 * sensor driver generations.
	 */
	int actual_fps;
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
void sensor_common_update(struct sensor_info *info, int rst_gpio, int pwdn_gpio,
			  int boot, int mclk, int video_interface,
			  int i2c_adapter);
void sensor_common_exit(void);
/*
 * 3.10.14 sensor drivers report the achieved FPS through this call. It is a
 * no-op for multi-sensor setups but must exist for those drivers to link.
 */
void sensor_update_actual_fps(int fps);

#endif // SENSOR_INFO_H
