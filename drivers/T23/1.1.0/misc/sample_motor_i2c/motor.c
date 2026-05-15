#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/mfd/jz_tcu.h>
#include <linux/mfd/core.h>
#include <linux/platform_device.h>
#include <soc/gpio.h>
#include <soc/base.h>
#include "motor.h"

#define DEV_ADDR (0x20>>1)	//ms32006地址,因为内部i2c做处理了，所以需要将地址右移一位

#define EXTAL_EN (0x1 << 2)  //EXT_EN
#define BYPASS (0x1 << 11)
#define CH_TCSR(n)		(0x4C + (n)*0x10)		/* Timer Control Reg */

struct i2c_client *client;

int jz_i2cdev_read(struct i2c_client *client,unsigned int dev_addr,unsigned int get_addr,unsigned char *data,int len)
{
	struct i2c_msg msgs[2];
	unsigned char writebufb[2];
	int msglen = 0,ret = 0;

	writebufb[0] = (unsigned char)get_addr;//0地址

	//先写map地址，再写data数据
	msgs[0].addr = dev_addr;
	msgs[0].flags = 0;
	msgs[0].len = len;
	msgs[0].buf = &writebufb[0];

	//读取len个字节的数据
	msgs[1].addr = dev_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	msglen = sizeof(msgs)/sizeof(struct i2c_msg);
	/* 向设备发送消息 */
	ret = i2c_transfer(client->adapter, msgs, msglen);
	if (ret < 0) {
		dev_err(&client->dev, "I2C transfer failed: %d\n", ret);
		return ret;
	}

	//printk("rd(%#hhx,%#hhx)\n",(unsigned char)get_addr,(unsigned char)data[0]);

	return ret;
}

