/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : nna_dma_driver_v2.h
 * Authors    : xqwu@aram.ic.jz.com
 * Create Time: 2019-07-08:15:19:01
 * Description:
 *
 */
#include "mxu3.h"
#include "nna_dma_memory_v1.h"

#ifndef __NNA_DMA_DRIVER_V2_H__
#define __NNA_DMA_DRIVER_V2_H__

//WAIT DMA complete. For RD: n=0,1,2. For WR: n=0,1
#define NNA_WAIT_DMA_RD(n) i_rdhwr(8+n)
#define NNA_WAIT_DMA_WR(n) i_rdhwr(8+3+n)

//#define NNDMA_DRIVER_DEBUG

const static unsigned int ORAM_MAX_ADDR = 0x126fffff; //0x12600000 + 0x100000 - 1

typedef struct {
    unsigned int d_va_st_addr ;
    unsigned int o_va_st_addr ;
    unsigned int o_va_mlc_addr;
    unsigned int o_mlc_bytes  ;
    unsigned int data_bytes   ;
    unsigned int des_link     ;
} nndma_cmd_buf;

typedef struct {
    uint32_t des_data[2048*2];
    uint32_t des_num[2048*2];
    uint32_t total_bytes[2048*2];
    uint32_t chain_st_idx[2048*2];
    uint32_t chain_num;
} des_info;

typedef struct {
    uint32_t rcmd_st_idx;
    uint32_t wcmd_st_idx;
    uint32_t dma_cmd_num;
    uint32_t finish;
} des_gen_result;

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

static inline void nndma_rdch_start(int chnId, uint32_t desram_ptr) {
    *(volatile unsigned int *)(dma_rcfg_vaddr[chnId]) = (1<<31) | desram_ptr;
}

static inline void nndma_wrch_start(int chnId, uint32_t desram_ptr) {
    *(volatile unsigned int *)(dma_wcfg_vaddr[chnId]) = (1<<31) | desram_ptr;
}

static inline void nndma_ch_check() {
    volatile uint32_t data = 0;
    data = *(volatile unsigned int *)(dma_wcfg_vaddr[0]);
}

//driver
int nna_dma_driver(uint32_t cmd_info[4], nndma_cmd_buf * cmd_buf, uint32_t * dma_cmd_buf, des_gen_result * des_gen_result);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __NNA_DMA_DRIVER_V2_H__ */

