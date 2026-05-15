/*
 * Cryptographic API.
 *
 * Support for Tseries AES HW.
 *
 * Copyright (c) 2024 Ingenic Corporation
 * Author: Weijie Xu <weijie.xu@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 */

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
#include <crypto/hash.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <soc/base.h>
#if (defined(CONFIG_KERNEL_4_4_94)|| defined(CONFIG_KERNEL_5_15)) && defined(CONFIG_SOC_PRJ007)
#include <dt-bindings/interrupt-controller/PRJ007-irq.h>
#include <dt-bindings/dma/ingenic-pdma.h>
#elif (defined(CONFIG_KERNEL_4_4_94)|| defined(CONFIG_KERNEL_5_15)) && defined(CONFIG_SOC_PRJ008)
#include <dt-bindings/interrupt-controller/PRJ008-irq.h>
#include <dt-bindings/dma/ingenic-pdma.h>
#else
#include <soc/irq.h>
#include <mach/jzdma.h>
#endif
//#include <mach/platform.h>
#include "jz-hash.h"
#include "pdmac.h"

#ifdef DEBUG
#define hash_debug(format, ...) {printk(format, ## __VA_ARGS__);}
#else
#define hash_debug(format, ...) do{ } while(0)
#endif

#define HASH_DRIVER_VERSION "H20250801a"

typedef unsigned int u32;
int ROUND ;

#if defined(DEBUG) && defined(CONFIG_SOC_PRJ007)
static void dump_hash_regs(hash_operation_t *hash)
{
	printk("HSCR   = 0x%08x\n", hash_reg_read(hash, HASH_HSCR));
	printk("HSSR   = 0x%08x\n", hash_reg_read(hash, HASH_HSSR));
	printk("HSINTM = 0x%08x\n", hash_reg_read(hash, HASH_HSINTM));
	printk("HSSA   = 0x%08x\n", hash_reg_read(hash, HASH_HSSA));
	printk("HSTC   = 0x%08x\n", hash_reg_read(hash, HASH_HSTC));
	printk("HSDI   = 0x%08x\n", hash_reg_read(hash, HASH_HSDI));
	printk("HSCG   = 0x%08x\n", hash_reg_read(hash, HASH_HSCG));
}
#endif
#if defined(DEBUG) && defined(CONFIG_SOC_PRJ008)
static void dump_hash_registers(hash_operation_t *hash)
{
    printk("-----------------hash_reg------------------\n");
    printk("0x00:   %08x\n", readl(hash->iomem + 0x00));
    printk("0x04:   %08x\n", readl(hash->iomem + 0x04));
    printk("0x08:   %08x\n", readl(hash->iomem + 0x08));
    printk("0x0c:   %08x\n", readl(hash->iomem + 0x0c));
    printk("0x10:   %08x\n", readl(hash->iomem + 0x10));
    printk("0x14:   %08x\n", readl(hash->iomem + 0x14));
    printk("0x1c:   %08x\n", readl(hash->iomem + 0x1c));
}

static void dump_pdma_registers(hash_operation_t *hash, int chn)
{
    printk("-----------------pdma_reg------------------\n");
    printk("0x00:   %08x\n", readl(hash->pdma_iomem + 0x00 + 0x20*chn));
    printk("0x04:   %08x\n", readl(hash->pdma_iomem + 0x04 + 0x20*chn));
    printk("0x08:   %08x\n", readl(hash->pdma_iomem + 0x08 + 0x20*chn));
    printk("0x0c:   %08x\n", readl(hash->pdma_iomem + 0x0c + 0x20*chn));
    printk("0x10:   %08x\n", readl(hash->pdma_iomem + 0x10 + 0x20*chn));
    printk("0x14:   %08x\n", readl(hash->pdma_iomem + 0x14 + 0x20*chn));
    printk("0x18:   %08x\n", readl(hash->pdma_iomem + 0x18 + 0x20*chn));
	printk("0x1c:   %08x\n", readl(hash->pdma_iomem + 0x1c + 0x20*chn));
    printk("0x1000:   %08x\n", readl(hash->pdma_iomem + 0x1000));
    printk("0x1004:   %08x\n", readl(hash->pdma_iomem + 0x1004));
    printk("0x1008:   %08x\n", readl(hash->pdma_iomem + 0x1008));
}
#endif

