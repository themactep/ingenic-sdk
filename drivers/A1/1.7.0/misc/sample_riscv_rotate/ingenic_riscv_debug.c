/*
 * Cryptographic API.
 *
 * Support for Tseries AES HW.
 *
 * Copyright (c) 2019 Ingenic Corporation
 * Author: Niky shen <xianghui.shen@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 */




#include "ingenic_riscv_debug.h"


static unsigned int opt_level = 0;
extern void do_gettimeofday(struct timeval *tv);

static int print_level = ROTATE_WARNING_LEVEL;
module_param(print_level, int, S_IRUGO);
MODULE_PARM_DESC(print_level, "rotate print level");


module_param(opt_level, int, S_IRUGO);
MODULE_PARM_DESC(opt_level, "riscv operator level");

//static unsigned int run_paddr = 0x12400000;
static unsigned int run_paddr = 0xEa00000;

module_param(run_paddr, int, S_IRUGO);
MODULE_PARM_DESC(run_paddr, "riscv run in address");

static unsigned int is_ddr = 0;
module_param(is_ddr, int, S_IRUGO);
MODULE_PARM_DESC(is_ddr, "riscv whether run in ddr");

static char *riscv_filename = "./riscv.bin";
module_param(riscv_filename, charp, S_IRUGO);
MODULE_PARM_DESC(riscv_filename, "riscv run filename");

static unsigned int  *run_vaddr;
static unsigned char *pic_paddr = 0xE400000;
static unsigned char *pic_vaddr = 0xE400000;
struct jz_rotate *rotatedev = NULL;






int rotate_printk(unsigned int level, unsigned char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r = 0;

	if(level >= print_level) {
		va_start(args, fmt);
		vaf.fmt = fmt;
		vaf.va = &args;

		r = printk("%pV", &vaf);
		va_end(args);
	}
	return r;
}

static int mailbox_open(struct inode *inode, struct file *file)
{
	/* struct miscdevice *dev = file->private_data; */

	printk("%s: aes open successful!\n", __func__);
	return 0;
}

static int mailbox_release(struct inode *inode, struct file *file)
{
	/* struct miscdevice *dev = file->private_data; */

	return 0;
}

static ssize_t mailbox_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t mailbox_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}


static int rotate_start_task(void)
{

	int ret = 0;
	//int i = 0,count = 60;
	//struct jz_rotate_info * info = pic_vaddr;

	memcpy(pic_vaddr,&rotatedev->rotate_data,sizeof(struct jz_rotate_info));
	//*(volatile unsigned int*)(0xb2a00008) = 0x5a5a0007;

#if 0
	ROTATE_ERROR("send info: \n \r weight:%d \n\r height:%d \n\r index:%d \n\r src:%d \n\r des:%d \n\r",\
	info->width,\
	info->height,\
	info->index,\
	info->src,\
	info->des);
#endif

	*(volatile unsigned int*)(0xb2a00008) = rotatedev->rotate_data.index;
	ret = wait_for_completion_timeout(&rotatedev->done,msecs_to_jiffies(800));
	//ROTATE_ERROR("send index:%d \n",rotatedev->rotate_data.index);


	if(ret < 0) {
		ROTATE_ERROR("[%s:%d] rotate complete wait be interrupt\n",__func__, __LINE__);
		ret = -EPERM;
	} else if(ret == 0) {
		ROTATE_ERROR("[%s:%d] rotate complete wait timeout\n",__func__, __LINE__);
		ret = -ETIMEDOUT;
	} else {
		//ROTATE_ERROR("[%s:%d] rotate complete wait end\n",__func__, __LINE__);
		ret = 0;
	}

	return ret;
}

