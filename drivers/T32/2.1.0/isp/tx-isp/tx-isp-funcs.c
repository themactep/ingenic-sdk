#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <linux/file.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/mfd/core.h>
#include <linux/mempolicy.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
#include <linux/i2c-gpio.h>
#endif

/* #include <gpio.h> */
#include <linux/gpio.h>

#include <soc/gpio.h>
#include <soc/base.h>

#ifdef CONFIG_KERNEL_3_10
#include <soc/irq.h>
#include <mach/platform.h>
#endif

#ifdef CONFIG_KERNEL_4_4_94
#include <dt-bindings/interrupt-controller/PRJ007-irq.h>
#endif

/* #include <mach/platform.h> */
/* #include <mach/jzdma.h> */
/* #include <mach/jzsnd.h> */
#include <linux/spi/spi.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <jz_proc.h>
/* #include <linux/mfd/jz_tcu.h> */

#ifdef CONFIG_ZERATUL
#include "include/txx-funcs.h"
#else
#include <txx-funcs.h>
#endif  /* CONFIG_ZERATUL */
/* -------------------debugfs interface------------------- */
static int print_level = ISP_WARNING_LEVEL;
module_param(print_level, int, S_IRUGO);
MODULE_PARM_DESC(print_level, "isp print level");

static char *clk_name = "mpll";
module_param(clk_name, charp, S_IRUGO);
MODULE_PARM_DESC(clk_name, "chose parent clk");

static char *clks_name = "mpll";
module_param(clks_name, charp, S_IRUGO);
MODULE_PARM_DESC(clks_name, "chose scaler parent clk");

static char *clka_name = "sclka";
module_param(clka_name, charp, S_IRUGO);
MODULE_PARM_DESC(clka_name, "chose axi parent clk");

static char *clkv_name = "sclka";
module_param(clkv_name, charp, S_IRUGO);
MODULE_PARM_DESC(clkv_name, "chose axi parent clk");

static char *clkl_name = "mpll";
module_param(clkl_name, charp, S_IRUGO);
MODULE_PARM_DESC(clkl_name, "chose ldc parent clk");

#ifdef CONFIG_SOC_PRJ008
static int isp_clk = 340000000;
#elif defined(CONFIG_SOC_PRJ007)
static int isp_clk = 219000000;
#endif
module_param(isp_clk, int, S_IRUGO);
MODULE_PARM_DESC(isp_clk, "isp clock freq");

static int isp_clks = 476000000;
module_param(isp_clks, int, S_IRUGO);
MODULE_PARM_DESC(isp_clks, "scaler clock freq");

static int isp_clka = 500000000;
module_param(isp_clka, int, S_IRUGO);
MODULE_PARM_DESC(isp_clka, "axi clock freq");

#ifdef CONFIG_SOC_PRJ008
static int isp_clkv = 340000000;
#elif defined(CONFIG_SOC_PRJ007)
static int isp_clkv = 500000000;
#endif
module_param(isp_clkv, int, S_IRUGO);
MODULE_PARM_DESC(isp_clkv, "axi clock freq");

#ifdef CONFIG_SOC_PRJ008
static int isp_clkl = 357000000;
#elif defined(CONFIG_SOC_PRJ007)
static int isp_clkl = 588000000;
#endif
module_param(isp_clkl, int, S_IRUGO);
MODULE_PARM_DESC(isp_clkl, "ldc clock freq");

static int isp_memopt = 5;
module_param(isp_memopt, int, S_IRUGO);
MODULE_PARM_DESC(isp_memopt, "isp memory optimize");

extern int isp_ch0_pre_dequeue_time;
module_param(isp_ch0_pre_dequeue_time, int, S_IRUGO);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_time, "isp pre dequeue time, unit ms");

int direct_mode = 0;
module_param(direct_mode, int, S_IRUGO);
MODULE_PARM_DESC(direct_mode, "isp direct mode");

int ivdc_mem_line = 0;
module_param(ivdc_mem_line, int, S_IRUGO);
MODULE_PARM_DESC(ivdc_mem_line, "ivdc mem line");

