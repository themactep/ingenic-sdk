/*
 * Cryptographic API.
 *
 * Copyright (c) 2016 Ingenic Corporation
 * Author: Elvis Wang <huan.wang@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/crypto.h>
#include <linux/interrupt.h>
#include <crypto/scatterwalk.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <soc/base.h>
#ifdef CONFIG_SOC_T40
#include <dt-bindings/interrupt-controller/t40-irq.h>
#else
#include <soc/irq.h>
#endif
#include "jz-dtrng.h"
#define SBUFF_SIZE		128
#define DEBUG
#ifdef DEBUG
#define dtrng_debug(format, ...) {printk(format, ## __VA_ARGS__);}
#else
#define dtrng_debug(format, ...) do{ } while(0)
#endif

static int dtrng_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	dtrng_operation_t *dtrng = miscdev_to_dtrngops(dev);
	dtrng->state = 0;
	return 0;
}

static ssize_t dtrng_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t dtrng_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static int dtrng_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	dtrng_operation_t *dtrng = miscdev_to_dtrngops(dev);
	if(dtrng->state){
		printk("dtrng driver is busy,can't open again!\n");
		return -EBUSY;
	}
	dtrng->state = 1;
	printk("%s:dtrng open successsful!\n",__func__);
	return 0;

}

unsigned int cnt = 0;
unsigned int cnt_t = 0;
unsigned int random_r = 0;
int dtrng_cpu_get_random(dtrng_operation_t *dtrng)
{
	static int i=0;
	unsigned int reg = 0;

	if(i==0){
		reg = dtrng_reg_read(dtrng, DTRNG_CFG);
		reg |= 8 << 1 | 1 << 11 | 1 << 0 | (dtrng->random[0]<<16);//div_num = 8, mask irq, enable dtrng
		dtrng_reg_write(dtrng, DTRNG_CFG, reg);
	}

	while(!(dtrng_reg_read(dtrng, DTRNG_STAT) & 0x1));

	dtrng->random[0] = dtrng_reg_read(dtrng, DTRNG_RANDOMNUM);
	return 0;
}

int dtrng_irqmode_random(dtrng_operation_t *dtrng)
{
	unsigned int reg = 0;
	dtrng_bit_clr(dtrng, 0x0, 11);   // no mask
	reg = dtrng_reg_read(dtrng, DTRNG_CFG);
	reg |= 8 << 1 | 1 << 0 | (dtrng->random[0]<<16);//div_num = 0, not mask irq, enable dtrng
	dtrng_reg_write(dtrng, DTRNG_CFG, reg);
	return 0;
}

static long dtrng_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = file->private_data;
    dtrng_operation_t *dtrng = miscdev_to_dtrngops(dev);
	void __user *argp  = (void __user *)arg;
	int ret = 0;

	switch(cmd) {
		case IOCTL_DTRNG_CPU_GET_RANDOM:

			if (copy_from_user(dtrng->random, argp , sizeof(unsigned int)))
			{
				printk("copy_from_user is error  : %s,%d\n",__func__,__LINE__);
				return -EFAULT;
			}
			dtrng_debug("data from user is %u\n",dtrng->random[0]);
			ret = dtrng_cpu_get_random(dtrng);
			if (ret) {
				printk("dtrng decrypt error!\n");
				return -1;
			}
			dtrng_debug("data to user is %u\n",dtrng->random[0]);
			if (copy_to_user(argp, &dtrng->random, sizeof(unsigned int))) {
				printk("dtrng get copy_to_user error!!!\n");
				return -EFAULT;
			}
			break;
		case IOCTL_DTRNG_DMA_GET_RANDOM:
			if (copy_from_user(dtrng->random, argp , sizeof(unsigned int)))
			{
				printk("copy_from_user is error  : %s,%d\n",__func__,__LINE__);
				return -EFAULT;
			}
			dtrng_debug("data from user is %u\n",dtrng->random[0]);

			ret = dtrng_irqmode_random(dtrng);
			if (ret) {
				printk("dtrng decrypt error!\n");
				return -1;
			}
			wait_for_completion(&dtrng->dtrng_complete);

			dtrng_debug("data to  user is %u\n",dtrng->random[0]);
			if (copy_to_user(argp, dtrng->random, sizeof(unsigned int))) {
				printk("dtrng get copy_to_user error!!!\n");
				return -EFAULT;
			}
		case IOCTL_DTRNG_GET_CNT:
			/*
			 *if (copy_to_user(argp, (void *)(&cnt), sizeof(unsigned int))) {
			 *    printk("dtrng get cnt copy_to_user error!!!\n");
			 *    return -EFAULT;
			 *}
			 */
			break;
		default:
			printk("%s default error\n", __func__);
	}
	return 0;
}

