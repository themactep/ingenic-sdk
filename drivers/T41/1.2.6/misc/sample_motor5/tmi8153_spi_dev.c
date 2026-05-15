/*
 * X1000 SoC SPI_NAND driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.

 *  You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include "tmi8153_spi_dev.h"


static struct spi_motor_platform_data spi_motor_pdata = {
    .name			= "TMI8153",
    .id             = 0x8160,
};

//spi驱动结构
struct ingenic_spi_dev
{
    void *private_data; /* 私有数据 */
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
    //printk("spi read:addr:%#hhx,value:%#hhx,%#x\n",addr, rbuf[0],*(unsigned int*)rbuf);
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

    addr |= 0x80; // tmi8153 write寄存器时 寄存器地址的第7bit要置1

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
    printk("[%s][%d]\n",__FUNCTION__,__LINE__);
    if(!motor->flag){
        printk("[%s][%d]motor resource not ready yet\n",__func__,__LINE__);
        return -EPERM;
    }
    filp->private_data = motor->spi;
    return 0;
}

static int sample_spi_release(struct inode *inode, struct file *filp)
{
     printk("[%s][%d]\n",__FUNCTION__,__LINE__);
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

    ingenic_spi_write_reg(spi, 0x02, 0x8a); //全局使能，退出复位状态，解锁计数寄存器
    //先配置默认参数,以确保电机可以运行
    if(pmotor_param->motor_num == MOTOR_1){
        ingenic_spi_write_reg(spi, 0x20, 0x1d); //通道12手动模式，手动模式结束后进入保持状态

        ingenic_spi_write_reg(spi, 0x28, 0x0); //相位计数器清零低8bit
        ingenic_spi_write_reg(spi, 0x29, 0x0); //相位计数器清零高8bit

        /*圈数计数清零 */
        ingenic_spi_write_reg(spi, 0x2a, 0x0); //清零圈数计数寄存器低 8 位
        ingenic_spi_write_reg(spi, 0x2b, 0x0); //清零计数高 8 位

        //相位默认设置为512
        ingenic_spi_write_reg(spi, 0x2c, 0x0);
        ingenic_spi_write_reg(spi, 0x2d, 0x20);

        /*细分数和转向控制 */
        ingenic_spi_write_reg(spi, 0x22, (pmotor_param->motor_config.ch_stepmd << 4) | pmotor_param->motor_config.rotation_dir);

        /*设置分频 */
        ingenic_spi_write_reg(spi, 0x23, 0x03); //4分频 详见TMI8160寄存器手册
        //ingenic_spi_write_reg(spi, 0x24, 0x00); //4分频 时钟预分频寄存器和pwm分频寄存器
        //速度控制时钟预分频寄存器和速度控制时钟分频寄存器,该值控制速度的原理是控制pwm频率
        /*公式：speed = 28MHZ/(2^(sysclk_div & 0xff) <<5) *(1/(clk_csel&0xff)) + 1;详见TMI8160寄存器手册 24h*/
        ingenic_spi_write_reg(spi, 0x24, (((pmotor_param->motor_config.sysclk_div &0xff) << 5) |  (pmotor_param->motor_config.clk_csel & 0xff)));

        //速度控制
        ingenic_spi_write_reg(spi, 0x26, 0x71); //暂定这种速度，可通过预分频和控制时钟寄存器来控制速度
        ingenic_spi_write_reg(spi, 0x27, 0x04);

        /*step5. 配置运行圈数：512相位下，二圈为512*2*/
        switch(pmotor_param->motor_config.rotation_dir){
            case MOTOR_DRV_CW:
                break;
            case MOTOR_DRV_CCW:
                /*反转：圈数的是8192 减去设置的圈数，其他不变*/
                pmotor_param->motor_config.circle_set = 65536 - pmotor_param->motor_config.circle_set;
                break;
            default:
                break;
        }
        ingenic_spi_write_reg(spi,0x2e,pmotor_param->motor_config.circle_set & 0xff);
        ingenic_spi_write_reg(spi,0x2f,(pmotor_param->motor_config.circle_set >> 8) & 0xff);

        //step7. 通道使能和全局使能写入
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
        } while (pmotor_param->motor_config.circle_set  != value);
        /* step2. 圈数计数清零 */
        ingenic_spi_write_reg(spi, 0x08, 0x0); // 清零计数低 8 位
        ingenic_spi_write_reg(spi, 0x09, 0x0); // 清零计数高 5 位
        reg_gctrl = ingenic_spi_read_reg(spi, 0x02);
        if (reg_gctrl & 0x2)
        {
            reg_gctrl &= (~(0x2));
            ingenic_spi_write_reg(spi, 0x2, reg_gctrl); // 关闭圈数计数寄存器写使能
        }
#endif
    }

    if (pmotor_param->motor_num == MOTOR_2) {
       ingenic_spi_write_reg(spi, 0x40, 0x1d); //通道12手动模式，手动模式结束后进入保持状态

        ingenic_spi_write_reg(spi, 0x48, 0x0); //相位计数器清零低8bit
        ingenic_spi_write_reg(spi, 0x49, 0x0); //相位计数器清零高8bit

        /* step2. 圈数计数清零 */
        ingenic_spi_write_reg(spi, 0x4a, 0x0); //清零圈数计数寄存器低 8 位
        ingenic_spi_write_reg(spi, 0x4b, 0x0); //清零计数高 8 位

        //相位默认设置为512
        ingenic_spi_write_reg(spi, 0x4c, 0x0);
        ingenic_spi_write_reg(spi, 0x4d, 0x20);

        /* step3. 细分数和转向控制 */
        ingenic_spi_write_reg(spi, 0x42, (pmotor_param->motor_config.ch_stepmd << 4) | pmotor_param->motor_config.rotation_dir);

        /* step4. 设置分频 */
        ingenic_spi_write_reg(spi, 0x43, 0x03); //4分频 时钟预分频寄存器和pwm分频寄存器
        ingenic_spi_write_reg(spi, 0x44, 0x00); //4分频 时钟预分频寄存器和pwm分频寄存器
        //速度控制时钟预分频寄存器和速度控制时钟分频寄存器
        ingenic_spi_write_reg(spi, 0x44, (((pmotor_param->motor_config.sysclk_div &0xff) << 5) |  (pmotor_param->motor_config.clk_csel & 0xff)));

        //速度控制
        ingenic_spi_write_reg(spi, 0x46, 0x71); //具体什么标准，手册中也没写明白，暂时固定速度吧
        ingenic_spi_write_reg(spi, 0x47, 0x04);

        /*step5. 配置运行圈数：512相位下，二圈为512*2*/
         switch(pmotor_param->motor_config.rotation_dir){
            case MOTOR_DRV_CW:
                break;
            case MOTOR_DRV_CCW:
                /*反转：圈数的是8192 减去设置的圈数，其他不变*/
                pmotor_param->motor_config.circle_set = 65536 - pmotor_param->motor_config.circle_set;
                break;
            default:
                break;
        }
        ingenic_spi_write_reg(spi,0x4e,pmotor_param->motor_config.circle_set & 0xff);
        ingenic_spi_write_reg(spi,0x4f,(pmotor_param->motor_config.circle_set >> 8) & 0xff);
        //step7. 通道使能和全局使能写入
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
            value = (ret & ~(1<<7)) & 0Xff; //禁止使能通道，即关闭
            ingenic_spi_write_reg(spi, 0x20, value);
            break;
        case MOTOR_2:
            ret = ingenic_spi_read_reg(spi, 0x40);
            value = (ret & ~(1<<7)) & 0Xff; //禁止使能通道，即关闭
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
#if 1
   ingenic_spi_write_reg(spi, 0x10, 0x86);

    msleep(100);//100ms 确保ircut切换后，再关闭IRCUT,该参数为芯片建议参数
    ingenic_spi_write_reg(spi, 0x10, 0x8a);
    ingenic_spi_write_reg(spi, 0x10, 0x00);//关闭ircut

    return;
#endif
#if 0
    ingenic_spi_write_reg(spi, 0x11, 0x86);
    ingenic_spi_write_reg(spi, 0x02, 0x8b);

    value0 = ingenic_spi_read_reg(spi,0x11);
    value1 = ingenic_spi_read_reg(spi,0x02);

    //printk("ircut run,value0:%#x\n",value0);
    msleep(100);//100ms 确保ircut切换后，再关闭IRCUT,该参数为芯片建议参数
    ingenic_spi_write_reg(spi, 0x11, 0x8a);
    ingenic_spi_write_reg(spi, 0x02, 0x8b);
#endif

    return ;
}

