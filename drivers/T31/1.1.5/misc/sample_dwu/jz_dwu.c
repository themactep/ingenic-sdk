/*
 * Copyright (c) 2015 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * Input file for Ingenic DWU driver
 *
 * This  program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/suspend.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/time.h>
#include <soc/base.h>
#include <dt-bindings/interrupt-controller/t40-irq.h>
#include "jz_dwu.h"


#define DEBUG
#ifdef	DEBUG
static int debug_dwu = 1;

#define DWU_DEBUG(format, ...) { if (debug_dwu) printk(format, ## __VA_ARGS__);}
#else
#define DWU_DEBUG(format, ...) do{ } while(0)
#endif


struct dwu_reg_struct jz_dwu_regs_name[] = {
	{"DWU_CTRL", DWU_CTRL},
	{"DWU_PROTECT_START_ADDR", DWU_PROTECT_START_ADDR},
	{"DWU_PROTECT_END_ADDR", DWU_PROTECT_END_ADDR},
	{"DWU_BAD_ADDR", DWU_BAD_ADDR},
	{"DWU_DEVICE0_CTRL", DWU_DEVICE0_CTRL},
	{"DWU_DEVICE1_CTRL", DWU_DEVICE1_CTRL},
	{"DWU_DEVICE2_CTRL", DWU_DEVICE2_CTRL},
	{"DWU_DEVICE3_CTRL", DWU_DEVICE3_CTRL},
	{"DWU_INT_CLR", DWU_INT_CLR},
	{"DWU_ERR_ADDR", DWU_ERR_ADDR},
	{"DWU_ERR_INFO", DWU_ERR_INFO},

};


static void reg_bit_set(struct jz_dwu *dwu, int offset, unsigned int bit)
{
	unsigned int reg = 0;
	reg = reg_read(dwu, offset);
	reg |= bit;
	reg_write(dwu, offset, reg);
}

static void reg_bit_clr(struct jz_dwu *dwu, int offset, unsigned int bit)
{
	unsigned int reg = 0;
	reg= reg_read(dwu, offset);
	reg &= ~(bit);
	reg_write(dwu, offset, reg);
}


static int _dwu_dump_regs(struct jz_dwu *dwu)
{
	int i = 0;
	int num = 0;

	if (dwu == NULL) {
		dev_err(dwu->dev, "dwu is NULL!\n");
		return -1;
	}
	printk("----- dump regs -----\n");

	num = sizeof(jz_dwu_regs_name) / sizeof(struct dwu_reg_struct);
	for (i = 0; i < num; i++) {
		printk("dwu_reg: %s: \t0x%08x\r\n", jz_dwu_regs_name[i].name, reg_read(dwu, jz_dwu_regs_name[i].addr));
	}

	return 0;
}

static void _dwu_dump_param(struct jz_dwu *dwu)
{
	return;
}

static int dwu_dump_info(struct jz_dwu *dwu)
{
	int ret = 0;
	if (dwu == NULL) {
		dev_err(dwu->dev, "dwu is NULL\n");
		return -1;
	}
	printk("dwu: dwu->base: %p\n", dwu->iomem);
	_dwu_dump_param(dwu);
	ret = _dwu_dump_regs(dwu);

	return ret;
}

static int dwu_reset_reg(struct jz_dwu *dwu)
{

    reg_write(dwu, DWU_CTRL, 0x00000000);
    reg_write(dwu, DWU_PROTECT_START_ADDR, 0x00000000);
    reg_write(dwu, DWU_PROTECT_END_ADDR, 0x00000000);
    reg_write(dwu, DWU_BAD_ADDR, 0x00000000);
    reg_write(dwu, DWU_DEVICE0_CTRL, 0x00000000);
    reg_write(dwu, DWU_DEVICE1_CTRL, 0x00000000);
    reg_write(dwu, DWU_DEVICE2_CTRL, 0x00000000);
    reg_write(dwu, DWU_DEVICE3_CTRL, 0x00000000);
    reg_write(dwu, DWU_ERR_ADDR, 0x00000000);

    return 0;
}

static int dwu_reg_set(struct jz_dwu *dwu, struct dwu_param *dwu_param)
{
    unsigned int addrl = 0;
    unsigned int addrh = 0;
    unsigned int addrerr = 0;
    unsigned int func_id0 = 0, func_id1 = 0, func_id2 = 0, func_id3 = 0;
    unsigned int dev_num = 0;

    struct dwu_param *ip = dwu_param;
    if (dwu == NULL) {
		dev_err(dwu->dev, "dwu: dwu is NULL or dwu_param is NULL\n");
		return -1;
	}

    addrl = ip->dwu_start_addr;
    addrh = ip->dwu_end_addr;
    addrerr = ip->dwu_err_addr;
    dev_num = ip->dwu_dev_num;
    func_id0 = ip->dwu_id0;
    func_id1 = ip->dwu_id1;
    func_id2 = ip->dwu_id2;
    func_id3 = ip->dwu_id3;

    printk("func_id0 = %d func_id1 = %d func_id2 = %d func_id3 = %d dev_num = %d\n", func_id0, func_id1, func_id2, func_id3, dev_num);
    printk("addrerr = 0x%08x\n", addrerr);
    printk("addrl = 0x%08x addrh = 0x%08x\n", addrl, addrh);

    switch(dev_num) {
        case 1:
            reg_write(dwu, DWU_DEVICE0_CTRL, ((func_id0 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE0_EN));
            break;
        case 2:
            reg_write(dwu, DWU_DEVICE0_CTRL, ((func_id0 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE0_EN));
            reg_write(dwu, DWU_DEVICE1_CTRL, ((func_id1 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE1_EN));
            break;
        case 3:
            reg_write(dwu, DWU_DEVICE0_CTRL, ((func_id0 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE0_EN));
            reg_write(dwu, DWU_DEVICE1_CTRL, ((func_id1 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE1_EN));
            reg_write(dwu, DWU_DEVICE2_CTRL, ((func_id2 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE2_EN));
            break;
        case 4:
            reg_write(dwu, DWU_DEVICE0_CTRL, ((func_id0 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE0_EN));
            reg_write(dwu, DWU_DEVICE1_CTRL, ((func_id1 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE1_EN));
            reg_write(dwu, DWU_DEVICE2_CTRL, ((func_id2 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE2_EN));
            reg_write(dwu, DWU_DEVICE3_CTRL, ((func_id3 << DEVICE_NUM) | WR_FLAG | RD_FLAG | DEVICE3_EN));
            break;
        default:
            break;
    }

    reg_write(dwu, DWU_PROTECT_START_ADDR, addrl);
    reg_write(dwu, DWU_PROTECT_END_ADDR, addrh);
    reg_write(dwu, DWU_BAD_ADDR, addrerr);

    return 0;
}

static int dwu_start(struct jz_dwu *dwu, struct dwu_param *dwu_param)
{
	int ret = 0;
	struct dwu_param *ip = dwu_param;

	if ((dwu == NULL) || (dwu_param == NULL)) {
		dev_err(dwu->dev, "dwu: dwu is NULL or dwu_param is NULL\n");
		return -1;
	}
	DWU_DEBUG("dwu: enter dwu_start %d\n", current->pid);

#ifndef CONFIG_FPGA_TEST
	clk_enable(dwu->clk);
#ifdef CONFIG_SOC_T40
	clk_enable(dwu->clk);
#endif
#endif

    dwu_reset_reg(dwu);

    /* open ddr lock can operation register */
    reg_write(dwu, DWU_LOCK, 0x00000000);

    ret = dwu_reg_set(dwu, ip);

	/* start dwu */
	__start_dwu_axi();
	__start_dwu_ahb();

