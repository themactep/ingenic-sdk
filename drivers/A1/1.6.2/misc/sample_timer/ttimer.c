/*
 * timer.c - Ingenic timer driver
 *
 * Copyright (C) 2015 Ingenic Semiconductor Co.,Ltd
 *       http://www.ingenic.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/mfd/core.h>
#include <linux/mempolicy.h>
#include <linux/interrupt.h>
#include <linux/mfd/ingenic-tcu.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
//#include <dt-bindings/interrupt-controller/t40-irq.h>

#include <soc/base.h>
#include <soc/extal.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <soc/gpio.h>

static unsigned int tcu_chn = 0;
module_param(tcu_chn, int, S_IRUGO);
MODULE_PARM_DESC(tcu_chn, "input tcu chn number");

#define TTIMER_STOP			0x1
#define TTIMER_START		0x2
#define TTIMER_SPEED		0x3

static unsigned int tcu_divs[6] = {1024, 256, 64, 16, 4, 1};
static unsigned int tcu_div_reg[6] = {
	TCU_PRESCALE_1024,
	TCU_PRESCALE_256,
	TCU_PRESCALE_64,
	TCU_PRESCALE_16,
	TCU_PRESCALE_4,
	TCU_PRESCALE_1,
};

struct ttimer_device {
	struct platform_device *pdev;
	const struct mfd_cell *cell;
	struct device	 *dev;
	struct miscdevice misc_dev;
	struct ingenic_tcu_chn *tcu;
	int tcu_speed;
	int run_step_irq;
	int flag;
};

static irqreturn_t jz_timer_interrupt(int irq, void *dev_id)
{
	printk("hello world!\n");
	return IRQ_HANDLED;
}

static int ttimer_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct ttimer_device *mdev = container_of(dev, struct ttimer_device, misc_dev);
	int ret = 0;
	if(mdev->flag){
		ret = -EBUSY;
		dev_err(mdev->dev, "ttimer driver busy now!\n");
	}else{
		mdev->flag = 1;
	}

	return ret;
}

static int ttimer_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct ttimer_device *mdev = container_of(dev, struct ttimer_device, misc_dev);
	mdev->flag = 0;
	ingenic_tcu_counter_stop(mdev->tcu);
	return 0;
}

static long ttimer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	struct ttimer_device *mdev = container_of(dev, struct ttimer_device, misc_dev);
	long ret = 0;

	if(mdev->flag == 0){
		printk("Please Open /dev/ttimer Firstly\n");
		return -EPERM;
	}

	switch (cmd) {
		case TTIMER_STOP:
    		printk("%s[%d]: stop\n",__func__,__LINE__);
			ingenic_tcu_counter_stop(mdev->tcu);
			break;
		case TTIMER_START:
		    printk("%s[%d]: start\n",__func__,__LINE__);
			ingenic_tcu_counter_begin(mdev->tcu);
			break;
		case TTIMER_SPEED:
			{
				unsigned int speed;
				int index = 0;
				unsigned int div = 0;
				if (copy_from_user(&speed, (void __user *)arg, sizeof(int))) {
					dev_err(mdev->dev, "[%s][%d] copy to user error\n", __func__, __LINE__);
					return -EFAULT;
				}

    			printk("%s[%d]: speed = %d\n",__func__,__LINE__,speed);
				while(index <= 5){
					div = 24000000 / speed / tcu_divs[index];
					if(div <= 0xffff)
						break;
					index++;
				}

				if(mdev->tcu->clk_div != tcu_div_reg[index]){
					mdev->tcu->clk_div = tcu_div_reg[index];
					ingenic_tcu_config(mdev->tcu);
				}
				//ingenic_tcu_set_period(mdev->tcu->cib.id,(24000000 / 64 / speed));
				ingenic_tcu_set_period(mdev->tcu->cib.id, div);
			}
			break;
		default:
			return -EINVAL;
	}

	return ret;
}

static struct file_operations ttimer_fops = {
	.open = ttimer_open,
	.release = ttimer_release,
	.unlocked_ioctl = ttimer_ioctl,
};

static int ttimer_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct ttimer_device *mdev;
	mdev = devm_kzalloc(&pdev->dev, sizeof(struct ttimer_device), GFP_KERNEL);
	if (!mdev) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "kzalloc ttimer device memery error\n");
		goto error_devm_kzalloc;
	}

	mdev->cell = mfd_get_cell(pdev);
	if (!mdev->cell) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "Failed to get mfd cell for jz_adc_aux!\n");
		goto error_devm_kzalloc;
	}

	mdev->dev = &pdev->dev;

	mdev->tcu = (struct ingenic_tcu_chn *)mdev->cell->platform_data;
	mdev->tcu->irq_type = FULL_IRQ_MODE;
	mdev->tcu->clk_src = TCU_CLKSRC_EXT;
	mdev->tcu_speed = 100;
//	mdev->tcu->is_pwm = 0;
//	mdev->tcu->cib.func = TRACKBALL_FUNC;
	mdev->tcu->clk_div = TCU_PRESCALE_64;
	ingenic_tcu_config(mdev->tcu);
	ingenic_tcu_set_period(mdev->tcu->cib.id,(24000000 / 64 / 100));
//	ingenic_tcu_counter_begin(mdev->tcu);


	ingenic_tcu_channel_to_virq(mdev->tcu);
	mdev->run_step_irq = mdev->tcu->virq[0];
	if (mdev->run_step_irq < 0) {
		ret = mdev->run_step_irq;
		dev_err(&pdev->dev, "Failed to get platform irq: %d\n", ret);
		goto error_get_irq;
	}

    printk("%s[%d]: mdev->run_step_irq = %d, mdev->tcu->cib.id = %d\n",__func__,__LINE__, mdev->run_step_irq, mdev->tcu->cib.id);
	ret = request_irq(mdev->run_step_irq, jz_timer_interrupt, 0,
				"jz_timer_interrupt", mdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to run request_irq() !\n");
		goto error_request_irq;
	}

	platform_set_drvdata(pdev, mdev);
	mdev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	mdev->misc_dev.name = "ttimer";
	mdev->misc_dev.fops = &ttimer_fops;
	ret = misc_register(&mdev->misc_dev);
	if (ret < 0) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "misc_register failed\n");
		goto error_misc_register;
	}

	/*ingenic_tcu_counter_begin(mdev->tcu);*/
	return 0;

