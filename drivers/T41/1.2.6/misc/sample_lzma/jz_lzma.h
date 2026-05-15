#ifndef __JZ_LZMA_H__
#define __JZ_LZMA_H__

//#define DEBUG_LZMA
#define JZ_LZMA0_START_ADDRESS 0x132c0000
#define JZ_LZMA1_START_ADDRESS 0x132e0000

#define LZMA_CTRL               0*4
#define LZMA_BS_BASE            1*4
#define LZMA_BS_SIZE            2*4
#define LZMA_DST_BASE           3*4
#define LZMA_TIMEOUT            4*4
#define LZMA_FINAL_SIZE         5*4
#define LZMA_VERSION            6*4

#ifdef DEBUG_LZMA
#define LZMA_FSM_DBG            7*4
#define LZMA_RADDR_CNT_DBG      8*4
#define LZMA_RADDR_CFG_NUM      9*4
#define LZMA_RADDR0_DBG         10*4
#define LZMA_RADDR1_DBG         11*4
#define LZMA_RDATA_CNT_DBG      12*4
#define LZMA_RDATA_CFG_NUM      13*4
#define LZMA_RDATAL0_DBG        14*4
#define LZMA_RDATAH0_DBG        15*4
#define LZMA_RDATAL1_DBG        16*4
#define LZMA_RDATAH1_DBG        17*4
#define LZMA_HDEC_DA_CNT        18*4
#define LZMA_HDEC_DA_CFG_NUM    19*4
#define LZMA_HDEC_DA            20*4
#define LZMA_FSM_DA_CNT         21*4
#define LZMA_FSM_DA_CFG_NUM     22*4
#define LZMA_FSM_DA             23*4
#define LZMA_DIST_DA_CNT        24*4
#define LZMA_DIST_DA_CFG_NUM    25*4
#define LZMA_DIST0_DA           26*4
#define LZMA_DIST1_DA           27*4
#define LZMA_LEN_DA_CNT         28*4
#define LZMA_LEN_DA_CFG_NUM     29*4
#define LZMA_LEN_DA             30*4
#define LZMA_LIT_DA_CNT         31*4
#define LZMA_LIT_DA_CFG_NUM     32*4
#define LZMA_LIT_DA             33*4
#define LZMA_CRC_RDATA          34*4
#define LZMA_CRC_HDEC_DATA      35*4
#define LZMA_CRC_FSM_DATA       36*4
#define LZMA_CRC_DIST_DATA      37*4
#define LZMA_CRC_LEN_DATA       38*4
#define LZMA_CRC_LIT_DATA       39*4
#define LZMA_MBC_CNT_DBG        40*4
#define LZMA_MBC_DA_CFG_NUM     41*4
#define LZMA_MBC_DAL0_DBG       42*4
#define LZMA_MBC_DAH0_DBG       43*4
#define LZMA_MBC_DAL1_DBG       44*4
#define LZMA_MBC_DAH1_DBG       45*4
#define LZMA_SBC_CNT_DBG        46*4
#define LZMA_SBC_DA_CFG_NUM     47*4
#define LZMA_SBC_RP0_DBG        48*4
#define LZMA_SBC_RP1_DBG        49*4
#define LZMA_SBC_WP0_DBG        50*4
#define LZMA_SBC_WP1_DBG        51*4
#define LZMA_SBC_RDMA0_IDX_DBG  52*4
#define LZMA_SBC_RDMA1_IDX_DBG  53*4
#define LZMA_BS_DA_CNT          54*4
#define LZMA_BS_DA_CFG_NUM      55*4
#define LZMA_BS_DA0             56*4
#define LZMA_BS_DA1             57*4
#endif

#ifdef DEBUG_LZMA
#define JZ_LZMA_LENGTH_ADDRESS 232
#else
#define JZ_LZMA_LENGTH_ADDRESS 28
#endif

#define JZLZMA_IOC_MAGIC  'L'
#define IOCTL_LZMA0_START				_IOW(JZLZMA_IOC_MAGIC, 110, struct lzma_conf*)
#define IOCTL_LZMA1_START		        _IOW(JZLZMA_IOC_MAGIC, 111, struct lzma_conf*)
#define IOCTL_LZMAA_START               _IOW(JZLZMA_IOC_MAGIC, 112, struct lzma_conf*)

struct lzma_conf {
	char src0_name[64];
	char dst0_name[64];
	char src1_name[64];
	char dst1_name[64];
    unsigned int src0_size;
    unsigned int dst0_size;
    unsigned int src1_size;
    unsigned int dst1_size;
};

struct jz_lzma {

	int irq;
	char name[16];

	struct clk *clk_ispa;
	struct clk *clk_lzma;
	void __iomem *iomem0;
	void __iomem *iomem1;
	struct device *dev;
	struct resource *res0;
	struct resource *res1;
	struct miscdevice misc_dev;

	struct mutex mutex;
	struct completion done_lzma;
};

static inline unsigned int lzma_reg_read(struct jz_lzma *jzlzma, int chn, int offset)
{
    unsigned int value = 0;
    if(0 == chn) {
	    value = readl(jzlzma->iomem0 + offset);
    } else if(1 == chn) {
	    value = readl(jzlzma->iomem1 + offset);
    }
    return value;
}

static inline void lzma_reg_write(struct jz_lzma *jzlzma, int chn, int offset, unsigned int val)
{
    if(0 == chn) {
	    writel(val, jzlzma->iomem0 + offset);
    } else if(1 == chn) {
	    writel(val, jzlzma->iomem1 + offset);
    }
}

#endif
