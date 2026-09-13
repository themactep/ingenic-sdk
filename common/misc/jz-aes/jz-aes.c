// SPDX-License-Identifier: GPL-2.0
/*
 * Ingenic JZ AES hardware accelerator driver
 *
 * Based on Ingenic T40 SDK sample_aes driver by Niky Shen,
 * with AES-192/256 and T32/T33 support from Tassadar SDK.
 *
 * Supports all T-series SoCs:
 *   T10-T31, T40/T41  — PRJ007 register layout
 *   T32/T33            — PRJ008 register layout (different offsets, LE bits)
 *
 * AES-128/192/256 ECB/CBC, DMA mode, interrupt-driven completion.
 * Multiple opens supported — per-fd sessions, mutex around HW access.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <soc/base.h>
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
#include <dt-bindings/interrupt-controller/t40-irq.h>
#elif defined(CONFIG_SOC_PRJ007)
#include <dt-bindings/interrupt-controller/PRJ007-irq.h>
#elif defined(CONFIG_SOC_PRJ008)
#include <dt-bindings/interrupt-controller/PRJ008-irq.h>
#else
#include <soc/irq.h>
#endif

#include "jz-aes.h"

/*
 * DMA allocation compat — dma_alloc_noncoherent changed signature in 5.9.
 * Use non-coherent for performance (2-3x faster than coherent on MIPS),
 * with explicit cache sync before/after DMA transfers.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
#define jz_dma_alloc(dev, sz, dma, gfp) \
	dma_alloc_noncoherent(dev, sz, dma, DMA_BIDIRECTIONAL, gfp)
#define jz_dma_free(dev, sz, va, dma) \
	dma_free_noncoherent(dev, sz, va, dma, DMA_BIDIRECTIONAL)
#else
#define jz_dma_alloc(dev, sz, dma, gfp) \
	dma_alloc_noncoherent(dev, sz, dma, gfp)
#define jz_dma_free(dev, sz, va, dma) \
	dma_free_noncoherent(dev, sz, va, dma)
#endif

/*
 * Resolve key length to ASCR bits 6-7 and number of key words.
 * Returns 0 on success, -EINVAL on bad keybits.
 */
static int aes_keyl_config(int keybits, unsigned int *ascr_keyl, int *nwords)
{
	switch (keybits) {
	case 0:		/* default = 128 */
	case 128:
		*ascr_keyl = ASCR_KEYL_128;
		*nwords = 4;
		return 0;
	case 192:
		*ascr_keyl = ASCR_KEYL_192;
		*nwords = 6;
		return 0;
	case 256:
		*ascr_keyl = ASCR_KEYL_256;
		*nwords = 8;
		return 0;
	default:
		return -EINVAL;
	}
}

/*
 * Byte-swap each 32-bit word in a kernel buffer (in-place).
 * Used after bulk copy_from_user / before bulk copy_to_user.
 */
static void bswap_buf32(unsigned char *buf, int len)
{
	unsigned int *p = (unsigned int *)buf;
	int n = len / 4;
	int i;

	for (i = 0; i < n; i++)
		p[i] = swab32(p[i]);
}

/*
 * Check if we can skip key re-expansion (same key+mode+keybits as last time).
 */
static int key_matches(struct aes_session *sess, struct aes_para *p, int nwords)
{
	if (!sess->key_loaded)
		return 0;
	if (sess->cached_keybits != p->keybits)
		return 0;
	if (sess->cached_mode != (int)p->enworkmode)
		return 0;
	return memcmp(sess->cached_key, p->aeskey, nwords * 4) == 0;
}

static void key_save(struct aes_session *sess, struct aes_para *p, int nwords)
{
	memcpy(sess->cached_key, p->aeskey, nwords * 4);
	sess->cached_keybits = p->keybits;
	sess->cached_mode = (int)p->enworkmode;
	sess->key_loaded = 1;
}

/*
 * Configure hardware and run a complete AES operation.
 * Caller must hold aes->hw_mutex.
 * If sess is non-NULL, uses key caching to skip re-expansion.
 */
