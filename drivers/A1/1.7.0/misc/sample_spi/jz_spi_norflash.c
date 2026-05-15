/*
 * linux/drivers/mtd/devices/jz_spi_norflash.c
 *
 * Add spi-norflsah to mtd
 *
 * Copyright (c) 2014 Ingenic
 * Author:Tiger <bohu.liang@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <soc/gpio.h>
#include "jz_spi_norflash.h"


static unsigned int spi_bus = 0;
module_param(spi_bus, int, S_IRUGO);
MODULE_PARM_DESC(spi_bus, "input spi bus number");

#define SPI_CMD_READ 0
#define SPI_CMD_WRITE 1
#define SPI_CMD_ERASE 2
#define SPI_CMD_PARAM 3
#define SPI_CMD_SIZE 4
struct jz_spi_param {
	unsigned int pagesize;
	unsigned int ppb;
	unsigned int totalblock;
};

struct jz_spi_opsparam {
	unsigned int page;
	unsigned int len;
	void *data;
};

/* Max time can take up to 3 seconds! */
#define MAX_READY_WAIT_TIME	3000	/* the time of erase BE(64KB) */

struct jz_spi_norflash {
	struct miscdevice misc_dev;
	struct spi_device	*spi;
	struct device *dev;
	struct mutex		lock;
	int flag;
	struct spi_nor_platform_data *pdata;
	struct spi_nor_block_info *binfo;
	void *tmpbuf;
};

struct spi_device_id jz_id_table[] = {
	{
		.name = "jz_spi_norflash",
	},
};

static struct spi_nor_block_info flash_block_info[] = {
	{
		.blocksize      = 64 * 1024,
		.cmd_blockerase = 0xD8,
		.be_maxbusy     = 1200,  /* 1.2s */
	},

	{
		.blocksize      = 32 * 1024,
		.cmd_blockerase = 0x52,
		.be_maxbusy     = 1000,  /* 1s */
	},
};
static struct spi_nor_platform_data spi_nor_pdata = {
	.name			= "MX25L12835F",
	.pagesize       = 256,
	.sectorsize     = 4 * 1024,
	.chipsize       = 16384 * 1024,
	.erasesize      = 32 * 1024,
	.id             = 0xc22018,

	.block_info     = flash_block_info,
	.num_block_info = ARRAY_SIZE(flash_block_info),

	.addrsize       = 3,
	.pp_maxbusy     = 3,            /* 3ms */
	.se_maxbusy     = 400,          /* 400ms */
	.ce_maxbusy     = 8 * 10000,    /* 80s */

	.st_regnum      = 3,
};

static inline int jz_spi_write(struct spi_device *spi, const void *buf, size_t len)
{
	struct spi_transfer     t = {
			.tx_buf         = buf,
			.len            = len,
			.cs_change  = 1,
		};
	struct spi_message      m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}

static int jz_spi_norflash_status(struct jz_spi_norflash *flash, int *status)
{
	int ret;
	unsigned char command[1];
	struct spi_message message;
	struct spi_transfer transfer[2];

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	command[0] = SPINOR_OP_RDSR;

	transfer[0].tx_buf = command;
	transfer[0].len = sizeof(command);
	spi_message_add_tail(&transfer[0], &message);

	transfer[1].rx_buf = command;
	transfer[1].len = sizeof(command);
	transfer[1].cs_change = 1;
	spi_message_add_tail(&transfer[1], &message);

	ret = spi_sync(flash->spi, &message);
	if (ret)
		return ret;

	*status = command[0];

	return 0;
}

static int jz_spi_norflash_wait_till_ready(struct jz_spi_norflash *flash)
{
	int status, ret;
	unsigned long deadline;

	deadline = jiffies + msecs_to_jiffies(MAX_READY_WAIT_TIME);
	do {
		ret = jz_spi_norflash_status(flash, &status);
		if (ret)
			return ret;

		if (!(status & SR_WIP))
			return 0;

		cond_resched();
	} while (!time_after_eq(jiffies, deadline));

	return -ETIMEDOUT;
}

static int jz_spi_norflash_write_enable(struct jz_spi_norflash *flash)
{
	int status, ret;
	unsigned char command[2];

	command[0] = SPINOR_OP_WREN;
	ret = jz_spi_write(flash->spi, command, 1);
	if (ret) {
		return ret;
	}

	command[0] = SPINOR_OP_WRSR;
	command[1] = 0;
	ret = jz_spi_write(flash->spi, command, 2);
	if (ret) {
		return ret;
	}

	ret = jz_spi_norflash_wait_till_ready(flash);
	if (ret)
		return ret;

	command[0] = SPINOR_OP_WREN;
	ret = jz_spi_write(flash->spi, command, 1);
	if (ret) {
		return ret;
	}

	ret = jz_spi_norflash_status(flash, &status);
	if (ret) {
		return ret;
	}

	if (!(status & SR_WEL)) {
		return -EROFS;
	}

	return 0;
}

