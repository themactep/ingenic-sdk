/*
 * pwm api code;
 *
 * Copyright (c) 2015 Ingenic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fs.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#ifdef CONFIG_SOC_T40
#include <linux/mfd/ingenic-tcu.h>
#else
#include <linux/mfd/jz_tcu.h>
#endif
#include <linux/spinlock.h>

/* Default-enable channels 0..3 if not provided by Kconfig/Kbuild */
#ifndef CONFIG_PWM0
#define CONFIG_PWM0
#endif
#ifndef CONFIG_PWM1
#define CONFIG_PWM1
#endif
#ifndef CONFIG_PWM2
#define CONFIG_PWM2
#endif
#ifndef CONFIG_PWM3
#define CONFIG_PWM3
#endif

#if defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T40)
#define PWM_NUM		8
#else /* other soc type */
#define PWM_NUM		4
#endif
#define PWM_CONFIG		0x001
#define PWM_CONFIG_DUTY		0x002
#define PWM_ENABLE		0x010
#define PWM_DISABLE		0x100
#define PWM_QUERY_STATUS	0x200

struct platform_device pwm_device = {
	.name = "pwm-jz",
	.id = -1,
};

struct pwm_lookup jz_pwm_lookup[] = {
#ifdef CONFIG_SOC_T40
#ifdef CONFIG_PWM0
	PWM_LOOKUP("ingenic,tcu_chn0.0", 1, "pwm-jz", "pwm-jz.0", 2000, 1),
#endif
#ifdef CONFIG_PWM1
	PWM_LOOKUP("ingenic,tcu_chn1.1", 1, "pwm-jz", "pwm-jz.1", 2000, 0),
#endif
#ifdef CONFIG_PWM2
	PWM_LOOKUP("ingenic,tcu_chn2.2", 1, "pwm-jz", "pwm-jz.2", 2000, 0),
#endif
#ifdef CONFIG_PWM3
	PWM_LOOKUP("ingenic,tcu_chn3.3", 1, "pwm-jz", "pwm-jz.3", 2000, 0),
#endif
#ifdef CONFIG_PWM4
	PWM_LOOKUP("ingenic,tcu_chn4.4", 1, "pwm-jz", "pwm-jz.4", 2000, 0),
#endif
#ifdef CONFIG_PWM5
	PWM_LOOKUP("ingenic,tcu_chn5.5", 1, "pwm-jz", "pwm-jz.5", 2000, 0),
#endif
#ifdef CONFIG_PWM6
	PWM_LOOKUP("ingenic,tcu_chn6.6", 1, "pwm-jz", "pwm-jz.6", 2000, 0),
#endif
#ifdef CONFIG_PWM7
	PWM_LOOKUP("ingenic,tcu_chn7.7", 1, "pwm-jz", "pwm-jz.7", 2000, 0),
#endif
#else
#ifdef CONFIG_PWM0
	PWM_LOOKUP("tcu_chn0.0", 1, "pwm-jz", "pwm-jz.0"),
#endif
#ifdef CONFIG_PWM1
	PWM_LOOKUP("tcu_chn1.1", 1, "pwm-jz", "pwm-jz.1"),
#endif
#ifdef CONFIG_PWM2
	PWM_LOOKUP("tcu_chn2.2", 1, "pwm-jz", "pwm-jz.2"),
#endif
#ifdef CONFIG_PWM3
	PWM_LOOKUP("tcu_chn3.3", 1, "pwm-jz", "pwm-jz.3"),
#endif
#ifdef CONFIG_PWM4
	PWM_LOOKUP("tcu_chn4.4", 1, "pwm-jz", "pwm-jz.4"),
#endif
#ifdef CONFIG_PWM5
	PWM_LOOKUP("tcu_chn5.5", 1, "pwm-jz", "pwm-jz.5"),
#endif
#ifdef CONFIG_PWM6
	PWM_LOOKUP("tcu_chn6.6", 1, "pwm-jz", "pwm-jz.6"),
#endif
#ifdef CONFIG_PWM7
	PWM_LOOKUP("tcu_chn7.7", 1, "pwm-jz", "pwm-jz.7"),
#endif
#endif
};

