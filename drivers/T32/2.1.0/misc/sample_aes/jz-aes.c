/*
 * Cryptographic API.
 *
 * Support for Tseries AES HW.
 *
 * Copyright (c) 2019 Ingenic Corporation
 * Author: Niky shen <xianghui.shen@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 */

#include <asm-generic/div64.h>
#include <asm/io.h>
#include <linux/completion.h>
#include <linux/gfp.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/crypto.h>
#include <linux/interrupt.h>
#include <crypto/scatterwalk.h>
#include <crypto/aes.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <soc/base.h>
#if (defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)) && defined(CONFIG_SOC_PRJ007)
#include <dt-bindings/dma/ingenic-pdma.h>
#include <dt-bindings/interrupt-controller/PRJ007-irq.h>
#elif (defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)) && defined(CONFIG_SOC_PRJ008)
#include <dt-bindings/dma/ingenic-pdma.h>
#include <dt-bindings/interrupt-controller/PRJ008-irq.h>
#else
#include <soc/irq.h>
#include <mach/jzdma.h>
#endif
//#include <mach/platform.h>
#include "jz-aes.h"
#include "pdmac.h"

// #define DEBUG_AES_SG
// #define DEBUG
#ifdef DEBUG
#define aes_debug(format, ...) {printk(format, ## __VA_ARGS__);}
#else
#define aes_debug(format, ...) do{ } while(0)
#endif

#define AES_DRIVER_VERSION "H20251024a"
#define DESCRIPT_NUM (260)

#ifdef DEBUG_TIME_CONSUME
struct timeval tvime1;;
struct timeval tvime2;

#define HISTORY_SIZE 30

typedef struct {
	unsigned long call_count;       // 该场景总调用次数
	unsigned long total_data;       // 该场景总数据量
	unsigned long data_history[HISTORY_SIZE];  // 数据大小历史
	unsigned long time_history[HISTORY_SIZE];  // 耗时历史（加密/解密时间）
	int history_index;              // 该场景的历史记录索引
	ktime_t start_time;             // 该场景起始时间
	ktime_t last_print;             // 该场景上次打印时间
	bool inited;                    // 该场景初始化标记
} ScenarioStats;


static ScenarioStats enc_stats = {0};
static ScenarioStats dec_stats = {0};


static inline void init_scenario_stats(ScenarioStats *stats) {
	int i;
	for (i = 0; i < HISTORY_SIZE; i++) {
		stats->data_history[i] = 0;
		stats->time_history[i] = 0;
	}
	stats->start_time = ktime_get();
	stats->last_print = stats->start_time;
	stats->inited = true;
}


void log_data_enc(unsigned long dealsize,unsigned long data_size, long enc_us, const char *name) {
	int i;
	ScenarioStats *stats = &enc_stats;
	ktime_t now = ktime_get();
	s64 diff_ns;

	if (!stats->inited) {
		init_scenario_stats(stats);
	}

	stats->call_count++;
	stats->total_data += data_size;

	stats->data_history[stats->history_index] = data_size;
	stats->time_history[stats->history_index] = enc_us;
	stats->history_index = (stats->history_index + 1) % HISTORY_SIZE;

	// 超过2秒打印加密统计（仅用加密自己的计时器）
	diff_ns = ktime_to_ns(ktime_sub(now, stats->last_print));
	if (diff_ns > 2LL * 1000000000LL) {
		u32 valid_count = (stats->call_count < HISTORY_SIZE) ? stats->call_count : HISTORY_SIZE;


		u64 total_enc = 0, avg_enc = 0;
		u64 min_enc = ~(u64)0;
		u64 max_enc = 0;
		for (i = 0; i < valid_count; i++) {
			u64 v = stats->time_history[i];
			total_enc += v;
			if (v < min_enc) min_enc = v;
			if (v > max_enc) max_enc = v;
		}
		avg_enc = total_enc;
		do_div(avg_enc, valid_count);

		printk("\n");
		printk(KERN_INFO "hw aes %s dealsize:%lu size:%lu totaldata:%lu,[enc :total:%llu (min~max/avg)=( %llu~%llu / %llu )] \n",
			name, dealsize,data_size, stats->total_data,
			total_enc, min_enc, max_enc, avg_enc);

		stats->last_print = now;
	}
}

void log_data_dec(unsigned long dealsize,unsigned long data_size, long dec_us, const char *name) {
	int i;
	ScenarioStats *stats = &dec_stats;
	ktime_t now = ktime_get();
	s64 diff_ns;

	if (!stats->inited) {
		init_scenario_stats(stats);
	}

	stats->call_count++;
	stats->total_data += data_size;

	stats->data_history[stats->history_index] = data_size;
	stats->time_history[stats->history_index] = dec_us;
	stats->history_index = (stats->history_index + 1) % HISTORY_SIZE;


	diff_ns = ktime_to_ns(ktime_sub(now, stats->last_print));
	if (diff_ns > 2LL * 1000000000LL) {

		u32 valid_count = (stats->call_count < HISTORY_SIZE) ? stats->call_count : HISTORY_SIZE;


		u64 total_dec = 0, avg_dec = 0;
		u64 min_dec = ~(u64)0;  // 初始最大
		u64 max_dec = 0;
		for (i = 0; i < valid_count; i++) {
			u64 v = stats->time_history[i];
			total_dec += v;
			if (v < min_dec) min_dec = v;
			if (v > max_dec) max_dec = v;
		}
		avg_dec = total_dec;
		do_div(avg_dec, valid_count);

		printk("\n");
		printk(KERN_INFO "hw aes %s dealsize:%lu size:%lu totaldata:%lu,[dec :total:%llu (min~max/avg)=( %llu~%llu / %llu )]\n",
			name, dealsize,data_size, stats->total_data,
			total_dec, min_dec, max_dec, avg_dec);

		stats->last_print = now;
	}
}
#endif

#if defined(DEBUG) && defined(CONFIG_SOC_PRJ007)
static void debug_regs(struct aes_operation *aes)
{
	printk("AES_ASCR = 0x%08x\n", aes_reg_read(aes, AES_ASCR));
	printk("AES_ASSR = 0x%08x\n", aes_reg_read(aes, AES_ASSR));
	printk("AES_ASINTM = 0x%08x\n", aes_reg_read(aes, AES_ASINTM));
	printk("AES_ASSA = 0x%08x\n", aes_reg_read(aes, AES_ASSA));
	printk("AES_ASDA = 0x%08x\n", aes_reg_read(aes, AES_ASDA));
	printk("AES_ASTC = 0x%08x\n", aes_reg_read(aes, AES_ASTC));
}
#elif defined(DEBUG) && defined(CONFIG_SOC_PRJ008)
static void debug_regs(struct aes_operation *aes)
{
	printk("**********************************\n");
	printk("AES_ASCR = 0x%08x\n", aes_reg_read(aes, AES_ASCR));
	printk("AES_ASSR = 0x%08x\n", aes_reg_read(aes, AES_ASSR));
	printk("AES_ASINTM = 0x%08x\n", aes_reg_read(aes, AES_ASINTM));
	printk("AES_ASSA = 0x%08x\n", aes_reg_read(aes, AES_ASSA));
	printk("AES_ASTC = 0x%08x\n", aes_reg_read(aes, AES_ASTC));
	printk("AES_ASDA = 0x%08x\n", aes_reg_read(aes, AES_ASDA));
	printk("AES_ASDTC = 0x%08x\n", aes_reg_read(aes, AES_ASDTC));
	printk("AES_ASBNUM = 0x%08x\n", aes_reg_read(aes, AES_ASBNUM));
	//printk("AES_ASDO = 0x%08x\n", aes_reg_read(aes, AES_ASDO));
	printk("**********************************\n");
}
#else
// static void debug_regs(struct aes_operation *aes) {}
#endif

struct aes_pdma_para g_pdma_src[DESCRIPT_NUM];
struct aes_pdma_para g_pdma_dst[DESCRIPT_NUM];

#if defined(CONFIG_SOC_PRJ008) && defined(DEBUG_AES_SG)
static int pdma_config(struct aes_operation *aes, dma_addr_t src, dma_addr_t dst ,int len, int chn, int type);
#endif

static int aes_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);

	aes->state = 1;

	printk("%s: aes open successful!\n", __func__);
	return 0;
}

static int aes_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);

	aes->state = 0;
	return 0;
}

