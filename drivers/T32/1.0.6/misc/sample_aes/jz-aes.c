/*
 * Cryptographic API.
 *
 * Support for Tseries AES HW.
 *
 * Copyright (c) 2019 Ingenic Corporation
 * Author: Niky shen <xianghui.shen@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 */
#include <asm/page.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/crypto.h>
#include <linux/interrupt.h>
#include <crypto/scatterwalk.h>
#include <crypto/aes.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/semaphore.h>

#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <soc/base.h>
#include <linux/time.h>
#if defined(CONFIG_KERNEL_4_4_94)
#include <dt-bindings/dma/ingenic-pdma.h>
#include <dt-bindings/interrupt-controller/PRJ007-irq.h>
#else
#include <soc/irq.h>
#endif
//#include <mach/platform.h>
#include "jz-aes.h"

/*#define DEBUG*/
#undef AES_EN_PROC_PRINT
#ifdef DEBUG
#define aes_debug(format, ...) {printk(format, ## __VA_ARGS__);}
#else
#define aes_debug(format, ...) do{ } while(0)
#endif
DEFINE_SPINLOCK(crypto_lock);

#define USRIRQ (1)
//struct semaphore crypto_sem;
#define AES_DRIVER_VERSION "H20250508a"
unsigned aes_done = 0;

s64 g_t0,g_t1,g_ioct0,g_ioct1,g_ioct2;

#ifdef DEBUG
static void debug_regs(struct aes_operation *aes)
{
	printk("AES_ASCR = 0x%08x\n", aes_reg_read(aes, AES_ASCR));
	printk("AES_ASSR = 0x%08x\n", aes_reg_read(aes, AES_ASSR));
	printk("AES_ASINTM = 0x%08x\n", aes_reg_read(aes, AES_ASINTM));
	printk("AES_ASSA = 0x%08x\n", aes_reg_read(aes, AES_ASSA));
	printk("AES_ASDA = 0x%08x\n", aes_reg_read(aes, AES_ASDA));
	printk("AES_ASTC = 0x%08x\n", aes_reg_read(aes, AES_ASTC));
}
#endif

static int debug_level = 0;
module_param(debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Debug level (0=off, 1=basic, 2=verbose)");

static void debug_print(const char *fmt, ...) {
	if (debug_level > 0) {
		va_list args;
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
	}
}

void ingenic_flush_cache(void)
{
	register unsigned long addr;
	/* Clear CP0 TagLo */
	asm volatile ("mtc0 $0, $28\n\t");
	for (addr = 0x80000000; addr < (0x80000000 + 32*1024);
			addr += 32) {
		asm volatile (".set mips32r2\n\t"
				" cache %0, 0(%1)\n\t"
				".set mips32r2\n\t"
				:
				: "I" (0x1), "r"(addr));
	}
	asm volatile (
		    "sync\n\t"
		    "lw $0,0(%0)"
		    ::"r" (0xa0000000));
}

static int aes_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);

	aes->state = 1;

	return 0;
}

static int aes_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);

	aes->state = 0;
	return 0;
}

static ssize_t aes_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t aes_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static inline s64 aes_getns(void){
	if(debug_level){
		ktime_t time = ktime_get_real(); // 获取实时时间
		return ktime_to_ns(time); // 转换为纳秒
	}
	return 0;
}

