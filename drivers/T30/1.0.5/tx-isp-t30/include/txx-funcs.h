#ifndef __TXX_DRV_FUNCS_H__
#define __TXX_DRV_FUNCS_H__
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <linux/file.h>
/*#include <linux/list.h>*/
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/sched.h>
/*#include <linux/delay.h>*/
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/mfd/core.h>
#include <linux/mempolicy.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <soc/irq.h>
#include <soc/base.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <soc/gpio.h>
#include <mach/platform.h>
/*#include <linux/seq_file.h>*/
#include <jz_proc.h>

struct jz_driver_common_interfaces {
	unsigned int flags_0;			// The flags must be checked.
	/* platform interface */
	int (*platform_driver_register)(struct platform_driver *drv);
	void (*platform_driver_unregister)(struct platform_driver *drv);
	void (*platform_set_drvdata)(struct platform_device *pdev, void *data);
	void *(*platform_get_drvdata)(const struct platform_device *pdev);
	int (*platform_device_register)(struct platform_device *pdev);
	void (*platform_device_unregister)(struct platform_device *pdev);
	struct resource *(*platform_get_resource)(struct platform_device *dev,
				       unsigned int type, unsigned int num);
	int (*dev_set_drvdata)(struct device *dev, void *data);
	void* (*dev_get_drvdata)(const struct device *dev);
	int (*platform_get_irq)(struct platform_device *dev, unsigned int num);
	struct resource * (*tx_request_mem_region)(resource_size_t start, resource_size_t n,
				   const char *name);
	void (*tx_release_mem_region)(resource_size_t start, resource_size_t n);
	void __iomem *(*tx_ioremap)(phys_addr_t offset, unsigned long size);
	void (*iounmap)(const volatile void __iomem *addr);
	unsigned int reserve_platform[8];

	/* interrupt interface */
	int (*request_threaded_irq)(unsigned int irq, irq_handler_t handler,
			 irq_handler_t thread_fn, unsigned long irqflags,
			 const char *devname, void *dev_id);
	void (*enable_irq)(unsigned int irq);
	void (*disable_irq)(unsigned int irq);
	void (*free_irq)(unsigned int irq, void *dev_id);

	/* lock and mutex interface */
	void (*tx_spin_lock_irqsave)(spinlock_t *lock, unsigned long *flags);
	void (*spin_unlock_irqrestore)(spinlock_t *lock, unsigned long flags);
	void (*tx_spin_lock_init)(spinlock_t *lock);
	void (*mutex_lock)(struct mutex *lock);
	void (*mutex_unlock)(struct mutex *lock);
	void (*tx_mutex_init)(struct mutex *mutex);

	/* clock interfaces */
	struct clk *(*clk_get)(struct device *dev, const char *id);
	int (*clk_enable)(struct clk *clk);
	int (*clk_is_enabled)(struct clk *clk);
	void (*clk_disable)(struct clk *clk);
	unsigned long (*clk_get_rate)(struct clk *clk);
	void(*clk_put)(struct clk *clk);
	int (*clk_set_rate)(struct clk *clk, unsigned long rate);
	unsigned int reserve_clk[8];

