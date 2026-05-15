#ifndef __SENSOR_INFO__
#define __SENSOR_INFO__


struct i2c_trans {
	uint32_t addr;
	uint32_t r_w;
	uint32_t data;
	uint32_t datalen;
};

uint8_t *sclk_name;

typedef struct SENSOR_INFO_S
{
	uint8_t *name;
	uint8_t i2c_addr;
	uint8_t *mclk_name;
	uint32_t clk;

	uint32_t id_value[8];
	uint32_t id_value_len;
	uint32_t id_addr[8];
	uint32_t id_addr_len;
	uint8_t id_cnt;

	struct i2c_adapter *adap;
} SENSOR_INFO_T, *SENSOR_INFO_P;

#define I2C_WRITE 0
#define I2C_READ  1

#define SENSOR_INFO_IOC_MAGIC  'S'
#define IOCTL_SINFO_GET			_IO(SENSOR_INFO_IOC_MAGIC, 100)
#define IOCTL_SINFO_FLASH		_IO(SENSOR_INFO_IOC_MAGIC, 101)


#endif
