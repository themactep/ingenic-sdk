/*
 *  Copyright (C) 2014,  King liuyang <liuyang@ingenic.cn>
 *  jz_PWM support
 */

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/mfd/core.h>
#ifdef CONFIG_SOC_T40
#include <linux/mfd/ingenic-tcu.h>
#else
#include <linux/mfd/jz_tcu.h>
#endif
#include <soc/extal.h>
#include <soc/gpio.h>
#include <linux/slab.h>

#if defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21) || defined(CONFIG_SOC_T40)
#define PWM_NUM		8
#else /* other soc type */
#define PWM_NUM		4
#endif
static int jz_pwm_en[PWM_NUM] = {0};
#define PWM_DRIVER_VERSION "H20210412a"
#ifdef CONFIG_SOC_T40
struct jz_pwm_device{
	short id;
	const char *label;
	struct ingenic_tcu_chn *tcu_cha;
};

struct jz_pwm_gpio
{
	enum gpio_port group;
	unsigned int pins;
	enum gpio_function func;
};
struct jz_pwm_gpio pwm_pin[PWM_NUM] = {
	[0] = {.group = GPIO_PORT_C, .pins = 1 << 2, .func = GPIO_FUNC_2},
	// [0] = {.group = GPIO_PORT_C, .pins = 1 << 27, .func = GPIO_FUNC_1},
	// [0] = {.group = GPIO_PORT_D, .pins = 1 << 26, .func = GPIO_FUNC_2},

	[1] = {.group = GPIO_PORT_C, .pins = 1 << 3, .func = GPIO_FUNC_2},
	// [1] = {.group = GPIO_PORT_C, .pins = 1 << 28, .func = GPIO_FUNC_1},
	// [1] = {.group = GPIO_PORT_D, .pins = 1 << 27, .func = GPIO_FUNC_2},

	// [2] = {.group = GPIO_PORT_B, .pins = 1 << 30, .func = GPIO_FUNC_0},
	[2] = {.group = GPIO_PORT_C, .pins = 1 << 4, .func = GPIO_FUNC_2},

	// [3] = {.group = GPIO_PORT_B, .pins = 1 << 31, .func = GPIO_FUNC_0},
	[3] = {.group = GPIO_PORT_C, .pins = 1 << 5, .func = GPIO_FUNC_2},

	// [4] = {.group = GPIO_PORT_B, .pins = 1 << 25, .func = GPIO_FUNC_1},
	[4] = {.group = GPIO_PORT_C, .pins = 1 << 6, .func = GPIO_FUNC_2},

	// [5] = {.group = GPIO_PORT_B, .pins = 1 << 26, .func = GPIO_FUNC_1},
	[5] = {.group = GPIO_PORT_C, .pins = 1 << 7, .func = GPIO_FUNC_2},

	[6] = {.group = GPIO_PORT_D, .pins = 1 << 22, .func = GPIO_FUNC_2},
	[7] = {.group = GPIO_PORT_D, .pins = 1 << 23, .func = GPIO_FUNC_2},
};

#else
struct jz_pwm_device{
	short id;
	const char *label;
	struct jz_tcu_chn *tcu_cha;
};
#endif

struct jz_pwm_chip {
	struct device *dev;
	const struct mfd_cell *cell;

	struct jz_pwm_device *pwm_chrs;
	struct pwm_chip chip;
};


static inline struct jz_pwm_chip *to_jz(struct pwm_chip *chip)
{
	return container_of(chip, struct jz_pwm_chip, chip);
}

static int jz_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct jz_pwm_chip *jz = to_jz(chip);
#ifdef CONFIG_SOC_T40
	int id = jz->pwm_chrs->tcu_cha->cib.id;
#else
	int id = jz->pwm_chrs->tcu_cha->index;
#endif

	if (id < 0 || id > PWM_NUM)
		return -ENODEV;

	printk("request pwm channel %d successfully\n", id);

	return 0;
}

static void jz_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct jz_pwm_chip *jz = to_jz(chip);
	kfree(jz->pwm_chrs);
	kfree (jz);
}

static int jz_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct jz_pwm_chip *jz = to_jz(chip);
#ifdef CONFIG_SOC_T40
	struct ingenic_tcu_chn *tcu_pwm = jz->pwm_chrs->tcu_cha;

    tcu_start_counter(tcu_pwm->cib.id);
	tcu_enable_counter(tcu_pwm->cib.id);

	jz_pwm_en[tcu_pwm->cib.id] = 1;
