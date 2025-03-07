
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/completion.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <dt-bindings/interrupt-controller/a1-irq.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "ingenic_aip.h"
#define AIP_DRIVER_VERSION "H20230213a"
/* #define DEBUG_ENABLE */
#ifdef DEBUG_ENABLE
#define my_printk(fmt, x...) \
	do {	\
		printk("[%s:%d] "fmt"",__func__,__LINE__,##x); \
	}while(0)
#else
#define my_printk(fmt, x...)
#endif

#define REG_AIP_IRQ_START       (0x04)

#define JZ_AIP_NUM      	3
static uint64_t jz_aip_dma_mask = ~((uint64_t)0);
static struct resource jz_aip_t_resources[] = {
	[0] = {
		.start = JZ_AIP_T_IOBASE,
		.end = JZ_AIP_T_IOBASE + JZ_AIP_IOSIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = MODULE_IRQ_NUM(IRQ_AIP0),
		.end = MODULE_IRQ_NUM(IRQ_AIP0),
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource jz_aip_f_resources[] = {
	[0] = {
		.start = JZ_AIP_F_IOBASE,
		.end = JZ_AIP_F_IOBASE + JZ_AIP_IOSIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = MODULE_IRQ_NUM(IRQ_AIP1),
		.end = MODULE_IRQ_NUM(IRQ_AIP1),
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource jz_aip_p_resources[] = {
	[0] = {
		.start = JZ_AIP_P_IOBASE,
		.end = JZ_AIP_P_IOBASE + JZ_AIP_IOSIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = MODULE_IRQ_NUM(IRQ_AIP2),
		.end = MODULE_IRQ_NUM(IRQ_AIP2),
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device jz_aip_device[3] = {
	{
		.name = "jz-aip",
		.id = 0,
		.dev = {
			.dma_mask = &jz_aip_dma_mask,
			.coherent_dma_mask = 0xffffffff,
		},
		.num_resources = ARRAY_SIZE(jz_aip_t_resources),
		.resource = jz_aip_t_resources,
	},
	{
		.name = "jz-aip",
		.id = 1,
		.dev = {
			.dma_mask = &jz_aip_dma_mask,
			.coherent_dma_mask = 0xffffffff,
		},
		.num_resources = ARRAY_SIZE(jz_aip_f_resources),
		.resource = jz_aip_f_resources,
	},
	{
		.name = "jz-aip",
		.id = 2,
		.dev = {
			.dma_mask = &jz_aip_dma_mask,
			.coherent_dma_mask = 0xffffffff,
		},
		.num_resources = ARRAY_SIZE(jz_aip_p_resources),
		.resource = jz_aip_p_resources,
	},
};

struct jz_aip {
	char				name[16];
	int				idx;
	void __iomem			*iomem;
	struct clk			*clk_gate;

	int				irq;
	struct completion		done;
	spinlock_t			slock;
	unsigned int			status;
	pid_t				owner_pid;
	struct miscdevice		mdev;
	struct mutex			mlock;
	int				opencnt;
	struct jz_aip_chainbuf 		*chainbuf;
};

#define jz_aip_readl(aip, offset)		__raw_readl((aip)->iomem + offset)
#define jz_aip_writel(aip, offset, value)	__raw_writel((value), (aip)->iomem + offset)

#define CLEAR_RADIX_BIT(aip,offset,bm)				\
	do {							\
		unsigned int stat;				\
		stat = jz_aip_readl(aip,offset);			\
		jz_aip_writel(aip,offset,stat & ~(bm));		\
	} while(0)

static irqreturn_t jz_aip_interrupt_t(int irq, void *data)
{
	struct jz_aip *aip = (struct jz_aip *)data;

	/* disable_irq_nosync(irq); */

	/* printk("############irq status = 0x%08x\n",aip->status); */
	aip->status = jz_aip_readl(aip, REG_AIP_IRQ_START);
	jz_aip_writel(aip, REG_AIP_IRQ_START, aip->status);
	my_printk("irq status = 0x%08x",aip->status);

	if(aip->idx == 0) {
		if(aip->status & 0x1<<3) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
			complete(&aip->done);
		}

		if(aip->status & 0x1<<2) {
			dev_err(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n, timeout!!!",__func__, irq, aip->status);
		}

		if(aip->status & 0x1<<1) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
			complete(&aip->done);
		}

		if(aip->status & 0x1) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
		}
	} else if(aip->idx == 1 || aip->idx == 2) {
		if(aip->status & 0x1<<2) {
			dev_err(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x, timeout!!!\n",__func__, irq, aip->status);
		}

		if(aip->status & 0x1<<1) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
			complete(&aip->done);
		}

		if(aip->status & 0x1) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
			complete(&aip->done);
		}
	}

	/* enable_irq(irq); */

	return IRQ_HANDLED;
}

static int jz_aip_open(struct inode *inode, struct file *file)
{
	struct miscdevice *mdev = file->private_data;
	struct jz_aip *aip = list_entry(mdev, struct jz_aip, mdev);

	mutex_lock(&aip->mlock);
	__asm__ volatile(
			".set push		\n\t"
			".set mips32r2	\n\t"
			"sync			\n\t"
			"lw $0,0(%0)	\n\t"
			::"r" (0xa0000000));
	*((volatile unsigned int *)(0xb2200000)) = 0x0;
	__asm__ volatile(
			".set push		\n\t"
			".set mips32r2	\n\t"
			"sync			\n\t"
			"lw $0,0(%0)	\n\t"
			::"r" (0xa0000000));
	if (!aip->opencnt) {
		aip->done.done = 0;
		enable_irq(aip->irq);
	}
	aip->opencnt++;
	mutex_unlock(&aip->mlock);

	my_printk("open successful, opencnt=%d!\n", aip->opencnt);
	return 0;
}

static int jz_aip_release(struct inode *inode, struct file *file)
{
	struct miscdevice *mdev = file->private_data;
	struct jz_aip *aip = list_entry(mdev, struct jz_aip, mdev);

	mutex_lock(&aip->mlock);
	if (aip->opencnt == 1) {
		disable_irq(aip->irq);
		aip->done.done = 0;
		aip->opencnt--;
	}
	mutex_unlock(&aip->mlock);
	my_printk("close successful, opencnt=%d!\n", aip->opencnt);

	return 0;
}

static ssize_t jz_aip_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t jz_aip_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static long jz_aip_wait_irq(struct jz_aip *aip, unsigned long arg)
{
	long ret = 0;
	my_printk("aip index=%d wait done....\n", aip->idx);

	ret = wait_for_completion_timeout(&aip->done, msecs_to_jiffies(10*1000));
	if (ret <= 0) {
		dev_err(aip->mdev.this_device, "aip index=%d wait done timeout!\n", aip->idx);
		return -EFAULT;
	}

	if (copy_to_user((void *)arg, &aip->status, sizeof(unsigned int))) {
		dev_err(aip->mdev.this_device, "copy_to_user aip index=%d, stat=0x%x failed!\n", aip->idx, aip->status);
		return -EFAULT;
	}

	return 0;
}

static long jz_aip_alloc_chainbuf(struct jz_aip *aip, unsigned long arg)
{
	long ret = 0;
	/*void *page = NULL;*/
	/*void *endpage = NULL;*/
	struct jz_aip_chainbuf *buf = aip->chainbuf;

#if 0
	if(copy_from_user(buf, (void *)arg, sizeof(struct jz_aip_chainbuf))) {
		dev_err(aip->mdev.this_device, "copy_from_user error!!!\n");
		ret = -EFAULT;
		goto err_copy_from_user;
	}
	buf->size = PAGE_ALIGN(buf->size);
	buf->vaddr = dma_alloc_coherent(aip->mdev.this_device, buf->size, (dma_addr_t *)&buf->paddr, GFP_KERNEL);
	if(buf->vaddr == NULL) {
		dev_err(aip->mdev.this_device, "[%s:%d]:dma_alloc_coherent failed!!!\n",__func__,__LINE__);
		ret = -ENOMEM;
		goto err_dma_alloc_coherent;
	}
	endpage = buf->vaddr + buf->size;
	for(page = buf->vaddr; page < endpage; page += PAGE_SIZE) {
		SetPageReserved(virt_to_page(page));
	}
#endif
	/* printk("alloc: buf.size = %d, buf.paddr = 0x%08x, buf.vaddr = 0x%08x\n",buf->size, buf->paddr, buf->vaddr); */
	if(copy_to_user((void *)arg, buf, sizeof(struct jz_aip_chainbuf))) {
		dev_err(aip->mdev.this_device, "copy_to_user failed!!! %s:%d\n",__func__,__LINE__);
		ret = -ENOMEM;
		goto err_copy_to_user;
	}

	return 0;

err_copy_to_user:
	/*dma_free_coherent(aip->mdev.this_device, buf->size, buf->vaddr, (dma_addr_t)buf->paddr);*/
/*err_dma_alloc_coherent:*/
/*err_copy_from_user:*/
	return ret;
}
static long jz_aip_free_chainbuf(struct jz_aip *aip, unsigned long arg)
{
	long ret = 0;
	/*struct jz_aip_chainbuf *buf = aip->chainbuf;*/

	/* printk("free: buf.size = %d, buf.paddr = 0x%08x, buf.vaddr = 0x%08x\n",buf->size, buf->paddr, buf->vaddr); */
	/*dma_free_coherent(aip->mdev.this_device, buf->size, buf->vaddr, (dma_addr_t)buf->paddr);*/

	return ret;
}

static long jz_aip_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *mdev = file->private_data;
	struct jz_aip *aip = list_entry(mdev, struct jz_aip, mdev);

	switch(cmd) {
	case IOCTL_AIP_IRQ_WAIT_CMP:
		return jz_aip_wait_irq(aip, arg);
	case IOCTL_AIP_MALLOC:
		return jz_aip_alloc_chainbuf(aip, arg);
	case IOCTL_AIP_FREE:
		return jz_aip_free_chainbuf(aip, arg);
	default:
		dev_err(mdev->this_device, "%s:invalid cmd %u\n", __func__, cmd);
		return -1;
	}

	return 0;
}

static int jz_aip_mmap(struct file *file, struct vm_area_struct *vma)
{
	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (io_remap_pfn_range(vma,vma->vm_start,
				vma->vm_pgoff,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

const struct file_operations jz_aip_fops = {
	.owner = THIS_MODULE,
	.open = jz_aip_open,
	.release = jz_aip_release,
	.read = jz_aip_read,
	.write = jz_aip_write,
	.unlocked_ioctl = jz_aip_ioctl,
	.mmap = jz_aip_mmap,
};

static int jz_aip_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *regs = NULL;
	struct jz_aip *aip = NULL;
	unsigned int value = 0;
	unsigned long flags;
	struct jz_aip_chainbuf *buf = NULL;

	aip = kzalloc(sizeof(struct jz_aip), GFP_KERNEL);
	if (!aip) {
		dev_err(&pdev->dev, "kzalloc struct jz_aip failed\n");
		ret = -ENOMEM;
		goto err_kzalloc_aip;
	}

	aip->idx = pdev->id;
	if(aip->idx == 0)
		sprintf(aip->name, "%s", "jzaip_t");
	else if(aip->idx == 1)
		sprintf(aip->name, "%s", "jzaip_f");
	else if(aip->idx == 2)
		sprintf(aip->name, "%s", "jzaip_p");

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "No iomem resource\n");
		ret = -ENXIO;
		goto err_get_aip_resource;
	}

	aip->iomem = ioremap(regs->start, resource_size(regs));
	if (!aip->iomem) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENXIO;
		goto err_get_aip_iomem;
	}

#ifndef CONFIG_FPGA_TEST
	aip->clk_gate = clk_get(&pdev->dev, "gate_aip");
	if (IS_ERR(aip->clk_gate)) {
		dev_err(&pdev->dev, "clk_gate get aip failed\n");
		ret = PTR_ERR(aip->clk_gate);
		goto err_get_aip_clk_gate;
	}
	clk_prepare_enable(aip->clk_gate);
#endif

	aip->irq = platform_get_irq(pdev, 0);
	if(aip->irq < 0) {
		dev_err(&pdev->dev, "get irq failed\n");
		goto err_get_aip_irq;
	}
	ret = request_irq(aip->irq, jz_aip_interrupt_t, IRQF_DISABLED, aip->name, aip);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq(%d) failed\n", aip->irq);
		goto err_aip_helix_irq;
	}
	disable_irq_nosync(aip->irq);

	init_completion(&aip->done);
	spin_lock_init(&aip->slock);
	mutex_init(&aip->mlock);
	aip->opencnt = 0;


	spin_lock_irqsave(&aip->slock, flags);
	value = *(volatile unsigned int *)(0xb00000f0);
	value |= (1<<28);
	*(volatile unsigned int *)(0xb00000f0) = value;
	spin_unlock_irqrestore(&aip->slock, flags);

	printk("cpm volatile value = 0x%x\n", value);
	msleep(10);

	spin_lock_irqsave(&aip->slock, flags);
	value = *(volatile unsigned int *)(0xb00000f0);
	value &= ~(1<<28);
	*(volatile unsigned int *)(0xb00000f0) = value;
	spin_unlock_irqrestore(&aip->slock, flags);

	aip->mdev.minor = MISC_DYNAMIC_MINOR;
	aip->mdev.fops = &jz_aip_fops;
	aip->mdev.name = aip->name;

	aip->chainbuf = kzalloc(sizeof(struct jz_aip_chainbuf), GFP_KERNEL);
	if(aip->chainbuf == NULL) {
		printk("[%s %d] kmalloc error\n",__func__,__LINE__);
		return -ENOMEM;
	}else{
		buf = aip->chainbuf;
		buf->size = 8192;
		buf->vaddr = dma_alloc_coherent(aip->mdev.this_device, buf->size, (dma_addr_t *)&buf->paddr, GFP_KERNEL);
		if(buf->vaddr == NULL) {
			dev_err(aip->mdev.this_device, "[%s:%d]:dma_alloc_coherent failed!!!\n",__func__,__LINE__);
			ret = -ENOMEM;
			goto err_dma_alloc_coherent;
		}
	}
	printk("dma_malloc is ok!\n");

	ret = misc_register(&aip->mdev);
	if(ret < 0) {
		dev_err(&pdev->dev,"request aip misc device failed!\n");
		goto err_aip_register;
	}

	platform_set_drvdata(pdev, aip);
	printk("The version of AIP driver is %s\n", AIP_DRIVER_VERSION);
	return 0;

err_aip_register:
	dma_free_coherent(aip->mdev.this_device, buf->size, buf->vaddr, (dma_addr_t)buf->paddr);
	kfree(aip->chainbuf);
err_dma_alloc_coherent:
	free_irq(aip->irq, aip);
err_aip_helix_irq:
err_get_aip_irq:
#ifndef CONFIG_FPGA_TEST
	clk_disable_unprepare(aip->clk_gate);
	clk_put(aip->clk_gate);
err_get_aip_clk_gate:
#endif
	iounmap(aip->iomem);
err_get_aip_iomem:
err_get_aip_resource:
	kfree(aip);
	aip = NULL;
err_kzalloc_aip:
	return ret;
}

static int jz_aip_remove(struct platform_device *dev)
{
	struct jz_aip *aip = platform_get_drvdata(dev);
	struct jz_aip_chainbuf *buf = NULL;
	buf = aip->chainbuf;

	if(buf->vaddr)
		dma_free_coherent(aip->mdev.this_device, buf->size, buf->vaddr, (dma_addr_t)buf->paddr);
	misc_deregister(&aip->mdev);
	kfree(aip->chainbuf);

	free_irq(aip->irq, aip);

#ifndef CONFIG_FPGA_TEST
	clk_disable_unprepare(aip->clk_gate);
	clk_put(aip->clk_gate);
#endif
	iounmap(aip->iomem);
	kfree(aip);
	aip = NULL;
	return 0;
}


static struct platform_driver jz_aip_driver = {
	.probe		= jz_aip_probe,
	.remove		= jz_aip_remove,
	.driver		= {
		.name	= "jz-aip",
	},
};

static int __init aip_init(void)
{
	int ret = 0;
	int i;
	for(i = 0; i < JZ_AIP_NUM; i++) {
		ret = platform_device_register(&jz_aip_device[i]);
		if (ret < 0) {
			printk("%s platform_device_register aip_%d failed:ret=0x%x\n", __func__, i, ret);
			goto err_platform_device_register;
		}
	}
	ret = platform_driver_register(&jz_aip_driver);
	if(ret){
		printk("%s platform_driver_register failed:ret=0x%x\n", __func__, ret);
		goto err_platform_driver_register;
	}

	printk("%s success\n", __func__);
	return 0;

err_platform_driver_register:
	for(--i; i >= 0; i--)
		platform_device_unregister(&jz_aip_device[i]);
	i = JZ_AIP_NUM;
err_platform_device_register:
	for(--i; i >= 0; i--)
		platform_device_unregister(&jz_aip_device[i]);
	return ret;
}

static void __exit aip_exit(void)
{
	int i;
	platform_driver_unregister(&jz_aip_driver);
	i = JZ_AIP_NUM;
	for(--i; i >= 0; i--)
		platform_device_unregister(&jz_aip_device[i]);
}

module_init(aip_init);
module_exit(aip_exit);

MODULE_DESCRIPTION("JZ AIP Driver");
MODULE_AUTHOR("Danny<yiwen.han@ingenic.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("20210520");