static irqreturn_t dtrng_ope_irq_handler(int irq, void *data)
{
	dtrng_operation_t *dtrng = data;
	dtrng->random[0] = dtrng_reg_read(dtrng, 0x4);
	printk("%s   random:	0x%08x\n", __func__,dtrng->random[0]);

	dtrng_bit_set(dtrng, 0x0, 12);//clear interrupt
	dtrng_bit_clr(dtrng, 0x0, 0);//disable dtrng
	dtrng_bit_set(dtrng, 0x0, 11);//mask the interrupt
	dtrng_bit_clr(dtrng, 0x0, 12);//normal work
	complete(&dtrng->dtrng_complete);
	return 0;
}

const struct file_operations dtrng_fops = {
	.owner = THIS_MODULE,
	.read = dtrng_read,
	.write = dtrng_write,
	.open = dtrng_open,
	.unlocked_ioctl = dtrng_ioctl,
	.release = dtrng_release,
};

dtrng_operation_t *dtrng_g = NULL;
static ssize_t dtrng_proc_read(struct file *filp, char __user * buff, size_t len, loff_t * offset)
{
#if 1
	len = strlen(dtrng_g->sbuff);
	if (*offset >= len) {
		return 0;
	}
	len -= *offset;
	if (copy_to_user((void __user *)buff, dtrng_g->sbuff + *offset, len)) {
		return -EFAULT;
	}
	*offset += len;
	printk("%s: sbuff = %s\n", __func__, dtrng_g->sbuff);

	return (ssize_t)len;
#endif
	return 0;
}

static ssize_t dtrng_proc_write(struct file *filp, const char __user * buff, size_t len, loff_t * offset)
{
#if 1
	int i = 0;
	int control[4] = {0};
	unsigned char *p = NULL;
	char *after = NULL;
	memset(dtrng_g->sbuff, 0, SBUFF_SIZE);
	len = len < SBUFF_SIZE ? len : SBUFF_SIZE;
	if (copy_from_user(dtrng_g->sbuff, buff, len)) {
		printk(KERN_INFO "[+dtrng_proc]: copy_from_user() error!\n");
		return -EFAULT;
	}
	p = dtrng_g->sbuff;
	control[0] = simple_strtoul(p, &after, 0);
	printk("control[0] = 0x%08x, after = %s\n", control[0], after);
	for (i = 1; i < 3; i++) {
		if (after[0] == ' ')
			after++;
		p = after;
		control[i] = simple_strtoul(p, &after, 0);
		printk("control[%d] = 0x%08x, after = %s\n", i, control[i], after);
	}
	for (i = 0; i < 3; i++) {
		printk("control[%d] = 0x%08x\n", i, control[i]);
	}
#if 1
	//echo 0/1 10000 > /proc/dtrng/jz_dtrng
	if (control[0] == 0) {
		dtrng_debug("Start IRQ Mode.\n");
		for (i = 0; i < control[1]; i++) {
			dtrng_g->random[0] = control[2];
			dtrng_irqmode_random(dtrng_g);
			wait_for_completion(&dtrng_g->dtrng_complete);
			//printk("%s   random:	0x%08x\n", __func__,dtrng_g->random[0]);
		}
	}

	if (control[0] == 1) {
		dtrng_debug("Start CPU Mode.\n");
		for (i = 0; i < control[1]; i++) {
			dtrng_g->random[0] = control[2];
			dtrng_cpu_get_random(dtrng_g);
			printk("random:	0x%08x\n", dtrng_g->random[0]);
		}
		dtrng_bit_set(dtrng_g, 0x0, 12);//clear interrupt
		dtrng_bit_clr(dtrng_g, 0x0, 12);//dtrng normal work
		dtrng_bit_clr(dtrng_g, 0x0, 0);//diable dirng
	}
#endif
#endif
	return len;
}