#ifdef CONFIG_SOC_PRJ008
int ivdc_mem_stride = 3200;
#elif defined(CONFIG_SOC_PRJ007)
int ivdc_mem_stride = 3840;
#endif
module_param(ivdc_mem_stride, int, S_IRUGO);
MODULE_PARM_DESC(ivdc_mem_stride, "ivdc mem stride");

int ivdc_threshold_line = 0;
module_param(ivdc_threshold_line, int, S_IRUGO);
MODULE_PARM_DESC(ivdc_threshold_line, "ivdc threshold line");

#ifdef CONFIG_ZERATUL
int zeratul_open = 1;
int kernel_encode = 1;
#else
int zeratul_open = 0;
#endif  /* CONFIG_ZERATUL */

module_param(zeratul_open, int, S_IRUGO);
MODULE_PARM_DESC(zeratul_open, "zeratul open");

#ifdef CONFIG_PM
int atlas_open = 1;
#else
int atlas_open = 0;
#endif

module_param(atlas_open, int, S_IRUGO);
MODULE_PARM_DESC(atlas_open, "atlas open");

int isp_cache_line = 0;
module_param(isp_cache_line, int, S_IRUGO);
MODULE_PARM_DESC(isp_cache_line, "isp cache line");

static int streamoff_mode = 0;
module_param(streamoff_mode, int, S_IRUGO);
MODULE_PARM_DESC(streamoff_mode, "sensor streamoff mode");

#ifdef CONFIG_ZERATUL
struct module *isp_module = NULL;
#else
struct module *isp_module = THIS_MODULE;
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_SOC_PRJ008
char *sclk_name[2] = {"sclka", "mpll"};
#elif defined(CONFIG_SOC_PRJ007)
char *sclk_name[3] = {"sclka", "mpll", "vpll"};
#endif


int isp_printf(unsigned int level, unsigned char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r = 0;

	if(level >= print_level){
		va_start(args, fmt);

		vaf.fmt = fmt;
		vaf.va = &args;

		r = printk("%pV",&vaf);
		va_end(args);
		if(level >= ISP_ERROR_LEVEL)
			dump_stack();
	}
	return r;
}
EXPORT_SYMBOL(isp_printf);

char *get_clk_name(void)
{
	return clk_name;
}

char *get_clks_name(void)
{
	return clks_name;
}

char *get_clka_name(void)
{
	return clka_name;
}

char *get_clkv_name(void)
{
	return clkv_name;
}

char *get_clkl_name(void)
{
	return clkl_name;
}

int get_isp_clk(void)
{
	return isp_clk;
}

int get_isp_clks(void)
{
	return isp_clks;
}

int get_isp_clka(void)
{
	return isp_clka;
}

int get_isp_clkv(void)
{
	return isp_clkv;
}

int get_isp_clkl(void)
{
	return isp_clkl;
}

int get_isp_memopt(void)
{
	return isp_memopt;
}

int get_isp_cache_line(void)
{
	return isp_cache_line;
}

int get_isp_streamoff_mode(void)
{
	return streamoff_mode;
}
/* platform interfaces */
int private_platform_driver_register(struct platform_driver *drv)
{
#ifdef CONFIG_ZERATUL
#ifdef CONFIG_KERNEL_4_4_94
	return __platform_driver_register(drv, NULL);
#endif  /* CONFIG_KERNEL_4_4_94 */
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_5_15)
	return platform_driver_register(drv);
#endif  /* CONFIG_KERNEL_3_10 || CONFIG_KERNEL_5_15 */
#else
	return platform_driver_register(drv);
#endif  /* CONFIG_ZERATUL */
}

void private_platform_driver_unregister(struct platform_driver *drv)
{
	platform_driver_unregister(drv);
}

void private_platform_set_drvdata(struct platform_device *pdev, void *data)
{
	platform_set_drvdata(pdev, data);
}