static int aes_start_cpu_encrypt_processing(struct aes_operation *aes)
{
	struct aes_para *para = &aes->para;
	int i = 0;
	int ret = 0;
	int offset = 0, len = 0;

	para->donelen = 0;
	para->status = JZ_AES_STATUS_PREPARE;

	if(para->datalen % 16){
		printk("The size of data should be 16bytes aligned!\n");
		return -EINVAL;
	}

	if(para->enworkmode >= IN_UNF_CIPHER_WORK_MODE_OTHER){
		printk("The enworkmode is invalid!\n");
		return -EINVAL;
	}

	para->status = JZ_AES_STATUS_WORKING;
	while(offset < para->datalen){
		len = para->datalen - offset > 16 ? 16 : para->datalen - offset;
		spin_lock(&crypto_lock);
		if(__copy_from_user_inatomic(aes->src_addr_v, (void __user *)(para->src + offset), len)){
		//if(copy_from_user(aes->src_addr_v, (void __user *)(para->src + offset), len)){
			ret = -EINVAL;
			spin_unlock(&crypto_lock);
			break;
		}
		spin_unlock(&crypto_lock);

		/* set transfer count */
		aes_reg_write(aes, AES_ASTC, len / 16);

		/* AES data input */
		spin_lock(&crypto_lock);
		for (i=0; i<len/4; i++) {
			//aes_reg_write(aes, AES_ASDI, ((int*)(aes->src_addr_v))[i]);
			*(volatile unsigned int *)(0xb3430018)= ((int*)(aes->src_addr_v))[i];
		}
		spin_unlock(&crypto_lock);

		/* start AES */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<3);

#if USRIRQ
		ret = wait_for_completion_interruptible_timeout(&aes->done_comp, msecs_to_jiffies(2000));
		if(!ret){
			printk("aes done outtime\n");
			ret = -EINVAL;
			break;
		}
#else
		/*wait interrupt*/
		while (!(aes_reg_read(aes, AES_ASSR) & 1<<1))
			usleep_range(1,2);

		/* clear interrupt */
		aes_reg_write(aes, AES_ASSR, 1<<1);
#endif
		/*for save one buf,use src buf*/
		for (i=0; i<len/4; i++) {
			((int*)(aes->src_addr_v))[i] =  aes_reg_read(aes, AES_ASDO);
		}

		spin_lock(&crypto_lock);
		// if (__copy_to_user_inatomic((void __user *)(para->dst + offset), aes->dst_addr_v, len)) {
		if (__copy_to_user_inatomic((void __user *)(para->dst + offset), aes->src_addr_v, len)) {
		// if (copy_to_user((void __user *)(para->dst + offset), aes->dst_addr_v, len)) {
			printk("%s[%d]:copy_to_user failed!\n",__func__,__LINE__);
			ret = -EFAULT;
			spin_unlock(&crypto_lock);
			break;
		}
		spin_unlock(&crypto_lock);
		offset += len;
		para->donelen = offset;
	}

	para->status = JZ_AES_STATUS_DONE;
	return 0;
}
static int aes_start_encrypt_processing(struct aes_operation *aes)
{
	struct aes_para *para = &aes->para;
	int ret = 0;
	int offset = 0, len = 0;
	unsigned long timeout;

	s64 T1,T2;
	para->donelen = 0;
	para->status = JZ_AES_STATUS_PREPARE;

	if(para->datalen % 16){
		printk("The size of data should be 16bytes aligned!\n");
		return -EINVAL;
	}

	if(para->enworkmode >= IN_UNF_CIPHER_WORK_MODE_OTHER){
		printk("The enworkmode is invalid!\n");
		return -EINVAL;
	}

	para->status = JZ_AES_STATUS_WORKING;
	while(offset < para->datalen){
		len = para->datalen - offset > aes->dma_datalen ? aes->dma_datalen : para->datalen - offset;

		T1 = aes_getns();

		dma_cache_sync(aes->dev, aes->src_addr_v, len, DMA_TO_DEVICE);
		// dma_cache_sync(aes->dev, aes->dst_addr_v, len, DMA_FROM_DEVICE);

		/* set transfer count len/16*/
		aes_reg_write(aes, AES_ASTC, len >> 4);

		/* start AES DMA */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<9);

		/*wait interrupt*/
#if USRIRQ
		timeout = msecs_to_jiffies(2000);
		ret = wait_for_completion_interruptible_timeout(&aes->done_comp,timeout);
		if(!ret){
			printk("aes done outtime\n");
			ret = -EINVAL;
			break;
		}
#else
		while (!(aes_reg_read(aes, AES_ASSR )& 1<<2));
			// usleep_range(1,2);

		/* clear interrupt */
		aes_reg_write(aes, AES_ASSR, 1<<2);
#endif
		T2 = aes_getns();

		offset += len;
		para->donelen = offset;

		debug_print("keydone:%lld ,aesdone = %lld,offset:%d\n",g_t1-g_t0,T2-T1,offset);

	}

	para->status = JZ_AES_STATUS_DONE;
	return 0;
}