struct pwm_ioctl_t {
	int index;
	int duty;
	int period;
	int polarity;
	int enabled;
};

struct pwm_device_t {
	int duty;
	int period;
	int polarity;
	int enabled;
	struct pwm_device *pwm_device;
};

struct pwm_jz_t {
	struct device *dev;
	struct miscdevice mdev;
	struct pwm_device_t *pwm_device_t[PWM_NUM];
	spinlock_t pwm_lock;
	struct mutex mlock;
};

static int pwm_jz_open(struct inode *inode, struct file *filp)
{
	pr_debug("pwm_hal: /dev/pwm open\n");

	return 0;
}

static int pwm_jz_release(struct inode *inode, struct file *filp)
{
	pr_debug("pwm_hal: /dev/pwm release\n");

	return 0;
}

/* Debug helper: decode ioctl cmd */
static const char *pwm_cmd_name(unsigned int cmd)
{
	switch (cmd) {
		case PWM_CONFIG: return "PWM_CONFIG";
		case PWM_CONFIG_DUTY: return "PWM_CONFIG_DUTY";
		case PWM_ENABLE: return "PWM_ENABLE";
		case PWM_DISABLE: return "PWM_DISABLE";
		case PWM_QUERY_STATUS: return "PWM_QUERY_STATUS";
		default: return "UNKNOWN";
	}
}

