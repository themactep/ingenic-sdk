#ifndef __SPI_SLV_H__
#define __SPI_SLV_H__

#include "asm/io.h"
#include <linux/io.h>
#include "linux/completion.h"
#include <linux/dmaengine.h>
#include <linux/miscdevice.h>
#include <asm-generic/ioctl.h>
#include <linux/device.h>

struct ssi_slv {
	struct miscdevice mdev;
	struct device	*dev;

	int irq;
	void __iomem *iomem;
	unsigned long phys;
	struct resource *res;
	struct resource *dma_res;
	struct clk *clk_gate;
	struct clk *clk;

	struct dma_chan *chan;
	// struct dma_chan *tx_chan;
	// struct dma_chan *rx_chan;
	struct dma_slave_config dma_config_tx;
	struct dma_async_tx_descriptor *desc_tx;
	struct dma_slave_config dma_config_rx;
	struct dma_async_tx_descriptor *desc_rx;
	struct completion dma_rx_done_comp;
	struct completion dma_tx_done_comp;

	struct completion rx_done_comp;
	struct completion tx_done_comp;

	const u8 *tx;
	u8 *rx;

	u8 *tx_buff;
	char *rx_buff;

	spinlock_t lock;
	u32 intr_cnt;
	u32 dma_type;
	// u32 dma_type_tx;
	// u32 dma_type_rx;

	u32 rx_count;
	u32 tx_count;

} ;

struct ssi_slv_param {
	u8 *tx;
	u8 *rx;

	u8 istx_dma;
	u8 isrx_dma;
	u32 tx_len;
	u32 rx_len;

	void *tx_vaddr;
	void *rx_vaddr;
	uint32_t tx_paddr;
	uint32_t rx_paddr;
};

#define SSISLV_IOC_MAGIC  'S'
#define IOCTL_SSISLV_READ	_IOWR(SSISLV_IOC_MAGIC,0,struct ssi_slv_param)
#define IOCTL_SSISLV_WRITE	_IOWR(SSISLV_IOC_MAGIC,1,struct ssi_slv_param)
#define IOCTL_SSISLV_DMA_READ	_IOWR(SSISLV_IOC_MAGIC,2,struct ssi_slv_param)
#define IOCTL_SSISLV_DMA_WRITE	_IOWR(SSISLV_IOC_MAGIC,3,struct ssi_slv_param)

/*SSI register*/
#define SLV_CTRLR	0x0	/* Control Register */
#define SLV_CTRL_RX	0x4	/* Receive Control Register */
#define SLV_CTRL_TX	0x8	/* Transmit Control Register */
#define	SLV_RXD		0xC /* Receive Data Register */
#define	SLV_TXD		0x10 /* Transmit Data Registe */
#define SLV_RXN		0X14
#define SLV_TXN		0X18
#define SLV_FLA		0X1C

/* Control Register  */
#define CTLR_CS_DIS		(1<<31) 	/* select ssi slave version 	0: new(defaut) 1: old */
#define CTLR_LOOP 		(1 << 30) 	/* select loop mode  		0: normal mode  1: loop mode*/
#define CTLR_CS_SW_EN 	(1 << 29)   /* Configures whether external cs signals are required for the controller
                                     * 1:  No external cs signal is required
                                     * 0:  An external cs signal is required    */
#define CTLR_RESET(n) 	((n) << 7) 	/* reset count time(0~63)  	times = n*1/plck */
#define CTLR_REQ		(1 << 6) 	/* enable/disable sslv interrupt */
#define CTLR_CKG_MASK 	(1 << 5) 	/* CLK mask bit */
#define CTLR_REQ_MASK	(1 << 4) 	/* interrupt mask bit */
#define CTLR_CPHA		(1 << 3) 	/* serial clock phase bit; 	0: Serial clock toggles in middles of first data bit
																*	1: Serial clock toggles at start of first data bit  */
#define CTLR_CPOL		(1 << 2) 	/*serial clock polarity bit;	0: Inactive state of serial clock is low
																*	1: Inactive state of serial clock is high	*/
#define CTLR_TX_EN		(1 << 1) 	/* enable/disable sslv transmit */
#define CTLR_RX_EN		(1 << 0) 	/* enable/disable sslv receive */