error_misc_register:
	free_irq(mdev->run_step_irq, mdev);
error_request_irq:
error_get_irq:
	kfree(mdev);
error_devm_kzalloc:
	return ret;
}

static int ttimer_remove(struct platform_device *pdev)
{
	struct ttimer_device *mdev = platform_get_drvdata(pdev);

	ingenic_tcu_counter_stop(mdev->tcu);
	free_irq(mdev->run_step_irq, mdev);
	misc_deregister(&mdev->misc_dev);

	kfree(mdev);
	return 0;
}

static struct of_device_id ttimer_match[]={
	{.compatible = "ingenic,tcu_chn0",},
	{}
};

static struct platform_driver ttimer_driver = {
	.probe = ttimer_probe,
	.remove = ttimer_remove,
	.driver = {
		.name	= "tcu_chn",
		.of_match_table = ttimer_match,
		.owner	= THIS_MODULE,
	}
};

static int __init ttimer_init(void)
{
	char *pname = ttimer_match[0].compatible;
	sprintf(pname, "ingenic,tcu_chn%d", tcu_chn);
	return platform_driver_register(&ttimer_driver);
}

static void __exit ttimer_exit(void)
{
	platform_driver_unregister(&ttimer_driver);
}

module_init(ttimer_init);
module_exit(ttimer_exit);

MODULE_LICENSE("GPL");