#ifdef DEBUG
	dwu_dump_info(dwu);
#endif
	DWU_DEBUG("dwu_start\n");

	//ret = wait_for_completion_interruptible(&dwu->done_dwu);
	ret = wait_for_completion_interruptible_timeout(&dwu->done_dwu, msecs_to_jiffies(8000));
	if (ret < 0) {
		printk("dwu: done_dwu wait_for_completion_interruptible_timeout err %d\n", ret);
		goto err_dwu_wait_for_done;
	} else if (ret == 0 ) {
		ret = -1;
		printk("dwu: done_dwu wait_for_completion_interruptible_timeout timeout %d\n", ret);
		dwu_dump_info(dwu);
		goto err_dwu_wait_for_done;
	} else {
		;
	}

    DWU_DEBUG("dwu: exit dwu_start %d\n", current->pid);

#ifndef CONFIG_FPGA_TEST
#ifdef CONFIG_SOC_T40
	clk_disable(dwu->clk);
#endif
#endif
    return 0;

err_dwu_wait_for_done:
    __stop_dwu_axi();
    __stop_dwu_ahb();
    __irq_clear_dwu();
    reg_write(dwu, DWU_LOCK, 0x00000001);
#ifndef CONFIG_FPGA_TEST
#ifdef CONFIG_SOC_T40
	clk_disable(dwu->clk);
#endif
#endif
	return ret;

}