static ssize_t hash_read(struct file *filp, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t hash_write(struct file *filp,const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static int hash_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *dev = filp->private_data;
	hash_operation_t *hash = miscdev_to_hashops(dev);
	if(hash->state){
		printk("HASH driver is busy,can't open again!\n");
		return -EBUSY;
	}
	hash->state = 1;
	hash_debug("%s:hash open successsful!\n",__func__);
	return 0;
}

static int hash_digest_init(hash_operation_t *hash,int cmd)
{
	int round = 0;

	//set register to default
	hash_reg_write(hash,HASH_HSCR,0x30000000);
	hash_reg_write(hash,HASH_HSSR,0x0);
	hash_reg_write(hash,HASH_HSINTM,0x0);
	hash_reg_write(hash,HASH_HSTC,0x0);
	hash_reg_write(hash,HASH_HSSA,0x0);
	//hash_reg_write(hash,HASH_HSSR,0x03);
	hash_bit_set(hash, HASH_HSCG, 0x0);

	//clock gate enable
	hash_bit_set(hash, HASH_HSCG, 1 << 0);
	//Degest mode set
	switch(cmd){
		case IOCTL_HASH_MD5:
			round = CPYPT_OUT_ROUND_MD5;
			hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (0 << 1));
			break;
		case IOCTL_HASH_SHA1:
			round = CRYPT_OUT_ROUND_SHA1;
			hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (1 << 1));
			break;
		case IOCTL_HASH_SHA224:
			round = CRYPT_OUT_ROUNT_SHA224;
			hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (2 << 1));
			break;
		case IOCTL_HASH_SHA256:
			round = CRYPT_OUT_ROUNT_SHA256;
			hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (3 << 1));
			break;
		case IOCTL_HASH_SHA384:
			round = CRYPT_OUT_ROUNT_SHA384;
			hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (4 << 1));
			break;
		case IOCTL_HASH_SHA512:
			round = CRYPT_OUT_ROUND_SHA512;
			hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (5 << 1));
			break;
	}
	ROUND = round;
	//hash enable
	hash_reg_write(hash,HASH_HSCR,hash_reg_read(hash,HASH_HSCR)|(1 << 0));
	if(cmd == IOCTL_HASH_MD5){
		hash_reg_write(hash,HASH_HSCR,hash_reg_read(hash,HASH_HSCR)|(1 << 8));
	}else{
		hash_reg_write(hash,HASH_HSCR,hash_reg_read(hash,HASH_HSCR)|(1 << 7));
	}

	hash_bit_set(hash,HASH_HSCR, 1<<4);

	return 0;

}