void *private_platform_get_drvdata(struct platform_device *pdev)
{
	return platform_get_drvdata(pdev);
}

int private_platform_device_register(struct platform_device *pdev)
{
	return platform_device_register(pdev);
}


void private_platform_device_unregister(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
}

struct resource *private_platform_get_resource(struct platform_device *dev,
					       unsigned int type, unsigned int num)
{
	return platform_get_resource(dev, type, num);
}

void private_dev_set_drvdata(struct device *dev, void *data)
{
	dev_set_drvdata(dev, data);
}

void* private_dev_get_drvdata(const struct device *dev)
{
	return dev_get_drvdata(dev);
}

int private_platform_get_irq(struct platform_device *dev, unsigned int num)
{
	return platform_get_irq(dev, num);
}

struct resource * private_request_mem_region(resource_size_t start, resource_size_t n,
					     const char *name)
{
	return request_mem_region(start, n, name);
}

void private_release_mem_region(resource_size_t start, resource_size_t n)
{
	release_mem_region(start, n);
}

void __iomem * private_ioremap(phys_addr_t offset, unsigned long size)
{
	return ioremap(offset, size);
}

void private_iounmap(const volatile void __iomem *addr)
{
	iounmap(addr);
}

/* interrupt interfaces */
int private_request_threaded_irq(unsigned int irq, irq_handler_t handler,
				 irq_handler_t thread_fn, unsigned long irqflags,
				 const char *devname, void *dev_id)
{
	return request_threaded_irq(irq, handler, thread_fn, irqflags, devname, dev_id);
}

int private_request_irq(unsigned int irq, irq_handler_t handler,
				 unsigned long irqflags, const char *devname, void *dev_id)
{
	return request_irq(irq, handler, irqflags, devname, dev_id);
}

void private_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void private_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void private_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

/* lock and mutex interfaces */
void __private_spin_lock_irqsave(spinlock_t *lock, unsigned long *flags)
{
	raw_spin_lock_irqsave(spinlock_check(lock), *flags);
}

void private_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
	spin_unlock_irqrestore(lock, flags);
}

void private_spin_lock_init(spinlock_t *lock)
{
	spin_lock_init(lock);
}

void private_mutex_lock(struct mutex *lock)
{
	mutex_lock(lock);
}

void private_mutex_unlock(struct mutex *lock)
{
	mutex_unlock(lock);
}

void private_raw_mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key)
{
	__mutex_init(lock, name, key);
}

/* clock interfaces */
struct clk * private_clk_get(struct device *dev, const char *id)
{
	return clk_get(dev, id);
}
EXPORT_SYMBOL(private_clk_get);

struct clk * private_devm_clk_get(struct device *dev, const char *id)
{
	return devm_clk_get(dev, id);
}
EXPORT_SYMBOL(private_devm_clk_get);

int private_clk_enable(struct clk *clk)
{
	return clk_enable(clk);
}
EXPORT_SYMBOL(private_clk_enable);

int private_clk_prepare_enable(struct clk *clk)
{
#ifdef CONFIG_KERNEL_3_10
	return clk_enable(clk);
#endif
#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
	return clk_prepare_enable(clk);
#endif
}
EXPORT_SYMBOL(private_clk_prepare_enable);

#if 0
int private_clk_is_enabled(struct clk *clk)
{
	return clk_is_enabled(clk);
}
#endif

void private_clk_disable(struct clk *clk)
{
	clk_disable(clk);
}
EXPORT_SYMBOL(private_clk_disable);

void private_clk_disable_unprepare(struct clk *clk)
{
#ifdef CONFIG_KERNEL_3_10
	clk_disable(clk);
#endif
#ifdef CONFIG_KERNEL_4_4_94
	clk_disable_unprepare(clk);
#endif
}
EXPORT_SYMBOL(private_clk_disable_unprepare);

unsigned long private_clk_get_rate(struct clk *clk)
{
	return clk_get_rate(clk);
}
EXPORT_SYMBOL(private_clk_get_rate);