static int jz_aes_cpu_init(struct file *file, struct aes_operation *aes)
{
	struct aes_para *para = &aes->para;
	int i, ret;
	int writekey_times, setkey_reg, key_byte;

	/* AES ENDIAN */
	AES_LITTLE_ENDIAN;

	/* Clear AES done and KEY done status */
	aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR));

	/* clear interrupt */
	//aes_reg_write(aes, AES_ASSR, 0x7);
	aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR) & (1<<0|1<<1|1<<2));

	/* enable AES done and KEY done interrupt */
	aes_reg_write(aes, AES_ASINTM, 0x3);

	/* Clear the IV and KEYS; enable AES module */
	aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<10 | 1 << 0) ;

	switch( para->keybits )
	{
		case 128:
			setkey_reg = 0;
			break;
		case 192:
			setkey_reg = 1;
			break;
		case 256:
			setkey_reg = 2;
			break;
		default :
			printk("error keybit\n");
			return -1;
	}
	writekey_times = para->keybits/32;
	key_byte = para->keybits/8;

	if((para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_ECB ) || (para->enworkmode == IN_UNF_CIPHER_WORK_MODE_DEC_ECB)){
		if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_ECB){
			/* Encrypts, ECB mode, key length */
			aes_reg_write(aes, AES_ASCR, 1<<0 | setkey_reg<<6);
		}else{
			/* Decrypts, ECB mode, key length */
			aes_reg_write(aes, AES_ASCR, 1<<0 | 1<<4 | setkey_reg << 6);
		}
	}else{
		if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_CBC){
			/* Encrypts, CBC mode, key length*/
			aes_reg_write(aes, AES_ASCR, 1<<0 | 1<<5 | setkey_reg << 6);
		}else{
			/* Decrypts, CBC mode, key length */
			aes_reg_write(aes, AES_ASCR, 1<<0 | 1<<5 | 1<<4 | setkey_reg << 6);
		}
		for(i = 0; i < 4; i++)
			aes_reg_write(aes, AES_ASIV, para->aesiv[i]);

		/* write IV init */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<1);
	}

	/* input KEYS */
	for(i = 0; i < writekey_times; i++)
	{
		aes_reg_write(aes, AES_ASKY, para->aeskey[i]);
	}

	/* write KEYS start */
	//init_completion(&aes->key_comp);
	aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<2);


#if !USRIRQ
	/*wait interrupt*/
	while (!(aes_reg_read(aes, AES_ASSR) & 1<<0));

	/* clear interrupt */
	aes_reg_write(aes, AES_ASSR, 1<<0);
#else
	ret = wait_for_completion_timeout(&aes->key_comp, msecs_to_jiffies(2000));
	if (!ret) {
		printk("%s[%d]:key timeout!\n",__func__,__LINE__);
		ret = -EINVAL;
		return ret;
	}