static int hash_digest_execute(hash_operation_t *hash,int cmd)
{
	struct hash_para *para = &hash->para;
	int ret = 0;
	int offset	= 0;
	int length = 0;
	int i = 0;
	//MD5 SHA1 SHA224 SHA256 (512bit*N)(64byte*N)
	//SHA384 SHA512			 (1024bit*N)(128byte*N)
	if(para->plaintext_len % 64 != 0){
		printk("The plain text must be 512bit or 1024bit aligned!\n");
		return -EINVAL;
	}

	//transfer BNUM :1bloct = 512bit
#if defined(CONFIG_SOC_PRJ008)
	hash_reg_write(hash, HASH_HSBNUM, para->plaintext_len/64);
#endif

	while(offset < para->plaintext_len){

		length = para->plaintext_len - offset > hash->dma_datalen ? hash->dma_datalen : para->plaintext_len - offset;

		//clear one round done
		hash_reg_write(hash, HASH_HSSR,0x03);

		//enable multi hash round done interrupt
		hash_reg_write(hash, HASH_HSINTM,0x03);

		//dma enable
		hash_bit_set(hash,HASH_HSCR,1<<5);

#if defined(CONFIG_SOC_PRJ007)
		//transfer count :1bloct = 512bit
		hash_reg_write(hash, HASH_HSTC, length/64);
#elif defined(CONFIG_SOC_PRJ008)
		//transfer count :1bloct = 32bit
		hash_reg_write(hash, HASH_HSTC, length/4);
#endif

		//set dma source address
		hash_reg_write(hash, HASH_HSSA, hash->src_addr_p);

		if(copy_from_user(hash->src_addr_v, (void __user *)(para->src + offset),length)){
			ret = -EINVAL;
			break;
		}
		dma_sync_single_for_device(hash->dev, (dma_addr_t)hash->src_addr_p, length, DMA_TO_DEVICE);

		//dma start
		hash_bit_set(hash,HASH_HSCR, 1<<6);
		i = 1000;
		while(i--){
			if(!wait_for_completion_interruptible(&hash->done_comp))
				break;
		}

		if(i <= 0){
			 printk("%s[%d]: timeout!\n",__func__,__LINE__);
			 ret = -EFAULT;
		}

		offset += length;
	}

	return ret;
}

static int hash_digest_final(hash_operation_t *hash,int cmd)
{
	int ret = 0;
	int i = 0;
	int round = 0;
	u32 *crypttext;
	struct hash_para *para = &hash->para;
	hash_digest_execute(hash,cmd);
	round = ROUND;
	crypttext = (u32*)kzalloc(round*sizeof(u32),GFP_KERNEL);
	for(i = 0;i < round;i ++){
		crypttext[i] = hash_reg_read(hash,HASH_HSDO);
		hash_debug("0x%08x\n",crypttext[i]);
	}
	if(copy_to_user((void __user *)(para->dst), crypttext, round * sizeof(u32))){
		ret = -EFAULT;
	}
	//debug_hash_regs(hash);
	para->crypttext_len = round * sizeof(u32);
	kfree(crypttext);
	return ret;
}

#if defined(CONFIG_SOC_PRJ008) && defined(CONFIG_TEST_PDMA_PRJ008)
static int hash_digest_execute_pdma(hash_operation_t *hash,int cmd)
{
    struct hash_para *para = &hash->para;
	int ret = 0;
    int round = 0;
    int i = 0;
    u32 *crypttext;
    //MD5 SHA1 SHA224 SHA256 (512bit*N)(64byte*N)
    //SHA384 SHA512          (1024bit*N)(128byte*N)
    if(para->plaintext_len % 4 != 0){
        printk("The plain text must be 4bit aligned!\n");
        return -EINVAL;
    }

	//set register to default
	hash_reg_write(hash,HASH_HSCR,0x30000000);
	hash_reg_write(hash,HASH_HSSR,0x0);
	hash_reg_write(hash,HASH_HSINTM,0x0);
	hash_reg_write(hash,HASH_HSTC,0x0);
	hash_reg_write(hash,HASH_HSSA,0x0);
	//hash_reg_write(hash,HASH_HSSR,0x03);
	hash_bit_set(hash, HASH_HSCG, 0x0);

    //clock gate enable
    hash_bit_set(hash, HASH_HSCG, 1 << 0);
    //Degest mode set
    switch(cmd){
        case IOCTL_HASH_MD5:
            round = CPYPT_OUT_ROUND_MD5;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (0 << 1));
            break;
        case IOCTL_HASH_SHA224:
            round = CRYPT_OUT_ROUNT_SHA224;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (2 << 1));
            break;
        case IOCTL_HASH_SHA256:
            round = CRYPT_OUT_ROUNT_SHA256;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (3 << 1));
            break;
        case IOCTL_HASH_SHA384:
            round = CRYPT_OUT_ROUNT_SHA384;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (4 << 1));
            break;
        case IOCTL_HASH_SHA512:
            round = CRYPT_OUT_ROUND_SHA512;
            hash_reg_write(hash, HASH_HSCR, hash_reg_read(hash,HASH_HSCR) | (5 << 1));
            break;
    }
    crypttext = (u32*)kzalloc(round*sizeof(u32),GFP_KERNEL);
	//hash enable
	hash_reg_write(hash,HASH_HSCR,hash_reg_read(hash,HASH_HSCR)|(1 << 0));
	if(cmd == IOCTL_HASH_MD5){
		hash_reg_write(hash,HASH_HSCR,hash_reg_read(hash,HASH_HSCR)|(1 << 8));
	}else{
		hash_reg_write(hash,HASH_HSCR,hash_reg_read(hash,HASH_HSCR)|(1 << 7));
	}

	hash_bit_set(hash, HASH_HSCR, 1<<4);

	//clear one round done
	hash_reg_write(hash, HASH_HSSR, hash_reg_read(hash, HASH_HSSR) | 0x02);

	//enable multi hash round done interrupt
	hash_reg_write(hash, HASH_HSINTM, 0x02);

	//dma enable
	hash_bit_set(hash, HASH_HSCR, 1<<5);

	hash_reg_write(hash, HASH_HSBNUM, para->plaintext_len/64);

	if(copy_from_user(hash->src_addr_v, (void __user *)(para->src), para->plaintext_len)){
            ret = -EINVAL;
            //break;
        }

	dma_sync_single_for_device(NULL, (dma_addr_t)hash->src_addr_p, JZ_HASH_DMA_DATALEN, DMA_TO_DEVICE);

