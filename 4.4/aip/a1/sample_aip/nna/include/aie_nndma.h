/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : aie_nndma.h
 * Authors    : yzhai@aram.ic.jz.com
 * Create Time: 2019-11-14:15:39:28
 * Description:
 *
 */

#ifndef __AIE_NNDMA_H__
#define __AIE_NNDMA_H__

#include "stdint.h"
#include "aie_mmap.h"
#include "mxu3.h"

/**
 * // according to nndma des limited, the max nna dma transfer size is 1MByte. So max oram size is 1MByte. except normally l2cache 128K, real normally oram size is 896KByte
 * typedef union nndma_des {
 *     unsigned long long int des64;        // the nndma des width is 64bit
 *     struct {
 *         unsigned int link_flag:1;        // [0 : 0], = 0 : link end, = 1: link to next
 *         unsigned int data_lenght_h:5;    // [5 : 1], means data_lenght[16:12]
 *         unsigned int ddr_addr:26;        // [31: 6], aliged to 64Byte, ddr_addr[25:0]=real_ddr_addr>>6; as source to rd channel, as dest to wr channel
 *         unsigned int oram_addr:20;       // [51:32], aliged to 64Byte, oram_addr[19:0]=(real_oram_addr-oram_phyoffset)>>6; as dest to rd channel,  as source to wr channel
 *                                                      the max_real_oram_size=2^20=1MByte, here, oram_addr field means as oram offset
 *         unsigned int data_lenght_l:12;   // [63:52], means data_lenght[11: 0], aliged to 64Bype, ddr_lenght[16:0]=(real_ddr_lenght>>6)-1; for each dma transfer,
 *                                                      the max_real_ddr_lenght=8MByte=2^17*64Byte
 *     } desv;
 * } nndma_des_t;
 */

#define NNDMA_WAIT_IF i_rdhwr(8+0)
#define NNDMA_WAIT_W  i_rdhwr(8+1)
#define NNDMA_WAIT_OF i_rdhwr(11+0)
#define NNBSCALER_WAIT

inline uint32_t __aie_get_desram_IF_ping() {
    return __aie_get_nndma_desram_vbase();
}
inline uint32_t __aie_get_desram_IF_pong() {
    return __aie_get_nndma_desram_vbase() + 2*1024;
}
inline uint32_t __aie_get_desram_W_ping() {
    return __aie_get_nndma_desram_vbase() + 4*1024;
}
inline uint32_t __aie_get_desram_W_pong() {
    return __aie_get_nndma_desram_vbase() + 6*1024;
}
inline uint32_t __aie_get_desram_OF_ping() {
    return __aie_get_nndma_desram_vbase() + 8*1024;
}
inline uint32_t __aie_get_desram_OF_pong() {
    return __aie_get_nndma_desram_vbase() + 10*1024;
}
inline uint32_t __aie_get_desram_ptr(uint32_t desram) {
    return (desram - __aie_get_nndma_desram_vbase())>>3;
}

inline void __aie_push_nndma_big_node(uint32_t* desram, uint32_t oram_addr, uint32_t ddr_addr, uint32_t size) {
    uint32_t **desram_p = (uint32_t **)desram;
    uint32_t size_64m1 = (size>>6)-1;
    *(*desram_p)++ = ddr_addr | (size_64m1>>11) & ~0x1;
    *(*desram_p)++ = (size_64m1<<20) | (oram_addr>>6);
}

inline void __aie_push_nndma_node(uint32_t* desram, uint32_t oram_addr, uint32_t ddr_addr, uint32_t size) {
    uint32_t **desram_p = (uint32_t **)desram;
    uint32_t size_64m1 = (size>>6)-1;
    *(*desram_p)++ = ddr_addr | 1;
    *(*desram_p)++ = (size_64m1<<20) | (oram_addr>>6);
}

inline void __aie_push_nndma_node_maylast(int last, uint32_t* desram, uint32_t oram_addr, uint32_t ddr_addr, uint32_t size) {
    uint32_t **desram_p = (uint32_t **)desram;
    uint32_t size_64m1 = (size>>6)-1;
    *(*desram_p)++ = ddr_addr | (last?0:1);
    *(*desram_p)++ = (size_64m1<<20) | (oram_addr>>6);
    if (last && (__aie_get_desram_cache_attr() & NNA_UNCACHED_ACCELERATED)) {
        fast_iob();
    }
}

inline void __aie_push_nndma_bignode_maylast(int last, uint32_t* desram, uint32_t oram_addr, uint32_t ddr_addr, uint32_t size) {
    uint32_t **desram_p = (uint32_t **)desram;
    uint32_t size_64m1 = (size>>6)-1;
    *(*desram_p)++ = ddr_addr | ((size_64m1>>11) & 0x3E) | (last?0:1);
    *(*desram_p)++ = (size_64m1<<20) | (oram_addr>>6);
    if (last && (__aie_get_desram_cache_attr() & NNA_UNCACHED_ACCELERATED)) {
        fast_iob();
    }
}

inline void __aie_set_nndma_unlink(uint32_t* desram) {
    uint32_t **desram_p = (uint32_t **)desram;
    *((*desram_p)-2) &= ~0x1;
    if (__aie_get_desram_cache_attr() & NNA_UNCACHED_ACCELERATED) {
        fast_iob();
    }
}

inline void __aie_sync_nndma_status() {
    volatile uint32_t data = 0;
    uint32_t nndma_io_vbase = __aie_get_nndma_io_vbase();
    data = *(volatile uint32_t *)(nndma_io_vbase);
}
inline void __aie_run_nndma_IF(uint32_t ptr) {
    volatile uint32_t data = 0;
    uint32_t nndma_io_vbase = __aie_get_nndma_io_vbase();
    *(volatile uint32_t *)(nndma_io_vbase+0x0) = (0x1<<31 | ptr);
    data = *(volatile uint32_t *)(nndma_io_vbase+0x0);
}
inline void __aie_run_nndma_W(uint32_t ptr) {
    volatile uint32_t data = 0;
    uint32_t nndma_io_vbase = __aie_get_nndma_io_vbase();
    *(volatile uint32_t *)(nndma_io_vbase+0x4) = (0x1<<31 | ptr);
    data = *(volatile uint32_t *)(nndma_io_vbase+0x4);
}
inline void __aie_run_nndma_OF(uint32_t ptr) {
    volatile uint32_t data = 0;
    uint32_t nndma_io_vbase = __aie_get_nndma_io_vbase();
    *(volatile uint32_t *)(nndma_io_vbase+0x10) = (0x1<<31 | ptr);
    data = *(volatile uint32_t *)(nndma_io_vbase+0x10);
}

#endif /* __AIE_NNDMA_H__ */

