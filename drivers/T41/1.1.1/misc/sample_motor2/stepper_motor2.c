#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/mfd/core.h>
#include <linux/mempolicy.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <jz_proc.h>
#include <soc/gpio.h>
#include "motor.h"
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
#include <linux/mfd/ingenic-tcu.h>
#include <dt-bindings/interrupt-controller/t40-irq.h>
#else
#include <linux/mfd/jz_tcu.h>
#endif

unsigned int motor_n = MOTOR_NUMBER;
unsigned int maxstep[MOTOR_NUMBER] = {MOTOR0_MAX_STEP, MOTOR1_MAX_STEP};
module_param_array(maxstep, uint, &motor_n, S_IRUGO);
MODULE_PARM_DESC(maxstep, "Max steps of motor\n");

unsigned int def_speed = MOTOR_DEF_SPEED;
module_param(def_speed, uint, S_IRUGO);
MODULE_PARM_DESC(def_speed, "Motor default speed\n");

#define TCU_TDFR0 0x40
#define TCU_REG_BASE 0x10002000
#define TCU_TMSTR 0x34
#define TCU_TMCLR 0x38
static void __iomem *tcu_base;

struct motor_dev
{
	struct platform_device *pdev;
	const struct mfd_cell *cell;
	struct device *dev;
	struct miscdevice misc_dev;
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	struct ingenic_tcu_chn *tcu;
#else
	struct jz_tcu_chn *tcu;
#endif
	int irqn;
	/* debug parameters */
	struct proc_dir_entry *proc;
};
struct motor_dev *mdev;

struct stMotorControl motor_cont = {
	.coordinate[0] = MOTOR0_MAX_STEP / 2,
	.target_coord[0] = MOTOR0_MAX_STEP / 2,
	.maxstep[0] = MOTOR0_MAX_STEP,
	.gear[0] = 1,
	.wire[0][0] = MOTOR0_WIRE_1,
	.wire[0][1] = MOTOR0_WIRE_2,
	.wire[0][2] = MOTOR0_WIRE_3,
	.wire[0][3] = MOTOR0_WIRE_4,
	.limit[0][0] = MOTOR0_LIMIT_MIN,
	.limit[0][1] = MOTOR0_LIMIT_MAX,
	.motor_status[0] = 1,
#if MOTOR_NUMBER > 1
	.coordinate[1] = MOTOR1_MAX_STEP / 2,
	.target_coord[1] = MOTOR1_MAX_STEP / 2,
	.maxstep[1] = MOTOR1_MAX_STEP,
	.gear[1] = 1,
	.wire[1][0] = MOTOR1_WIRE_1,
	.wire[1][1] = MOTOR1_WIRE_2,
	.wire[1][2] = MOTOR1_WIRE_3,
	.wire[1][3] = MOTOR1_WIRE_4,
	.limit[1][0] = MOTOR1_LIMIT_MIN,
	.limit[1][1] = MOTOR1_LIMIT_MAX,
	.motor_status[1] = 1,
#endif
};

struct SysStatus sys_status = {
	.irq_status = 0,
	.reset_status = RESET_NO,
};

struct stMotorStatus motor_status = {
	.coordinate[0] = MOTOR0_MAX_STEP / 2,
	.target_coord[0] = MOTOR0_MAX_STEP / 2,
	.gear[0] = 1,
#if MOTOR_NUMBER > 1
	.coordinate[1] = MOTOR1_MAX_STEP / 2,
	.target_coord[1] = MOTOR0_MAX_STEP / 2,
	.gear[1] = 1,
#endif
	.speed = MOTOR_DEF_SPEED,
	.reset_status = RESET_NO,
};

struct stTargetCoord target_coord;
struct stRotationSteps rotation_step;
struct stMotorSpeed motor_speed;

static int motor_open(struct inode *inode, struct file *file)
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
	jz_tcu_set_count(mdev->tcu, 0);
#endif
}

