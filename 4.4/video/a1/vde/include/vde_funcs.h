#ifndef __VDE_FUNCS_H__
#define __VDE_FUNCS_H__
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <linux/file.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/mfd/core.h>
#include <linux/mempolicy.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/completion.h>
#include <linux/spi/spi.h>
#include <soc/base.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <soc/gpio.h>
#include <jz_proc.h>
#include <linux/hrtimer.h>

#ifndef U16_MAX
#define U16_MAX 					0xFFFF
#endif

#define VDE_INFO_LEVEL		0x0
#define VDE_WARNING_LEVEL	0x1
#define VDE_ERROR_LEVEL		0x2
#define VDE_PRINT(level, format, ...)			\
	vde_printf(level, format, ##__VA_ARGS__)
#define VDE_INFO(...) VDE_PRINT(VDE_INFO_LEVEL, __VA_ARGS__)
#define VDE_WARNING(...) VDE_PRINT(VDE_WARNING_LEVEL, __VA_ARGS__)
#define VDE_ERROR(...) VDE_PRINT(VDE_ERROR_LEVEL, __VA_ARGS__)
#define PRIVATE_DEFINE_TIMER DEFINE_TIMER

int vde_printf(unsigned int level, unsigned char *fmt, ...);
int get_vdea_clk(void);
int get_vga_pin_order(void);
int get_hdmi_hpd(void);
char* get_hdmi_ddc_gpio_func(void);
int get_tft_fmt(void);
int get_backlight_pin(void);
int get_backlight_status(void);
void *private_vmalloc(unsigned long size);
void private_vfree(const void *addr);
void *private_devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);
void private_devm_kfree(struct device *dev, void *p);

#define paddr2vaddr(phyaddr) ((void *)((phyaddr) + PAGE_OFFSET - PHYS_OFFSET))

/* platform interfaces */
int private_platform_driver_register(struct platform_driver *drv);
void private_platform_driver_unregister(struct platform_driver *drv);
void private_platform_set_drvdata(struct platform_device *pdev, void *data);
void *private_platform_get_drvdata(struct platform_device *pdev);
int private_platform_device_register(struct platform_device *pdev);
void private_platform_device_unregister(struct platform_device *pdev);
struct resource *private_platform_get_resource(struct platform_device *dev,
					       unsigned int type, unsigned int num);
void private_dev_set_drvdata(struct device *dev, void *data);
void* private_dev_get_drvdata(const struct device *dev);
int private_platform_get_irq(struct platform_device *dev, unsigned int num);
struct resource * private_request_mem_region(resource_size_t start, resource_size_t n,
					     const char *name);
void private_release_mem_region(resource_size_t start, resource_size_t n);

void __iomem * private_ioremap(phys_addr_t offset, unsigned long size);
void private_iounmap(const volatile void __iomem *addr);
void __iomem *private_devm_ioremap_resource(struct device *dev, struct resource *res);

/* interrupt interfaces */
int private_request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,const char *name, void *dev);
int private_request_threaded_irq(unsigned int irq, irq_handler_t handler,
				 irq_handler_t thread_fn, unsigned long irqflags,
				 const char *devname, void *dev_id);
void private_enable_irq(unsigned int irq);
void private_disable_irq(unsigned int irq);
void private_free_irq(unsigned int irq, void *dev_id);
void private_disable_irq_nosync(unsigned int irq);

/* lock and mutex interfaces */

void private_spin_lock(spinlock_t *lock);
void private_spin_unlock(spinlock_t *lock);
void __private_spin_lock_irqsave(spinlock_t *lock, unsigned long *flags);
#define private_spin_lock_irqsave(lock, flags)		\
	__private_spin_lock_irqsave(lock, (&flags));
void private_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);
void private_spin_lock_init(spinlock_t *lock);
int private_mutex_is_locked(struct mutex *lock);
void private_mutex_lock(struct mutex *lock);
void private_mutex_unlock(struct mutex *lock);
void private_raw_mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key);

