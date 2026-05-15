/*
 * Copyright (c) 2015 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * Input file for Ingenic bus_monitor driver
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
#include "bus_monitor.h"


static unsigned int global_chn = 2;
static unsigned int global_mode;
#define DEBUG
#ifdef	DEBUG
static int debug_monitor = 1;

#define MONITOR_DEBUG(format, ...) { if (debug_monitor) printk(format, ## __VA_ARGS__);}
#else
#define MONITOR_DEBUG(format, ...) do{ } while(0)
#endif

#define MONITOR_BUF_SIZE (1024 * 1024 * 2)

struct monitor_reg_struct jz_monitor_regs_name[] = {
	{"MONITOR_CTRL", MONITOR_CTRL},
	{"MONITOR_SLOT_CYC_NUM", MONITOR_SLOT_CYC_NUM},
	{"MONITOR_RD_WR_ID", MONITOR_RD_WR_ID},
	{"MONITOR_BUF_LIMIT_CYC", MONITOR_BUF_LIMIT_CYC},
	{"MONITOR_RD_CYC_CNT", MONITOR_RD_CYC_CNT},
	{"MONITOR_RD_CMD_CNT", MONITOR_RD_CMD_CNT},
	{"MONITOR_RD_ARVALID_CNT", MONITOR_RD_ARVALID_CNT},
	{"MONITOR_RD_RVALID_CNT", MONITOR_RD_RVALID_CNT},
	{"MONITOR_RD_DATA_CNT", MONITOR_RD_DATA_CNT},
	{"MONITOR_RD_LAST_CNT", MONITOR_RD_LAST_CNT},
	{"MONITOR_RD_DA_TO_LAST_CNT", MONITOR_RD_DA_TO_LAST_CNT},
	{"MONITOR_WR_CYC_CNT", MONITOR_WR_CYC_CNT},
	{"MONITOR_WR_CMD_CNT", MONITOR_WR_CMD_CNT},
	{"MONITOR_WR_AWVALID_CNT", MONITOR_WR_AWVALID_CNT},
	{"MONITOR_WR_WVALID_CNT", MONITOR_WR_WVALID_CNT},
	{"MONITOR_WR_DATA_CNT", MONITOR_WR_DATA_CNT},
	{"MONITOR_WR_LAST_CNT", MONITOR_WR_LAST_CNT},
	{"MONITOR_WR_DA_TO_LAST_CNT", MONITOR_WR_DA_TO_LAST_CNT},
	{"MONITOR_WATCH_ADDRL", MONITOR_WATCH_ADDRL},
	{"MONITOR_WATCH_ADDRH", MONITOR_WATCH_ADDRH},
	{"MONITOR_WATCH_MATCH_WRID", MONITOR_WATCH_MATCH_WRID},
	{"MONITOR_WATCH_MATCH_WADDR", MONITOR_WATCH_MATCH_WADDR},
	{"MONITOR_WATCH_MATCH_ARADDR", MONITOR_WATCH_MATCH_ARADDR},
	{"MONITOR_INT_STA", MONITOR_INT_STA},
	{"MONITOR_INT_MASK", MONITOR_INT_MASK},
	{"MONITOR_INT_CLR", MONITOR_INT_CLR},

};

int dump_chx_all_reg_value(struct jz_monitor *monitor)
{
	printk("/**********************************dump begin*********************************/\n");
	printk("----- dump all axi channel regs -----\n");
    printk("MONITOR_CTRL = 0x%08x\n", reg_read(monitor,              (MONITOR_CTRL + global_chn*0x100)));
    printk("MONITOR_SLOT_CYC_NUM = 0x%08x\n", reg_read(monitor,      (MONITOR_SLOT_CYC_NUM + global_chn*0x100)));

    printk("MONITOR_RD_WR_ID = 0x%08x\n", reg_read(monitor,          (MONITOR_RD_WR_ID + global_chn*0x100)));
    printk("MONITOR_BUF_LIMIT_CYC = 0x%08x\n", reg_read(monitor,     (MONITOR_BUF_LIMIT_CYC + global_chn*0x100)));
    printk("MONITOR_RD_RVALID_CNT = 0x%08x\n", reg_read(monitor,     (MONITOR_RD_RVALID_CNT + global_chn*0x100)));

    printk("MONITOR_RD_CYC_CNT = 0x%08x\n", reg_read(monitor,        (MONITOR_RD_CYC_CNT + global_chn*0x100)));
    printk("MONITOR_RD_CMD_CNT = 0x%08x\n", reg_read(monitor,        (MONITOR_RD_CMD_CNT + global_chn*0x100)));
    printk("MONITOR_RD_ARVALID_CNT = 0x%08x\n", reg_read(monitor,    (MONITOR_RD_ARVALID_CNT + global_chn*0x100)));
    printk("MONITOR_RD_RVALID_CNT = 0x%08x\n", reg_read(monitor,     (MONITOR_RD_RVALID_CNT + global_chn*0x100)));
    printk("MONITOR_RD_DATA_CNT = 0x%08x\n", reg_read(monitor,       (MONITOR_RD_DATA_CNT + global_chn*0x100)));
    printk("MONITOR_RD_LAST_CNT = 0x%08x\n", reg_read(monitor,       (MONITOR_RD_LAST_CNT + global_chn*0x100)));
    printk("MONITOR_RD_DA_TO_LAST_CNT = 0x%08x\n", reg_read(monitor, (MONITOR_RD_DA_TO_LAST_CNT + global_chn*0x100)));
    printk("MONITOR_WR_CYC_CNT = 0x%08x\n", reg_read(monitor,        (MONITOR_WR_CYC_CNT + global_chn*0x100)));
    printk("MONITOR_WR_CMD_CNT = 0x%08x\n", reg_read(monitor,        (MONITOR_WR_CMD_CNT + global_chn*0x100)));
    printk("MONITOR_WR_AWVALID_CNT = 0x%08x\n", reg_read(monitor,    (MONITOR_WR_AWVALID_CNT + global_chn*0x100)));
    printk("MONITOR_WR_WVALID_CNT = 0x%08x\n", reg_read(monitor,     (MONITOR_WR_WVALID_CNT + global_chn*0x100)));
    printk("MONITOR_WR_DATA_CNT = 0x%08x\n", reg_read(monitor,       (MONITOR_WR_DATA_CNT + global_chn*0x100)));
    printk("MONITOR_WR_LAST_CNT = 0x%08x\n", reg_read(monitor,       (MONITOR_WR_LAST_CNT + global_chn*0x100)));
    printk("MONITOR_WR_BRESP_CNT = 0x%08x\n", reg_read(monitor,      (MONITOR_WR_BRESP_CNT + global_chn*0x100)));
    printk("MONITOR_WR_DA_TO_LAST_CNT = 0x%08x\n", reg_read(monitor, (MONITOR_WR_DA_TO_LAST_CNT + global_chn*0x100)));

    printk("MONITOR_WATCH_ADDRL = 0x%08x\n", reg_read(monitor,       (MONITOR_WATCH_ADDRL + global_chn*0x100)));
    printk("MONITOR_WATCH_ADDRH = 0x%08x\n", reg_read(monitor,       (MONITOR_WATCH_ADDRH + global_chn*0x100)));
    printk("MONITOR_WATCH_MATCH_WRID = 0x%08x\n", reg_read(monitor,  (MONITOR_WATCH_MATCH_WRID + global_chn*0x100)));
    printk("MONITOR_WATCH_MATCH_WRID = 0x%08x\n", reg_read(monitor,  (MONITOR_WATCH_MATCH_WADDR + global_chn*0x100)));
    printk("MONITOR_WATCH_MATCH_ARADDR = 0x%08x\n", reg_read(monitor,(MONITOR_WATCH_MATCH_ARADDR + global_chn*0x100)));
    printk("MONITOR_INT_MASK = 0x%08x\n", reg_read(monitor,          (MONITOR_INT_MASK + global_chn*0x100)));
    printk("MONITOR_INT_STA = 0x%08x\n", reg_read(monitor,           (MONITOR_INT_STA + global_chn*0x100)));
    printk("MONITOR_INT_CLR = 0x%08x\n", reg_read(monitor,           (MONITOR_INT_CLR + global_chn*0x100)));
	printk("----- dump all axi channel regs  over -----\n");
	printk("/***********************************dump end**********************************/\n");

    return 0;
}