static int motor_release(struct inode *inode, struct file *file)
{
	return 0;
}
static int set_motor_speed(struct stMotorSpeed motor_speed)
{
	unsigned char n;
	if ((motor_speed.speed < MOTOR_MIN_SPEED) || (motor_speed.speed > MOTOR_MAX_SPEED))
	{
		printk("Speed:%d error\n", motor_speed.speed);
		return -1;
	}
	if (motor_status.speed != motor_speed.speed)
	{
		motor_status.speed = motor_speed.speed;
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
		set_irq_time(mdev->tcu->cib.id, 1000000 / motor_speed.speed);
#else
		set_irq_time(mdev->tcu->index, 1000000 / motor_speed.speed);
#endif
	}
	for (n = 0; n < MOTOR_NUMBER; ++n)
	{
		if (motor_speed.gear[n] < 1 || motor_speed.gear[n] > MOTOR_GEAR)
		{
			printk("Gear%d:%d error\n", n, motor_speed.gear[n]);
			continue;
		}
		if (motor_status.gear[n] != motor_speed.gear[n])
		{
			disable_timer_irq(motor_cont.tcu_n);
			motor_status.gear[n] = motor_cont.gear[n] = motor_speed.gear[n];
			if (sys_status.irq_status)
				enable_timer_irq(motor_cont.tcu_n);
		}
	}
	return 0;
}

const unsigned char excitation_tab[8] = {0x08, 0x0c, 0x04, 0x06, 0x02, 0x03, 0x01, 0x09};

void motor_step_one(unsigned int motor_num, enum enMotorDirection direction)
{
	static int n[MOTOR_NUMBER] = {0, 0};
	static unsigned char level[MOTOR_NUMBER] = {0, 0};
	if (motor_num >= MOTOR_NUMBER)
		return;
	if (direction == MOTOR_POSITIVE)
	{
		++n[motor_num];
		if (n[motor_num] > 7)
			n[motor_num] = 0;
	}
	else if (direction == MOTOR_NEGATIVE)
	{
		--n[motor_num];
		if (n[motor_num] < 0)
			n[motor_num] = 7;
	}
	else if (direction == MOTOR_STOP_MID)
	{
#if !MAGNETIC_LOCK
		if (level[motor_num])
		{
			CON_WIRE(motor_cont.wire[motor_num][0], STOP_GPIO_LEVEL);
			CON_WIRE(motor_cont.wire[motor_num][1], STOP_GPIO_LEVEL);
			CON_WIRE(motor_cont.wire[motor_num][2], STOP_GPIO_LEVEL);
			CON_WIRE(motor_cont.wire[motor_num][3], STOP_GPIO_LEVEL);
			level[motor_num] = 0;
		}
#endif
		return;
	}
	level[motor_num] = 1;
	CON_WIRE(motor_cont.wire[motor_num][0], 0x1 & excitation_tab[n[motor_num]]);
	CON_WIRE(motor_cont.wire[motor_num][1], 0x2 & excitation_tab[n[motor_num]]);
	CON_WIRE(motor_cont.wire[motor_num][2], 0x4 & excitation_tab[n[motor_num]]);
	CON_WIRE(motor_cont.wire[motor_num][3], 0x8 & excitation_tab[n[motor_num]]);
}
void set_all_wire(char leve)
{
	unsigned char i = 0, n = 0;
	for (i = 0; i < MOTOR_NUMBER; ++i)
	{
		for (n = 0; n < MOTOR_WIRE_NUM; ++n)
		{
			gpio_set_value(motor_cont.wire[i][n], leve);
		}
	}
}

