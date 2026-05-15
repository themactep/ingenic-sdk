#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <soc/gpio.h>

#define VGA_MAGIC 'D'

#define IOCTL_VGA_PHY_EN _IOW(VGA_MAGIC,110,unsigned int)

#ifdef  DEBUG
#define VGA_DEBUG(format, ...) do{printk(format, ## __VA_ARGS__);}while(0)
#else
#define VGA_DEBUG(format, ...) do{ } while(0)
#endif

#define VGA_PHY_ADDR		0x10
#define SYSTEM_RST_GPIO		GPIO_PC(16);
#define DAC_RST_GPIO		GPIO_PC(17);
#define CS0_GPIO			GPIO_PC(15);
#define CS1_GPIO			GPIO_PC(14);
#define CS2_GPIO			GPIO_PC(6);

static int i2c_bus = 0x0;
module_param(i2c_bus, int, S_IRUGO);
MODULE_PARM_DESC(i2c_bus, "the i2c bus of vga phy");

struct i2c_client *g_vga_i2c_client = NULL;

struct vga_driver
{
	int sys_rst_gpio;
	int dac_rst_gpio;
	int cs0_gpio;
	int cs1_gpio;
	int cs2_gpio;
	struct miscdevice miscdev;
	struct i2c_client *client;
	struct mutex mutex;
};

static unsigned int vga_phy_i2c_read(struct i2c_client *client,unsigned char reg)
{
	char rxbuf[1] = {reg};
	char txbuf[1] = {0};
	struct i2c_msg xfer[2] = {
		[0]	= {
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(rxbuf),
			.buf = rxbuf,
		},
		[1]	= {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(txbuf),
			.buf = txbuf,
		}
	};

	i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	return (unsigned int)txbuf[0];
}

static void vga_phy_i2c_write(struct i2c_client *client,unsigned char reg, unsigned char value)
{
	unsigned char txbuf[2] = {reg,value};
	struct i2c_msg xfer[1] = {
		[0]	= {
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(txbuf),
			.buf = txbuf,
		}
	};
	i2c_transfer(client->adapter,xfer,ARRAY_SIZE(xfer));
}
#if 0
static void vga_phy_reg_set(struct i2c_client *client,unsigned char reg, int start, int end, unsigned char val)
{
	int i = 0, mask = 0;
	unsigned char oldv = 0, new = 0;
	for(i = 0; i < (end-start + 1); i++) {
		mask += (1 << (start + i));
	}
	oldv = vga_phy_i2c_read(client,reg);
	new = oldv & (~mask);
	new |= val << start;
	vga_phy_i2c_write(client,reg, new);
}
#endif

static ssize_t vga_phy_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t vga_phy_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}


static int vga_phy_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct vga_driver *vga = container_of(dev, struct vga_driver, miscdev);
	VGA_DEBUG("%s - %d\n",__func__,__LINE__);
	return 0;
}

static int vga_phy_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct vga_driver *vga = container_of(dev, struct vga_driver, miscdev);
	VGA_DEBUG("%s - %d\n",__func__,__LINE__);
	return 0;
}

static long vga_phy_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct miscdevice *dev = file->private_data;
	struct vga_driver *vga = container_of(dev, struct vga_driver, miscdev);
	mutex_lock(&vga->mutex);
	switch(cmd)	{
		case IOCTL_VGA_PHY_EN:
			{
				vga_phy_i2c_write(vga->client,0x0a, 0xf0);
				vga_phy_i2c_write(vga->client,0x03, 0x74);
				VGA_DEBUG("vga phy 0x0a= %x\n",vga_phy_i2c_read(cga->client,0x0a));
				VGA_DEBUG("vga phy 0x03= %x\n",vga_phy_i2c_read(cga->client,0x03));
			}
			break;
		default:
			VGA_DEBUG("%s - %d\n invalid command",__func__,__LINE__);
			ret = -EFAULT;
			break;
	}
	mutex_lock(&vga->mutex);
	return ret;
}

const struct file_operations vga_phy_fops = {
	.owner = THIS_MODULE,
	.read = vga_phy_read,
	.write = vga_phy_write,
	.open = vga_phy_open,
	.unlocked_ioctl = vga_phy_ioctl,
	.release = vga_phy_release,
};

