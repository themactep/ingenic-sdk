#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/platform_device.h>
#include <soc/gpio.h>
#include <soc/base.h>
#if defined(CONFIG_KERNEL_4_4_94)
#include <linux/mfd/ingenic-tcu.h>
#include <dt-bindings/interrupt-controller/PRJ007-irq.h>
#else
#include <linux/mfd/jz_tcu.h>
#endif
#include "motor.h"

#define MS32006_CONFIG 1 //MS32006_CONFIG/MS32007_CONFIG

// #define DEV_ADDR (0x20>>1)
#define DEV_ADDR (0x56) //(0x56>>1)

#define EXTAL_EN (0x1 << 2)  //EXT_EN
#define BYPASS (0x1 << 11)
#define CH_TCSR(n)		(0x4C + (n)*0x10)		/* Timer Control Reg */

struct i2c_client *client;

int jz_i2cdev_read(struct i2c_client *client,unsigned int dev_addr,unsigned int get_addr,unsigned char *data,int len)
{
	struct i2c_msg msgs[2];
	unsigned char writebufb[2];
	int msglen = 0,ret = 0;

	writebufb[0] = (unsigned char)get_addr;

	msgs[0].addr = dev_addr;
	msgs[0].flags = 0;
	msgs[0].len = len;
	msgs[0].buf = &writebufb[0];

	msgs[1].addr = dev_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	msglen = sizeof(msgs)/sizeof(struct i2c_msg);
	ret = i2c_transfer(client->adapter, msgs, msglen);
	if (ret < 0) {
		dev_err(&client->dev, "I2C transfer failed: %d\n", ret);
		return ret;
	}

	// printk("rd(%#hhx,%#hhx)\n",(unsigned char)get_addr,(unsigned char)data[0]);

	return ret;
}

int jz_i2cdev_write(struct i2c_client *client,unsigned int dev_addr,unsigned int set_addr,unsigned char *data,int len)
{
	struct i2c_msg msgs[2];
	unsigned char mapbuf[2];
	int msglen = 0,ret = 0;

	mapbuf[0] = (unsigned char)set_addr;

	msgs[0].addr = dev_addr;
	msgs[0].flags = 0;
	msgs[0].len = len;
	msgs[0].buf = &mapbuf[0];

	msgs[1].addr = dev_addr;
	msgs[1].flags = 0;//I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	msglen = sizeof(msgs)/sizeof(struct i2c_msg);
	ret = i2c_transfer(client->adapter, msgs, msglen);
	if (ret < 0) {
		dev_err(&client->dev, "I2C transfer failed: %d\n", ret);
		return ret;
	}

	// printk("wd(%#hhx,%#hhx)\n",(unsigned char)set_addr,(unsigned char)data[0]);

	return ret;
}


int i2cmotor_read(struct i2c_client *client,uint16_t reg,unsigned char *value)
{
	int ret;
	ret = jz_i2cdev_read(client,client->addr,reg, value,sizeof(char));
	return ret;
}

int i2cmotor_write(struct i2c_client *client,uint16_t reg,unsigned char value)
{
	int ret;
	//printk("reg:%#x,value:%#hhx\n",reg,value);
	ret = jz_i2cdev_write(client,client->addr,reg,&value,sizeof(char));

	return ret;
}

static int misc_open (struct inode *inode, struct file *filp)
{
	filp->private_data = client;
	return 0;
}