static irqreturn_t jz_tcu_interrupt(int irq, void *dev_id)
{
	int n;
	static unsigned char gear[MOTOR_NUMBER] = {0, 0};
	if (sys_status.irq_status)
	{
		if (sys_status.reset_status == RESET_ING)
		{
			for (n = 0; n < MOTOR_NUMBER; ++n)
			{
				if (motor_cont.motor_status[n] && (gpio_get_value(motor_cont.limit[n][1]) != MOTOR_LIMIT_LEVEL))
				{
					motor_step_one(n, MOTOR_POSITIVE);
				}
				else if(motor_cont.motor_status[n])
				{
					motor_step_one(n, MOTOR_STOP_MID);
					motor_cont.motor_status[n] = 0;
				}
			}
			if (motor_cont.motor_status[0] + motor_cont.motor_status[1] == 0)
			{
				sys_status.reset_status = RESET_END;
			}
		}
		else if (sys_status.reset_status == RESET_END)
		{
			for (n = 0; n < MOTOR_NUMBER; ++n)
			{
				if (++gear[n] >= motor_cont.gear[n])
				{
					gear[n] = 0;
					if (motor_cont.target_coord[n] > motor_cont.coordinate[n])
					{
						if (gpio_get_value(motor_cont.limit[n][1]) != MOTOR_LIMIT_LEVEL)
						{
							motor_step_one(n, MOTOR_POSITIVE);
							if (motor_cont.coordinate[n] < motor_cont.maxstep[n]){
								++motor_cont.coordinate[n];
							}
						}
						else{
							motor_cont.target_coord[n] = motor_cont.coordinate[n] = motor_cont.maxstep[n];
						}
					}
					else if (motor_cont.target_coord[n] < motor_cont.coordinate[n])
					{
						if (gpio_get_value(motor_cont.limit[n][0]) != MOTOR_LIMIT_LEVEL)
						{
							motor_step_one(n, MOTOR_NEGATIVE);
							if (motor_cont.coordinate[n] > 0){
								--motor_cont.coordinate[n];
							}
						}
						else{
							motor_cont.target_coord[n] = motor_cont.coordinate[n] = 0;
						}
					}
					else if(motor_cont.motor_status[n])
					{
						motor_step_one(n, MOTOR_STOP_MID);
						motor_cont.motor_status[n]=0;
					}
				}
			}
			if (motor_cont.motor_status[0] + motor_cont.motor_status[1] == 0)
			{
#if !MAGNETIC_LOCK
				set_all_wire(STOP_GPIO_LEVEL);
#endif
				disable_timer_irq(motor_cont.tcu_n);
				sys_status.irq_status = 0;
			}
		}
	}
	return IRQ_HANDLED;
}

