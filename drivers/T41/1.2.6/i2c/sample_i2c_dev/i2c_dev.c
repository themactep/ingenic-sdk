#include <stdio.h>
#include <linux/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

static int g_i2c_fd = 0;
struct i2c_rdwr_ioctl_data mcu_data;

#define I2C_0

#define CHIP_ID_H 0x0f
#define CHIP_ID_L 0x22

#define CHIP_ID_H_REG 0x0a
#define CHIP_ID_L_REG 0x0b

#ifdef I2C_0
#define I2C_DEV			"/dev/i2c-0"
#define I2C_DEV_ADDR	0x40 //JXF23 ADDR
#define DATA_ADDR		0xc1
#endif
#ifdef I2C_1
#define I2C_DEV			"/dev/i2c-1"
#define I2C_DEV_ADDR	0x50 //EEPROM ADDR
#define DATA_ADDR       0x50
#endif

int i2c_init(void)
{
	int fd = 0;

	fd = open(I2C_DEV, O_RDWR);

	ioctl(fd,I2C_TIMEOUT,1);/*超时时间*/
	ioctl(fd,I2C_RETRIES,2);/*重复次数*/

	if (fd < 0) {
		printf("open %s failed:%s\n", I2C_DEV, strerror(errno));
		return -1;
	}

	g_i2c_fd = fd;

	mcu_data.nmsgs = 2;
	mcu_data.msgs = (struct i2c_msg*)malloc(mcu_data.nmsgs*sizeof(struct i2c_msg));

	if (!mcu_data.msgs) {
		printf("Allocation failed:%s\n", strerror(errno));
		close(fd);
		g_i2c_fd = 0;
		return -1;
	}

	return 0;
}

int i2c_exit(void)
{
	free(mcu_data.msgs);
	close(g_i2c_fd);
	g_i2c_fd = 0;

	return 0;
}

#ifdef I2C_0
unsigned char read_buffer[1];
unsigned char write_buffer[2];
#endif
#ifdef I2C_1
unsigned char read_buffer[8];
unsigned char write_buffer[9];
#endif

int i2c_write(uint32_t reg, int reg_size, uint32_t value, int value_size)
{
	int ret = 0;
	int dev_node = g_i2c_fd;
	int i = 0;

	mcu_data.nmsgs = 1;

	mcu_data.msgs[0].len = sizeof(write_buffer);
	mcu_data.msgs[0].addr = I2C_DEV_ADDR;
	mcu_data.msgs[0].flags = 0;
	mcu_data.msgs[0].buf= write_buffer;

	write_buffer[0] = DATA_ADDR; //data address

	for(i = 1; i < sizeof(write_buffer); i++)
		write_buffer[i] = i+32;

	printf("eeprom write addr: %d\ndata have:\n",write_buffer[0]);
	for(i = 1; i < sizeof(write_buffer); i++)
		printf("# %d\n", write_buffer[i]);
	ret = ioctl(dev_node, I2C_RDWR, (unsigned long)&mcu_data);

	if(ret < 0) {
		printf("write data error:%s\n", strerror(errno));
		exit(1);
	}

	return 0;
}

int i2c_read(uint32_t reg, int reg_size, uint32_t *value, int value_size)
{
	int ret = 0, i = 0;
	int dev_node = g_i2c_fd;
	uint8_t *trans_buf;

	memset(read_buffer, 0, sizeof(read_buffer));
	mcu_data.nmsgs = 2;

	(mcu_data.msgs[0]).buf = write_buffer;
	(mcu_data.msgs[0]).len = 1;
	(mcu_data.msgs[0]).addr = I2C_DEV_ADDR;
	(mcu_data.msgs[0]).flags = 0;

	(mcu_data.msgs[1]).buf = read_buffer;
	(mcu_data.msgs[1]).len = sizeof(read_buffer);
	(mcu_data.msgs[1]).addr = I2C_DEV_ADDR;
	(mcu_data.msgs[1]).flags = I2C_M_RD;


	ret = ioctl(dev_node, I2C_RDWR, (unsigned long)&mcu_data);

	if(ret < 0) {
		printf("I2C: read error\n");
		return ret;
	}

	printf("eeprom read addr: %d\ndata have:\n",write_buffer[0]);
	for(i = 0; i < sizeof(read_buffer); i++)
		printf("# %d\n", read_buffer[i]);


	return value_size;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int value = 0;


	ret = i2c_init();
	if(ret){
		printf("Failed to i2c_init!!!\n");
		return -1;
	}

#if 1
	ret = i2c_write(0, 1, value, 1);
	if(ret < 0){
		printf("Failed to read CHIP_ID_H_REG!!!\n");
		goto exit;
	}
	sleep(3);
#endif

	ret = i2c_read(0, 1, &value, 1);
	if(ret < 0){
		printf("Failed to read CHIP_ID_H_REG!!!\n");
		goto exit;
	}

	i2c_exit();
	printf("Test ok!!!\n");
	return 0;

exit:
	i2c_exit();
	printf("Test failed!!!");
	return 0;
}