/* receive control register */
#define	RX_EMPTY_IRQ_MASK		(1<<31) 	/* receive FIFO empty interrupt mask */
#define RX_FULL_IRQ_MASK		(1<<30) 	/* receive FIFO full interrupt mask*/
#define RX_RDFLAG_IRQ_MASK		(1<<29) 	/* receive read flag interrupt mask */
#define RX_WDFLAG_IRQ_MASK		(1<<28) 	/* receive writel flag interrupt mask */
#define	RX_RDERR_IRQ_MASK		(1<<27) 	/* receive FIFO read data empty error interrupt mask */
#define RX_WDERR_IRQ_MASK		(1<<26) 	/* receive FIFO writel data full error interrupt mask */
#define RX_CHANNEL(n)			((n)<<24) 	/* receive channels		00:normal SPI	01:dual SPI   10:qual SPI  11:receive */
#define RX_ENDIAN(n)			((n)<<22) 	/* receive data endian */
#define RX_BITWIDTH(n)			((n)<<16) 	/* receive data bit width(4-32) */
#define RX_RDTHR(n)				((n)<<8) 	/* receive read threshold(1-128) */
#define RX_WRTHR(n)				((n)<<0) 	/* receive writel threshold(0-127) */

/* transmit control register */
#define	TX_EMPTY_IRQ_MASK		(1<<31) 	/* transmit FIFO empty interrupt mask */
#define TX_FULL_IRQ_MASK		(1<<30) 	/* transmit FIFO full interrupt mask*/
#define TX_RDFLAG_IRQ_MASK		(1<<29) 	/* transmit read flag interrupt mask */
#define TX_WDFLAG_IRQ_MASK		(1<<28) 	/* transmit writel flag interrupt mask */
#define	TX_RDERR_IRQ_MASK		(1<<27) 	/* transmit FIFO read data empty error interrupt mask */
#define TX_WDERR_IRQ_MASK		(1<<26) 	/* transmit FIFO writel data full error interrupt mask */
#define TX_CHANNEL(n)			((n)<<24) 	/* transmit channels		00:normal SPI	01:dual SPI   10:qual SPI  11:receive */
#define TX_ENDIAN(n)			((n)<<22) 	/* transmit data endian */
#define TX_BITWIDTH(n)			((n)<<16) 	/* transmit data bit width(4-32) */
#define TX_RDTHR(n)				((n)<<8) 	/* transmit read threshold(1-128) */
#define TX_WRTHR(n)				((n)<<0) 	/* transmit writel threshold(0-127) */


/* interrupt flag bits*/
#define TX_WDFLAG			(1<<23)
#define TX_RDEMPTY_ERR		(1<<22)
#define TX_WRFULL_ERR		(1<<21)
#define TX_EMPTY			(1<<20)
#define TX_FULL				(1<<19)
#define RX_RDFLAG			(1<<18)
#define RX_RDEMPTY_ERR		(1<<16)
#define RX_WRFULL_ERR		(1<<15)
#define RX_EMPTY			(1<<14)
#define RX_FULL				(1<<13)



/* Control Register 1 */
#define SLV_TRANSFER_MODE_TXRX 	(0x0)
#define SLV_TRANSFER_MODE_TX 	(0x1)
#define SLV_TRANSFER_MODE_RX 	(0x2)

#define SLV_POL_MODE_LOW 	(0x0)
#define SLV_POL_MODE_HIGH 	(0x1)

#define SLV_PHA_MODE_MID 	(0x0)
#define SLV_PHA_MODE_START 	(0x1)


/* Status Register */
#define	SR_BUSY		(1<<11) /* SSI_SLV is actively transferring data */
#define SR_TFE		(1<<10) /* Transmit FIFO is empty */
#define SR_RFF		(1<<9) /* Receive FIFO is full */
#define SR_RFE		(1<<8) /* Receive FIFO is empty */
#define	SR_END		(1<<7) /* SSI_SLV is finish transferring data */
#define SR_TFF		(1<<6) /* Transmit FIFO is full */
#define SR_TFHE		(1<<5) /* Transmit FIFO is less or equal to threshold */
#define SR_RFHF		(1<<4) /* Receive FIFO is more or equal to threshold */
#define SR_TUNDR	(1<<3) /* Transmit FIFO is underrun */
#define SR_TOVER	(1<<2) /* Transmit FIFO is overrun */
#define SR_RUNDR	(1<<1) /* Receive FIFO is underrun */
#define SR_ROVER	(1<<0) /* Receive FIFO is overrun */