#if 1

	//1K
	/*struct hash_pdma_para pdma_src[1] = {
		{hash->src_addr_p, 256+16}
	};*/

	//32K
	struct hash_pdma_para pdma_src[16] = {
		{hash->src_addr_p, 512},
		{hash->src_addr_p + 2048, 512},
		{hash->src_addr_p + 2048*2, 512},
		{hash->src_addr_p + 2048*3, 512},
		{hash->src_addr_p + 2048*4, 512},
		{hash->src_addr_p + 2048*5, 512},
		{hash->src_addr_p + 2048*6, 512},
		{hash->src_addr_p + 2048*7, 512},
		{hash->src_addr_p + 2048*8, 512},
		{hash->src_addr_p + 2048*9, 512},
		{hash->src_addr_p + 2048*10, 512},
		{hash->src_addr_p + 2048*11, 512},
		{hash->src_addr_p + 2048*12, 512},
		{hash->src_addr_p + 2048*13, 512},
		{hash->src_addr_p + 2048*14, 512},
		{hash->src_addr_p + 2048*15, 512+16}
	};
#endif
	dma_sync_single_for_device(NULL, (dma_addr_t)hash->pdma_data_p, 128, DMA_TO_DEVICE);

	for (i = 0; i < sizeof(pdma_src)/sizeof(pdma_src[0]); i++)
		((struct hash_pdma_para *)hash->pdma_data_v)[i] = pdma_src[i];

#ifdef DEBUG
	printk("sizeof(pdma_src) = %d, sizeof(pdma_src[0]) = %d\n", sizeof(pdma_src), sizeof(pdma_src[0]));
	for (i = 0; i < sizeof(pdma_src)/sizeof(pdma_src[0]); i++)
		printk("addr = %08x, len = %d\n", ((struct hash_pdma_para *)hash->pdma_data_v)[i].data_addr, \
				((struct hash_pdma_para *)hash->pdma_data_v)[i].len);
	//start filling pdma register
	printk("pdma iomem is :0x%08x\n", (unsigned int)hash->pdma_iomem);
	printk("pdma_data_v = %p, pdma_data_p = %08x\n", (unsigned int)hash->pdma_data_v, hash->pdma_data_p);
	for(i = 0; i < (sizeof(pdma_src)/sizeof(pdma_src[0])*2); i++)
	{
		printk("pdma_data_p is %08x, data is %x\n", (hash->pdma_data_v + (i * 4)), *(volatile unsigned int*)(hash->pdma_data_v + (i * 4)));
	}
	dump_hash_registers(hash);

	printk("hash sa:   %08x\n", readl(hash->iomem + 0x0c));
	printk("hash tc:   %08x\n", readl(hash->iomem + 0x10));
