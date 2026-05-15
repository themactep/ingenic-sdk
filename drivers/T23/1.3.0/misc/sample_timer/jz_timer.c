#define DEBUG

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/mempolicy.h>
#include <linux/mfd/core.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <soc/gpio.h>
#if defined(CONFIG_OF)
#include <linux/mfd/ingenic-tcu.h>
#else
#include <linux/mfd/jz_tcu.h>
#endif

#define CON_GPIO GPIO_PB(17)
#define DEFAULT_TIME 100 // 100ms
unsigned int def_time = DEFAULT_TIME;
module_param(def_time, uint, S_IRUGO);
MODULE_PARM_DESC(def_time, "Timer default time\n");
#define TIMER_STOP 0x1
#define TIMER_START 0x2
#define TIMER_SET_TIME 0x3

#define TCU_TDFR0 0x40
#define TCU_REG_BASE 0x10002000
#define TCU_TMSTR 0x34 // W 置1
#define TCU_TMCLR 0x38 // W 清0

#define TCU_PRESCALE TCU_PRESCALE_1024 //TCU PRESCALE
#define TCU_PRE (0x1 << (TCU_PRESCALE << 1))

static void __iomem *tcu_base;

struct timer_dev
{
    const struct mfd_cell *cell;
    struct device *dev;
    struct miscdevice misc_dev;
#if defined(CONFIG_OF)
    struct ingenic_tcu_chn *tcu;
#else
    struct jz_tcu_chn *tcu;
#endif
    unsigned int chn;
    unsigned int timems;
    int irqn;
};
struct timer_dev *tdev;

static int timer_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int timer_release(struct inode *inode, struct file *file)
{
    return 0;
}

void enable_timer_irq(unsigned int num)
{
    writel(1 << num, tcu_base + TCU_TMCLR);
}
void disable_timer_irq(unsigned int num)
{
    writel(1 << num, tcu_base + TCU_TMSTR);
}

void set_irq_time(unsigned int num, unsigned int ms)
{
    unsigned int tdfr = 0;
    printk("TCU_PRESCALE %d TCU_PRE %d\n", TCU_PRESCALE, TCU_PRE);
    tdfr = (24 * 1024 * 1024  / TCU_PRE / 1000) * ms;
    printk("count tdfr %d\n", tdfr);
    tdfr < 2 ? tdfr = 1 : --tdfr;
    writel(tdfr < 0xffff ? tdfr : 0xffff, tcu_base + (TCU_TDFR0 + 0x10 * num));
#if defined(CONFIG_OF)
    tcu_set_counter(num, 0);
#else
    jz_tcu_set_count(tdev->tcu, 0); // Clear Count
#endif
}

static long timer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

    switch (cmd)
    {
    case TIMER_STOP:
        printk("%s[%d]: stop\n", __func__, __LINE__);
        disable_irq(tdev->irqn);
#if defined(CONFIG_OF)
        tcu_stop_counter(tdev->tcu->cib.id);
        tcu_disable_counter(tdev->tcu->cib.id);
#else
        jz_tcu_disable_counter(tdev->tcu);
        jz_tcu_stop_counter(tdev->tcu);
#endif
        break;

    case TIMER_START:
        printk("%s[%d]: start %d ms\n", __func__, __LINE__, tdev->timems);
#if defined(CONFIG_OF)
        tcu_start_counter(tdev->chn);
        tcu_enable_counter(tdev->chn);
#else
        jz_tcu_start_counter(tdev->tcu);
        jz_tcu_enable_counter(tdev->tcu);
#endif
        enable_irq(tdev->irqn);
        break;

    case TIMER_SET_TIME:
        get_user(tdev->timems, (u32 *)arg);
        printk("time=%d ms\n", tdev->timems);
        set_irq_time(tdev->chn, tdev->timems); // 定时器时间
        break;

    default:
        return -EINVAL;
    }
    return 0;
}

static irqreturn_t timer_interrupt(int irq, void *devid)
{
    static int level = 0;
    level = !level;
    gpio_set_value(CON_GPIO, level);
    // printk("T23 TCU test!\n");
    return IRQ_HANDLED;
}

static struct file_operations timer_fops = {
    .owner = THIS_MODULE,
    .open = timer_open,
    .release = timer_release,
    .unlocked_ioctl = timer_ioctl,
};