#else
	struct jz_tcu_chn *tcu_pwm = jz->pwm_chrs->tcu_cha;

    jz_tcu_start_counter(tcu_pwm);
	jz_tcu_enable_counter(tcu_pwm);

	jz_pwm_en[tcu_pwm->index] = 1;
#endif

#if defined(CONFIG_SOC_T21)
	if(tcu_pwm->index == 1 || tcu_pwm->index == 0) {
		jzgpio_set_func(tcu_pwm->gpio / 32,GPIO_FUNC_0,BIT(tcu_pwm->gpio & 0x1f));
    } else {
		jzgpio_set_func(tcu_pwm->gpio / 32,GPIO_FUNC_1,BIT(tcu_pwm->gpio & 0x1f));
    }
#elif defined(CONFIG_SOC_T31)
	if (tcu_pwm->gpio == 14)
	{
		jzgpio_set_func(tcu_pwm->gpio / 32, GPIO_FUNC_2, BIT(tcu_pwm->gpio & 0x1f));
	}
	else if (tcu_pwm->gpio == (32 + 27) || tcu_pwm->gpio == (32 + 28))
	{
		jzgpio_set_func(tcu_pwm->gpio / 32, GPIO_FUNC_1, BIT(tcu_pwm->gpio & 0x1f));
	}
	else
	{
		jzgpio_set_func(tcu_pwm->gpio / 32, GPIO_FUNC_0, BIT(tcu_pwm->gpio & 0x1f));
	}
#elif defined(CONFIG_SOC_T40)
	jzgpio_set_func(pwm_pin[tcu_pwm->cib.id].group,pwm_pin[tcu_pwm->cib.id].func,pwm_pin[tcu_pwm->cib.id].pins);
#else
	jzgpio_set_func(tcu_pwm->gpio / 32,GPIO_FUNC_0,BIT(tcu_pwm->gpio & 0x1f));
#endif

    return 0;
}

static void jz_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct jz_pwm_chip *jz = to_jz(chip);
#ifdef CONFIG_SOC_T40
	struct ingenic_tcu_chn *tcu_pwm = jz->pwm_chrs->tcu_cha;

	tcu_disable_counter(tcu_pwm->cib.id);
	tcu_stop_counter(tcu_pwm->cib.id);

	jzgpio_set_func(pwm_pin[tcu_pwm->cib.id].group,tcu_pwm->init_level > 0 ? GPIO_OUTPUT0 : GPIO_OUTPUT1 , pwm_pin[tcu_pwm->cib.id].pins);
	jz_pwm_en[tcu_pwm->cib.id] = 0;
#else
    struct jz_tcu_chn *tcu_pwm = jz->pwm_chrs->tcu_cha;
	jz_tcu_disable_counter(tcu_pwm);

	jz_tcu_stop_counter(tcu_pwm);
	jzgpio_set_func(tcu_pwm->gpio / 32,tcu_pwm->init_level > 0 ? GPIO_OUTPUT0 : GPIO_OUTPUT1,BIT(tcu_pwm->gpio & 0x1f));
	jz_pwm_en[tcu_pwm->index] = 0;
#endif
}