static int jz_spi_norflash_erase_sector(struct jz_spi_norflash *flash, uint32_t offset)
{
	int ret;
	unsigned char command[4];

	ret = jz_spi_norflash_write_enable(flash);
	if (ret)
		return ret;

	switch(flash->binfo->blocksize) {
	case 0x1000:
		command[0] = SPINOR_OP_BE_4K;
		break;
	case 0x8000:
		command[0] = SPINOR_OP_BE_32K;
		break;
	case 0x10000:
		command[0] = SPINOR_OP_SE;
		break;
	}

	command[1] = offset >> 16;
	command[2] = offset >> 8;
	command[3] = offset;
	ret = jz_spi_write(flash->spi, command, 4);
	if (ret)
		return ret;

	ret = jz_spi_norflash_wait_till_ready(flash);
	if (ret)
		return ret;

	return 0;
}

static int jz_spi_norflash_erase(struct jz_spi_norflash *flash, unsigned int blockid)
{
	int ret;
	mutex_lock(&flash->lock);
	ret = jz_spi_norflash_wait_till_ready(flash);
	if (ret) {
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("spi wait timeout !\n");
		mutex_unlock(&flash->lock);
		return ret;
	}

	ret = jz_spi_norflash_erase_sector(flash, blockid * flash->binfo->blocksize);
	if (ret) {
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("erase error !\n");
		mutex_unlock(&flash->lock);
		return ret;
	}

	mutex_unlock(&flash->lock);

	return 0;
}

static int jz_spi_norflash_read(struct jz_spi_norflash *flash, unsigned int from,
		size_t len, size_t *retlen, unsigned char *buf)
{
	int ret;
	unsigned char command[5];
	struct spi_message message;
	struct spi_transfer transfer[2];

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	command[0] = SPINOR_OP_READ_FAST;
	command[1] = from >> 16;
	command[2] = from >> 8;
	command[3] = from;
	command[4] = 0;

	transfer[0].tx_buf = command;
	transfer[0].len = sizeof(command);
	spi_message_add_tail(&transfer[0], &message);

	transfer[1].rx_buf = buf;
	transfer[1].len = len;
	transfer[1].cs_change = 1;
	spi_message_add_tail(&transfer[1], &message);

	mutex_lock(&flash->lock);

	ret = jz_spi_norflash_wait_till_ready(flash);
	if (ret) {
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("spi wait timeout !\n");
		mutex_unlock(&flash->lock);
		return ret;
	}

	ret = spi_sync(flash->spi, &message);
	if(ret) {
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("spi_sync() error !\n");
		return ret;
	}

	*retlen = message.actual_length - sizeof(command);

	mutex_unlock(&flash->lock);

	return 0;
}

static int jz_spi_norflash_write(struct jz_spi_norflash *flash, unsigned int to, size_t len,
			size_t *retlen, unsigned char *buf)
{
	unsigned int ret;
	struct spi_message message;
	struct spi_transfer transfer[1];
	unsigned char *command = buf;

	*retlen = 0;

	mutex_lock(&flash->lock);

	ret = jz_spi_norflash_write_enable(flash);
	if (ret) {
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("write enable error !\n");
		mutex_unlock(&flash->lock);
		return ret;
	}

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	command[0] = SPINOR_OP_PP;
	command[1] = to >> 16;
	command[2] = to >> 8;
	command[3] = to;

	transfer[0].tx_buf = command;
	transfer[0].len = len + 4;
	spi_message_add_tail(&transfer[0], &message);

	ret = jz_spi_norflash_wait_till_ready(flash);
	if (ret) {
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("wait timeout !\n");
		mutex_unlock(&flash->lock);
		return ret;
	}

	transfer[0].cs_change = 1;
	ret = spi_sync(flash->spi, &message);
	if(ret) {
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("write error !\n");
		return ret;
	}

	*retlen += (message.actual_length - 4);

	mutex_unlock(&flash->lock);

	if(*retlen != len)
		printk("ret len error!\n");

	return 0;
}

