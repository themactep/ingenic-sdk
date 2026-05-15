#ifndef __PDMAC__H__
#define __PDMAC__H__

#define PDMA_P		0xb3420000
#define DMA_DSAn		0x00
#define DMA_DTAn		0x04
#define DMA_DTCn		0x08
#define DMA_DRTn		0x0C
#define DMA_DCSn		0x10
#define DMA_DCMn		0x14
#define DMA_DDAn		0x18
#define DMA_DSDn		0x1C
#define DMA_DMACn		0x1000
#define DMA_READBIT(reg, bit, width)		(((reg)>>(bit))&(~((~0)<<(width))))
//#define u32 unsigned int

#define UART0_BASE		0x10030000
#define UART1_BASE		0x10031000
#define UART2_BASE		0x10032000
#define UART3_BASE		0x10033000

#define RBR_THR_DLLR 0x00
#define DLHR_IER     0x04
#define IIR_FCR      0x08
#define LCR          0x0c
#define MCR          0x10
#define LSR          0x14
#define SPR          0x1c
#define ISR          0x20
#define UMR          0x24
#define UART_FCR_UUE    (1 << 4)    /* 0: disable UART */
#define SIRCR_RSIRE (1 << 1) /* 0: RX is in UART mode 1: IrDA mode */
#define SIRCR_TSIRE (1 << 0) /* 0: TX is in UART mode 1: IrDA mode */

#define DMADSA(chn,pdmacreg_v)		(*(volatile u32*)((pdmacreg_v)+(DMA_DSAn)+(chn)*0x20))
#define DMADTA(chn,pdmacreg_v)		(*(volatile u32*)((pdmacreg_v)+(DMA_DTAn)+(chn)*0x20))
#define DMADTC(chn,pdmacreg_v)		(*(volatile u32*)((pdmacreg_v)+(DMA_DTCn)+(chn)*0x20))
#define DMADRT(chn,pdmacreg_v)		(*(volatile u32*)((pdmacreg_v)+(DMA_DRTn)+(chn)*0x20))
#define DMADCS(chn,pdmacreg_v)		(*(volatile u32*)((pdmacreg_v)+(DMA_DCSn)+(chn)*0x20))
#define DMADCM(chn,pdmacreg_v)		(*(volatile u32*)((pdmacreg_v)+(DMA_DCMn)+(chn)*0x20))
#define DMADDA(chn,pdmacreg_v)		(*(volatile u32*)((pdmacreg_v)+(DMA_DDAn)+(chn)*0x20))
#define DMADSD(chn,pdmacreg_v)		(*(volatile u32*)((pdmacreg_v)+(DMA_DSDn)+(chn)*0x20))
#define DMADMAC(pdmacreg_v)			(*(volatile u32*)((pdmacreg_v)+(DMA_DMACn)))

/*
 * Define macros for UART_FCR
 * UART FIFO Control Register
 */
#define UART_FCR_FE	(1 << 0)	/* 0: non-FIFO mode  1: FIFO mode */
#define UART_FCR_RFLS	(1 << 1)	/* write 1 to flush receive FIFO */
#define UART_FCR_TFLS	(1 << 2)	/* write 1 to flush transmit FIFO */
#define UART_FCR_DMS	(1 << 3)	/* 0: disable DMA mode */
#define UART_FCR_UUE	(1 << 4)	/* 0: disable UART */
#define UART_FCR_RTRG	(3 << 6)	/* Receive FIFO Data Trigger */
#define UART_FCR_RTRG_1	(0 << 6)
#define UART_FCR_RTRG_4	(1 << 6)
#define UART_FCR_RTRG_8	(2 << 6)
#define UART_FCR_RTRG_15	(3 << 6)

#define UART_LCR_WLEN_8 (3 << 0)
#define UART_LCR_STOP   (1 << 2)
#define UART_LCR_STOP_1 (0 << 2)

#define UART_LCR_DLAB   (1 << 7)

#define UART_LSR_TDRQ   (1 << 5)
#define UART_LSR_TEMT   (1 << 6)
#endif