/* These definitions of structs and macros are used in driver */
#define R_MODE			0x1
#define W_MODE			0x2
#define RW_MODE			(R_MODE | W_MODE)

#define R_DMA			0x4
#define W_DMA			0x8
#define RW_DMA			(R_DMA |W_DMA)

#define SPI_DMA_ACK		0x1

#define SPI_DMA_ERROR  		-3
#define SPI_CPU_ERROR		-4

#define SPI_COMPLETE		5

#define JZ_SSI_MAX_FIFO_ENTRIES 	128
#define JZ_SSI_DMA_BURST_LENGTH 	16

#define FIFO_W8			8
#define FIFO_W16		16
#define FIFO_W32		32

#define SPI_8BITS		1
#define SPI_16BITS		2
#define SPI_32BITS		4


/* tx rx threshold from 0x0 to 0xF */
#define SSI_TX_FIFO_THRESHOLD		0x1
#define SSI_RX_FIFO_THRESHOLD		0x1
#define SSI_SAFE_THRESHOLD		0x1

#define MAX_SSI_INTR		10000

#define MAX_SSICDR			63
#define MAX_CGV				255

#define SSI_DMA_FASTNESS_CHNL 	 0   // SSI controller [n] FASTNESS when probe();

#define JZ_NEW_CODE_TYPE

#define BUFFER_SIZE	PAGE_SIZE

#define JZ_SPI_LOOP_OPS 0
#define JZ_SPI_INT_OPS 1
#define JZ_SPI_CPU_OPS 2
#define JZ_SPI_DMA_OPS 3


void inline ss_write(struct ssi_slv *ingss, unsigned int offset, unsigned int value)
{
	writel(value, (ingss->iomem + offset));
}

unsigned int inline ss_read(struct ssi_slv *ingss, unsigned int offset)
{
	return readl(ingss->iomem + offset);
}

void inline set_control(struct ssi_slv *ingss, unsigned int cs_dis)
{
	unsigned int value = ss_read(ingss,SLV_CTRLR);

    /* 不依靠外部cs使能 */
    if(cs_dis)
    {
        value |= CTLR_CS_DIS | CTLR_CS_SW_EN;
        ss_write(ingss,SLV_CTRLR,value);
    }
    else
    {
        value &= ~(CTLR_CS_DIS | CTLR_CS_SW_EN);
        ss_write(ingss,SLV_CTRLR,value);
    }

	printk("SSI_SLV CTRLR0 : %x\n",ss_read(ingss,SLV_CTRLR));
}

/* the functions is about SSI_SLV control register */
void inline start_transmit(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value |= CTLR_TX_EN;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline start_receive(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value |= CTLR_RX_EN;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline finish_transmit(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value &= ~CTLR_TX_EN;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline finish_receive(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value &= ~CTLR_RX_EN;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline receive_count(struct ssi_slv *ingss,unsigned int cnt)
{
	ss_write(ingss, SLV_RXN, cnt);
}

void inline transmit_count(struct ssi_slv *ingss,unsigned int cnt)
{
	ss_write(ingss, SLV_TXN, cnt);
}
void inline enable_loop_test(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value |= CTLR_LOOP;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline disable_loop_test(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value &= ~CTLR_LOOP;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline ssi_slv_reset(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value |= CTLR_RESET(10);	//set reset time is 0x10*1/pclk ns
	ss_write(ingss, SLV_CTRLR, value);
}

void inline mask_irq_interrupts(struct ssi_slv *ingss, unsigned int mode)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	if(mode)
		value |= CTLR_REQ_MASK;
	else
		value &= ~CTLR_REQ_MASK;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline clear_irq_interrupt(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value &= ~CTLR_REQ;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline enable_irq_interrupts(struct ssi_slv *ingss)
{
	mask_irq_interrupts(ingss, 0);
}

void inline disable_irq_interrupts(struct ssi_slv *ingss)
{
	mask_irq_interrupts(ingss, 1);
	clear_irq_interrupt(ingss);
}

void inline mask_transmit_interrupts(struct ssi_slv *ingss, unsigned int mode)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_TX);
	value |= mode;
	ss_write(ingss, SLV_CTRL_TX, value);
}

void inline enable_transmit_interrupts(struct ssi_slv *ingss, unsigned int mode)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_TX);
	value &= ~mode;
	ss_write(ingss, SLV_CTRL_TX, value);
}

void inline mask_receive_interrupts(struct ssi_slv *ingss, unsigned int mode)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_RX);
	value |= mode;
	ss_write(ingss, SLV_CTRL_RX, value);
}

void inline enable_receive_interrupts(struct ssi_slv *ingss, unsigned int mode)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_RX);
	value &= ~mode;
	ss_write(ingss, SLV_CTRL_RX, value);
}