static  int jz_spi_norflash_match_device(struct spi_device *spi,int chip_id)
{
	int ret;
	unsigned int id = 0;
	unsigned char send_command[1], recv_command[3];
	struct spi_message message;
	struct spi_transfer transfer[2];

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	send_command[0] = SPINOR_OP_RDID;

	transfer[0].tx_buf = send_command;
	transfer[0].len = sizeof(send_command);
	spi_message_add_tail(&transfer[0], &message);

	transfer[1].rx_buf = recv_command;
	transfer[1].len = sizeof(recv_command);
	transfer[1].cs_change = 1;
	spi_message_add_tail(&transfer[1], &message);

	ret = spi_sync(spi, &message);
	if (ret) {
		printk("error reading device id\n");
		return EINVAL;
	}

	id = (recv_command[0] << 16) | (recv_command[1] << 8) | recv_command[2];

	//printk("nor flash id = 0x%x\n",id);
	if(id == chip_id){
		printk("the spi mtd chip id is %x\n",id);
	}else{
		printk("unknow chip id %x\n",id);
		return EINVAL;
	}

	return 0;
}

static int spiflash_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct jz_spi_norflash *flash = container_of(dev, struct jz_spi_norflash, misc_dev);
	int ret = 0;
	if(flash->flag){
		ret = -EBUSY;
		dev_err(flash->dev, "spiflash driver busy now!\n");
	}else{
		flash->flag = 1;
	}

	return ret;
}

static int spiflash_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct jz_spi_norflash *flash = container_of(dev, struct jz_spi_norflash, misc_dev);
	flash->flag = 0;
	return 0;
}

static long spiflash_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	struct jz_spi_norflash *flash = container_of(dev, struct jz_spi_norflash, misc_dev);
	struct jz_spi_opsparam ops;
	struct jz_spi_param param;
	long ret = 0;
	size_t reallen;

	if(flash->flag == 0){
		printk("Please Open /dev/spiflash Firstly\n");
		return -EPERM;
	}

	switch (cmd) {
		case SPI_CMD_READ:
			if (copy_from_user(&ops, (void __user *)arg, sizeof(ops))) {
				dev_err(flash->dev, "[%s][%d] copy to user error\n", __func__, __LINE__);
				return -EFAULT;
			}
			if(ops.len > flash->pdata->pagesize){
				dev_err(flash->dev, "[%s][%d] can't read the %d Bytes, the pagesize is %d\n", __func__, __LINE__, ops.len, flash->pdata->pagesize);
				return -EFAULT;
			};
			printk("Begin to read page %d, len = %d\n", ops.page, ops.len);
			ret = jz_spi_norflash_read(flash, ops.page * flash->pdata->pagesize, ops.len, &reallen, flash->tmpbuf);
			if(ret){
				dev_err(flash->dev, "[%s][%d] Failed to read page[%d]\n", __func__, __LINE__, ops.page);
				return -EFAULT;
			}
			ops.len = reallen;
			if (copy_to_user((void __user *)ops.data, flash->tmpbuf, reallen)) {
				dev_err(flash->dev, "[%s][%d] copy to user error\n", __func__, __LINE__);
				return -EFAULT;
			}

			if (copy_to_user((void __user *)arg, &ops, sizeof(ops))) {
				dev_err(flash->dev, "[%s][%d] copy to user error\n", __func__, __LINE__);
				return -EFAULT;
			}
			break;
		case SPI_CMD_WRITE:
			if (copy_from_user(&ops, (void __user *)arg, sizeof(ops))) {
				dev_err(flash->dev, "[%s][%d] copy from user error\n", __func__, __LINE__);
				return -EFAULT;
			}
			if(ops.len > flash->pdata->pagesize){
				dev_err(flash->dev, "[%s][%d] can't read the %d Bytes, the pagesize is %d\n", __func__, __LINE__, ops.len, flash->pdata->pagesize);
				return -EFAULT;
			};
			printk("Begin to write page %d, len = %d\n", ops.page, ops.len);
			if (copy_from_user(flash->tmpbuf + SPI_CMD_SIZE, (void __user *)ops.data, ops.len)) {
				dev_err(flash->dev, "[%s][%d] copy from user error\n", __func__, __LINE__);
				return -EFAULT;
			}
			ret = jz_spi_norflash_write(flash, ops.page * flash->pdata->pagesize, ops.len, &reallen, flash->tmpbuf);
			if(ret){
				dev_err(flash->dev, "[%s][%d] Failed to write page[%d]\n", __func__, __LINE__, ops.page);
				return -EFAULT;
			}
			break;
		case SPI_CMD_ERASE:
    		printk("%s[%d]: erase block is %ld\n",__func__,__LINE__, arg);
			jz_spi_norflash_erase(flash, arg);
			break;
		case SPI_CMD_PARAM:
			param.pagesize = flash->pdata->pagesize;
			param.ppb = flash->binfo->blocksize / flash->pdata->pagesize;
			param.totalblock = flash->pdata->chipsize / flash->binfo->blocksize;
			if (copy_to_user((void __user *)arg, &param, sizeof(param))) {
				dev_err(flash->dev, "[%s][%d] copy to user error\n", __func__, __LINE__);
				return -EFAULT;
			}
			break;
		default:
			return -EINVAL;
	}

	return ret;
}

