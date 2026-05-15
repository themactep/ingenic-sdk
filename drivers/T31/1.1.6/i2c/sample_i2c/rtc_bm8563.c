/*
 * rtc_bm8563.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
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

static unsigned i2c_adapter_nr = 1;
module_param(i2c_adapter_nr, uint, 0644);
MODULE_PARM_DESC(i2c_adapter_nr, "sensor used i2c_adapter nr");

#define RTC_IOC_MAGIC  'S'
#define IOCTL_RTC_GET	_IO(RTC_IOC_MAGIC, 100)
#define IOCTL_RTC_SET	_IO(RTC_IOC_MAGIC, 101)

#define I2C_WRITE 0
#define I2C_READ  1

struct i2c_trans {
	uint32_t addr;
	uint32_t r_w;
	uint32_t data;
	uint32_t datalen;
};

typedef struct RTC_DATE_S
{
	uint8_t *name;
	uint8_t i2c_addr;

	uint8_t date_cnt;
	uint32_t date_addr[6]; /*年月日时分秒*/
	uint32_t date_addr_len;
	uint32_t date_value_len;
} RTC_DATE_T, *RTC_DATE_P;

RTC_DATE_T date_info[] =
{
	{"bm8536", 0x51, 6, {0x08, 0x07, 0x05, 0x04, 0x03, 0x02}, 1, 1},
};

int rtc_i2c_write(RTC_DATE_P info, struct i2c_adapter *adap, uint32_t addr, uint32_t value)
{
	int ret;
	uint8_t buf[4] = {0};
	uint8_t data[4] = {0};

	uint8_t rlen = info->date_value_len;
	uint8_t wlen = info->date_addr_len;
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= info->i2c_addr,
			.flags	= I2C_WRITE,
			.len	= wlen,
			.buf	= buf,
		},
		[1] = {
			.addr	= info->i2c_addr,
			.flags	= I2C_WRITE,
			.len	= wlen,
			.buf	= data,
		}
	};

	if (1 == wlen) {
		buf[0] = addr & 0xff;
	} else if (2 == wlen){
		buf[0] = (addr >> 8) & 0xff;
		buf[1] = addr & 0xff;
	} else if (3 == wlen){
		buf[0] = (addr >> 16) & 0xff;
		buf[1] = (addr >> 8) & 0xff;
		buf[2] = addr & 0xff;
	} else if (4 == wlen){
		buf[0] = (addr >> 24) & 0xff;
		buf[1] = (addr >> 16) & 0xff;
		buf[2] = (addr >> 8) & 0xff;
		buf[3] = addr & 0xff;
	} else {
		printk("error: %s,%d wlen = %d\n", __func__, __LINE__, wlen);
	}

	if (1 == rlen) {
		data[0] = value & 0xff;
	} else if (2 == rlen){
		data[0] = (value >> 8) & 0xff;
		data[1] = value & 0xff;
	} else if (3 == rlen){
		data[0] = (value >> 16) & 0xff;
		data[1] = (value >> 8) & 0xff;
		data[2] = value & 0xff;
	} else if (4 == rlen){
		data[0] = (value >> 24) & 0xff;
		data[1] = (value >> 16) & 0xff;
		data[2] = (value >> 8) & 0xff;
		data[3] = value & 0xff;
	} else {
		printk("error: %s,%d rlen = %d\n", __func__, __LINE__, rlen);
	}

	ret = i2c_transfer(adap, msg, 2);
	if (ret > 0) ret = 0;
	if (0 != ret)
		printk("error: %s,%d ret = %d\n", __func__, __LINE__, ret);
	return ret;
}


int rtc_i2c_read(RTC_DATE_P info, struct i2c_adapter *adap, uint32_t addr, uint32_t *value)
{
	int ret;
	uint8_t buf[4] = {0};
	uint8_t data[4] = {0};

	uint8_t rlen = info->date_value_len;
	uint8_t wlen = info->date_addr_len;
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= info->i2c_addr,
			.flags	= I2C_WRITE,
			.len	= wlen,
			.buf	= buf,
		},
		[1] = {
			.addr	= info->i2c_addr,
			.flags	= I2C_READ,
			.len	= rlen,
			.buf	= data,
		}
	};

	if (1 == wlen) {
		buf[0] = addr & 0xff;
	} else if (2 == wlen){
		buf[0] = (addr >> 8) & 0xff;
		buf[1] = addr & 0xff;
	} else if (3 == wlen){
		buf[0] = (addr >> 16) & 0xff;
		buf[1] = (addr >> 8) & 0xff;
		buf[2] = addr & 0xff;
	} else if (4 == wlen){
		buf[0] = (addr >> 24) & 0xff;
		buf[1] = (addr >> 16) & 0xff;
		buf[2] = (addr >> 8) & 0xff;
		buf[3] = addr & 0xff;
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
		*value = (data[0] << 8) | data[1];
	} else if (3 == rlen){
		*value = (data[0] << 16) | (data[1] << 8) | data[2];
	} else if (4 == rlen){
		*value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	} else {
		printk("error: %s,%d rlen = %d\n", __func__, __LINE__, rlen);
	}
	printk(" sensor_read: addr=0x%x value = 0x%x\n", addr, *value);
	return ret;
}

