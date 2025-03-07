/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : dma_func.h
 * Authors    : jzhang@elim.ic.jz.com
 * Create Time: 2020-03-03:16:43:56
 * Description:
 *
 */

#ifndef __DMA_FUNC_H__
#define __DMA_FUNC_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

void w_d2o_orasetrun(int do_oralloc, int blk_num,
             uint8_t ** ddr_ptr, uint8_t ** or_ptr, uint32_t * size);

int if_d2rbo_oraset(int do_oralloc, uint32_t line_size,
            int blk_lnum, int blk0_lnum_more, int orbuf_lnum,
            int total_lnum, int ifp_num, int qifp_num,
            int *qorbuf_size_p,
            uint8_t **ddr_ptr, uint8_t **or_ptr, uint32_t **des_entry);

int of_o2d_oraset(int do_oralloc, uint32_t line_size, int blk_lnum,
          int buf_bnum, int total_lnum, int ofp_num,
          uint8_t *ddr_ptr, uint8_t **or_ptr, uint32_t **des_entry_p);

void aie_run_nndma_rdchn(int rd_chn, uint32_t ptr, int total_size);
void aie_run_nndma_wrchn(int wr_chn, uint32_t ptr, int total_size);
void aie_wait_nndma_rdchn(int rd_chn);
void aie_wait_nndma_wrchn(int wr_chn);
int check_rd_wr_chn_same(uint32_t ddr_vaddr_in, uint32_t ddr_vaddr_out, int block_size);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __DMA_FUNC_H__ */