#endif
	dma_sync_single_for_device(NULL, (dma_addr_t)hash->pdma_data_p, 128, DMA_TO_DEVICE);
	DMADMAC(PDMA_P) = 0;		//dis DMA

	DMADSA(1,PDMA_P) = hash->pdma_data_p;			//src physical address
	DMADTA(1,PDMA_P) = PDMA_DST_ADDR;	        //dst physical address
	DMADRT(1,PDMA_P) = PDMA_MODE;
	DMADTC(1,PDMA_P) = (sizeof(pdma_src)/sizeof(pdma_src[0])) * 8;
	printk("Pdma transfer count:%d\n", (sizeof(pdma_src)/sizeof(pdma_src[0])) * 8);

	DMADCM(1,PDMA_P) =
		(1<<23)| //SAI
		(0<<22)| //DAI
		(5<<16)| //RDIL
		(0<<14)| //SP
		(0<<12)| //DP
		(7<<8 )| //TSZ
		(0<<2 )| //STDE
		(0<<1 )| //TIE
		(0<<0 )  //LINK
		;

	DMADCS(1,PDMA_P) =
		(1<<31)| //NDES
		(0<<30)| //DES4
		(0<<4 )| //AR
		(0<<3 )| //TT
		(0<<2 )| //HTL
		(1<<0 )  //CTE
		;
#ifdef DEBUG
	printk("hash sa:   %08x\n", readl(hash->iomem + 0x0c));
	printk("hash tc:   %08x\n", readl(hash->iomem + 0x10));
	dump_pdma_registers(hash, 1);
#endif

	DMADMAC(PDMA_P) = 1;		//Enable DMA

	//dma start
	hash_bit_set(hash,HASH_HSCR, 1<<6);

	while(DMA_READBIT(DMADCS(1,PDMA_P),3,1)==0) {//wait until finish
		msleep(5);
	}

	i = 1000;
	while(i--){
		if(!wait_for_completion_interruptible(&hash->done_comp))
			break;
	}
	if(i <= 0){
		 printk("%s[%d]: timeout!\n",__func__,__LINE__);
		 ret = -EFAULT;
	}
	DMADCS(1,PDMA_P) = 0 << 0;

    for(i = 0;i < round;i ++){
        crypttext[i] = hash_reg_read(hash,HASH_HSDO);
		hash_debug("0x%08x\n",crypttext[i]);
    }
    if(copy_to_user((void __user *)(para->dst), crypttext, round * sizeof(u32))){
        ret = -EFAULT;
    }
	//debug_hash_regs(hash);
    para->crypttext_len = round * sizeof(u32);
    kfree(crypttext);
    return ret;
}
#endif