static int jz_aes_do_operation(struct aes_operation *aes, struct aes_para *p,
			       struct aes_session *sess)
{
	unsigned int ascr, ascr_keyl;
	int i, nwords, timeout, offset = 0, len, ret = 0;
	int skip_key;

	/* validate */
	if (p->datalen % 16)
		return -EINVAL;
	if (p->enworkmode >= JZ_AES_MODE_OTHER)
		return -EINVAL;
	if (aes_keyl_config(p->keybits, &ascr_keyl, &nwords))
		return -EINVAL;

	skip_key = sess && key_matches(sess, p, nwords);

	/* --- hardware init --- */

	/* clear status */
#if defined(CONFIG_SOC_PRJ008)
	aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR) & ASSR_DMAD);
#else
	aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR));
#endif

	/* enable interrupts */
#if defined(CONFIG_SOC_PRJ008)
	aes_reg_write(aes, AES_ASINTM, ASINTM_DMA);
#else
	aes_reg_write(aes, AES_ASINTM,
		      ASINTM_DMA | (skip_key ? 0 : ASINTM_KEY));
#endif

	/* clear IV/keys, enable AES */
	aes_reg_write(aes, AES_ASCR, ASCR_CLR | ASCR_EN);

	/* configure mode + key length */
	ascr = ASCR_EN | ASCR_DMAE | ascr_keyl;
	switch (p->enworkmode) {
	case JZ_AES_MODE_ENC_ECB:
		break;
	case JZ_AES_MODE_DEC_ECB:
		ascr |= ASCR_DECRYPT;
		break;
	case JZ_AES_MODE_ENC_CBC:
		ascr |= ASCR_CBC;
		break;
	case JZ_AES_MODE_DEC_CBC:
		ascr |= ASCR_CBC | ASCR_DECRYPT;
		break;
	default:
		return -EINVAL;
	}
	aes_reg_write(aes, AES_ASCR, ascr);

	/* LE mode — no-op on T10-T31, functional on T32+ */
	aes_reg_write(aes, AES_ASCR,
		      aes_reg_read(aes, AES_ASCR) | ASCR_LE_IN | ASCR_LE_OUT);

	/* load IV for CBC modes */
	if (p->enworkmode == JZ_AES_MODE_ENC_CBC ||
	    p->enworkmode == JZ_AES_MODE_DEC_CBC) {
		for (i = 0; i < 4; i++)
			aes_reg_write(aes, AES_ASIV, p->aesiv[i]);
		aes_reg_write(aes, AES_ASCR,
			      aes_reg_read(aes, AES_ASCR) | ASCR_INIT_IV);
	}

	if (!skip_key) {
		/* load key and trigger key expansion */
		aes->hw_status = JZ_AES_STATUS_PREPARE;
		init_completion(&aes->key_comp);
		for (i = 0; i < nwords; i++)
			aes_reg_write(aes, AES_ASKY, p->aeskey[i]);
		aes_reg_write(aes, AES_ASCR,
			      aes_reg_read(aes, AES_ASCR) | ASCR_KEYS);

		/* wait for key expansion (PRJ008 has no KEY interrupt) */
#if !defined(CONFIG_SOC_PRJ008)
		timeout = 1000;
		while (timeout--) {
			if (!wait_for_completion_interruptible(&aes->key_comp))
				break;
		}
		if (timeout <= 0)
			return -ETIMEDOUT;
#endif
		if (sess)
			key_save(sess, p, nwords);
	}

	/* set DMA addresses */
	aes_reg_write(aes, AES_ASSA, aes->src_addr_p);
	aes_reg_write(aes, AES_ASDA, aes->dst_addr_p);

	/* --- DMA processing in chunks --- */

	p->donelen = 0;
	aes->hw_status = JZ_AES_STATUS_WORKING;

	while (offset < p->datalen) {
		len = min_t(int, p->datalen - offset, aes->dma_datalen);

		if (copy_from_user(aes->src_addr_v,
				   (void __user *)(p->src + offset), len)) {
			ret = -EFAULT;
			break;
		}
		if (p->flags & AES_FLAG_BSWAP)
			bswap_buf32(aes->src_addr_v, len);

		dma_sync_single_for_device(NULL, aes->src_addr_p,
					   len, DMA_TO_DEVICE);
		dma_sync_single_for_device(NULL, aes->dst_addr_p,
					   len, DMA_FROM_DEVICE);

		init_completion(&aes->done_comp);

		/* set transfer count */
#if defined(CONFIG_SOC_PRJ008)
		aes_reg_write(aes, AES_ASBNUM, len / 16);
		aes_reg_write(aes, AES_ASTC, len / 4);
		aes_reg_write(aes, AES_ASDTC, len / 4);
#else
		aes_reg_write(aes, AES_ASTC, len / 16);
#endif

		/* start DMA */
		aes_reg_write(aes, AES_ASCR,
			      aes_reg_read(aes, AES_ASCR) | ASCR_DMAS);

		/* wait for DMA completion */
		timeout = 1000;
		while (timeout--) {
			if (!wait_for_completion_interruptible(&aes->done_comp))
				break;
		}
		if (timeout <= 0) {
			ret = -ETIMEDOUT;
			break;
		}

		dma_sync_single_for_cpu(NULL, aes->dst_addr_p,
					len, DMA_FROM_DEVICE);

		if (p->flags & AES_FLAG_BSWAP)
			bswap_buf32(aes->dst_addr_v, len);

		if (copy_to_user((void __user *)(p->dst + offset),
				 aes->dst_addr_v, len)) {
			ret = -EFAULT;
			break;
		}

		offset += len;
		p->donelen = offset;
	}

	aes->hw_status = JZ_AES_STATUS_DONE;
	p->status = JZ_AES_STATUS_DONE;
	return ret;
}

