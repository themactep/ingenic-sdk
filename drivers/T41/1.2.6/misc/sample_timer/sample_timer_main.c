/*
 * sample_timer.c - ingenic_tcu mfd cell driver.
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/*
*操作位置：/sys/module/sample_timer/drivers/platform:tcu_chn2/ingenic,tcu_chn3.3/
* echo 0 > enable
* echo 174  > period_ms
* echo 1 > enable
* */

#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/mfd/core.h>
#include <linux/mempolicy.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <soc/gpio.h>
#if (defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_6_1))
#if defined(CONFIG_SOC_T41)
#include <linux/mfd/ingenic-tcu.h>
#include <dt-bindings/interrupt-controller/t41-irq.h>
#elif defined(CONFIG_SOC_T40)
#include <linux/mfd/ingenic-tcu.h>
#include <dt-bindings/interrupt-controller/t40-irq.h>
#endif
#else
#include <linux/mfd/jz_tcu.h>
#endif

#define TIMER_SRC TCU_CLKSRC_EXT   //时钟源大小
#define TIMER_SRC_FREQ (CONFIG_EXTAL_CLOCK * 1000000) //时钟大小
#define TIMER_DIV (1 << (2 * TCU_PRESCALE_64)) //分频比
#define TIMER_FREQ (TIMER_SRC_FREQ / TIMER_DIV) //频率
#define TIMER_MAX_PERIOD_MS (0xffff * 1000 / TIMER_FREQ) //64分频下，period_ms max is 174ms

#define MISC_DEV_NAME "sample_timer"

#define TCU_TDFR0 0x40
#define TCU_REG_BASE 0x10002000
#define TCU_TMSTR 0x34
#define TCU_TMCLR 0x38
static void __iomem *tcu_base;

struct timer_dev
{
	const struct mfd_cell *cell;
	struct device *dev;
	struct miscdevice misc_dev;
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	struct ingenic_tcu_chn *tcu;
#else
	struct jz_tcu_chn *tcu;
#endif
	int irqn;
};
struct timer_dev *tdev;
int tcu_channel = 0;


#if 1
void enable_timer_irq(unsigned int num)
{
	writel(1 << num, tcu_base + TCU_TMCLR);
}
void disable_timer_irq(unsigned int num)
{
	writel(1 << num, tcu_base + TCU_TMSTR);
}
void set_irq_time(unsigned int num, unsigned int us)
{
	unsigned int tdfr;
	tdfr = us * 24 / 64 - 1;
	if (tdfr < 2)
		tdfr = 2;
	--tdfr;
	writel(tdfr < 0xffff ? tdfr : 0xffff, tcu_base + (TCU_TDFR0 + 0x10 * num));
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	tcu_set_counter(num, 0);
#else
	jz_tcu_set_count(tdev->tcu, 0);
#endif
}
#endif

static irqreturn_t jz_tcu_interrupt(int irq, void *dev_id)
{
	struct timer_dev *tdev = dev_id;

	printk("jz_tcu_interrupt\n");
	/* echo 8 > /proc/sys/kernel/printk to see this print. */
	dev_dbg(tdev->dev, "t=%llu\n", ktime_get());

	return IRQ_HANDLED;
}

static ssize_t sample_timer_period_ms_store(struct device *dev,
				   struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct timer_dev *tdev = dev_get_drvdata(dev);
	int period_ms;

	if (buf == NULL)
		return 0;

	sscanf(buf, "%d", &period_ms);
	if (period_ms > TIMER_MAX_PERIOD_MS) {
		dev_err(tdev->dev, "Timer max period is %d\n", TIMER_MAX_PERIOD_MS);
		return -1;
	}

#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	ingenic_tcu_set_period(tdev->tcu->cib.id, (u16)(period_ms * TIMER_FREQ / 1000));
#else
	jz_tcu_set_period(tdev->tcu, (u16)(period_ms * TIMER_FREQ / 1000));
#endif

	return count;
}