int jz_i2cdev_write(struct i2c_client *client,unsigned int dev_addr,unsigned int set_addr,unsigned char *data,int len)
{
	struct i2c_msg msgs[2];
	unsigned char mapbuf[2];
	int msglen = 0,ret = 0;

	mapbuf[0] = (unsigned char)set_addr;//0地址

	//先写map地址，再写data数据
	msgs[0].addr = dev_addr;
	msgs[0].flags = 0;
	msgs[0].len = len;
	msgs[0].buf = &mapbuf[0];

	//写len个字节的数据
	msgs[1].addr = dev_addr;
	msgs[1].flags = 0;//I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	msglen = sizeof(msgs)/sizeof(struct i2c_msg);
	/* 向设备发送消息 */
	ret = i2c_transfer(client->adapter, msgs, msglen);
	if (ret < 0) {
		dev_err(&client->dev, "I2C transfer failed: %d\n", ret);
		return ret;
	}

	//printk("wd(%#hhx,%#hhx)\n",(unsigned char)set_addr,(unsigned char)data[0]);

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
	//一个字节一个字节的写
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
	unsigned int mot1_dir, mot1_pps,mot1_step, mot2_dir,mot2_pps, mot2_step;

	unsigned int mot_set_speed_data = 0x00;	  //设置运动速度
	unsigned char mot_set_speed_l=0x00; 	   //速度低8位
	unsigned char mot_set_speed_h=0x00; 	   //速度高6位

	unsigned int mot_set_step_data =0x00;	   //设置运动步数
	unsigned char mot_set_step_l=0x00;		   //设置低8位
	unsigned char mot_set_step_h=0x00;		   //设置高4位。
	unsigned char readstate=0x00;

	unsigned int mot_run_mode = MOTOR_HALF;

	mot1_dir = (unsigned int)param->move.mot1_dir;
	mot1_pps = param->move.mot1_pps;
	mot1_step = param->move.mot1_step;

	mot2_dir = (unsigned int)param->move.mot2_dir;
	mot2_pps = param->move.mot2_pps;
	mot2_step = param->move.mot2_step;

	mot_run_mode = param->type;


	i2cmotor_read(client, 0x00,&readstate); //读取地址0数据
	//readstate &= (0<<0);	//复位芯片,若想复位，调用I2C_MOTOR_RESET

	//几项几线电机
	if(param->mts == MOTOR_2P_4W){
		//2项4线
		readstate &= 0x0 << 7;

	}else {
		//4项5线
		readstate |=1 << 7;
	}
	readstate |=0x01; //芯片正常工作

	i2cmotor_write(client,0x00,readstate);
	if(mot1_step>0){
		mot_set_speed_data=ms32006_fclk/mot1_pps/128;              //设置速度 xx pps  128是固定参数。
		if(mot_set_speed_data<32){mot_set_speed_data=32;}         //设置速度最小值是32
		if(mot_set_speed_data>16383){mot_set_speed_data=16383;}   //设置速度最大值是16383

		mot_set_speed_l = mot_set_speed_data&0x00ff; //取低8位
		mot_set_speed_h = mot_set_speed_data/0x100; //高6位

		if(mot_run_mode==MOTOR_HALF) {
			mot_set_speed_h |=0x80; //第8位置1
		} else if(mot_run_mode==MOTOR_FULL) {
			mot_set_speed_h &=0x7f; //第8位置0
		}else {
			mot_set_speed_h |=0x80; //第8位置1
		}

		mot_set_step_data=mot1_step;
		mot_set_step_l = mot_set_step_data&0x00ff;    //取低8位
		mot_set_step_h = mot_set_step_data/0x100;     //高4位
		mot_set_step_h |=0x80; //第8位置1，打开输出使能


		if(mot1_dir == MOTOR_CW)
		{
			mot_set_step_h &=0xBF; //第7位置0，CW
		}
		else if(mot1_dir == MOTOR_CCW)
		{
			mot_set_step_h |=0x40; //第7位置1，CCW
		}
		else
		{
			mot_set_step_h &=0xBF; //第7位置0，CW
		}

		i2cmotor_write(client,0x01,mot_set_speed_l);
		i2cmotor_write(client,0x02,mot_set_speed_h);
		i2cmotor_write(client,0x03,mot_set_step_l);
		i2cmotor_write(client,0x04,mot_set_step_h);

	}

	if(mot2_step>0){
		mot_set_speed_data=ms32006_fclk/mot2_pps/128;              //设置速度 xx pps  128是固定参数。
		if(mot_set_speed_data<32){mot_set_speed_data=32;}         //设置速度最小值是32
		if(mot_set_speed_data>16383){mot_set_speed_data=16383;}   //设置速度最大值是16383

		mot_set_speed_l = mot_set_speed_data&0x00ff; //取低8位
		mot_set_speed_h = mot_set_speed_data/0x100; //高6位

		if(mot_run_mode==MOTOR_HALF) {
			mot_set_speed_h |=0x80; //第8位置1
		} else if(mot_run_mode==MOTOR_FULL) {
			mot_set_speed_h &=0x7f; //第8位置0
		}else {
			mot_set_speed_h |=0x80; //第8位置1
		}

		mot_set_step_data=mot2_step;
		mot_set_step_l = mot_set_step_data&0x00ff;    //取低8位
		mot_set_step_h = mot_set_step_data/0x100;     //高4位
		mot_set_step_h |=0x80; //第8位置1，打开输出使能


		if(mot2_dir == MOTOR_CW)
		{
			mot_set_step_h &=0xBF; //第7位置0，CW
		}
		else if(mot2_dir == MOTOR_CCW)
		{
			mot_set_step_h |=0x40; //第7位置1，CCW
		}
		else
		{
			mot_set_step_h &=0xBF; //第7位置0，CW
		}

		i2cmotor_write(client,0x05,mot_set_speed_l);
		i2cmotor_write(client,0x06,mot_set_speed_h);
		i2cmotor_write(client,0x07,mot_set_step_l);
		i2cmotor_write(client,0x08,mot_set_step_h);

	}

	 //下边是先读 9H 的数据，然后再根据情况使能 A或者B电机。
	readstate=0x00;
	i2cmotor_read(client, 0x09,&readstate); //读取地址9H数据
	if((mot1_step>0)&&(mot2_step>0))
	{
		readstate |=0xC0; //A B电机同时使能
	}
	else if(mot1_step>0)
	{
		readstate |=0x80; //A电机使能
	}
	else if(mot2_step>0)
	{
		readstate |=0x40; //B电机使能
	}

	i2cmotor_write(client, 0x09, readstate);  //写9H地址

	return ;
}

