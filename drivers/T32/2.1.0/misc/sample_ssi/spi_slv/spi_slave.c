#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "asm-generic/errno-base.h"
#include "asm/page.h"
#include "linux/completion.h"
#include "linux/fs.h"
#include "linux/gfp.h"
#include "linux/slab.h"
#include "spi_slave.h"

#ifdef CONFIG_KERNEL_3_10
#include <soc/base.h>
#include "mach/jzdma.h"
#include "soc/irq.h"
#endif

// #define FPGA_TEST
#define isSLVrxLen 32
#define isSLVtxLen 4 //128
#define SSI_DEGUG
// #define LOOP
// #define SSISLV_QUAD
// #define SSISLV_DMA
#define SSISLV_TXTHR 4
#ifdef SSI_DEGUG
#define print_dbg(format, arg...) \
	printk(format, ##arg)
#else
#define print_dbg(format, arg...)
#endif

struct ssi_slv *ingss = NULL;

static void ssi_slv_dma(struct ssi_slv *ingss);
static void dma_rx_callback(void *data)
{
	//printk("%s\n", __func__);
	struct completion *dma_rx = data;
	complete(dma_rx);
}

static void dma_tx_callback(void *data)
{
	//printk("%s\n", __func__);
	struct completion *dma_tx = data;
	complete(dma_tx);
}

void dump_spi_slaver_regs(struct ssi_slv *ingss)
{
	printk("******************** dump slaver regs *********************\n");
	printk("SLV_CTRLR =         0x%08x\n", ss_read(ingss, SLV_CTRLR));
	printk("SLV_CTRL_RX =       0x%08x\n", ss_read(ingss, SLV_CTRL_RX));
	printk("SLV_CTRL_TX =       0x%08x\n", ss_read(ingss, SLV_CTRL_TX));
	printk("SLV_RXN =           0x%08x\n", ss_read(ingss, SLV_RXN));
	printk("SLV_TXN =           0x%08x\n", ss_read(ingss, SLV_TXN));
	printk("SLV_FLA =           0x%08x\n", ss_read(ingss, SLV_FLA));
	printk("******************** dump slaver end *********************\n");
}
static int ssi_slv_init_for_tx(struct ssi_slv *ingss)
{
		/* step1. enable loop mode */
	enable_loop_test(ingss);
	/* step2. Set reset time. */
	ssi_slv_reset(ingss);
	while (ss_read(ingss, SLV_CTRLR) & CTLR_RESET(0x3f)); // wait reset

	/* step3. disable loop mode */
	disable_loop_test(ingss);

	/* step4. */
	set_control(ingss, 0);

	/* step5. set SSI config */
	set_pol_mode(ingss, 0); //	0: High level active ; 			1: low level active
	set_pha_mode(ingss, 0); //	0: First pulse acquisition ; 	1: Second pulse acquisition

	set_tx_msb(ingss);
	set_rx_msb(ingss);
	// set_tx_lsb(ingss);
	// set_rx_lsb(ingss);

	/* step6. set SSI_SLV */
	set_txSPImode(ingss, 0); // spi trans mode  0: Standard  1: Dual mode  2: Quad mode
	set_rxSPImode(ingss, 0);

	/* step7*/
	disable_irq_interrupts(ingss);

	/* step8.*/
	set_frame_length(ingss, 32);
	transmit_count(ingss, isSLVtxLen);
	ss_write(ingss,SLV_TXN,0x1<<5);

	set_txWrthr(ingss, isSLVtxLen);

	mask_receive_interrupts(ingss, 0x37 << 26);
	mask_transmit_interrupts(ingss, 0x3f << 26);
	disable_irq_interrupts(ingss);

	enable_transmit_interrupts(ingss, TX_RDFLAG_IRQ_MASK | TX_RDERR_IRQ_MASK | TX_FULL_IRQ_MASK | 0x3f << 26);
	enable_irq_interrupts(ingss);

	start_transmit(ingss);
	ss_write(ingss, SLV_CTRLR, (1 << 29) | ss_read(ingss,SLV_CTRLR));

	return 0;
}

static int ssi_slv_init_for_ssi0(struct ssi_slv *ingss)
{
	/* step1. enable loop mode */
	enable_loop_test(ingss);
	/* step2. Set reset time. */
	ssi_slv_reset(ingss);
	while (ss_read(ingss, SLV_CTRLR) & CTLR_RESET(0x3f)); // wait reset

	/* step3. disable loop mode */
#ifndef LOOP
	disable_loop_test(ingss);
#endif
	/* step4. 0:choose cs 1:no cs*/
	set_control(ingss, 0);

	/* step5. set SSI config */
	set_pol_mode(ingss, 0); //	0: High level active ; 			1: low level active
	set_pha_mode(ingss, 0); //	0: First pulse acquisition ; 	1: Second pulse acquisition

	// set_tx_msb(ingss);
	// set_rx_msb(ingss);
	set_tx_lsb(ingss);
	set_rx_lsb(ingss);

	/* step6. set SSI_SLV 0: Standard  1: Dual mode  2: Quad mode*/
	set_txSPImode(ingss, 0);
	set_rxSPImode(ingss, 0);

	/* step7.  */
	set_frame_length(ingss, 8);
	receive_count(ingss, isSLVrxLen);  //config rx frame len
	transmit_count(ingss, isSLVtxLen); // config tx frame len
	ss_write(ingss,SLV_RXN,0x1<<5);//recv num
#ifdef LOOP
	ss_write(ingss,SLV_RXN,0x1<<5);
	ss_write(ingss,SLV_TXN,0x1<<5);
#endif

	set_rxRdthr(ingss, isSLVrxLen);
	set_txWrthr(ingss, isSLVtxLen);

	mask_receive_interrupts(ingss, 0x37 << 26);
	mask_transmit_interrupts(ingss, 0x3f << 26);

	disable_irq_interrupts(ingss);

	// enable_receive_interrupts(ingss, RX_RDFLAG_IRQ_MASK | RX_RDERR_IRQ_MASK | RX_FULL_IRQ_MASK);
	enable_receive_interrupts(ingss, RX_RDFLAG_IRQ_MASK  | RX_FULL_IRQ_MASK);
	// enable_transmit_interrupts(ingss, TX_RDFLAG_IRQ_MASK | TX_RDERR_IRQ_MASK | TX_FULL_IRQ_MASK);
	/* enable interrupts */
	enable_irq_interrupts(ingss);
	start_receive(ingss);
	start_transmit(ingss);
#ifdef LOOP
	ss_write(ingss, SLV_CTRLR, (1 << 4) | ss_read(ingss,SLV_CTRLR));//4.28
#else
	ss_write(ingss, SLV_CTRLR, (1 << 31) | ss_read(ingss,SLV_CTRLR));//4.22
	ss_write(ingss, SLV_CTRLR, (1 << 29) | ss_read(ingss,SLV_CTRLR));//4.22
#endif

	return 0;
}

static int ssi_slv_init_setup(struct ssi_slv *ingss)
{

#ifndef FPGA_TEST
	if (!clk_is_enabled(ingss->clk_gate)){
		printk("%s[%d]: enable spi slave clk\n",__func__,__LINE__);
		clk_enable(ingss->clk_gate);
	}
#endif

	/* First set the loop,then set the reset time,
		* and wait for the reset time to clear  */
	/* step1. enable loop mode */
	enable_loop_test(ingss);
	/* step2. Set reset time. */
	ssi_slv_reset(ingss);
	while (ss_read(ingss, SLV_CTRLR) & CTLR_RESET(0x3f)); // wait reset

	/* step3. disable loop mode */
#ifndef LOOP
	disable_loop_test(ingss);
#endif
	/* step4. 0:choose cs  1:no cs*/
	set_control(ingss, 0);

	/* step5. set SSI config */
#ifdef SSISLV_QUAD
	set_pol_mode(ingss, 1); //	0: High level active ; 			1: low level active
	set_pha_mode(ingss, 1); //	0: First pulse acquisition ; 	1: Second pulse acquisition
#else
	set_pol_mode(ingss, 0); //	0: High level active ; 			1: low level active
	set_pha_mode(ingss, 0); //	0: First pulse acquisition ; 	1: Second pulse acquisition
#endif
	// set_tx_msb(ingss);
	// set_rx_msb(ingss);
	set_tx_lsb(ingss);
	set_rx_lsb(ingss);

	/* step6. set SSI_SLV transfer mode 0: Standard  1: Dual mode  2: Quad mode*/
#ifdef SSISLV_QUAD
	set_txSPImode(ingss, 2);
	set_rxSPImode(ingss, 2);
#else
	set_txSPImode(ingss, 0);
	set_rxSPImode(ingss, 0);
#endif
	/* step7*/
	disable_irq_interrupts(ingss);

	/* step8.  */
	set_frame_length(ingss, 32);
	receive_count(ingss, isSLVrxLen);
	transmit_count(ingss, isSLVtxLen);
#ifdef LOOP
	ss_write(ingss,SLV_RXN,0x1<<3);
	ss_write(ingss,SLV_TXN,0x1<<3);
#endif

	set_rxRdthr(ingss, isSLVrxLen);
	set_txWrthr(ingss, isSLVtxLen);

	mask_receive_interrupts(ingss, 0x37 << 26);
	mask_transmit_interrupts(ingss, 0x3f << 26);
	disable_irq_interrupts(ingss);

	// enable_receive_interrupts(ingss, RX_RDFLAG_IRQ_MASK | RX_RDERR_IRQ_MASK | RX_FULL_IRQ_MASK);
	enable_receive_interrupts(ingss, RX_RDFLAG_IRQ_MASK  | RX_FULL_IRQ_MASK);
	// enable_transmit_interrupts(ingss, TX_RDFLAG_IRQ_MASK | TX_RDERR_IRQ_MASK | TX_FULL_IRQ_MASK);
	/* enable interrupts */
	enable_irq_interrupts(ingss);
	start_receive(ingss);
	// start_transmit(ingss);
#ifdef LOOP
	ss_write(ingss, SLV_CTRLR, (1 << 4) | ss_read(ingss,SLV_CTRLR));
#else
	ss_write(ingss, SLV_CTRLR, (1 << 31) | (1 << 18) |ss_read(ingss,SLV_CTRLR));
	ss_write(ingss, SLV_CTRLR, (1 << 29) | ss_read(ingss,SLV_CTRLR));
#endif
	return 0;
}

static int cpu_write_txfifo(struct ssi_slv *ingss)
{
	u8 i = 0;
	u8 write_buf[isSLVtxLen];
	u8 cnt = read_txfifo_entries(ingss);
	for (i = 0; i < sizeof(write_buf); i++)
		write_buf[i] = i;
	printk("tx fifo free space:%d\n", cnt);

	spin_lock(&ingss->lock);
	while ((ss_read(ingss, SLV_CTRLR) & TX_WDFLAG) == 0)
		;
	printk("ssi slv fifo free space is enough, start sending\n");

	for (i = 0; i < sizeof(write_buf); i++)
	{
		ss_write(ingss, SLV_TXD, write_buf[i]);
	}
	cnt = read_txfifo_entries(ingss);
	printk("tx fifo free space:%d\n", cnt);
	printk("ssi slv tx end!send data:%d\n", i);
	spin_unlock(&ingss->lock);
	return i;
}

static int cpu_read_rxfifo(struct ssi_slv *ingss)
{
	u32 rx_cnt = 0;
	u32 data_num = 0, error = 0;

	print_dbg("enter cpu_read_rxfifo \n");
	rx_cnt = read_rxfifo_entries(ingss);
	if (rx_cnt == 0)
		return 0;

	printk("fifo entry:%d\n",rx_cnt);
	spin_lock(&ingss->lock);

	while(read_rxfifo_entries(ingss))
	{
		// data = ss_read(ingss, SLV_RXD);
		// *rx++ = data;
		printk("data = 0x%08x\n", ss_read(ingss, SLV_RXD));
		data_num++;
	}

	spin_unlock(&ingss->lock);

	printk("INFO: receive fifo count %d  , error data %d\n", data_num, error);

	return data_num;
}

static int jz_ssi_slv_common(struct ssi_slv *ingss)
{
	u8 *rx = (char *)ingss->rx_buff;
	int ret = 0;
	if (*rx == 0x66)
	{
		enable_transmit_interrupts(ingss, TX_RDFLAG_IRQ_MASK | TX_RDERR_IRQ_MASK | TX_FULL_IRQ_MASK); // 打开TX所有中断
		ret = cpu_write_txfifo(ingss);
	}
	else
	{
		printk("[%s][%d]enable irq again\n",__func__,__LINE__);
		enable_irq_interrupts(ingss);
		// enable_receive_interrupts(ingss, RX_RDFLAG_IRQ_MASK | RX_RDERR_IRQ_MASK | RX_FULL_IRQ_MASK); // 打开RX所有中断
		enable_receive_interrupts(ingss, RX_RDFLAG_IRQ_MASK  | RX_FULL_IRQ_MASK); // 打开RX部分中断
		enable_transmit_interrupts(ingss, TX_RDFLAG_IRQ_MASK | TX_RDERR_IRQ_MASK | TX_FULL_IRQ_MASK); //打开TX所有中断
		/* enable interrupts */
		start_receive(ingss);         // 打开rx 使能
		start_transmit(ingss);         // 打开tx 使能
	}

	return ret;
}

static void irq_err_check(unsigned int reg_val)
{
	//rx
	if(reg_val & RX_RDEMPTY_ERR){
		printk("RX_RDEMPTY_ERR\n");
	}

	if(reg_val & RX_WRFULL_ERR){
		printk("RX_WRFULL_ERR\n");
	}

	//tx
	if(reg_val & TX_RDEMPTY_ERR){
		printk("TX_RDEMPTY_ERR\n");
	}

	if(reg_val & TX_WRFULL_ERR){
		printk("TX_WRFULL_ERR\n");
	}

	return ;
	}

static irqreturn_t ssi_slv_irq_handler(int irq, void *dev)
{
	struct ssi_slv *ingss = dev;
	unsigned int reg_val = 0;

	dump_spi_slaver_regs(ingss);
	disable_irq_interrupts(ingss);
	reg_val = ss_read(ingss, SLV_CTRLR);

	// printk("irq event:%d,ret:%#x\n",irq,reg_val);

	//<<<<irq err status
	irq_err_check(reg_val);

	//<<<< read write
	if(reg_val & RX_RDFLAG){
		// cpu_read_rxfifo(ingss);
		complete(&ingss->rx_done_comp);

		// finish_receive(ingss);
		// disable_irq_interrupts(ingss);
	}

	if(reg_val & TX_WDFLAG){
		printk("TX TX_WDFLAG\n");
	}

	jz_ssi_slv_common(ingss);
	return IRQ_HANDLED;
}

static int ssi_slv_read(struct ssi_slv_param *param)
{
	u32 rx_data = 0;
	u32 offset = 0,framelen = 0;

	if(!param){
		return -ENXIO;
	}
	framelen = (ss_read(ingss, SLV_CTRL_RX) & RX_BITWIDTH(0x3f)) >> 16;

	printk("start to wait rx_done_comp\n");
	wait_for_completion_interruptible(&ingss->rx_done_comp);
	printk("framelen=%d bit,paramlen:%d,wrlen:%d\n",framelen,param->rx_len,read_rxfifo_entries(ingss));

	spin_lock(&ingss->lock);

	while(read_rxfifo_entries(ingss)){
		rx_data = ss_read(ingss,SLV_RXD);
		// printk("rx_data:%d\n",rx_data);

		memcpy(param->rx+offset,&rx_data,sizeof(rx_data));
		offset += framelen/8;
		if(offset > param->rx_len){
			printk("WRNING,datalen:%d big than rx_len:%d\n",offset,param->rx_len);
			spin_unlock(&ingss->lock);
			return -1;
		}
	}

	spin_unlock(&ingss->lock);
	// //debug
	// for(i = 0; i < param->rx_len;i++){
	// printk("i:%d,rxbuf:%d\n",i,*(param->rx+(i*framelen/8)));
	// }
	return 0;
}

static void ssi_slv_dma_config(struct ssi_slv *ingss,struct ssi_slv_param *param)
{
	if(!ingss){
		printk("param is null\n");
	}

	if(param->isrx_dma){
		dma_sync_single_for_device(NULL, (dma_addr_t)param->rx_paddr, param->rx_len, DMA_TO_DEVICE);
		//config
		ingss->dma_config_rx.direction = DMA_DEV_TO_MEM;//DMA_MEM_TO_DEV;//default DMA_MEM_TO_DEV，else type dmatype+1，
		ingss->dma_config_rx.src_addr = (dma_addr_t)(ingss->phys + SLV_RXD);
		ingss->dma_config_rx.dst_addr = (dma_addr_t)param->rx_paddr;
		ingss->dma_config_rx.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		ingss->dma_config_rx.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		ingss->dma_config_rx.src_maxburst = 4;
		ingss->dma_config_rx.dst_maxburst = 4;
		printk("rx (srcaddr,dstaddr)=(%#x,%#x)\n",ingss->dma_config_rx.src_addr,ingss->dma_config_rx.dst_addr);
		dmaengine_slave_config(ingss->chan,&ingss->dma_config_rx);

		//submit
		// ingss->desc_rx = dmaengine_prep_slave_single(ingss->rx_chan,ingss->dma_config_rx.src_addr,param->rx_len,DMA_DEV_TO_MEM,DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
		ingss->desc_rx = dmaengine_prep_slave_single(ingss->chan,ingss->dma_config_rx.dst_addr,param->rx_len,DMA_DEV_TO_MEM,DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
		ingss->desc_rx->callback = dma_rx_callback;
		ingss->desc_rx->callback_param = &ingss->dma_rx_done_comp;
		if(dmaengine_submit(ingss->desc_rx) < 0) {
			printk("DMA rx Request error\n");
			return ;
		}
	} else {
		dma_sync_single_for_device(NULL, (dma_addr_t)param->tx_paddr, param->tx_len, DMA_FROM_DEVICE);
		//tx config
		ingss->dma_config_tx.direction = DMA_MEM_TO_DEV;
		ingss->dma_config_tx.src_addr = (dma_addr_t)param->tx_paddr;
		ingss->dma_config_tx.dst_addr = (dma_addr_t)(ingss->phys + SLV_TXD);
		ingss->dma_config_tx.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		ingss->dma_config_tx.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		ingss->dma_config_tx.src_maxburst = 4;
		ingss->dma_config_tx.dst_maxburst = 4;
		printk("tx (srcaddr,dstaddr)=(%#x,%#x)\n",ingss->dma_config_tx.src_addr,ingss->dma_config_tx.dst_addr);
		dmaengine_slave_config(ingss->chan,&ingss->dma_config_tx);

		//submit
		// ingss->desc_rx = dmaengine_prep_slave_single(ingss->chan,ingss->dma_config_tx.src_addr,param->tx_len,DMA_MEM_TO_DEV,DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
		ingss->desc_rx = dmaengine_prep_slave_single(ingss->chan,ingss->dma_config_tx.dst_addr,param->tx_len,DMA_MEM_TO_DEV,DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
		ingss->desc_rx->callback = dma_tx_callback;
		ingss->desc_rx->callback_param = &ingss->dma_tx_done_comp;
		if(dmaengine_submit(ingss->desc_rx) < 0) {
			printk("DMA rx Request error\n");
			return ;
		}
	}

	return ;
}

static int ssi_slv_read_dma(struct ssi_slv_param *param)
{
	u32 cnt = 0;
	static int count = 0;
	// u32 offset = 0,framelen = 0;
	u32 i = 0;
	if(!param){
		return -ENXIO;
	}

	receive_count(ingss, param->rx_len); // config rX frame len
	set_frame_length(ingss, 32); // config tx frame width，bit
	set_rxRdthr(ingss, 32); // rx threshold
	// set_tx_msb(ingss);//msb
	// set_rx_msb(ingss);
	set_tx_lsb(ingss);
	set_rx_lsb(ingss);
	disable_irq_interrupts(ingss);

	if(count++ == 0){
		dump_spi_slaver_regs(ingss);
	}

	ssi_slv_dma_config(ingss,param);

	// rx enable
	start_receive(ingss);

	cnt = read_rxfifo_entries(ingss);
	printk("rx fifo fill space:%d,unfill space:%d\n", cnt,ss_read(ingss, SLV_FLA)&0xff);

	//pending dma
	printk("start to pending dma\n");
	dma_async_issue_pending(ingss->chan);
	wait_for_completion_interruptible(&ingss->dma_rx_done_comp);

	if(count++ == 1){
		printk("after dma\n");
		dump_spi_slaver_regs(ingss);
	}
	//for debug
	for(i = 0;i < param->rx_len;i++){
		printk("read buf[%d] :%#x,rxd:%#x\n",i,((int*)param->rx_vaddr)[i], ss_read(ingss,SLV_RXD));
		// printk("read buf[%d] :%#x\n",i,((char*)param->rx_vaddr)[i]);
	}

	return 0;
}

static int ssi_slv_write_dma(struct ssi_slv_param *param)
{
	u32 cnt = 0;
	u32 i = 0;
	static int count = 0;

	if(!param){
		return -ENXIO;
	}

	//debug
	for(i = 0;i < param->tx_len;i++){
		// printk("src param txbuf:%d\n",*(param->tx+i*sizeof(int)));
		printk("src param txbuf:%#x\n",((int*)param->tx_vaddr)[i]);
		// ss_write(ingss, SLV_TXD,param->tx[i]);
	}

	transmit_count(ingss, param->tx_len); // config TX len
	set_frame_length(ingss, 32); // config TX width, bit
	set_txWrthr(ingss, 4); // tx threshold
	// set_tx_msb(ingss);//00 msb
	// set_rx_msb(ingss);
	set_tx_lsb(ingss);
	set_rx_lsb(ingss);

	// tx enable
	start_transmit(ingss);

	if(count++ == 0){
		dump_spi_slaver_regs(ingss);
	}

	// ssi_slv_dma(ingss);
	ssi_slv_dma_config(ingss,param);

	cnt = read_txfifo_entries(ingss);
	printk("tx fifo unfill space:%d,fill space:%d\n", cnt,(ss_read(ingss, SLV_FLA)>>24 & 0xff));
	while(!(ss_read(ingss, SLV_CTRLR) & (1<<23)));

	//pending dma
	printk("start to pending dma\n");
	dma_async_issue_pending(ingss->chan);
	wait_for_completion_interruptible(&ingss->dma_tx_done_comp);

	//for debug
	cnt = read_txfifo_entries(ingss);
	printk("rx_rdflag:%d,unfill space:%d\n",ss_read(ingss, SLV_CTRLR) & (1<<18),cnt);
	//for debug
	cnt = ss_read(ingss, SLV_FLA)>>24 & 0xff;
	printk("fill space:%d\n",cnt);
	while(cnt--){
		printk("txddata:%#x\n",ss_read(ingss,SLV_TXD));
	}
	return 0;
}

static int ssi_slv_write(struct ssi_slv_param *param)
{
	u32 cnt = 0;
	static int count = 0;
	// u32 offset = 0,framelen = 0;
	u32 i = 0;
		if(!param){
		return -ENXIO;
	}
	//debug
	for(i = 0;i < param->tx_len;i++){
		// printk("src param txbuf:%d\n",*(param->tx+i*sizeof(int)));
		printk("src param txbuf:%d\n",param->tx[i]);
		// ss_write(ingss, SLV_TXD,param->tx[i]);
	}

#if 0
	//设置动态阈值，即发送写的数据量，
	// transmit_count(ingss, param->tx_len); // 配置TX数据帧长度
	transmit_count(ingss, 32); // 配置TX数据帧个数，该长度随着写入txd的数据量而减少
	receive_count(ingss,32);//配置rx的数据帧个数也为32
	set_rxRdthr(ingss, 4); // rx 阈值  如果SSLVFLR[15:8]rx_fifo_data 大于这个值时候就会触发RX中断
	set_txWrthr(ingss, 4); // tx 阈值  如果SSLVFLR[7:0]tx__unfill_data 大于这个值时候就会触发TX中断
	set_tx_msb(ingss);//默认使用大端模式
	set_rx_msb(ingss);

	// set_tx_lsb(ingss);
	// set_rx_lsb(ingss);

	//根据实际需求改大小端，这里默认改成00
	ss_write(ingss, SLV_CTRL_TX, ss_read(ingss, SLV_CTRL_TX) & (~(0x3<<22)));
	ss_write(ingss, SLV_CTRL_RX, ss_read(ingss, SLV_CTRL_RX) & (~(0x3<<22)));
	start_transmit(ingss);
#endif

	if(count++ == 0){
		dump_spi_slaver_regs(ingss);
	}

	cnt = read_txfifo_entries(ingss);
	printk("tx fifo free space:%d\n", cnt);
	while(!(ss_read(ingss, SLV_CTRLR) & (1<<23)));

	//for debug
	for(i = 0 ;i < 32;i++){
		// printk("write tx:%d\n",i);
		ss_write(ingss, SLV_TXD,i);
	}

	// printk("txbit22 :%d\n",(ss_read(ingss, SLV_CTRLR) & (1<<22) )>> 22);

	cnt = read_txfifo_entries(ingss);
	printk("rx_rdflag:%d,fifo free space:%d\n",ss_read(ingss, SLV_CTRLR) & (1<<23),cnt);
	while(!(ss_read(ingss, SLV_CTRLR) & (1<<23))){
		msleep(100);
	}

	// printk("aft tx fifo free space:%d,rx entry:%d\n", cnt,read_rxfifo_entries(ingss));

	// while(read_rxfifo_entries(ingss)){
	// 	msleep(100);
	// 	// printk("rx_data:%d\n",ss_read(ingss,SLV_RXD));
	// }

	return 0;

}

static bool dma_chan_filter(struct dma_chan *chan, void *filter_param)
{
	struct ssi_slv *ingss = (struct ssi_slv*)filter_param;
#if defined(CONFIG_KERNEL_4_4_94)
	return ingss->dma_type == (int)chan->private;
#else
	// printk("rx dma_chan:%d,type:%#x,dma_type:%#x\n",chan->chan_id,(int)chan->private,ingss->dma_type_rx);
	if(ingss->dma_type == (int)chan->private){
		// printk("dma_chan:%d,type:%#x,dma_type:%#x\n",chan->chan_id,(int)chan->private,ingss->dma_type);
		return 1;
	} else {
		return 0;
	}
#endif
}

static void ssi_slv_dma(struct ssi_slv *ingss)
{
	dma_cap_mask_t mask;
	if(!ingss){
		printk("err param is null\n");
		return ;
	}

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE,mask);
	ingss->chan = dma_request_channel(mask,dma_chan_filter,(void *)ingss);
	if(!ingss->chan) {
		printk("Request DMA RX channel error!\n");
		return;
	}

	return;
}

static int ingss_misc_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ingss_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long ingss_misc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct miscdevice *dev = filp->private_data;
	struct ssi_slv *ingss = container_of(dev, struct ssi_slv , mdev);

	struct ssi_slv_param param;
	struct ssi_slv_param *usr_param = NULL;

	memset(&param,0x0,sizeof(param));
	usr_param = (struct ssi_slv_param *)arg;

	switch (cmd) {
		case IOCTL_SSISLV_READ:
			if (copy_from_user(&param, usr_param, sizeof(struct ssi_slv_param))) {
				printk("ssislv set para copy_from_user error!!!\n");
				return -EFAULT;
			}
			param.rx = kzalloc(param.rx_len, GFP_KERNEL);
			if(!param.rx){
				printk("no memm\n");
				return -ENOMEM;
			}

			//get param
			ssi_slv_read(&param);

			if (copy_to_user(usr_param->rx, param.rx, param.rx_len)) {
				printk("ssislv set para copy_to_user error!!!\n");
				return -EFAULT;
			}

			break;
		case IOCTL_SSISLV_WRITE:
			if (copy_from_user(&param, usr_param, sizeof(struct ssi_slv_param))) {
				printk("ssislv get para copy_from_user error!!!\n");
				return -EFAULT;
			}

			param.tx = kzalloc(param.tx_len, GFP_KERNEL);
			if(!param.tx){
				printk("no memm\n");
				return -ENOMEM;
			}

			if(copy_from_user(param.tx, usr_param->tx,param.tx_len)){
				printk("ssislv get para copy_from_user error!!!\n");
				return -EFAULT;
			}

			ssi_slv_write(&param);

			// if (copy_to_user(usr_param->tx, param.tx, param.tx_len)) {
			//     printk("ssislv set para copy_to_user error!!!\n");
			//     return -EFAULT;
			// }
			break;
		case IOCTL_SSISLV_DMA_READ:
			//ssi_slv rx
			if (copy_from_user(&param, usr_param, sizeof(struct ssi_slv_param))) {
				printk("ssislv set para copy_from_user error!!!\n");
				return -EFAULT;
			}

			if(!param.rx_len){
				printk("err,rx_len is 0,check\n");
			}

			param.rx_vaddr = dma_alloc_coherent(ingss->dev, param.rx_len,
				&param.rx_paddr, GFP_KERNEL | GFP_DMA);
			if(!param.rx_vaddr){
				printk("no memm\n");
				return -ENOMEM;
			}
			memset(param.rx_vaddr,0xff,param.rx_len);
			// memset(param.rx_vaddr,0xff,PAGE_SIZE);
			printk("rx paddr:%#x,len:%d\n",param.rx_paddr,param.rx_len);
			//get param
			// ssi_slv_read_dma(&param);
			param.isrx_dma = 1;
			ssi_slv_read_dma(&param);
			param.isrx_dma = 0;

			if (copy_to_user(usr_param->rx_vaddr, param.rx_vaddr, param.rx_len)) {
				printk("ssislv set para copy_to_user error!!!\n");
				return -EFAULT;
			}

			break;
		case IOCTL_SSISLV_DMA_WRITE:
			//ssi_slv tx
			if (copy_from_user(&param, usr_param, sizeof(struct ssi_slv_param))) {
				printk("ssislv get para copy_from_user error!!!\n");
				return -EFAULT;
			}

			param.tx_vaddr = dma_alloc_coherent(ingss->dev, param.tx_len,
				&param.tx_paddr, GFP_KERNEL);
			if(!param.tx_vaddr){
				printk("no memm\n");
				return -ENOMEM;
			}
			printk("tx paddr:%#x\n",param.tx_paddr);

			if(copy_from_user(param.tx_vaddr, usr_param->tx_vaddr,param.tx_len)){
				printk("ssislv get para copy_from_user error!!!\n");
				return -EFAULT;
			}

			//get param
			param.istx_dma = 1;
			ssi_slv_write_dma(&param);
			param.istx_dma = 0;
			// if (copy_to_user(usr_param->tx, param.tx, param.tx_len)) {
			//     printk("ssislv set para copy_to_user error!!!\n");
			//     return -EFAULT;
			// }
			break;
		default:
			printk("[%s][%d]not support yet\n",__func__,__LINE__);
			break;
	}

	return ret;
}
static struct file_operations ingss_misc_fops = {
	.open = ingss_misc_open,
	.release = ingss_misc_release,
	.unlocked_ioctl = ingss_misc_ioctl,
};

static int ingenic_ssi_slv_probe(struct platform_device *pdev)
{
	int err = 0, ret = 0;

	ingss = kzalloc(sizeof(struct ssi_slv), GFP_KERNEL);
	if (ingss == NULL)
	{
		printk("ERROR: ingss kzalloc failed ! \n");
	}

	ingss->rx_buff = kmalloc((32 * 32) / 4, GFP_KERNEL);
	if (!ingss->rx_buff)
	{
		printk("ERROR: rx_buff kmalloc failed ! \n");
		goto error;
	}

	spin_lock_init(&ingss->lock);
	init_completion(&ingss->rx_done_comp);
	init_completion(&ingss->tx_done_comp);
	init_completion(&ingss->dma_rx_done_comp);
	init_completion(&ingss->dma_tx_done_comp);

	ingss->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!ingss->res)
	{
		dev_err(&pdev->dev, "Unable to get SPI MEM resource\n");
		return -ENXIO;
	}

	ingss->phys = ingss->res->start;
	ingss->dev = &pdev->dev;
#ifndef SSISLV_DMA
	ingss->irq = platform_get_irq(pdev, 0);
	if (ingss->irq <= 0)
	{
		dev_err(&pdev->dev, "No IRQ specified\n");
		err = -ENOENT;
		goto err_no_irq;
	}
#else
	printk("start to get dam resource\n");
	ingss->dma_res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!ingss->res)
	{
		dev_err(&pdev->dev, "Unable to get SPI MEM resource\n");
		return -ENXIO;
	}

	printk("res_tx->start:%#x\n",ingss->dma_res->start);
	ingss->dma_type = GET_MAP_TYPE(ingss->dma_res->start);
	printk("dma_type:%#x\n",ingss->dma_type);