static int misc_release (struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

void i2cmotor_move(struct i2c_client *client,jz_motor_param_t *param)
{
	unsigned int microstep1,microstep2;
	unsigned int mot1_dir, mot1_pps,mot1_step, mot2_dir,mot2_pps, mot2_step;

	unsigned char readstate=0x00;

	mot1_dir = (unsigned int)param->move.mot1_dir;
	mot1_pps = param->move.mot1_pps;
	mot1_step = param->move.mot1_step;
	microstep1 = param->microstep1;

	mot2_dir = (unsigned int)param->move.mot2_dir;
	mot2_pps = param->move.mot2_pps;
	mot2_step = param->move.mot2_step;
	microstep2 = param->microstep2;


	i2cmotor_read(client, 0x02,&readstate);
	// printk("global_ctrl:%#x\n",readstate);
	readstate |= (0x01 << 1 | 0x1 << 3 | 0x1 << 7);
	i2cmotor_write(client,0x02,readstate);

	if(mot1_step>0)
	{
		i2cmotor_read(client, 0x20,&readstate);
		readstate &= ~(0x7);
		// readstate |= EXCITATION_1_2_MODE | (0x1 << 7);
		readstate |= RELATIVE_CONTROL_MODE;
		// readstate |= ABSOLUTE_POSITION_MODE | (0x1 << 7);
		i2cmotor_write(client,0x20,readstate);

		i2cmotor_read(client, 0x22,&readstate);

		readstate &= ~(0x7<<4);
		readstate |= microstep1;
		if(mot1_dir == MOTOR_CW)
		{
			readstate &= ~(0x7);
			readstate |= 1<<0;
			i2cmotor_write(client,0x22,readstate);
		}
		else if(mot1_dir == MOTOR_CCW)
		{
			readstate &= ~(0x7);
			readstate |= 1<<1;
			i2cmotor_write(client,0x22,readstate);
		}

		//clock div
		i2cmotor_write(client,0x23,0x03);

		//speed control div
		i2cmotor_write(client,0x24,0x03);
		i2cmotor_write(client,0x25,0x03);

		//speed count
		i2cmotor_write(client,0x26,0xff);
		i2cmotor_write(client,0x27,0x00);

		//circle count
		i2cmotor_write(client,0x2e, (mot1_step & 0xff));//l
		i2cmotor_write(client,0x2f, (mot1_step >> 8));//h

		i2cmotor_read(client, 0x20,&readstate);
		readstate |= (0x1 << 7);
		i2cmotor_write(client,0x20,readstate);

	}

	if(mot2_step>0){
		i2cmotor_read(client, 0x40,&readstate);
		readstate &= ~(0x7);
		// readstate |= EXCITATION_1_2_MODE | (0x1 << 7);
		readstate |= RELATIVE_CONTROL_MODE;
		// readstate |= ABSOLUTE_POSITION_MODE | (0x1 << 7);
		i2cmotor_write(client,0x40,readstate);

		i2cmotor_read(client, 0x42,&readstate);
		readstate &= ~(0x7<<4);
		readstate |= microstep1;
		if(mot2_dir == MOTOR_CW)
		{
			readstate &= ~(0x7);
			readstate |= 1<<0;
			i2cmotor_write(client,0x42,readstate);
		}
		else if(mot2_dir == MOTOR_CCW)
		{
			readstate &= ~(0x7);
			readstate |= 1<<1;
			i2cmotor_write(client,0x42,readstate);
		}

		//clock div
		i2cmotor_write(client,0x43,0x03);
		// i2cmotor_write(client,0x23,0xa0);

		//speed count
		i2cmotor_write(client,0x44,0x03);
		i2cmotor_write(client,0x45,0x03);

		//speed count
		i2cmotor_write(client,0x46,0xff);
		i2cmotor_write(client,0x47,0x00);

		//circle count
		i2cmotor_write(client,0x4e, (mot2_step & 0xff));//l
		i2cmotor_write(client,0x4f, (mot2_step >> 8));//h

		i2cmotor_read(client, 0x40,&readstate);
		readstate |= (0x1 << 7);
		i2cmotor_write(client,0x40,readstate);
	}


	return ;
}

void i2cmotor_stop(struct i2c_client *client,jz_motor_param_t *param)
{
	unsigned char readstate=0x00;

	int ret,set_mot;
	readstate = 0x00;
	ret = i2cmotor_read(client, 0x02,&readstate);
	if(ret < 0) {
		pr_err("%s read err\n",__func__);
		return ;
	}

	set_mot = param->motor;

	if(set_mot == MOTOR_1)
	{
		i2cmotor_read(client, 0x20,&readstate);
		i2cmotor_write(client, 0x20, readstate & (~(0x1<<7)));
	} else if(set_mot == MOTOR_2) {
		i2cmotor_read(client, 0x40,&readstate);
		i2cmotor_write(client, 0x40, readstate & (~(0x1<<7)));
	}

	return ;
}

void i2cmotor_pause(struct i2c_client *client,jz_motor_param_t *param)
{
	int ret,set_mot;
	unsigned char readstate=0x00;

	set_mot = param->motor;
	//printk("motor:%d start to pause\n",set_mot);

	if(set_mot==MOTOR_1)
	{
		ret = i2cmotor_read(client, 0x20,&readstate);
		readstate &= ~(0x3 << 4);
		readstate |= 0x03;
		i2cmotor_write(client, 0x20, readstate);
	}
	else if(set_mot==MOTOR_2)
	{
		ret = i2cmotor_read(client, 0x40,&readstate);
		readstate &= ~(0x3 << 4);
		readstate |= 0x03;
		i2cmotor_write(client, 0x40, readstate);
	}

	return ;
}

void i2cmotor_ircut(struct i2c_client *client,jz_motor_param_t *param)
{
	unsigned char setdata=0x00;
	unsigned char readstate=0x00;

	i2cmotor_read(client, 0x10,&readstate);
	// readstate &= ~(0x3 << 2); //clr bit2 bit3
	readstate |= (0x1 << 7);

	switch(param->irctype){
		case MOTOR_IRCUT_CW:
			setdata = readstate | 0X05; //BIT2=1 BIT3=0
			break;
		case MOTOR_IRCUT_CCW:
			setdata = readstate | 0X0a; //BIT2=0 BIT3=1
			break;
		case MOTOR_IRCUT_CLOSE:
			setdata = readstate & ~(0x1 << 7); //BIT2=0 BIT3=0 High resistance state
			break;
		default:
			printk("[%s] not support yet!\n",__func__);
			return ;
	}

	i2cmotor_write(client, 0x10, setdata);

	msleep(20);
	i2cmotor_write(client, 0x10, setdata & ~(0x1 << 7));
	//printk("ret%d,readstat:%#hhx\n",ret,setdata);
	return ;
}


void i2cmotor_reset(struct i2c_client *client,jz_motor_param_t *param)
{
	unsigned char readstate=0x00;

	i2cmotor_read(client, 0x02,&readstate);
	readstate &= ~(1<<7);	//reset chip

	i2cmotor_write(client,0x02,readstate);
	return;
}

void i2cmotor_dbg(struct i2c_client *client,jz_motor_param_t *param)
{

	unsigned char sta0 = 0,sta1 = 0,sta2 = 0,sta3 = 0,sta4 = 0,sta5 = 0,sta6 = 0,sta7 = 0,sta8 = 0,sta9 = 0;
	// unsigned char bsta = 0,csta = 0,dsta = 0,esta = 0;

	i2cmotor_read(client, 0x0,&sta0);
	i2cmotor_read(client, 0x1,&sta1);
	i2cmotor_read(client, 0x2,&sta2);
	i2cmotor_read(client, 0x3,&sta3);
	i2cmotor_read(client, 0x4,&sta4);
	printk(" 0H=%#hhx\n 1H=%#hhx\n 2H=%#hhx\n 3H=%#hhx\n 4H=%hhx\n",sta0,sta1,sta2,sta3,sta4);

	printk("*********motor1 param dump******\n");

	i2cmotor_read(client, 0x20,&sta5);
	i2cmotor_read(client, 0x21,&sta6);
	i2cmotor_read(client, 0x22,&sta7);
	i2cmotor_read(client, 0x23,&sta8);
	i2cmotor_read(client, 0x24,&sta9);
	printk(" 20H=%#hhx\n 21H=%#hhx\n 22H=%#hhx\n 23H=%#hhx\n 24H=%hhx\n",sta5,sta6,sta7,sta8,sta9);

	i2cmotor_read(client, 0x26,&sta5);
	i2cmotor_read(client, 0x27,&sta6);
	i2cmotor_read(client, 0x28,&sta7);
	i2cmotor_read(client, 0x29,&sta8);
	printk(" 26H=%#hhx\n 27H=%#hhx\n 28H=%#hhx\n 29H=%#hhx \n",sta5,sta6,sta7,sta8);

	i2cmotor_read(client, 0x2a,&sta5);
	i2cmotor_read(client, 0x2b,&sta6);
	i2cmotor_read(client, 0x2c,&sta7);
	i2cmotor_read(client, 0x2d,&sta8);
	printk(" 2AH=%#hhx\n 2BH=%#hhx\n 2CH=%#hhx\n 2DH=%#hhx \n",sta5,sta6,sta7,sta8);

	i2cmotor_read(client, 0x2e,&sta5);
	i2cmotor_read(client, 0x2f,&sta6);
	i2cmotor_read(client, 0x30,&sta7);
	i2cmotor_read(client, 0x31,&sta8);
	printk(" 2EH=%#hhx\n 2FH=%#hhx\n 30H=%#hhx\n 31H=%#hhx \n",sta5,sta6,sta7,sta8);

	printk("*********motor2 param dump******\n");

	i2cmotor_read(client, 0x40,&sta5);
	i2cmotor_read(client, 0x41,&sta6);
	i2cmotor_read(client, 0x42,&sta7);
	i2cmotor_read(client, 0x43,&sta8);
	i2cmotor_read(client, 0x44,&sta9);
	printk(" 40H=%#hhx\n 41H=%#hhx\n 42H=%#hhx\n 43H=%#hhx\n 44H=%hhx\n",sta5,sta6,sta7,sta8,sta9);

	i2cmotor_read(client, 0x46,&sta5);
	i2cmotor_read(client, 0x47,&sta6);
	i2cmotor_read(client, 0x48,&sta7);
	i2cmotor_read(client, 0x49,&sta8);
	printk(" 46H=%#hhx\n 47H=%#hhx\n 48H=%#hhx\n 49H=%#hhx \n",sta5,sta6,sta7,sta8);

	i2cmotor_read(client, 0x4a,&sta5);
	i2cmotor_read(client, 0x4b,&sta6);
	i2cmotor_read(client, 0x4c,&sta7);
	i2cmotor_read(client, 0x4d,&sta8);
	printk(" 4AH=%#hhx\n 4BH=%#hhx\n 4CH=%#hhx\n 4DH=%#hhx \n",sta5,sta6,sta7,sta8);

	i2cmotor_read(client, 0x4e,&sta5);
	i2cmotor_read(client, 0x4f,&sta6);
	i2cmotor_read(client, 0x50,&sta7);
	i2cmotor_read(client, 0x51,&sta8);
	printk(" 4EH=%#hhx\n 4FH=%#hhx\n 50H=%#hhx\n 51H=%#hhx \n",sta5,sta6,sta7,sta8);

	return;
}

static int i2cmotor_param_check(jz_motor_param_t *param)
{
	if(param->motor < MOTOR_1 || param->motor > MOTOR_2){
		printk("motor out range,current only supprot motor 1 and 2\n");
		return -ENODEV;
	}

	// if(param->type < MOTOR_HALF || param->type >MOTOR_FULL){
	// 	printk("motor type out range\n");
	// 	return -ENODEV;
	// }

	// if(param->mts < MOTOR_2P_4W || param->mts > MOTOR_4P_5W){
	// 	printk("motor mts out range\n");
	// 	return -ENODEV;
	// }

	if(param->move.mot1_dir != MOTOR_CW && param->move.mot1_dir != MOTOR_CCW){
		printk("motor mot1_dir out range\n");
		return -ENODEV;
	}

	if(param->move.mot2_dir != MOTOR_CW && param->move.mot2_dir != MOTOR_CCW){
		printk("motor mot2_dir out range\n");
		return -ENODEV;
	}

	return 0;
}

static long misc_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{

	struct i2c_client *client = NULL;
	jz_motor_param_t motorparam = {0};

	client = filp->private_data;
	if(!client){
		pr_err("%s failed to get I2C motor param，should open /dev/i2c_motor\n",__func__);
		return -ENOTTY;
	}

	if (copy_from_user(&motorparam, (void __user *)arg, sizeof(motorparam)) != 0) {
		return -EFAULT;
	}

	//printk("addr:%#x,cmd:%#x,mot1stp:%d\n",client->addr,cmd,motorparam.move.mot1_step);
	if(i2cmotor_param_check(&motorparam) < 0){
		pr_err("%s failed to get I2C motor param，should open /dev/i2c_motor\n",__func__);
		return -EFAULT;
	}

	switch(cmd){
		case I2C_MOTOR_STOP:
			i2cmotor_stop(client,&motorparam);
			break;
		case I2C_MOTOR_RUN:
			i2cmotor_move(client,&motorparam);
			break;
		case I2C_MOTOR_PAUSE:
			i2cmotor_pause(client,&motorparam);
			break;
		case I2C_MOTOR_RESET:
			i2cmotor_reset(client,&motorparam);
			break;
		case I2C_MOTOR_IRCUT:
			i2cmotor_ircut(client,&motorparam);
			break;
		case I2C_MOTOR_DEBUG:
			i2cmotor_dbg(client,&motorparam);
			break;
		default:
			printk("%s,not support this choose\n",__func__);
			break;
	}

	return 0;
}

static const struct file_operations i2cmotor_misc_fops = {
	.owner = THIS_MODULE,
	.open = misc_open,
	.release = misc_release,
	.unlocked_ioctl = misc_ioctl,
};

static struct miscdevice i2cmotor_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "i2c_motor",
	.fops = &i2cmotor_misc_fops,
};

