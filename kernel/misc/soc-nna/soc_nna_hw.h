#ifndef __SOC_NNA_HW_H__
#define __SOC_NNA_HW_H__

#define SOC_NNA_DMA_IOBASE          0x12502000
#define SOC_NNA_DMA_IOSIZE          0x1000

#define SOC_NNA_DMA_DESRAM_ADDR     0x1250f000
#define SOC_NNA_DMA_DESRAM_SIZE     0x4000          //16*1024

#define NNA_DMA_RCFG            0x0
#define NNA_DMA_WCFG            0x4
#define NNA_DMA_RCNT            0x8
#define NNA_DMA_WCNT            0xc

/* NNA_DMA_RCFG */
#define RCFG_START              0
#define RCFG_DES_ADDR           1
#define RCFG_DES_ADDR_MASK      (0x7ff << (RCFG_DES_ADDR))

/* NNA_DMA_WCFG */
#define WCFG_START              0
#define WCFG_DES_ADDR           1
#define WCFG_DES_ADDR_MASK      (0x7ff << (WCFG_DES_ADDR))

#define DES_CFG_FLAG            50ULL
#define DES_CFG_FLAG_MASK       (0x3ULL << (DES_CFG_FLAG))
#define DES_CFG_CNT             0ULL
#define DES_CFG_LINK            1ULL
#define DES_CFG_END             2ULL

#define DES_TOTAL_BYTES         0ULL
#define DES_TOTAL_BYTES_MASK    (0xfffffULL << (DES_TOTAL_BYTES))
#define DES_DATA_LEN            40ULL
#define DES_DATA_LEN_MASK       (0x3ffULL << (DES_DATA_LEN))
#define DES_ORAM_ADDR           26ULL
#define DES_ORAM_ADDR_MASK      (0x3fffULL << (DES_ORAM_ADDR))
#define DES_DDR_ADDR            0ULL
#define DES_DDR_ADDR_MASK       (0x3ffffffULL << (DES_DDR_ADDR))

#endif