const struct file_operations dtrng_devices_fileops = {
	.owner		= THIS_MODULE,
	.read		= dtrng_proc_read,
	.write		= dtrng_proc_write,
};

static struct proc_dir_entry *proc_dtrng_dir = NULL;
static struct proc_dir_entry *entry = NULL;

static int jz_dtrng_probe(struct platform_device *pdev)
{
	int ret = 0;
	dtrng_operation_t *dtrng_ope = (dtrng_operation_t *)kzalloc(sizeof(dtrng_operation_t), GFP_KERNEL);
	if (!dtrng_ope) {
		dev_err(&pdev->dev, "alloc dtrng mem_region failed!\n");
		return -ENOMEM;
	}
	dtrng_g = dtrng_ope;
	sprintf(dtrng_ope->name, "jz-dtrng");
	printk("%s %d\n",__func__,__LINE__);
	dtrng_ope->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!dtrng_ope->res) {
		dev_err(&pdev->dev, "failed to get dev resources\n");
		ret = -EINVAL;
		goto failed_get_mem;
	}

	dtrng_ope->res = request_mem_region(dtrng_ope->res->start,
			dtrng_ope->res->end - dtrng_ope->res->start + 1,
			pdev->name);
	if (!dtrng_ope->res) {
		dev_err(&pdev->dev, "failed to request regs memory region");
		ret = -EINVAL;
		goto failed_req_region;
	}

	dtrng_ope->iomem = ioremap(dtrng_ope->res->start, resource_size(dtrng_ope->res));
	if (!dtrng_ope->iomem) {
		dev_err(&pdev->dev, "failed to remap regs memory region\n");
		ret = -EINVAL;
		goto failed_iomap;
	}
	dtrng_debug("%s, dtrng iomem is :0x%08x\n", __func__, (unsigned int)dtrng_ope->iomem);

	dtrng_ope->irq = platform_get_irq(pdev, 0);
	if (request_irq(dtrng_ope->irq, dtrng_ope_irq_handler, IRQF_SHARED, dtrng_ope->name, dtrng_ope)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto failed_get_irq;
	}
	dtrng_debug("%s, dtrng irq is : %d\n",__func__, dtrng_ope->irq);
	/*enable_irq(dtrng_ope->irq);*/

	/****************************************************************/

	dtrng_ope->dtrng_dev.minor = MISC_DYNAMIC_MINOR;
	dtrng_ope->dtrng_dev.fops = &dtrng_fops;
	dtrng_ope->dtrng_dev.name = "dtrng";
	dtrng_ope->dev = &pdev->dev;

	ret = misc_register(&dtrng_ope->dtrng_dev);
	if(ret) {
		dev_err(&pdev->dev,"request misc device failed!\n");
		ret = -EINVAL;
		goto failed_misc_register;
	}

