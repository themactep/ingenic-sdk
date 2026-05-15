/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 *
 * */
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <linux/freezer.h>
#include <linux/delay.h>

#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include "tmi8153_spi_dev.h"


static struct spi_motor_platform_data spi_motor_pdata = {
	.name			= "TMI8153",
	.id				= 0x8160,
};

//SPI driver structure
struct ingenic_spi_dev
{
	void *private_data; /* private data */
	struct miscdevice mdev;
	struct device *dev;
	unsigned char flag;
	struct spi_device *spi;
};

struct ingenic_spi_dev *motor;


static int ingenic_spi_read_reg(struct spi_device *spi, unsigned char addr)
{
	struct spi_message message;
	struct spi_transfer transfer[2];
	unsigned char rbuf[4] = {0, 0, 0, 0};
	unsigned char wbuf[4] = {0, 0, 0, 0};
	int ret = 0;

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	wbuf[0] = addr & 0xff;

	transfer[0].tx_buf = wbuf;
	transfer[0].len = 1;
	spi_message_add_tail(&transfer[0], &message);

	transfer[1].rx_buf = rbuf;
	transfer[1].len = 1;
	transfer[1].cs_change = 1;
	spi_message_add_tail(&transfer[1], &message);

	ret = spi_sync(spi, &message);
	if (ret)
	{
		printk("%s -- %s --%d  spi_sync() error !\n", __FILE__, __func__, __LINE__);
		return ret;
	}
	// *value = *(unsigned int*)rbuf;
	//printk("spi read:addr:%#hhx,value:%#hhx,%#x\n",addr,rbuf[0],*(unsigned int*)rbuf);
	return *rbuf;
}

static int ingenic_spi_write_reg(struct spi_device *spi, unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct spi_message message;
	struct spi_transfer transfer[1];
	char wbuf[8];

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	addr |= 0x80; //When writing the register, the 7th bit of the register address in tmi8153 should be set to 1

	wbuf[0] = addr & 0xff;
	wbuf[1] = value & 0xff;
	//wbuf[2] = (value & 0xff00) >> 8;

	transfer[0].tx_buf = wbuf;
	transfer[0].len = 2;
	transfer[0].cs_change = 1;

	spi_message_add_tail(&transfer[0], &message);
	ret = spi_sync(spi, &message);
	if (ret)
	{
		printk("spi_sync error ! %s %s %d\n", __FILE__, __func__, __LINE__);
		ret = -EIO;
	}
	//printk("w addr:%#hhx,value:%#x\n",addr,value);
	return ret;
}

static int sample_spi_open(struct inode *inode, struct file *filp)
{
	if(!motor->flag){
		printk("[%s][%d]motor resource not ready yet\n",__func__,__LINE__);
		return -EPERM;
	}
	filp->private_data = motor->spi;
	return 0;
}

