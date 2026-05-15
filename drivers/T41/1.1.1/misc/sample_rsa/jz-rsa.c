/*
 * Cryptographic API.
 *
 * Support for Tseries AES HW.
 *
 * Copyright (c) 2020 Ingenic Corporation
 * Author: Weijie Xu <weijie.xu@ingenic.com>
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
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <soc/base.h>
#if defined(CONFIG_SOC_T41)&&defined(CONFIG_KERNEL_4_4_94)
#include <dt-bindings/interrupt-controller/t41-irq.h>
#elif	defined(CONFIG_SOC_T40)
#include <dt-bindings/interrupt-controller/t40-irq.h>
#else
#include <soc/irq.h>
#endif

#include "jz-rsa.h"
#define RSA_CLOCK 400000000 //<=500M
#ifdef DEBUG
#define rsa_debug(format, ...) {printk(format, ## __VA_ARGS__);}
#else
#define rsa_debug(format, ...) do{ } while(0)
#endif

typedef unsigned int u32;
int little2big(u32 le)
{
	return ((le & 0x000000FF) << 24|(le &0x0000FF00)<<8|(le&0x00ff0000)>>8|(le & 0xff000000)>>24);
}
#if 1
static void debug_rsa_regs(rsa_operation_t *rsa)
{
	printk("RSAC   = 0x%08x\n", rsa_reg_read(rsa, RSAC));
	//printk("RSAE   = 0x%08x\n", rsa_reg_read(rsa, RSAE));
	//printk("RSAN   = 0x%08x\n", rsa_reg_read(rsa, RSAN));
	//printk("RSAM   = 0x%08x\n", rsa_reg_read(rsa, RSAM));
	printk("RSAP   = 0x%08x\n", rsa_reg_read(rsa, RSAP));
}
#endif

static ssize_t rsa_read(struct file *filp, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t rsa_write(struct file *filp,const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static int rsa_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *dev = filp->private_data;
	rsa_operation_t *rsa = miscdev_to_rsaops(dev);
	if(rsa->state){
		printk("RSA driver is busy,can't open again!\n");
		return -EBUSY;
	}
	rsa->state = 1;
	printk("%s:rsa open successsful!\n",__func__);
	return 0;
}

static long rsa_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	rsa_operation_t *rsa = miscdev_to_rsaops(dev);
	void __user *argp = (void __user *)arg;
	int rsa_time = 0;
	int i = 0;
	unsigned int rsa_buffer[64] = {0};
	switch(cmd){
		case IOCTL_RSA_ENC_DEC:
			{
				if (copy_from_user(&rsa->rsa_data, argp, sizeof(struct rsa_data))){
					printk("rsa get para copy_from_user error!!!\n");
					return -EFAULT;
				}
				rsa_bit_clr(rsa,RSAC,1<<0);//clear RSAN RSAM RSAP RSAE
				rsa_bit_set(rsa,RSAC,1<<0);//clear RSAN RSAM RSAP RSAE
				if(rsa->rsa_data.rsa_mode == 1024){//RSA1024
					rsa_bit_clr(rsa,RSAC,1<<7);
					rsa_time = 32;
				}else if(rsa->rsa_data.rsa_mode == 2048){//RSA2048
					rsa_bit_set(rsa,RSAC,1<<7);
					rsa_time = 64;
				}else{
					printk("rsa unsupport this mode!!!\n");
					return -EPERM;
				}

				/* write E / D*/
				for(i=0; i < rsa_time; i++){
					rsa_reg_write(rsa,RSAE, rsa->rsa_data.e_or_d[i]);
				}
				/* write N */
				for(i=0; i < rsa_time; i++){
					rsa_reg_write(rsa,RSAN, rsa->rsa_data.n[i]);
				}
				/* write M / G*/
				for(i=0; i < rsa_time; i++){
					rsa_reg_write(rsa,RSAM, rsa->rsa_data.input[i]);
				}
				/* start RSA to pre-process */
				rsa_bit_set(rsa, RSAC, 1<<1);

				/* Wait to pre-process done */
				i = 1000;
				while(i--){
					if((rsa_reg_read(rsa,RSAC) & (1<<2)))
						break;
				}

				/* start RSA to main-process */
				rsa_bit_set(rsa,RSAC,1<<4);

				/* Wait to main-process done */
				i = 1000;
				while(i--){
					if((rsa_reg_read(rsa,RSAC) & (1<<5)))
						break;
				}
				for(i = 0; i < rsa_time; i++){
					 while(rsa_reg_read(rsa,RSAC) & (1<<9));
					 rsa_buffer[rsa_time - i - 1] = rsa_reg_read(rsa,RSAP);
					 rsa_buffer[rsa_time - i - 1] = little2big(rsa_buffer[rsa_time - i - 1]);
				}
				if (copy_to_user((void __user *)rsa->rsa_data.output, rsa_buffer, rsa_time * sizeof(unsigned int))) {
					printk("rsa copy_to_user error!!!\n");
					return -EFAULT;
				}

				//	rsa_bit_set(rsa,RSAC,1<<2);
				//	rsa_bit_set(rsa,RSAC,1<<3);
				//	rsa_bit_set(rsa,RSAC,1<<5);
				//	rsa_bit_set(rsa,RSAC,1<<6);
				rsa_reg_write(rsa,RSAC, 1<<2 | 1<<3 | 1<<5 | 1<<6);
				rsa_reg_write(rsa,RSAC, 0x30000);
			}
			break;
		default:
			break;
	}
	return 0;
}

