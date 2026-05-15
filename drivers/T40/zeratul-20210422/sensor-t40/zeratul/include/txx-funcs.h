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
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/completion.h>
#include <net/netlink.h>
#include <linux/spi/spi.h>

// #include <soc/irq.h>
#include <dt-bindings/interrupt-controller/t40-irq.h>

#include <soc/base.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <soc/gpio.h>
// #include <mach/platform.h>
/*#include <linux/seq_file.h>*/
#include <jz_proc.h>

#ifndef U16_MAX
#define U16_MAX 					0xFFFF
#endif

#define ISP_INFO_LEVEL		0x0
#define ISP_WARNING_LEVEL	0x1
#define ISP_ERROR_LEVEL		0x2
#define ISP_PRINT(level, format, ...)			\
	isp_printf(level, format, ##__VA_ARGS__)
#define ISP_INFO(...) ISP_PRINT(ISP_INFO_LEVEL, __VA_ARGS__)
#define ISP_WARNING(...) ISP_PRINT(ISP_WARNING_LEVEL, __VA_ARGS__)
#define ISP_ERROR(...) ISP_PRINT(ISP_ERROR_LEVEL, __VA_ARGS__)

//extern unsigned int isp_print_level;
/*int isp_debug_init(void);*/
/*int isp_debug_deinit(void);*/
int isp_printf(unsigned int level, unsigned char *fmt, ...);
int get_isp_clk(void);
void *private_vmalloc(unsigned long size);
void private_vfree(const void *addr);

#define paddr2vaddr(phyaddr) ((void *)((phyaddr) + PAGE_OFFSET - PHYS_OFFSET))

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
void private_dev_set_drvdata(struct device *dev, void *data);
void* private_dev_get_drvdata(const struct device *dev);
int private_platform_get_irq(struct platform_device *dev, unsigned int num);
struct resource * private_request_mem_region(resource_size_t start, resource_size_t n,
					     const char *name);
void private_release_mem_region(resource_size_t start, resource_size_t n);

void __iomem * private_ioremap(phys_addr_t offset, unsigned long size);
void private_iounmap(const volatile void __iomem *addr);
/* interrupt interfaces */
int private_request_threaded_irq(unsigned int irq, irq_handler_t handler,
				 irq_handler_t thread_fn, unsigned long irqflags,
				 const char *devname, void *dev_id);
void private_enable_irq(unsigned int irq);
void private_disable_irq(unsigned int irq);
void private_free_irq(unsigned int irq, void *dev_id);

/* lock and mutex interfaces */
void __private_spin_lock_irqsave(spinlock_t *lock, unsigned long *flags);
#define private_spin_lock_irqsave(lock, flags) \
			__private_spin_lock_irqsave(lock, (&flags));
void private_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);
void private_spin_lock_init(spinlock_t *lock);
void private_mutex_lock(struct mutex *lock);
void private_mutex_unlock(struct mutex *lock);
void private_raw_mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key);

#define private_mutex_init(mutex) \
	do {								\
	static struct lock_class_key __key;		\
							\
	private_raw_mutex_init((mutex), #mutex, &__key);		\
} while (0)

/* clock interfaces */
struct clk * private_clk_get(struct device *dev, const char *id);
int private_clk_enable(struct clk *clk);
void private_clk_disable(struct clk *clk);
unsigned long private_clk_get_rate(struct clk *clk);
void private_clk_put(struct clk *clk);
int private_clk_set_rate(struct clk *clk, unsigned long rate);

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

/* system interfaces */
void private_msleep(unsigned int msecs);
bool private_capable(int cap);
unsigned long long private_sched_clock(void);
bool private_try_module_get(struct module *module);
int private_request_module(bool wait, const char *fmt, ...);
void private_module_put(struct module *module);

/* wait interfaces */
void private_init_completion(struct completion *x);
void private_complete(struct completion *x);
int private_wait_for_completion_interruptible(struct completion *x);
unsigned long private_wait_for_completion_timeout(struct completion *x, unsigned long timeover);
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
struct proc_dir_entry* private_jz_proc_mkdir(char *s);
void private_proc_remove(struct proc_dir_entry *de);
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

/* netlink */
struct sk_buff* private_nlmsg_new(size_t payload, gfp_t flags);
struct nlmsghdr *private_nlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
					 int type, int payload, int flags);
int private_netlink_unicast(struct sock *ssk, struct sk_buff *skb,
		    u32 portid, int nonblock);
struct sock *private_netlink_kernel_create(struct net *net, int unit, struct netlink_kernel_cfg *cfg);
void private_sock_release(struct socket *sock);

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

/* isp driver interface */
void private_get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size);

/* int private_driver_get_interface(void); */

#endif /*__TXX_DRV_FUNCS_H__*/