static int sample_spi_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static void spi_motor_run(struct spi_device *spi,spi_motor_param_t *pmotor_param)
{
	if(!spi || !pmotor_param){
		printk("[%s][%d]err,param is null\n",__func__,__LINE__);
		return ;
	}

	if((pmotor_param->motor_num != MOTOR_1) && ( pmotor_param->motor_num != MOTOR_2)){
		printk("[%s][%d]err,param is out over range\n",__func__,__LINE__);
		return ;
	}

	if(TMI8153_STEPMD_64 > pmotor_param->motor_config.ch_stepmd || pmotor_param->motor_config.ch_stepmd > TMI8153_STEPMD_256){
		printk("[%s][%d]err,param is out over range\n",__func__,__LINE__);
		return ;
	}

	if(TMI8153_CSEL_1 > pmotor_param->motor_config.clk_csel || pmotor_param->motor_config.clk_csel > TMI8153_CSEL_9){
		printk("[%s][%d]err,param is out over range\n",__func__,__LINE__);
		return ;
	}

	if(TMI8153_CDIV_1 > pmotor_param->motor_config.sysclk_div || pmotor_param->motor_config.sysclk_div > TMI8153_CDIV_128){
		printk("[%s][%d]err,param is out over range\n",__func__,__LINE__);
		return ;
	}

	ingenic_spi_write_reg(spi, 0x02, 0x8a); //Global enable, exit reset state, unlock count register
	//Configure default parameters first to ensure that the motor can run
	if(pmotor_param->motor_num == MOTOR_1){
		ingenic_spi_write_reg(spi, 0x20, 0x1d); //Channel 12 manual mode, enters hold mode after manual mode ends

		ingenic_spi_write_reg(spi, 0x28, 0x0); //Phase counter reset to low 8-bit
		ingenic_spi_write_reg(spi, 0x29, 0x0); //Phase counter reset to high 8-bit

		/* Reset the number of turns count to zero */
		ingenic_spi_write_reg(spi, 0x2a, 0x0); //Reset the low 8 bits of the cycle count register
		ingenic_spi_write_reg(spi, 0x2b, 0x0); //Reset the high 8 bits of the cycle count register

		//The default phase setting is 512
		ingenic_spi_write_reg(spi, 0x2c, 0x0);
		ingenic_spi_write_reg(spi, 0x2d, 0x20);

		/* Fine score and steering control */
		ingenic_spi_write_reg(spi, 0x22, (pmotor_param->motor_config.ch_stepmd << 4) | pmotor_param->motor_config.rotation_dir);

		/* Set frequency division */
		ingenic_spi_write_reg(spi, 0x23, 0x03); //See TMI8160 Register Manual for details on the 4-way frequency division
		//ingenic_spi_write_reg(spi, 0x24, 0x00); //4-way clock pre division register and PWM division register
		//The speed control clock pre division register and speed control clock division register are used to control the PWM frequency
		/*formula：speed = 28MHZ/(2^(sysclk_div & 0xff) <<5) *(1/(clk_csel&0xff)) + 1;Refer to TMI8160 Register Manual for details 24h*/
		ingenic_spi_write_reg(spi, 0x24, (((pmotor_param->motor_config.sysclk_div &0xff) << 5) |  (pmotor_param->motor_config.clk_csel & 0xff)));

		//speed control
		ingenic_spi_write_reg(spi, 0x26, 0x71); //Temporarily set this speed, which can be controlled by pre dividing and controlling the clock register
		ingenic_spi_write_reg(spi, 0x27, 0x04);

		/* step5. Configure the number of operating cycles: Under 512 phase, the second cycle is 512 * 2 */
		switch(pmotor_param->motor_config.rotation_dir){
			case MOTOR_DRV_CW:
				break;
			case MOTOR_DRV_CCW:
				/* Reverse: The number of turns is 8192 minus the set number of turns, and everything else remains unchanged */
				pmotor_param->motor_config.circle_set = 65536 - pmotor_param->motor_config.circle_set;
				break;
			default:
				break;
		}
		ingenic_spi_write_reg(spi,0x2e,pmotor_param->motor_config.circle_set & 0xff);
		ingenic_spi_write_reg(spi,0x2f,(pmotor_param->motor_config.circle_set >> 8) & 0xff);

		//step7. Channel enable and global enable write
		ingenic_spi_write_reg(spi, 0x20, 0x9d);
		//ingenic_spi_write_reg(spi, 0x02, 0x8b);
#if 0
		printk("circle:%#x,%#x\n",ingenic_spi_read_reg(spi, 0x0b),ingenic_spi_read_reg(spi, 0x0c));
#endif
#if 0
		do
		{
			msleep(50);
			value = (ingenic_spi_read_reg(spi, 0x08) | (ingenic_spi_read_reg(spi, 0x09) << 8));
			printk("value:%#x,circle_set:%#x\n",value,pmotor_param->motor_config.circle_set);
		} while (pmotor_param->motor_config.circle_set	!= value);
		/* step2. Reset the number of turns count to zero */
		ingenic_spi_write_reg(spi, 0x08, 0x0); //Reset count low by 8 digits
		ingenic_spi_write_reg(spi, 0x09, 0x0); // Reset count high by 5 digits
		reg_gctrl = ingenic_spi_read_reg(spi, 0x02);
		if (reg_gctrl & 0x2)
		{
			reg_gctrl &= (~(0x2));
			ingenic_spi_write_reg(spi, 0x2, reg_gctrl); //Turn off the write enable of the lap count register
		}
#endif
	}

	if (pmotor_param->motor_num == MOTOR_2) {
	   ingenic_spi_write_reg(spi, 0x40, 0x1d); //Channel 12 manual mode, enters hold mode after manual mode ends

		ingenic_spi_write_reg(spi, 0x48, 0x0); //Phase counter reset to low 8-bit
		ingenic_spi_write_reg(spi, 0x49, 0x0); //Phase counter reset to high 8-bit

		/* step2. Reset the number of turns count to zero */
		ingenic_spi_write_reg(spi, 0x4a, 0x0); //Reset the low 8 bits of the cycle count register
		ingenic_spi_write_reg(spi, 0x4b, 0x0); //Reset the high 8 bits of the cycle count register

		//The default phase setting is 512
		ingenic_spi_write_reg(spi, 0x4c, 0x0);
		ingenic_spi_write_reg(spi, 0x4d, 0x20);

		/* step3. Fine score and steering control */
		ingenic_spi_write_reg(spi, 0x42, (pmotor_param->motor_config.ch_stepmd << 4) | pmotor_param->motor_config.rotation_dir);

		/* step4. Set frequency division */
		ingenic_spi_write_reg(spi, 0x43, 0x03); //4-way clock pre division register and PWM division register
		ingenic_spi_write_reg(spi, 0x44, 0x00); //4-way clock pre division register and PWM division register
		//Speed control clock pre division register and speed control clock division register
		ingenic_spi_write_reg(spi, 0x44, (((pmotor_param->motor_config.sysclk_div &0xff) << 5) |  (pmotor_param->motor_config.clk_csel & 0xff)));

		//speed control
		ingenic_spi_write_reg(spi, 0x46, 0x71);
		ingenic_spi_write_reg(spi, 0x47, 0x04);

		/* step5. Configure the number of operating cycles: Under 512 phase, the second cycle is 512*2 */
		 switch(pmotor_param->motor_config.rotation_dir){
			case MOTOR_DRV_CW:
				break;
			case MOTOR_DRV_CCW:
				/* Reverse: The number of turns is 8192 minus the set number of turns, and everything else remains unchanged */
				pmotor_param->motor_config.circle_set = 65536 - pmotor_param->motor_config.circle_set;
				break;
			default:
				break;
		}
		ingenic_spi_write_reg(spi,0x4e,pmotor_param->motor_config.circle_set & 0xff);
		ingenic_spi_write_reg(spi,0x4f,(pmotor_param->motor_config.circle_set >> 8) & 0xff);
		//step7. Channel enable and global enable write
		ingenic_spi_write_reg(spi, 0x40, 0x9d);
	}

	return ;
}
static void spi_motor_stop(struct spi_device *spi,spi_motor_param_t *pmotor_param)
{
	unsigned int ret = 0;
	unsigned char value = 0;
	if(!spi || !pmotor_param){
		printk("[%s][%d]err,param is null\n",__func__,__LINE__);
		return ;
	}

	switch(pmotor_param->motor_num)
	{
		case MOTOR_1:
			ret = ingenic_spi_read_reg(spi, 0x20);
			value = (ret & ~(1<<7)) & 0Xff; //Prohibit enabling channels, i.e. close
			ingenic_spi_write_reg(spi, 0x20, value);
			break;
		case MOTOR_2:
			ret = ingenic_spi_read_reg(spi, 0x40);
			value = (ret & ~(1<<7)) & 0Xff; //Prohibit enabling channels, i.e. close
			ingenic_spi_write_reg(spi, 0x40, value);
			break;
		default:
			printk("[%s][%d]not support this motor yet\n",__func__,__LINE__);
			break;
	}
	return ;
}