static long mailbox_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	/* struct miscdevice *dev = file->private_data; */
	/* void __user *argp  = (void __user *)arg; */
	if (_IOC_TYPE(cmd) != JZ_ROTATE_MAGIC){
			printk("[%s:%d] invalid cmd\n",__func__, __LINE__);
			return -EFAULT;
	}
	switch (cmd) {
	case IOCTL_ROTATE_SET_SCALER_TABLE:
		if (copy_from_user(&rotatedev->rotate_data, (void *)arg, sizeof(struct jz_rotate_info))) {
			ROTATE_ERROR("[%s:%d] copy_from_user failed\n",__func__, __LINE__);
			return -EFAULT;
		}
		rotate_start_task();
		break;
	default:
		ROTATE_ERROR("[%s:%d] unsupport cmd = 0x%x\n",__func__, __LINE__,cmd);
		return -1;
	}

	return 0;
}




struct file_operations mailbox_fops = {
	.owner = THIS_MODULE,
	.read = mailbox_read,
	.write = mailbox_write,
	.open = mailbox_open,
	.unlocked_ioctl = mailbox_ioctl,
	.release = mailbox_release,
};






static irqreturn_t mailbox_ope_irq_handler(int irq, void *data)
{

	unsigned int mailbox = 0;

	mailbox = *(volatile unsigned int*)(0xb2a0000c);
	*(volatile unsigned int*)(0xb2a0000c) = 0; //clear
	complete(&rotatedev->done);
	if(mailbox == 0x123){
		printk("---rotate fail---\n");
	}

	if(opt_level == 0) {
		//*(volatile unsigned int*)(0xb2a00008) = 0x5a5a0000;
	} else if(opt_level == 7) {
		*(volatile unsigned int*)(0xb2a00008) = 0x5a5a0007;
	}else{
		*(volatile unsigned int*)(0xb2a00008) = 0x5a5a0000;
	}


	return IRQ_HANDLED;
}

#define XBURST2_CCU_BASE            (0xb2200000)
#define XBURST2_CCU_CFCR            (volatile unsigned int *)(XBURST2_CCU_BASE + 0x0fe0)
#define XBURST2_LEP_STOP            *XBURST2_CCU_CFCR = *XBURST2_CCU_CFCR |  (1 << 31)
#define XBURST2_LEP_RUN             *XBURST2_CCU_CFCR = *XBURST2_CCU_CFCR & ~(1 << 31)

#define CCU_BASE                    (0xb2a00000)
// bit0:ie/rw bit1:bus_mask/rw bit2:bus_idle/r bit3:sleep/r bit4:running/r bit5:timer_en/rw
#define CCU_CCSR                    (volatile unsigned int *)(CCU_BASE + 0x0 * 4)
// bit31~0:reset entry/rw
#define CCU_CRER                    (volatile unsigned int *)(CCU_BASE + 0x1 * 4)
// bit31~0:host to riscv mailbox, gen intrrrupt/rw
#define CCU_FROM_HOST               (volatile unsigned int *)(CCU_BASE + 0x2 * 4)
// bit31~0:riscv to host mailbox, gen intrrrupt/rw
#define CCU_TO_HOST                 (volatile unsigned int *)(CCU_BASE + 0x3 * 4)
#define CCU_TIME_L                  (volatile unsigned int *)(CCU_BASE + 0x4 * 4)
#define CCU_TIME_H                  (volatile unsigned int *)(CCU_BASE + 0x5 * 4)
#define CCU_TIME_CMP_L              (volatile unsigned int *)(CCU_BASE + 0x6 * 4)
#define CCU_TIME_CMP_H              (volatile unsigned int *)(CCU_BASE + 0x7 * 4)
#define CCU_INTC_MASK_L             (volatile unsigned int *)(CCU_BASE + 0x8 * 4)
#define CCU_INTC_MASK_H             (volatile unsigned int *)(CCU_BASE + 0x9 * 4)
#define CCU_INTC_PEND_L             (volatile unsigned int *)(CCU_BASE + 0xa * 4)
#define CCU_INTC_PEND_H             (volatile unsigned int *)(CCU_BASE + 0xb * 4)