void i2cmotor_stop(struct i2c_client *client,jz_motor_param_t *param) //fixme:wj after t32 fix
{
	unsigned int set_data=0x00;
	unsigned char readstate=0x00;

	int ret,set_mot;
	readstate=0x00;
	ret = i2cmotor_read(client, 0x09,&readstate); //读取地址9H数据
	if(ret < 0) {
		pr_err("%s read err\n",__func__);
		return ;
	}

	set_mot = param->motor;

	if(set_mot == MOTOR_1) {
	  i2cmotor_write(client, 0x04, set_data); //写4H地址数据

	  readstate |= 0x80; //A电机使能
	  i2cmotor_write(client, 0x09, readstate);  //写9H地址
	}
	else if(set_mot == MOTOR_2) {
	  i2cmotor_write(client, 0x08, set_data); //写8H地址数据

	  readstate |= 0x40; //B电机使能
	  i2cmotor_write(client, 0x09, readstate);  //写9H地址
	}

	return ;
}

void i2cmotor_pause(struct i2c_client *client,jz_motor_param_t *param)
{
	int ret,set_mot;
	unsigned char readstate=0x00;

	set_mot = param->motor;
	//printk("motor:%d start to pause\n",set_mot);

   	ret = i2cmotor_read(client, 0x00,&readstate); //读取地址0数据
	if(set_mot==MOTOR_1)
	{
		readstate |=0x04; //第2位置1
		i2cmotor_write(client, 0x00, readstate); //写0地址数据
	}
	else if(set_mot==MOTOR_2)
	{
		readstate |=0x02; //第1位置1
		i2cmotor_write(client, 0x00, readstate); //写0地址数据
	}

	return ;
}

void i2cmotor_ircut(struct i2c_client *client,jz_motor_param_t *param)
{
	unsigned char setdata=0x00;
	unsigned char readstate=0x00;
	int ret;

	i2cmotor_read(client, 0x09,&readstate);
	readstate &= ~(0x3 << 2); //clr bit2 bit3

	switch(param->irctype){
		case MOTOR_IRCUT_CW:
			setdata = readstate | 0X04;	//BIT2=1 BIT3=0
			break;
		case MOTOR_IRCUT_CCW:
			setdata = readstate | 0X08;	//BIT2=0 BIT3=1
			break;
		case MOTOR_IRCUT_CLOSE:
			setdata =readstate;	//BIT2=0 BIT3=0	高阻态
			break;
		default:
			printk("[%s] not support yet!\n",__func__);
			return ;
	}

	i2cmotor_write(client, 0x09, setdata);

	//printk("ret%d,readstat:%#hhx\n",ret,setdata);
	return ;
}


void i2cmotor_reset(struct i2c_client *client,jz_motor_param_t *param)
{
	unsigned char readstate=0x00;

	i2cmotor_read(client, 0x00,&readstate); //读取地址0数据
	readstate &= (0<<0);	//复位芯片

	i2cmotor_write(client,0x00,readstate);
	return;
}