static int32_t rtc_set_time(struct device *dev, void *data)
{
	struct i2c_adapter *adap;
	uint32_t value = 0;
	uint32_t date[6];

	printk("data is %s\n", data);
	memset(date, 0, 6);
	if(dev->type != &i2c_adapter_type)
	{
		printk("i2c not enable!\n");
		return 0;
	}
	adap = to_i2c_adapter(dev);
	if (adap->nr != i2c_adapter_nr)
		return 0;
	else
		printk("name : %s nr : %d\n", adap->name, adap->nr);

	rtc_i2c_write(&date_info[0], adap, 0x0, 0x0);
	rtc_i2c_write(&date_info[0], adap, 0x8, 0x21);
	rtc_i2c_write(&date_info[0], adap, 0x7, 0x04);
	rtc_i2c_write(&date_info[0], adap, 0x5, 0x29);
	rtc_i2c_write(&date_info[0], adap, 0x4, 0x14);
	rtc_i2c_write(&date_info[0], adap, 0x3, 0x12);
	rtc_i2c_write(&date_info[0], adap, 0x2, 0x0);
	rtc_i2c_read(&date_info[0], adap, 0x0, &value);
	printk("contrl is %x\n", value);
	return 0;
}


static int32_t rtc_get_time(struct device *dev, void *data)
{
	struct i2c_adapter *adap;
	int i = 0, j = 0, ret =0;
	uint32_t value = 0;
	unsigned int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
	uint32_t date[6];
	int rtc_num = sizeof(date_info)/sizeof(date_info[0]);

	memset(date, 0, 6);
	if(dev->type != &i2c_adapter_type)
	{
		printk("i2c not enable!\n");
		return 0;
	}
	adap = to_i2c_adapter(dev);
	if (adap->nr != i2c_adapter_nr)
		return 0;
	else
		printk("name : %s nr : %d\n", adap->name, adap->nr);

	//rtc_i2c_write(&date_info[0], adap, 0x2, 0x0);
	rtc_i2c_write(&date_info[0], adap, 0x0, 0x0);
	rtc_i2c_read(&date_info[0], adap, 0x0, &value);
	printk("contrl is %x\n", value);
	for(i = 0; i < rtc_num; i++)
	{
		value = 0;
		for(j = 0; j < date_info[i].date_cnt; j++)
		{
			ret = rtc_i2c_read(&date_info[i], adap, date_info[i].date_addr[j], &value);
			if(ret)
				break;
			date[j] = value;
		}
	}
	year = ((date[0] >> 4) & 0xf) * 10 + (date[0] & 0xf);
	date[1] &= 0x1f;
	month = ((date[1] >> 4) & 0xf) * 10 + (date[1] & 0xf);
	date[2] &= 0x3f;
	day = ((date[2] >> 4) & 0xf) * 10 + (date[2] & 0xf);
	date[3] &= 0x3f;
	hour = ((date[3] >> 4) & 0xf) * 10 + (date[3] & 0xf);
	date[4] &= 0x7f;
	minute = ((date[4] >> 4) & 0xf) * 10 + (date[4] & 0xf);
	date[5] &= 0x7f;
	second = ((date[5] >> 4) & 0xf) * 10 + (date[5] & 0xf);
	printk("year:20%d month:%d day:%d hour:%d minute:%d second:%d\n", year, month, day, hour, minute, second);
	return 0;
}

static int rtc_open(struct inode *inode, struct file *filp)
{
	printk("rtc_open\n");
	//i2c_for_each_dev(NULL, process_one_adapter);
	return 0;
}

static ssize_t rtc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	printk("rtc_read\n");
	return 0;
}

static ssize_t rtc_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
	printk("rtc_proc_write\n");
	return len;
}

static long rtc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	printk("rtc_ioctl\n");
	return ret;
}

static int rtc_release(struct inode *inode, struct file *filp)
{
	printk ("misc rtc_release\n");
	return 0;
}

static struct file_operations rtc_fops =
{
	.owner = THIS_MODULE,
	.open = rtc_open,
	.read = rtc_read,
	.write = rtc_write,
	.unlocked_ioctl = rtc_ioctl,
	.release = rtc_release,
};

static struct miscdevice misc_rtc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rtc",
	.fops = &rtc_fops,
};

static int rtc_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "rtc_proc_show\n");
	return 0;
}

static int rtc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rtc_proc_show, NULL);
}

ssize_t rtc_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	printk("rtc_proc_read\n");
	i2c_for_each_dev(NULL, rtc_get_time);
	return 0;
}

ssize_t rtc_proc_write(struct file *filp, char *buf, size_t len, loff_t *off)
{
	printk("rtc_proc_write,date is %s\n", buf);
	i2c_for_each_dev(buf, rtc_set_time);
	return len;
}

static const struct file_operations rtc_proc_fops = {
	.owner = THIS_MODULE,
	.open = rtc_proc_open,
	.read = rtc_proc_read,
	.write = rtc_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

struct proc_dir_entry *g_rtc_proc;
static __init int init_rtc(void)
{
	int ret = 0;
	g_rtc_proc = proc_mkdir("jz/rtc", 0);
	if(!g_rtc_proc)
	{
		printk("proc_jz_rtc failed\n");
		return -1;
	}
	proc_create_data("date", S_IRUGO, g_rtc_proc, &rtc_proc_fops, NULL);
	ret = misc_register(&misc_rtc);
	if(ret)
		printk("rtc misc_register failed\n");
	return ret;
}

static __exit void exit_rtc(void)
{
	proc_remove(g_rtc_proc);
	misc_deregister(&misc_rtc);
}

module_init(init_rtc);
module_exit(exit_rtc);

MODULE_DESCRIPTION("A Simple driver for get rtc info ");
MODULE_LICENSE("GPL");
