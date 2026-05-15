/*
 *
 * Copyright (C) 2017 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <txx-funcs.h>
#include <tx-isp-debug.h>

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

/* lock and mutex interfaces */
void  __private_spin_lock_irqsave(spinlock_t *lock, unsigned long *flags)
{
	_raw_spin_lock_irqsave(spinlock_check(lock), *flags);
}

void  private_spin_lock_init(spinlock_t *lock)
{
	spin_lock_init(lock);
}

void  private_raw_mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key)
{
	__mutex_init(lock, name, key);
}

int  private_request_module(bool wait, const char *fmt, ...)
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

int  private_wait_event_interruptible(wait_queue_head_t *q, int (*state)(void *), void *data)
{
	return wait_event_interruptible((*q), state(data));
}

void  private_wake_up_all(wait_queue_head_t *q)
{
	wake_up_all(q);
}

void  private_wake_up(wait_queue_head_t *q)
{
	wake_up(q);
}

void  private_init_waitqueue_head(wait_queue_head_t *q)
{
	init_waitqueue_head(q);
}

unsigned long  private_wait_for_completion_timeout(struct completion *x, unsigned long timeout)
{
	return wait_for_completion_timeout(x, timeout);
}

void  private_proc_remove(struct proc_dir_entry *de)
{
	return proc_remove(de);
}

int  private_seq_printf(struct seq_file *m, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r = 0;
	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	r = seq_printf(m, "%pV", &vaf);
	va_end(args);
	return r;
}

unsigned long long  private_simple_strtoull(const char *cp, char **endp, unsigned int base)
{
	return simple_strtoull(cp, endp, base);
}

/* kthread */
bool  private_kthread_should_stop(void)
{
	return kthread_should_stop();
}

struct task_struct*  private_kthread_run(int (*threadfn)(void *data), void *data, const char namefmt[])
{
	return kthread_run(threadfn, data, namefmt);
}

int  private_kthread_stop(struct task_struct *k)
{
	return kthread_stop(k);
}

void *  private_kmalloc(size_t s, gfp_t gfp)
{
	return kmalloc(s, gfp);
}

void  private_kfree(void *p)
{
	kfree(p);
}

long  private_copy_from_user(void *to, const void __user *from, long size)
{
	return copy_from_user(to, from, size);
}

long  private_copy_to_user(void __user *to, const void *from, long size)
{
	return copy_to_user(to, from, size);
}

/* netlink */
struct sk_buff*  private_nlmsg_new(size_t payload, gfp_t flags)
{
	return nlmsg_new(payload, flags);
}

struct nlmsghdr*  private_nlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
					 int type, int payload, int flags)
{
	return nlmsg_put(skb, portid, seq, type, payload, flags);
}

int  private_netlink_unicast(struct sock *ssk, struct sk_buff *skb,
		    u32 portid, int nonblock)
{
	return netlink_unicast(ssk, skb, portid, nonblock);
}

struct sock *  private_netlink_kernel_create(struct net *net, int unit, struct netlink_kernel_cfg *cfg)
{
	return netlink_kernel_create(net, unit, cfg);
}

void  private_sock_release(struct socket *sock)
{
	return sock_release(sock);
}

/* file ops */
struct file *  private_filp_open(const char *filename, int flags, umode_t mode)
{
	return filp_open(filename, flags, mode);
}

int  private_filp_close(struct file *filp, fl_owner_t id)
{
	return filp_close(filp, id);
}

ssize_t  private_vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	return vfs_read(file, buf, count, pos);
}

ssize_t  private_vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	return vfs_write(file, buf, count, pos);
}

loff_t  private_vfs_llseek(struct file *file, loff_t offset, int whence)
{
	return vfs_llseek(file, offset, whence);
}

mm_segment_t  private_get_fs(void)
{
	return get_fs();
}

void  private_set_fs(mm_segment_t val)
{
	set_fs(val);
}

void  private_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
			 enum dma_data_direction direction)
{
	dma_cache_sync(dev, vaddr, size, direction);
}

void  private_dma_sync_single_for_device(struct device *dev, dma_addr_t addr, size_t size,
					      enum dma_data_direction dir)
{
	dma_sync_single_for_device(dev, addr, size, dir);
}

extern struct net init_net;
struct net* private_get_init_net(void)
{
	return &init_net;
}

void  private_getrawmonotonic(struct timespec *ts)
{
	return getrawmonotonic(ts);
}