static long pwm_jz_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int id, ret = 0;
	struct pwm_ioctl_t pwm_ioctl;
	struct miscdevice *dev = filp->private_data;
	struct pwm_jz_t *gpwm = container_of(dev, struct pwm_jz_t, mdev);

	pr_debug("pwm_hal: ioctl cmd=%s(0x%x)\n", pwm_cmd_name(cmd), cmd);

	mutex_lock(&gpwm->mlock);
	switch(cmd) {
		case PWM_CONFIG:
			ret = copy_from_user(&pwm_ioctl, (void __user *)arg, sizeof(pwm_ioctl));
			if (ret) {
				dev_err(gpwm->dev, "ioctl error(%d) !\n", __LINE__);
				ret = -1;
				break;
			}

			id = pwm_ioctl.index;
			if ((id >= PWM_NUM) || (id < 0)) {
				dev_err(gpwm->dev, "ioctl error(%d) !\n", __LINE__);
				ret = -1;
				break;
			}
				if (!gpwm->pwm_device_t[id]) {
					dev_err(gpwm->dev, "channel %d not available\n", id);
					ret = -ENODEV;
					break;
				}


			if ((pwm_ioctl.period > 1000000000) || (pwm_ioctl.period < 200)) {
				dev_err(gpwm->dev, "period error !\n");
				ret = -1;
				break;
			}

			if ((pwm_ioctl.duty > pwm_ioctl.period) || (pwm_ioctl.duty < 0)) {
				dev_err(gpwm->dev, "duty error !\n");
				ret = -1;
				break;
			}

			if ((pwm_ioctl.polarity > 1) || (pwm_ioctl.polarity < 0)) {
				dev_err(gpwm->dev, "polarity error !\n");
				ret = -1;
				break;
			}

			gpwm->pwm_device_t[id]->period = pwm_ioctl.period;
			gpwm->pwm_device_t[id]->duty = pwm_ioctl.duty;
			gpwm->pwm_device_t[id]->polarity = pwm_ioctl.polarity;

			break;
		case PWM_CONFIG_DUTY:
			ret = copy_from_user(&pwm_ioctl, (void __user *)arg, sizeof(pwm_ioctl));
			if (ret) {
				dev_err(gpwm->dev, "ioctl error(line %d) !\n", __LINE__);
				ret = -1;
				break;
			}

			if ((pwm_ioctl.duty > pwm_ioctl.period) || (pwm_ioctl.duty < 0)) {
				dev_err(gpwm->dev, "duty error(line %d) !\n",__LINE__);
				ret = -1;
				break;
			}

			id = pwm_ioctl.index;
				if (!gpwm->pwm_device_t[id]) {
					dev_err(gpwm->dev, "channel %d not available\n", id);
					ret = -ENODEV;
					break;
				}

			gpwm->pwm_device_t[id]->duty = pwm_ioctl.duty;
			pwm_config(gpwm->pwm_device_t[id]->pwm_device, gpwm->pwm_device_t[id]->duty, gpwm->pwm_device_t[id]->period);

			break;
		case PWM_ENABLE:
		{
			/* Accept both legacy (arg = channel id) and struct-pointer forms */
			int ch = (int)arg;
			if (ch < 0 || ch >= PWM_NUM) {
				if (copy_from_user(&pwm_ioctl, (void __user *)arg, sizeof(pwm_ioctl))) {
					dev_err(gpwm->dev, "ioctl error(%d) !\n", __LINE__);
					ret = -EFAULT;
					break;
				}
				ch = pwm_ioctl.index;
			}
			id = ch;

			if ((id >= PWM_NUM) || (id < 0)) {
				dev_err(gpwm->dev, "ioctl error(%d) !\n", __LINE__);
				ret = -1;
				break;
			}

			if (!gpwm->pwm_device_t[id]) {
				dev_err(gpwm->dev, "channel %d not available\n", id);
				ret = -ENODEV;
				break;
			}

			if ((gpwm->pwm_device_t[id]->pwm_device == NULL) || (IS_ERR(gpwm->pwm_device_t[id]->pwm_device))) {
				dev_err(gpwm->dev, "pwm could not work !\n");
				ret = -1;
				break;
			}

			if ((gpwm->pwm_device_t[id]->period == -1) || (gpwm->pwm_device_t[id]->duty == -1) || (gpwm->pwm_device_t[id]->polarity == -1)) {
				dev_err(gpwm->dev, "the parameter of pwm could not init !\n");
				ret = -1;
				break;
			}

			if (gpwm->pwm_device_t[id]->polarity == 0)
				pwm_set_polarity(gpwm->pwm_device_t[id]->pwm_device, PWM_POLARITY_INVERSED);
			else
				pwm_set_polarity(gpwm->pwm_device_t[id]->pwm_device, PWM_POLARITY_NORMAL);

			pwm_enable(gpwm->pwm_device_t[id]->pwm_device);
			gpwm->pwm_device_t[id]->enabled = 1;

			pwm_config(gpwm->pwm_device_t[id]->pwm_device, gpwm->pwm_device_t[id]->duty, gpwm->pwm_device_t[id]->period);
			break;
		}

		case PWM_DISABLE:
		{
			/* Accept both legacy (arg = channel id) and struct-pointer forms */
			int ch = (int)arg;
			if (ch < 0 || ch >= PWM_NUM) {
				if (copy_from_user(&pwm_ioctl, (void __user *)arg, sizeof(pwm_ioctl))) {
					dev_err(gpwm->dev, "ioctl error(%d) !\n", __LINE__);
					ret = -EFAULT;
					break;
				}
				ch = pwm_ioctl.index;
			}
			id = ch;

			if ((id >= PWM_NUM) || (id < 0)) {
				dev_err(gpwm->dev, "ioctl error(%d) !\n", __LINE__);
				ret = -1;
				break;
			}

			if (!gpwm->pwm_device_t[id]) {
				dev_err(gpwm->dev, "channel %d not available\n", id);
				ret = -ENODEV;
				break;
			}

			if ((gpwm->pwm_device_t[id]->pwm_device == NULL) || (IS_ERR(gpwm->pwm_device_t[id]->pwm_device))) {
				dev_err(gpwm->dev, "pwm could not work !\n");
				ret = -1;
				break;
			}

			if (!gpwm->pwm_device_t[id]) {
				dev_err(gpwm->dev, "channel %d not available\n", id);
				ret = -ENODEV;
				break;
			}

			if ((gpwm->pwm_device_t[id]->period == -1) || (gpwm->pwm_device_t[id]->duty == -1) || (gpwm->pwm_device_t[id]->polarity == -1)) {
				dev_err(gpwm->dev, "the parameter of pwm could not init !\n");
				ret = -1;
				break;
			}

			if (!gpwm->pwm_device_t[id]) {
				dev_err(gpwm->dev, "channel %d not available\n", id);
				ret = -ENODEV;
				break;
			}

			pwm_disable(gpwm->pwm_device_t[id]->pwm_device);
			gpwm->pwm_device_t[id]->enabled = 0;

			break;
			}

		case PWM_QUERY_STATUS:
			if (copy_from_user(&pwm_ioctl, (struct pwm_ioctl_t __user *)arg, sizeof(pwm_ioctl))) {
				dev_err(gpwm->dev, "Error copying data from user space!\n");
				ret = -EFAULT;
				break;
			}

			id = pwm_ioctl.index;

			if ((id >= PWM_NUM) || (id < 0)) {
				dev_err(gpwm->dev, "ioctl error(%d) !\n", __LINE__);
				ret = -1;
				break;
			}

			if (!gpwm->pwm_device_t[id]) {
				dev_err(gpwm->dev, "channel %d not available\n", id);
				ret = -ENODEV;
				break;
			}

			dev_dbg(gpwm->dev, "Queried PWM Channel: %d\n", id);

			pwm_ioctl.duty = gpwm->pwm_device_t[id]->duty;
			pwm_ioctl.period = gpwm->pwm_device_t[id]->period;
			pwm_ioctl.polarity = gpwm->pwm_device_t[id]->polarity;
			pwm_ioctl.enabled = gpwm->pwm_device_t[id]->enabled;

			dev_dbg(gpwm->dev, "Channel %d - Duty: %d, Period: %d, Polarity: %d\n",
				id, pwm_ioctl.duty, pwm_ioctl.period, pwm_ioctl.polarity);

			if (copy_to_user((void __user *)arg, &pwm_ioctl, sizeof(pwm_ioctl))) {
				dev_err(gpwm->dev, "Error copying data to user space!\n");
				ret = -EFAULT;
			}
			break;

		default:
			dev_err(gpwm->dev, "unsupport cmd !\n");
			break;
	}

	mutex_unlock(&gpwm->mlock);

	return ret;
}