static ssize_t aes_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t aes_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static int aes_start_crypt_processing(struct aes_operation *aes)
{
	struct aes_para *para = &aes->para;
	int i = 0;
	int offset = 0, len = 0;
	int debug = 0;
#ifdef DEBUG_TIME_CONSUME
	long onetimes = 0;
	static int tmp_len = 0,totaltime = 0;
#endif
	para->donelen = 0;
	para->status = JZ_AES_STATUS_PREPARE;
	if(para->datalen % 16){
		printk("The size of data should be 16bytes aligned!\n");
		return -EINVAL;
	}

	if(para->enworkmode >= IN_UNF_CIPHER_WORK_MODE_OTHER){
		printk("The enworkmode is invalid!\n");
		return -EINVAL;
	}

	para->status = JZ_AES_STATUS_WORKING;
	while(offset < para->datalen){
		len = para->datalen - offset > aes->dma_datalen ? aes->dma_datalen : para->datalen - offset;

		dma_cache_sync(aes->dev, aes->src_addr_v, len, DMA_TO_DEVICE);

		/* set transfer count */
#if defined(CONFIG_SOC_PRJ007)
		aes_reg_write(aes, AES_ASTC, len / 16);
#elif defined(CONFIG_SOC_PRJ008)
		/* set Block Number */
		aes_reg_write(aes, AES_ASBNUM, len / 16);

		/* set transfer count */
		aes_reg_write(aes, AES_ASTC, len / 4);

		/* set destination transfer count */
		aes_reg_write(aes, AES_ASDTC, len / 4);
#endif
		/* start AES DMA */
		// time1
#ifdef DEBUG_TIME_CONSUME
		do_gettimeofday(&tvime1);
#endif
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<9);

		/* wait for AES done */
		i = 1000;
		debug = 0;
		// debug_regs(aes);
		while(i--){
			if(!wait_for_completion_interruptible(&aes->dma_comp)){
				break;
			}
		}
#ifdef DEBUG_TIME_CONSUME
		//统计相应的数据量时间
		tmp_len += len;
		onetimes = (tvime2.tv_sec - tvime1.tv_sec) * 1000000 + (tvime2.tv_usec - tvime1.tv_usec);
		totaltime += onetimes;
		 // printk("aes crypt %d KB data time is %ld us\n", tmp_len/1024, onetimes);
		unsigned long dealsize = aes->para.datalen; //本次处理数据量
		if(para->enworkmode==IN_UNF_CIPHER_WORK_MODE_ENC_CBC){
			if(tmp_len % (dealsize) == 0){
				// printk("enc templen: %d datalen:%d\n",tmp_len,para->datalen);
				log_data_enc(dealsize,tmp_len, totaltime, "aes_hw_cbc_enc");
				tmp_len = 0;
				totaltime = 0;
			}
		 } else if(para->enworkmode==IN_UNF_CIPHER_WORK_MODE_DEC_CBC){
			if(tmp_len % (dealsize) == 0){
				// printk("dec templen: %d datalen:%d\n",tmp_len,para->datalen);
				log_data_dec(dealsize,tmp_len, totaltime, "aes_hw_cbc_dec");
				tmp_len = 0;
				totaltime = 0;
			}
		 }
#endif
		if(i <= 0){
			printk("%s[%d]: timeout!\n",__func__,__LINE__);
			break;
		}

		offset += len;
		para->donelen = offset;
	}

	para->status = JZ_AES_STATUS_DONE;
	return 0;
}

#if defined(CONFIG_SOC_PRJ008)

static bool dmatx_chan_filter(struct dma_chan *chan, void *filter_param)
{
	struct aes_operation *aes = (struct aes_operation*)filter_param;

	// printk("tx dma_chan:%d,type:%#x,dma_type:%#x\n",chan->chan_id,(int)chan->private,i2c->dma_type_tx);
	return aes->dma_tx_type == (int)chan->private;
}

static bool dmarx_chan_filter(struct dma_chan *chan, void *filter_param)
{
	struct aes_operation *aes = (struct aes_operation*)filter_param;

	// printk("tx dma_chan:%d,type:%#x,dma_type:%#x\n",chan->chan_id,(int)chan->private,i2c->dma_type_tx);

	return aes->dma_rx_type == (int)chan->private;
}


static void dma_write_done_callback(void *callback_param)
{
	struct aes_operation *aes = callback_param;
	complete(&aes->hwdma_tx_done);
	return ;
}

static void dma_read_done_callback(void *callback_param)
{
	struct aes_operation *aes = callback_param;
	complete(&aes->hwdma_rx_done);
	return ;
}


static int aes_dma_config_tx(struct aes_operation *aes,dma_addr_t src,dma_addr_t dst,int trans_len)
{
	int ret = 0;
	if(!aes){
		printk("err aes param is null\n");
		return -1;
	}

	aes->dma_config_tx.direction = DMA_MEM_TO_DEV;
	aes->dma_config_tx.src_addr = src;
	aes->dma_config_tx.dst_addr = dst;
	aes->dma_config_tx.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	aes->dma_config_tx.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	aes->dma_config_tx.src_maxburst = DMA_SLAVE_BUSWIDTH_2_BYTES;
	aes->dma_config_tx.dst_maxburst = DMA_SLAVE_BUSWIDTH_2_BYTES;
	// printk("tx (srcaddr,dstaddr)=(%#x,%#x)\n",aes->dma_config_tx.src_addr,aes->dma_config_tx.dst_addr);
	ret = dmaengine_slave_config(aes->chan_tx,&aes->dma_config_tx);
	if(ret < 0){
		printk("i2c dmaengine_slave_config err\n");
		return -1;
	}

	//submit
	aes->desc_tx= dmaengine_prep_slave_single(aes->chan_tx,aes->dma_config_tx.src_addr,\
				trans_len,DMA_MEM_TO_DEV,DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
	if(!aes->desc_tx){
		printk("dmaengine_prep_slave_single err\n");
		return -1;
	}

	aes->desc_tx->callback = dma_write_done_callback;
	aes->desc_tx->callback_param = aes;
	if(dmaengine_submit(aes->desc_tx) < 0) {
		printk("DMA rx Request error\n");
		return -1;
	}

	dma_async_issue_pending(aes->chan_tx);

	return 0;
}

static int aes_dma_config_rx(struct aes_operation *aes,dma_addr_t src,dma_addr_t dst,int trans_len)
{
	int ret = 0;
	if(!aes){
		printk("err aes param is null\n");
		return -1;
	}

	aes->dma_config_rx.direction = DMA_MEM_TO_DEV;
	aes->dma_config_rx.src_addr = src;
	aes->dma_config_rx.dst_addr = dst;
	aes->dma_config_rx.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	aes->dma_config_rx.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	aes->dma_config_rx.src_maxburst = DMA_SLAVE_BUSWIDTH_2_BYTES;
	aes->dma_config_rx.dst_maxburst = DMA_SLAVE_BUSWIDTH_2_BYTES;
	ret = dmaengine_slave_config(aes->chan_rx,&aes->dma_config_rx);
	if(ret < 0){
		printk("i2c dmaengine_slave_config err\n");
		return -1;
	}

	//submit
	aes->desc_rx= dmaengine_prep_slave_single(aes->chan_rx,aes->dma_config_rx.src_addr,\
				trans_len,DMA_MEM_TO_DEV,DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
	if(!aes->desc_rx){
		printk("dmaengine_prep_slave_single err\n");
		return -1;
	}

	aes->desc_rx->callback = dma_read_done_callback;
	aes->desc_rx->callback_param = aes;
	if(dmaengine_submit(aes->desc_rx) < 0) {
		printk("DMA rx Request error\n");
		return -1;
	}
	dma_async_issue_pending(aes->chan_rx);

	return 0;
}

static int aes_dma_init(struct aes_operation *aes)
{
	dma_cap_mask_t mask_tx,mask_rx;
	if(!aes){
		printk("err param is null\n");
		return -1;
	}

#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
	aes->dma_tx_type = INGENIC_DMA_REQ_AES0_TX;
	aes->dma_rx_type = INGENIC_DMA_REQ_AES1_TX;
#else
	aes->dma_tx_type = JZDMA_REQ_AES0_TX;
	aes->dma_rx_type = JZDMA_REQ_AES1_TX;
#endif

	dma_cap_zero(mask_tx);
	dma_cap_set(DMA_SLAVE,mask_tx);
	aes->chan_tx = dma_request_channel(mask_tx,dmatx_chan_filter,(void *)aes);
	if(!aes->chan_tx) {
		printk("%s Request DMA TX channel error!\n",__FUNCTION__);
		return -1;
	}

	dma_cap_zero(mask_rx);
	dma_cap_set(DMA_SLAVE,mask_rx);
	aes->chan_rx = dma_request_channel(mask_rx,dmarx_chan_filter,(void *)aes);
	if(!aes->chan_rx) {
		printk("%s Request DMA RX channel error!\n",__FUNCTION__);
		return -1;
	}

	return 0;
}

static void aes_dma_exit(struct aes_operation *aes)
{
	if(aes->chan_tx){
		dma_release_channel(aes->chan_tx);
		aes->chan_tx = NULL;
	}

	if(aes->chan_rx){
		dma_release_channel(aes->chan_rx);
		aes->chan_rx = NULL;
	}
}
#endif

static int jz_aes_init(struct file *file, struct aes_operation *aes)
{

	struct aes_para *para = &aes->para;
	int writekey_times, setkey_reg, key_byte;
	int i = 0;
	/* Clear AES done and KEY done status */
#if defined(CONFIG_SOC_PRJ007)
	aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR));
#elif defined(CONFIG_SOC_PRJ008)
	aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR) & (1<<1));
