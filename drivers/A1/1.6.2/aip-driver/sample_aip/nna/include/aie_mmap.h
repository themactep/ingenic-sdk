/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : aie_mmap.h
 * Authors    : yzhai@aram.ic.jz.com
 * Create Time: 2019-11-14:14:29:05
 * Description:
 *
 */

#ifndef __AIE_MMAP_H__
#define __AIE_MMAP_H__

#include "stdint.h"
#include "ddr_mem.h"
#include "oram_mem.h"
//#include "aie_nndbg.h"
#ifdef __cplusplus
extern "C" {
#endif

#define NNDMA_DESRAM_BASE  0x12500000
#define NNDMA_DESRAM_SIZE  16*1024
#define NNDMA_IO_BASE      0x12508000
#define NNDMA_IO_SIZE      0x20
#define NNDBG_IO_BASE      0x13F00000
#define NNDBG_IO_SIZE      0xFF
#define BSCALER_IO_BASE    0x13090000
#define BSCALER_IO_SIZE    0x1000
#define L2C_SIZE           128*1024
#define ORAM_BASE          (0x12600000+L2C_SIZE)
#define ORAM_SIZE          (1024*1024-L2C_SIZE)
#define FASTIO_SIZE        0x1000

/*
 * cached attr is a bit ored unsigned int value,
 * mainly to mark __nndma_desram_cache_attr, __oram_vbase_cache_attr, __ddr_vbase_cache_attr;
 */
typedef enum nna_cache_attr {
    NNA_UNCACHED                = 0x1,    /* 1 << 0, uncached, mainly to io memory, optionally to desram, oram, ddr */
    NNA_UNCACHED_ACCELERATED    = 0x2,    /* 1 << 1, cached accelerated, optionally to desram oram, ddr */
    NNA_CACHED                  = 0x4,    /* 1 << 2, cached, optionally to ddr memory */
} nna_cache_attr_t;

enum nna_dma_data_direction {
    NNA_DMA_BIDIRECTIONAL   = 0,        /* write back invalide */
    NNA_DMA_TO_DEVICE       = 1,        /* write back invalide */
    NNA_DMA_FROM_DEVICE     = 2,        /* Invalidate */
    NNA_DMA_NONE            = 3,
};

extern void *__nndma_io_vbase;
extern void *__nndma_desram_vbase;
extern void *__nndbg_io_vbase;
extern void *__bscaler_io_vbase;
extern void *__oram_vbase;
extern void *__ddr_vbase;
extern void *__ddr_pbase;
extern void *__nndma_fastio_vbase;
extern unsigned int __nndma_desram_cache_attr;
extern unsigned int __oram_vbase_cache_attr;
extern unsigned int __ddr_vbase_cache_attr;

/**
 * Used to flush uncached accelerated instruction buffer's all instruction to be executed
 */
#define __sync()                     \
    __asm__ __volatile__(            \
        ".set    push\n\t"           \
        ".set    noreorder\n\t"      \
        ".set    mips2\n\t"          \
        "sync\n\t"                   \
        ".set    pop"                \
        : /* no output */            \
        : /* no input */             \
        : "memory")

/**
 * Used to confirm all data to be really written to their own space, such as desram, oram, ddr and so on
 * after their corresponding instruction be executed.
 */
#define __fast_iob()                                       \
    __asm__ __volatile__(                                  \
        ".set    push\n\t"                                 \
        ".set    noreorder\n\t"                            \
        "lw    $0,%0\n\t"                                  \
        "nop\n\t"                                          \
        ".set    pop"                                      \
        : /* no output */                                  \
        : "m" (*(unsigned int *)__nndma_fastio_vbase)      \
        : "memory")

#define fast_iob()                  \
    do {                            \
        __sync();                   \
        __fast_iob();               \
    } while (0)

/**
 * __aie_mmap should be called in main thread where use multi-thread process evironment.
 */
int __aie_mmap(int ddr_mem_size, int b_use_rmem, nna_cache_attr_t desram_cache_attr, nna_cache_attr_t oram_cache_attr, nna_cache_attr_t ddr_cache_attr);
int __aie_flushcache(void *ddr_mem_vaddr, int ddr_mem_size);
int __aie_flushcache_dir(void *ddr_mem_vaddr, int ddr_mem_size, enum nna_dma_data_direction dir);
int __aie_munmap();

inline __attribute__((always_inline))
uint32_t __aie_get_ddr_paddr(uint32_t ddr_vaddr) {
    return (uint32_t)__ddr_pbase + (ddr_vaddr - (uint32_t)__ddr_vbase);
}
inline __attribute__((always_inline))
uint32_t __aie_get_oram_paddr(uint32_t oram_vaddr) {
    return (uint32_t)ORAM_BASE + (oram_vaddr - (uint32_t)__oram_vbase);
}
inline __attribute__((always_inline))
uint32_t __aie_get_oram_offset(uint32_t oram_vaddr) {
    return (uint32_t)L2C_SIZE + (oram_vaddr - (uint32_t)__oram_vbase);
}
inline __attribute__((always_inline))
uint32_t __aie_get_oram_vbase() {
    return (uint32_t)__oram_vbase;
}
inline __attribute__((always_inline))
uint32_t __aie_get_nndma_io_vbase() {
    return (uint32_t)__nndma_io_vbase;
}
inline __attribute__((always_inline))
uint32_t __aie_get_nndma_desram_vbase() {
    return (uint32_t)__nndma_desram_vbase;
}
inline __attribute__((always_inline))
uint32_t __aie_get_nndbg_io_vbase() {
    return (uint32_t)__nndbg_io_vbase;
}
inline __attribute__((always_inline))
uint32_t __aie_get_bscaler_io_vbase() {
    return (uint32_t)__bscaler_io_vbase;
}
inline __attribute__((always_inline))
uint32_t __aie_get_desram_cache_attr() {
    return (uint32_t)__nndma_desram_cache_attr;
}
inline __attribute__((always_inline))
uint32_t __aie_get_oram_cache_attr() {
    return (uint32_t)__oram_vbase_cache_attr;
}
inline __attribute__((always_inline))
uint32_t __aie_get_ddr_cache_attr() {
    return (uint32_t)__ddr_vbase_cache_attr;
}

#ifdef __cplusplus
}
#endif
#endif /* __AIE_MMAP_H__ */

