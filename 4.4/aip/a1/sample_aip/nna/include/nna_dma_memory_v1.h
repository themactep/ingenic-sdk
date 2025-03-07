/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : nna_dma_va_gen.h
 * Authors    : xqwu@aram.ic.jz.com
 * Create Time: 2019-05-29:12:13:22
 * Description:
 *
 */

#ifndef __NNA_DMA_MEMORY_H__
#define __NNA_DMA_MEMORY_H__

extern void *nndma_base_vaddr;
extern void *nnoram_base_vaddr;

#define NNA_DMA_IO_BASE          0x12500000
#define NNA_DMA_IO_SIZE          0x200000

#define NNA_DMA_RCFG_BASE_ADDR   0x008000
#define NNA_DMA_RCFG_CHN_ADDR(i) (NNA_DMA_RCFG_BASE_ADDR + 0x4 * (i))
#define NNA_DMA_WCFG_BASE_ADDR   0x008010
#define NNA_DMA_WCFG_CHN_ADDR(i) (NNA_DMA_WCFG_BASE_ADDR + 0x4 * (i))

#define NNA_DESRAM_STR_ADDR      0x000000
#define NNA_ORAM_STR_ADDR        0x120000 //(0x100000 + 0x20000) =>128KB L2CACHE

#ifdef USE_SIM
//UNCACHEABLE
#define NNA_ORAM_BASE_VA_ADDR    (nnoram_base_vaddr + NNA_ORAM_STR_ADDR  - 0x12600000 + 0x30000000)
#else
#define NNA_ORAM_BASE_VA_ADDR    (nndma_base_vaddr + NNA_ORAM_STR_ADDR)
#endif
#define NNA_DESRAM_BASE_VA_ADDR  (nndma_base_vaddr + NNA_DESRAM_STR_ADDR)
#define NNA_DMA_RCFG_VA_ADDR(i)  (nndma_base_vaddr + NNA_DMA_RCFG_CHN_ADDR(i))
#define NNA_DMA_WCFG_VA_ADDR(i)  (nndma_base_vaddr + NNA_DMA_WCFG_CHN_ADDR(i))

const static unsigned int RESERVED_DDR_SIZE = 262144;  //2048 * (28 + 4) * 2

extern unsigned int *dma_rcfg_vaddr[3];
extern unsigned int *dma_wcfg_vaddr[2];
extern unsigned int *desram_rvaddr;
extern unsigned int *desram_wvaddr;

//unsigned int nndma_va_gen(unsigned int dma_channel);
//void nndma_va_gen();
extern int nndma_memory_init(int max_ddr_size);
extern int nndma_memory_deinit();
//void *nndma_memory_vir_to_phy(void *vaddr);
void *nndma_memory_phy_to_vir(void *paddr);

void *nndma_oram_vir_to_phy(void *vaddr);
void *nndma_ddr_vir_to_phy(void *vaddr);
void *nndma_oram_phy_to_vir(void *paddr);
void *nndma_ddr_phy_to_vir(void *paddr);

#endif /* __NNA_DMA_MEMORY_H__ */