#endif

	para->status = JZ_AES_STATUS_DONE;
	return 0;

}
static int jz_aes_init(struct file *file, struct aes_operation *aes)
{

	struct aes_para *para = &aes->para;
	int i, ret;
	unsigned long timeout;
	int writekey_times, setkey_reg, key_byte;

	para->status = JZ_AES_STATUS_PREPARE;

	if(para->datalen % 16){
		printk("The size of data should be 16bytes aligned!\n");
		// return -EINVAL;
	}

	aes_reg_write(aes, AES_ASINTM, 0);
	/* Clear AES done and KEY done status */
	aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR));

	/* clear interrupt */
	//aes_reg_write(aes, AES_ASSR, 0x7);
	aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR) & (1<<0|1<<1|1<<2));

	/* enable AES dma done and KEY done interrupt */
	aes_reg_write(aes, AES_ASINTM, 0x5);

	/* Clear the IV and KEYS; enable AES module */
	aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<10 | 1 << 0) ;

	switch( para->keybits )
	{
		case 128:
			setkey_reg = 0;
			break;
		case 192:
			setkey_reg = 1;
			break;
		case 256:
			setkey_reg = 2;
			break;
		default :
			printk("error keybit\n");
			return -1;
	}
	writekey_times = para->keybits >> 5;// para->keybits/32;
	key_byte = para->keybits >> 3;//para->keybits/8;

	if((para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_ECB ) || (para->enworkmode == IN_UNF_CIPHER_WORK_MODE_DEC_ECB)){
		if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_ECB){
			/* Enable AES DMA, Encrypts, ECB mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<0 | 1<<8 | setkey_reg<<6);
		}else{
			/* Enable AES DMA, Decrypts, ECB mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<0 | 1<<4 | 1<<8 | setkey_reg << 6);
		}
	}else{
		if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_CBC){
			/* Enable AES DMA, Encrypts, CBC mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<8 | 1<<5 | 1<<0 | setkey_reg << 6);
		}else{
			/* Enable AES DMA, Decrypts, CBC mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<8 | 1<<5 | 1<<4 | 1<<0 | setkey_reg << 6);
		}
		for(i = 0; i < 4; i++)
			aes_reg_write(aes, AES_ASIV, para->aesiv[i]);

		/* write IV init */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<1);
	}

	/* input KEYS */
	for(i = 0; i < writekey_times; i++)
	{
		aes_reg_write(aes, AES_ASKY, para->aeskey[i]);
	}

	AES_LITTLE_ENDIAN;
	/* write KEYS start */
	aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<2);

	/*for save mem,use common buf*/
	aes_reg_write(aes, AES_ASSA, aes->src_addr_p);
	aes_reg_write(aes, AES_ASDA, aes->src_addr_p);
	// aes_reg_write(aes, AES_ASDA, aes->dst_addr_p);

#if !USRIRQ
	while (!(aes_reg_read(aes, AES_ASSR & 1<<0)));
	/* clear interrupt */
	aes_reg_write(aes, AES_ASSR, 1<<0);
#else
	timeout = msecs_to_jiffies(2000);
	ret = wait_for_completion_interruptible_timeout(&aes->key_comp,timeout);
	if(!ret){
		printk("aes key done outtime\n");
		ret = -EINVAL;
		return ret;
	}
#endif
	para->status = JZ_AES_STATUS_DONE;
	return 0;

}

static int aes_start_processing(struct file *file, struct aes_operation *aes)
{
	int ret = 0;
	if((aes->para.enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_ECB) ||aes->para.enworkmode == IN_UNF_CIPHER_WORK_MODE_DEC_ECB ){
		jz_aes_cpu_init(file, aes);
		ret = aes_start_cpu_encrypt_processing(aes);
		if (ret) {
			printk("aes encrypt error!\n");
			return -1;
		}
	}

	if((aes->para.enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_CBC) ||aes->para.enworkmode == IN_UNF_CIPHER_WORK_MODE_DEC_CBC ){
		g_t0 = aes_getns();
		ret = jz_aes_init(file, aes);
		if(ret){
			return ret;
		}
		g_t1 = aes_getns();
		ret = aes_start_encrypt_processing(aes);
		if (ret) {
			printk("aes encrypt error!\n");
			return -1;
		}
	}
	return 0;

}

static int aes_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	// unsigned long pfn;
	unsigned long total_pages;
	struct miscdevice *dev = filp->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);
	unsigned long size = vma->vm_end - vma->vm_start;

	unsigned long off = vma->vm_pgoff;
	unsigned long user_count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long per_buf_pages = PAGE_ALIGN(JZ_AES_DMA_DATALEN) >> PAGE_SHIFT;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	// 总页数 = src页数 + dst页数
	total_pages = 2 * per_buf_pages;

	if (off >= total_pages || user_count > total_pages - off) {
		return -EINVAL;
	}