#endif

	/* enable AES dma done and KEY done interrupt */
#if defined(CONFIG_SOC_PRJ007)
	aes_reg_write(aes, AES_ASINTM, 0x5);
#elif defined(CONFIG_SOC_PRJ008)
	aes_reg_write(aes, AES_ASINTM, 0x2);
#endif

	/* Clear the IV and KEYS; enable AES module */
	aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<10 | 1 << 0) ;

	switch( para->keybits )
	{
		case 128:
			setkey_reg = 0;
			break;
		case 192:
			setkey_reg = 1;
			break;
		case 256:
			setkey_reg = 2;
			break;
		default :
			printk("[%s] error keybit\n",__func__);
			return -1;
	}
	writekey_times = para->keybits >> 5;// para->keybits/32;
	key_byte = para->keybits >> 3;//para->keybits/8;

	if((para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_ECB ) || (para->enworkmode == IN_UNF_CIPHER_WORK_MODE_DEC_ECB)){
		if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_ECB){
			/* Enable AES DMA, Encrypts, ECB mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<0 | 1<<8 | setkey_reg << 6);
		}else{
			/* Enable AES DMA, Decrypts, ECB mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<0 | 1<<4 | 1<<8 | setkey_reg << 6);
		}
	}else{
		if(para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_CBC){
			/* Enable AES DMA, Encrypts, CBC mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<8 | 1<<5 | 1<<0 | setkey_reg << 6);
#ifdef CONFIG_SOC_PRJ008
			aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<12 | 1<<13);//小端输入、输出
#endif
		}else{
			/* Enable AES DMA, Decrypts, CBC mode */
			aes_reg_write(aes, AES_ASCR, 3 << 28 | 1<<8 | 1<<5 | 1<<4 | 1<<0 | setkey_reg << 6);
#ifdef CONFIG_SOC_PRJ008
			aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<13);//大端输入、小端输出
#endif
		}
		for(i = 0; i < 4; i++){
			aes_reg_write(aes, AES_ASIV, para->aesiv[i]);
		}

		/* write IV init */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<1);
	}
#if defined(CONFIG_SOC_PRJ007)
	AES_LITTLE_ENDIAN;
#endif
	/* input KEYS */
	for(i = 0; i < writekey_times; i++)
	{
		aes_reg_write(aes, AES_ASKY, para->aeskey[i]);
	}

	/* write KEYS start */
	aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<2);

	aes_reg_write(aes, AES_ASSA, aes->src_addr_p);
	// aes_reg_write(aes, AES_ASDA, aes->dst_addr_p);
	aes_reg_write(aes, AES_ASDA, aes->src_addr_p);

	/* wait for key done */
#if defined(CONFIG_SOC_PRJ007)
	i = 1000;
	while(i--){
		if(!wait_for_completion_interruptible(&aes->key_comp))
			break;
	}
#endif
	para->status = JZ_AES_STATUS_DONE;
	return 0;

}

static int aes_start_processing(struct file *file, struct aes_operation *aes)
{
	int ret = 0;

	jz_aes_init(file,aes);

	ret = aes_start_crypt_processing(aes);
	if (ret) {
		printk("aes encrypt/decrypt error!\n");
		return -1;
	}
	return 0;

}

#ifdef DEBUG_AES_SG
static void aes_sg_debug(struct aes_operation *aes, int mapped_src, int mapped_dst)
{
	int i;
	int cnt = mapped_src;
	struct aes_pdma_para *tbl = NULL;
	tbl = (struct aes_pdma_para *)aes->pdma_data_src_v;
	printk("PDMA SRC table: virt=%p phys=0x%08llx entries=%d\n",
			aes->pdma_data_src_v, (unsigned long long)aes->pdma_data_src_p, cnt);
	for (i = 0; i < cnt; i++)
		printk("  [%2d] data_addr=0x%016llx  len=%-6u\n",
				i,
				(unsigned long long)tbl[i].data_addr,
				tbl[i].len);

	printk("src_addr_v=%p src_addr_p=0x%08x\n", aes->src_addr_v, aes->src_addr_p);
	{
		int words = cnt*2;//(sizeof(pdma_src)/sizeof(pdma_src[0])) * 2;
		volatile unsigned int *p = (volatile unsigned int *)aes->pdma_data_src_v;
		printk("PDMA SRC descriptor raw (words=%d):\n", words);
		for (i = 0; i < words; i++)
			printk("  [%3d] %p: %08x\n", i,
					(void *)((char *)aes->pdma_data_src_v + i * 4),
					(unsigned int)p[i]);
	}


	cnt = mapped_dst;
	tbl = (struct aes_pdma_para *)aes->pdma_data_dst_v;
	printk("PDMA DST table: virt=%p phys=0x%08llx entries=%d\n",
			aes->pdma_data_dst_v, (unsigned long long)aes->pdma_data_dst_p, cnt);
	for (i = 0; i < cnt; i++)
		printk("  [%2d] data_addr=0x%016llx  len=%-6u\n",
				i,
				(unsigned long long)tbl[i].data_addr,
				tbl[i].len);

	printk("dst_addr_v=%p dst_addr_p=0x%08x\n", aes->dst_addr_v, aes->dst_addr_p);
	{
		int words = cnt*2;//(sizeof(pdma_dst)/sizeof(pdma_dst[0])) * 2;
		volatile unsigned int *p = (volatile unsigned int *)aes->pdma_data_dst_v;
		printk("PDMA DST descriptor raw (words=%d):\n", words);
		for (i = 0; i < words; i++)
			printk("  [%3d] %p: %08x\n", i,
					(void *)((char *)aes->pdma_data_dst_v + i * 4),
					(unsigned int)p[i]);
	}

	debug_regs(aes);
	return ;
}
#endif