static struct file_operations spiflash_fops = {
	.open = spiflash_open,
	.release = spiflash_release,
	.unlocked_ioctl = spiflash_ioctl,
};

static int jz_spi_norflash_probe(struct spi_device *spi)
{
	int ret;
	struct jz_spi_norflash *flash;
	struct spi_nor_platform_data *pdata = spi->dev.platform_data;
	int chip_id = 0;

	chip_id = pdata->id;
	flash = kzalloc(sizeof(struct jz_spi_norflash), GFP_KERNEL);
	if (!flash) {
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("kzalloc() error !\n");
		return -ENOMEM;
	}

    printk("%s[%d]: \n",__func__,__LINE__);
	ret = gpio_request(GPIO_PB(10), "norflash hold");
	gpio_direction_output(GPIO_PB(10), 1);

	ret = gpio_request(GPIO_PB(11), "norflash wp");
	gpio_direction_output(GPIO_PB(11), 1);

	ret = jz_spi_norflash_match_device(spi, chip_id);
	if (ret ) {
		printk("unknow id ,the id not match the spi bsp config\n");
		ret = -ENODEV;
		goto out;
	}

	flash->tmpbuf = kzalloc(pdata->pagesize + SPI_CMD_SIZE, GFP_KERNEL);
	if(!flash->tmpbuf){
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("kzalloc() error !\n");
		ret = -ENOMEM;
		goto out;
	}
	flash->spi = spi;
	flash->pdata = pdata;
	flash->binfo = pdata->block_info;
	flash->dev = &spi->dev;
	mutex_init(&flash->lock);
	dev_set_drvdata(&spi->dev, flash);

	flash->misc_dev.minor = MISC_DYNAMIC_MINOR;
	flash->misc_dev.name = "spinor";
	flash->misc_dev.fops = &spiflash_fops;
	ret = misc_register(&flash->misc_dev);
	if (ret < 0) {
		ret = -ENOENT;
		dev_err(&spi->dev, "misc_register failed\n");
		goto error_misc_register;
	}


	printk("SPI NOR MTD LOAD OK\n");
	return 0;
error_misc_register:
	kfree(flash->tmpbuf);
out:
	gpio_free(GPIO_PB(10));
	gpio_free(GPIO_PB(11));
	kfree(flash);
	return ret;
}

static int jz_spi_norflash_remove(struct spi_device *spi)
{
	int ret;
	struct jz_spi_norflash *flash;

	flash = dev_get_drvdata(&spi->dev);
	if(!flash)
		return 0;

	gpio_free(GPIO_PB(10));
	gpio_free(GPIO_PB(11));
	return ret;
}

static struct spi_driver jz_spi_norflash_driver = {
	.driver = {
		.name	= "jz_spi_norflash",
		.owner	= THIS_MODULE,
	},
	.id_table	= jz_id_table,
	.probe		= jz_spi_norflash_probe,
	.remove		= jz_spi_norflash_remove,
	.shutdown	= NULL, //forever start
};

struct spi_board_info jz_spi_board_info = {
	.modalias       =  "jz_spi_norflash",
	.platform_data          = &spi_nor_pdata,
	.controller_data        = 0, /* cs for spi gpio */
	.max_speed_hz           = 25000000,
	.bus_num                = 0,
	.chip_select            = 0,
	.mode = 0,
};

static int __init jz_spi_norflash_driver_init(void)
{
	struct spi_master *master = spi_busnum_to_master(spi_bus);
	struct spi_device * spidev = NULL;
	jz_spi_board_info.bus_num = spi_bus;
	spidev = spi_new_device(master, &jz_spi_board_info);
	return spi_register_driver(&jz_spi_norflash_driver);
}
static void __exit jz_spi_norflash_driver_exit(void)
{
	spi_unregister_driver(&jz_spi_norflash_driver);
}

module_init(jz_spi_norflash_driver_init);
module_exit(jz_spi_norflash_driver_exit);
MODULE_AUTHOR("xhshen <nick.xhshen@ingenic.com>");
MODULE_DESCRIPTION("spi device driver");
MODULE_LICENSE("GPL v2");