static long motor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned char n;
	switch (cmd)
	{
	case MOTOR_COORDINATE:
		if (copy_from_user(&target_coord, (void __user *)arg, sizeof(struct stTargetCoord)))
		{
			dev_err(mdev->dev, "[%s][%d] copy from user error\n", __func__, __LINE__);
			return -EFAULT;
		}
		disable_timer_irq(motor_cont.tcu_n);
		for (n = 0; n < MOTOR_NUMBER; ++n)
		{
#if 0
			if (target_coord.coordinate[n] > motor_cont.maxstep[n])
				target_coord.coordinate[n] = motor_cont.maxstep[n];
			else if (target_coord.coordinate[n] < 0)
				target_coord.coordinate[n] = 0;
#endif
			motor_cont.target_coord[n] = target_coord.coordinate[n];
			motor_cont.motor_status[n] = 1;
		}
		sys_status.irq_status = 1;
		enable_timer_irq(motor_cont.tcu_n);
		break;
	case MOTOR_STEPS:
		if (copy_from_user(&rotation_step, (void __user *)arg, sizeof(struct stRotationSteps)))
		{
			dev_err(mdev->dev, "[%s][%d] copy from user error\n", __func__, __LINE__);
			return -EFAULT;
		}
		disable_timer_irq(motor_cont.tcu_n);
		for (n = 0; n < MOTOR_NUMBER; ++n)
		{
			target_coord.coordinate[n] = rotation_step.step[n] + motor_cont.coordinate[n];
#if 0
			if (target_coord.coordinate[n] > motor_cont.maxstep[n])
				target_coord.coordinate[n] = motor_cont.maxstep[n];
			else if (target_coord.coordinate[n] < 0)
				target_coord.coordinate[n] = 0;
#endif
			motor_cont.target_coord[n] = target_coord.coordinate[n];
			motor_cont.motor_status[n] = 1;
		}
		sys_status.irq_status = 1;
		enable_timer_irq(motor_cont.tcu_n);
		break;
	case MOTOR_SET_SPEED:
		if (copy_from_user(&motor_speed, (void __user *)arg, sizeof(struct stMotorSpeed)))
		{
			dev_err(mdev->dev, "[%d]Copy from user error\n", __LINE__);
			return -EFAULT;
		}
		set_motor_speed(motor_speed);
		break;
	case MOTOR_STOP:
		disable_timer_irq(motor_cont.tcu_n);
		sys_status.irq_status = 0;
		if (sys_status.reset_status == RESET_ING)
		{
			sys_status.reset_status = RESET_NO;
		}
		else if (sys_status.reset_status == RESET_END)
		{
			for (n = 0; n < MOTOR_NUMBER; ++n)
			{
				motor_cont.target_coord[n] = motor_cont.coordinate[n];
			}
		}
#if !MAGNETIC_LOCK
		set_all_wire(STOP_GPIO_LEVEL);
#endif
		break;
	case MOTOR_GET_STATUS:
		disable_timer_irq(motor_cont.tcu_n);
		motor_status.reset_status = sys_status.reset_status;
		for (n = 0; n < MOTOR_NUMBER; ++n)
		{
			motor_status.coordinate[n] = motor_cont.coordinate[n];
			motor_status.target_coord[n] = motor_cont.target_coord[n];
		}
		if (sys_status.irq_status == 1)
			enable_timer_irq(motor_cont.tcu_n);
		if (copy_to_user((void __user *)arg, &motor_status, sizeof(struct stMotorStatus)))
		{
			dev_err(mdev->dev, "[%d]Copy to user error\n", __LINE__);
			return -EFAULT;
		}
		break;
	case MOTOR_RESET:
		sys_status.irq_status = 0;
		disable_timer_irq(motor_cont.tcu_n);
		motor_status.reset_status = sys_status.reset_status = RESET_ING;
		for (n = 0; n < MOTOR_NUMBER; ++n)
		{
			motor_cont.coordinate[n] = motor_cont.maxstep[n];
			motor_cont.motor_status[n] = 1;
		}
		sys_status.irq_status = 1;
		enable_timer_irq(motor_cont.tcu_n);
		break;
	case MOTOR_NO_RESET:
		if (copy_from_user(&target_coord, (void __user *)arg, sizeof(struct stTargetCoord)))
		{
			dev_err(mdev->dev, "[%s][%d] copy from user error\n", __func__, __LINE__);
			return -EFAULT;
		}
		if (sys_status.reset_status != RESET_ING)
		{
			sys_status.irq_status = 0;
			disable_timer_irq(motor_cont.tcu_n);
			for (n = 0; n < MOTOR_NUMBER; ++n)
			{
				if (target_coord.coordinate[n] > motor_cont.maxstep[n])
					target_coord.coordinate[n] = motor_cont.maxstep[n];
				else if (target_coord.coordinate[n] < 0)
					target_coord.coordinate[n] = 0;

				motor_cont.coordinate[n] = motor_cont.target_coord[n] = target_coord.coordinate[n];
			}
			sys_status.irq_status = 1;
			enable_timer_irq(motor_cont.tcu_n);
		}
		motor_status.reset_status = sys_status.reset_status = RESET_END;
		break;
	default:
		printk("Motor CMD error\n");
		break;
	}
	return 0;
}

static struct file_operations motor_fops = {
	.owner = THIS_MODULE,
	.open = motor_open,
	.release = motor_release,
	.unlocked_ioctl = motor_ioctl,
};

