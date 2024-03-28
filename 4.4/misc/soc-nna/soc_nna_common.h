#ifndef __SOC_NNA_COMMON_H__
#define __SOC_NNA_COMMON_H__

#include <linux/platform_device.h>

#include "soc_nna.h"
#include "soc_nna_hw.h"

extern struct platform_device soc_nna_device;
#define SOC_NNA_MAX_DES_CHN_CNT     2048        //16384 / 8 = 2048
#define SOC_NNA_ADDR_ALIGN_BIT      6LL

typedef struct nna_dma_des_info {
    unsigned long long int      des_data[SOC_NNA_MAX_DES_CHN_CNT];
    unsigned int                des_num[SOC_NNA_MAX_DES_CHN_CNT];
    unsigned int                total_bytes[SOC_NNA_MAX_DES_CHN_CNT];
    unsigned int                chain_st_idx[SOC_NNA_MAX_DES_CHN_CNT];
    unsigned int                chain_num;
} nna_dma_des_info_t;

#define soc_nna_readl(pnna, offset)           __raw_readl((pnna)->iomem + (offset))
#define soc_nna_writel(pnna, offset, value)   __raw_writel((value), (pnna)->iomem + (offset))

#endif //__SOC_NNA_COMMON_H__