#ifdef CONFIG_SOC_T40
	dtrng_ope->clk = clk_get(dtrng_ope->dev, "gate_dtrng");
	if (IS_ERR(dtrng_ope->clk)) {
		dev_err(&pdev->dev, "dtrng clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}

	clk_prepare_enable(dtrng_ope->clk);
#else
	dtrng_ope->clk = clk_get(dtrng_ope->dev, "dtrng");
	if (IS_ERR(dtrng_ope->clk)) {
		dev_err(&pdev->dev, "dtrng clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}

	clk_enable(dtrng_ope->clk);

#endif
	platform_set_drvdata(pdev,dtrng_ope);

	dtrng_ope->sbuff = kmalloc(SBUFF_SIZE, GFP_KERNEL);
	if(!dtrng_ope->sbuff){
		ret = -ENOMEM;
		goto failed_alloc_buf;
	}
#if 1
	proc_dtrng_dir = proc_mkdir("dtrng", NULL);
	if (!proc_dtrng_dir){
		ret = -ENOMEM;
		goto failed_mkdir_dtrng;
	}

	entry = proc_create("jz_dtrng", 0, proc_dtrng_dir,&dtrng_devices_fileops);
	if (!entry) {
		printk("%s: create proc_create error!\n", __func__);
		ret = -EINVAL;
		goto failed_create_dtrng;
	}
#endif
	init_completion(&dtrng_ope->dtrng_complete);
	dtrng_debug("%s: probe() done\n", __func__);
	return 0;
failed_create_dtrng:
	proc_remove(proc_dtrng_dir);
failed_mkdir_dtrng:
	kfree(dtrng_ope->sbuff);
failed_alloc_buf:
#ifdef CONFIG_SOC_T40
	clk_disable_unprepare(dtrng_ope->clk);
#else
	clk_disable(dtrng_ope->clk);
#endif
failed_clk:
	misc_deregister(&dtrng_ope->dtrng_dev);
failed_misc_register:
	 free_irq(dtrng_ope->irq, dtrng_ope);
failed_get_irq:
	iounmap(dtrng_ope->iomem);
failed_iomap:
	release_mem_region(dtrng_ope->res->start, dtrng_ope->res->end - dtrng_ope->res->start + 1);
failed_req_region:
failed_get_mem:
	kfree(dtrng_ope);
	return ret;
}

static int jz_dtrng_remove(struct platform_device *pdev)
{
	struct dtrng_operation *dtrng_ope = platform_get_drvdata(pdev);
	proc_remove(proc_dtrng_dir);
	proc_remove(entry);
#ifdef CONFIG_SOC_T40
	clk_disable_unprepare(dtrng_ope->clk);
	devm_clk_put(&pdev->dev, dtrng_ope->clk);
#else
	clk_disable(dtrng_ope->clk);
    clk_put(dtrng_ope->clk);
#endif
	misc_deregister(&dtrng_ope->dtrng_dev);
	free_irq(dtrng_ope->irq, dtrng_ope);
	iounmap(dtrng_ope->iomem);
	kfree(dtrng_ope->sbuff);
	kfree(dtrng_ope);
	return 0;
}

static void platform_dtrng_release(struct device *dev)
{
	return ;
}

static struct resource jz_dtrng_resources[] = {
	[0] = {
		.start  = DTRNG_IOBASE,
		.end    = DTRNG_IOBASE + 0x20,
		.flags  = IORESOURCE_MEM,
	},
#ifdef CONFIG_SOC_T40
	[1] = {
		.start  = IRQ_DTRNG+8,
		.end    = IRQ_DTRNG+8,
		.flags  = IORESOURCE_IRQ,
	},
#else
	[1] = {
		.start  = IRQ_DTRNG,
		.end    = IRQ_DTRNG,
		.flags  = IORESOURCE_IRQ,
	},
#endif
};

struct platform_device jz_dtrng_device = {
	.name = "jz-dtrng",
	.id = 0,
	.resource = jz_dtrng_resources,
	.dev = {
		.release = platform_dtrng_release,
	},
	.num_resources = ARRAY_SIZE(jz_dtrng_resources),
};

static struct platform_driver jz_dtrng_driver = {
	.probe	= jz_dtrng_probe,
	.remove	= jz_dtrng_remove,
	.driver	= {
		.name	= "jz-dtrng",
		.owner	= THIS_MODULE,
	},
};

static int __init jz_dtrng_mod_init(void)
{
	int ret = 0;
	dtrng_debug("loading %s driver\n", "jz-dtrng");
	ret = platform_device_register(&jz_dtrng_device);
	if(ret){
		printk("Failed to insmod dtrng device!!!\n");
		return ret;
	}
	return platform_driver_register(&jz_dtrng_driver);
}

static void __exit jz_dtrng_mod_exit(void)
{
	platform_device_unregister(&jz_dtrng_device);
	platform_driver_unregister(&jz_dtrng_driver);
}

module_init(jz_dtrng_mod_init);
module_exit(jz_dtrng_mod_exit);

MODULE_DESCRIPTION("Ingenic DTRNG hw support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Elvis Wang");