	/* i2c interfaces */
	struct i2c_adapter* (*i2c_get_adapter)(int nr);
	void (*i2c_put_adapter)(struct i2c_adapter *adap);
	int (*i2c_transfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
	int (*i2c_register_driver)(struct module *, struct i2c_driver *);
	void (*i2c_del_driver)(struct i2c_driver *);

	struct i2c_client *(*i2c_new_device)(struct i2c_adapter *adap, struct i2c_board_info const *info);
	void *(*i2c_get_clientdata)(const struct i2c_client *dev);
	void (*i2c_set_clientdata)(struct i2c_client *dev, void *data);
	void (*i2c_unregister_device)(struct i2c_client *client);

	unsigned int reserver_i2c[8];

	/* gpio interfaces */
	int (*gpio_request)(unsigned gpio, const char *label);
	void (*gpio_free)(unsigned gpio);
	int (*gpio_direction_output)(unsigned gpio, int value);
	int (*gpio_direction_input)(unsigned gpio);
	int (*gpio_set_debounce)(unsigned gpio, unsigned debounce);
	int (*jzgpio_set_func)(enum gpio_port port, enum gpio_function func,unsigned long pins);
	int (*jzgpio_ctrl_pull)(enum gpio_port port, int enable_pull,unsigned long pins);

	/* system interface */
	void (*msleep)(unsigned int msecs);
	bool (*capable)(int cap);
	unsigned long long (*sched_clock)(void);
	bool (*try_module_get)(struct module *module);
	int (*tx_request_module)(bool wait, const char *fmt, char *s);
	void (*module_put)(struct module *module);

	/* wait */
	void (*init_completion)(struct completion *x);
	void (*complete)(struct completion *x);
	int (*wait_for_completion_interruptible)(struct completion *x);

	/* misc */
	int (*misc_register)(struct miscdevice *mdev);
	int (*misc_deregister)(struct miscdevice *mdev);
	struct proc_dir_entry *(*proc_create_data)(const char *name, umode_t mode,
					struct proc_dir_entry *parent,
					const struct file_operations *proc_fops,
					void *data);
	/* proc */
	ssize_t (*seq_read)(struct file *file, char __user *buf, size_t size, loff_t *ppos);
	loff_t (*seq_lseek)(struct file *file, loff_t offset, int whence);
	int (*single_release)(struct inode *inode, struct file *file);
	int (*single_open_size)(struct file *file, int (*show)(struct seq_file *, void *),
		void *data, size_t size);
	struct proc_dir_entry* (*jz_proc_mkdir)(char *s);

	/* isp driver interface */
	void (*get_isp_priv_mem)(unsigned int *phyaddr, unsigned int *size);
	unsigned int flags_1;			// The flags must be checked.
};



#ifndef U16_MAX
#define U16_MAX 					0xFFFF
#endif

#define paddr2vaddr(phyaddr) ((void *)((phyaddr) + PAGE_OFFSET - PHYS_OFFSET))

#if 0
#define APICAL_ABS(a)        ((a)>=0?(a):-(a))
#define APICAL_SIGN(a)  ((a)>=0?(1):(-1))
#define APICAL_MIN(a,b) ((a)>=b?(b):(a))
#define APICAL_MAX(a,b) ((a)>=b?(a):(b))
#define APICAL_ABSDIFF(a,b) ((a)>(b)? (a-b) : (b-a))
#define LIN_EQUATION_FRACTION_SIZE 5
#define round_shift(a,sh)   (((a)>>(sh))+(((a)>>(sh-1))&1))
#endif

uint8_t private_leading_one_position(const uint32_t in);
uint32_t private_log2_int_to_fixed(const uint32_t val, const uint8_t out_precision, const uint8_t shift_out);
uint32_t private_log2_fixed_to_fixed(const uint32_t val, const int in_fix_point, const uint8_t out_fix_point);
int32_t private_log2_fixed_to_fixed_64(uint64_t val, int32_t in_fix_point, uint8_t out_fix_point);
uint32_t private_math_exp2(uint32_t val, const unsigned char shift_in, const unsigned char shift_out);

/* platform interfaces */
int private_platform_driver_register(struct platform_driver *drv);
void private_platform_driver_unregister(struct platform_driver *drv);
void private_platform_set_drvdata(struct platform_device *pdev, void *data);
void *private_platform_get_drvdata(struct platform_device *pdev);
int private_platform_device_register(struct platform_device *pdev);
void private_platform_device_unregister(struct platform_device *pdev);
struct resource *private_platform_get_resource(struct platform_device *dev,
			       unsigned int type, unsigned int num);
int private_dev_set_drvdata(struct device *dev, void *data);
void* private_dev_get_drvdata(const struct device *dev);
int private_platform_get_irq(struct platform_device *dev, unsigned int num);
#if 0
struct resource * private_request_mem_region(resource_size_t start, resource_size_t n,
			   const char *name);
void private_release_mem_region(resource_size_t start, resource_size_t n);

void __iomem * private_ioremap(phys_addr_t offset, unsigned long size);
#else
#define private_request_mem_region(s ,n ,name) request_mem_region((s),(n),(name))

#define private_release_mem_region(o, s) release_mem_region((o), (s))
#define private_ioremap(o, s) ioremap((o), (s))
#endif
void private_iounmap(const volatile void __iomem *addr);
/* interrupt interfaces */
int private_request_threaded_irq(unsigned int irq, irq_handler_t handler,
		irq_handler_t thread_fn, unsigned long irqflags,
		const char *devname, void *dev_id);
void private_enable_irq(unsigned int irq);
void private_disable_irq(unsigned int irq);
void private_free_irq(unsigned int irq, void *dev_id);

/* lock and mutex interfaces */
#if 0
void private_spin_lock_irqsave(spinlock_t *lock, unsigned long flags);
#else
#define private_spin_lock_irqsave(l, f) spin_lock_irqsave((l), (f))
#endif
void private_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);
#if 0
void private_spin_lock_init(spinlock_t *lock);
#else
#define private_spin_lock_init(l) spin_lock_init((l))
#endif
void private_mutex_lock(struct mutex *lock);
void private_mutex_unlock(struct mutex *lock);
#if 0
void private_mutex_init(struct mutex *mutex);
#else
#define private_mutex_init(m) mutex_init((m))
#endif
/* clock interfaces */
struct clk * private_clk_get(struct device *dev, const char *id);
int private_clk_enable(struct clk *clk);
int private_clk_is_enabled(struct clk *clk);
void private_clk_disable(struct clk *clk);
void private_clk_put(struct clk *clk);
int private_clk_set_rate(struct clk *clk, unsigned long rate);
unsigned long private_clk_get_rate(struct clk *clk);