#define private_mutex_init(mutex)					\
	do {								\
		static struct lock_class_key __key;			\
									\
		private_raw_mutex_init((mutex), #mutex, &__key);	\
	} while (0)

/* clock interfaces */

int private_clk_prepare(struct clk *clk);
void private_clk_unprepare(struct clk *clk);
struct clk * private_clk_get(struct device *dev, const char *id);
int private_clk_enable(struct clk *clk);
void private_clk_disable(struct clk *clk);
unsigned long private_clk_get_rate(struct clk *clk);
struct clk *private_clk_get_parent(struct clk *clk);
void private_clk_put(struct clk *clk);
int private_clk_set_rate(struct clk *clk, unsigned long rate);
long private_clk_round_rate(struct clk *clk, unsigned long rate);

struct clk *private_devm_clk_get(struct device *dev, const char *id);
int private_clk_prepare_enable(struct clk *clk);
void private_clk_disable_unprepare(struct clk *clk);
void private_devm_clk_put(struct device *dev, struct clk *clk);

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
// int private_jzgpio_ctrl_pull(enum gpio_port port, int enable_pull,unsigned long pins);
int private_gpio_to_irq(unsigned gpio);
int private_gpio_get_value(unsigned int gpio);

/* system interfaces */
void private_msleep(unsigned int msecs);
bool private_capable(int cap);
unsigned long long private_sched_clock(void);
bool private_try_module_get(struct module *module);
int private_request_module(bool wait, const char *fmt, ...);
void private_module_put(struct module *module);

/* wait interfaces */
void private_init_completion(struct completion *x);
void private_reinit_completion(struct completion *x);
void private_complete(struct completion *x);
void private_complete_all(struct completion *x);
int private_wait_for_completion_interruptible(struct completion *x);
unsigned long private_wait_for_completion_timeout(struct completion *x, unsigned long timeover);
long private_wait_for_completion_interruptible_timeout(struct completion *x,unsigned long timeout);
int private_wait_event_interruptible(wait_queue_head_t *q, int (*state)(void *), void *data);
void private_wake_up_all(wait_queue_head_t *q);
void private_wake_up(wait_queue_head_t *q);
void private_init_waitqueue_head(wait_queue_head_t *q);

/* misc driver interfaces */
int private_misc_register(struct miscdevice *mdev);
void private_misc_deregister(struct miscdevice *mdev);

struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode, struct proc_dir_entry *parent,const struct file_operations *proc_fops, void *data);
/* proc file interfaces */
ssize_t private_seq_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
loff_t private_seq_lseek(struct file *file, loff_t offset, int whence);
int private_single_release(struct inode *inode, struct file *file);
int private_single_open_size(struct file *file, int (*show)(struct seq_file *, void *),
			     void *data, size_t size);
int private_single_open(struct file *file, int (*show)(struct seq_file *, void *),void *data);
struct proc_dir_entry* private_jz_proc_mkdir(char *s);
void private_proc_remove(struct proc_dir_entry *de);
void private_remove_proc_entry(const char *name, struct proc_dir_entry *parent);
void private_seq_printf(struct seq_file *m, const char *f, ...);
unsigned long long private_simple_strtoull(const char *cp, char **endp, unsigned int base);

/* kthread interfaces */
bool private_kthread_should_stop(void);
struct task_struct* private_kthread_run(int (*threadfn)(void *data), void *data, const char namefmt[]);
int private_kthread_stop(struct task_struct *k);

void* private_kmalloc(size_t s, gfp_t gfp);
void private_kfree(void *p);
long private_copy_from_user(void *to, const void __user *from, long size);
long private_copy_to_user(void __user *to, const void *from, long size);

/* file ops */
struct file *private_filp_open(const char *filename, int flags, umode_t mode);
int private_filp_close(struct file *filp, fl_owner_t id);
ssize_t private_vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
ssize_t private_vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
loff_t private_vfs_llseek(struct file *file, loff_t offset, int whence);
mm_segment_t private_get_fs(void);
void private_set_fs(mm_segment_t val);
void private_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
			    enum dma_data_direction direction);
void private_getrawmonotonic(struct timespec *ts);
struct net *private_get_init_net(void);

ktime_t private_ktime_set(const long secs, const unsigned long nsecs);
void private_set_current_state(unsigned int state);
int private_schedule_hrtimeout(ktime_t *ex, const enum hrtimer_mode mode);
bool private_schedule_work(struct work_struct *work);
void private_do_gettimeofday(struct timeval *tv);
int private_fasync_helper(int fd, struct file * filp, int on, struct fasync_struct **fapp);
void private_kill_fasync(struct fasync_struct **fp, int sig, int band);

//of ops
const struct of_device_id *private_of_match_node(const struct of_device_id *matches,const struct device_node *node);
struct device_node *private_dev_of_node(struct device *dev);

int private_i2c_add_adapter(struct i2c_adapter *adapter);
void  private_i2c_del_adapter(struct i2c_adapter *adap);
void private_i2c_set_adapdata(struct i2c_adapter *dev, void *data);
void *private_i2c_get_adapdata(const struct i2c_adapter *dev);

/*timer interfaces*/
int private_mod_timer(struct timer_list *timer, unsigned long expires);
int private_del_timer(struct timer_list *timer);
#endif
