#ifndef __DWU__H__
#define __DWU__H__



#define DWU_BASE	0xB34F0000

#define DWU_LOCK                (0xd8)
#define DWU_CTRL	            (0x180)   /* WR */
#define DWU_PROTECT_START_ADDR  (0x188)   /* WR */
#define DWU_PROTECT_END_ADDR	(0x190)   /* WR */
#define DWU_BAD_ADDR	        (0x198)   /* WR */
#define DWU_DEVICE0_CTRL	    (0x1a0)   /* WR */
#define DWU_DEVICE1_CTRL	    (0x1a8)   /* WR */
#define DWU_DEVICE2_CTRL	    (0x1b0)   /* WR */
#define DWU_DEVICE3_CTRL	    (0x1b8)   /* WR */
#define DWU_INT_CLR	            (0x1c0)   /* WR */
#define DWU_ERR_ADDR	        (0x1c8)   /* WR */
#define DWU_ERR_INFO	        (0x1d0)   /* R */
#define DWU_INT_REG	            (0x1d8)   /* R */


#define u32 unsigned int

struct dwu_reg_struct {
	char *name;
	unsigned int addr;
};

struct jz_dwu {

	int irq;
	char name[16];

	struct clk *clk;
	void __iomem *iomem;
	struct device *dev;
	struct resource *res;
	struct miscdevice misc_dev;

	struct mutex mutex;
	struct completion done_dwu;
	struct completion done_buf;
};

struct dwu_param
{
	unsigned int 		dwu_id0;
	unsigned int 		dwu_id1;
	unsigned int 		dwu_id2;
	unsigned int 		dwu_id3;
	unsigned int 		dwu_start_addr;
	unsigned int 		dwu_end_addr;
	unsigned int 		dwu_err_addr;
	unsigned int		dwu_dev_num;

};


#define JZDWU_IOC_MAGIC  'U'
#define IOCTL_DWU_START			_IO(JZDWU_IOC_MAGIC, 106)
#define IOCTL_DWU_RES_PBUFF		_IO(JZDWU_IOC_MAGIC, 114)
#define IOCTL_DWU_GET_PBUFF		_IO(JZDWU_IOC_MAGIC, 115)
#define IOCTL_DWU_BUF_LOCK		_IO(JZDWU_IOC_MAGIC, 116)
#define IOCTL_DWU_BUF_UNLOCK	_IO(JZDWU_IOC_MAGIC, 117)
#define IOCTL_DWU_BUF_FLUSH_CACHE	_IO(JZDWU_IOC_MAGIC, 118)

/* DWU_CTRL*/
#define DWU_AHB_EN          (1 << 1)
#define DWU_AXI_EN          (1 << 0)

/* DWU_PROTECT_START_ADDR */
#define PROTECT_START_ADDR   (0)

/* DWU_PROTECT_END_ADDR */
#define PROTECT_END_ADDR     (0)

/* DWU_BAD_ADDR */
#define BAD_ADDR       (0)

/* DWU_DEVICE0_CTRL */
#define DEVICE_NUM       (3)
#define WR_FLAG          (1 << 2)
#define RD_FLAG          (1 << 1)
#define DEVICE0_EN       (1 << 0)

/* DWU_DEVICE1_CTRL */
/*
#define DEVICE_NUM       (3)
#define WR_FLAG          (1 << 2)
#define RD_FLAG          (1 << 1)
*/
#define DEVICE1_EN       (1 << 0)

/* DWU_DEVICE2_CTRL */
/*
#define DEVICE_NUM       (3)
#define WR_FLAG          (1 << 2)
#define RD_FLAG          (1 << 1)
*/
#define DEVICE2_EN       (1 << 0)

/* DWU_DEVICE3_CTRL */
/*
#define DEVICE_NUM       (3)
#define WR_FLAG          (1 << 2)
#define RD_FLAG          (1 << 1)
*/
#define DEVICE3_EN       (1 << 0)

/* DWU_INT_CLR */
#define INT_CLR            (1 << 0)

/* DWU_ERR_ADDR */
#define ERROR_ADDR       (0)

/* DWU_ERR_INFO */
#define ILLEGAL_DEVICE_NUM      (1)
#define ILLEGAL_FLAG_WR         (1 << 0)
#define ILLEGAL_FLAG_RD         (0 << 0)


static inline unsigned int reg_read(struct jz_dwu *jzdwu, int offset)
{
	return readl(jzdwu->iomem + offset);
}

static inline void reg_write(struct jz_dwu *jzdwu, int offset, unsigned int val)
{
	writel(val, jzdwu->iomem + offset);
}


#define __start_dwu_axi()			    reg_bit_set(dwu, DWU_CTRL, DWU_AXI_EN)
#define __start_dwu_ahb()			    reg_bit_set(dwu, DWU_CTRL, DWU_AHB_EN)
#define __irq_clear_dwu()			reg_bit_set(dwu, DWU_INT_CLR, INT_CLR)
#define __stop_dwu_axi()			    reg_bit_clr(dwu, DWU_CTRL, DWU_AXI_EN)
#define __stop_dwu_ahb()			    reg_bit_clr(dwu, DWU_CTRL, DWU_AHB_EN)

#endif