#endif
	ingss->iomem = devm_ioremap_resource(&pdev->dev, ingss->res);
	if (!ingss->iomem)
	{
		dev_err(&pdev->dev, "Cannot map IO\n");
		err = -ENXIO;
		goto err_no_iomap;
	}

#ifndef FPGA_TEST
	ingss->clk_gate = devm_clk_get(&pdev->dev, "gate_ssi_slv");
	if(IS_ERR(ingss->clk_gate)) {
		ingss->clk_gate = NULL;
		printk("warningL ssi slv clk get failed ! \n");
		goto failed_get_clk;
	}

	if(clk_get_rate(ingss->clk_gate) > 24*1000*1000){
		clk_set_rate(ingss->clk_gate,24*1000*1000);
	}

	if(ingss->clk_gate)
		clk_prepare_enable(ingss->clk_gate);
#endif

#ifdef SSISLV_QUAD
	ssi_slv_init_setup(ingss);
#else
	ssi_slv_init_for_ssi0(ingss);
#endif
	// ssi_slv_init_for_tx(ingss);

	dump_spi_slaver_regs(ingss);
	/* request spi slave irq */
#ifndef SSISLV_DMA
	ret = devm_request_irq(&pdev->dev,ingss->irq, ssi_slv_irq_handler, 0, "ssi_slv", (void *)ingss);
	if (ret)
	{
		printk("ERROR: request irq error ! \n");
		goto err_req_irq;
	}