static int vga_phy_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
	int ret = 0;
	int value = 0;
	struct vga_driver *vga = NULL;
	unsigned int data = 0;
	int times = 1000;
	vga = kzalloc(sizeof(struct vga_driver),GFP_KERNEL);
	if(!vga){
		printk("alloc vga driver error\n");
		return -ENOMEM;
	}
	vga->client = i2c;

	gpio_request(GPIO_PB(28), "stat phy reset");
	gpio_direction_output(GPIO_PB(28), 1);
	msleep(100);
	gpio_direction_output(GPIO_PB(28), 0);
	msleep(100);
	gpio_direction_output(GPIO_PB(28), 1);

	data = *(volatile unsigned int *)0xb30d0178;
	data |= (1 << 28) | (1<<8);
	*(volatile unsigned int *)0xb30d0178 = data;
	msleep(10);
	data = *(volatile unsigned int *)0xb30d0178;
	data |= (1<<4);
	*(volatile unsigned int *)0xb30d0178 = data;
	msleep(10);

	i2c->addr = 0x10;
	vga_phy_i2c_write(i2c, 0x23, 0x10);
	i2c->addr = 0x30;
	vga_phy_i2c_write(i2c, 0x00, 0x40);

	i2c->addr = 0x10;
	vga_phy_i2c_write(i2c, 0x24, 0x1e);
	vga_phy_i2c_write(i2c, 0x63, 0x1e);
	vga_phy_i2c_write(i2c, 0x60, 0x01);
	vga_phy_i2c_write(i2c, 0x64, 0x01);
	vga_phy_i2c_write(i2c, 0x7f, 0x01);
	vga_phy_i2c_write(i2c, 0x87, 0x0f);
	vga_phy_i2c_write(i2c, 0x88, 0xff);
	vga_phy_i2c_write(i2c, 0x89, 0xff);
	vga_phy_i2c_write(i2c, 0x84, 0x28);
	vga_phy_i2c_write(i2c, 0x38, 0x07);
	vga_phy_i2c_write(i2c, 0x37, 0x45);
	vga_phy_i2c_write(i2c, 0x31, 0x80);
	vga_phy_i2c_write(i2c, 0x1a, 0x02);
	vga_phy_i2c_write(i2c, 0x61, 0x03);

	i2c->addr = 0x11;
	vga_phy_i2c_write(i2c, 0x7c, 0x01);
	vga_phy_i2c_write(i2c, 0x79, 0x21);
	vga_phy_i2c_write(i2c, 0x47, 0x02);
	vga_phy_i2c_write(i2c, 0x78, 0x3a);
	vga_phy_i2c_write(i2c, 0x73, 0x4e);
	vga_phy_i2c_write(i2c, 0x45, 0x1f);

	i2c->addr = 0x10;
	vga_phy_i2c_write(i2c, 0x0e, 0x13);
	vga_phy_i2c_write(i2c, 0x00, 0x1f);
	vga_phy_i2c_write(i2c, 0x00, 0x1e);

	/*read */
	i2c->addr = 0x10;
	printk("0x1023 = 0x%04x\n", vga_phy_i2c_read(i2c, 0x23));
	printk("0x1024 = 0x%04x\n", vga_phy_i2c_read(i2c, 0x24));
	printk("0x1063 = 0x%04x\n", vga_phy_i2c_read(i2c, 0x63));
	printk("0x100e = 0x%04x\n", vga_phy_i2c_read(i2c, 0x0e));
	i2c->addr = 0x30;
	printk("0x3000 = 0x%04x\n", vga_phy_i2c_read(i2c, 0x00));
	i2c->addr = 0x11;
	printk("0x117c = 0x%04x\n", vga_phy_i2c_read(i2c, 0x7c));
	printk("0x1145 = 0x%04x\n", vga_phy_i2c_read(i2c, 0x45));

	while((*(volatile unsigned int *)0xb30d017c & 0x70) && times--)
		printk("0x17c = 0x%08x\n", *(volatile unsigned int *)0xb30d017c);

	printk("0x17c = 0x%08x\n", *(volatile unsigned int *)0xb30d017c);


	vga->miscdev.minor = MISC_DYNAMIC_MINOR;
	vga->miscdev.name = "vga_phy";
	vga->miscdev.fops = &vga_phy_fops;
	ret = misc_register(&vga->miscdev);
	if (ret < 0) {
		ret = -ENOENT;
		printk("Failed to register vga phy miscdev!\n");
		goto failed_misc_register;
	}
	mutex_init(&vga->mutex);
	i2c_set_clientdata(i2c, vga);
	return 0;
failed_misc_register:
	kfree(vga);
	return ret;
}

static int vga_phy_i2c_remove(struct i2c_client *client)
{
	struct vga_driver *vga = i2c_get_clientdata(client);
	misc_deregister(&vga->miscdev);
	kfree(vga);
	return 0;
}

static struct i2c_board_info vga_i2c_info = {
	    I2C_BOARD_INFO("vga_phy", VGA_PHY_ADDR),
};

static const struct i2c_device_id vga_phy_i2c_id[] = {
	{ "vga_phy", 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, vga_phy_i2c_id);

static struct i2c_driver vga_phy_i2c_driver = {
	.driver = {
		.name = "vga_phy",
		.owner = THIS_MODULE,
	},
	.probe = vga_phy_i2c_probe,
	.remove = vga_phy_i2c_remove,
	.id_table = vga_phy_i2c_id,
};

static int __init init_vga_phy(void)
{
	int ret = 0;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	adapter = i2c_get_adapter(i2c_bus);
	if (!adapter) {
		printk("error:(%s,%d), i2c get adapter failed.\n",__func__,__LINE__);
		return -1;
	}
	client = i2c_new_device(adapter, &vga_i2c_info);
	if (!client) {
		printk("error:(%s,%d),new i2c device failed.\n",__func__,__LINE__);
		return -1;
	}
	g_vga_i2c_client = client;
	i2c_put_adapter(adapter);

	ret = i2c_add_driver(&vga_phy_i2c_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "Failed to register vga phy I2C driver: %d\n", ret);
		return -1;
	}
	return ret;
}

static void __exit exit_vga_phy(void)
{
	i2c_unregister_device(g_vga_i2c_client);
	g_vga_i2c_client = NULL;
	i2c_del_driver(&vga_phy_i2c_driver);
}

module_init(init_vga_phy);
module_exit(exit_vga_phy);
MODULE_AUTHOR("Weijie Xu <Weijie.xu@ingenic.com>");
MODULE_DESCRIPTION("VGA PHY driver");
MODULE_LICENSE("GPL v2");