//暂停，刹车
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
            value = ret | 0x03; //11b对应刹车
            ingenic_spi_write_reg(spi, 0x05, value);
            break;
        case MOTOR_2:
            ret = ingenic_spi_read_reg(spi, 0x15);
            value = ret | 0x03; //11b对应刹车
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

    /* step1. 芯片复位及初始化 */
    ingenic_spi_write_reg(spi, 0x02, 0x00);
    ingenic_spi_write_reg(spi, 0x02, 0x88); //退出复位状态

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

/* 定义SPI 设备 file_operations */
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

    /* 创建spi设备 */
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

    motor->flag = 1;//初始化完成，置1
    printk("sample_spi probe succeed\n");
    return 0;

error_misc_register:
    return err;
}

#ifdef CONFIG_KERNEL_6_1
static void ingenic_spidev_remove(struct spi_device *spi)
{
    static struct ingenic_spi_dev *motor_quit;
    motor_quit = spi->dev.driver_data;
    motor_quit->flag = 0;
    misc_deregister(&motor_quit->mdev);
    kfree(motor_quit);
    dev_set_drvdata(&spi->dev, NULL);

    return 0;
}
#else
static int ingenic_spidev_remove(struct spi_device *spi)
{
    static struct ingenic_spi_dev *motor_quit;
    motor_quit = spi->dev.driver_data;
    motor_quit->flag = 0;
    misc_deregister(&motor_quit->mdev);
    kfree(motor_quit);
    dev_set_drvdata(&spi->dev, NULL);

    return 0;
}
#endif

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
        .modalias           = "spidev",
        .platform_data      = &spi_motor_pdata,
        .controller_data    = 0,                /* cs for spi gpio */
        .bus_num            = 1,                // SPI0 = 0; SPI1 = 1
        .chip_select        = 0,
        .max_speed_hz       = 2000000,
        .mode               = 0,
    },
};

int ingenic_spi_board_info_size = sizeof(ingenic_spi_board_info) / sizeof(ingenic_spi_board_info[0]);

struct spi_device *ingenic_spi_device;

int __init ingenic_spidev_init(void)
{
#ifdef CONFIG_KERNEL_6_1
	if(spi_register_board_info(ingenic_spi_board_info, 2)) {
		printk("spi_register_board_info failed\n");
		return -1;
	}
#else
    struct spi_master *ingenic_spi_master;
    ingenic_spi_master = spi_busnum_to_master(ingenic_spi_board_info->bus_num);
    ingenic_spi_device = spi_new_device(ingenic_spi_master, ingenic_spi_board_info);
#endif
    return spi_register_driver(&ingenic_spidev_driver);
}

void __exit ingenic_spidev_exit(void)
{
#ifndef CONFIG_KERNEL_6_1
    spi_unregister_device(ingenic_spi_device);
#endif
    spi_unregister_driver(&ingenic_spidev_driver);
}

module_init(ingenic_spidev_init);
module_exit(ingenic_spidev_exit);
MODULE_LICENSE("GPL");
