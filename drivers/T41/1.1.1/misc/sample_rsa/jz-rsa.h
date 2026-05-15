#ifndef __JZ_RSA_H__
#define __JZ_RSA_H__

#define JZRSA_IOC_MAGIC        'R'
#define IOCTL_RSA_ENC_DEC _IOW(JZRSA_IOC_MAGIC, 110, unsigned int)
#define RSAC   0x00
#define RSAE   0x04
#define RSAN   0x08
#define RSAM   0x0C
#define RSAP   0X10


#define miscdev_to_rsaops(mdev) (container_of(mdev, struct rsa_operation, rsa_dev))

struct rsa_data {
	unsigned int *e_or_d;
    unsigned int *n;
    unsigned int rsa_mode;
    unsigned int *input;
    unsigned int inlen;
    unsigned int *output;
};

typedef struct rsa_operation{
	char name[16];
	int state;
	struct miscdevice rsa_dev;
	struct resource *io_res;
	struct resource *dma_res;
	void __iomem *iomem;
	struct clk *clk;
#if (defined(CONFIG_SOC_T41)&&defined(CONFIG_KERNEL_4_4_94)) || defined(CONFIG_SOC_T40)
	struct clk *divclk;
#endif
	struct device *dev;
	struct rsa_data rsa_data;
	int irq;
	struct completion pre_proc;
	struct completion main_proc;
}rsa_operation_t;

static inline unsigned int rsa_reg_read(rsa_operation_t *rsa_ope, int offset)
{
	//printk("%s, read:0x%08x, val = 0x%08x\n", __func__, rsa_ope->iomem + offset, readl(rsa_ope->iomem + offset));
	return readl(rsa_ope->iomem + offset);
}

static inline void rsa_reg_write(rsa_operation_t *rsa_ope, int offset, unsigned int val)
{
	//printk("%s, write:0x%08x, val = 0x%08x\n", __func__, rsa_ope->iomem + offset, val);
	writel(val, rsa_ope->iomem + offset);
}

void rsa_bit_set(rsa_operation_t *rsa_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = rsa_reg_read(rsa_ope, offset);
	tmp |= bit;
	rsa_reg_write(rsa_ope, offset, tmp);
}

void rsa_bit_clr(rsa_operation_t *rsa_ope, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = rsa_reg_read(rsa_ope, offset);
	tmp &= ~(bit);
	rsa_reg_write(rsa_ope, offset, tmp);
}

#endif