#define CCU_PMA_ADR0                (volatile unsigned int *)(CCU_BASE + 0x10 * 4)
#define CCU_PMA_ADR1                (volatile unsigned int *)(CCU_BASE + 0x11 * 4)
#define CCU_PMA_ADR2                (volatile unsigned int *)(CCU_BASE + 0x12 * 4)
#define CCU_PMA_ADR3                (volatile unsigned int *)(CCU_BASE + 0x13 * 4)
#define CCU_PMA_CFG0                (volatile unsigned int *)(CCU_BASE + 0x18 * 4)
#define CCU_PMA_CFG1                (volatile unsigned int *)(CCU_BASE + 0x19 * 4)
#define CCU_PMA_CFG2                (volatile unsigned int *)(CCU_BASE + 0x1a * 4)
#define CCU_PMA_CFG3                (volatile unsigned int *)(CCU_BASE + 0x1b * 4)



static int riscv_init(unsigned int paddr)
{
	*CCU_CCSR = *CCU_CCSR & ~(1 << 1); // Bus_mask set 0
    XBURST2_LEP_STOP;

    // Reset value
    *CCU_CCSR        = 0x43;
    *CCU_CRER        = paddr;
    *CCU_FROM_HOST   = 0;
    *CCU_TO_HOST     = 0;
    *CCU_INTC_MASK_L = 0;
    *CCU_INTC_MASK_H = 0;
    *CCU_TIME_L      = 0;
    *CCU_TIME_H      = 0;
    *CCU_TIME_CMP_L  = -1;
    *CCU_TIME_CMP_H  = -1;
    *CCU_PMA_CFG0    = 0x1f01;
    *CCU_PMA_CFG1    = 0x2F03;
    *CCU_PMA_CFG2    = 0x1f01;
    *CCU_PMA_CFG3    = 0x1e01;
    *CCU_PMA_ADR0    = 0x04000000;
    *CCU_PMA_ADR1    = 0x05ffffff;
    *CCU_PMA_ADR2    = 0x1fffffff;
    *CCU_PMA_ADR3    = 0x00000000;


	return 0;
}



static int load_riscv_file(char *filename, unsigned int *vaddr, unsigned int paddr)
{
	int ret = 0;
	struct file *filep = NULL;
	//struct file *pic = NULL;
	mm_segment_t fs;
	struct kstat stat;
	int file_len;
	char *file_buf = NULL;

	fs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(filename, O_RDONLY, 0);
	if (filep < 0) {
		printk("[%s:%d] Failed to open file %s\n",__func__,__LINE__,filename);
		goto err_filp_open;
	}

	ret = vfs_stat(filename, &stat);
	if(ret) {
		printk("[%s:%d] Failed to stat file %s\n",__func__,__LINE__,filename);
		goto err_vfs_stat;
	}
	file_len = stat.size;
	printk("riscv bin file length = %d\n",file_len);

	file_buf = kmalloc(file_len, GFP_KERNEL);
	if(!file_buf) {
		printk("[%s:%d] kmalloc file buffer Failed\n",__func__,__LINE__);
		goto err_kmalloc;
	}

	ret = vfs_read(filep, file_buf, file_len, &filep->f_pos);
	if(ret != file_len) {
		printk("[%s:%d] vfs_read %s Failed!! read_len=%d,file_len=%d\n",__func__,__LINE__,filename, ret, file_len);
		goto err_vfs_read;
	}

	riscv_init(paddr);
	memcpy(run_vaddr, file_buf, file_len);


	printk("riscv running\n");
	msleep(200);
   	XBURST2_LEP_RUN;

err_vfs_read:
	kfree(file_buf);
err_kmalloc:
err_vfs_stat:
	filp_close(filep,NULL);
err_filp_open:
	set_fs(fs);

	return ret;
}