static void i2cmotor_set_gpio_pwm(void *base,int pwm_chn)
{
	int value = 0;

	//Crystal oscillator across pwm2 use gpio pb27
	enum gpio_port port = GPIO_PORT_B;
	enum gpio_function func = GPIO_FUNC_1;
	unsigned int pins = 0x1 << 27;

	switch (pwm_chn){
		case TCU_CHN0:
		{
			port = GPIO_PORT_A;
			func = GPIO_FUNC_2;
			pins = 0x1 << 14;
			break;
		}
		case TCU_CHN1:
		{
			port = GPIO_PORT_B;
			func = GPIO_FUNC_1;
			pins = 0x1 << 18;
			break;
		}
		case TCU_CHN2:
		{
			port = GPIO_PORT_B;
			func = GPIO_FUNC_1;
			pins = 0x1 << 27;
			break;
		}
		case TCU_CHN3:
		{
			port = GPIO_PORT_B;
			func = GPIO_FUNC_1;
			pins = 0x1 << 28;
			break;
		}
		default:
			printk("error,%s not support this chn\n",__func__);
			break;
	}

	jzgpio_set_func(port,func,pins);

	value = readl(base+CH_TCSR(pwm_chn));
	value |= EXTAL_EN | BYPASS;
	writel(value, base+CH_TCSR(pwm_chn));

	return ;
}