static ssize_t sample_timer_enable_store(struct device *dev,
				   struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct timer_dev *tdev = dev_get_drvdata(dev);

	if (buf == NULL)
		return 0;

	if (strncmp(buf, "1", 1) == 0) {
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	    tcu_enable_counter(tdev->tcu->cib.id);
	    enable_timer_irq(tdev->tcu->cib.id);
		tcu_set_counter(tdev->tcu->cib.id, 0); /* Clear Count */
	    tcu_start_counter(tdev->tcu->cib.id);
		dev_info(tdev->dev, "timer enable\n");
#else
        jz_tcu_enable_counter(tdev->tcu); /* Enable TCU Channel */
	    enable_timer_irq(tdev->tcu->index);
		jz_tcu_set_counter(tdev->tcu, 0); /* Clear Count */
		jz_tcu_start_counter(tdev->tcu); /* start Counter */
		dev_info(tdev->dev, "timer enable\n");
#endif
	} else if (strncmp(buf, "0", 1) == 0) {
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	    tcu_stop_counter(tdev->tcu->cib.id);
	    disable_timer_irq(tdev->tcu->cib.id);
		tcu_disable_counter(tdev->tcu->cib.id); /* Disable TCU Channel */
		dev_info(tdev->dev, "timer disable\n");
#else
		jz_tcu_stop_counter(tdev->tcu); /* Stop Counter */
	    disable_timer_irq(tdev->tcu->index);
		jz_tcu_disable_counter(tdev->tcu); /* Disable TCU Channel */
		dev_info(tdev->dev, "timer disable\n");
#endif
	} else {
		dev_err(tdev->dev, "Invalid option\n");
		return -1;
	}

	return count;
}

static ssize_t sample_timer_count_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct timer_dev *tdev = dev_get_drvdata(dev);
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	u16 cnt_now = tcu_get_counter(tdev->tcu->cib.id);
#else
	u16 cnt_now = jz_tcu_get_count(tdev->tcu);
#endif

	return sprintf(buf, "%u\n", cnt_now);
}

static DEVICE_ATTR(period_ms, S_IWUSR, NULL, sample_timer_period_ms_store);
static DEVICE_ATTR(enable, S_IWUSR, NULL, sample_timer_enable_store);
static DEVICE_ATTR(count, S_IRUSR, sample_timer_count_show, NULL);

static struct attribute *sample_timer_attributes[] = {
	&dev_attr_period_ms.attr,
	&dev_attr_enable.attr,
	&dev_attr_count.attr,
	NULL
};

static const struct attribute_group sample_timer_attr_group = {
	.attrs = sample_timer_attributes,
};