int dump_chx_reg_value(struct jz_monitor *monitor)
{
	printk("/**********************************dump begin*********************************/\n");
	printk("----- dump axi monitor regs -----\n");
    printk("MONITOR_RD_CYC_CNT = 0x%08x\n", reg_read(monitor,        (MONITOR_RD_CYC_CNT + global_chn*0x100)));
    printk("MONITOR_RD_CMD_CNT = 0x%08x\n", reg_read(monitor,        (MONITOR_RD_CMD_CNT + global_chn*0x100)));
    printk("MONITOR_RD_ARVALID_CNT = 0x%08x\n", reg_read(monitor,    (MONITOR_RD_ARVALID_CNT + global_chn*0x100)));
    printk("MONITOR_RD_RVALID_CNT = 0x%08x\n", reg_read(monitor,     (MONITOR_RD_RVALID_CNT + global_chn*0x100)));
    printk("MONITOR_RD_DATA_CNT = 0x%08x\n", reg_read(monitor,       (MONITOR_RD_DATA_CNT + global_chn*0x100)));
    printk("MONITOR_RD_LAST_CNT = 0x%08x\n", reg_read(monitor,       (MONITOR_RD_LAST_CNT + global_chn*0x100)));
    printk("MONITOR_RD_DA_TO_LAST_CNT = 0x%08x\n", reg_read(monitor, (MONITOR_RD_DA_TO_LAST_CNT + global_chn*0x100)));
    printk("MONITOR_WR_CYC_CNT = 0x%08x\n", reg_read(monitor,        (MONITOR_WR_CYC_CNT + global_chn*0x100)));
    printk("MONITOR_WR_CMD_CNT = 0x%08x\n", reg_read(monitor,        (MONITOR_WR_CMD_CNT + global_chn*0x100)));
    printk("MONITOR_WR_AWVALID_CNT = 0x%08x\n", reg_read(monitor,    (MONITOR_WR_AWVALID_CNT + global_chn*0x100)));
    printk("MONITOR_WR_WVALID_CNT = 0x%08x\n", reg_read(monitor,     (MONITOR_WR_WVALID_CNT + global_chn*0x100)));
    printk("MONITOR_WR_DATA_CNT = 0x%08x\n", reg_read(monitor,       (MONITOR_WR_DATA_CNT + global_chn*0x100)));
    printk("MONITOR_WR_LAST_CNT = 0x%08x\n", reg_read(monitor,       (MONITOR_WR_LAST_CNT + global_chn*0x100)));
    printk("MONITOR_WR_BRESP_CNT = 0x%08x\n", reg_read(monitor,      (MONITOR_WR_BRESP_CNT + global_chn*0x100)));
    printk("MONITOR_WR_DA_TO_LAST_CNT = 0x%08x\n", reg_read(monitor, (MONITOR_WR_DA_TO_LAST_CNT + global_chn*0x100)));

    printk("MONITOR_SLOT_CYC_NUM = 0x%08x\n", reg_read(monitor, (MONITOR_SLOT_CYC_NUM + global_chn*0x100)));
    printk("MONITOR_CTRL = 0x%08x\n", reg_read(monitor, (MONITOR_CTRL + global_chn*0x100)));
	printk("----- dump axi monitor regs over  -----\n");
	printk("/********************************dump end***********************************/\n");

    return 0;
}