#if 0
	if (off < per_buf_pages) {
		pfn = page_to_pfn(virt_to_page(aes->src_addr_v));
		ret = remap_pfn_range(vma, vma->vm_start, pfn + off,
								user_count << PAGE_SHIFT, vma->vm_page_prot);
	}
	else
	{
		off -= per_buf_pages;
		pfn = page_to_pfn(virt_to_page(aes->dst_addr_v));
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		ret = remap_pfn_range(vma, vma->vm_start, pfn + off,
								user_count << PAGE_SHIFT, vma->vm_page_prot);
	}
#else
	ret = dma_mmap_coherent(aes->dev, vma, aes->src_addr_v, aes->src_addr_p, JZ_AES_DMA_DATALEN);
#endif
	return ret;
}


static long aes_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);
	void __user *argp  = (void __user *)arg;
	unsigned long flags=0;
	int ret=0;

	mutex_lock(&aes->mutex);
	switch(cmd) {
	case IOCTL_AES_GET_OUTPUT:
		spin_lock_irqsave(&crypto_lock,flags);
		if (__copy_to_user_inatomic(argp, aes->dst_addr_v, 16)) {
		//if (copy_to_user((void __user *)(para->dst + offset), aes->dst_addr_v, len)) {
			ret = -EFAULT;
			printk("output copy_to_user  error %d!!!\n", ret);
			spin_unlock_irqrestore(&crypto_lock,flags);
			break;
		}
		spin_unlock_irqrestore(&crypto_lock,flags);
		asm volatile (
		    "sync\n\t"
		    "lw $0,0(%0)"
		    ::"r" (0xa0000000));
		wmb();
		break;
	case IOCTL_AES_GET_PARA:
		if (copy_to_user(argp, &aes->para, sizeof(struct aes_para))) {
			printk("aes set para copy_to_user error!!!\n");
			ret = -EFAULT;
		}
		asm volatile (
		    "sync\n\t"
		    "lw $0,0(%0)"
		    ::"r" (0xa0000000));
		break;
	case IOCTL_AES_PROCESSING:
		g_ioct0 = aes_getns();
		spin_lock_irqsave(&crypto_lock,flags);
		if (__copy_to_user_inatomic(&aes->para, argp, sizeof(struct aes_para))) {
			printk("aes get para copy_from_user error!!!\n");
			spin_unlock_irqrestore(&crypto_lock,flags);
			ret = -EFAULT;
		}
		spin_unlock_irqrestore(&crypto_lock,flags);
		g_ioct1 = aes_getns();
		ret = aes_start_processing(file,aes);

		g_ioct2 = aes_getns();
		debug_print("ioc t1-t0:%lld,t2-t1:%lld,toll:%lld\n",g_ioct1-g_ioct0,g_ioct2-g_ioct1,g_ioct2-g_ioct0);
		break;
	case IOCTL_AES_LOCK:
		mutex_lock(&aes->mutex_aes);
		break;
	case IOCTL_AES_UNLOCK:
		mutex_unlock(&aes->mutex_aes);
		break;
	default:
		break;
	}
	mutex_unlock(&aes->mutex);

	return ret;
}

const struct file_operations aes_fops = {
	.owner = THIS_MODULE,
	.read = aes_read,
	.write = aes_write,
	.open = aes_open,
	.unlocked_ioctl = aes_ioctl,
	.mmap = aes_mmap,
	.release = aes_release,
};
#if USRIRQ
static irqreturn_t aes_ope_irq_handler(int irq, void *data)
{
	struct aes_operation *aes_ope = data;
	unsigned int status = aes_reg_read(aes_ope, AES_ASSR);
	unsigned int mask = aes_reg_read(aes_ope, AES_ASINTM);
	unsigned int pending = status & mask;

	/* clear interrupt */
	aes_reg_write(aes_ope, AES_ASSR, status);
	/*if(aes_ope->para.status != JZ_AES_STATUS_WORKING){
		return IRQ_HANDLED;
	}*/
	if(((pending & AES_ASSR_DMAD) || (pending & AES_ASSR_AESD)) && (aes_ope->para.status == JZ_AES_STATUS_WORKING)){
		complete(&aes_ope->done_comp);
		aes_done=1;
	}
	if((pending & AES_ASSR_KEYD) && (aes_ope->para.status == JZ_AES_STATUS_PREPARE)){
		complete(&aes_ope->key_comp);
	}

	return IRQ_HANDLED;
}
#endif