static int timer_probe(struct platform_device *pdev)
{
    int ret = 0;

    if (gpio_request(CON_GPIO, "timer_irq"))
    {
        printk("request gpio:%d fail !!!\n", CON_GPIO);
        goto error_gpio_request;
    }
    gpio_direction_output(CON_GPIO, 1);
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
        dev_err(&pdev->dev, "kzalloc memery failed\n");
        goto error_devm_kzalloc;
    }

    tdev->timems = def_time;
    tdev->dev = &pdev->dev;

    tdev->cell = mfd_get_cell(pdev);
    if (!tdev->cell)
    {
        ret = -ENOENT;
        dev_err(&pdev->dev, "Failed to get mfd cell\n");
        goto error_get_cell;
    }

#if defined(CONFIG_OF)
    tdev->tcu = (struct ingenic_tcu_chn *)tdev->cell->platform_data;
    tdev->chn = tdev->tcu->cib.id;
#else
    tdev->tcu = (struct jz_tcu_chn *)tdev->cell->platform_data;
    tdev->chn = tdev->tcu->index;
#endif
    printk("TCU%d\n", tdev->chn);

    tdev->tcu->irq_type = FULL_IRQ_MODE;
    tdev->tcu->clk_src = TCU_CLKSRC_EXT; // 时钟源 24M
#if defined(CONFIG_OF)
    tdev->tcu->is_pwm = 0;
    tdev->tcu->cib.func = TRACKBALL_FUNC;
    tdev->tcu->clk_div = TCU_PRESCALE; // 分频
    ingenic_tcu_config(tdev->tcu);
#else
    tdev->tcu->prescale = TCU_PRESCALE; // 分频
    jz_tcu_config_chn(tdev->tcu);
#endif

    set_irq_time(tdev->chn, tdev->timems);
    platform_set_drvdata(pdev, tdev);

#if defined(CONFIG_OF)
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
    ret = request_irq(tdev->irqn, timer_interrupt, 0, "tcu_irq", tdev);
    if (ret)
    {
        dev_err(tdev->dev, "Failed to request irq %d\n", ret);
        goto error_request_irq;
    }
    disable_irq(tdev->irqn);

#if defined(CONFIG_OF)
    tcu_stop_counter(tdev->tcu->cib.id);
    tcu_disable_counter(tdev->tcu->cib.id);
#else
    jz_tcu_disable_counter(tdev->tcu); // 计数器停止计数
    jz_tcu_stop_counter(tdev->tcu);    // 停止向定时器提供时钟
#endif

    tdev->misc_dev.minor = MISC_DYNAMIC_MINOR;
    tdev->misc_dev.name = "timer";
    tdev->misc_dev.fops = &timer_fops;
    ret = misc_register(&tdev->misc_dev);
    if (ret < 0)
    {
        ret = -ENOENT;
        dev_err(&pdev->dev, "misc_register failed\n");
        goto error_misc_register;
    }

    return 0;

error_misc_register:
    free_irq(tdev->irqn, tdev);
error_request_irq:
error_get_irq:
error_get_cell:
    kfree(tdev);
error_devm_kzalloc:
error_ioremap:
    gpio_free(CON_GPIO);
error_gpio_request:
    return ret;
}

static int timer_remove(struct platform_device *pdev)
{
    struct timer_dev *tdev = platform_get_drvdata(pdev);
    gpio_free(CON_GPIO);
#if defined(CONFIG_OF)
    tcu_stop_counter(tdev->tcu->cib.id);
    tcu_disable_counter(tdev->tcu->cib.id);
#else
    jz_tcu_disable_counter(tdev->tcu); // 计数器停止计数
    jz_tcu_stop_counter(tdev->tcu);    // 停止向定时器提供时钟
#endif
    misc_deregister(&tdev->misc_dev);
    free_irq(tdev->irqn, tdev);
    kfree(tdev);

    return 0;
}

#ifdef CONFIG_OF
static struct of_device_id timer_match[] = {
    {
        .compatible = "ingenic,tcu_chn3",
    },
    {},
};
MODULE_DEVICE_TABLE(of, timer_match);
#else
#endif
static struct platform_driver timer_driver = {
    .probe = timer_probe,
    .remove = timer_remove,
    .driver = {
        .name = "tcu_chn3",
#ifdef CONFIG_OF
        .of_match_table = timer_match,
#endif
        .owner = THIS_MODULE,
    },
};

static int __init sample_timer_init(void)
{
    platform_driver_register(&timer_driver);

    return 0;
}

static void __exit sample_timer_exit(void)
{
    platform_driver_unregister(&timer_driver);
}

module_init(sample_timer_init);
module_exit(sample_timer_exit);

MODULE_LICENSE("GPL");