static void reg_bit_set(struct jz_monitor *monitor, int offset, unsigned int bit)
{
	unsigned int reg = 0;
	reg = reg_read(monitor, offset);
	reg |= bit;
	reg_write(monitor, offset, reg);
}

static void reg_bit_clr(struct jz_monitor *monitor, int offset, unsigned int bit)
{
	unsigned int reg = 0;
	reg= reg_read(monitor, offset);
	reg &= ~(bit);
	reg_write(monitor, offset, reg);
}


static int _monitor_dump_regs(struct jz_monitor *monitor)
{
	int i = 0;
	int num = 0;

	if (monitor == NULL) {
		dev_err(monitor->dev, "monitor is NULL!\n");
		return -1;
	}
	printk("----- dump regs -----\n");

	num = sizeof(jz_monitor_regs_name) / sizeof(struct monitor_reg_struct);
	for (i = 0; i < num; i++) {
		printk("monitor_reg: %s: \t0x%08x\r\n", jz_monitor_regs_name[i].name, reg_read(monitor, (jz_monitor_regs_name[i].addr + global_chn*0x100)));
	}

	return 0;
}

static int monitor_reg_set(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
    unsigned int chn = 0;
    unsigned int addrl = 0;
    unsigned int addrh = 0;
    unsigned int monitor_mode = 0;
    unsigned int func_id = 0;

    struct monitor_param *ip = monitor_param;
    if (monitor == NULL) {
		dev_err(monitor->dev, "monitor: monitor is NULL or monitor_param is NULL\n");
		return -1;
	}

    global_chn = ip->monitor_chn;
    global_mode = ip->monitor_mode;
    chn = ip->monitor_chn;
    addrl = ip->monitor_start;
    addrh = ip->monitor_end;
    monitor_mode = ip->monitor_mode;
    func_id = ip->monitor_id;

    printk("func_id = %d\n", func_id);
    printk("chn = %d monitor_mode = %d\n", chn, monitor_mode);
    printk("addrl = 0x%08x addrh = 0x%08x\n", addrl, addrh);

    if (chn != 5) { /* AXI */
        reg_write(monitor, (MONITOR_CTRL + chn*0x100), 0x0000ff00);
        if(monitor_mode == 0) {      /* watch addr */
            reg_bit_set(monitor, (MONITOR_CTRL + chn*0x100), MODE_EN_WATCH);
            reg_bit_clr(monitor, (MONITOR_CTRL + chn*0x100), ID_MODE);
            reg_write(monitor, (MONITOR_WATCH_ADDRL + chn*0x100), addrl);
            reg_write(monitor, (MONITOR_WATCH_ADDRH + chn*0x100), addrh);
            printk("axi watch addr MONITOR_CTRL = 0x%08x\n", reg_read(monitor, MONITOR_CTRL+chn*0x100));
        } else if(monitor_mode == 1) {  /* watch id */
            reg_bit_set(monitor, (MONITOR_CTRL + chn*0x100), MODE_EN_WATCH);
            reg_bit_set(monitor, (MONITOR_CTRL + chn*0x100), ID_MODE);
            reg_write(monitor, (MONITOR_CTRL + chn*0x100), (0xfffff0ff & reg_read(monitor, (MONITOR_CTRL + chn*0x100))));//mask id
            reg_write(monitor, (MONITOR_WATCH_ADDRL + chn*0x100), addrl);
            reg_write(monitor, (MONITOR_WATCH_ADDRH + chn*0x100), addrh);
            reg_write(monitor, (MONITOR_RD_WR_ID + chn*0x100), (WRITE_ID_EN | (func_id << 16) | READ_ID_EN | (func_id << 0)));
            printk("axi watch id MONITOR_CTRL = 0x%08x\n", reg_read(monitor, MONITOR_CTRL+chn*0x100));
        } else if(monitor_mode == 2) {     /* monitor */
            reg_bit_set(monitor, (MONITOR_CTRL + chn*0x100), MODE_EN_MON);
            reg_write(monitor, (MONITOR_CTRL + chn*0x100), (0x80000000 | reg_read(monitor, (MONITOR_CTRL+chn*0x100))));//mask id
            reg_write(monitor, (MONITOR_SLOT_CYC_NUM + chn*0x100), 0xffffffff);
            reg_write(monitor, (MONITOR_BUF_LIMIT_CYC + chn*0x100), 0xffffffff);
            printk("axi monitor MONITOR_CTRL = 0x%08x\n", reg_read(monitor, MONITOR_CTRL+chn*0x100));
        } else {     /* monitor id*/
            reg_bit_set(monitor, (MONITOR_CTRL + chn*0x100), MODE_EN_MON);
            reg_write(monitor, (MONITOR_CTRL + chn*0x100), (0xfffff0ff & reg_read(monitor, (MONITOR_CTRL + chn*0x100))));//mask id
            reg_write(monitor, (MONITOR_SLOT_CYC_NUM + chn*0x100), 0xffffffff);
            reg_write(monitor, (MONITOR_BUF_LIMIT_CYC + chn*0x100), 0xffffffff);
            reg_write(monitor, (MONITOR_RD_WR_ID + chn*0x100), (WRITE_ID_EN | (func_id << 16) | READ_ID_EN | (func_id << 0)));
            printk("axi monitor id MONITOR_CTRL = 0x%08x\n", reg_read(monitor, MONITOR_CTRL+chn*0x100));
        }
    } else {   /* AHB channel 5 */
        reg_write(monitor, AHB_MON_CTRL, 0x00000000);
        if(monitor_mode == 0) {      /* watch addr */
            reg_bit_set(monitor, AHB_MON_CTRL, AHB_WATCH_EN);
            reg_bit_set(monitor, AHB_MON_CTRL, AHB_ID_MODE);
            reg_write(monitor, AHB_MON_WATCH_ADDRL, addrl);
            reg_write(monitor, AHB_MON_WATCH_ADDRH, addrh);
            printk("ahb MONITOR_CTRL = 0x%08x\n", reg_read(monitor, MONITOR_CTRL+chn*0x100));
        } else if(monitor_mode == 1) {  /* watch id */
            reg_bit_set(monitor, AHB_MON_CTRL, AHB_WATCH_EN);
            reg_bit_clr(monitor, AHB_MON_CTRL, AHB_ID_MODE);
            reg_write(monitor, AHB_MON_CTRL, ((func_id << 12) | reg_read(monitor, AHB_MON_CTRL)));
            printk("ahb MONITOR_CTRL = 0x%08x\n", reg_read(monitor, MONITOR_CTRL+chn*0x100));
        }
    }

    return 0;
}