/* --- file operations ---------------------------------------------------- */

static int aes_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);
	struct aes_session *sess;

	sess = kzalloc(sizeof(*sess), GFP_KERNEL);
	if (!sess)
		return -ENOMEM;

	sess->aes = aes;
	sess->para.status = JZ_AES_STATUS_DONE;
	file->private_data = sess;
	return 0;
}

static int aes_release(struct inode *inode, struct file *file)
{
	struct aes_session *sess = file->private_data;

	kfree(sess);
	return 0;
}

static long aes_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct aes_session *sess = file->private_data;
	struct aes_operation *aes = sess->aes;
	void __user *argp = (void __user *)arg;
	int ret;

	switch (cmd) {
	case IOCTL_AES_INIT:
		/* store params in session — no hardware access */
		if (copy_from_user(&sess->para, argp, sizeof(sess->para)))
			return -EFAULT;
		sess->para.status = JZ_AES_STATUS_DONE;
		return 0;

	case IOCTL_AES_PROCESSING:
		/* update params from user (src/dst/datalen may change) */
		if (copy_from_user(&sess->para, argp, sizeof(sess->para)))
			return -EFAULT;

		/* serialize hardware access */
		mutex_lock(&aes->hw_mutex);
		ret = jz_aes_do_operation(aes, &sess->para, NULL);
		mutex_unlock(&aes->hw_mutex);

		/* return updated para (donelen, status) */
		if (copy_to_user(argp, &sess->para, sizeof(sess->para)))
			return -EFAULT;
		return ret;

	case IOCTL_AES_DO:
		/*
		 * Combined init + processing in one syscall.
		 * Uses per-session key cache to skip re-expansion.
		 */
		if (copy_from_user(&sess->para, argp, sizeof(sess->para)))
			return -EFAULT;

		mutex_lock(&aes->hw_mutex);
		ret = jz_aes_do_operation(aes, &sess->para, sess);
		mutex_unlock(&aes->hw_mutex);

		if (copy_to_user(argp, &sess->para, sizeof(sess->para)))
			return -EFAULT;
		return ret;

	case IOCTL_AES_GET_PARA:
		if (copy_to_user(argp, &sess->para, sizeof(sess->para)))
			return -EFAULT;
		return 0;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations aes_fops = {
	.owner		= THIS_MODULE,
	.open		= aes_open,
	.release	= aes_release,
	.unlocked_ioctl	= aes_ioctl,
};

/* --- IRQ handler -------------------------------------------------------- */

static irqreturn_t aes_irq_handler(int irq, void *data)
{
	struct aes_operation *aes = data;
	unsigned int status = aes_reg_read(aes, AES_ASSR);
	unsigned int mask = aes_reg_read(aes, AES_ASINTM);
	unsigned int pending = status & mask;

	/* clear all pending */
	aes_reg_write(aes, AES_ASSR, status);

	if ((pending & ASSR_DMAD) &&
	    aes->hw_status == JZ_AES_STATUS_WORKING)
		complete(&aes->done_comp);

#if !defined(CONFIG_SOC_PRJ008)
	if ((pending & ASSR_KEYD) &&
	    aes->hw_status == JZ_AES_STATUS_PREPARE)
		complete(&aes->key_comp);
#endif

	return IRQ_HANDLED;
}

/* --- platform driver ---------------------------------------------------- */

static int jz_aes_probe(struct platform_device *pdev)
{
	struct aes_operation *aes;
	int ret;

	aes = kzalloc(sizeof(*aes), GFP_KERNEL);
	if (!aes)
		return -ENOMEM;

	snprintf(aes->name, sizeof(aes->name), "jz-aes");

	/* map registers */
	aes->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!aes->res) {
		dev_err(&pdev->dev, "no memory resource\n");
		ret = -EINVAL;
		goto err_free;
	}
	aes->res = request_mem_region(aes->res->start,
				      resource_size(aes->res), pdev->name);
	if (!aes->res) {
		dev_err(&pdev->dev, "failed to request mem region\n");
		ret = -EBUSY;
		goto err_free;
	}
	aes->iomem = ioremap(aes->res->start, resource_size(aes->res));
	if (!aes->iomem) {
		dev_err(&pdev->dev, "failed to ioremap\n");
		ret = -ENOMEM;
		goto err_release;
	}

	/* IRQ */
	aes->irq = platform_get_irq(pdev, 0);
	if (aes->irq <= 0) {
		dev_err(&pdev->dev, "no IRQ\n");
		ret = -EINVAL;
		goto err_unmap;
	}
	ret = request_irq(aes->irq, aes_irq_handler, IRQF_SHARED,
			  aes->name, aes);
	if (ret) {
		dev_err(&pdev->dev, "failed to request IRQ %d\n", aes->irq);
		goto err_unmap;
	}

	/* DMA buffers */
	aes->src_addr_v = jz_dma_alloc(&pdev->dev, JZ_AES_DMA_DATALEN,
				       &aes->src_addr_p, GFP_KERNEL);
	if (!aes->src_addr_v) {
		dev_err(&pdev->dev, "failed to alloc DMA src\n");
		ret = -ENOMEM;
		goto err_irq;
	}
	aes->dst_addr_v = jz_dma_alloc(&pdev->dev, JZ_AES_DMA_DATALEN,
				       &aes->dst_addr_p, GFP_KERNEL);
	if (!aes->dst_addr_v) {
		dev_err(&pdev->dev, "failed to alloc DMA dst\n");
		ret = -ENOMEM;
		goto err_dma_src;
	}
	aes->dma_datalen = JZ_AES_DMA_DATALEN;

	/* misc device */
	aes->aes_dev.minor = MISC_DYNAMIC_MINOR;
	aes->aes_dev.fops = &aes_fops;
	aes->aes_dev.name = "aes";
	ret = misc_register(&aes->aes_dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register misc device\n");
		goto err_dma_dst;
	}

	/* ungate AES clock */
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	aes->clk = clk_get(&pdev->dev, "gate_aes");
	if (IS_ERR(aes->clk)) {
		dev_err(&pdev->dev, "clk_get(gate_aes) failed\n");
		ret = PTR_ERR(aes->clk);
		goto err_misc;
	}
	clk_prepare_enable(aes->clk);
#else
	aes->clk = clk_get(&pdev->dev, "aes");
	if (IS_ERR(aes->clk)) {
		dev_err(&pdev->dev, "clk_get(aes) failed\n");
		ret = PTR_ERR(aes->clk);
		goto err_misc;
	}
	clk_enable(aes->clk);
#endif

	mutex_init(&aes->hw_mutex);
	init_completion(&aes->done_comp);
	init_completion(&aes->key_comp);
	aes->hw_status = JZ_AES_STATUS_DONE;

	platform_set_drvdata(pdev, aes);
	dev_info(&pdev->dev, "AES engine ready (DMA %uK, 128/192/256-bit)\n",
		 JZ_AES_DMA_DATALEN / 1024);
	return 0;

err_misc:
	misc_deregister(&aes->aes_dev);
err_dma_dst:
	jz_dma_free(&pdev->dev, JZ_AES_DMA_DATALEN,
		    aes->dst_addr_v, aes->dst_addr_p);
err_dma_src:
	jz_dma_free(&pdev->dev, JZ_AES_DMA_DATALEN,
		    aes->src_addr_v, aes->src_addr_p);
err_irq:
	free_irq(aes->irq, aes);
err_unmap:
	iounmap(aes->iomem);
err_release:
	release_mem_region(aes->res->start, resource_size(aes->res));
err_free:
	kfree(aes);
	return ret;
}