static int timer_probe(struct platform_device *pdev)
{
	int ret = 0;

	tcu_base = (unsigned int *)ioremap(TCU_REG_BASE, 4 * 1024);
	if (tcu_base == NULL)
	{
		ret = -ENOENT;
		dev_err(&pdev->dev, "ioremap memery error\n");
		goto error_ioremap;
	}

	tdev = devm_kzalloc(&pdev->dev, sizeof(struct timer_dev), GFP_KERNEL);
	if (!tdev)
	{
		ret = -ENOENT;
		dev_err(&pdev->dev, "kzalloc pwm device memery error\n");
		goto error_devm_kzalloc;
	}

	tdev->cell = mfd_get_cell(pdev);
	if (!tdev->cell)
	{
		ret = -ENOENT;
		dev_err(&pdev->dev, "Failed to get mfd cell for jz_adc_aux!\n");
		goto error_get_cell;
	}

	tdev->dev = &pdev->dev;
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	tdev->tcu = (struct ingenic_tcu_chn *)tdev->cell->platform_data;
	tcu_channel = tdev->tcu->cib.id;
#else
	tdev->tcu = (struct jz_tcu_chn *)tdev->cell->platform_data;
	tcu_channel = tdev->tcu->index;
#endif
	printk("TCU%d\n", tcu_channel);
	tdev->tcu->irq_type = FULL_IRQ_MODE;
	tdev->tcu->clk_src = TCU_CLKSRC_EXT;
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	tdev->tcu->is_pwm = 0;
	tdev->tcu->cib.func = TRACKBALL_FUNC;
	tdev->tcu->clk_div = TCU_PRESCALE_64;
	ingenic_tcu_config(tdev->tcu);
#else
	tdev->tcu->prescale = TCU_PRESCALE_64;
	jz_tcu_config_chn(tdev->tcu);
#endif
	set_irq_time(tcu_channel, 1000000 / 600);
	platform_set_drvdata(pdev, tdev);
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	ingenic_tcu_channel_to_virq(tdev->tcu);
	tdev->irqn = tdev->tcu->virq[0];
#else
	tdev->irqn = platform_get_irq(pdev, 0);
#endif
	if (tdev->irqn < 0)
	{
		ret = tdev->irqn;
		dev_err(&pdev->dev, "Failed to get platform irq: %d\n", ret);
		goto error_get_irq;
	}

	ret = request_irq(tdev->irqn, jz_tcu_interrupt, 0, "jz_tcu_interrupt", tdev);
	if (ret)
	{
		dev_err(&pdev->dev, "Failed to run request_irq()!\n");
		goto error_request_irq;
	}
	tdev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	tdev->misc_dev.name = "sample_timer";
	ret = misc_register(&tdev->misc_dev);
	if (ret < 0)
	{
		ret = -ENOENT;
		dev_err(&pdev->dev, "misc_register failed\n");
		goto error_misc_register;
	}
    printk("sample_timer probe OK!!\n");
	ret = sysfs_create_group(&pdev->dev.kobj, &sample_timer_attr_group);
	if (ret < 0)
		goto err_sysfs_create;

#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	tcu_start_counter(tcu_channel);
	disable_timer_irq(tcu_channel);
	tcu_enable_counter(tcu_channel);
#else
	jz_tcu_start_counter(tdev->tcu);
	disable_timer_irq(tcu_channel);
	jz_tcu_enable_counter(tdev->tcu);
#endif
	return 0;

err_sysfs_create:
	misc_deregister(&tdev->misc_dev);
error_misc_register:
	free_irq(tdev->irqn, tdev);
error_request_irq:
error_get_irq:
error_get_cell:
	kfree(tdev);
error_devm_kzalloc:
error_ioremap:
	return ret;
}

static int timer_remove(struct platform_device *pdev)
{
	struct timer_dev *tdev = platform_get_drvdata(pdev);
	disable_timer_irq(tcu_channel);
	printk("%s,tdev=%p\n", __func__, tdev);
	sysfs_remove_group(&pdev->dev.kobj, &sample_timer_attr_group);
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	tcu_stop_counter(tdev->tcu->cib.id);
	tcu_disable_counter(tdev->tcu->cib.id);
#else
	jz_tcu_disable_counter(tdev->tcu);
	jz_tcu_stop_counter(tdev->tcu);
#endif
	free_irq(tdev->irqn, tdev);

	misc_deregister(&tdev->misc_dev);

	kfree(tdev);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id timer_match[] = {
	{ .compatible = "ingenic,tcu_chn3",	},
	{},
};
MODULE_DEVICE_TABLE(of, timer_match);
#else
#endif
static struct platform_driver timer_driver = {
	.probe = timer_probe,
	.remove = timer_remove,
	.driver = {
		.name	= "tcu_chn2", /* Match TCU Channel name */
#ifdef CONFIG_OF
		.of_match_table = timer_match,
#endif
		.owner = THIS_MODULE,
	},
};

static int __init timer_init(void)
{
	return platform_driver_register(&timer_driver);
}

static void __exit timer_exit(void)
{
	platform_driver_unregister(&timer_driver);
}

module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