static int jz_mailbox_probe(struct platform_device *pdev)
{
	int ret = 0;

	rotatedev = (struct jz_rotate *)kmalloc(sizeof(struct jz_rotate), GFP_KERNEL);
	if (!rotatedev) {
		printk("[%s:%d] kmalloc struct jz_ipu failed\n",__func__, __LINE__);
		ret = -ENOMEM;
		goto err_kzalloc_jz_ipu;
	}
	memset(rotatedev, 0x0, sizeof(struct jz_rotate));

	ret = snprintf(rotatedev->name, sizeof(rotatedev->name), "%s", "rotate");
	if (ret < 0) {
		printk("[%s:%d] snprintf ipu name failed\n",__func__, __LINE__);
		ret = -EINVAL;
		goto err_snprintf_name;
	}

	rotatedev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	rotatedev->misc_dev.name = rotatedev->name;
	rotatedev->misc_dev.fops = &mailbox_fops;
	rotatedev->dev = &pdev->dev;




	if (request_irq(IRQ_SYS_LEP + 8, mailbox_ope_irq_handler, IRQF_SHARED, "riscv-debug", &mailbox_fops)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto failed_req_irq;
	}
	printk("%s, mailbox irq is : %d\n",__func__, IRQ_SYS_LEP);
	pic_vaddr = ioremap_nocache(pic_paddr, 1*1024*1024);
	if(!pic_vaddr) {
			printk("[%s:%d] ioremap_nocache Failed\n",__func__,__LINE__);
			goto failed_ioremap_nocache;
	}

	if(is_ddr) {	//ddr
		printk("DDR mode\n");
		run_vaddr = ioremap_nocache(run_paddr, 1*1024*1024);
		if(!run_vaddr) {
			printk("[%s:%d] ioremap_nocache Failed\n",__func__,__LINE__);
			goto failed_ioremap_nocache;
		}
		load_riscv_file(riscv_filename, run_vaddr, run_paddr+0x80);
	} else {	//sram

		printk("SRAM mode\n");
		run_vaddr = (unsigned int *)(run_paddr + 0xa0000000);
		load_riscv_file(riscv_filename, run_vaddr, run_paddr+0x80);
	}
	platform_set_drvdata(pdev, rotatedev);
	ret = misc_register(&rotatedev->misc_dev);
	if (ret < 0) {
		printk("[%s:%d] register misc device failed\n", __func__, __LINE__);
		//goto err_misc_register;
	}

	rotatedev->proc_root = jz_proc_mkdir("rotate");
	if(!rotatedev->proc_root){
		printk("Failed to create debug directory of ipu!\n");
		rotatedev->proc_root = NULL;
		//goto err_to_proc;
	}

	//初始化等待队列头
	init_completion(&rotatedev->done);




	return 0;

failed_ioremap_nocache:
failed_req_irq:
	free_irq(IRQ_SYS_LEP, &mailbox_fops);

err_snprintf_name:
	kfree(rotatedev);
err_kzalloc_jz_ipu:

	return ret;
}


static int jz_mailbox_remove(struct platform_device *pdev)
{

	if(is_ddr) {
		iounmap(run_vaddr);
	}
		iounmap(pic_vaddr);
	run_vaddr = NULL;
	free_irq(IRQ_SYS_LEP + 8, &mailbox_fops);
	kfree(rotatedev);

	misc_deregister(&rotatedev->misc_dev);
	reinit_completion(&rotatedev->done);

	return 0;
}

static void  platform_mailbox_release(struct device *dev)
{
	return ;
}

static struct platform_driver jz_mailbox_driver = {
	.probe	= jz_mailbox_probe,
	.remove	= jz_mailbox_remove,
	.driver	= {
		.name	= "jz-debug",
		.owner	= THIS_MODULE,
	},
};

struct platform_device jz_mailbox_device = {
	.name   = "jz-debug",
	.id = 0,
	.resource   = NULL,
	.dev = {
		.release = platform_mailbox_release,
	},
	.num_resources  = 0,
};


static int __init jz_mailbox_mod_init(void)
{
	int ret = 0;
	ret = platform_device_register(&jz_mailbox_device);
	if(ret){
		printk("Failed to insmod mailbox device!!!\n");
		return ret;
	}
	//init_waitqueue_head(&wqh);
	return  platform_driver_register(&jz_mailbox_driver);
}

static void __exit jz_mailbox_mod_exit(void)
{
	platform_driver_unregister(&jz_mailbox_driver);
	platform_device_unregister(&jz_mailbox_device);
}

module_init(jz_mailbox_mod_init);
module_exit(jz_mailbox_mod_exit);

MODULE_DESCRIPTION("Ingenic mailbox support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("keven.ywhan@ingenic.com");