static int jz_aes_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct aes_operation *aes_ope = NULL;
	aes_ope = (struct aes_operation *)kzalloc(sizeof(struct aes_operation), GFP_KERNEL);
	if (!aes_ope) {
		dev_err(&pdev->dev, "alloc aes mem_region failed!\n");
		return -ENOMEM;
	}
	sprintf(aes_ope->name, "jz-aes");
	aes_debug("%s, aes name is : %s\n",__func__, aes_ope->name);

	aes_ope->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!aes_ope->res) {
		dev_err(&pdev->dev, "failed to get dev resources\n");
		ret = -EINVAL;
		goto failed_get_mem;
	}

	aes_ope->res = request_mem_region(aes_ope->res->start,
			aes_ope->res->end - aes_ope->res->start + 1,
			pdev->name);
	if (!aes_ope->res) {
		dev_err(&pdev->dev, "failed to request regs memory region");
		ret = -EINVAL;
		goto failed_req_region;
	}
	aes_ope->iomem = ioremap(aes_ope->res->start, resource_size(aes_ope->res));
	if (!aes_ope->iomem) {
		dev_err(&pdev->dev, "failed to remap regs memory region\n");
		ret = -EINVAL;
		goto failed_iomap;
	}
	aes_debug("%s, aes iomem is :0x%08x\n", __func__, (unsigned int)aes_ope->iomem);

	aes_ope->irq = platform_get_irq(pdev, 0);
	if(aes_ope->irq <= 0){
		dev_err(&pdev->dev, "get irq failed\n");
		ret = -EINVAL;
		goto failed_get_irq;
	}
#if USRIRQ
	if (request_irq(aes_ope->irq, aes_ope_irq_handler, IRQF_SHARED, aes_ope->name, aes_ope)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto failed_req_irq;
	}
	aes_debug("%s, aes irq is : %d\n",__func__, aes_ope->irq);