static void spi_motor_ircut(struct spi_device *spi,spi_motor_param_t *pmotor_param)
{
	 //unsigned int value0,value1;
	if(!spi || !pmotor_param){
		printk("[%s][%d]err,param is null\n",__func__,__LINE__);
		return ;
	}

   ingenic_spi_write_reg(spi, 0x10, 0x84);

	msleep(100);//After ensuring the circuit switch for 100ms, turn off IRCUT, which is the recommended parameter for the chip
	ingenic_spi_write_reg(spi, 0x10, 0x88);
	msleep(100);
	ingenic_spi_write_reg(spi, 0x10, 0x00);//close ircut

	return ;
}

//Pause, brake
static void spi_motor_pause(struct spi_device *spi,spi_motor_param_t *pmotor_param)
{
	unsigned int ret = 0;
	unsigned char value = 0;

	if(!spi || !pmotor_param){
		printk("[%s][%d]err,param is null\n",__func__,__LINE__);
		return ;
	}

	switch(pmotor_param->motor_num)
	{
		case MOTOR_1:
			ret = ingenic_spi_read_reg(spi, 0x22);
			value = ret | 0x03; //11b corresponds to brakes
			ingenic_spi_write_reg(spi, 0x05, value);
			break;
		case MOTOR_2:
			ret = ingenic_spi_read_reg(spi, 0x15);
			value = ret | 0x03; //11b corresponds to brakes
			ingenic_spi_write_reg(spi, 0x15, value);
			break;
		default:
			printk("[%s][%d]not support this motor yet\n",__func__,__LINE__);
			break;
	}
	return ;
}

static void spi_motor_reset(struct spi_device *spi,spi_motor_param_t *pmotor_param)
{
	if(!spi || !pmotor_param){
		printk("[%s][%d]err,param is null\n",__func__,__LINE__);
		return ;
	}

	/* step1. Chip reset and initialization */
	ingenic_spi_write_reg(spi, 0x02, 0x00);
	ingenic_spi_write_reg(spi, 0x02, 0x88); //Exit the reset state

	return ;
}

