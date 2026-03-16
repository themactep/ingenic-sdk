#include "vdec-funcs.h"

/* platform interfaces */
int private_platform_driver_register(struct platform_driver *drv)
{
	return platform_driver_register(drv);
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

struct platform_device *private_platform_device_register_full(const struct platform_device_info *info)
{
	return platform_device_register_full(info);
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
void private_spin_lock_irqsave(spinlock_t *lock, unsigned long *flags)
{
	spin_lock_irqsave(lock, *flags);
}

void __private_spin_lock_irqsave(spinlock_t *lock, unsigned long *flags)
{
	raw_spin_lock_irqsave(spinlock_check(lock), *flags);
}

void private_spin_unlock_irqrestore(spinlock_t *lock, unsigned long *flags)
{
	spin_unlock_irqrestore(lock, *flags);
}

void private_spin_lock(spinlock_t *lock)
{
	spin_lock(lock);
}

void private_spin_unlock(spinlock_t *lock)
{
	spin_unlock(lock);
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

struct clk * private_devm_clk_get(struct device *dev, const char *id)
{
	return devm_clk_get(dev, id);
}

int private_clk_enable(struct clk *clk)
{
	return clk_enable(clk);
}

int private_clk_prepare_enable(struct clk *clk)
{
	return clk_prepare_enable(clk);
}

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

void private_clk_disable_unprepare(struct clk *clk)
{
	clk_disable_unprepare(clk);
}

unsigned long private_clk_get_rate(struct clk *clk)
{
	return clk_get_rate(clk);
}

void private_clk_put(struct clk *clk)
{
	return clk_put(clk);
}

void private_devm_clk_put(struct device *dev, struct clk *clk)
{
	return devm_clk_put(dev, clk);
}

int private_clk_set_rate(struct clk *clk, unsigned long rate)
{
	return clk_set_rate(clk, rate);
}

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

int private_i2c_register_driver(struct module *owner, struct i2c_driver *driver)
{
	return i2c_register_driver(owner, driver);
}

void private_i2c_del_driver(struct i2c_driver *drv)
{
	i2c_del_driver(drv);
}

struct i2c_client *private_i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
{
	return i2c_new_device(adap, info);
}

void private_i2c_set_clientdata(struct i2c_client *dev, void *data)
{
	i2c_set_clientdata(dev, data);
}

void *private_i2c_get_clientdata(const struct i2c_client *dev)
{
	return i2c_get_clientdata(dev);
}

int private_i2c_add_driver(struct i2c_driver *drv)
{
	return i2c_add_driver(drv);
}

void private_i2c_unregister_device(struct i2c_client *client)
{
	i2c_unregister_device(client);
}

/* gpio interfaces */
int private_gpio_request(unsigned gpio, const char *label)
{
	return gpio_request(gpio, label);
}

void private_gpio_free(unsigned gpio)
{
	gpio_free(gpio);
}

int private_gpio_direction_output(unsigned gpio, int value)
{
	return gpio_direction_output(gpio, value);
}

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

bool private_capable(int cap)
{
	return capable(cap);
}

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

int private_wait_event_interruptible1(wait_queue_head_t *q, int (*state)(void *), void *data)
{
	return wait_event_interruptible(*q, state(data));
}

int private_wait_event_interruptible2(wait_queue_head_t *q, int (*state)(void *, void *), void *data1, void *data2)
{
	return wait_event_interruptible(*q, state(data1, data2));
}

int private_wait_event_interruptible3(wait_queue_head_t *q, int (*state)(void *, void *, void *), void *data1, void *data2, void *data3)
{
	return wait_event_interruptible(*q, state(data1, data2, data3));
}

int private_wait_event_interruptible4(wait_queue_head_t *q, int (*state)(void *, void *, void *, void *), void *data1, void *data2, void *data3, void *data4)
{
	return wait_event_interruptible(*q, state(data1, data2, data3, data4));
}

int private_wait_event_interruptible_timeout1(wait_queue_head_t *q, int (*state)(void *), void *data, long timeout)
{
	return wait_event_interruptible_timeout(*q, state(data), timeout);
}

int private_wait_event_interruptible_timeout2(wait_queue_head_t *q, int (*state)(void *, void *), void *data1, void * data2, long timeout)
{
	return wait_event_interruptible_timeout(*q, state(data1, data2), timeout);
}

int private_wait_event_interruptible_timeout3(wait_queue_head_t *q, int (*state)(void *, void *, void *), void *data1, void * data2, void *data3, long timeout)
{
	return wait_event_interruptible_timeout(*q, state(data1, data2, data3), timeout);
}

void private_wake_up_interruptible_all(wait_queue_head_t *q)
{
	wake_up_interruptible_all(q);
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

struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode,
						struct proc_dir_entry *parent,
						const struct file_operations *proc_fops,
						void *data)
{
	return proc_create_data(name, mode, parent, proc_fops, data);
}

//malloc
void *private_vmalloc(unsigned long size)
{
	void *addr = vmalloc(size);
	return addr;
}

void private_vfree(const void *addr)
{
	vfree(addr);
}

void * private_kmalloc(size_t s, gfp_t gfp)
{
	void *addr = kmalloc(s, gfp);
	return addr;
}

void private_kfree(void *p){
	kfree(p);
}

//copy user
long private_copy_from_user(void *to, const void __user *from, long size)
{
	return copy_from_user(to, from,size);
}

long private_copy_to_user(void __user *to, const void *from, long size)
{
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

ssize_t private_vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	return vfs_read(file, buf, count, pos);
}
ssize_t private_vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	return vfs_write(file, buf, count, pos);
}

loff_t private_vfs_llseek(struct file *file, loff_t offset, int whence)
{
	return vfs_llseek(file, offset, whence);
}

mm_segment_t private_get_fs(void)
{
	return get_fs();
}

void private_set_fs(mm_segment_t val)
{
	set_fs(val);
}

void private_dma_cache_sync(struct device *dev, void *vaddr, size_t size, int direction)
{
	dma_cache_sync(dev, vaddr, size, direction);
}

void private_getrawmonotonic(struct timespec *ts)
{
	getrawmonotonic(ts);
}

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

/* struct net *private_get_init_net(void) */
/* { */
/* 	return get_init_net(); */
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


void private_do_gettimeofday(struct timeval *tv)
{
	do_gettimeofday(tv);
	return;
}

void private_init_timer(struct timer_list *timer)
{
	init_timer(timer);
	return;
}

void private_add_timer(struct timer_list *timer)
{
	add_timer(timer);
	return;
}

int private_mod_timer(struct timer_list *timer, unsigned long expires)
{
	return mod_timer(timer, expires);
}

void private_setup_timer(struct timer_list *timer, void *callback, unsigned int flag)
{
	setup_timer(timer, callback, flag);
	return;
}

void private_del_timer(struct timer_list *timer)
{
	del_timer(timer);
	return;
}

void __iomem * private_ioremap_nocache(unsigned long offset, unsigned long size)
{
	return ioremap_nocache(offset, size);
}

unsigned int private_ioread32(void __iomem *addr)
{
	return ioread32(addr);
}

unsigned int private_readl(const volatile void __iomem *addr)
{
	return readl(addr);
}

void private_iowrite32(unsigned int val, void __iomem *addr)
{
	iowrite32(val, addr);
	return;
}

void private_writel(unsigned int b, volatile void __iomem *addr)
{
	writel(b, addr);
	return;
}

void private_sema_init(struct semaphore *sem, int val)
{
	sema_init(sem, val);
	return;
}

void private_atomic_inc(atomic_t *v)
{
	atomic_inc(v);
	return;
}

void private_up(struct semaphore *sem)
{
	up(sem);
	return;
}

int private_down_interruptible(struct semaphore *sem)
{
	return down_interruptible(sem);
}

int private_atomic_read(atomic_t * v)
{
	return atomic_read(v);
}

int private_request_irq(unsigned int irq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id)
{
	return request_irq(irq, handler, irqflags, devname, dev_id);
}

void *private_dma_alloc_coherent(struct device *dev, size_t size,dma_addr_t *dma_handle, gfp_t gfp)
{
	return dma_alloc_coherent(dev, size, dma_handle, gfp);
}

void private_dma_free_coherent(struct device *dev, size_t size, void *vaddr, dma_addr_t dma_handle)
{
	dma_free_coherent(dev, size, vaddr, dma_handle);
	return;
}

void private_mdelay(unsigned long ms)
{
	mdelay(ms);
}

int private_printk(const char *fmt, ...)
{
	va_list args;
	struct va_format vaf;
	int r = 0;
	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;
	r = printk("%pV", &vaf);
	va_end(args);
	return r;
}