#endif

	aes_ope->src_addr_v = dma_alloc_coherent(&pdev->dev,
							  JZ_AES_DMA_DATALEN,
							  &aes_ope->src_addr_p,
							  GFP_KERNEL);
	if (aes_ope->src_addr_v == NULL){
		printk("failed to alloc dma src buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_src;
	}

#if 0
	aes_ope->dst_addr_v = dma_alloc_coherent(&pdev->dev,
							  JZ_AES_DMA_DATALEN,
							  &aes_ope->dst_addr_p,
							  GFP_KERNEL);

	if (aes_ope->dst_addr_v == NULL){
		printk("failed to alloc dma dst buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_dst;
	}
#endif
	aes_ope->dma_datalen = JZ_AES_DMA_DATALEN;

	/****************************************************************/
	aes_ope->aes_dev.minor = MISC_DYNAMIC_MINOR;
	aes_ope->aes_dev.fops = &aes_fops;
	aes_ope->aes_dev.name = "aes";
	aes_ope->dev = &pdev->dev;

	ret = misc_register(&aes_ope->aes_dev);
	if(ret) {
		dev_err(&pdev->dev,"request misc device failed!\n");
		ret = -EINVAL;
		goto failed_misc;
	}
	aes_ope->clk = clk_get(aes_ope->dev,"gate_aes");
	if (IS_ERR(aes_ope->clk)){
		dev_err(&pdev->dev, "aes clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
#if defined(CONFIG_KERNEL_4_4_94)
	clk_prepare_enable(aes_ope->clk);
#else
	clk_enable(aes_ope->clk);
#endif


	platform_set_drvdata(pdev, aes_ope);

	mutex_init(&aes_ope->mutex);
	mutex_init(&aes_ope->mutex_aes);
	init_completion(&aes_ope->done_comp);
	init_completion(&aes_ope->key_comp);

	aes_ope->para.status = JZ_AES_STATUS_DONE;
	aes_debug("%s: probe() done\n", __func__);
	printk("@@@@ aes driver ok(version %s) @@@@@\n", AES_DRIVER_VERSION);

	return 0;

failed_clk:
	misc_deregister(&aes_ope->aes_dev);
failed_misc:
// 	dma_free_coherent(&pdev->dev, JZ_AES_DMA_DATALEN,
// 							  aes_ope->dst_addr_v, aes_ope->dst_addr_p);
// failed_alloc_dst:
	dma_free_coherent(&pdev->dev, JZ_AES_DMA_DATALEN,
							  aes_ope->src_addr_v, aes_ope->src_addr_p);
failed_alloc_src:
#if USRIRQ
	free_irq(aes_ope->irq, aes_ope);
#endif
failed_get_irq:
failed_req_irq:
	iounmap(aes_ope->iomem);
failed_iomap:
	release_mem_region(aes_ope->res->start, aes_ope->res->end - aes_ope->res->start + 1);
failed_req_region:
failed_get_mem:
	kfree(aes_ope);
	return ret;
}

static int jz_aes_remove(struct platform_device *pdev)
{
	struct aes_operation *aes_ope = platform_get_drvdata(pdev);

	misc_deregister(&aes_ope->aes_dev);
#if defined(CONFIG_KERNEL_4_4_94)
	clk_disable_unprepare(aes_ope->clk);
	devm_clk_put(&pdev->dev, aes_ope->clk);
#else
	clk_disable(aes_ope->clk);
	clk_put(aes_ope->clk);
#endif
	dma_free_coherent(&pdev->dev, JZ_AES_DMA_DATALEN,
							  aes_ope->src_addr_v, aes_ope->src_addr_p);
	// dma_free_coherent(&pdev->dev, JZ_AES_DMA_DATALEN,
	// 						  aes_ope->dst_addr_v, aes_ope->dst_addr_p);
#if USRIRQ
	free_irq(aes_ope->irq, aes_ope);
#endif
	iounmap(aes_ope->iomem);
	release_mem_region(aes_ope->res->start, aes_ope->res->end - aes_ope->res->start + 1);
	kfree(aes_ope);

	return 0;
}

static void  platform_aes_release(struct device *dev)
{
	return ;
}

static struct platform_driver jz_aes_driver = {
	.probe	= jz_aes_probe,
	.remove	= jz_aes_remove,
	.driver	= {
		.name	= "jz-aes",
		.owner	= THIS_MODULE,
	},
};

static struct resource jz_aes_resources[] = {
	[0] = {
		.start	= AES_IOBASE,
		.end	= AES_IOBASE + 0x28,
		.flags	= IORESOURCE_MEM,
	},

#if defined(CONFIG_KERNEL_4_4_94)
	[1] = {
		.start	= IRQ_AES + 8,
		.end	= IRQ_AES + 8,
		.flags	= IORESOURCE_IRQ,
	},
#else
	[1] = {
		.start	= IRQ_AES,
		.end	= IRQ_AES,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device jz_aes_device = {
	.name	= "jz-aes",
	.id = 0,
	.resource	= jz_aes_resources,
	.dev = {
		.release = platform_aes_release,
	},
	.num_resources	= ARRAY_SIZE(jz_aes_resources),
};


static int __init jz_aes_mod_init(void)
{
	int ret = 0;
	aes_debug("loading %s driver\n", "jz-aes");

	ret = platform_device_register(&jz_aes_device);
	if(ret){
		printk("Failed to insmod aes device!!!\n");
		return ret;
	}
	return	platform_driver_register(&jz_aes_driver);
}

static void __exit jz_aes_mod_exit(void)
{
	platform_driver_unregister(&jz_aes_driver);
	platform_device_unregister(&jz_aes_device);
}


module_init(jz_aes_mod_init);
module_exit(jz_aes_mod_exit);

MODULE_DESCRIPTION("Ingenic AES hw support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Elvis Wang");