static int monitor_start(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
	int ret = 0;
    unsigned int reg_addr = 0;
    unsigned int dbox_b = 0;
	struct monitor_param *ip = monitor_param;

	if ((monitor == NULL) || (monitor_param == NULL)) {
		dev_err(monitor->dev, "monitor: monitor is NULL or monitor_param is NULL\n");
		return -1;
	}
	MONITOR_DEBUG("monitor: enter monitor_start %d\n", current->pid);

#ifndef CONFIG_FPGA_TEST
	clk_enable(monitor->clk);
#ifdef CONFIG_SOC_T40
	clk_enable(monitor->ahb0_gate);
#endif
#endif

    ret = monitor_reg_set(monitor, ip);
    reg_addr =  global_chn*0x100;
    printk("ioctl_start global_chn = %d reg_addr = 0x%08x\n", global_chn, reg_addr);

    if (global_chn != 5) {
        reg_write(monitor, (MONITOR_CTRL + reg_addr), 0x0000ff00);
        __reset_monitor(reg_addr);
    } else {
        reg_write(monitor, AHB_MON_CTRL, 0x00000000);
        __reset_monitor_ahb();
    }
    //usleep(50);
    if (global_chn != 5) {
        reg_bit_clr(monitor, reg_addr, SOFT_RESET);
    } else {
        reg_bit_clr(monitor, AHB_MON_CTRL, AHB_SOFT_RESET);
    }

    ret = monitor_reg_set(monitor, ip);

    if (global_chn != 5) {
        reg_write(monitor, (MONITOR_INT_MASK + global_chn*0x100), 0x00000000); //mask irq
    } else {
        reg_write(monitor, AHB_MON_INT_MASK, 0x00000000); //mask irq
    }

    /* start monitor */
    if (global_chn != 5) {
        reg_bit_set(monitor, reg_addr, FUNC_EN);
        __start_monitor(reg_addr);
    } else {
        __start_monitor_ahb();
    }
    printk("ioctl_start global_chn = %d  global_mode = %d reg_addr = 0x%08x\n", global_chn, global_mode, reg_addr);
    printk("ioctl_start MONITOR_CTRL = 0x%08x\n", reg_read(monitor, MONITOR_CTRL+global_chn*0x100));

	MONITOR_DEBUG("monitor_start\n");

    if ((2 == global_mode) || (3 == global_mode)) {
        //msleep(1);
        dbox_b = *((volatile unsigned int *)(0xb30d0000));
        printk("ioctl_start dbox_b = 0x%08x\n", dbox_b);
        while(((*(volatile unsigned int *)0xb30d0000) & 0x00000001) == 0) {
            msleep(5);
        }
    }
    if ((2 == global_mode) || (3 == global_mode)) {
       // msleep(50);
        reg_bit_set(monitor, (MONITOR_CTRL + global_chn*0x100), CNT_LOCK);
        reg_bit_set(monitor, (MONITOR_CTRL + global_chn*0x100), CNT_CAPTURE);

        //msleep(500);

        dump_chx_reg_value(monitor);

        reg_bit_set(monitor, (MONITOR_CTRL + global_chn*0x100), CNT_CLEAR);
        printk("MONITOR_INT_STA = 0x%08x\n", reg_read(monitor, (MONITOR_INT_STA + global_chn*0x100)));
        dump_chx_reg_value(monitor);
    }

	ret = wait_for_completion_interruptible_timeout(&monitor->done_monitor, msecs_to_jiffies(10000));
	if (ret < 0) {
		printk("monitor: done_monitor wait_for_completion_interruptible_timeout err %d\n", ret);
		goto err_monitor_wait_for_done;
	} else if (ret == 0 ) {
		ret = -1;
		printk("monitor: done_monitor wait_for_completion_interruptible_timeout timeout %d\n", ret);
		dump_chx_all_reg_value(monitor);
		goto err_monitor_wait_for_done;
	} else {
		;
	}

    MONITOR_DEBUG("monitor: exit monitor_start %d\n", current->pid);

#ifndef CONFIG_FPGA_TEST
#ifdef CONFIG_SOC_T40
	clk_disable(monitor->ahb0_gate);
#endif
#endif
    return 0;

err_monitor_wait_for_done:
#ifndef CONFIG_FPGA_TEST
#ifdef CONFIG_SOC_T40
	clk_disable(monitor->ahb0_gate);
#endif
#endif
	return ret;

}