static int i2cmotor_device_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	misc_register(&i2cmotor_misc_device);

	return 0;
}

static int i2cmotor_my_device_remove(struct i2c_client *client)
{
	misc_deregister(&i2cmotor_misc_device);

	return 0;
}

static int tcu_probe(struct platform_device *pdev)
{
	void *base;
	struct mfd_cell *cell;
#if defined(CONFIG_KERNEL_4_4_94)
	struct ingenic_tcu_chn *tcu_chn;
#else
	struct jz_tcu_chn *tcu_chn = NULL;
#endif

	cell = (struct mfd_cell *)mfd_get_cell(pdev);
	if(!cell){
		dev_err(&pdev->dev, "No cell resource\n");
		return -ENXIO;
	}
#if defined(CONFIG_KERNEL_4_4_94)
	tcu_chn = (struct ingenic_tcu_chn *)cell->platform_data;
	if(!tcu_chn){
		dev_err(&pdev->dev, "No cell->platform_data\n");
		return -ENXIO;
	}

	base = tcu_chn->parent_iobase;
	if(!base){
		dev_err(&pdev->dev, "tcu->iomem is null,need run tcu\n");
		return -ENODEV;
	}
#else
	tcu_chn = (struct jz_tcu_chn *)cell->platform_data;
	if(!tcu_chn){
		dev_err(&pdev->dev, "No cell->platform_data\n");
		return -ENXIO;
	}

	base = tcu_chn->tcu->iomem;
	if(!base){
		dev_err(&pdev->dev, "tcu->iomem is null,need run tcu\n");
		return -ENODEV;
	}
#endif

	i2cmotor_set_gpio_pwm(base,(int)PWM_CHN);

	return 0;
}