static struct file_operations pwm_jz_fops = {
	.owner		= THIS_MODULE,
	.open		= pwm_jz_open,
	.release	= pwm_jz_release,
	.unlocked_ioctl	= pwm_jz_ioctl,
};

static int jz_pwm_probe(struct platform_device *pdev)
{
	int i, ret;
	struct pwm_jz_t *gpwm;
	char pd_name[10];

	gpwm = devm_kzalloc(&pdev->dev, sizeof(struct pwm_jz_t), GFP_KERNEL);
	dev_dbg(&pdev->dev, "pwm_hal: probe start, PWM_NUM=%d\n", PWM_NUM);

	if (gpwm == NULL) {
		dev_err(&pdev->dev, "devm_kzalloc gpwm error !\n");
		return -ENOMEM;
	}

	for (i = 0; i < PWM_NUM; i++) {
		gpwm->pwm_device_t[i] = devm_kzalloc(&pdev->dev, sizeof(struct pwm_device_t), GFP_KERNEL);
		if (gpwm->pwm_device_t[i] == NULL) {
			dev_err(&pdev->dev, "devm_kzalloc pwm_device_t error !\n");
			return -ENOMEM;
		}

		sprintf(pd_name, "pwm-jz.%d", i);
		dev_dbg(&pdev->dev, "pwm_hal: requesting pwm by name '%s' (channel %d)\n", pd_name, i);

		gpwm->pwm_device_t[i]->pwm_device = devm_pwm_get(&pdev->dev, pd_name);
		if (IS_ERR(gpwm->pwm_device_t[i]->pwm_device)) {
			int err = PTR_ERR(gpwm->pwm_device_t[i]->pwm_device);
			/* If provider not ready yet, defer probe so we re-try later when providers bind */
			if (err == -EPROBE_DEFER) {
				dev_dbg(&pdev->dev, "PWM provider not ready for channel %d, deferring probe\n", i);
				return err;
			}
			/* If busy, skip this channel but keep the driver alive */
			if (err == -EBUSY) {
				dev_warn(&pdev->dev, "PWM channel %d busy (error %d), skipping...\n", i, err);
				gpwm->pwm_device_t[i]->pwm_device = NULL;
				continue;
			}
			/* Other errors are fatal */
			dev_err(&pdev->dev, "devm_pwm_get error for channel %d: %d\n", i, err);

			return err;
		}
		dev_dbg(&pdev->dev, "pwm_hal: acquired pwm channel %d\n", i);

		gpwm->pwm_device_t[i]->duty = -1;
		gpwm->pwm_device_t[i]->period = -1;
		gpwm->pwm_device_t[i]->polarity = -1;
	}

	spin_lock_init(&gpwm->pwm_lock);
	mutex_init(&gpwm->mlock);
	gpwm->mdev.minor = MISC_DYNAMIC_MINOR;
	gpwm->mdev.name = "pwm";
	gpwm->mdev.fops = &pwm_jz_fops;
	ret = misc_register(&gpwm->mdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "misc_register failed !\n");
	} else {
		dev_dbg(&pdev->dev, "pwm_hal: misc_register OK, /dev/pwm ready\n");
	}

	gpwm->dev = &pdev->dev;

	platform_set_drvdata(pdev, gpwm);

	{
		int ok = 0, j;
		for (j = 0; j < PWM_NUM; j++) {
			if (gpwm->pwm_device_t[j] && gpwm->pwm_device_t[j]->pwm_device && !IS_ERR(gpwm->pwm_device_t[j]->pwm_device)) ok++;
		}
		dev_dbg(&pdev->dev, "pwm-hal bound to %d channel(s)\n", ok);
	}

	dev_dbg(&pdev->dev, "%s register ok !\n", __func__);

	return 0;
}