static int jz_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			     int duty_ns, int period_ns)
{
	unsigned long long tmp;
	unsigned long period, duty;
	struct jz_pwm_chip *jz = to_jz(chip);
	int prescaler = 0; /*prescale = 0,1,2,3,4,5*/
	static int pwm_func[PWM_NUM] = {0};
#ifdef CONFIG_SOC_T40
	struct ingenic_tcu_chn *tcu_pwm = jz->pwm_chrs->tcu_cha;
#else
	struct jz_tcu_chn *tcu_pwm = jz->pwm_chrs->tcu_cha;
#endif
	if (duty_ns < 0 || duty_ns > period_ns)
		return -EINVAL;
	if (period_ns < 200 || period_ns > 1000000000)
		return -EINVAL;

	tcu_pwm->irq_type = NULL_IRQ_MODE;
	tcu_pwm->is_pwm = pwm->flags;

#ifndef CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH
	tmp = JZ_EXTAL;
#else
	tmp = JZ_EXTAL;//32768 RTC CLOCK failure!
#endif
	tmp = tmp * period_ns;

	do_div(tmp, 1000000000);
	period = tmp;

	while (period > 0xffff && prescaler < 6) {
		period >>= 2;
		++prescaler;
	}
	if (prescaler == 6)
		return -EINVAL;

	tmp = (unsigned long long)period * duty_ns;
	do_div(tmp, period_ns);
	duty = tmp;

	if (duty >= period)
		duty = period - 1;
	tcu_pwm->full_num = period;
	tcu_pwm->half_num = duty;
#ifdef CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH
	tcu_pwm->clk_src = TCU_CLKSRC_RTC;
#else
	tcu_pwm->clk_src = TCU_CLKSRC_EXT;
#endif
	tcu_pwm->shutdown_mode = 0;

#ifdef CONFIG_SOC_T40
	tcu_pwm->clk_div = prescaler;
	ingenic_tcu_config(tcu_pwm);
	ingenic_tcu_set_period(tcu_pwm->cib.id, tcu_pwm->full_num);
	ingenic_tcu_set_duty(tcu_pwm->cib.id, tcu_pwm->half_num);
	if (duty_ns == 0)
	{
		jzgpio_set_func(pwm_pin[tcu_pwm->cib.id].group, tcu_pwm->init_level > 0 ? GPIO_OUTPUT0 : GPIO_OUTPUT1, pwm_pin[tcu_pwm->cib.id].pins);
		pwm_func[tcu_pwm->index] = 0;
	}
	else if (duty_ns == period_ns)
	{
		jzgpio_set_func(pwm_pin[tcu_pwm->cib.id].group, tcu_pwm->init_level > 0 ? GPIO_OUTPUT1 : GPIO_OUTPUT0, pwm_pin[tcu_pwm->cib.id].pins);
		pwm_func[tcu_pwm->index] = 0;
	}
	else if (pwm_func[tcu_pwm->index] == 0 && jz_pwm_en[tcu_pwm->index])
	{
		jzgpio_set_func(pwm_pin[tcu_pwm->cib.id].group, pwm_pin[tcu_pwm->cib.id].func, pwm_pin[tcu_pwm->cib.id].pins);
		pwm_func[tcu_pwm->index] = 1;
	}
#else
	tcu_pwm->prescale = prescaler;
	jz_tcu_config_chn(tcu_pwm);
	jz_tcu_set_period(tcu_pwm, tcu_pwm->full_num);
	jz_tcu_set_duty(tcu_pwm, tcu_pwm->half_num);
	if (duty_ns == 0)
	{
		jzgpio_set_func(tcu_pwm->gpio / 32, tcu_pwm->init_level > 0 ? GPIO_OUTPUT0 : GPIO_OUTPUT1, BIT(tcu_pwm->gpio & 0x1f));
		pwm_func[tcu_pwm->index] = 0;
	}
	else if (duty_ns == period_ns)
	{
		jzgpio_set_func(tcu_pwm->gpio / 32, tcu_pwm->init_level > 0 ? GPIO_OUTPUT1 : GPIO_OUTPUT0, BIT(tcu_pwm->gpio & 0x1f));
		pwm_func[tcu_pwm->index] = 0;
	}
	else if (pwm_func[tcu_pwm->index] == 0 && jz_pwm_en[tcu_pwm->index])
	{
#if defined(CONFIG_SOC_T21)
		if (tcu_pwm->index == 1 || tcu_pwm->index == 0)
			jzgpio_set_func(tcu_pwm->gpio / 32, GPIO_FUNC_0, BIT(tcu_pwm->gpio & 0x1f));
		else
			jzgpio_set_func(tcu_pwm->gpio / 32, GPIO_FUNC_1, BIT(tcu_pwm->gpio & 0x1f));
#elif defined(CONFIG_SOC_T31)
		if (tcu_pwm->gpio == 14)
			jzgpio_set_func(tcu_pwm->gpio / 32, GPIO_FUNC_2, BIT(tcu_pwm->gpio & 0x1f));
		else if (tcu_pwm->gpio == (32 + 27) || tcu_pwm->gpio == (32 + 28))
			jzgpio_set_func(tcu_pwm->gpio / 32, GPIO_FUNC_1, BIT(tcu_pwm->gpio & 0x1f));
		else
			jzgpio_set_func(tcu_pwm->gpio / 32, GPIO_FUNC_0, BIT(tcu_pwm->gpio & 0x1f));
#else
		jzgpio_set_func(tcu_pwm->gpio / 32, GPIO_FUNC_0, BIT(tcu_pwm->gpio & 0x1f));
#endif
		pwm_func[tcu_pwm->index] = 1;
	}

#endif
//	printk("hwpwm = %d index = %d\n", pwm->hwpwm, tcu_pwm->index);

	return 0;
}