void private_clk_put(struct clk *clk)
{
	return clk_put(clk);
}
EXPORT_SYMBOL(private_clk_put);

void private_devm_clk_put(struct device *dev, struct clk *clk)
{
	return devm_clk_put(dev, clk);
}
EXPORT_SYMBOL(private_devm_clk_put);

int private_clk_set_rate(struct clk *clk, unsigned long rate)
{
	return clk_set_rate(clk, rate);
}
EXPORT_SYMBOL(private_clk_set_rate);

/* i2c interfaces */
struct i2c_adapter* private_i2c_get_adapter(int nr)
{
	return i2c_get_adapter(nr);
}

void private_i2c_put_adapter(struct i2c_adapter *adap)
{
	i2c_put_adapter(adap);
}

int private_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	return i2c_transfer(adap, msgs, num);
}
EXPORT_SYMBOL(private_i2c_transfer);

int private_i2c_register_driver(struct module *owner, struct i2c_driver *driver)
{
#ifdef CONFIG_ZERATUL
	return i2c_register_driver(NULL, driver);
#else
	return i2c_register_driver(owner, driver);
#endif  /* CONFIG_ZERATUL */
}

void private_i2c_del_driver(struct i2c_driver *drv)
{
	i2c_del_driver(drv);
}
EXPORT_SYMBOL(private_i2c_del_driver);

struct i2c_client *private_i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
{
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
	return i2c_new_device(adap, info);
#endif
#ifdef CONFIG_KERNEL_5_15
	return i2c_new_client_device(adap, info);
#endif
}

void private_i2c_set_clientdata(struct i2c_client *dev, void *data)
{
	i2c_set_clientdata(dev, data);
}
EXPORT_SYMBOL(private_i2c_set_clientdata);

void *private_i2c_get_clientdata(const struct i2c_client *dev)
{
	return i2c_get_clientdata(dev);
}
EXPORT_SYMBOL(private_i2c_get_clientdata);

int private_i2c_add_driver(struct i2c_driver *drv)
{
	return i2c_add_driver(drv);
}
EXPORT_SYMBOL(private_i2c_add_driver);

void private_i2c_unregister_device(struct i2c_client *client)
{
	i2c_unregister_device(client);
}

/* gpio interfaces */
int private_gpio_request(unsigned gpio, const char *label)
{
	return gpio_request(gpio, label);
}
EXPORT_SYMBOL(private_gpio_request);

void private_gpio_free(unsigned gpio)
{
	gpio_free(gpio);
}
EXPORT_SYMBOL(private_gpio_free);

int private_gpio_direction_output(unsigned gpio, int value)
{
	return gpio_direction_output(gpio, value);
}
EXPORT_SYMBOL(private_gpio_direction_output);

int private_gpio_direction_input(unsigned gpio)
{
	return gpio_direction_input(gpio);
}

int private_gpio_set_debounce(unsigned gpio, unsigned debounce)
{
	return gpio_set_debounce(gpio, debounce);
}

int private_jzgpio_set_func(enum gpio_port port, enum gpio_function func,unsigned long pins)
{
	return jzgpio_set_func(port, func, pins);
}
EXPORT_SYMBOL(private_jzgpio_set_func);

#if 0
int private_jzgpio_ctrl_pull(enum gpio_port port, int enable_pull,unsigned long pins)
{
	return jzgpio_ctrl_pull(port, enable_pull,pins);
}
#endif

/* system interfaces */
void private_msleep(unsigned int msecs)
{
	msleep(msecs);
}
EXPORT_SYMBOL(private_msleep);
void private_mdelay(unsigned int msecs)
{
	mdelay(msecs);
}
EXPORT_SYMBOL(private_mdelay);

bool private_capable(int cap)
{
	return capable(cap);
}
EXPORT_SYMBOL(private_capable);

unsigned long long private_sched_clock(void)
{
	return sched_clock();
}

bool private_try_module_get(struct module *module)
{
	return try_module_get(module);
}