static long dwu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct dwu_param iparam;
	struct miscdevice *dev = filp->private_data;
	struct jz_dwu *dwu = container_of(dev, struct jz_dwu, misc_dev);

	DWU_DEBUG("dwu: %s pid: %d, tgid: %d file: %p, cmd: 0x%08x\n",
			__func__, current->pid, current->tgid, filp, cmd);

	if (_IOC_TYPE(cmd) != JZDWU_IOC_MAGIC) {
		dev_err(dwu->dev, "invalid cmd!\n");
		return -EFAULT;
	}

	mutex_lock(&dwu->mutex);

	switch (cmd) {
		case IOCTL_DWU_START:
			if (copy_from_user(&iparam, (void *)arg, sizeof(struct dwu_param))) {
				dev_err(dwu->dev, "copy_from_user error!!!\n");
				ret = -EFAULT;
				break;
			}
			ret = dwu_start(dwu, &iparam);
			if (ret) {
				printk("dwu: error dwu start ret = %d\n", ret);
			}
			break;
		case IOCTL_DWU_BUF_LOCK:
			ret = wait_for_completion_interruptible_timeout(&dwu->done_buf, msecs_to_jiffies(2000));
			if (ret < 0) {
				printk("dwu: done_buf wait_for_completion_interruptible_timeout err %d\n", ret);
			} else if (ret == 0 ) {
				printk("dwu: done_buf wait_for_completion_interruptible_timeout timeout %d\n", ret);
				ret = -1;
				dwu_dump_info(dwu);
			} else {
				ret = 0;
			}
			break;
		case IOCTL_DWU_BUF_UNLOCK:
			complete(&dwu->done_buf);
			break;
	    default:
			dev_err(dwu->dev, "invalid command: 0x%08x\n", cmd);
			ret = -EINVAL;
	}

	mutex_unlock(&dwu->mutex);
	return ret;
}

static int dwu_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct miscdevice *dev = filp->private_data;
	struct jz_dwu *dwu = container_of(dev, struct jz_dwu, misc_dev);

	DWU_DEBUG("dwu: %s pid: %d, tgid: %d filp: %p\n",
			__func__, current->pid, current->tgid, filp);
	mutex_lock(&dwu->mutex);

	mutex_unlock(&dwu->mutex);
	return ret;
}

static int dwu_release(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct miscdevice *dev = filp->private_data;
	struct jz_dwu *dwu = container_of(dev, struct jz_dwu, misc_dev);

	DWU_DEBUG("dwu: %s  pid: %d, tgid: %d filp: %p\n",
			__func__, current->pid, current->tgid, filp);
	mutex_lock(&dwu->mutex);

	mutex_unlock(&dwu->mutex);
	return ret;
}

static struct file_operations dwu_ops = {
	.owner = THIS_MODULE,
	.open = dwu_open,
	.release = dwu_release,
	.unlocked_ioctl = dwu_ioctl,
};

static irqreturn_t dwu_irq_handler(int irq, void *data)
{
	struct jz_dwu *dwu;
	unsigned int status;
	unsigned int err_addr = 0, err_info = 0;

	DWU_DEBUG("dwu: %s\n", __func__);
	dwu = (struct jz_dwu *)data;

    status = reg_read(dwu, DWU_INT_REG);

    err_addr = reg_read(dwu, DWU_ERR_ADDR);
    err_info = reg_read(dwu, DWU_ERR_INFO);

    printk("----- err_addr = 0x%08x err_info = 0x%08x -----\n", err_addr, err_info);

//    __irq_clear_dwu();
    reg_write(dwu, DWU_INT_CLR, status);

    DWU_DEBUG("----- %s\n", __func__);
    DWU_DEBUG("----- %s, status= 0x%08x\n", __func__, status);
	 /* this status doesn't do anything including trigger interrupt,
	 * just give a hint */
//    if (status & 0x7f){
        complete(&dwu->done_dwu);
  //  }

#ifdef DEBUG
    DWU_DEBUG("----------------------interrupt handler dump------------------\n");
	dwu_dump_info(dwu);
    DWU_DEBUG("----------------------interrupt handler end ------------------\n");
#endif

    __stop_dwu_axi();
    __stop_dwu_ahb();
    __irq_clear_dwu();
    reg_write(dwu, DWU_LOCK, 0x00000001);
    return IRQ_HANDLED;
}

