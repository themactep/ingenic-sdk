#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <soc/gpio.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/fs.h>

struct gpio_des {
	uint32_t pin;
	uint32_t irq_mode;
};

#define REQUEST_GPIO		_IOW('G',1, struct gpio_des)
#define FREE_GPIO		_IOW('G',2, struct gpio_des)
#define GPIO_PULL_UP		_IOW('G',3, struct gpio_des)
#define GPIO_PULL_DOWN		_IOW('G',4, struct gpio_des)
#define GPIO_REQUEST_IRQ	_IOW('G',5, struct gpio_des)
#define GPIO_FREE_IRQ		_IOW('G',6, struct gpio_des)

#define TRIGGER_RISING		1
#define TRIGGER_FALLING		2
#define TRIGGER_RISING_FALLING	3

int irq_num;
static irqreturn_t gpio_irq_func(int irqno, void *arg)
{
	irq_num++;
	return IRQ_WAKE_THREAD;
}
static irqreturn_t gpio_irq_func_thread(int irqno, void *arg)
{
	printk(" irq_num = %d, IRQ OK\n",irq_num);
	msleep(1);

	return IRQ_HANDLED;
}

static int my_open(struct inode *inode, struct file *filp)
{
	irq_num = 0;
	return 0;
}

static int my_release(struct inode *inode, struct file *filp)
{
	irq_num = 0;
	return 0;
}

static long my_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	char name[16] = {0};
	struct gpio_des test_gpio;

	if (copy_from_user(&test_gpio, (void *)arg, sizeof(struct gpio_des))) {
		printk("copy_from_user error!!!\n");
		ret = -EFAULT;
		return ret;
	}

	switch(cmd) {
	case REQUEST_GPIO:
		sprintf(name,"%s_%d","gpio_pin_",test_gpio.pin);
		if(gpio_is_valid(test_gpio.pin)) {
			ret = gpio_request(test_gpio.pin, name);
			if(ret) {
				printk("request gpio error!!!\n");
				return ret;
			}
		} else {
			printk("error: gpio%d invalid\n",test_gpio.pin);
		}
		break;
	case FREE_GPIO:
		gpio_free(test_gpio.pin);
		break;
	case GPIO_PULL_UP:
		gpio_direction_output(test_gpio.pin, 1);
		break;
	case GPIO_PULL_DOWN:
		gpio_direction_output(test_gpio.pin, 0);
		break;
	case GPIO_REQUEST_IRQ:
		sprintf(name,"%s_%d","gpio_irq",test_gpio.pin);
		gpio_direction_input(test_gpio.pin);
		if(test_gpio.irq_mode == TRIGGER_RISING)
			ret = request_threaded_irq(gpio_to_irq(test_gpio.pin), gpio_irq_func, gpio_irq_func_thread,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT, name , NULL);
		else if(test_gpio.irq_mode == TRIGGER_FALLING)
			ret = request_threaded_irq(gpio_to_irq(test_gpio.pin), gpio_irq_func, gpio_irq_func_thread,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT, name , NULL);
		else if(test_gpio.irq_mode == TRIGGER_RISING_FALLING)
			ret = request_threaded_irq(gpio_to_irq(test_gpio.pin), gpio_irq_func,
					gpio_irq_func_thread, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, name , NULL);
		if(ret)
			printk("request_irq error\n");
		break;
	case GPIO_FREE_IRQ:
		free_irq(gpio_to_irq(test_gpio.pin), NULL);
		break;
	default:
		printk("invalid cmd!!!\n");
		break;
	}

	return ret;
}
struct file_operations fops = {
	.open 		= my_open,
	.release 	= my_release,
	.unlocked_ioctl = my_ioctl,

};
struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "jz_gpio",
	.fops = &fops,

};
static int __init test_init(void)
{
	int ret;

	ret = misc_register(&mdev);
	return ret;
}

static void __exit test_exit(void)
{
	misc_deregister(&mdev);
}

module_init(test_init);
module_exit(test_exit);

MODULE_DESCRIPTION("JZ IRQ_GPIO driver");
MODULE_AUTHOR("Danny <yiwen.han@ingenic.com>");
MODULE_LICENSE("GPL");
