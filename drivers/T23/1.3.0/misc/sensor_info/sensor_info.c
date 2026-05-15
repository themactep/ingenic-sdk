/*
 * sensor_info.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <jz_proc.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <soc/gpio.h>
#include "sensor_info.h"

static unsigned i2c_adapter_nr = 0;
module_param(i2c_adapter_nr, uint, 0644);
MODULE_PARM_DESC(i2c_adapter_nr, "sensor used i2c_adapter nr");

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int g_sensor_num = 0;

#define SENSOR_TYPE_INVALID	-1

//#define I2C_READ_WRITE 1   //to do i2c read and write
//#define CONCTL_SENSOR_OPENCLOSE 1  //to do sensor enable and disable

#ifdef CONFIG_KERNEL_4_4_94
SENSOR_INFO_T g_sinfo[] =
{
    {"gc2083",  0x37,  "div_cim", 24000000, {0x20, 0x83}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
    {"sc535IoT",  0x30,  "div_cim", 24000000, {0xce, 0x78}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc1084",  0x37,  "div_cim", 24000000, {0x10, 0x84}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
    {"jxf38p",  0x40,  "div_cim", 24000000, {0x08, 0x44}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc202cs",  0x10,  "div_cim", 24000000, {0xeb, 0x52}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc2053",  0x37,  "div_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
    {"sc3332",  0x30,  "div_cim", 24000000, {0xcc, 0x44}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc2331",  0x30,  "div_cim", 24000000, {0xcb, 0x5c}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc3003a",  0x37,  "div_cim", 24000000, {0x30, 0x03, 0x10}, 1, {0x03f0, 0x03f1, 0x036a}, 2, 3, NULL},
    {"cv4002",  0x35,  "div_cim", 24000000, {0x02, 0x40}, 1, {0x3002, 0x3003}, 2, 2, NULL},
    {"sc200ai",  0x30,  "div_cim", 24000000, {0xcb, 0x1c}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxk306p",  0x40,  "div_cim", 24000000, {0x08, 0x60}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"jxh63p",  0x40,  "div_cim", 24000000, {0x08, 0x48}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc3336p",  0x30,  "div_cim", 24000000, {0x9c, 0x41}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"cv2003",  0x35,  "div_cim", 24000000, {0x20, 0x03}, 1, {0x3011, 0x3138}, 2, 2, NULL},
    {"jxf28p",  0x40,  "div_cim", 24000000, {0x08, 0x50}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc1a4t",  0x30,  "div_cim", 24000000, {0x9a, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf57",  0x46,  "div_cim", 24000000, {0x08, 0x75}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc223a",  0x30,  "div_cim", 24000000, {0xcb, 0x3e}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxq03p",  0x40,  "div_cim", 24000000, {0x08, 0x43}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"imx327",  0x36,  "div_cim", 37125000, {0xb2, 0x01}, 1, {0x301e, 0x301f}, 2, 2, NULL},
    {"jxf37p",  0x46,  "div_cim", 24000000, {0x08, 0x41}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc3336",  0x30,  "div_cim", 24000000, {0xcc, 0x41}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc2336p",  0x30,  "div_cim", 24000000, {0x9b, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"ov9734",  0x30,  "div_cim", 24000000, {0x97, 0x34}, 1, {0x300a, 0x300b}, 2, 2, NULL},
    {"sc2337p", 0x30,  "div_cim", 24000000, {0x9b, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc2336", 0x30,  "div_cim", 24000000, {0xcb, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf23",  0x40,  "div_cim", 24000000, {0x0f, 0x23}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"gc2063",  0x37,  "div_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
    {"sc3332p",  0x30,  "div_cim", 24000000, {0xcc, 0x44}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc2310",  0x30,  "div_cim", 24000000, {0x23, 0x11}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf37pa",  0x40,  "div_cim", 24000000, {0x08, 0x41}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc301IoT",  0x30,  "div_cim", 24000000, {0xcc, 0x40}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc1346",  0x30,  "div_cim", 24000000, {0xda, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf51",  0x40,  "div_cim", 24000000, {0x0f, 0x51}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc301iot",  0x30,  "div_cim", 24000000, {0xcc, 0x40}, 1, {0x3107, 0x3108}, 2, 2, NULL},
};

SENSOR_INFO_T g_sinfo_s1[] =
{
    {"sc301iots1",  0x32,  "div_cim", 24000000, {0xcc, 0x40}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc1084s1",  0x37,  "div_cim", 24000000, {0x10, 0x84}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
    {"gc2083s1",  0x37,  "div_cim", 24000000, {0x20, 0x83}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
    {"sc2336s1",  0x32,  "div_cim", 24000000, {0xcb, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc2053s1",  0x37,  "div_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
    {"sc2331s1",  0x32,  "div_cim", 24000000, {0xcb, 0x5c}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf38ps1",  0x40,  "div_cim", 24000000, {0x08, 0x44}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"jxh63ps1",  0x40,  "div_cim", 24000000, {0x08, 0x48}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"cv2003s1",  0x36,  "div_cim", 24000000, {0x20, 0x03}, 1, {0x3011, 0x3138}, 2, 2, NULL},
    {"sc1a4ts1",  0x30,  "div_cim", 24000000, {0x9a, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"ov9734s1",  0x10,  "div_cim", 24000000, {0x97, 0x34}, 1, {0x300a, 0x300b}, 2, 2, NULL},
    {"jxf37pas1",  0x40,  "div_cim", 24000000, {0x08, 0x41}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc1346s1",  0x30,  "div_cim", 24000000, {0xda, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc2063s1",  0x37,  "div_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
    {"sc2336ps1",  0x32,  "div_cim", 24000000, {0x9b, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL}
};

SENSOR_INFO_T g_sinfo_s2[] =
{
    {"jxh63ps2",  0x46,  "div_cim", 24000000, {0x08, 0x48}, 1, {0x0a, 0x0b}, 1, 2, NULL},
};

#elif CONFIG_KERNEL_3_10
SENSOR_INFO_T g_sinfo[] =
{
    {"gc2083",  0x37,  "cgu_cim", 24000000, {0x20, 0x83}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
    {"sc535IoT",  0x30,  "cgu_cim", 24000000, {0xce, 0x78}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc1084",  0x37,  "cgu_cim", 24000000, {0x10, 0x84}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
    {"jxf38p",  0x40,  "cgu_cim", 24000000, {0x08, 0x44}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc202cs",  0x10,  "cgu_cim", 24000000, {0xeb, 0x52}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc2053",  0x37,  "cgu_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
    {"sc3332",  0x30,  "cgu_cim", 24000000, {0xcc, 0x44}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc2331",  0x30,  "cgu_cim", 24000000, {0xcb, 0x5c}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc3003a",  0x37,  "cgu_cim", 24000000, {0x30, 0x03, 0x10}, 1, {0x03f0, 0x03f1, 0x036a}, 2, 3, NULL},
    {"cv4002",  0x35,  "cgu_cim", 24000000, {0x02, 0x40}, 1, {0x3002, 0x3003}, 2, 2, NULL},
    {"sc200ai",  0x30,  "cgu_cim", 24000000, {0xcb, 0x1c}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxk306p",  0x40,  "cgu_cim", 24000000, {0x08, 0x60}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"jxh63p",  0x40,  "cgu_cim", 24000000, {0x08, 0x48}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc3336p",  0x30,  "cgu_cim", 24000000, {0x9c, 0x41}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"cv2003",  0x35,  "cgu_cim", 24000000, {0x20, 0x03}, 1, {0x3011, 0x3138}, 2, 2, NULL},
    {"jxf28p",  0x40,  "cgu_cim", 24000000, {0x08, 0x50}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc1a4t",  0x30,  "cgu_cim", 24000000, {0x9a, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf57",  0x46,  "cgu_cim", 24000000, {0x08, 0x75}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc223a",  0x30,  "cgu_cim", 24000000, {0xcb, 0x3e}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxq03p",  0x40,  "cgu_cim", 24000000, {0x08, 0x43}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"imx327",  0x36,  "cgu_cim", 37125000, {0xb2, 0x01}, 1, {0x301e, 0x301f}, 2, 2, NULL},
    {"jxf37p",  0x46,  "cgu_cim", 24000000, {0x08, 0x41}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc3336",  0x30,  "cgu_cim", 24000000, {0xcc, 0x41}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc2336p",  0x30,  "cgu_cim", 24000000, {0x9b, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"ov9734",  0x30,  "cgu_cim", 24000000, {0x97, 0x34}, 1, {0x300a, 0x300b}, 2, 2, NULL},
    {"sc2337p", 0x30,  "cgu_cim", 24000000, {0x9b, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc2336", 0x30,  "cgu_cim", 24000000, {0xcb, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf23",  0x40,  "cgu_cim", 24000000, {0x0f, 0x23}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"gc2063",  0x37,  "cgu_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
    {"sc3332p",  0x30,  "cgu_cim", 24000000, {0xcc, 0x44}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc2310",  0x30,  "cgu_cim", 24000000, {0x23, 0x11}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf37pa",  0x40,  "cgu_cim", 24000000, {0x08, 0x41}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc301IoT",  0x30,  "cgu_cim", 24000000, {0xcc, 0x40}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"sc1346",  0x30,  "cgu_cim", 24000000, {0xda, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf51",  0x40,  "cgu_cim", 24000000, {0x0f, 0x51}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc301iot",  0x30,  "cgu_cim", 24000000, {0xcc, 0x40}, 1, {0x3107, 0x3108}, 2, 2, NULL},
};

SENSOR_INFO_T g_sinfo_s1[] =
{
    {"sc301iots1",  0x32,  "cgu_cim", 24000000, {0xcc, 0x40}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc1084s1",  0x37,  "cgu_cim", 24000000, {0x10, 0x84}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
    {"gc2083s1",  0x37,  "cgu_cim", 24000000, {0x20, 0x83}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
    {"sc2336s1",  0x32,  "cgu_cim", 24000000, {0xcb, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc2053s1",  0x3f,  "cgu_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
    {"sc2331s1",  0x32,  "cgu_cim", 24000000, {0xcb, 0x5c}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"jxf38ps1",  0x40,  "cgu_cim", 24000000, {0x08, 0x44}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"jxh63ps1",  0x40,  "cgu_cim", 24000000, {0x08, 0x48}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"cv2003s1",  0x36,  "cgu_cim", 24000000, {0x20, 0x03}, 1, {0x3011, 0x3138}, 2, 2, NULL},
    {"sc1a4ts1",  0x30,  "cgu_cim", 24000000, {0x9a, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"ov9734s1",  0x10,  "cgu_cim", 24000000, {0x97, 0x34}, 1, {0x300a, 0x300b}, 2, 2, NULL},
    {"jxf37pas1",  0x40,  "cgu_cim", 24000000, {0x08, 0x41}, 1, {0x0a, 0x0b}, 1, 2, NULL},
    {"sc1346s1",  0x30,  "cgu_cim", 24000000, {0xda, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
    {"gc2063s1",  0x37,  "cgu_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
    {"sc2336ps1",  0x32,  "cgu_cim", 24000000, {0x9b, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL}
};

SENSOR_INFO_T g_sinfo_s2[] =
{
    {"jxh63ps2",  0x46,  "cgu_cim", 24000000, {0x08, 0x48}, 1, {0x0a, 0x0b}, 1, 2, NULL},
};
#endif

static int8_t g_sensor_id = -1;
static struct mutex g_mutex;

int sensor_write(SENSOR_INFO_P sinfo, struct i2c_adapter *adap, uint16_t reg, unsigned char value)
{
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= sinfo->i2c_addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;
	ret = i2c_transfer(adap, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_read(SENSOR_INFO_P sinfo, struct i2c_adapter *adap, uint32_t addr, uint32_t *value)
{
	int ret;
	uint8_t buf[4] = {0};
	uint8_t data[4] = {0};

	uint8_t rlen = sinfo->id_value_len;
	uint8_t wlen = sinfo->id_addr_len;
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= sinfo->i2c_addr,
			.flags	= 0,
			.len	= wlen,
			.buf	= buf,
		},
		[1] = {
			.addr	= sinfo->i2c_addr,
			.flags	= I2C_M_RD,
			.len	= rlen,
			.buf	= data,
		}
	};

	if (1 == wlen) {
		buf[0] = addr&0xff;
	} else if (2 == wlen){
		buf[0] = (addr>>8)&0xff;
		buf[1] = addr&0xff;
	} else if (3 == wlen){
		buf[0] = (addr>>16)&0xff;
		buf[1] = (addr>>8)&0xff;
		buf[2] = addr&0xff;
	} else if (4 == wlen){
		buf[0] = (addr>>24)&0xff;
		buf[1] = (addr>>16)&0xff;
		buf[2] = (addr>>8)&0xff;
		buf[3] = addr&0xff;
	} else {
		printk("error: %s,%d wlen = %d\n", __func__, __LINE__, wlen);
	}
	ret = i2c_transfer(adap, msg, 2);
	if (ret > 0) ret = 0;
	if (0 != ret)
		printk("error: %s,%d ret = %d\n", __func__, __LINE__, ret);
	if (1 == rlen) {
		*value = data[0];
	} else if (2 == rlen){
		*value = (data[0]<<8)|data[1];
	} else if (3 == rlen){
		*value = (data[0]<<16)|(data[1]<<8)|data[2];
	} else if (4 == rlen){
		*value = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
	} else {
		printk("error: %s,%d rlen = %d\n", __func__, __LINE__, rlen);
	}
	printk(" sensor_read: addr=0x%x value = 0x%x\n", addr, *value);
	return ret;
}

static int32_t process_one_adapter_s2(struct device *dev, void *data)
{
	int32_t ret;
	int32_t i = 0;
	int32_t j = 0;
	struct clk *mclk;
	struct i2c_adapter *adap;
	uint8_t scnt_s2 = sizeof(g_sinfo_s2)/sizeof(g_sinfo_s2[0]);
	mutex_lock(&g_mutex);
	if (dev->type != &i2c_adapter_type) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	adap = to_i2c_adapter(dev);
	printk("name : %s nr : %d\n", adap->name, adap->nr);

	if (adap->nr != i2c_adapter_nr) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	for (i = 0; i < scnt_s2; i++) {
		uint8_t idcnt = g_sinfo_s2[i].id_cnt;

		mclk = clk_get(NULL, g_sinfo_s2[i].mclk_name);
		if (IS_ERR(mclk)) {
			printk("Cannot get sensor input clock div_cim\n");
			mutex_unlock(&g_mutex);
			return PTR_ERR(mclk);
		}

		clk_set_rate(mclk, g_sinfo_s2[i].clk);

#ifdef CONFIG_KERNEL_3_10
		clk_enable(mclk);
#elif CONFIG_KERNEL_4_4_94
		clk_prepare_enable(mclk);
#endif

		if(reset_gpio != -1){
			ret = gpio_request(reset_gpio,"reset");
			if(!ret){
				gpio_direction_output(reset_gpio, 1);
				msleep(20);
				gpio_direction_output(reset_gpio, 0);
				if (strcmp(g_sinfo_s2[i].name, "sp1409") == 0) {
					msleep(600);
				} else if (strcmp(g_sinfo_s2[i].name, "sc2336p") == 0){
					msleep(250);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				} else if (strcmp(g_sinfo_s2[i].name, "sc2337p") == 0){
					msleep(250);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				} else if (strcmp(g_sinfo_s2[i].name, "sc3336p") == 0){
					msleep(250);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				} else {
					msleep(20);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				}
			}else{
				printk("gpio requrest fail %d\n",reset_gpio);
			}
		}
		if(pwdn_gpio != -1){
			ret = gpio_request(pwdn_gpio,"pwdn");
			if(!ret){
				gpio_direction_output(pwdn_gpio, 1);
				msleep(150);
				gpio_direction_output(pwdn_gpio, 0);
				if(strcmp(g_sinfo_s2[i].name, "sp1409") == 0)
					msleep(600);
				else
					msleep(10);
			}else{
				printk("gpio requrest fail %d\n",pwdn_gpio);
			}
		}

		for (j = 0; j < idcnt; j++) {
			uint32_t value = 0;

			if ((strcmp(g_sinfo_s2[i].name, "sc2336p") == 0) && (j == 0)) {
				ret = sensor_write(&g_sinfo_s2[i], adap, 0x301a, 0xf8);
				ret += sensor_write(&g_sinfo_s2[i], adap, 0x0100, 0x01);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				msleep(5);
			} else if ((strcmp(g_sinfo_s2[i].name, "sc2337p") == 0) && (j == 0)) {
				ret = sensor_write(&g_sinfo_s2[i], adap, 0x301a, 0xf8);
				ret += sensor_write(&g_sinfo_s2[i], adap, 0x0100, 0x01);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				msleep(5);
			} else if ((strcmp(g_sinfo_s2[i].name, "sc3336p") == 0) && (j == 0)) {
				ret = sensor_write(&g_sinfo_s2[i], adap, 0x440d, 0x10);
				ret += sensor_write(&g_sinfo_s2[i], adap, 0x4400, 0x11);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				msleep(10);
			}

			ret = sensor_read(&g_sinfo_s2[i], adap, g_sinfo_s2[i].id_addr[j], &value);
			if (0 != ret) {
				printk("err sensor read addr = 0x%x, value = 0x%x\n", g_sinfo_s2[i].id_addr[j], value);
				break;
			}

			if ((strcmp(g_sinfo_s2[i].name, "sc2336p") == 0) && (j == 1)) {
				uint32_t reg_val = 0;
				ret = sensor_read(&g_sinfo_s2[i], adap, 0x801e, &reg_val);
				if (0 != ret) {
					printk("sensor read reg error!\n");
					break;
				}
				ret = sensor_write(&g_sinfo_s2[i], adap, 0x0100, 0x00);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				if ((reg_val & 0x0000000f) != 0) {
					j--;
					break;
				}
			} else if ((strcmp(g_sinfo_s2[i].name, "sc2337p") == 0) && (j == 1)) {
				uint32_t reg_val = 0;
				ret = sensor_read(&g_sinfo_s2[i], adap, 0x801e, &reg_val);
				if (0 != ret) {
					printk("sensor read reg error!\n");
					break;
				}
				ret = sensor_write(&g_sinfo_s2[i], adap, 0x0100, 0x00);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				if ((reg_val & 0x0000000f) == 0) {
					j--;
					break;
				}
			}

			if(strcmp(g_sinfo_s2[i].name, "ov2735b") == 0 && j == 2){
				if (value == g_sinfo_s2[i].id_value[j])
					j++;
			}
			else
				if (value != g_sinfo_s2[i].id_value[j])
					break;
		}

		if (-1 != reset_gpio)
			gpio_free(reset_gpio);
		if (-1 != pwdn_gpio)
			gpio_free(pwdn_gpio);

#ifdef CONFIG_KERNEL_3_10
		clk_disable(mclk);
#elif CONFIG_KERNEL_4_4_94
		clk_disable_unprepare(mclk);
#endif

		clk_put(mclk);
		if (j == idcnt) {
			printk("info: success sensor find : %s\n", g_sinfo_s2[i].name);
			g_sinfo_s2[i].adap = adap;
			g_sensor_id = i;
			g_sensor_num = 3;
			goto end_sensors2_find;
		}
	}
	printk("info: failed sensor find\n");
	g_sensor_id = -1;
	mutex_unlock(&g_mutex);
	return 0;
end_sensors2_find:
	mutex_unlock(&g_mutex);
	return 0;
}

static int32_t process_one_adapter_s1(struct device *dev, void *data)
{
	int32_t ret;
	int32_t i = 0;
	int32_t j = 0;
	struct clk *mclk;
	struct i2c_adapter *adap;
	uint8_t scnt_s1 = sizeof(g_sinfo_s1)/sizeof(g_sinfo_s1[0]);
	mutex_lock(&g_mutex);
	if (dev->type != &i2c_adapter_type) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	adap = to_i2c_adapter(dev);
	printk("name : %s nr : %d\n", adap->name, adap->nr);

	if (adap->nr != i2c_adapter_nr) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	for (i = 0; i < scnt_s1; i++) {
		uint8_t idcnt = g_sinfo_s1[i].id_cnt;

		mclk = clk_get(NULL, g_sinfo_s1[i].mclk_name);
		if (IS_ERR(mclk)) {
			printk("Cannot get sensor input clock div_cim\n");
			mutex_unlock(&g_mutex);
			return PTR_ERR(mclk);
		}

		clk_set_rate(mclk, g_sinfo_s1[i].clk);

#ifdef CONFIG_KERNEL_3_10
		clk_enable(mclk);
#elif CONFIG_KERNEL_4_4_94
		clk_prepare_enable(mclk);
#endif

		if(reset_gpio != -1){
			ret = gpio_request(reset_gpio,"reset");
			if(!ret){
				gpio_direction_output(reset_gpio, 1);
				msleep(20);
				gpio_direction_output(reset_gpio, 0);
				if (strcmp(g_sinfo_s1[i].name, "sp1409") == 0) {
					msleep(600);
				} else if (strcmp(g_sinfo_s1[i].name, "sc2336p") == 0){
					msleep(250);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				} else if (strcmp(g_sinfo_s1[i].name, "sc2337p") == 0){
					msleep(250);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				} else if (strcmp(g_sinfo_s1[i].name, "sc3336p") == 0){
					msleep(250);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				} else {
					msleep(20);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				}
			}else{
				printk("gpio requrest fail %d\n",reset_gpio);
			}
		}
		if(pwdn_gpio != -1){
			ret = gpio_request(pwdn_gpio,"pwdn");
			if(!ret){
				gpio_direction_output(pwdn_gpio, 1);
				msleep(150);
				gpio_direction_output(pwdn_gpio, 0);
				if(strcmp(g_sinfo_s1[i].name, "sp1409") == 0)
					msleep(600);
				else
					msleep(10);
			}else{
				printk("gpio requrest fail %d\n",pwdn_gpio);
			}
		}

		for (j = 0; j < idcnt; j++) {
			uint32_t value = 0;

			if ((strcmp(g_sinfo_s1[i].name, "sc2336p") == 0) && (j == 0)) {
				ret = sensor_write(&g_sinfo_s1[i], adap, 0x301a, 0xf8);
				ret += sensor_write(&g_sinfo_s1[i], adap, 0x0100, 0x01);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				msleep(5);
			} else if ((strcmp(g_sinfo_s1[i].name, "sc2337p") == 0) && (j == 0)) {
				ret = sensor_write(&g_sinfo_s1[i], adap, 0x301a, 0xf8);
				ret += sensor_write(&g_sinfo_s1[i], adap, 0x0100, 0x01);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				msleep(5);
			} else if ((strcmp(g_sinfo_s1[i].name, "sc3336p") == 0) && (j == 0)) {
				ret = sensor_write(&g_sinfo_s1[i], adap, 0x440d, 0x10);
				ret += sensor_write(&g_sinfo_s1[i], adap, 0x4400, 0x11);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				msleep(10);
			}

			ret = sensor_read(&g_sinfo_s1[i], adap, g_sinfo_s1[i].id_addr[j], &value);
			if (0 != ret) {
				printk("err sensor read addr = 0x%x, value = 0x%x\n", g_sinfo_s1[i].id_addr[j], value);
				break;
			}

			if ((strcmp(g_sinfo_s1[i].name, "sc2336p") == 0) && (j == 1)) {
				uint32_t reg_val = 0;
				ret = sensor_read(&g_sinfo_s1[i], adap, 0x801e, &reg_val);
				if (0 != ret) {
					printk("sensor read reg error!\n");
					break;
				}
				ret = sensor_write(&g_sinfo_s1[i], adap, 0x0100, 0x00);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				if ((reg_val & 0x0000000f) != 0) {
					j--;
					break;
				}
			} else if ((strcmp(g_sinfo_s1[i].name, "sc2337p") == 0) && (j == 1)) {
				uint32_t reg_val = 0;
				ret = sensor_read(&g_sinfo_s1[i], adap, 0x801e, &reg_val);
				if (0 != ret) {
					printk("sensor read reg error!\n");
					break;
				}
				ret = sensor_write(&g_sinfo_s1[i], adap, 0x0100, 0x00);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				if ((reg_val & 0x0000000f) == 0) {
					j--;
					break;
				}
			}

			if(strcmp(g_sinfo_s1[i].name, "ov2735b") == 0 && j == 2){
				if (value == g_sinfo_s1[i].id_value[j])
					j++;
			}
			else
				if (value != g_sinfo_s1[i].id_value[j])
					break;
		}

		if (-1 != reset_gpio)
			gpio_free(reset_gpio);
		if (-1 != pwdn_gpio)
			gpio_free(pwdn_gpio);

#ifdef CONFIG_KERNEL_3_10
		clk_disable(mclk);
#elif CONFIG_KERNEL_4_4_94
		clk_disable_unprepare(mclk);
#endif

		clk_put(mclk);
		if (j == idcnt) {
			printk("info: success sensor find : %s\n", g_sinfo_s1[i].name);
			g_sinfo_s1[i].adap = adap;
			g_sensor_id = i;
			g_sensor_num = 2;
			goto end_sensors1_find;
		}
	}
	printk("info: failed sensor find\n");
	g_sensor_id = -1;
	mutex_unlock(&g_mutex);
	return 0;
end_sensors1_find:
	mutex_unlock(&g_mutex);
	return 0;
}

static int32_t process_one_adapter(struct device *dev, void *data)
{
	int32_t ret;
	int32_t i = 0;
	int32_t j = 0;
	struct clk *mclk;
	struct i2c_adapter *adap;
	uint8_t scnt = sizeof(g_sinfo)/sizeof(g_sinfo[0]);
	mutex_lock(&g_mutex);
	if (dev->type != &i2c_adapter_type) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	adap = to_i2c_adapter(dev);
	printk("name : %s nr : %d\n", adap->name, adap->nr);

	if (adap->nr != i2c_adapter_nr) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	for (i = 0; i < scnt; i++) {
		uint8_t idcnt = g_sinfo[i].id_cnt;

		mclk = clk_get(NULL, g_sinfo[i].mclk_name);
		if (IS_ERR(mclk)) {
			printk("Cannot get sensor input clock div_cim\n");
			mutex_unlock(&g_mutex);
			return PTR_ERR(mclk);
		}

		clk_set_rate(mclk, g_sinfo[i].clk);

#ifdef CONFIG_KERNEL_3_10
		clk_enable(mclk);
#elif CONFIG_KERNEL_4_4_94
		clk_prepare_enable(mclk);
#endif

		if(reset_gpio != -1){
			ret = gpio_request(reset_gpio,"reset");
			if(!ret){
				gpio_direction_output(reset_gpio, 1);
				msleep(20);
				gpio_direction_output(reset_gpio, 0);
				if (strcmp(g_sinfo[i].name, "sp1409") == 0) {
					msleep(600);
				} else if (strcmp(g_sinfo[i].name, "sc2336p") == 0){
					msleep(250);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				} else if (strcmp(g_sinfo[i].name, "sc2337p") == 0){
					msleep(250);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				} else if (strcmp(g_sinfo[i].name, "sc3336p") == 0){
					msleep(250);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				} else {
					msleep(20);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				}
			}else{
				printk("gpio requrest fail %d\n",reset_gpio);
			}
		}
		if(pwdn_gpio != -1){
			ret = gpio_request(pwdn_gpio,"pwdn");
			if(!ret){
				gpio_direction_output(pwdn_gpio, 1);
				msleep(150);
				gpio_direction_output(pwdn_gpio, 0);
				if(strcmp(g_sinfo[i].name, "sp1409") == 0)
					msleep(600);
				else
					msleep(10);
			}else{
				printk("gpio requrest fail %d\n",pwdn_gpio);
			}
		}

		for (j = 0; j < idcnt; j++) {
			uint32_t value = 0;

			if ((strcmp(g_sinfo[i].name, "sc2336p") == 0) && (j == 0)) {
				ret = sensor_write(&g_sinfo[i], adap, 0x301a, 0xf8);
				ret += sensor_write(&g_sinfo[i], adap, 0x0100, 0x01);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				msleep(5);
			} else if ((strcmp(g_sinfo[i].name, "sc2337p") == 0) && (j == 0)) {
				ret = sensor_write(&g_sinfo[i], adap, 0x301a, 0xf8);
				ret += sensor_write(&g_sinfo[i], adap, 0x0100, 0x01);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				msleep(5);
			} else if ((strcmp(g_sinfo[i].name, "sc3336p") == 0) && (j == 0)) {
				ret = sensor_write(&g_sinfo[i], adap, 0x440d, 0x10);
				ret += sensor_write(&g_sinfo[i], adap, 0x4400, 0x11);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				msleep(10);
			}

			ret = sensor_read(&g_sinfo[i], adap, g_sinfo[i].id_addr[j], &value);
			if (0 != ret) {
				printk("err sensor read addr = 0x%x, value = 0x%x\n", g_sinfo[i].id_addr[j], value);
				break;
			}

			if ((strcmp(g_sinfo[i].name, "sc2336p") == 0) && (j == 1)) {
				uint32_t reg_val = 0;
				ret = sensor_read(&g_sinfo[i], adap, 0x801e, &reg_val);
				if (0 != ret) {
					printk("sensor read reg error!\n");
					break;
				}
				ret = sensor_write(&g_sinfo[i], adap, 0x0100, 0x00);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				if ((reg_val & 0x0000000f) != 0) {
					j--;
					break;
				}
			} else if ((strcmp(g_sinfo[i].name, "sc2337p") == 0) && (j == 1)) {
				uint32_t reg_val = 0;
				ret = sensor_read(&g_sinfo[i], adap, 0x801e, &reg_val);
				if (0 != ret) {
					printk("sensor read reg error!\n");
					break;
				}
				ret = sensor_write(&g_sinfo[i], adap, 0x0100, 0x00);
				if (0 != ret) {
					printk("sensor write reg error!\n");
					break;
				}
				if ((reg_val & 0x0000000f) == 0) {
					j--;
					break;
				}
			}

			if(strcmp(g_sinfo[i].name, "ov2735b") == 0 && j == 2){
				if (value == g_sinfo[i].id_value[j])
					j++;
			}
			else
				if (value != g_sinfo[i].id_value[j])
					break;
		}

		if (-1 != reset_gpio)
			gpio_free(reset_gpio);
		if (-1 != pwdn_gpio)
			gpio_free(pwdn_gpio);

#ifdef CONFIG_KERNEL_3_10
		clk_disable(mclk);
#elif CONFIG_KERNEL_4_4_94
		clk_disable_unprepare(mclk);
#endif

		clk_put(mclk);
		if (j == idcnt) {
			printk("info: success sensor find : %s\n", g_sinfo[i].name);
			g_sinfo[i].adap = adap;
			g_sensor_id = i;
			g_sensor_num = 1;
			goto end_sensor_find;
		}
	}
	printk("info: failed sensor find\n");
	g_sensor_id = -1;
	mutex_unlock(&g_mutex);
	return 0;
end_sensor_find:
	mutex_unlock(&g_mutex);
	return 0;
}

#ifdef CONCTL_SENSOR_OPENCLOSE
static int32_t sensor_open(void)
{
	int ret = -1;
	struct clk *sclk;
	struct clk *mclk;
	if (-1 == g_sensor_id)
		return 0;

	mutex_lock(&g_mutex);


	mclk = clk_get(NULL, g_sinfo[g_sensor_id].mclk_name);
	if (IS_ERR(mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		mutex_unlock(&g_mutex);
		return PTR_ERR(mclk);
	}
	clk_set_rate(mclk, g_sinfo[g_sensor_id].clk);

#ifdef CONFIG_KERNEL_3_10
		clk_enable(mclk);
#elif CONFIG_KERNEL_4_4_94
		clk_prepare_enable(mclk);
#endif

	if(reset_gpio != -1){
		ret = gpio_request(reset_gpio,"reset");
		if(!ret){
			gpio_direction_output(reset_gpio, 1);
			msleep(20);
			gpio_direction_output(reset_gpio, 0);
			msleep(20);
			gpio_direction_output(reset_gpio, 1);
			msleep(20);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = gpio_request(pwdn_gpio,"pwdn");
		if(!ret){
			gpio_direction_output(pwdn_gpio, 1);
			msleep(150);
			gpio_direction_output(pwdn_gpio, 0);
			msleep(10);
		}else{
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	mutex_unlock(&g_mutex);
	return 0;
}
#endif

static int32_t sensor_release(void)
{
	struct clk *mclk;
	if (-1 == g_sensor_id)
		return 0;
	mutex_lock(&g_mutex);
	mclk = clk_get(NULL, g_sinfo[g_sensor_id].mclk_name);
	if (IS_ERR(mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		mutex_unlock(&g_mutex);
		return PTR_ERR(mclk);
	}
	if (-1 != reset_gpio)
		gpio_free(reset_gpio);
	if (-1 != pwdn_gpio)
		gpio_free(pwdn_gpio);

#ifdef CONFIG_KERNEL_3_10
		clk_disable(mclk);
#elif CONFIG_KERNEL_4_4_94
		clk_disable_unprepare(mclk);
#endif

	clk_put(mclk);
	mutex_unlock(&g_mutex);
	return 0;
}

#ifdef I2C_READ_WRITE
static int32_t i2c_read_write(struct device *dev, void *data)
{
	int32_t ret;
	struct i2c_adapter *adap;
	struct i2c_trans *t = data;

	uint8_t buf[4] = {0};
	uint32_t value = 0;
	uint8_t len = t->datalen;
	struct i2c_msg msg = {
			.addr	= t->addr,
			.flags	= (t->r_w == I2C_WRITE)?0:I2C_M_RD,
			.len	= len,
			.buf	= buf,
	};


	mutex_lock(&g_mutex);
	if (dev->type != &i2c_adapter_type) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	adap = to_i2c_adapter(dev);
	printk("name : %s nr : %d\n", adap->name, adap->nr);

	if (adap->nr != i2c_adapter_nr) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	if (I2C_WRITE == t->r_w) {
		if (1 == len) {
			buf[0] = (t->data)&0xff;
		} else if (2 == len){
			buf[0] = ((t->data)>>8)&0xff;
			buf[1] = (t->data)&0xff;
		} else if (3 == len){
			buf[0] = ((t->data)>>16)&0xff;
			buf[1] = ((t->data)>>8)&0xff;
			buf[2] = (t->data)&0xff;
		} else if (4 == len){
			buf[0] = ((t->data)>>24)&0xff;
			buf[1] = ((t->data)>>16)&0xff;
			buf[2] = ((t->data)>>8)&0xff;
			buf[3] = (t->data)&0xff;
		} else {
			printk("error: %s,%d len = %d\n", __func__, __LINE__, len);
		}
	}
	ret = i2c_transfer(adap, &msg, 1);
	if (ret > 0) ret = 0;
	if (0 != ret)
		printk("error: %s,%d ret = %d\n", __func__, __LINE__, ret);

	if (I2C_READ == t->r_w) {
		if (1 == len) {
			value = buf[0];
		} else if (2 == len){
			value = (buf[0]<<8)|buf[1];
		} else if (3 == len){
			value = (buf[0]<<16)|(buf[1]<<8)|buf[2];
		} else if (4 == len){
			value = (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
		} else {
			printk("error: %s,%d len = %d\n", __func__, __LINE__, len);
		}
		printk(" i2c: addr=0x%x value = 0x%x\n", t->addr, value);
	}
	mutex_unlock(&g_mutex);
	return 0;
}
#endif
static long sinfo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int32_t data;

	mutex_lock(&g_mutex);
	switch (cmd) {
	case IOCTL_SINFO_GET:
		if (-1 == g_sensor_id)
			data = -1;
		else
			data = g_sensor_id;
		if (copy_to_user((void *)arg, &data, sizeof(data))) {
			printk("copy_from_user error!!!\n");
			ret = -EFAULT;
			break;
		}
		break;
	case IOCTL_SINFO_FLASH:
		i2c_for_each_dev(NULL, process_one_adapter);
		break;
	default:
		printk("invalid command: 0x%08x\n", cmd);
		ret = -EINVAL;
	}
	mutex_unlock(&g_mutex);
	return ret;
}
static int sinfo_open(struct inode *inode, struct file *filp)
{
	i2c_for_each_dev(NULL, process_one_adapter);
	return 0;
}
static int sinfo_release(struct inode *inode, struct file *filp)
{
	printk ("misc sinfo_release\n");
	sensor_release();
	return 0;
}

static ssize_t sinfo_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static struct file_operations sinfo_fops =
{
	.owner = THIS_MODULE,
	.read = sinfo_read,
	.unlocked_ioctl = sinfo_ioctl,
	.open = sinfo_open,
	.release = sinfo_release,
};

static struct miscdevice misc_sinfo = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sinfo",
	.fops = &sinfo_fops,
};


static int sinfo_proc_show(struct seq_file *m, void *v)
{
	if (-1 == g_sensor_id) {
		seq_printf(m, "not found\n");
	} else {
		if (g_sensor_num == 1 || g_sensor_num == 0) {
			seq_printf(m, "sensor :%s\n", g_sinfo[g_sensor_id].name);
		} else if (g_sensor_num == 2) {
			seq_printf(m, "sensor :%s\n", g_sinfo_s1[g_sensor_id].name);
		} else if (g_sensor_num == 3) {
			seq_printf(m, "sensor :%s\n", g_sinfo_s2[g_sensor_id].name);
		} else {
			seq_printf(m, "sensor_num not found\n");
		}
	}
	return 0;
}

static int sinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sinfo_proc_show, NULL);
}

ssize_t sinfo_proc_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
#ifdef I2C_READ_WRITE
	int ret = 0;
	uint32_t addr,data,datalen;
#endif
	char cmd[100] = {0};

	if (len > 100) {
		printk("err: cmd too long\n");
		return -EFAULT;
	}
	if(copy_from_user(cmd, buf, len))
	{
		return -EFAULT;
	}
	/* probe sensor */
	if (!strncmp(cmd, "1", strlen("1"))) {
		i2c_for_each_dev(NULL, process_one_adapter);
	} else if (!strncmp(cmd, "2", strlen("2"))) {
		i2c_for_each_dev(NULL, process_one_adapter_s1);
	} else if (!strncmp(cmd, "3", strlen("3"))) {
		i2c_for_each_dev(NULL, process_one_adapter_s2);

#ifdef I2C_READ_WRITE
    /* sensor open/release i2c read/write
	 * open: set sensor clk,reset
	 * release: free clk,reset gpio
	 *
	 * example: sc2135
	 *
	 * echo open:sc2135 > /proc/jz/sinfo/info
	 * echo i2c-w:0x30-0x3017-2 > /proc/jz/sinfo/info
	 * echo i2c-r:0x30-1 > /proc/jz/sinfo/info
	 *
	 * */

	} else if (!strncmp(cmd, "i2c-w:", strlen("i2c-w:"))) {
		ret = sscanf(cmd, "i2c-w:%i-%i-%i", &addr, &data, &datalen);
		if (3 != ret) {
			printk("err: cmd error %s\n", cmd);
			return len;
		} else {
			struct i2c_trans t = {addr, I2C_WRITE, data, datalen};
			printk("info: i2c-w:%d-%d-%d\n", addr, data, datalen);
			i2c_for_each_dev(&t, i2c_read_write);
		}
	} else if (!strncmp(cmd, "i2c-r:", strlen("i2c-r:"))) {
		ret = sscanf(cmd, "i2c-r:%i-%i", &addr, &datalen);
		if (2 != ret) {
			printk("err: cmd error %s\n", cmd);
			return len;
		} else {
			struct i2c_trans t = {addr, I2C_READ, 0, datalen};
			printk("info: i2c-r:%d-%d\n", addr, datalen);
			i2c_for_each_dev(&t, i2c_read_write);
		}
#endif
#ifdef CONCTL_SENSOR_OPENCLOSE
	} else if (!strncmp(cmd, "open", strlen("open"))) {
		int i = 0;
		char s[20] = {0};
		ret = sscanf(cmd, "open:%s", s);
		if (1 != ret) {
			printk("err: cmd error %s\n", cmd);
			return len;
		} else {
			uint8_t scnt = sizeof(g_sinfo)/sizeof(g_sinfo[0]);
			for (i = 0; i < scnt; i++) {
				if (!strcmp(s, g_sinfo[i].name)) {
					g_sensor_id = i;
					break;
				}
			}
			if (i >= scnt) {
				printk("err: sensor not found %s, cmd %s\n", s, cmd);
				return len;
			}
			sensor_open();
		}
	} else if (!strncmp(cmd, "release", strlen("release"))) {
		sensor_release();
#endif
	} else {
		printk("err: cmd not support\n");
	}
	return len;
}

static const struct file_operations sinfo_proc_fops = {
	.owner = THIS_MODULE,
	.open = sinfo_proc_open,
	.read = seq_read,
	.write = sinfo_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

struct proc_dir_entry *g_sinfo_proc;
static __init int init_sinfo(void)
{
	int ret = 0;
	mutex_init(&g_mutex);

    g_sinfo_proc = proc_mkdir("jz/sinfo", 0);
	if (!g_sinfo_proc) {
		printk("err: jz_proc_mkdir failed\n");
	}
	proc_create_data("info", S_IRUGO, g_sinfo_proc, &sinfo_proc_fops, NULL);
	/* i2c_for_each_dev(NULL, process_one_adapter); */
	ret = misc_register(&misc_sinfo);
	/* printk("##### g_sensor_id = %d\n", g_sensor_id); */
	return ret;

}

static __exit void exit_sinfo(void)
{
	proc_remove(g_sinfo_proc);
	misc_deregister(&misc_sinfo);
}

module_init(init_sinfo);
module_exit(exit_sinfo);

MODULE_DESCRIPTION("A Simple driver for get sensors info ");
MODULE_LICENSE("GPL");