#else
	ssi_slv_dma(ingss);
#endif

	memset(&ingss->mdev, 0, sizeof(struct miscdevice));
	ingss->mdev.minor = MISC_DYNAMIC_MINOR;
	ingss->mdev.name = "jz_spi_slave";
	ingss->mdev.fops = &ingss_misc_fops;

	ret = misc_register(&ingss->mdev);
	if (ret < 0)
	{
		printk("ERROR: jzss miscdev register failed ! \n");
		goto err_misc_register;
	}

	printk("jzss driver init ok ! \n");

	return 0;

err_misc_register:
err_req_irq:
err_no_iomap:
err_no_irq:
failed_get_clk:
error:
	return 0;
}

static int ingenic_ssi_slv_remove(struct platform_device *pdev)
{
	misc_deregister(&ingss->mdev);
	kfree(ingss->rx_buff);
	kfree(ingss);

	return 0;
}

static int ingenic_ssi_slv_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int ingenic_ssi_slv_resume(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id ingenic_ssi_slv_match[] = {
	{
		.compatible = "ingenic,ssi_slv",
	},
	{}};
MODULE_DEVICE_TABLE(of, ingenic_ssi_slv_match);

static struct platform_driver ingenic_ssi_slv_drv = {
	.probe = ingenic_ssi_slv_probe,
	.remove = ingenic_ssi_slv_remove,
	.suspend = ingenic_ssi_slv_suspend,
	.resume = ingenic_ssi_slv_resume,
	.driver = {
		.name = "jz_spi_slave",
		.of_match_table = ingenic_ssi_slv_match,
		.owner = THIS_MODULE,
	},
};


static int __init ssi_slv_init(void)
{
	int ret;
	ret = platform_driver_register(&ingenic_ssi_slv_drv);
	return 0;
}

static void __exit ssi_slv_exit(void)
{
	platform_driver_unregister(&ingenic_ssi_slv_drv);
	return ;
}

module_init(ssi_slv_init);
module_exit(ssi_slv_exit);

MODULE_ALIAS("ingenic_ssi_slv");
MODULE_AUTHOR("Bo Liu <bo.liu@ingenic.com>");
MODULE_AUTHOR("jim.jwang@ingenic.com>");
MODULE_DESCRIPTION("INGENIC ssi_slv controller driver");
MODULE_LICENSE("GPL");