static long sample_motor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	//struct ingenic_spi_dev *motor = NULL;
	int data = 0,ret = 0;
	struct spi_device *spi;
	spi_motor_param_t motor_param = {0};

	spi  = filp->private_data;
	if(!spi) {
		printk(KERN_ERR"[%s][%d]this motor not init yet!\n",__func__,__LINE__);
		return -ENODEV;
	}

	switch(cmd)
	{
		case SPI_MOTOR_RUN:
			copy_from_user(&motor_param, (void __user *)arg, sizeof(spi_motor_param_t));
			spi_motor_run(spi,&motor_param);
			break;
		case SPI_MOTOR_STOP:
			copy_from_user(&motor_param, (void __user *)arg, sizeof(spi_motor_param_t));
			spi_motor_stop(spi,&motor_param);
			break;
		case SPI_MOTOR_PAUSE:
			copy_from_user(&motor_param, (void __user *)arg, sizeof(spi_motor_param_t));
			spi_motor_pause(spi,&motor_param);
			break;
		case SPI_MOTOR_IRCUT:
			copy_from_user(&motor_param, (void __user *)arg, sizeof(spi_motor_param_t));
			spi_motor_ircut(spi,&motor_param);
			break;
		case SPI_MOTOR_RESET:
			copy_from_user(&motor_param, (void __user *)arg, sizeof(spi_motor_param_t));
			spi_motor_reset(spi,&motor_param);
			break;
		case SPI_MOTOR_REGISTER_DEBUG:
			copy_from_user(&data, (void __user *)arg, sizeof(int));
			ret = ingenic_spi_read_reg(spi,(data & 0xff));
			printk("debug(dev:%#hhx,data:%#x)\n",data & 0xff,ret);
			break;
		default:
			break;
	}
	return 0;
}

/* Definition SPI Device file_operations */
static struct file_operations sample_spi_fops = {
	.owner = THIS_MODULE,
	.open = sample_spi_open,
	.release = sample_spi_release,
	.unlocked_ioctl = sample_motor_ioctl,
};

static int ingenic_spidev_probe(struct spi_device *spi)
{
	int err;
	//struct spi_motor_platform_data *pdata = spi->dev.platform_data;

	motor = kzalloc(sizeof(struct ingenic_spi_dev), GFP_KERNEL);
	if (!motor)
	{
		printk("%s---%s---%d\n", __FILE__, __func__, __LINE__);
		printk("kzalloc() error !\n");
		return -ENOMEM;
	}

	motor->spi = spi;
	motor->dev = &spi->dev;
	dev_set_drvdata(&spi->dev, motor);

	/* Create spi device */
	motor->mdev.minor = MISC_DYNAMIC_MINOR;
	motor->mdev.name = "motor";
	motor->mdev.fops = &sample_spi_fops;

	err = misc_register(&motor->mdev);
	if (err < 0)
	{
		err = -ENOENT;
		dev_err(&spi->dev, "misc_register failed\n");
		goto error_misc_register;
	}

	motor->flag = 1;//Initialization completed, set to 1
	printk("sample_spi probe succeed\n");
	return 0;

error_misc_register:
	return err;
}

static int ingenic_spidev_remove(struct spi_device *spi)
{
	static struct ingenic_spi_dev *motor_quit;
	motor_quit = dev_get_drvdata(&spi->dev);
	motor_quit->flag = 0;
	misc_deregister(&motor_quit->mdev);
	kfree(motor_quit);
	dev_set_drvdata(&spi->dev, NULL);

	return 0;
}

struct spi_device_id ingenic_id_table[] = {
	{
		.name = "spidev",
	},
};

static struct spi_driver ingenic_spidev_driver = {
	.driver = {
		.name = "spidev",
		.owner = THIS_MODULE,
	},
	.id_table = ingenic_id_table,
	.probe = ingenic_spidev_probe,
	.remove = ingenic_spidev_remove,
	.shutdown = NULL,
};

static struct spi_board_info ingenic_spi_board_info[] = {
	[0] = {
		.modalias			= "spidev",
		.platform_data		= &spi_motor_pdata,
		.controller_data	= 0,				/* cs for spi gpio */
		.bus_num			= 0,				// SPI0 = 0; SPI1 = 1
		.chip_select		= 0,
		.max_speed_hz		= 2000000,
		.mode				= 0,
	},
};

int ingenic_spi_board_info_size = sizeof(ingenic_spi_board_info) / sizeof(ingenic_spi_board_info[0]);

struct spi_device *ingenic_spi_device;

int __init ingenic_spidev_init(void)
{
	struct spi_master *ingenic_spi_master;
	ingenic_spi_master = spi_busnum_to_master(ingenic_spi_board_info->bus_num);
	ingenic_spi_device = spi_new_device(ingenic_spi_master, ingenic_spi_board_info);
	return spi_register_driver(&ingenic_spidev_driver);
}

void __exit ingenic_spidev_exit(void)
{
	spi_unregister_device(ingenic_spi_device);
	spi_unregister_driver(&ingenic_spidev_driver);
}

module_init(ingenic_spidev_init);
module_exit(ingenic_spidev_exit);
MODULE_LICENSE("GPL");