static int rsa_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *dev = filp->private_data;
	struct rsa_operation *rsa = miscdev_to_rsaops(dev);
	rsa->state = 0;
	return 0;
}

const struct file_operations rsa_fops = {
	.owner = THIS_MODULE,
	.read = rsa_read,
	.write = rsa_write,
	.open = rsa_open,
	.unlocked_ioctl = rsa_ioctl,
	.release = rsa_release,
};

static int jz_rsa_probe(struct platform_device *pdev)
{
	int ret = 0;
	rsa_operation_t *rsa_ope = (rsa_operation_t*)kzalloc(sizeof(rsa_operation_t),GFP_KERNEL);
	if(!rsa_ope){
		dev_err(&pdev->dev, "alloc rsa mem_region failed!\n");
		return -ENOMEM;
	}
	sprintf(rsa_ope->name,"jz-rsa");
	//platform_device mem resource get
	rsa_ope->io_res = platform_get_resource(pdev, IORESOURCE_MEM,0);
	if(!rsa_ope->io_res){
		dev_err(&pdev->dev, "failed to get dev io resources\n");
		ret = -EINVAL;
		goto failed_get_mem;
	}

	//I/O request
	rsa_ope->io_res = request_mem_region(rsa_ope->io_res->start,
				rsa_ope->io_res->end - rsa_ope->io_res->start + 1,
				pdev->name);
	if(!rsa_ope->io_res){
		dev_err(&pdev->dev, "failed to request rsa regs memory region");
		ret = -EINVAL;
		goto failed_req_region;
	}

	//ioremap
	rsa_ope->iomem = ioremap(rsa_ope->io_res->start,resource_size(rsa_ope->io_res));
	if(!rsa_ope->iomem){
		dev_err(&pdev->dev, "failed to remap regs memory region\n");
		ret = -EINVAL;
		goto failed_iomap;
	}
	rsa_debug("%s, rsa iomem is :0x%08x\n", __func__, (unsigned int)rsa_ope->iomem);

	rsa_ope->rsa_dev.minor = MISC_DYNAMIC_MINOR;
	rsa_ope->rsa_dev.fops  = &rsa_fops;
	rsa_ope->rsa_dev.name  = "rsa";
	rsa_ope->dev = &pdev->dev;

	ret = misc_register(&rsa_ope->rsa_dev);
	if(ret){
		dev_err(&pdev->dev,"request misc device failed!\n");
		ret = -EINVAL;
		goto failed_misc;
	}

#if (defined(CONFIG_SOC_T41)&&defined(CONFIG_KERNEL_4_4_94)) || defined(CONFIG_SOC_T40)
	rsa_ope->clk = clk_get(rsa_ope->dev,"gate_rsa");
	if (IS_ERR(rsa_ope->clk)){
		dev_err(&pdev->dev, "rsa clk get failed!\n");
		ret = -EINVAL;
		goto failed_get_clk;
	}
	rsa_ope->divclk = clk_get(rsa_ope->dev,"div_rsa");
	if (IS_ERR(rsa_ope->divclk)){
		dev_err(&pdev->dev, "rsa clk get failed!\n");
		ret = -EINVAL;
		goto failed_get_divclk;
	}
	struct clk *clk = clk_get(rsa_ope->dev,"mux_rsa");
//	struct clk *epll = clk_get(NULL, "epll");
//	ret = clk_set_rate(epll,1000000000);
	ret = clk_set_parent(clk, clk_get(NULL, "mpll"));
	if(ret){
		dev_err(&pdev->dev, "rsa clk set parent failed!\n");
	}
	ret = clk_set_rate(rsa_ope->divclk,RSA_CLOCK);
	if(ret){
		dev_err(&pdev->dev, "rsa clk set failed!\n");
		ret =-EINVAL;
		goto failed_setclk;
	}
	clk_prepare_enable(rsa_ope->clk);
	clk_prepare_enable(rsa_ope->divclk);
#else
	rsa_ope->clk = clk_get(rsa_ope->dev,"rsa");
	if (IS_ERR(rsa_ope->clk)){
		dev_err(&pdev->dev, "rsa clk get failed!\n");
		ret = -EINVAL;
		goto failed_get_clk;
	}
	struct clk *clk = clk_get(rsa_ope->dev,"cgu_rsa");
	ret = clk_set_rate(clk,RSA_CLOCK);
	if(ret){
		dev_err(&pdev->dev, "rsa clk set failed!\n");
		ret =-EINVAL;
		goto failed_setclk;
	}

	clk_enable(rsa_ope->clk);
#endif
	platform_set_drvdata(pdev, rsa_ope);
	rsa_debug("%s: probe() done\n", __func__);
	return 0;
failed_setclk:
failed_get_divclk:
failed_get_clk:
	misc_deregister(&rsa_ope->rsa_dev);
failed_misc:
	iounmap(rsa_ope->iomem);
failed_iomap:
	release_mem_region(rsa_ope->io_res->start, rsa_ope->io_res->end - rsa_ope->io_res->start + 1);
failed_req_region:
failed_get_mem:
	kfree(rsa_ope);
	return ret;
}