#if defined(CONFIG_SOC_PRJ008)
static int aes_process_user_sg(struct aes_operation *aes, unsigned long src_user,
                             unsigned long dst_user, unsigned int length)
{
#ifdef DEBUG_AES_SG
	void *p;
	int j = 0;
#endif
	int ret = 0;
	unsigned long src_start = src_user & PAGE_MASK;
	unsigned long src_end = (src_user + length + PAGE_SIZE - 1) & PAGE_MASK;
	unsigned long src_npages = (src_end - src_start) >> PAGE_SHIFT;
	unsigned long dst_start = dst_user & PAGE_MASK;
	unsigned long dst_end = (dst_user + length + PAGE_SIZE - 1) & PAGE_MASK;
	unsigned long dst_npages = (dst_end - dst_start) >> PAGE_SHIFT;

	struct page **src_pages = NULL;
	struct page **dst_pages = NULL;
	struct scatterlist *src_sg = NULL;
	struct scatterlist *dst_sg = NULL;
	int i, src_nr, dst_nr;
	int mapped_src, mapped_dst;
	unsigned int src_offset = src_user & ~PAGE_MASK;
	unsigned int dst_offset = dst_user & ~PAGE_MASK;
	unsigned int remaining = length;

	// 总长度必须16对齐（AES块大小）
	if (length == 0 || (length % 16) != 0){
		printk("aes invalid length %u (must be 16-aligned)\n", length);
		return -EINVAL;
	}

#ifdef DEBUG_AES_SG
	printk("aes_process_user_sg: src_user=0x%lx, dst_user=0x%lx, length=%u\n",
		src_user, dst_user, length);
	printk("src_npages=%lu, dst_npages=%lu, src_offset=%u, dst_offset=%u\n",
		src_npages, dst_npages, src_offset, dst_offset);
#endif

	src_pages = kmalloc_array(src_npages, sizeof(struct page *), GFP_KERNEL);
	dst_pages = kmalloc_array(dst_npages, sizeof(struct page *), GFP_KERNEL);
	if (!src_pages || !dst_pages) {
		ret = -ENOMEM;
		goto out_free_pages;
	}

#if defined (CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_3_10)
	down_read(&current->mm->mmap_sem);
	src_nr = get_user_pages(current, current->mm, src_user, src_npages,
						1 /* 可写访问 */, 0, src_pages, NULL);
	up_read(&current->mm->mmap_sem);
	if (src_nr < src_npages) {
		printk("aes: get src pages failed (got %d/%ld)\n", src_nr, src_npages);
		ret = -EFAULT;
		goto out_unpin_src;
	}

	down_read(&current->mm->mmap_sem);
	dst_nr = get_user_pages(current, current->mm, dst_user, dst_npages,
						1 /* 可写访问 */, 0, dst_pages, NULL);
	up_read(&current->mm->mmap_sem);
	if (dst_nr < dst_npages) {
		printk("aes: get dst pages failed (got %d/%ld)\n", dst_nr, dst_npages);
		ret = -EFAULT;
		goto out_unpin_dst;
	}
#else
	//kernel 5.x
	mmap_read_lock(current->mm);
	src_nr = pin_user_pages(src_user, src_npages, FOLL_PIN | FOLL_WRITE, src_pages, NULL);
	mmap_read_unlock(current->mm);
	if (src_nr != src_npages) {
		printk("aes: get src pages failed (got %d/%ld)\n", src_nr, src_npages);
		ret = -EFAULT;
		goto out_unpin_src;
	}

	mmap_read_lock(current->mm);
	dst_nr = pin_user_pages(dst_user, dst_npages, FOLL_PIN | FOLL_WRITE, dst_pages, NULL);
	mmap_read_unlock(current->mm);
	if (dst_nr != dst_npages) {
		printk("aes: get dst pages failed (got %d/%ld)\n", dst_nr, dst_npages);
		ret = -EFAULT;
		goto out_unpin_dst;
	}
#endif

	if(src_nr != src_npages || dst_nr != dst_npages) {
		printk("aes: insufficient pages (src %d/%ld, dst %d/%ld)\n",
			src_nr, src_npages, dst_nr, dst_npages);
		ret = -EFAULT;
		goto out_unpin_dst;
	}

	src_sg = kmalloc_array(src_npages, sizeof(struct scatterlist), GFP_KERNEL);
	dst_sg = kmalloc_array(dst_npages, sizeof(struct scatterlist), GFP_KERNEL);
	if (!src_sg || !dst_sg) {
		ret = -ENOMEM;
		goto out_free_sg;
	}

	sg_init_table(src_sg, src_npages);
	sg_init_table(dst_sg, dst_npages);

	/* 填充源SG表*/
	remaining = length;
	for (i = 0; i < src_npages; i++) {
		unsigned int page_avail = PAGE_SIZE;
		unsigned int raw_len;
		unsigned int off = (i == 0) ? src_offset : 0;

		if (i == 0)
			page_avail = PAGE_SIZE - src_offset;

		/* 当前页实际可用长度（不超过 remaining） */
		raw_len = min((unsigned int)page_avail, remaining);

		if (i != src_npages - 1) {
			/* 向下对齐 4 字节,该长度为硬件分块对齐要求最低单位 */
			raw_len &= ~0x3;
			if (raw_len == 0) {
				sg_set_page(&src_sg[i], src_pages[i], 0, off);
				printk("src_sg[%d] became 0 after align (off=%u)\n", i, off);
				continue;
			}
		} else {
			/* 最后一页：保证把剩余全部取走（remaining 必然为16的倍数） */
			raw_len = remaining;
		}

		sg_set_page(&src_sg[i], src_pages[i], raw_len, off);
		remaining -= raw_len;
#ifdef DEBUG_AES_SG
		printk("src_sg[%d]: srcaddr:%p  len=%u, offset=%u, offsetaddr:%p\n",
			i, page_address(sg_page(&src_sg[i])), sg_dma_len(&src_sg[i]),
			off, (void *)(page_address(sg_page(&src_sg[i])) + off));
#endif
	}

	remaining = length;
	for (i = 0; i < dst_npages; i++) {
		unsigned int page_avail = PAGE_SIZE;
		unsigned int raw_len;
		unsigned int off = (i == 0) ? dst_offset : 0;

		if (i == 0)
			page_avail = PAGE_SIZE - dst_offset;

		raw_len = min((unsigned int)page_avail, remaining);

		if (i != dst_npages - 1) {
			raw_len &= ~0x3;
			if (raw_len == 0) {
				sg_set_page(&dst_sg[i], dst_pages[i], 0, off);
				printk("dst_sg[%d] became 0 after align (off=%u)\n", i, off);
				continue;
			}
		} else {
			raw_len = remaining;
		}

		sg_set_page(&dst_sg[i], dst_pages[i], raw_len, off);
		remaining -= raw_len;

#ifdef DEBUG_AES_SG
		printk("dst_sg[%d]: dstaddr:%p len=%u, offset=%u, offsetaddr:%p\n",
			i, page_address(sg_page(&dst_sg[i])), sg_dma_len(&dst_sg[i]),
			off, (void *)(page_address(sg_page(&dst_sg[i])) + off));
#endif
	}

	/* 映射SG表到DMA地址 */
	mapped_src = dma_map_sg(aes->dev, src_sg, src_npages, DMA_TO_DEVICE);
	mapped_dst = dma_map_sg(aes->dev, dst_sg, dst_npages, DMA_FROM_DEVICE);
	if (!mapped_src || !mapped_dst) {
		dev_err(aes->dev, "DMA map failed: src=%d, dst=%d\n", mapped_src, mapped_dst);
		ret = -EIO;
		goto out_unmap_sg;
	}

	// 同步源缓冲区（CPU缓存 -> 内存）
	for (i = 0; i < mapped_src; i++) {
		dma_sync_single_for_device(aes->dev, sg_dma_address(&src_sg[i]),
									sg_dma_len(&src_sg[i]), DMA_TO_DEVICE);
	}
	aes->para.status = JZ_AES_STATUS_WORKING;

	aes_reg_write(aes, AES_ASBNUM, length / 16);
	if(mapped_src > DESCRIPT_NUM || mapped_dst > DESCRIPT_NUM){
		printk("ERRRRRRR: mapped_src %d,mapped_dst %d < %d may be invalid!\n",
				mapped_src,mapped_dst,DESCRIPT_NUM);
		return -EINVAL;
	}

	for(i = 0; i < mapped_src; i++) {
		g_pdma_src[i].data_addr = sg_dma_address(&src_sg[i]);
		g_pdma_src[i].len = sg_dma_len(&src_sg[i])/4;
	}

	for(i = 0; i < mapped_dst; i++) {
		g_pdma_dst[i].data_addr = sg_dma_address(&dst_sg[i]);
		g_pdma_dst[i].len = sg_dma_len(&dst_sg[i])/4;
#ifdef DEBUG_AES_SG
		printk("g_pdma_dst[%d]: phyaddr=0x%08lx,  worlds=%u\n", i,
			(unsigned long)g_pdma_dst[i].data_addr, g_pdma_dst[i].len);
#endif
	}

#ifdef DEBUG_AES_SG
	printk("(src_npages mapped_src)=(%lu,%d), (dst_npages,mapped_dst=(%lu,%d)\n", 
		src_npages,mapped_src,dst_npages, mapped_dst);
	for(i = 0 ;i < src_npages; i++ ) {
		p = kmap(sg_page(&src_sg[i]));
		printk("src:phyaddr=0x%08lx viraddr=%p,offset:%d,data:\n",
			(unsigned long)sg_dma_address(&src_sg[i]),p, src_sg[i].offset);
		// for(j = 0; j < sg_dma_len(&src_sg[i]); j++){
		// 	printk("%02x ",*((unsigned char *)(p + src_sg[i].offset + j)));
		// }
		kunmap(sg_page(&src_sg[i]));

		printk("g_pdma_src[%d]: phyaddr=0x%08lx, worlds=%u\n", i,
			(unsigned long)g_pdma_src[i].data_addr,g_pdma_src[i].len);
	}
#endif

	memcpy(aes->pdma_data_src_v, g_pdma_src, mapped_src * sizeof(g_pdma_src[0]));
	memcpy(aes->pdma_data_dst_v, g_pdma_dst, mapped_dst * sizeof(g_pdma_dst[0]));
#ifdef DEBUG_AES_SG
	aes_sg_debug(aes,mapped_src,mapped_dst);
#endif

	dma_sync_single_for_device(aes->dev, (dma_addr_t)aes->pdma_data_src_p,
			mapped_src * sizeof(g_pdma_src[0]), DMA_TO_DEVICE);
	dma_sync_single_for_device(aes->dev, (dma_addr_t)aes->pdma_data_dst_p,
			mapped_dst * sizeof(g_pdma_dst[0]), DMA_TO_DEVICE);

#ifdef DEBUG_AES_SG_PDMA
	DMADMAC(PDMA_P) = 0;//dis DMA
	pdma_config(aes, aes->pdma_data_src_p, DMA_AES_SRC_ADDR, mapped_src*sizeof(g_pdma_src[0]),aes->chan_tx->chan_id, 0x20);
	pdma_config(aes, aes->pdma_data_dst_p, DMA_AES_DST_ADDR, mapped_dst*sizeof(g_pdma_dst[0]),aes->chan_rx->chan_id, 0x21);
#else
	aes_dma_config_tx(aes,aes->pdma_data_src_p,
		DMA_AES_SRC_ADDR,
		mapped_src*sizeof(g_pdma_src[0]));

	aes_dma_config_rx(aes,aes->pdma_data_dst_p,
		DMA_AES_DST_ADDR,
		mapped_dst*sizeof(g_pdma_dst[0]));
#endif

#ifdef DEBUG_AES_SG
	debug_regs(aes);
#endif
	/* start AES DMA */
#ifdef DEBUG_TIME_CONSUME
	do_gettimeofday(&tvime1);
#endif
	aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | (1<<9));