static long monitor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct monitor_param iparam;
	struct miscdevice *dev = filp->private_data;
	struct jz_monitor *monitor = container_of(dev, struct jz_monitor, misc_dev);

	MONITOR_DEBUG("monitor: %s pid: %d, tgid: %d file: %p, cmd: 0x%08x\n",
			__func__, current->pid, current->tgid, filp, cmd);

	if (_IOC_TYPE(cmd) != JZMONITOR_IOC_MAGIC) {
		dev_err(monitor->dev, "invalid cmd!\n");
		return -EFAULT;
	}

	mutex_lock(&monitor->mutex);

	switch (cmd) {
		case IOCTL_MONITOR_START:
			if (copy_from_user(&iparam, (void *)arg, sizeof(struct monitor_param))) {
				dev_err(monitor->dev, "copy_from_user error!!!\n");
				ret = -EFAULT;
				break;
			}
			ret = monitor_start(monitor, &iparam);
			if (ret) {
				printk("monitor: error monitor start ret = %d\n", ret);
			}
			break;
		case IOCTL_MONITOR_BUF_LOCK:
			ret = wait_for_completion_interruptible_timeout(&monitor->done_buf, msecs_to_jiffies(2000));
			if (ret < 0) {
				printk("monitor: done_buf wait_for_completion_interruptible_timeout err %d\n", ret);
			} else if (ret == 0 ) {
				printk("monitor: done_buf wait_for_completion_interruptible_timeout timeout %d\n", ret);
				ret = -1;
				dump_chx_all_reg_value(monitor);
			} else {
				ret = 0;
			}
			break;
		case IOCTL_MONITOR_BUF_UNLOCK:
			complete(&monitor->done_buf);
			break;
	    default:
			dev_err(monitor->dev, "invalid command: 0x%08x\n", cmd);
			ret = -EINVAL;
	}

	mutex_unlock(&monitor->mutex);
	return ret;
}