int motor_gpio_request(void)
{
	unsigned char i = 0, n = 0;
	unsigned char request_ok[MOTOR_NUMBER][MOTOR_WIRE_NUM] = {{0}, {0}};
	unsigned char request_limit_ok[MOTOR_NUMBER][2] = {{0}, {0}};
	for (i = 0; i < MOTOR_NUMBER; ++i)
	{
		for (n = 0; n < MOTOR_WIRE_NUM; ++n)
		{
			if (gpio_request(motor_cont.wire[i][n], "motor_wire") < 0)
			{
				printk("request gpio:%d fail !!!\n", motor_cont.wire[i][n]);
				goto error_gpio_request;
			}
			gpio_direction_output(motor_cont.wire[i][n], STOP_GPIO_LEVEL);
			request_ok[i][n] = 1;
		}
		for (n = 0; n < 2; ++n)
		{
			if (gpio_request(motor_cont.limit[i][n], "motor_limit") < 0)
			{
				printk("request gpio:%d fail !!!\n", motor_cont.wire[i][n]);
				goto error_gpio_request;
			}
			gpio_direction_input(motor_cont.limit[i][n]);
			request_limit_ok[i][n] = 1;
		}
	}
	return 0;
error_gpio_request:
	for (i = 0; i < MOTOR_NUMBER; ++i)
	{
		for (n = 0; n < MOTOR_WIRE_NUM; ++n)
		{
			if (request_ok[i][n])
			{
				gpio_free(motor_cont.wire[i][n]);
			}
		}
		for (n = 0; n < 2; ++n)
		{
			if (request_limit_ok[i][n])
			{
				gpio_free(motor_cont.limit[i][n]);
			}
		}
	}
	return -1;
}
static int motor_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	if (motor_gpio_request())
		return -ENOENT;
	tcu_base = (unsigned int *)ioremap(TCU_REG_BASE, 4 * 1024);
	if (tcu_base == NULL)
	{
		ret = -ENOENT;
		dev_err(&pdev->dev, "ioremap memery error\n");
		goto error_ioremap;
	}
	if ((def_speed < MOTOR_MIN_SPEED) || (def_speed > MOTOR_MAX_SPEED))
	{
		printk("Speed:%d error\n", def_speed);
		def_speed = MOTOR_DEF_SPEED;
	}
	motor_status.speed = def_speed;
	for (i = 0; i < MOTOR_NUMBER; ++i)
	{
		motor_cont.maxstep[i] = maxstep[i];
		motor_cont.coordinate[i] = maxstep[i] / 2;
		motor_cont.target_coord[i] = maxstep[i] / 2;
	}

	mdev = devm_kzalloc(&pdev->dev, sizeof(struct motor_dev), GFP_KERNEL);
	if (!mdev)
	{
		ret = -ENOENT;
		dev_err(&pdev->dev, "kzalloc pwm device memery error\n");
		goto error_devm_kzalloc;
	}

	mdev->cell = mfd_get_cell(pdev);
	if (!mdev->cell)
	{
		ret = -ENOENT;
		dev_err(&pdev->dev, "Failed to get mfd cell for jz_adc_aux!\n");
		goto error_get_cell;
	}

	mdev->dev = &pdev->dev;
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	mdev->tcu = (struct ingenic_tcu_chn *)mdev->cell->platform_data;
	motor_cont.tcu_n = mdev->tcu->cib.id;
#else
	mdev->tcu = (struct jz_tcu_chn *)mdev->cell->platform_data;
	motor_cont.tcu_n = mdev->tcu->index;
#endif
	printk("TCU%d\n", motor_cont.tcu_n);
	mdev->tcu->irq_type = FULL_IRQ_MODE;
	mdev->tcu->clk_src = TCU_CLKSRC_EXT;
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	mdev->tcu->is_pwm = 0;
	mdev->tcu->cib.func = TRACKBALL_FUNC;
	mdev->tcu->clk_div = TCU_PRESCALE_64;
	ingenic_tcu_config(mdev->tcu);