int jz_pwm_set_polarity(struct pwm_chip *chip,struct pwm_device *pwm, enum pwm_polarity polarity)
{
	struct jz_pwm_chip *jz = to_jz(chip);
#ifdef CONFIG_SOC_T40
	struct ingenic_tcu_chn *tcu_pwm = jz->pwm_chrs->tcu_cha;
#else
	struct jz_tcu_chn *tcu_pwm = jz->pwm_chrs->tcu_cha;
#endif

	if(polarity == PWM_POLARITY_INVERSED)
		tcu_pwm->init_level = 0;

	if(polarity == PWM_POLARITY_NORMAL)
		tcu_pwm->init_level = 1;

	return 0;
}

static const struct pwm_ops jz_pwm_ops = {
	.request = jz_pwm_request,
	.free = jz_pwm_free,
	.config = jz_pwm_config,
	.set_polarity = jz_pwm_set_polarity,
	.enable = jz_pwm_enable,
	.disable = jz_pwm_disable,
	.owner = THIS_MODULE,
};

static int jz_pwm_probe(struct platform_device *pdev)
{
	struct jz_pwm_chip *jz;
	int ret;

	jz = kzalloc(sizeof(struct jz_pwm_chip), GFP_KERNEL);

	jz->pwm_chrs = kzalloc(sizeof(struct jz_pwm_device),GFP_KERNEL);
	if (!jz || !jz->pwm_chrs)
		return -ENOMEM;

	jz->cell = mfd_get_cell(pdev);
	if (!jz->cell) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "Failed to get mfd cell\n");
		goto err_free;
	}

	jz->chip.dev = &pdev->dev;
	jz->chip.ops = &jz_pwm_ops;
	jz->chip.npwm = PWM_NUM;
	jz->chip.base = -1;

#ifdef CONFIG_SOC_T40
	jz->pwm_chrs->tcu_cha = (struct ingenic_tcu_chn *)jz->cell->platform_data;
#else
	jz->pwm_chrs->tcu_cha = (struct jz_tcu_chn *)jz->cell->platform_data;
#endif

	ret = pwmchip_add(&jz->chip);
	if (ret < 0) {
		printk("%s[%d] ret = %d\n", __func__,__LINE__, ret);
		goto err_free;
	}

	if(pdev->dev.driver)
		printk("%s[%d] d_name = %s\n", __func__,__LINE__,pdev->dev.driver->name);
	platform_set_drvdata(pdev, jz);

	return 0;
err_free:
		kfree(jz->pwm_chrs);
		kfree(jz);
	return ret;
}

static int jz_pwm_remove(struct platform_device *pdev)
{
	struct jz_pwm_chip *jz = platform_get_drvdata(pdev);
	int ret;

	ret = pwmchip_remove(&jz->chip);
	if (ret < 0)
		return ret;


	return 0;
}

#ifdef CONFIG_PWM0
static struct platform_driver jz_pwm0_driver = {
#ifdef CONFIG_SOC_T40
	.driver = {
		.name = "ingenic,tcu_chn0",
		.owner = THIS_MODULE,
	},
#else
	.driver = {
		.name = "tcu_chn0",
		.owner = THIS_MODULE,
	},
#endif
	.probe = jz_pwm_probe,
	.remove = jz_pwm_remove,
};
#endif

#ifdef CONFIG_PWM1
static struct platform_driver jz_pwm1_driver = {
#ifdef CONFIG_SOC_T40
	.driver = {
		.name = "ingenic,tcu_chn1",
		.owner = THIS_MODULE,
	},
#else
	.driver = {
		.name = "tcu_chn1",
		.owner = THIS_MODULE,
	},
#endif
	.probe = jz_pwm_probe,
	.remove = jz_pwm_remove,
};
#endif

#ifdef CONFIG_PWM2
static struct platform_driver jz_pwm2_driver = {
#ifdef CONFIG_SOC_T40
	.driver = {
		.name = "ingenic,tcu_chn2",
		.owner = THIS_MODULE,
	},
#else
	.driver = {
		.name = "tcu_chn2"
		.owner = THIS_MODULE,
	},
#endif
	.probe = jz_pwm_probe,
	.remove = jz_pwm_remove,
};
#endif

#ifdef CONFIG_PWM3
static struct platform_driver jz_pwm3_driver = {
#ifdef CONFIG_SOC_T40
	.driver = {
		.name = "ingenic,tcu_chn3",
		.owner = THIS_MODULE,
	},
#else
	.driver = {
		.name = "tcu_chn3",
		.owner = THIS_MODULE,
	},
#endif
	.probe = jz_pwm_probe,
	.remove = jz_pwm_remove,
};
#endif