static long hash_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	hash_operation_t *hash = miscdev_to_hashops(dev);
	void __user *argp = (void __user *)arg;
	int ret = 0;
	switch(cmd){
		case IOCTL_HASH_GET_PARA:
			{
				if (copy_to_user(argp, &hash->para, sizeof(struct hash_para))){
					printk("hash set para copy_to_user error!!!\n");
					return -EFAULT;
				}
			}
			break;
		case IOCTL_HASH_MD5:
#if defined(CONFIG_SOC_PRJ007)
		case IOCTL_HASH_SHA1:
#endif
		case IOCTL_HASH_SHA224:
		case IOCTL_HASH_SHA256:
		case IOCTL_HASH_SHA384:
		case IOCTL_HASH_SHA512:
#if (defined(CONFIG_TEST_DMA_PRJ008) && defined(CONFIG_SOC_PRJ008)) || defined(CONFIG_SOC_PRJ007)
		{
			if (copy_from_user(&hash->para, argp, sizeof(struct hash_para)))
			{
				printk("hash get para copy_from_user error!!!\n");
				return -EFAULT;
			}
			if(hash->para.init_flag == 1)
				hash_digest_init(hash,cmd);
			if(hash->para.final_flag == 0)
				ret = hash_digest_execute(hash,cmd);
			else
				ret = hash_digest_final(hash,cmd);
			if (ret)
			{
				printk("hash digest error!\n");
				return -1;
			}
			if (copy_to_user(argp, &hash->para, sizeof(struct hash_para))) {
				printk("hash set para copy_to_user error!!!\n");
				return -EFAULT;
			}
		}
		break;
#endif

#if defined(CONFIG_SOC_PRJ008) && defined(CONFIG_TEST_PDMA_PRJ008)
		{
			if (copy_from_user(&hash->para, argp, sizeof(struct hash_para)))
			{
				printk("hash get para copy_from_user error!!!\n");
				return -EFAULT;
			}

			ret = hash_digest_execute_pdma(hash,cmd);
			if (ret)
			{
				printk("hash digest error!\n");
				return -1;
			}
			if (copy_to_user(argp, &hash->para, sizeof(struct hash_para))) {
				printk("hash set para copy_to_user error!!!\n");
				return -EFAULT;
			}
		}
		break;
#endif
		default:
			break;
	}
	return 0;
}

static int hash_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *dev = filp->private_data;
	struct hash_operation *hash = miscdev_to_hashops(dev);
	hash->state = 0;
	return 0;
}

const struct file_operations hash_fops = {
	.owner = THIS_MODULE,
	.read = hash_read,
	.write = hash_write,
	.open = hash_open,
	.unlocked_ioctl = hash_ioctl,
	.release = hash_release,
};

static irqreturn_t hash_ope_irq_handler(int irq, void *data)
{
	hash_operation_t *hash_ope = data;
	unsigned int status = hash_reg_read(hash_ope, HASH_HSSR);
	unsigned int mask = hash_reg_read(hash_ope, HASH_HSINTM);

	unsigned int pending = status & mask;

	/* clear interrupt */
	hash_debug("mask = %d status = %d\n",mask,status);

	if(pending & HASH_HSSR_ORD){
		hash_bit_set(hash_ope, HASH_HSSR, 0x01);
		complete(&hash_ope->done_comp);
	}

	if(pending & HASH_HSSR_MRD){
		hash_bit_set(hash_ope, HASH_HSSR, 0x02);
		complete(&hash_ope->done_comp);
	}
	return 0;
}