void inline set_pol_mode(struct ssi_slv *ingss, unsigned int mode)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value &= ~CTLR_CPOL;
	value |= mode<<2;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline set_pha_mode(struct ssi_slv *ingss, unsigned int mode)
{
	unsigned int value = ss_read(ingss, SLV_CTRLR);
	value &= ~CTLR_CPHA;
	value |= mode<<3;
	ss_write(ingss, SLV_CTRLR, value);
}

void inline set_rxSPImode(struct ssi_slv *ingss,u32 SPImode)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_RX);
	value &= ~(0x3<<24);
	value |= (SPImode<<24);
	ss_write(ingss, SLV_CTRL_RX, value);
}

void inline set_txSPImode(struct ssi_slv *ingss,u32 SPImode)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_TX);
	value &= ~(0x3<<24);
	value |= (SPImode<<24);
	ss_write(ingss, SLV_CTRL_TX, value);
}

void inline set_rx_msb(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_RX);
	value &= ~(0x3<<22);
	ss_write(ingss, SLV_CTRL_RX, value);
}

void inline set_tx_msb(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_TX);
	value &= ~(0x3<<22);
	ss_write(ingss, SLV_CTRL_TX, value);
}
void inline set_rx_lsb(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_RX);
	value &= ~(0x3<<22);
	value |= (0x3<<22);
	ss_write(ingss, SLV_CTRL_RX, value);
}

void inline set_tx_lsb(struct ssi_slv *ingss)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_TX);
	value &= ~(0x3<<22);
	value |= (0x3<<22);
	ss_write(ingss, SLV_CTRL_TX, value);
}

void inline set_frame_length(struct ssi_slv *ingss, unsigned int len)
{
	unsigned int value0 = ss_read(ingss, SLV_CTRL_RX);
	unsigned int value1 = ss_read(ingss, SLV_CTRL_TX);
	len = len < 4 ? 4 : len;
	value0 &= ~RX_BITWIDTH(0x3f);
	value0 |= RX_BITWIDTH(len);
	ss_write(ingss, SLV_CTRL_RX, value0);
	value1 &= ~TX_BITWIDTH(0x3f);
	value1 |= TX_BITWIDTH(len);
	ss_write(ingss, SLV_CTRL_TX, value1);
}

/* transmit write threshold 1-128 */
void inline set_txWrthr(struct ssi_slv *ingss, int len)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_TX);
	len = (len >= 1) ? len : 1;
	value &= ~TX_WRTHR(0x7f);
	value |= TX_WRTHR(len);
	ss_write(ingss, SLV_CTRL_TX, value);
}
/* receive read threshold 0-127 */
void inline set_rxRdthr(struct ssi_slv *ingss, int len)
{
	unsigned int value = ss_read(ingss, SLV_CTRL_RX);
	len = len >= 1 ? len : 1;
	value &= ~RX_RDTHR(0x7f);
	value |= RX_RDTHR(len);
	ss_write(ingss, SLV_CTRL_RX, value);
}

u32 inline read_rxfifo_entries(struct ssi_slv *ingss)
{
	u32 cnt = ss_read(ingss, SLV_FLA);
	cnt = (cnt >> 8) & 0xff;
	return cnt;
}

u32 inline read_txfifo_entries(struct ssi_slv *ingss)
{
	u32 cnt = ss_read(ingss, SLV_FLA);
	cnt = (cnt >> 16) & 0xff;
	return cnt;
}
u32 inline rxfifo_empty (struct ssi_slv *ingss)
{
	if((ss_read(ingss,SLV_CTRLR)>>14) & 0x01)
		return 1;
	else
		return 0;
}


static inline void transmit_data(struct ssi_slv *ingss, u32 value)
{
	ss_write(ingss, SLV_TXD, value);
}

#endif /*__SPI_SLV_H__*/