static int jz_pwm_remove(struct platform_device *pdev)
{
	struct pwm_jz_t *gpwm = platform_get_drvdata(pdev);
	int i = 0;
	if (gpwm == NULL)
		return 0;
	misc_deregister(&gpwm->mdev);

	for(i = 0; i < PWM_NUM; i++) {
		if (gpwm->pwm_device_t[i]->pwm_device) {
			devm_pwm_put(&pdev->dev, gpwm->pwm_device_t[i]->pwm_device);
			devm_kfree(&pdev->dev, gpwm->pwm_device_t[i]);
		}
	}
	devm_kfree(&pdev->dev, gpwm);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver jz_pwm_driver = {
	.driver = {
		.name = "pwm-jz",
		.owner = THIS_MODULE,
	},
	.probe = jz_pwm_probe,
	.remove = jz_pwm_remove,
};


static int __init pwm_init(void)
{
	int ret;
	int jz_pwm_lookup_size = ARRAY_SIZE(jz_pwm_lookup);

	pr_debug("pwm_hal: registering platform driver and device; lookup entries=%d\n", jz_pwm_lookup_size);

	/* Register driver first so device bind happens with providers possibly ready */
	ret = platform_driver_register(&jz_pwm_driver);
	if (ret)
		return ret;

	pwm_add_table(jz_pwm_lookup, jz_pwm_lookup_size);

	ret = platform_device_register(&pwm_device);
	if (ret) {
		platform_driver_unregister(&jz_pwm_driver);

		return ret;
	}
	pr_debug("pwm_hal: platform device pwm-jz registered\n");

	return 0;
}

static void __exit pwm_exit(void)
{
	/* Unregister device then driver to avoid dangling device */
	platform_device_unregister(&pwm_device);
	platform_driver_unregister(&jz_pwm_driver);
}

module_init(pwm_init);
module_exit(pwm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("JZ PWM Driver");