void i2cmotor_dbg(struct i2c_client *client,jz_motor_param_t *param)
{
	unsigned char sta0 = 0,sta1 = 0,sta2 = 0,sta3 = 0,sta4 = 0,sta5 = 0,sta6 = 0,sta7 = 0,sta8 = 0,sta9 = 0;
	unsigned char bsta = 0,csta = 0,dsta = 0,esta = 0;

	i2cmotor_read(client, 0x0,&sta0);
	i2cmotor_read(client, 0x1,&sta1);
	i2cmotor_read(client, 0x2,&sta2);
	i2cmotor_read(client, 0x3,&sta3);
	i2cmotor_read(client, 0x4,&sta4);
	printk(" 0H=%#hhx\n 1H=%#hhx\n 2H=%#hhx\n 3H=%#hhx 4H=%hhx\n",sta0,sta1,sta2,sta3,sta4);

	i2cmotor_read(client, 0x5,&sta5);
	i2cmotor_read(client, 0x6,&sta6);
	i2cmotor_read(client, 0x7,&sta7);
	i2cmotor_read(client, 0x8,&sta8);
	i2cmotor_read(client, 0x9,&sta9);
	printk(" 5H=%#hhx\n 6H=%#hhx\n 7H=%#hhx\n 8H=%#hhx 9H=%hhx\n",sta5,sta6,sta7,sta8,sta9);

	i2cmotor_read(client, 0xB,&bsta);
	i2cmotor_read(client, 0xC,&csta);
	i2cmotor_read(client, 0xD,&dsta);
	i2cmotor_read(client, 0xE,&esta);
	param->mrs.bhstate = bsta;
	param->mrs.chstate = csta;
	param->mrs.dhstate = dsta;
	param->mrs.ehstate = esta;
	printk("BH=%#hhx\nCH=%#hhx\nDH=%#hhx\nEH=%#hhx\n",bsta,csta,dsta,esta);

	return;
}

static int i2cmotor_param_check(jz_motor_param_t *param)
{
	if(param->motor < MOTOR_1 || param->motor > MOTOR_2){
		printk("motor out range,current only supprot motor 1 and 2\n");
		return -ENODEV;
	}

	if(param->type < MOTOR_HALF || param->type >MOTOR_FULL){
		printk("motor type out range\n");
		return -ENODEV;
	}

	if(param->mts < MOTOR_2P_4W || param->mts > MOTOR_4P_5W){
		printk("motor mts out range\n");
		return -ENODEV;
	}

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
    .name = "i2c_motor", 	/*设备节点的名字*/
    .fops = &i2cmotor_misc_fops,
};

static void i2cmotor_set_gpio_pwm(void *base,int pwm_chn)
{
	int value = 0;

	//t23 内部晶振默认通过pwm2来提供，对应gpio pb27
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

	/*用的是pwm2*/
	value = readl(base+CH_TCSR(pwm_chn));
	value |= EXTAL_EN | BYPASS;
	writel(value, base+CH_TCSR(pwm_chn));

	return ;
}

static int i2cmotor_device_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	//创建设备节点，用于和应用层交互
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
	struct jz_tcu_chn *tcu_chn = NULL;

	cell = (struct mfd_cell *)mfd_get_cell(pdev);
	if(!cell){
		dev_err(&pdev->dev, "No cell resource\n");
		return -ENXIO;
	}

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
    .probe  = tcu_probe,
    .remove = tcu_remove,
    .driver = {
		/*根据硬件的要求，这里需要pwm2，因此使用tcu_chn2*/
        .name 	= "tcu_chn2",
        .owner  = THIS_MODULE,
    },
};

static int tcu_get(void)
{
	int ret = 0;

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

	tcu_get();

    // 获取I2C适配器
    adapter = i2c_get_adapter(I2CID);
    if (!adapter) {
        pr_err("%s failed to get I2C adapter\n",__func__);
        return -ENODEV;
    }

    // 配置I2C设备信息,这个必须要和i2c_device_id中的name匹配
    strlcpy(board_info.type, "i2cmotor_device", sizeof(board_info.type));
    board_info.addr = DEV_ADDR;

    // 注册I2C设备驱动程序
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
	tcu_put();

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