#ifndef	DEBUG_AES_SG_PDMA
	if(!wait_for_completion_interruptible_timeout(&aes->hwdma_tx_done,msecs_to_jiffies(5000))){
		printk("wait hwdma tx aes timeout\n");
		goto out_unmap_sg;
	}

	if(!wait_for_completion_interruptible_timeout(&aes->hwdma_rx_done,msecs_to_jiffies(5000))){
		printk("wait hwdma rx aes timeout\n");
		goto out_unmap_sg;
	}
#endif

	if(!wait_for_completion_interruptible_timeout(&aes->dma_comp, msecs_to_jiffies(5000))){
		printk("%s[%d]: timeout!\n",__func__,__LINE__);
		ret = -ETIMEDOUT;
		goto out_unmap_sg;
	}
#ifdef DEBUG_TIME_CONSUME
	//统计相应的数据量时间
	long onetimes = 0;
	static unsigned long totaltime = 0;
	onetimes = (tvime2.tv_sec - tvime1.tv_sec) * 1000000 + (tvime2.tv_usec - tvime1.tv_usec);
	totaltime += onetimes;
	// printk("aes crypt %d KB data time is %ld us\n", tmp_len/1024, onetimes);
	unsigned long dealsize =length; //本次处理数据量
	if(aes->para.enworkmode==IN_UNF_CIPHER_WORK_MODE_ENC_CBC){
		log_data_enc(dealsize,dealsize, totaltime, "aes_hw_cbc_enc");
		totaltime = 0;
	} else if(aes->para.enworkmode==IN_UNF_CIPHER_WORK_MODE_DEC_CBC){
		log_data_dec(dealsize,dealsize, totaltime, "aes_hw_cbc_dec");
		totaltime = 0;
	}
#endif
	for (i = 0; i < dst_npages; i++) {
		dma_sync_single_for_cpu(aes->dev, sg_dma_address(&dst_sg[i]),
								sg_dma_len(&dst_sg[i]), DMA_FROM_DEVICE);
	}
#ifdef DEBUG_AES_SG
	for(i = 0 ;i < dst_npages; i++) {
		p = kmap(sg_page(&dst_sg[i]));
		printk("dst:phyaddr=0x%08lx viraddr=%p,offset:%d,data:\n",
			(unsigned long)sg_dma_address(&dst_sg[i]),p, dst_sg[i].offset);
		for(j = 0; j < sg_dma_len(&dst_sg[i]); j++){
			printk("%02x ",*((unsigned char *)(p + dst_sg[i].offset + j)));
		}
		kunmap(sg_page(&dst_sg[i]));
	}
#endif
	aes->para.status = JZ_AES_STATUS_DONE;

out_unmap_sg:
	if (mapped_src)
		dma_unmap_sg(aes->dev, src_sg, mapped_src, DMA_TO_DEVICE);
	if (mapped_dst)
		dma_unmap_sg(aes->dev, dst_sg, mapped_dst, DMA_FROM_DEVICE);
out_free_sg:
	kfree(src_sg);
	kfree(dst_sg);
out_unpin_dst:
#if defined (CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_3_10)
	for (i = 0; i < dst_nr; i++)
		put_page(dst_pages[i]);
#else
	unpin_user_pages(dst_pages, dst_nr);
#endif
out_unpin_src:
#if defined (CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_3_10)
	for (i = 0; i < src_nr; i++)
		put_page(src_pages[i]);
#else
	unpin_user_pages(src_pages, src_nr);
#endif
out_free_pages:
	kfree(src_pages);
	kfree(dst_pages);
	return ret;
}
#endif

#if defined(CONFIG_SOC_PRJ008) && defined(TEST_AES_EFUSE)
static int aes_start_crypt_processing_efuse(struct aes_operation *aes)
{
	struct aes_para *para = &aes->para;
	int i = 0;
	int ret = 0;
	int offset = 0, len = 0;
	int debug = 0;
	int fusion = 0;
	int efuse_key_mood = 0;
	int aeskey_len = 0;
	unsigned int userkey_efuse[8];

	aeskey_len = para->aes_key_len; //0:128bit 2:256bit
	efuse_key_mood = para->efuse_key_mood; /*if aeskey_len = 0
						0:user_key[127:0]
						1:user_key[255:128]
						2:user_key[127:0]|user_key[255:128]
						*/
	fusion = para->fusion; //0:no fusion 1:chipid fusion

	userkey_efuse[0] = *(volatile unsigned int *)0xb3540250;
	userkey_efuse[1] = *(volatile unsigned int *)0xb3540254;
	userkey_efuse[2] = *(volatile unsigned int *)0xb3540258;
	userkey_efuse[3] = *(volatile unsigned int *)0xb354025c;

	printk("-----user_key low128-------\n");
	for(i = 0; i < 4 ; i++) {
		printk("%08x", userkey_efuse[i]);
	}
	printk("\n");

	para->donelen = 0;
	para->status = JZ_AES_STATUS_PREPARE;
	if(para->datalen % 16){
		printk("The size of data should be 16bytes aligned!\n");
		return -EINVAL;
	}

	if(para->enworkmode >= IN_UNF_CIPHER_WORK_MODE_OTHER){
		printk("The enworkmode is invalid!\n");
		return -EINVAL;
	}

	para->status = JZ_AES_STATUS_WORKING;
	while(offset < para->datalen){
		len = para->datalen - offset > aes->dma_datalen ? aes->dma_datalen : para->datalen - offset;
		if(copy_from_user(aes->src_addr_v, (void __user *)(para->src + offset), len)){
			ret = -EINVAL;
			break;
		}

		printk("-----aes src-------\n");
		for(i = 0; i < 16; i++) {
			printk("%02x", aes->src_addr_v[i]);
		}
		printk("\n");

        dma_sync_single_for_device(NULL, (dma_addr_t)aes->dst_addr_p, len, DMA_FROM_DEVICE);
		dma_sync_single_for_device(NULL, (dma_addr_t)aes->src_addr_p, len, DMA_TO_DEVICE);

		/* Clear AES done and KEY done status */
		aes_reg_write(aes, AES_ASSR, aes_reg_read(aes, AES_ASSR));

		/* enable AES dma done and KEY done interrupt */
		aes_reg_write(aes, AES_ASINTM, 0x1);

		/* Clear the IV and KEYS; enable AES module */
		aes_reg_write(aes, AES_ASCR, 3 << 28 | 1 << 10 | 1 << 0) ;

		switch(para->enworkmode)
		{
			case IN_UNF_CIPHER_WORK_MODE_ENC_ECB:
				/* Encrypts, ECB mode, key length */
				aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<0 | aeskey_len << 6 | 1 << 19 | efuse_key_mood << 16 | fusion << 18);
				break;
			case IN_UNF_CIPHER_WORK_MODE_DEC_ECB:
				/* Decrypts, ECB mode, key length */
				aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<0 | 1<<4 | aeskey_len << 6 | 1 << 19 | efuse_key_mood << 16 | fusion << 18);
				break;
			case IN_UNF_CIPHER_WORK_MODE_ENC_CBC:
				/* Encrypts, CBC mode, key length*/
				aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<0 | 1<<5 | aeskey_len << 6 | 1 << 19 | efuse_key_mood << 16 | fusion << 18);
				break;
			case IN_UNF_CIPHER_WORK_MODE_DEC_CBC:
				/* Decrypts, CBC mode, key length */
				aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<0 | 1<<5 | 1<<4 | aeskey_len << 6 | 1 << 19 | efuse_key_mood << 16 | fusion << 18);
				break;
			default :
				printk("error AES modeo!!!'\n");
				return -1;
		}
		printk("aes ctrl:%08x\n", aes_reg_read(aes, AES_ASCR));
		if (para->enworkmode == IN_UNF_CIPHER_WORK_MODE_ENC_CBC || para->enworkmode == IN_UNF_CIPHER_WORK_MODE_DEC_CBC) {
			/* write IV */
			for(i = 0; i < 4; i++)
				aes_reg_write(aes, AES_ASIV, para->aesiv[i]);

			/* write IV init */
			aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<1);
		}

		/* little endian */
		//aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<13);

		/* write KEYS start */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<2);

		/* aes data input */
		for (i=0; i<len/4; i++) {
			aes_debug(" %08x",(((int*)(aes->src_addr_v))[i]));
			aes_reg_write(aes, AES_ASDI, ((int*)(aes->src_addr_v))[i]);
		}
		aes_debug("\n");

		/* start AES */
		aes_reg_write(aes, AES_ASCR, aes_reg_read(aes, AES_ASCR) | 1<<3);

		/* wait for AES done */
		i = 1000;
		debug = 0;
		while(i--){
			if(!wait_for_completion_interruptible(&aes->cpu_comp)){
				break;
			}
		}
		if(i <= 0){
			printk("%s[%d]: timeout!\n",__func__,__LINE__);
			break;
		}
		debug_regs(aes);
		aes_debug("aes efuse output\n");
		for(i = 0; i < para->datalen/4; i++) {
			((int *)(aes->dst_addr_v))[i] = aes_reg_read(aes, AES_ASDO);
			aes_debug("%08x", ((int *)(aes->dst_addr_v))[i]);
		}
		aes_debug("\n");

		if (copy_to_user((void __user *)(para->dst + offset), aes->dst_addr_v, len)) {
			ret = -EFAULT;
			break;
		}

		offset += len;
		para->donelen = offset;
	}

	para->status = JZ_AES_STATUS_DONE;
	return 0;
}
#endif