/* i2c interfaces */
struct i2c_adapter* private_i2c_get_adapter(int nr);
void private_i2c_put_adapter(struct i2c_adapter *adap);
int private_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
int private_i2c_register_driver(struct module *, struct i2c_driver *);
void private_i2c_del_driver(struct i2c_driver *drv);
struct i2c_client *private_i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info);
void private_i2c_set_clientdata(struct i2c_client *dev, void *data);
void *private_i2c_get_clientdata(const struct i2c_client *dev);
int private_i2c_add_driver(struct i2c_driver *drv);
void private_i2c_unregister_device(struct i2c_client *client);

/* gpio interfaces */
int private_gpio_request(unsigned gpio, const char *label);
void private_gpio_free(unsigned gpio);
int private_gpio_direction_output(unsigned gpio, int value);
int private_gpio_direction_input(unsigned gpio);
int private_gpio_set_debounce(unsigned gpio, unsigned debounce);
int private_jzgpio_set_func(enum gpio_port port, enum gpio_function func,unsigned long pins);
int private_jzgpio_ctrl_pull(enum gpio_port port, int enable_pull,unsigned long pins);

/* system interfaces */
void private_msleep(unsigned int msecs);
bool private_capable(int cap);
unsigned long long private_sched_clock(void);
bool private_try_module_get(struct module *module);
#if 0
int private_request_module(bool wait, const char *fmt, char *s);
#else
#define private_request_module(f ,s) request_module((f), (s))
#endif
void private_module_put(struct module *module);

/* wait interfaces */
void private_init_completion(struct completion *x);
void private_complete(struct completion *x);
int private_wait_for_completion_interruptible(struct completion *x);

/* misc driver interfaces */
int private_misc_register(struct miscdevice *mdev);
int private_misc_deregister(struct miscdevice *mdev);

struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode,
		struct proc_dir_entry *parent,
		const struct file_operations *proc_fops,
		void *data);
/* proc file interfaces */
ssize_t private_seq_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
loff_t private_seq_lseek(struct file *file, loff_t offset, int whence);
int private_single_release(struct inode *inode, struct file *file);
int private_single_open_size(struct file *file, int (*show)(struct seq_file *, void *),
		void *data, size_t size);
struct proc_dir_entry* private_jz_proc_mkdir(char *s);


/* isp driver interface */
void private_get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size);

int private_driver_get_interface(void);

#endif /*__TXX_DRV_FUNCS_H__*/