static int tcu_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct i2c_device_id i2cmotor_device_id[] = {
	{ "i2cmotor_device", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2cmotor_device_id);

static struct i2c_driver i2cmotor_driver = {
	.probe = i2cmotor_device_probe,
	.remove = i2cmotor_my_device_remove,
	.id_table = i2cmotor_device_id,
	.driver = {
		.name = "i2cmotor_driver",
		.owner = THIS_MODULE,
	},
};


struct platform_driver tcu_driver = {
	.probe	= tcu_probe,
	.remove = tcu_remove,
	.driver = {
#ifdef CONFIG_KERNEL_4_4_94
		.name	= "ingenic,tcu_chn2",
#else
		.name	= "tcu_chn2",
#endif
		.owner	= THIS_MODULE,
	},
};

static int tcu_get(void)
{
	int ret = 0;

	printk("register tcu driver\n");
	ret = platform_driver_register(&tcu_driver);
	if(ret){
		printk("fail to register tcu_drv\n");
		return ret;
	}

	return ret;
}

static void tcu_put(void)
{
	platform_driver_unregister(&tcu_driver);
	return ;
}

static int __init i2cmotor_init(void)
{
	struct i2c_adapter *adapter;
	struct i2c_board_info board_info = {};

	// tcu_get();

	adapter = i2c_get_adapter(I2CID);
	if (!adapter) {
		pr_err("%s failed to get I2C adapter\n",__func__);
		return -ENODEV;
	}

	strlcpy(board_info.type, "i2cmotor_device", sizeof(board_info.type));
	board_info.addr = DEV_ADDR;

	client = i2c_new_device(adapter, &board_info);
	if (!client) {
		pr_err("%s,failed to register I2C device\n",__func__);
		return -ENODEV;
	}

	i2c_put_adapter(adapter);

	return i2c_add_driver(&i2cmotor_driver);
}

static void __exit i2cmotor_exit(void)
{
	// tcu_put();

	i2c_del_driver(&i2cmotor_driver);
	if(client){
		i2c_unregister_device(client);
		client = NULL;
	}
}

module_init(i2cmotor_init);
module_exit(i2cmotor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jim.jwang@ingenic.com");