#if defined(CONFIG_SOC_PRJ008) && defined(TEST_AES_PDMA)
#ifdef DEBUG
static void dump_pdma_registers(struct aes_operation *aes, int chn)
{
	uintptr_t base = (uintptr_t)aes->pdma_iomem + 0x20 * chn;

	printk("-----------------pdma chn %d reg------------------\n", chn);
	printk("%08x:   %08x\n", (unsigned int)(base + 0x00), readl(aes->pdma_iomem + 0x00 + 0x20*chn));
	printk("%08x:   %08x\n", (unsigned int)(base + 0x04), readl(aes->pdma_iomem + 0x04 + 0x20*chn));
	printk("%08x:   %08x\n", (unsigned int)(base + 0x08), readl(aes->pdma_iomem + 0x08 + 0x20*chn));
	printk("%08x:   %08x\n", (unsigned int)(base + 0x0c), readl(aes->pdma_iomem + 0x0c + 0x20*chn));
	printk("%08x:   %08x\n", (unsigned int)(base + 0x10), readl(aes->pdma_iomem + 0x10 + 0x20*chn));
	printk("%08x:   %08x\n", (unsigned int)(base + 0x14), readl(aes->pdma_iomem + 0x14 + 0x20*chn));
	printk("%08x:   %08x\n", (unsigned int)(base + 0x18), readl(aes->pdma_iomem + 0x18 + 0x20*chn));
	printk("%08x:   %08x\n", (unsigned int)(base + 0x1c), readl(aes->pdma_iomem + 0x1c + 0x20*chn));

	/* 高位偏移寄存器也按相同格式打印绝对地址和值 */
	printk("%08x:   %08x\n", (unsigned int)((uintptr_t)aes->pdma_iomem + 0x1000), readl(aes->pdma_iomem + 0x1000));
	printk("%08x:   %08x\n", (unsigned int)((uintptr_t)aes->pdma_iomem + 0x1004), readl(aes->pdma_iomem + 0x1004));
	printk("%08x:   %08x\n", (unsigned int)((uintptr_t)aes->pdma_iomem + 0x1008), readl(aes->pdma_iomem + 0x1008));
}
#endif

#ifdef DEBUG_AES_SG
static int pdma_config(struct aes_operation *aes, dma_addr_t src, dma_addr_t dst ,int len, int chn, int type)
{
	DMADSA(chn,PDMA_P) = src;//aes data src physical address
	DMADRT(chn,PDMA_P) = type;
	DMADTA(chn,PDMA_P) = dst;//aes src physical address
	DMADTC(chn,PDMA_P) = len;
#ifdef DEBUG
	printk("Pdma transfer count:%d\n", len);
#endif
	DMADCM(chn,PDMA_P) =
		(1<<23)| //SAI
		(0<<22)| //DAI
		(5<<16)| //RDIL
		(0<<14)| //SP
		(0<<12)| //DP
		(7<<8 )| //TSZ
		(0<<2 )| //STDE
		(0<<1 )| //TIE
		(0<<0 )  //LINK
		;

	DMADCS(chn,PDMA_P) =
		(1<<31)| //NDES
		(0<<30)| //DES4
		(0<<4 )| //AR
		(0<<3 )| //TT
		(0<<2 )| //HTL
		(1<<0 )  //CTE
		;
#ifdef DEBUG
	dump_pdma_registers(aes, chn);
#endif
	DMADMAC(PDMA_P) = 1;		//Enable DMA

	return 0;
}
#endif
#endif

#if defined(CONFIG_SOC_PRJ008)
static int aes_start_crypt_processing_sg(struct file *file,struct aes_operation *aes)
{
	struct aes_para *para = &aes->para;
	int ret = 0;

	para->donelen = 0;
	para->status = JZ_AES_STATUS_PREPARE;

	if((unsigned long)aes->para.src & 0x3 ||
		(unsigned long)aes->para.dst & 0x3){
		printk("WARNING!!!, The src/dst address should be 4bytes aligned!\n");
		para->status = JZ_AES_STATUS_DONE;
		return -EINVAL;
	}

	ret = jz_aes_init(file, aes);
	if(ret){
		printk("aes init error!\n");
		return ret;
	}

	if (aes->para.src && aes->para.dst) {
		ret = aes_process_user_sg(aes, (unsigned long)aes->para.src,
								(unsigned long)aes->para.dst,
								aes->para.datalen);
		if (ret) {
			para->status = JZ_AES_STATUS_DONE;
			printk("aes sg encrypt/decrypt error!\n");
			return -1;
		}
	}

	para->status = JZ_AES_STATUS_DONE;
	return 0;
}
#endif
static long aes_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = file->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);
	void __user *argp  = (void __user *)arg;
	int ret = 0;

	switch(cmd) {
	case IOCTL_AES_GET_PARA:
		if (copy_to_user(argp, &aes->para, sizeof(struct aes_para))) {
			printk("aes set para copy_to_user error!!!\n");
			return -EFAULT;
		}
		break;
	case IOCTL_AES_PROCESSING:
		if(aes->para.status != JZ_AES_STATUS_DONE){
			printk("aes is busy now!!!\n");
			return -EBUSY;
		}
		if (copy_from_user(&aes->para, argp, sizeof(struct aes_para))) {
			printk("aes get para copy_from_user error!!!\n");
			return -EFAULT;
		}
		ret = aes_start_processing(file,aes);
		if (copy_to_user(argp, &aes->para, sizeof(struct aes_para))) {
			printk("aes set para copy_to_user error!!!\n");
			return -EFAULT;
		}
		break;
	case IOCTL_AES_INIT:
		if (copy_from_user(&aes->para, argp, sizeof(struct aes_para))) {
				printk("aes get para copy_from_user error!!!\n");
				return -EFAULT;
		}
		ret = jz_aes_init(file,aes);
		break;
#if defined(CONFIG_SOC_PRJ008)
	case IOCTL_AES_SG_PROCESSING:
		if(aes->para.status != JZ_AES_STATUS_DONE){
			printk("aes is busy now!!!\n");
			return -EBUSY;
		}
		if (copy_from_user(&aes->para, argp, sizeof(struct aes_para))) {
			printk("aes get para copy_from_user error!!!\n");
			return -EFAULT;
		}
		ret = aes_start_crypt_processing_sg(file,aes);
		if (ret) {
			printk("aes encrypt/decrypt error!\n");
			return -1;
		}
		if (copy_to_user(argp, &aes->para, sizeof(struct aes_para))) {
			printk("aes set para copy_to_user error!!!\n");
			return -EFAULT;
		}
		break;
#endif

#if defined(CONFIG_SOC_PRJ008) && defined(TEST_AES_EFUSE)
	case IOCTL_AES_EFUSE:
		if(aes->para.status != JZ_AES_STATUS_DONE){
		 printk("aes is busy now!!!\n");
		 return -EBUSY;
		}
		if (copy_from_user(&aes->para, argp, sizeof(struct aes_para))) {
		 printk("aes get para copy_from_user error!!!\n");
		 return -EFAULT;
		}
		printk("para->datalen = %d\n", aes->para.datalen);
		ret = aes_start_crypt_processing_efuse(aes);
		if (ret) {
		 printk("aes encrypt error!\n");
		 return -1;
		}
		if (copy_to_user(argp, &aes->para, sizeof(struct aes_para))) {
		 printk("aes set para copy_to_user error!!!\n");
		 return -EFAULT;
		}
		break;
#endif
	default:
		printk("aes not support this cmd now,please check!!!\n");
		break;
	}
	return 0;
}

