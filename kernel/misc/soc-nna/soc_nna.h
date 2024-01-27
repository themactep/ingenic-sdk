#ifndef __SOC_NNA_H__
#define __SOC_NNA_H__

#include <stdbool.h>

#define SOC_NNA_DEVICE_NAME         "/dev/soc-nna"
#define SOC_NNA_MAX_CHN_CNT         2048    //16*1024/8=2048

/* oram information */
#define NNA_ORAM_BASE_ADDR      0x12600000
#define NNA_ORAM_BASE_SIZE      0xe0000         //(1024-128)*1024

/* IOCTL MACRO DEFINE PLACE */
#define SOC_NNA_MAGIC               'c'
#define IOCTL_SOC_NNA_MALLOC        _IOWR(SOC_NNA_MAGIC, 0, int)
#define IOCTL_SOC_NNA_FREE          _IOWR(SOC_NNA_MAGIC, 1, int)
#define IOCTL_SOC_NNA_FLUSHCACHE    _IOWR(SOC_NNA_MAGIC, 2, int)
#define IOCTL_SOC_NNA_SETUP_DES     _IOWR(SOC_NNA_MAGIC, 3, int)
#define IOCTL_SOC_NNA_RDCH_START    _IOWR(SOC_NNA_MAGIC, 4, int)
#define IOCTL_SOC_NNA_WRCH_START    _IOWR(SOC_NNA_MAGIC, 5, int)
#define IOCTL_SOC_NNA_VERSION    	_IOWR(SOC_NNA_MAGIC, 6, int)

/*
 * dir value defined in  enum dma_data_direction in linux/dma-direction.h
 * 0:DMA_BIDIRECTIONAL, 1:DMA_TO_DEVICE, 2:DMA_FROM_DEVICE
 */
struct flush_cache_info {
	unsigned int	addr;
	unsigned int	len;
	unsigned int	dir;
};

struct soc_nna_buf {
    void        *vaddr;
    void        *paddr;
    int         size;
};

typedef struct nna_dma_cmd {
    unsigned int    d_va_st_addr;
    unsigned int    o_va_st_addr;
    unsigned int    o_va_mlc_addr;
    unsigned int    o_mlc_bytes;
    unsigned int    data_bytes;
    unsigned int    des_link;
} nna_dma_cmd_t;

typedef struct des_gen_result {
    unsigned int    rcmd_st_idx;
    unsigned int    wcmd_st_idx;
    unsigned int    dma_chn_num;
    unsigned int    finish;
} des_gen_result_t;

typedef struct nna_dma_cmd_set {
    unsigned int        rd_cmd_cnt;
    unsigned int        rd_cmd_st_idx;
    unsigned int        wr_cmd_cnt;
    unsigned int        wr_cmd_st_idx;
    nna_dma_cmd_t       *d_va_cmd;
    unsigned int        *d_va_chn;
    des_gen_result_t    des_rslt;
} nna_dma_cmd_set_t;

#endif