int private_request_module(bool wait, const char *fmt, ...)
{
	int ret = 0;
	struct va_format vaf;
	va_list args;
	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	ret =  __request_module(true,"%pV", &vaf);
	va_end(args);
	return ret;
}

void private_module_put(struct module *module)
{
	module_put(module);
}


/* wait interfaces */
void private_init_completion(struct completion *x)
{
	init_completion(x);
}

void private_complete(struct completion *x)
{
	complete(x);
}

int private_wait_for_completion_interruptible(struct completion *x)
{
	return wait_for_completion_interruptible(x);
}

unsigned long private_wait_for_completion_timeout(struct completion *x, unsigned long timeover)
{
	return wait_for_completion_timeout(x, timeover);
}

int private_wait_event_interruptible(wait_queue_head_t *q, int (*state)(void *), void *data)
{
	return wait_event_interruptible((*q), state(data));
}

void private_wake_up_all(wait_queue_head_t *q)
{
	wake_up_all(q);
}

void private_wake_up(wait_queue_head_t *q)
{
	wake_up(q);
}

void private_init_waitqueue_head(wait_queue_head_t *q)
{
	init_waitqueue_head(q);
}


/* misc driver interfaces */
int private_misc_register(struct miscdevice *mdev)
{
	return misc_register(mdev);
}

void private_misc_deregister(struct miscdevice *mdev)
{
	misc_deregister(mdev);
}

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode,
                                                struct proc_dir_entry *parent,
                                                const struct file_operations *proc_fops,
                                                void *data)
#endif
#ifdef CONFIG_KERNEL_5_15
struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode,
                                                struct proc_dir_entry *parent,
                                                const struct proc_ops *proc_fops,
                                                void *data)
#endif
{
	return proc_create_data(name, mode, parent, proc_fops, data);
}

//malloc
#ifndef MALLOC_CHECK
void *private_vmalloc(unsigned long size)
{
	void *addr = NULL;
	if (size >= 4 * 1024) {
		addr = vmalloc(size);
	} else {
		addr = kmalloc(size, GFP_KERNEL);
	}
	return addr;
}

void private_vfree(const void *addr)
{
	if (is_vmalloc_addr(addr)) {
		vfree(addr);
	} else {
		kfree(addr);
	}
}

void * private_kmalloc(size_t s, gfp_t gfp)
{
	void *addr = kmalloc(s, gfp);
	return addr;
}

void private_kfree(void *p)
{
	kfree(p);
}
#else
void *test_malloc_addr = 0;
#endif /* MALLOC_CHECK */

//copy user
long private_copy_from_user(void *to, const void __user *from, long size)
{
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
	if (!access_ok(VERIFY_READ, from, size)) {
		memcpy(to, from, size);
		return 0;
	}
#endif
#if defined(CONFIG_KERNEL_5_15)
	if (!access_ok(from, size)) {
		memcpy(to, from, size);
		return 0;
	}
#endif
	return copy_from_user(to, from,size);
}

long private_copy_to_user(void __user *to, const void *from, long size)
{
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
	if (!access_ok(VERIFY_WRITE, to, size)) {
		memcpy(to, from, size);
		return 0;
	}
#endif
#if defined(CONFIG_KERNEL_5_15)
	if (!access_ok(to, size)) {
		memcpy(to, from, size);
		return 0;
	}
#endif
	return copy_to_user(to, from, size);
}

/* file ops */
struct file *private_filp_open(const char *filename, int flags, umode_t mode)
{
	return filp_open(filename, flags, mode);
}

int private_filp_close(struct file *filp, fl_owner_t id)
{
	return filp_close(filp, id);
}

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
ssize_t private_vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	return vfs_read(file, buf, count, pos);
}
ssize_t private_vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	return vfs_write(file, buf, count, pos);
}
#endif
#ifdef CONFIG_KERNEL_5_15
ssize_t private_vfs_read(struct file *file, void *buf, size_t count, loff_t *pos)
{
	return kernel_read(file, buf, count, pos);
}
ssize_t private_vfs_write(struct file *file, const void *buf, size_t count, loff_t *pos)
{
	return kernel_write(file, buf, count, pos);
}
#endif