static int dwu_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct jz_dwu *dwu;

	DWU_DEBUG("%s\n", __func__);
	dwu = (struct jz_dwu *)kzalloc(sizeof(struct jz_dwu), GFP_KERNEL);
	if (!dwu) {
		dev_err(&pdev->dev, "alloc jz_dwu failed!\n");
		return -ENOMEM;
	}

	sprintf(dwu->name, "dwu");

	dwu->misc_dev.minor = MISC_DYNAMIC_MINOR;
	dwu->misc_dev.name = dwu->name;
	dwu->misc_dev.fops = &dwu_ops;
	dwu->dev = &pdev->dev;

	mutex_init(&dwu->mutex);
	init_completion(&dwu->done_dwu);
	init_completion(&dwu->done_buf);
	complete(&dwu->done_buf);

	dwu->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!dwu->res) {
		dev_err(&pdev->dev, "failed to get dev resources: %d\n", ret);
		ret = -EINVAL;
		goto err_get_res;
	}

    printk("### dwu->res->start = 0x%08x  res->end = 0x%08x\n", dwu->res->start, dwu->res->end);
	dwu->res = request_mem_region(dwu->res->start,
			dwu->res->end - dwu->res->start + 1,
			pdev->name);
	if (!dwu->res) {
		dev_err(&pdev->dev, "failed to request regs memory region");
		ret = -EINVAL;
		goto err_get_res;
	}
	dwu->iomem = ioremap(dwu->res->start, resource_size(dwu->res));
	if (!dwu->iomem) {
		dev_err(&pdev->dev, "failed to remap regs memory region: %d\n",ret);
		ret = -EINVAL;
		goto err_ioremap;
	}

	dwu->irq = platform_get_irq(pdev, 0);
    printk("## dwu->irq = %d\n", dwu->irq);
	if (request_irq(dwu->irq, dwu_irq_handler, IRQF_SHARED, dwu->name, dwu)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto err_req_irq;
	}

#ifndef CONFIG_FPGA_TEST
	dwu->clk = clk_get(dwu->dev, "gate_ddr");
	if (IS_ERR(dwu->clk)) {
		dev_err(&pdev->dev, "dwu clk get failed!\n");
		goto err_get_clk;
	}
#endif

	dev_set_drvdata(&pdev->dev, dwu);

	dwu_reset_reg(dwu);

    ret = misc_register(&dwu->misc_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "register misc device failed!\n");
		goto err_set_drvdata;
	}

    printk("JZ DWU probe ok!!!!\n");
	return 0;

err_set_drvdata:
#ifndef CONFIG_FPGA_TEST
	clk_put(dwu->clk);
err_get_clk:
#endif
	free_irq(dwu->irq, dwu);
err_req_irq:
	iounmap(dwu->iomem);
err_ioremap:
err_get_res:
	kfree(dwu);

	return ret;
}

static int dwu_remove(struct platform_device *pdev)
{
	struct jz_dwu *dwu;
	struct resource *res;
	DWU_DEBUG("%s\n", __func__);

	dwu = dev_get_drvdata(&pdev->dev);
	res = dwu->res;
	free_irq(dwu->irq, dwu);
	iounmap(dwu->iomem);
	release_mem_region(res->start, res->end - res->start + 1);

	misc_deregister(&dwu->misc_dev);

    if (dwu) {
		kfree(dwu);
	}

	return 0;
}

static struct platform_driver jz_dwu_driver = {
	.probe	= dwu_probe,
	.remove = dwu_remove,
	.driver = {
		.name = "jz-dwu",
	},
};

static struct resource jz_dwu_resources[] = {
    [0] = {
	.start  = DDRC_IOBASE,
	.end    = DDRC_IOBASE + 0x8000-1,
	.flags  = IORESOURCE_MEM,
    },
    [1] = {
        .start  = IRQ_DDR_DWU,
        .end    = IRQ_DDR_DWU,
        .flags  = IORESOURCE_IRQ,
    },
};

struct platform_device jz_dwu_device = {
    .name   = "jz-dwu",
    .id = 0,
    .resource   = jz_dwu_resources,
    .num_resources  = ARRAY_SIZE(jz_dwu_resources),
};

static int __init dwudev_init(void)
{
    int ret = 0;

    DWU_DEBUG("%s\n", __func__);

    ret = platform_device_register(&jz_dwu_device);
    if(ret){
	    printk("Failed to insmod des device!!!\n");
	    return ret;
    }
	platform_driver_register(&jz_dwu_driver);
	return 0;
}

static void __exit dwudev_exit(void)
{
	DWU_DEBUG("%s\n", __func__);
	platform_device_unregister(&jz_dwu_device);
	platform_driver_unregister(&jz_dwu_driver);
}

module_init(dwudev_init);
module_exit(dwudev_exit);

MODULE_DESCRIPTION("JZ DWU driver");
MODULE_AUTHOR("jiansheng.zhang <jiansheng.zhang@ingenic.com>");
MODULE_LICENSE("GPL");