static int jz_aes_remove(struct platform_device *pdev)
{
	struct aes_operation *aes = platform_get_drvdata(pdev);

	misc_deregister(&aes->aes_dev);
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	clk_disable_unprepare(aes->clk);
#else
	clk_disable(aes->clk);
#endif
	clk_put(aes->clk);
	jz_dma_free(&pdev->dev, JZ_AES_DMA_DATALEN,
		    aes->dst_addr_v, aes->dst_addr_p);
	jz_dma_free(&pdev->dev, JZ_AES_DMA_DATALEN,
		    aes->src_addr_v, aes->src_addr_p);
	free_irq(aes->irq, aes);
	iounmap(aes->iomem);
	release_mem_region(aes->res->start, resource_size(aes->res));
	kfree(aes);
	return 0;
}

static struct platform_driver jz_aes_driver = {
	.probe	= jz_aes_probe,
	.remove	= jz_aes_remove,
	.driver	= {
		.name	= "jz-aes",
		.owner	= THIS_MODULE,
	},
};

/* --- self-registered platform device (out-of-tree) ---------------------- */

static struct resource jz_aes_resources[] = {
	[0] = {
		.start	= AES_IOBASE,
		.end	= AES_IOBASE + 0x30,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41) || \
    defined(CONFIG_SOC_PRJ007) || defined(CONFIG_SOC_PRJ008)
		.start	= IRQ_AES + 8,
		.end	= IRQ_AES + 8,
#else
		.start	= IRQ_AES,
		.end	= IRQ_AES,
#endif
		.flags	= IORESOURCE_IRQ,
	},
};

static void jz_aes_dev_release(struct device *dev) { }

static struct platform_device jz_aes_device = {
	.name		= "jz-aes",
	.id		= 0,
	.resource	= jz_aes_resources,
	.num_resources	= ARRAY_SIZE(jz_aes_resources),
	.dev = {
		.release = jz_aes_dev_release,
	},
};

static int __init jz_aes_mod_init(void)
{
	int ret;

	ret = platform_device_register(&jz_aes_device);
	if (ret) {
		printk(KERN_ERR "jz-aes: failed to register device\n");
		return ret;
	}
	ret = platform_driver_register(&jz_aes_driver);
	if (ret) {
		platform_device_unregister(&jz_aes_device);
		return ret;
	}
	return 0;
}

static void __exit jz_aes_mod_exit(void)
{
	platform_driver_unregister(&jz_aes_driver);
	platform_device_unregister(&jz_aes_device);
}

module_init(jz_aes_mod_init);
module_exit(jz_aes_mod_exit);

MODULE_DESCRIPTION("Ingenic JZ AES hardware accelerator");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ingenic / thingino");