loff_t private_vfs_llseek(struct file *file, loff_t offset, int whence)
{
	return vfs_llseek(file, offset, whence);
}

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
mm_segment_t private_get_fs(void)
{
	return get_fs();
}

void private_set_fs(mm_segment_t val)
{
	set_fs(val);
}
#endif

void private_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
			    enum dma_data_direction direction)
{
	dma_cache_sync(dev, vaddr, size, direction);
}

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
void private_getrawmonotonic(struct timespec *ts)
{
	getrawmonotonic(ts);
}
#endif
#ifdef CONFIG_KERNEL_5_15
void private_getrawmonotonic(struct timespec64 *ts)
{
        ktime_get_real_ts64(ts);
}
#endif

/* kthread interfaces */

bool private_kthread_should_stop(void)
{
	return kthread_should_stop();
}

struct task_struct* private_kthread_run(int (*threadfn)(void *data), void *data, const char namefmt[])
{
	return kthread_run(threadfn, data, namefmt);
}

int private_kthread_stop(struct task_struct *k)
{
	return kthread_stop(k);
}

/* proc file interfaces */
ssize_t private_seq_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	return seq_read(file, buf, size, ppos);
}

loff_t private_seq_lseek(struct file *file, loff_t offset, int whence)
{
	return seq_lseek(file, offset, whence);
}

int private_single_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

int private_single_open_size(struct file *file, int (*show)(struct seq_file *, void *), void *data, size_t size)
{
	return single_open_size(file, show, data, size);
}

struct proc_dir_entry* private_jz_proc_mkdir(char *s)
{
	return jz_proc_mkdir(s);
}

void private_proc_remove(struct proc_dir_entry *de)
{
	proc_remove(de);
}

void private_seq_printf(struct seq_file *m, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r = 0;
	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	seq_printf(m, "%pV", &vaf);
	r = m->count;
	va_end(args);
}

unsigned long long private_simple_strtoull(const char *cp, char **endp, unsigned int base)
{
	return simple_strtoull(cp, endp,  base);
}
extern unsigned long ispmem_base;
extern unsigned long ispmem_size;

static void get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size)
{
	*phyaddr = ispmem_base;
	*size = ispmem_size;
}

void private_get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size)
{
	get_isp_priv_mem(phyaddr, size);
}

/* struct net *private_get_init_net(void) */
/* { */
/*      return get_init_net(); */
/* } */

ktime_t private_ktime_set(const long secs, const unsigned long nsecs)
{
	return ktime_set(secs, nsecs);
}

void private_set_current_state(unsigned int state)
{
	__set_current_state(state);
	return;
}


int private_schedule_hrtimeout(ktime_t *ex, const enum hrtimer_mode mode)
{
	return schedule_hrtimeout(ex, mode);
}

bool private_schedule_work(struct work_struct *work)
{
	return schedule_work(work);
}

bool private_flush_work(struct work_struct *work)
{
	return flush_work(work);
}

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
void private_do_gettimeofday(struct timeval *tv)
{
	do_gettimeofday(tv);
	return;
}
#endif
#ifdef CONFIG_KERNEL_5_15
void private_do_gettimeofday(struct timeval *tv)
{
	struct timespec64 ts;
	ktime_get_ts64(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	return;
}
#endif

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
struct proc_dir_entry *private_proc_create(const char *name, umode_t mode, struct proc_dir_entry *parent, const struct file_operations *proc_fops)
{
    return  proc_create(name, mode, parent, proc_fops);
}
#endif

#ifdef CONFIG_KERNEL_5_15
struct proc_dir_entry *private_proc_create(const char *name, umode_t mode, struct proc_dir_entry *parent, const struct proc_ops *proc_fops)
{
    return  proc_create(name, mode, parent, proc_fops);
}
#endif

void private_remove_proc_entry(const char *name, struct proc_dir_entry *parent)
{
    return remove_proc_entry(name, parent);
}
