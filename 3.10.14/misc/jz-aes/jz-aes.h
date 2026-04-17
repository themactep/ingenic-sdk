/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Ingenic JZ AES hardware accelerator
 *
 * Supports all T-series SoCs:
 *   T10-T31, T40/T41 (PRJ007 register layout)
 *   T32/T33           (PRJ008 register layout)
 *
 * AES-128/192/256 ECB/CBC, DMA mode, interrupt-driven.
 */
#ifndef __JZ_AES_H__
#define __JZ_AES_H__

#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/clk.h>

/* ioctl interface */
#define JZAES_IOC_MAGIC		'A'
#define IOCTL_AES_GET_PARA	_IOW(JZAES_IOC_MAGIC, 110, unsigned int)
#define IOCTL_AES_PROCESSING	_IOW(JZAES_IOC_MAGIC, 111, unsigned int)
#define IOCTL_AES_INIT		_IOW(JZAES_IOC_MAGIC, 112, unsigned int)
#define IOCTL_AES_DO		_IOW(JZAES_IOC_MAGIC, 113, unsigned int)

enum jz_aes_status {
	JZ_AES_STATUS_PREPARE = 0,
	JZ_AES_STATUS_WORKING,
	JZ_AES_STATUS_DONE,
};

enum jz_aes_work_mode {
	JZ_AES_MODE_ENC_ECB = 0,
	JZ_AES_MODE_DEC_ECB = 1,
	JZ_AES_MODE_ENC_CBC = 2,
	JZ_AES_MODE_DEC_CBC = 3,
	JZ_AES_MODE_OTHER   = 4,
};

struct aes_para {
	unsigned int status;
	unsigned int enworkmode;
	unsigned int aeskey[8];		/* up to AES-256 (8 × 32-bit) */
	unsigned int aesiv[4];
	unsigned char *src;
	unsigned char *dst;
	unsigned int datalen;
	unsigned int donelen;
	int keybits;			/* 128, 192, or 256 (0 = default 128) */
	unsigned int flags;		/* AES_FLAG_* */
};

/* aes_para.flags */
#define AES_FLAG_BSWAP	(1 << 0)	/* byte-swap each 32-bit word during DMA copy */

/* Per-fd session — each open() gets one */
struct aes_session {
	struct aes_para para;
	struct aes_operation *aes;	/* back-pointer to device */
	/* key cache — skip re-expansion when key+mode unchanged */
	unsigned int cached_key[8];
	int cached_keybits;
	int cached_mode;
	int key_loaded;			/* 1 if hardware has this key */
};

/* Global device state — one per hardware engine */
struct aes_operation {
	struct miscdevice aes_dev;
	struct resource *res;
	void __iomem *iomem;
	struct clk *clk;
	struct device *dev;
	int irq;
	char name[16];
	dma_addr_t src_addr_p;
	unsigned char *src_addr_v;
	dma_addr_t dst_addr_p;
	unsigned char *dst_addr_v;
	unsigned int dma_datalen;
	struct mutex hw_mutex;		/* serializes hardware access */
	struct completion done_comp;
	struct completion key_comp;
	/* status tracked during hw operation (under hw_mutex) */
	unsigned int hw_status;
};

#define JZ_AES_DMA_DATALEN	(16 * 1024)

#define miscdev_to_aesops(mdev) \
	container_of(mdev, struct aes_operation, aes_dev)

/*
 * Register offsets
 *
 * PRJ007 layout: T10-T31, T40/T41 (original AES block)
 * PRJ008 layout: T32/T33 (revised AES block, different offsets after ASSA)
 */
#define AES_ASCR	0x00
#define AES_ASSR	0x04
#define AES_ASINTM	0x08
#define AES_ASSA	0x0c

#if defined(CONFIG_SOC_PRJ008)
/* PRJ008: reordered, plus ASDTC and ASBNUM */
#define AES_ASTC	0x10	/* source transfer count (words) */
#define AES_ASDA	0x14
#define AES_ASDTC	0x18	/* destination transfer count (words) */
#define AES_ASBNUM	0x1c	/* block number (16-byte blocks) */
#define AES_ASDI	0x20
#define AES_ASDO	0x24
#define AES_ASKY	0x28
#define AES_ASIV	0x2c
#else
/* PRJ007: original layout (T10-T41) */
#define AES_ASDA	0x10
#define AES_ASTC	0x14	/* transfer count (16-byte blocks) */
#define AES_ASDI	0x18
#define AES_ASDO	0x1c
#define AES_ASKY	0x20
#define AES_ASIV	0x24
#endif

/* ASCR bits — common to all variants */
#define ASCR_EN		(1 << 0)
#define ASCR_INIT_IV	(1 << 1)
#define ASCR_KEYS	(1 << 2)
#define ASCR_AESS	(1 << 3)
#define ASCR_DECRYPT	(1 << 4)
#define ASCR_CBC	(1 << 5)
#define ASCR_KEYL_SHIFT	6		/* bits 6-7: key length */
#define ASCR_KEYL_128	(0 << 6)
#define ASCR_KEYL_192	(1 << 6)
#define ASCR_KEYL_256	(2 << 6)
#define ASCR_DMAE	(1 << 8)
#define ASCR_DMAS	(1 << 9)
#define ASCR_CLR	(1 << 10)
#define ASCR_LE_IN	(1 << 12)	/* little-endian DMA input (T32+) */
#define ASCR_LE_OUT	(1 << 13)	/* little-endian DMA output (T33/PRJ008) */

/* ASSR bits */
#if defined(CONFIG_SOC_PRJ008)
#define ASSR_AESD	(1 << 0)
#define ASSR_DMAD	(1 << 1)
#else
#define ASSR_KEYD	(1 << 0)
#define ASSR_AESD	(1 << 1)
#define ASSR_DMAD	(1 << 2)
#endif

/* ASINTM bits */
#if defined(CONFIG_SOC_PRJ008)
#define ASINTM_AES	(1 << 0)
#define ASINTM_DMA	(1 << 1)
#else
#define ASINTM_KEY	(1 << 0)
#define ASINTM_AES	(1 << 1)
#define ASINTM_DMA	(1 << 2)
#endif

static inline unsigned int aes_reg_read(struct aes_operation *aes, int offset)
{
	return readl(aes->iomem + offset);
}

static inline void aes_reg_write(struct aes_operation *aes, int offset,
				 unsigned int val)
{
	writel(val, aes->iomem + offset);
}

#endif /* __JZ_AES_H__ */
