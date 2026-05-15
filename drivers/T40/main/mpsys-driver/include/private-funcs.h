#ifndef __PRIVATE_FUNCS_H__
#define __PRIVATE_FUNCS_H__
#include <linux/init.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <jz_proc.h>

/* semaphore and mutex interfaces */
int private_down_interruptible(struct semaphore *sem);
void private_up(struct semaphore *sem);
void private_mutex_lock(struct mutex *lock);
void private_mutex_unlock(struct mutex *lock);
int private_mutex_lock_interruptible(struct mutex *lock);

/* wait interfaces */
void private_init_completion(struct completion *x);
void private_complete(struct completion *x);
int private_wait_for_completion_interruptible(struct completion *x);
int private_wait_event_interruptible(wait_queue_head_t *wq, int (* state)(void));
void private_wake_up(wait_queue_head_t *q);

/* mem ops */
void* private_kzalloc(size_t s, gfp_t gfp);
void private_kfree(void *p);
long private_copy_from_user(void *to, const void __user *from, long size);
long private_copy_to_user(void __user *to, const void *from, long size);

/* file ops */
struct file* private_filp_open(const char *, int, umode_t);
int private_filp_close(struct file *filp, fl_owner_t id);
int private_kernel_read(struct file *file, loff_t offset, char *addr, unsigned long count);

/* string ops */
size_t private_strlen(const char *s);
int private_kstrtoint(const char *s, unsigned int base, int *res);
char* private_strstr(const char *s1, const char *s2);

/* misc driver interfaces */
int private_misc_register(struct miscdevice *misc);
void private_misc_deregister(struct miscdevice *misc);

/* system interfaces */
void private_msleep(unsigned int msecs);

/* proc file interfaces */
struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode, struct proc_dir_entry *parent,const struct file_operations *proc_fops, void *data);
ssize_t private_seq_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
loff_t private_seq_lseek(struct file *file, loff_t offset, int whence);
int private_single_release(struct inode *inode, struct file *file);
int private_single_open_size(struct file *file, int (*show)(struct seq_file *, void *), void *data, size_t size);
struct proc_dir_entry* private_jz_proc_mkdir(char *s);
void private_proc_remove(struct proc_dir_entry *de);
void private_remove_proc_entry(const char *name, struct proc_dir_entry *parent);
void private_seq_printf(struct seq_file *m, const char *f, ...);

#endif