static int jz_rsa_remove(struct platform_device *pdev)
{
	rsa_operation_t *rsa_ope = platform_get_drvdata(pdev);
	misc_deregister(&rsa_ope->rsa_dev);
#if (defined(CONFIG_SOC_T41)&&defined(CONFIG_KERNEL_4_4_94)) || defined(CONFIG_SOC_T40)
	clk_disable_unprepare(rsa_ope->clk);
	clk_disable_unprepare(rsa_ope->divclk);
	devm_clk_put(&pdev->dev, rsa_ope->clk);
	devm_clk_put(&pdev->dev, rsa_ope->divclk);
#else
    clk_disable(rsa_ope->clk);
	clk_put(rsa_ope->clk);
#endif
	iounmap(rsa_ope->iomem);
	kfree(rsa_ope);
	return 0;
}

static void  platform_rsa_release(struct device *dev)
{
	return ;
}

static struct platform_driver jz_rsa_driver = {
	.probe = jz_rsa_probe,
	.remove = jz_rsa_remove,
	.driver = {
		.name = "jz-rsa",
		.owner = THIS_MODULE,
	}
};

static struct resource jz_rsa_resources[] = {
	[0] = {
		.start  = RSA_IOBASE,
		.end    = RSA_IOBASE + 0x10,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device jz_rsa_device = {
	.name   = "jz-rsa",
	.id = 0,
	.resource   = jz_rsa_resources,
	.dev = {
		.release = platform_rsa_release,
	},
	.num_resources  = ARRAY_SIZE(jz_rsa_resources),
};

static int __init jz_rsa_mod_init(void)
{
	int ret = 0;
	rsa_debug("loading %s driver\n", "jz-rsa");

	ret = platform_device_register(&jz_rsa_device);
	if(ret){
		printk("Failed to insmod rsa device driver!!!\n");
		return ret;
	}
	return platform_driver_register(&jz_rsa_driver);
}

static void __exit jz_rsa_mod_exit(void)
{
	platform_device_unregister(&jz_rsa_device);
	platform_driver_unregister(&jz_rsa_driver);
}

module_init(jz_rsa_mod_init);
module_exit(jz_rsa_mod_exit);

MODULE_DESCRIPTION("Ingenic RSA hw support");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Weijie Xu");