static int monitor_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct miscdevice *dev = filp->private_data;
	struct jz_monitor *monitor = container_of(dev, struct jz_monitor, misc_dev);

	MONITOR_DEBUG("monitor: %s pid: %d, tgid: %d filp: %p\n",
			__func__, current->pid, current->tgid, filp);
	mutex_lock(&monitor->mutex);

	mutex_unlock(&monitor->mutex);
	return ret;
}

static int monitor_release(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct miscdevice *dev = filp->private_data;
	struct jz_monitor *monitor = container_of(dev, struct jz_monitor, misc_dev);

	MONITOR_DEBUG("monitor: %s  pid: %d, tgid: %d filp: %p\n",
			__func__, current->pid, current->tgid, filp);
	mutex_lock(&monitor->mutex);

	mutex_unlock(&monitor->mutex);
	return ret;
}

static struct file_operations monitor_ops = {
	.owner = THIS_MODULE,
	.open = monitor_open,
	.release = monitor_release,
	.unlocked_ioctl = monitor_ioctl,
};


static irqreturn_t monitor_irq_handler(int irq, void *data)
{
	struct jz_monitor *monitor;
	unsigned int status;

	MONITOR_DEBUG("monitor: %s\n", __func__);
	monitor = (struct jz_monitor *)data;

    if (5 != global_chn) {
        status = reg_read(monitor, (MONITOR_INT_STA + global_chn*0x100));
    } else {
        status = reg_read(monitor, AHB_MON_INT_STA);
    }

    if (5 != global_chn) {
        reg_write(monitor, (MONITOR_INT_CLR + global_chn*0x100), status);
    } else {
        reg_write(monitor, AHB_MON_INT_CLR, status);
    }

    MONITOR_DEBUG("----- %s, status= 0x%08x\n", __func__, status);
	 /* this status doesn't do anything including trigger interrupt,
	 * just give a hint */
    if (status & 0x7f){
        complete(&monitor->done_monitor);
    }
    if (global_chn != 5) {
        reg_bit_set(monitor, (MONITOR_CTRL + global_chn*0x100), SOFT_RESET);
    } else {
        reg_bit_set(monitor, AHB_MON_CTRL, AHB_SOFT_RESET);
    }


    return IRQ_HANDLED;
}