static int aes_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	// unsigned long pfn;
	unsigned long total_pages;
	struct miscdevice *dev = filp->private_data;
	struct aes_operation *aes = miscdev_to_aesops(dev);
	unsigned long size = vma->vm_end - vma->vm_start;

	unsigned long off = vma->vm_pgoff;
	unsigned long user_count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long per_buf_pages = PAGE_ALIGN(JZ_AES_DMA_DATALEN) >> PAGE_SHIFT;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	// 总页数 = src页数 + dst页数
	total_pages = 2 * per_buf_pages;

	if (off >= total_pages || user_count > total_pages - off) {
		return -EINVAL;
	}

#if 0
	if (off < per_buf_pages) {
		pfn = page_to_pfn(virt_to_page(aes->src_addr_v));
		ret = remap_pfn_range(vma, vma->vm_start, pfn + off,
								user_count << PAGE_SHIFT, vma->vm_page_prot);
	}
	else
	{
		off -= per_buf_pages;
		pfn = page_to_pfn(virt_to_page(aes->dst_addr_v));
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		ret = remap_pfn_range(vma, vma->vm_start, pfn + off,
								user_count << PAGE_SHIFT, vma->vm_page_prot);
	}
#else
	ret = dma_mmap_coherent(aes->dev, vma, aes->src_addr_v, aes->src_addr_p, JZ_AES_DMA_DATALEN);
#endif
	return ret;
}

const struct file_operations aes_fops = {
	.owner = THIS_MODULE,
	.read = aes_read,
	.write = aes_write,
	.open = aes_open,
	.unlocked_ioctl = aes_ioctl,
	.release = aes_release,
	.mmap = aes_mmap,
};

static irqreturn_t aes_ope_irq_handler(int irq, void *data)
{
	struct aes_operation *aes_ope = data;
	unsigned int status = aes_reg_read(aes_ope, AES_ASSR);
	unsigned int mask = aes_reg_read(aes_ope, AES_ASINTM);
	unsigned int pending = status & mask;

	/* clear interrupt */
	//printk("status = %x, mask = %x\n", status, mask);
	aes_reg_write(aes_ope, AES_ASSR, status);
	/*if(aes_ope->para.status != JZ_AES_STATUS_WORKING){
		return IRQ_HANDLED;
	}*/
	if((pending & AES_ASSR_DMAD) && (aes_ope->para.status == JZ_AES_STATUS_WORKING)){
#ifdef DEBUG_TIME_CONSUME
		do_gettimeofday(&tvime2);
#endif
		complete(&aes_ope->dma_comp);
	}
	if((pending & AES_ASSR_AESD) && (aes_ope->para.status == JZ_AES_STATUS_WORKING)){
		complete(&aes_ope->cpu_comp);
	}
#if defined(CONFIG_SOC_PRJ007)
	if((pending & AES_ASSR_KEYD) && (aes_ope->para.status == JZ_AES_STATUS_PREPARE)){
		complete(&aes_ope->key_comp);
	}
#endif
	return IRQ_HANDLED;
}

static int jz_aes_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct aes_operation *aes_ope = NULL;
	aes_ope = (struct aes_operation *)kzalloc(sizeof(struct aes_operation), GFP_KERNEL);
	if (!aes_ope) {
		dev_err(&pdev->dev, "alloc aes mem_region failed!\n");
		return -ENOMEM;
	}
	sprintf(aes_ope->name, "jz-aes");
	aes_debug("%s, aes name is : %s\n",__func__, aes_ope->name);

	aes_ope->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!aes_ope->res) {
		dev_err(&pdev->dev, "failed to get dev resources\n");
		ret = -EINVAL;
		goto failed_get_mem;
	}

	aes_ope->res = request_mem_region(aes_ope->res->start,
			aes_ope->res->end - aes_ope->res->start + 1,
			pdev->name);
	if (!aes_ope->res) {
		dev_err(&pdev->dev, "failed to request regs memory region");
		ret = -EINVAL;
		goto failed_req_region;
	}
	
	aes_ope->iomem = ioremap(aes_ope->res->start, resource_size(aes_ope->res));
	if (!aes_ope->iomem) {
		dev_err(&pdev->dev, "failed to remap regs memory region\n");
		ret = -EINVAL;
		goto failed_iomap;
	}
	aes_debug("%s, aes iomem is :0x%08x\n", __func__, (unsigned int)aes_ope->iomem);

#if defined(TEST_AES_PDMA) && defined(CONFIG_SOC_PRJ008)
     /*ioremap pdma*/
     aes_ope->pdma_iomem = ioremap(PDMA_BASE, PDMA_BASE_LEN);
     if(!aes_ope->pdma_iomem)
     {
         dev_err(&pdev->dev, "failed to remap pdma regs memory region\n");
         ret = -EINVAL;
         goto failed_pdma_iomap;
     }
     aes_debug("%s, pdma iomem is :0x%08x\n", __func__, (unsigned int)aes_ope->pdma_iomem);
#endif
#if defined(TEST_AES_EFUSE)
	 /*ioremap efuse*/
     aes_ope->efuse_iomem = ioremap(EFUSE_BASE, EFUSE_BASE_LEN);
     if(!aes_ope->efuse_iomem)
     {
         dev_err(&pdev->dev, "failed to remap efuse regs memory region\n");
         ret = -EINVAL;
         goto failed_efuse_iomap;
     }
     aes_debug("%s, efuse iomem is :0x%08x\n", __func__, (unsigned int)aes_ope->efuse_iomem);