#if defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21) || defined(CONFIG_SOC_T40)
#if (PWM_NUM > 4)
#ifdef CONFIG_PWM4
static struct platform_driver jz_pwm4_driver = {
#ifdef CONFIG_SOC_T40
	.driver = {
		.name = "ingenic,tcu_chn4",
		.owner = THIS_MODULE,
	},
#else
	.driver = {
		.name = "tcu_chn4",
		.owner = THIS_MODULE,
	},
#endif
	.probe = jz_pwm_probe,
	.remove = jz_pwm_remove,
};
#endif
#endif

#if (PWM_NUM > 5)
#ifdef CONFIG_PWM5
static struct platform_driver jz_pwm5_driver = {
#ifdef CONFIG_SOC_T40
	.driver = {
		.name = "ingenic,tcu_chn5",
		.owner = THIS_MODULE,
	},
#else
	.driver = {
		.name = "tcu_chn05",
		.owner = THIS_MODULE,
	},
#endif
	.probe = jz_pwm_probe,
	.remove = jz_pwm_remove,
};
#endif
#endif

#if (PWM_NUM > 6)
#ifdef CONFIG_PWM6
static struct platform_driver jz_pwm6_driver = {
#ifdef CONFIG_SOC_T40
	.driver = {
		.name = "ingenic,tcu_chn6",
		.owner = THIS_MODULE,
	},
#else
	.driver = {
		.name = "tcu_chn6",
		.owner = THIS_MODULE,
	},
#endif
	.probe = jz_pwm_probe,
	.remove = jz_pwm_remove,
};
#endif
#endif

#if (PWM_NUM > 7)
#ifdef CONFIG_PWM7
static struct platform_driver jz_pwm7_driver = {
#ifdef CONFIG_SOC_T40
	.driver = {
		.name = "ingenic,tcu_chn7",
		.owner = THIS_MODULE,
	},
#else
	.driver = {
		.name = "tcu_chn7",
		.owner = THIS_MODULE,
	},
#endif
	.probe = jz_pwm_probe,
	.remove = jz_pwm_remove,
};
#endif
#endif
#else /* T20 */
#if (defined(CONFIG_PWM4) || defined(CONFIG_PWM5) || defined(CONFIG_PWM6) || defined(CONFIG_PWM7))
#error "Can't support the pwm in t10/t20 chip!"
#endif
#endif

static int __init pwm_init(void)
{
#ifdef CONFIG_PWM0
	platform_driver_register(&jz_pwm0_driver);
#endif
#ifdef CONFIG_PWM1
	platform_driver_register(&jz_pwm1_driver);
#endif
#ifdef CONFIG_PWM2
	platform_driver_register(&jz_pwm2_driver);
#endif
#ifdef CONFIG_PWM3
	platform_driver_register(&jz_pwm3_driver);
#endif
#ifdef CONFIG_PWM4
	platform_driver_register(&jz_pwm4_driver);
#endif
#ifdef CONFIG_PWM5
	platform_driver_register(&jz_pwm5_driver);
#endif
#ifdef CONFIG_PWM6
	platform_driver_register(&jz_pwm6_driver);
#endif
#ifdef CONFIG_PWM7
	platform_driver_register(&jz_pwm7_driver);
#endif
	printk("The version of PWM driver is %s\n", PWM_DRIVER_VERSION);
	return 0;
}

static void __exit pwm_exit(void)
{
#ifdef CONFIG_PWM0
	platform_driver_unregister(&jz_pwm0_driver);
#endif
#ifdef CONFIG_PWM1
	platform_driver_unregister(&jz_pwm1_driver);
#endif
#ifdef CONFIG_PWM2
	platform_driver_unregister(&jz_pwm2_driver);
#endif
#ifdef CONFIG_PWM3
	platform_driver_unregister(&jz_pwm3_driver);
#endif
#ifdef CONFIG_PWM4
	platform_driver_unregister(&jz_pwm4_driver);
#endif
#ifdef CONFIG_PWM5
	platform_driver_unregister(&jz_pwm5_driver);
#endif
#ifdef CONFIG_PWM6
	platform_driver_unregister(&jz_pwm6_driver);
#endif
#ifdef CONFIG_PWM7
	platform_driver_unregister(&jz_pwm7_driver);
#endif
}
#undef PWM_NUM
module_init(pwm_init);
module_exit(pwm_exit);

MODULE_ALIAS("platform:jz-pwm");
MODULE_LICENSE("GPL");