static int monitor_probe(struct platform_device *pdev)
{
	int ret = 0;
    unsigned int reg_addr = 0;
	struct jz_monitor *monitor;

	MONITOR_DEBUG("%s\n", __func__);
	monitor = (struct jz_monitor *)kzalloc(sizeof(struct jz_monitor), GFP_KERNEL);
	if (!monitor) {
		dev_err(&pdev->dev, "alloc jz_monitor failed!\n");
		return -ENOMEM;
	}

	sprintf(monitor->name, "monitor");

	monitor->misc_dev.minor = MISC_DYNAMIC_MINOR;
	monitor->misc_dev.name = monitor->name;
	monitor->misc_dev.fops = &monitor_ops;
	monitor->dev = &pdev->dev;

	mutex_init(&monitor->mutex);
	init_completion(&monitor->done_monitor);
	init_completion(&monitor->done_buf);
	complete(&monitor->done_buf);

	monitor->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!monitor->res) {
		dev_err(&pdev->dev, "failed to get dev resources: %d\n", ret);
		ret = -EINVAL;
		goto err_get_res;
	}

    printk("### monitor->res->start = 0x%08x  res->end = 0x%08x\n", monitor->res->start, monitor->res->end);
	monitor->res = request_mem_region(monitor->res->start,
			monitor->res->end - monitor->res->start + 1,
			pdev->name);
	if (!monitor->res) {
		dev_err(&pdev->dev, "failed to request regs memory region");
		ret = -EINVAL;
		goto err_get_res;
	}
	monitor->iomem = ioremap(monitor->res->start, resource_size(monitor->res));
	if (!monitor->iomem) {
		dev_err(&pdev->dev, "failed to remap regs memory region: %d\n",ret);
		ret = -EINVAL;
		goto err_ioremap;
	}

	monitor->irq = platform_get_irq(pdev, 0);
    printk("## monitor->irq = %d\n", monitor->irq);
	if (request_irq(monitor->irq, monitor_irq_handler, IRQF_SHARED, monitor->name, monitor)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto err_req_irq;
	}