#else
	mdev->tcu->prescale = TCU_PRESCALE_64;
	jz_tcu_config_chn(mdev->tcu);
#endif
	set_irq_time(motor_cont.tcu_n, 1000000 / def_speed);
	platform_set_drvdata(pdev, mdev);
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	ingenic_tcu_channel_to_virq(mdev->tcu);
	mdev->irqn = mdev->tcu->virq[0];
#else
	mdev->irqn = platform_get_irq(pdev, 0);
#endif
	if (mdev->irqn < 0)
	{
		ret = mdev->irqn;
		dev_err(&pdev->dev, "Failed to get platform irq: %d\n", ret);
		goto error_get_irq;
	}

	ret = request_irq(mdev->irqn, jz_tcu_interrupt, 0, "jz_tcu_interrupt", mdev);
	if (ret)
	{
		dev_err(&pdev->dev, "Failed to run request_irq()!\n");
		goto error_request_irq;
	}
	mdev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	mdev->misc_dev.name = "motor";
	mdev->misc_dev.fops = &motor_fops;
	ret = misc_register(&mdev->misc_dev);
	if (ret < 0)
	{
		ret = -ENOENT;
		dev_err(&pdev->dev, "misc_register failed\n");
		goto error_misc_register;
	}

	/* debug info */
	mdev->proc = jz_proc_mkdir("motor");
	if (!mdev->proc)
	{
		mdev->proc = NULL;
		printk("create dev_attr_isp_info failed!\n");
	}
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	tcu_start_counter(motor_cont.tcu_n);
	disable_timer_irq(motor_cont.tcu_n);
	tcu_enable_counter(motor_cont.tcu_n);
#else
	jz_tcu_start_counter(mdev->tcu);
	disable_timer_irq(motor_cont.tcu_n);
	jz_tcu_enable_counter(mdev->tcu);
#endif
	return 0;

error_misc_register:
	free_irq(mdev->irqn, mdev);
error_request_irq:
error_get_irq:
error_get_cell:
	kfree(mdev);
error_devm_kzalloc:
error_ioremap:
	return ret;
}

static int motor_remove(struct platform_device *pdev)
{
	int i, n;
	struct motor_dev *mdev = platform_get_drvdata(pdev);
	disable_timer_irq(motor_cont.tcu_n);
	printk("%s,mdev=%p\n", __func__, mdev);
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	tcu_stop_counter(mdev->tcu->cib.id);
	tcu_disable_counter(mdev->tcu->cib.id);
#else
	jz_tcu_disable_counter(mdev->tcu);
	jz_tcu_stop_counter(mdev->tcu);
#endif
	free_irq(mdev->irqn, mdev);
	for (i = 0; i < MOTOR_NUMBER; ++i)
	{
		for (n = 0; n < MOTOR_WIRE_NUM; ++n)
		{
			gpio_direction_output(motor_cont.wire[i][n], STOP_GPIO_LEVEL);
			gpio_free(motor_cont.wire[i][n]);
		}
	}
	if (mdev->proc)
		proc_remove(mdev->proc);
	misc_deregister(&mdev->misc_dev);

	kfree(mdev);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id motor_match[] = {
	{ .compatible = "ingenic,tcu_chn3",	},
	{},
};
MODULE_DEVICE_TABLE(of, motor_match);
#else
#endif
static struct platform_driver motor_driver = {
	.probe = motor_probe,
	.remove = motor_remove,
	.driver = {
		.name = "tcu_chn2",
#ifdef CONFIG_OF
		.of_match_table = motor_match,
#endif
		.owner = THIS_MODULE,
	},
};

static int __init motor_init(void)
{
	return platform_driver_register(&motor_driver);
}

static void __exit motor_exit(void)
{
	platform_driver_unregister(&motor_driver);
}

module_init(motor_init);
module_exit(motor_exit);
MODULE_LICENSE("GPL");