static int jz_hash_probe(struct platform_device *pdev)
{
	int ret = 0;
	hash_operation_t *hash_ope = (hash_operation_t*)kzalloc(sizeof(hash_operation_t),GFP_KERNEL);
	if(!hash_ope){
		dev_err(&pdev->dev, "alloc hash mem_region failed!\n");
		return -ENOMEM;
	}
	sprintf(hash_ope->name,"jz-hash");
	hash_debug("%s,hash name is : %s\n",__func__,hash_ope->name);

	//platform_device mem resource get
	hash_ope->io_res = platform_get_resource(pdev, IORESOURCE_MEM,0);
	if(!hash_ope->io_res)
	{
		dev_err(&pdev->dev, "failed to get dev io resources\n");
		ret = -EINVAL;
		goto failed_get_mem;
	}

	//I/O request
	hash_ope->io_res = request_mem_region(hash_ope->io_res->start,
				hash_ope->io_res->end - hash_ope->io_res->start + 1,
				pdev->name);
	if(!hash_ope->io_res)
	{
		dev_err(&pdev->dev, "failed to request regs memory region");
		ret = -EINVAL;
		goto failed_req_region;
	}

	//ioremap
	hash_ope->iomem = ioremap(hash_ope->io_res->start,resource_size(hash_ope->io_res));
	if(!hash_ope->iomem)
	{
		dev_err(&pdev->dev, "failed to remap regs memory region\n");
		ret = -EINVAL;
		goto failed_iomap;
	}
	hash_debug("%s, hash iomem is :0x%08x\n", __func__, (unsigned int)hash_ope->iomem);

#if defined(CONFIG_SOC_PRJ008)
    /*ioremap pdma*/
	hash_ope->pdma_iomem = ioremap(PDMA_BASE_ADDR, PDMA_ADDR_LEN);
    if(!hash_ope->pdma_iomem)
    {
        dev_err(&pdev->dev, "failed to remap pdma regs memory region\n");
        ret = -EINVAL;
        goto failed_pdma_iomap;
    }
    hash_debug("%s, pdma iomem is :0x%08x\n", __func__, (unsigned int)hash_ope->pdma_iomem);
#endif

	//interrupt
	hash_ope->irq = platform_get_irq(pdev, 0);
	if (request_irq(hash_ope->irq, hash_ope_irq_handler, IRQF_SHARED, hash_ope->name, hash_ope)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto failed_get_irq;
	}
	hash_debug("%s, hash irq is : %d\n",__func__, hash_ope->irq);

	//dma
	hash_ope->src_addr_v = dma_alloc_coherent(&pdev->dev,
								JZ_HASH_DMA_DATALEN,
								&hash_ope->src_addr_p,
								GFP_KERNEL | GFP_DMA);
	if (hash_ope->src_addr_v == NULL)
	{
		printk("failed to alloc dma src buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_dma;
	}
	hash_ope->dma_datalen = JZ_HASH_DMA_DATALEN;

#if defined(CONFIG_SOC_PRJ008)
#ifdef CONFIG_KERNEL_5_15
    hash_ope->pdma_data_v = dma_alloc_noncoherent(&pdev->dev,
                                sizeof(struct hash_pdma_para)*16,
                                &hash_ope->pdma_data_p,
								DMA_TO_DEVICE,
                                GFP_KERNEL);
#else
    hash_ope->pdma_data_v = dma_alloc_noncoherent(&pdev->dev,
                                sizeof(struct hash_pdma_para)*16,
                                &hash_ope->pdma_data_p,
                                GFP_KERNEL | GFP_DMA);
#endif
    if (hash_ope->pdma_data_v == NULL)
    {
        printk("failed to alloc dma src buffer\n");
        ret = -ENOMEM;
        goto failed_alloc_pdata_dma;
    }
#endif

	hash_ope->hash_dev.minor = MISC_DYNAMIC_MINOR;
	hash_ope->hash_dev.fops  = &hash_fops;
	hash_ope->hash_dev.name  = "hash";
	hash_ope->dev = &pdev->dev;

	init_completion(&hash_ope->done_comp);
	init_completion(&hash_ope->one_comp);

	ret = misc_register(&hash_ope->hash_dev);
	if(ret)
	{
		dev_err(&pdev->dev,"request misc device failed!\n");
		ret = -EINVAL;
		goto failed_misc;
	}

	hash_ope->clk = clk_get(hash_ope->dev,"gate_hash");
	if (IS_ERR(hash_ope->clk))
	{
		dev_err(&pdev->dev, "hash clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
	clk_prepare_enable(hash_ope->clk);
#else
	clk_enable(hash_ope->clk);
#endif

	platform_set_drvdata(pdev, hash_ope);

	hash_debug("%s: probe() done\n", __func__);
	printk("@@@@ hash driver ok(version %s) @@@@@\n", HASH_DRIVER_VERSION);

	return 0;

failed_clk:
	misc_deregister(&hash_ope->hash_dev);
failed_misc:
#if defined(CONFIG_SOC_PRJ008)
#ifdef CONFIG_KERNEL_5_15
    dma_free_noncoherent(&pdev->dev, sizeof(struct hash_pdma_para)*16,hash_ope->pdma_data_v, hash_ope->pdma_data_p,DMA_TO_DEVICE);
#else
    dma_free_noncoherent(&pdev->dev, sizeof(struct hash_pdma_para)*16,hash_ope->pdma_data_v, hash_ope->pdma_data_p);
#endif
failed_alloc_pdata_dma:
#endif
	dma_free_coherent(&pdev->dev, JZ_HASH_DMA_DATALEN,hash_ope->src_addr_v, hash_ope->src_addr_p);
failed_alloc_dma:
	free_irq(hash_ope->irq, hash_ope);
failed_get_irq:
#if defined(CONFIG_SOC_PRJ008)
	iounmap(hash_ope->pdma_iomem);
failed_pdma_iomap:
#endif
	iounmap(hash_ope->iomem);
failed_iomap:
	release_mem_region(hash_ope->io_res->start, hash_ope->io_res->end - hash_ope->io_res->start + 1);
failed_req_region:
failed_get_mem:
	kfree(hash_ope);
	return ret;
}

static int jz_hash_remove(struct platform_device *pdev)
{
	hash_operation_t *hash_ope = platform_get_drvdata(pdev);
	misc_deregister(&hash_ope->hash_dev);
#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
	clk_disable_unprepare(hash_ope->clk);
	devm_clk_put(&pdev->dev, hash_ope->clk);
#else
	clk_disable(hash_ope->clk);
	clk_put(hash_ope->clk);
#endif
	dma_free_coherent(&pdev->dev, JZ_HASH_DMA_DATALEN, hash_ope->src_addr_v, hash_ope->src_addr_p);
	iounmap(hash_ope->iomem);
#if defined(CONFIG_SOC_PRJ008)
	iounmap(hash_ope->pdma_iomem);
#ifdef CONFIG_KERNEL_5_15
    dma_free_noncoherent(&pdev->dev, sizeof(struct hash_pdma_para)*16,hash_ope->pdma_data_v, hash_ope->pdma_data_p,DMA_TO_DEVICE);
#else
    dma_free_noncoherent(&pdev->dev, sizeof(struct hash_pdma_para)*16,hash_ope->pdma_data_v, hash_ope->pdma_data_p);
#endif
#endif
	kfree(hash_ope);
	return 0;
}

static void  platform_hash_release(struct device *dev)
{
	return ;
}

static struct platform_driver jz_hash_driver = {
	.probe = jz_hash_probe,
	.remove = jz_hash_remove,
	.driver = {
		.name = "jz-hash",
		.owner = THIS_MODULE,
	}
};

static struct resource jz_hash_resources[] = {
	[0] = {
		.start	= HASH_IOBASE,
		.end	= HASH_IOBASE + 0x20,
		.flags	= IORESOURCE_MEM,
	},
#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
	[1] = {
		.start	= IRQ_HASH + 8,
		.end	= IRQ_HASH + 8,
		.flags	= IORESOURCE_IRQ,
	},
#else
	[1] = {
		.start	= IRQ_HASH,
		.end	= IRQ_HASH,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device jz_hash_device = {
	.name	= "jz-hash",
	.id = 0,
	.resource	= jz_hash_resources,
	.dev = {
		.release  = platform_hash_release,
	},
	.num_resources	= ARRAY_SIZE(jz_hash_resources),
};

static int __init jz_hash_mod_init(void)
{
	int ret = 0;
	hash_debug("loading %s driver\n", "jz-hash");

	ret = platform_device_register(&jz_hash_device);
	if(ret){
		printk("Failed to insmod hash device driver!!!\n");
		return ret;
	}
	return platform_driver_register(&jz_hash_driver);
}

static void __exit jz_hash_mod_exit(void)
{
	platform_device_unregister(&jz_hash_device);
	platform_driver_unregister(&jz_hash_driver);
}

module_init(jz_hash_mod_init);
module_exit(jz_hash_mod_exit);

MODULE_DESCRIPTION("Ingenic HASH hw support");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Weijie Xu");
