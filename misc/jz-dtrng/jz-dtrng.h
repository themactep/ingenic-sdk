#ifndef __JZ_DTRNG_H__
#define __JZ_DTRNG_H__

#define JZDTRNG_IOC_MAGIC  'D'
#define IOCTL_DTRNG_DMA_GET_RANDOM					_IO(JZDTRNG_IOC_MAGIC, 110)
#define IOCTL_DTRNG_CPU_GET_RANDOM					_IO(JZDTRNG_IOC_MAGIC, 111)
#define IOCTL_DTRNG_GET_CNT							_IO(JZDTRNG_IOC_MAGIC, 112)

#define DTRNG_CFG				0x00//dtrng control register
#define DTRNG_RANDOMNUM			0x04//dtrng random num register
#define DTRNG_STAT				0x08//dtrng stat register

typedef struct dtrng_operation {
	struct miscdevice dtrng_dev;
	struct resource *res;
	int state;
	void __iomem *iomem;
	struct clk *clk;
	struct device *dev;
	int irq;
	char name[16];
	unsigned char *sbuff;
	unsigned int random[1];
	struct completion dtrng_complete;
}dtrng_operation_t;

#define miscdev_to_dtrngops(mdev) (container_of(mdev, struct dtrng_operation, dtrng_dev))

static inline unsigned int dtrng_reg_read(struct dtrng_operation *dtrng_ope, int offset)
{
//	printk("%s, read:0x%08x, val = 0x%08x\n", __func__, dtrng_ope->iomem + offset, readl(dtrng_ope->iomem + offset));
	return readl(dtrng_ope->iomem + offset);
}

static inline void dtrng_reg_write(struct dtrng_operation *dtrng_ope, int offset, unsigned int val)
{
	writel(val, dtrng_ope->iomem + offset);
//	printk("%s, write:0x%08x, val = 0x%08x\n", __func__, dtrng_ope->iomem + offset, val);
}

void dtrng_bit_set(struct dtrng_operation *dtrng_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = dtrng_reg_read(dtrng_ope, offset);
	tmp |= (1 << bit);
	dtrng_reg_write(dtrng_ope, offset, tmp);
}

void dtrng_bit_clr(struct dtrng_operation *dtrng_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = dtrng_reg_read(dtrng_ope, offset);
	tmp &= ~(1 << bit);
	dtrng_reg_write(dtrng_ope, offset, tmp);
}

#endif