#endif

	aes_ope->irq = platform_get_irq(pdev, 0);
	if(aes_ope->irq <= 0){
		dev_err(&pdev->dev, "get irq failed\n");
		ret = -EINVAL;
		goto failed_get_irq;
	}
	if (request_irq(aes_ope->irq, aes_ope_irq_handler, IRQF_SHARED, aes_ope->name, aes_ope)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto failed_req_irq;
	}
	aes_debug("%s, aes irq is : %d\n",__func__, aes_ope->irq);

	aes_ope->src_addr_v = dma_alloc_coherent(&pdev->dev,
							  JZ_AES_DMA_DATALEN,
							  &aes_ope->src_addr_p,
							  GFP_KERNEL);
	if (aes_ope->src_addr_v == NULL){
		printk("failed to alloc dma src buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_src;
	}

	aes_ope->dst_addr_v = dma_alloc_coherent(&pdev->dev,
							  JZ_AES_DMA_DATALEN,
							  &aes_ope->dst_addr_p,
							  GFP_KERNEL);

	if (aes_ope->dst_addr_v == NULL){
		printk("failed to alloc dma dst buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_dst;
	}
	aes_ope->dma_datalen = JZ_AES_DMA_DATALEN;

#if defined(CONFIG_SOC_PRJ008) && defined(TEST_AES_PDMA)
	ret = aes_dma_init(aes_ope);
	if(ret < 0){
		ret = -ENODEV;
		goto failed_alloc_pdata_dma_src;
	}
#if defined (CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_3_10)
	aes_ope->pdma_data_src_v = dma_alloc_noncoherent(&pdev->dev,
								sizeof(struct aes_pdma_para)*DESCRIPT_NUM,
								&aes_ope->pdma_data_src_p,
								GFP_KERNEL | GFP_DMA);
	if (aes_ope->pdma_data_src_v == NULL)
	{
		printk("failed to alloc dma src buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_pdata_dma_src;
	}

	aes_ope->pdma_data_dst_v = dma_alloc_noncoherent(&pdev->dev,
								sizeof(struct aes_pdma_para)*DESCRIPT_NUM,
								&aes_ope->pdma_data_dst_p,
								GFP_KERNEL | GFP_DMA);
	if (aes_ope->pdma_data_dst_v == NULL)
	{
		printk("failed to alloc dma dst buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_pdata_dma_dst;
	}
#else
	aes_ope->pdma_data_src_v = dma_alloc_noncoherent(&pdev->dev,
								sizeof(struct aes_pdma_para)*DESCRIPT_NUM,
								&aes_ope->pdma_data_src_p,
								DMA_TO_DEVICE,
								GFP_KERNEL);
	if (aes_ope->pdma_data_src_v == NULL)
	{
		printk("failed to alloc dma src buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_pdata_dma_src;
	}

	aes_ope->pdma_data_dst_v = dma_alloc_noncoherent(&pdev->dev,
								sizeof(struct aes_pdma_para)*DESCRIPT_NUM,
								&aes_ope->pdma_data_dst_p,
								DMA_FROM_DEVICE,
								GFP_KERNEL);
	if (aes_ope->pdma_data_dst_v == NULL)
	{
		printk("failed to alloc dma dst buffer\n");
		ret = -ENOMEM;
		goto failed_alloc_pdata_dma_dst;
	}
#endif

#endif
	/****************************************************************/
	aes_ope->aes_dev.minor = MISC_DYNAMIC_MINOR;
	aes_ope->aes_dev.fops = &aes_fops;
	aes_ope->aes_dev.name = "aes";
	aes_ope->dev = &pdev->dev;

	ret = misc_register(&aes_ope->aes_dev);
	if(ret) {
		dev_err(&pdev->dev,"request misc device failed!\n");
		ret = -EINVAL;
		goto failed_misc;
	}

#if defined(CONFIG_KERNEL_4_4_94) || defined (CONFIG_KERNEL_5_15)
	aes_ope->clk = devm_clk_get(&pdev->dev, "gate_aes");
	if (IS_ERR(aes_ope->clk)){
		dev_err(&pdev->dev, "aes clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
	clk_prepare_enable(aes_ope->clk);
#else
	aes_ope->clk = clk_get(aes_ope->dev,"gate_aes");
	if (IS_ERR(aes_ope->clk)){
		dev_err(&pdev->dev, "aes clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
	clk_enable(aes_ope->clk);
#endif

	platform_set_drvdata(pdev, aes_ope);

	mutex_init(&aes_ope->mutex);
	init_completion(&aes_ope->hwdma_tx_done);
	init_completion(&aes_ope->hwdma_rx_done);
	init_completion(&aes_ope->dma_comp);
	init_completion(&aes_ope->cpu_comp);
	init_completion(&aes_ope->key_comp);

	aes_ope->para.status = JZ_AES_STATUS_DONE;
	aes_debug("%s: probe() done\n", __func__);
	printk("@@@@ aes driver ok(version %s) @@@@@\n", AES_DRIVER_VERSION);

	return 0;

failed_clk:
	misc_deregister(&aes_ope->aes_dev);
failed_misc:
#if defined(CONFIG_SOC_PRJ008)  && defined(TEST_AES_PDMA)
#if defined (CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_3_10)
	dma_free_noncoherent(&pdev->dev, sizeof(struct aes_pdma_para)*DESCRIPT_NUM, aes_ope->pdma_data_dst_v, aes_ope->pdma_data_dst_p);
#else
	dma_free_noncoherent(&pdev->dev, sizeof(struct aes_pdma_para)*DESCRIPT_NUM,\
		 aes_ope->pdma_data_dst_v, aes_ope->pdma_data_dst_p, DMA_TO_DEVICE);
#endif
failed_alloc_pdata_dma_dst:
#if defined (CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_3_10)
	dma_free_noncoherent(&pdev->dev, sizeof(struct aes_pdma_para)*DESCRIPT_NUM, aes_ope->pdma_data_src_v, aes_ope->pdma_data_src_p);
#else
	dma_free_noncoherent(&pdev->dev, sizeof(struct aes_pdma_para)*DESCRIPT_NUM,\
		 aes_ope->pdma_data_src_v, aes_ope->pdma_data_src_p, DMA_FROM_DEVICE);
#endif
failed_alloc_pdata_dma_src:
#endif
	dma_free_coherent(&pdev->dev, JZ_AES_DMA_DATALEN,
			aes_ope->dst_addr_v, aes_ope->dst_addr_p);
failed_alloc_dst:
	dma_free_coherent(&pdev->dev, JZ_AES_DMA_DATALEN,
							  aes_ope->src_addr_v, aes_ope->src_addr_p);
failed_alloc_src:
	free_irq(aes_ope->irq, aes_ope);
failed_get_irq:
failed_req_irq:
#if defined(CONFIG_SOC_PRJ008)
	iounmap(aes_ope->efuse_iomem);
#if defined(TEST_AES_EFUSE)
failed_efuse_iomap:
	iounmap(aes_ope->pdma_iomem);
#endif
#if defined(TEST_AES_PDMA) && defined(CONFIG_SOC_PRJ008)
failed_pdma_iomap:
#endif
#endif
	iounmap(aes_ope->iomem);
failed_iomap:
	release_mem_region(aes_ope->res->start, aes_ope->res->end - aes_ope->res->start + 1);
failed_req_region:
failed_get_mem:
	kfree(aes_ope);
	return ret;
}

static int jz_aes_remove(struct platform_device *pdev)
{
	struct aes_operation *aes_ope = platform_get_drvdata(pdev);

	misc_deregister(&aes_ope->aes_dev);
#if defined(CONFIG_KERNEL_4_4_94) || defined (CONFIG_KERNEL_5_15)
	clk_disable_unprepare(aes_ope->clk);
#else
	clk_disable(aes_ope->clk);
	clk_put(aes_ope->clk);
#endif
	dma_free_coherent(&pdev->dev, JZ_AES_DMA_DATALEN,
								aes_ope->src_addr_v, aes_ope->src_addr_p);
	dma_free_coherent(&pdev->dev, JZ_AES_DMA_DATALEN,
								aes_ope->dst_addr_v, aes_ope->dst_addr_p);
#if defined(CONFIG_SOC_PRJ008) && defined(TEST_AES_PDMA)
	aes_dma_exit(aes_ope);
#if defined (CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_3_10)
	dma_free_noncoherent(&pdev->dev, sizeof(struct aes_pdma_para)*DESCRIPT_NUM,
								aes_ope->pdma_data_src_v, aes_ope->pdma_data_src_p);
	dma_free_noncoherent(&pdev->dev, sizeof(struct aes_pdma_para)*DESCRIPT_NUM,
								aes_ope->pdma_data_dst_v, aes_ope->pdma_data_dst_p);
#else
	dma_free_noncoherent(&pdev->dev, sizeof(struct aes_pdma_para)*DESCRIPT_NUM,
								aes_ope->pdma_data_src_v, aes_ope->pdma_data_src_p, DMA_TO_DEVICE);
	dma_free_noncoherent(&pdev->dev, sizeof(struct aes_pdma_para)*DESCRIPT_NUM,
								aes_ope->pdma_data_dst_v, aes_ope->pdma_data_dst_p, DMA_FROM_DEVICE);
#endif

	iounmap(aes_ope->pdma_iomem);
#endif
	free_irq(aes_ope->irq, aes_ope);
	iounmap(aes_ope->iomem);
	kfree(aes_ope);

	return 0;
}

static void  platform_aes_release(struct device *dev)
{
	return ;
}

static struct platform_driver jz_aes_driver = {
	.probe	= jz_aes_probe,
	.remove	= jz_aes_remove,
	.driver	= {
		.name	= "jz-aes",
		.owner	= THIS_MODULE,
	},
};

static struct resource jz_aes_resources[] = {
	[0] = {
		.start	= AES_IOBASE,
		.end	= AES_IOBASE + 0x28,
		.flags	= IORESOURCE_MEM,
	},

#if defined(CONFIG_KERNEL_4_4_94) || defined (CONFIG_KERNEL_5_15)
	[1] = {
		.start	= IRQ_AES + 8,
		.end	= IRQ_AES + 8,
		.flags	= IORESOURCE_IRQ,
	},
#else
	[1] = {
		.start	= IRQ_AES,
		.end	= IRQ_AES,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device jz_aes_device = {
	.name	= "jz-aes",
	.id = 0,
	.resource	= jz_aes_resources,
	.dev = {
		.release = platform_aes_release,
	},
	.num_resources	= ARRAY_SIZE(jz_aes_resources),
};


static int __init jz_aes_mod_init(void)
{
	int ret = 0;
	aes_debug("loading %s driver\n", "jz-aes");

	ret = platform_device_register(&jz_aes_device);
	if(ret){
		printk("Failed to insmod aes device!!!\n");
		return ret;
	}
	return	platform_driver_register(&jz_aes_driver);
}

static void __exit jz_aes_mod_exit(void)
{
	platform_driver_unregister(&jz_aes_driver);
	platform_device_unregister(&jz_aes_device);
}


module_init(jz_aes_mod_init);
module_exit(jz_aes_mod_exit);

MODULE_DESCRIPTION("Ingenic AES hw support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Elvis Wang");