#ifndef CONFIG_FPGA_TEST
	monitor->clk = clk_get(monitor->dev, "gate_bus_monitor");
	if (IS_ERR(monitor->clk)) {
		dev_err(&pdev->dev, "monitor clk get failed!\n");
		goto err_get_clk;
	}

#ifdef CONFIG_SOC_T40
	monitor->ahb0_gate = clk_get(monitor->dev, "gate_ahb0");
	if (IS_ERR(monitor->clk)) {
		dev_err(&pdev->dev, "monitor clk get failed!\n");
		goto err_get_clk;
	}
#endif
#endif

	dev_set_drvdata(&pdev->dev, monitor);

    reg_addr = global_chn*0x100;

    if (global_chn != 5) {
        __reset_monitor(reg_addr);
    } else {
        __reset_monitor_ahb();
    }
    //usleep(50);
    if (global_chn != 5) {
        reg_bit_clr(monitor, reg_addr, SOFT_RESET);
    } else {
        reg_bit_clr(monitor, AHB_MON_CTRL, AHB_SOFT_RESET);
    }
	//__reset_monitor(reg_addr);

    ret = misc_register(&monitor->misc_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "register misc device failed!\n");
		goto err_set_drvdata;
	}

    printk("JZ MONITOR probe ok!!!!\n");
	return 0;

err_set_drvdata:
#ifndef CONFIG_FPGA_TEST
	clk_put(monitor->clk);
err_get_clk:
#endif
	free_irq(monitor->irq, monitor);
err_req_irq:
	iounmap(monitor->iomem);
err_ioremap:
err_get_res:
	kfree(monitor);

	return ret;
}

static int monitor_remove(struct platform_device *pdev)
{
	struct jz_monitor *monitor;
	struct resource *res;
	MONITOR_DEBUG("%s\n", __func__);

	monitor = dev_get_drvdata(&pdev->dev);
	res = monitor->res;
	free_irq(monitor->irq, monitor);
	iounmap(monitor->iomem);
	release_mem_region(res->start, res->end - res->start + 1);

	misc_deregister(&monitor->misc_dev);

    if (monitor) {
		kfree(monitor);
	}

	return 0;
}

static struct platform_driver jz_monitor_driver = {
	.probe	= monitor_probe,
	.remove = monitor_remove,
	.driver = {
		.name = "jz-monitor",
	},
};

static struct resource jz_monitor_resources[] = {
    [0] = {
	.start  = MONITOR_IOBASE,
	.end    = MONITOR_IOBASE + 0x8000-1,
	.flags  = IORESOURCE_MEM,
    },
    [1] = {
        .start  = IRQ_MON,
        .end    = IRQ_MON,
        .flags  = IORESOURCE_IRQ,
    },
};

struct platform_device jz_monitor_device = {
    .name   = "jz-monitor",
    .id = 0,
    .resource   = jz_monitor_resources,
    .num_resources  = ARRAY_SIZE(jz_monitor_resources),
};

static int __init monitordev_init(void)
{
    int ret = 0;

    MONITOR_DEBUG("%s\n", __func__);

    ret = platform_device_register(&jz_monitor_device);
    if(ret){
	    printk("Failed to insmod des device!!!\n");
	    return ret;
    }
	platform_driver_register(&jz_monitor_driver);
	return 0;
}

static void __exit monitordev_exit(void)
{
	MONITOR_DEBUG("%s\n", __func__);
	platform_device_unregister(&jz_monitor_device);
	platform_driver_unregister(&jz_monitor_driver);
}

module_init(monitordev_init);
module_exit(monitordev_exit);

MODULE_DESCRIPTION("JZ BUS_MONITOR driver");
MODULE_AUTHOR("jiansheng.zhang <jiansheng.zhang@ingenic.cn>");
MODULE_LICENSE("GPL");
