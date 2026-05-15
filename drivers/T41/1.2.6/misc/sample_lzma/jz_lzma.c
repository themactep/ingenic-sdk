#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/clk-provider.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include "jz_lzma.h"

#define LZMA_DRIVER_VERSION "H20240416a"

int power(int want)
{
    int result = 1, i = 0;
    for(i = 1; i<1000; i++){
        result *= 2;
        if(result >= want)
            break;
    }

    return i;
}

void jz_lzma_debug(struct jz_lzma *lzma, unsigned int index)
{
#ifdef DEBUG_LZMA
	printk("LZMA_FSM_DBG = 0x%x\n",     lzma_reg_read(lzma, index, LZMA_FSM_DBG));
	printk("LZMA_RADDR_CNT_DBG = %d\n", lzma_reg_read(lzma, index, LZMA_RADDR_CNT_DBG));
	printk("LZMA_RADDR_CFG_NUM = %d\n", lzma_reg_read(lzma, index, LZMA_RADDR_CFG_NUM));
	printk("LZMA_RADDR0_DBG = 0x%x\n",  lzma_reg_read(lzma, index, LZMA_RADDR0_DBG));
	printk("LZMA_RADDR1_DBG = 0x%x\n",  lzma_reg_read(lzma, index, LZMA_RADDR1_DBG));
	printk("=============================================\n");
	printk("LZMA_RDATA_CNT_DBG = %d\n", lzma_reg_read(lzma, index, LZMA_RDATA_CNT_DBG));
	printk("LZMA_RDATA_CFG_NUM = %d\n", lzma_reg_read(lzma, index, LZMA_RDATA_CFG_NUM));
	printk("LZMA_RDATAL0_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_RDATAL0_DBG));
	printk("LZMA_RDATAH0_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_RDATAH0_DBG));
	printk("LZMA_RDATAL1_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_RDATAL1_DBG));
	printk("LZMA_RDATAH1_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_RDATAH1_DBG));
	printk("=============================================\n");
	printk("LZMA_HDEC_DA_CNT = %d\n",     lzma_reg_read(lzma, index, LZMA_HDEC_DA_CNT));
	printk("LZMA_HDEC_DA_CFG_NUM = %d\n", lzma_reg_read(lzma, index, LZMA_HDEC_DA_CFG_NUM));
	printk("LZMA_HDEC_DA = 0x%x\n",       lzma_reg_read(lzma, index, LZMA_HDEC_DA));
	printk("=============================================\n");
	printk("LZMA_FSM_DA_CNT = %d\n",     lzma_reg_read(lzma, index, LZMA_FSM_DA_CNT));
	printk("LZMA_FSM_DA_CFG_NUM = %d\n", lzma_reg_read(lzma, index, LZMA_FSM_DA_CFG_NUM));
	printk("LZMA_FSM_DA = 0x%x\n",       lzma_reg_read(lzma, index, LZMA_FSM_DA));
	printk("=============================================\n");
	printk("LZMA_DIST_DA_CNT = %d\n",     lzma_reg_read(lzma, index, LZMA_DIST_DA_CNT));
	printk("LZMA_DIST_DA_CFG_NUM = %d\n", lzma_reg_read(lzma, index, LZMA_DIST_DA_CFG_NUM));
	printk("LZMA_DIST0_DA = 0x%x\n",      lzma_reg_read(lzma, index, LZMA_DIST0_DA));
	printk("LZMA_DIST1_DA = 0x%x\n",      lzma_reg_read(lzma, index, LZMA_DIST1_DA));
	printk("=============================================\n");
	printk("LZMA_LEN_DA_CNT = %d\n",     lzma_reg_read(lzma, index, LZMA_LEN_DA_CNT));
	printk("LZMA_LEN_DA_CFG_NUM = %d\n", lzma_reg_read(lzma, index, LZMA_LEN_DA_CFG_NUM));
	printk("LZMA_LEN_DA = 0x%x\n",       lzma_reg_read(lzma, index, LZMA_LEN_DA));
	printk("=============================================\n");
	printk("LZMA_LIT_DA_CNT = %d\n",     lzma_reg_read(lzma, index, LZMA_LIT_DA_CNT));
	printk("LZMA_LIT_DA_CFG_NUM = %d\n", lzma_reg_read(lzma, index, LZMA_LIT_DA_CFG_NUM));
	printk("LZMA_LIT_DA = 0x%x\n",       lzma_reg_read(lzma, index, LZMA_LIT_DA));
	printk("=============================================\n");
	printk("LZMA_MBC_CNT_DBG = %d\n",    lzma_reg_read(lzma, index, LZMA_MBC_CNT_DBG));
	printk("LZMA_MBC_DA_CFG_NUM = %d\n", lzma_reg_read(lzma, index, LZMA_MBC_DA_CFG_NUM));
	printk("LZMA_MBC_DAL0_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_MBC_DAL0_DBG));
	printk("LZMA_MBC_DAH0_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_MBC_DAH0_DBG));
	printk("LZMA_MBC_DAL1_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_MBC_DAL1_DBG));
	printk("LZMA_MBC_DAH1_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_MBC_DAH1_DBG));
	printk("=============================================\n");
	printk("LZMA_SBC_CNT_DBG = %d\n",         lzma_reg_read(lzma, index, LZMA_SBC_CNT_DBG));
	printk("LZMA_SBC_DA_CFG_NUM = %d\n",      lzma_reg_read(lzma, index, LZMA_SBC_DA_CFG_NUM));
	printk("LZMA_SBC_RP0_DBG = 0x%x\n",       lzma_reg_read(lzma, index, LZMA_SBC_RP0_DBG));
	printk("LZMA_SBC_RP1_DBG = 0x%x\n",       lzma_reg_read(lzma, index, LZMA_SBC_RP1_DBG));
	printk("LZMA_SBC_WP0_DBG = 0x%x\n",       lzma_reg_read(lzma, index, LZMA_SBC_WP0_DBG));
	printk("LZMA_SBC_WP1_DBG = 0x%x\n",       lzma_reg_read(lzma, index, LZMA_SBC_WP1_DBG));
	printk("LZMA_SBC_RDMA0_IDX_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_SBC_RDMA0_IDX_DBG));
	printk("LZMA_SBC_RDMA1_IDX_DBG = 0x%x\n", lzma_reg_read(lzma, index, LZMA_SBC_RDMA1_IDX_DBG));
	printk("=============================================\n");
	printk("LZMA_BS_DA_CNT = %d\n",     lzma_reg_read(lzma, index, LZMA_BS_DA_CNT));
	printk("LZMA_BS_DA_CFG_NUM = %d\n", lzma_reg_read(lzma, index, LZMA_BS_DA_CFG_NUM));
	printk("LZMA_BS_DA0 = 0x%x\n",      lzma_reg_read(lzma, index, LZMA_BS_DA0));
	printk("LZMA_BS_DA1 = 0x%x\n",      lzma_reg_read(lzma, index, LZMA_BS_DA1));
	printk("=============================================\n");
	printk("LZMA_CRC_RDATA = 0x%x\n",       lzma_reg_read(lzma, index, LZMA_CRC_RDATA));
	printk("LZMA_CRC_HDEC_DATA = 0x%x\n",   lzma_reg_read(lzma, index, LZMA_CRC_HDEC_DATA));
	printk("LZMA_CRC_FSM_DATA = 0x%x\n",    lzma_reg_read(lzma, index, LZMA_CRC_FSM_DATA));
	printk("LZMA_CRC_DIST_DATA = 0x%x\n",   lzma_reg_read(lzma, index, LZMA_CRC_DIST_DATA));
	printk("LZMA_CRC_LEN_DATA = 0x%x\n",    lzma_reg_read(lzma, index, LZMA_CRC_LEN_DATA));
	printk("LZMA_CRC_LIT_DATA = 0x%x\n",    lzma_reg_read(lzma, index, LZMA_CRC_LIT_DATA));
#endif

}

static int lzma_open(struct inode *inode, struct file *pfil){
	//应用层调用时，做准备工作，可被执行多次
	printk("----------- lzma_open .\n");

	return 0;
}

static int lzma_close(struct inode *inode, struct file *pfil){
	//应用层调用时，做善后工作，可被执行多次
	printk("----------- lzma_close .\n");
	return 0;
}

static ssize_t lzma_read(struct file * pfil, char __user *user, size_t size, loff_t *offset){
	//__user 指定形参是用户空间的
	printk("----------- lzma_read .\n");
	return 0;
}

static ssize_t lzma_write(struct file *pfil, const char __user *user, size_t size, loff_t *offset){
	printk("----------- lzma_write .\n");
	return 0;
}

int work_lzma0(struct jz_lzma *lzma, struct lzma_conf *conf)
{
	unsigned int new_value = 0;
	unsigned long file_fp;
	unsigned long file_fp_out;
	struct file *fp = NULL;
	struct file *fp1 = NULL;
	unsigned long file_in_addr = 0;
	mm_segment_t fs;
	loff_t pos = 0;

	file_fp = __get_free_pages(GFP_KERNEL, power((conf->src0_size+4095)/4096));   //10 == 1024 == 4M ,if page_size=4096bytes,11==(2^11)*4K
	if (file_fp <= 0)
	{
		printk("------ file_fp alloc failed.\n");
		return -1;
	}
	memset((void*)file_fp, 0x0, conf->src0_size);

	file_fp_out = __get_free_pages(GFP_KERNEL,  power((conf->dst0_size+4095)/4096));   //10 == 1024 == 4M ,if page_size=4096bytes,12==16M
	if (file_fp_out <= 0)
	{
		free_pages(file_fp,  power((conf->src0_size+4095)/4096));
		printk("------ file_fp_out alloc failed.\n");
		return -1;
	}

	memset((void*)file_fp_out, 0, conf->dst0_size);

	//file operations: read image data to buffer now
	fp = filp_open(conf->src0_name, O_RDWR, 0644);
	if (fp == NULL)
	{
		printk("file: uImage.lzma0 open failed...\n");
		goto OUT;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	dma_cache_sync(NULL, (void *)file_fp, conf->src0_size, DMA_BIDIRECTIONAL);
	printk("lzma0 vfs_read size=%d\n", vfs_read(fp, (unsigned char*)file_fp, conf->src0_size, &pos));
	dma_cache_sync(NULL, (void *)file_fp, conf->src0_size, DMA_BIDIRECTIONAL);
	filp_close(fp, NULL);
	set_fs(fs);

	//step1. start lzma decoder
	if(((lzma_reg_read(lzma, 0, LZMA_CTRL) >> 31) & 0x1) != 1){
		printk("lzma0-----Write LZMA_CTRL[31]\n");
		lzma_reg_write(lzma, 0, LZMA_CTRL, 0x1<<31);
	}

	//step2. reset lzma module
	lzma_reg_write(lzma, 0, LZMA_CTRL, 0x1<<1);
	while ((lzma_reg_read(lzma, 0, LZMA_CTRL) >> 1) & 0x1);

#ifdef DEBUG_LZMA
	lzma_reg_write(lzma, 0, LZMA_RADDR_CFG_NUM, 1<<31 | 3);
	lzma_reg_write(lzma, 0, LZMA_RDATA_CFG_NUM, 40);
	lzma_reg_write(lzma, 0, LZMA_HDEC_DA_CFG_NUM, 0);
	lzma_reg_write(lzma, 0, LZMA_FSM_DA_CFG_NUM, 223);
	lzma_reg_write(lzma, 0, LZMA_DIST_DA_CFG_NUM, 47);
	lzma_reg_write(lzma, 0, LZMA_LEN_DA_CFG_NUM, 75);
	lzma_reg_write(lzma, 0, LZMA_LIT_DA_CFG_NUM, 107);
	lzma_reg_write(lzma, 0, LZMA_MBC_DA_CFG_NUM, 48);
	lzma_reg_write(lzma, 0, LZMA_SBC_DA_CFG_NUM, 470);
	lzma_reg_write(lzma, 0, LZMA_BS_DA_CFG_NUM, 47);
#endif

	//step4. set bs_base, bs_size, dst_base
	new_value = 0;
	file_in_addr = virt_to_phys((void*)file_fp);
	lzma_reg_write(lzma, 0, LZMA_BS_BASE, file_in_addr);
	if (file_in_addr != lzma_reg_read(lzma, 0, LZMA_BS_BASE))
	{
		printk("%d lzma0 reg set failed now, new_value=0x%lx, reg_val=0x%x\n", __LINE__, file_in_addr, lzma_reg_read(lzma, 0, LZMA_BS_BASE));
		goto LZMA_OUT;
		return -1;
	}

	new_value = 0;
	new_value |= conf->src0_size;
	lzma_reg_write(lzma, 0, LZMA_BS_SIZE, new_value);
	if (new_value != lzma_reg_read(lzma, 0, LZMA_BS_SIZE))
	{
		printk("%d lzma0 reg set failed now, new_value=0x%x, reg_val=0x%x\n",  __LINE__, new_value, lzma_reg_read(lzma, 0, LZMA_BS_SIZE));
		goto LZMA_OUT;
		return -1;
	}

	new_value = 0;
	new_value = virt_to_phys((void*)file_fp_out);
	lzma_reg_write(lzma, 0, LZMA_DST_BASE, new_value);
	if (new_value != lzma_reg_read(lzma, 0, LZMA_DST_BASE))
	{
		printk("%d lzma0 reg set failed now, new_value=0x%x, reg_val=0x%x\n",  __LINE__, new_value, lzma_reg_read(lzma, 0, LZMA_DST_BASE));
		goto LZMA_OUT;
		return -1;
	}

	//step5. close irq and start lzma0 decode
	printk("----- lzma0 decode begin...\n");

	lzma_reg_write(lzma, 0, LZMA_CTRL, 0x1<<0);
	while(lzma_reg_read(lzma, 0, LZMA_CTRL) & 0x01){
#if 0
		if(lzma_reg_read(lzma, 0, LZMA_TIMEOUT) >= 0xfff5f000){
			break;
		}
#endif
	}

	printk("------ lzma0 decode over.\n");
#ifdef DEBUG_LZMA
	jz_lzma_debug(lzma, 0);
#endif

	fp1 = filp_open(conf->dst0_name, O_RDWR | O_CREAT, 0644);
	if (fp1 == NULL)
	{
		printk("file: uImage.lzma0 open failed...\n");
		goto LZMA_OUT;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	dma_cache_sync(NULL, (void *)file_fp_out, conf->dst0_size, DMA_BIDIRECTIONAL);
	vfs_write(fp1, (char*)file_fp_out, lzma_reg_read(lzma, 0, LZMA_FINAL_SIZE), &pos);   //vmlinux raw size:6256492, crc_32=0x4BE56CE4
	dma_cache_sync(NULL, (void *)file_fp_out, conf->dst0_size, DMA_BIDIRECTIONAL);

	filp_close(fp1, NULL);
	set_fs(fs);
LZMA_OUT:
	if(((lzma_reg_read(lzma, 0, LZMA_CTRL) >> 31) & 0x1) == 1){
		lzma_reg_write(lzma, 0, LZMA_CTRL, 0x1<<31);
		printk("lzma0-----Read LZMA_CTRL[31]=0x%x set to jpeg\n", ((lzma_reg_read(lzma, 0, LZMA_CTRL) >> 31) & 0x1));
	}
OUT:
	free_pages(file_fp,  power(conf->src0_size/4096));
	free_pages(file_fp_out,  power(conf->dst0_size/4096));

	return 0;
}

int work_lzma1(struct jz_lzma *lzma, struct lzma_conf *conf)
{
	unsigned int new_value = 0;
	unsigned long file_fp;
	unsigned long file_fp_out;
	struct file *fp = NULL;
	struct file *fp1 = NULL;
	unsigned long file_in_addr = 0;

	mm_segment_t fs;
	loff_t pos = 0;

	file_fp = __get_free_pages(GFP_KERNEL, power((conf->src1_size+4095)/4096));   //10 == 1024 == 4M ,if page_size=4096bytes,11==(2^11)*4K
	if (file_fp <= 0)
	{
		printk("------ file_fp alloc failed.\n");
		return -1;
	}
	memset((void*)file_fp, 0x0, conf->src1_size);

	file_fp_out = __get_free_pages(GFP_KERNEL, power((conf->dst1_size+4095)/4096));   //10 == 1024 == 4M ,if page_size=4096bytes,12==16M
	if (file_fp_out <= 0)
	{
		free_pages(file_fp, power((conf->src1_size+4095)/4096));
		printk("------ file_fp_out alloc failed.\n");
		return -1;
	}

	memset((void*)file_fp_out, 0, conf->dst1_size);

	fp = filp_open(conf->src1_name, O_RDWR, 0644);
	if (fp == NULL)
	{
		printk("file: uImage.lzma1 open failed...\n");
		goto OUT;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	dma_cache_sync(NULL, (void *)file_fp, conf->src1_size, DMA_BIDIRECTIONAL);
	printk("lzma1 vfs_read size=%d\n", vfs_read(fp, (unsigned char*)file_fp, conf->src1_size, &pos));
	dma_cache_sync(NULL, (void *)file_fp, conf->src1_size, DMA_BIDIRECTIONAL);
	filp_close(fp, NULL);
	set_fs(fs);

	//step1. start lzma1 decoder
	if(((lzma_reg_read(lzma, 1, LZMA_CTRL) >> 31) & 0x1) != 1){
		printk("lzma1-----Write LZMA_CTRL[31]\n");
		lzma_reg_write(lzma, 1, LZMA_CTRL, 0x1<<31);
	}

	//step2. reset lzma1 module
	lzma_reg_write(lzma, 1, LZMA_CTRL, 0x1<<1);
	while ((lzma_reg_read(lzma, 1, LZMA_CTRL) >> 1) & 0x1);

	//step4. set bs_base, bs_size, dst_base
	new_value = 0;
	file_in_addr = virt_to_phys((void*)file_fp);
	lzma_reg_write(lzma, 1, LZMA_BS_BASE, file_in_addr);
	if (file_in_addr != lzma_reg_read(lzma, 1, LZMA_BS_BASE))
	{
		printk("lzma1 reg set failed now, new_value=0x%x, reg_val=0x%x\n", new_value, lzma_reg_read(lzma, 1, LZMA_BS_BASE));
		goto OUT;
		return -1;
	}

	//printk("----- set bs_size begin...\n");
	new_value = 0;
	new_value |= conf->src1_size;
	lzma_reg_write(lzma, 1, LZMA_BS_SIZE, new_value);
	if (new_value != lzma_reg_read(lzma, 1, LZMA_BS_SIZE))
	{
		printk("lzma1 reg set failed now, new_value=0x%x, reg_val=0x%x\n", new_value, lzma_reg_read(lzma, 1, LZMA_BS_SIZE));
		goto OUT;
		return -1;
	}
	new_value = 0;
	new_value = virt_to_phys((void*)file_fp_out);
	lzma_reg_write(lzma, 1, LZMA_DST_BASE, new_value);
	if (new_value != lzma_reg_read(lzma, 1, LZMA_DST_BASE))
	{
		printk("lzma1 reg set failed now, new_value=0x%x, reg_val=0x%x\n", new_value, lzma_reg_read(lzma, 1, LZMA_DST_BASE));
		goto OUT;
		return -1;
	}

	//step5. close irq and start lzma1 decode
	printk("------lzma1 decode begin...\n");

	lzma_reg_write(lzma, 1, LZMA_CTRL, 0x1<<0);
	while(lzma_reg_read(lzma, 1, LZMA_CTRL) & 0x01){
#if 0
		if(lzma_reg_read(lzma, 1, LZMA_TIMEOUT) >= 0xfff5f000){
			break;
		}
#endif
	}

	printk("------lzma1 decode over.\n");

	fp1 = filp_open(conf->dst1_name, O_RDWR | O_CREAT, 0644);
	if (fp1 == NULL)
	{
		printk("file: uImage.lzma1 open failed...\n");
		goto OUT;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	dma_cache_sync(NULL, (void *)file_fp_out, conf->dst1_size, DMA_BIDIRECTIONAL);
	vfs_write(fp1, (char*)file_fp_out, lzma_reg_read(lzma, 1, LZMA_FINAL_SIZE), &pos);   //vmlinux raw size:6256492, crc_32=0x4BE56CE4
	dma_cache_sync(NULL, (void *)file_fp_out, conf->dst1_size, DMA_BIDIRECTIONAL);

	filp_close(fp1, NULL);
	set_fs(fs);
OUT:
	free_pages(file_fp, power((conf->src1_size+4095)/4096));
	free_pages(file_fp_out, power((conf->dst1_size+4095)/4096));

	return 0;
}

int work_lzma_s(struct jz_lzma *lzma, struct lzma_conf *conf)
{
	unsigned int new_value0 = 0;
	unsigned int new_value1 = 0;
	unsigned long file_fp0;
	unsigned long file_fp1;
	unsigned long file_fp_out0;
	unsigned long file_fp_out1;
	unsigned long file_in_addr0 = 0;
	unsigned long file_in_addr1 = 0;
	struct file *fpIn0 = NULL;
	struct file *fpIn1 = NULL;
	struct file *fpout0 = NULL;
	struct file *fpout1 = NULL;

	mm_segment_t fs;
	loff_t pos = 0;

	file_fp0 = __get_free_pages(GFP_KERNEL, power((conf->src0_size+4095)/4096));   //10 == 1024 == 4M ,if page_size=4096bytes,11==(2^11)*4K
	file_fp1 = __get_free_pages(GFP_KERNEL, power((conf->src1_size+4095)/4096));   //10 == 1024 == 4M ,if page_size=4096bytes,11==(2^11)*4K
	if (file_fp0 <= 0 || file_fp1 <= 0)
	{
		printk("------ file_fp alloc failed.\n");
		return -1;
	}
	memset((void*)file_fp0, 0x0, conf->src0_size);
	memset((void*)file_fp1, 0x0, conf->src1_size);

	file_fp_out0 = __get_free_pages(GFP_KERNEL, power((conf->dst0_size+4095)/4096));   //10 == 1024 == 4M ,if page_size=4096bytes,12==16M
	file_fp_out1 = __get_free_pages(GFP_KERNEL, power((conf->dst1_size+4095)/4096));   //10 == 1024 == 4M ,if page_size=4096bytes,12==16M
	if (file_fp_out0 <= 0 || file_fp_out1 <= 0)
	{
		free_pages(file_fp0, power((conf->src0_size+4095)/4096));
		free_pages(file_fp1, power((conf->src1_size+4095)/4096));
		printk("------ file_fp_out alloc failed.\n");
		return -1;
	}

	memset((void*)file_fp_out0, 0, conf->dst0_size);
	memset((void*)file_fp_out1, 0, conf->dst1_size);

	fpIn0 = filp_open(conf->src0_name, O_RDWR, 0644);
	fpIn1 = filp_open(conf->src1_name, O_RDWR, 0644);
	if (fpIn0 == NULL || fpIn1 == NULL)
	{
		printk("file: uImage.lzma1 open failed...\n");
		goto OUT;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	dma_cache_sync(NULL, (void *)file_fp0, conf->src0_size, DMA_BIDIRECTIONAL);
	printk("lzma0 vfs_read size=%d\n", vfs_read(fpIn0, (unsigned char*)file_fp0, conf->src0_size, &pos));
	dma_cache_sync(NULL, (void *)file_fp0, conf->src0_size, DMA_BIDIRECTIONAL);
	pos = 0;
	dma_cache_sync(NULL, (void *)file_fp1, conf->src1_size, DMA_BIDIRECTIONAL);
	printk("lzma1 vfs_read size=%d\n", vfs_read(fpIn1, (unsigned char*)file_fp1, conf->src1_size, &pos));
	dma_cache_sync(NULL, (void *)file_fp1, conf->src1_size, DMA_BIDIRECTIONAL);
	filp_close(fpIn0, NULL);
	filp_close(fpIn1, NULL);
	set_fs(fs);

	//step1. start lzma1 decoder
	if(((lzma_reg_read(lzma, 0, LZMA_CTRL) >> 31) & 0x1) != 1){
		printk("lzma1-----Write LZMA_CTRL[31]\n");
		lzma_reg_write(lzma, 0, LZMA_CTRL, 0x1<<31);
	}
	if(((lzma_reg_read(lzma, 1, LZMA_CTRL) >> 31) & 0x1) != 1){
		printk("lzma1-----Write LZMA_CTRL[31]\n");
		lzma_reg_write(lzma, 1, LZMA_CTRL, 0x1<<31);
	}

	//step2. reset lzma1 module
	lzma_reg_write(lzma, 0, LZMA_CTRL, 0x1<<1);
	while ((lzma_reg_read(lzma, 0, LZMA_CTRL) >> 1) & 0x1);

	lzma_reg_write(lzma, 1, LZMA_CTRL, 0x1<<1);
	while ((lzma_reg_read(lzma, 1, LZMA_CTRL) >> 1) & 0x1);

	//step4. set bs_base, bs_size, dst_base
	new_value0 = 0;
	file_in_addr0 = virt_to_phys((void*)file_fp0);
	file_in_addr1 = virt_to_phys((void*)file_fp1);
	lzma_reg_write(lzma, 0, LZMA_BS_BASE, file_in_addr0);
	lzma_reg_write(lzma, 1, LZMA_BS_BASE, file_in_addr1);
	if (file_in_addr0 != lzma_reg_read(lzma, 0, LZMA_BS_BASE))
	{
		printk("lzma1 reg set failed now, new_value=0x%x, reg_val=0x%x\n", new_value0, lzma_reg_read(lzma, 0, LZMA_BS_BASE));
		goto OUT;
		return -1;
	}

	new_value0 = 0;
	new_value1 = 0;
	new_value0 |= conf->src0_size;
	new_value1 |= conf->src1_size;
	lzma_reg_write(lzma, 0, LZMA_BS_SIZE, new_value0);
	lzma_reg_write(lzma, 1, LZMA_BS_SIZE, new_value1);
	if (new_value0 != lzma_reg_read(lzma, 0, LZMA_BS_SIZE))
	{
		printk("lzma1 reg set failed now, new_value=0x%x, reg_val=0x%x\n", new_value0, lzma_reg_read(lzma, 0, LZMA_BS_SIZE));
		goto OUT;
		return -1;
	}
	new_value0 = 0;
	new_value1 = 0;
	new_value0 = virt_to_phys((void*)file_fp_out0);
	new_value1 = virt_to_phys((void*)file_fp_out1);
	lzma_reg_write(lzma, 0, LZMA_DST_BASE, new_value0);
	lzma_reg_write(lzma, 1, LZMA_DST_BASE, new_value1);
	if (new_value0 != lzma_reg_read(lzma, 0, LZMA_DST_BASE))
	{
		printk("lzma1 reg set failed now, new_value=0x%x, reg_val=0x%x\n", new_value0, lzma_reg_read(lzma, 0, LZMA_DST_BASE));
		goto OUT;
		return -1;
	}

	//step5. close irq and start lzma1 decode
	printk("------lzma1 decode begin...\n");

	lzma_reg_write(lzma, 0, LZMA_CTRL, 0x1<<0);
	lzma_reg_write(lzma, 1, LZMA_CTRL, 0x1<<0);
	while( (lzma_reg_read(lzma, 0, LZMA_CTRL) & 0x01) ){
#if 0
		if(lzma_reg_read(lzma, 0, LZMA_TIMEOUT) >= 0xfff5f000){
			break;
		}
#endif
	}

	printk("------lzma0 decode over.\n");
	while( (lzma_reg_read(lzma, 1, LZMA_CTRL) & 0x01) ){}
	printk("------lzma1 decode over.\n");

	fpout0 = filp_open(conf->dst0_name, O_RDWR | O_CREAT, 0644);
	fpout1 = filp_open(conf->dst1_name, O_RDWR | O_CREAT, 0644);
	if (fpout0 == NULL)
	{
		printk("file: uImage.lzma1 open failed...\n");
		goto OUT;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	dma_cache_sync(NULL, (void *)file_fp_out0, conf->dst0_size, DMA_BIDIRECTIONAL);
	vfs_write(fpout0, (char*)file_fp_out0, lzma_reg_read(lzma, 0, LZMA_FINAL_SIZE), &pos);   //vmlinux raw size:6256492, crc_32=0x4BE56CE4
	dma_cache_sync(NULL, (void *)file_fp_out0, conf->dst0_size, DMA_BIDIRECTIONAL);

	dma_cache_sync(NULL, (void *)file_fp_out1, conf->dst1_size, DMA_BIDIRECTIONAL);
	pos = 0;
	vfs_write(fpout1, (char*)file_fp_out1, lzma_reg_read(lzma, 1, LZMA_FINAL_SIZE), &pos);   //vmlinux raw size:6256492, crc_32=0x4BE56CE4
	dma_cache_sync(NULL, (void *)file_fp_out1, conf->dst1_size, DMA_BIDIRECTIONAL);

	filp_close(fpout0, NULL);
	filp_close(fpout1, NULL);
	set_fs(fs);
OUT:
	free_pages(file_fp0, power((conf->src0_size+4095)/4096));
	free_pages(file_fp1, power((conf->src1_size+4095)/4096));
	free_pages(file_fp_out0, power((conf->dst0_size+4095)/4096));
	free_pages(file_fp_out1, power((conf->dst1_size+4095)/4096));

	return 0;
}

static long lzma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	struct miscdevice *dev = filp->private_data;
	struct jz_lzma *lzma = container_of(dev, struct jz_lzma, misc_dev);
	struct lzma_conf lzma_conf;
	int ret = 0;

	if (_IOC_TYPE(cmd) != JZLZMA_IOC_MAGIC) {
		dev_err(lzma->dev, "invalid cmd!\n");
		return -EFAULT;
	}
	mutex_lock(&lzma->mutex);

	switch (cmd) {
		case IOCTL_LZMA0_START:
			if (copy_from_user(&lzma_conf, (void *)arg, sizeof(struct lzma_conf))) {
				dev_err(lzma->dev, "copy_from_user error!!!\n");
				ret = -EFAULT;
				break;
			}
		    ret = work_lzma0(lzma, &lzma_conf);
			if (ret) {
				printk("lzma: error ipu start ret = %d\n", ret);
			}
			break;
		case IOCTL_LZMA1_START:
			if (copy_from_user(&lzma_conf, (void *)arg, sizeof(struct lzma_conf))) {
				dev_err(lzma->dev, "copy_from_user error!!!\n");
				ret = -EFAULT;
				break;
			}
		    ret = work_lzma1(lzma, &lzma_conf);
			if (ret) {
				printk("lzma: error ipu start ret = %d\n", ret);
			}
			break;
		case IOCTL_LZMAA_START:
			if (copy_from_user(&lzma_conf, (void *)arg, sizeof(struct lzma_conf))) {
				dev_err(lzma->dev, "copy_from_user error!!!\n");
				ret = -EFAULT;
				break;
			}
		    ret = work_lzma1(lzma, &lzma_conf);
			if (ret) {
				printk("lzma: error ipu start ret = %d\n", ret);
			}
			break;
	    default:
			dev_err(lzma->dev, "invalid command: 0x%08x\n", cmd);
			ret = -EINVAL;
	}

	mutex_unlock(&lzma->mutex);
	return ret;
#if 0
	printk("jz----------- lzma%d_ioctl .\n", data[0]);
	argp = (void __user*)val;
	copy_from_user(data, (void*)argp, sizeof(data));

	printk("cmd=%d data0=0x%x, data1=%u, data2=0x%x\n", cmd, data[0], data[1], data[2]);
	if(data[0] == 0){
		ret = work_lzma0(lzma, data[1]);
	}
	if(data[0] == 1){
		ret = work_lzma1(lzma, data[1]);
	}
	if(data[0] == 9){
		ret = work_lzma_s(lzma, data[1], data[2]);
	}
	return ret;
#endif
}

static const struct file_operations jz_lzma_ops = {
	.owner=THIS_MODULE,
	.open=lzma_open,
	.release=lzma_close,
	.read=lzma_read,
	.write=lzma_write,
	.unlocked_ioctl=lzma_ioctl,
};

static int jz_lzma_probe(struct platform_device *pdev)
{
	//request mem
	int ret = 0;
	struct jz_lzma *lzma;

	lzma = (struct jz_lzma *)kzalloc(sizeof(struct jz_lzma), GFP_KERNEL);
	if (!lzma) {
		dev_err(&pdev->dev, "alloc jz_lzma failed!\n");
		return -ENOMEM;
	}

	sprintf(lzma->name, "jz_lzma");

	lzma->misc_dev.minor = MISC_DYNAMIC_MINOR;
	lzma->misc_dev.name = lzma->name;
	lzma->misc_dev.fops = &jz_lzma_ops;
	lzma->dev = &pdev->dev;

	mutex_init(&lzma->mutex);
	lzma->res0 = request_mem_region(JZ_LZMA0_START_ADDRESS, JZ_LZMA_LENGTH_ADDRESS, "jz_lzma0");
	lzma->res1 = request_mem_region(JZ_LZMA1_START_ADDRESS, JZ_LZMA_LENGTH_ADDRESS, "jz_lzma1");
	if (lzma->res0 == NULL)
	{
		printk("----- res request failed.\n");
		goto iomem_failed;
	}

	//iomap
	lzma->iomem0 = ioremap(JZ_LZMA0_START_ADDRESS, JZ_LZMA_LENGTH_ADDRESS);
	lzma->iomem1 = ioremap(JZ_LZMA1_START_ADDRESS, JZ_LZMA_LENGTH_ADDRESS);
	if ((lzma->iomem0 == NULL) || (lzma->iomem1 == NULL))
	{
		printk("------ ioremap failed.\n");
		goto iomem_failed;
	}

	//misc register
	ret = misc_register(&lzma->misc_dev);
	if (ret) {
		dev_err(&pdev->dev,"request misc device failed!\n");
		ret = -EINVAL;
		goto failed_misc;
	}

#if (defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_6_1))
	lzma->clk_ispa = clk_get(&pdev->dev,"mux_ispa");
	if (IS_ERR(lzma->clk_ispa)) {
		dev_err(&pdev->dev, "ispa clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
	lzma->clk_lzma = clk_get(&pdev->dev,"gate_lzma");
	if (IS_ERR(lzma->clk_lzma)) {
		dev_err(&pdev->dev, "lzma clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
	ret = clk_set_parent(lzma->clk_ispa, clk_get(NULL, "vpll"));
	clk_prepare_enable(lzma->clk_ispa);
	clk_prepare_enable(lzma->clk_lzma);
#else
	lzma->clk_ispa = clk_get(&pdev->dev, "cgu_ispa");
	if (IS_ERR(lzma->clk_ispa)) {
		dev_err(&pdev->dev, "ispa clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
	lzma->clk_lzma = clk_get(&pdev->dev, "lzma");
	if (IS_ERR(lzma->clk_lzma)) {
		dev_err(&pdev->dev, "lzma clk get failed!\n");
		ret = -EINVAL;
		goto failed_clk;
	}
	ret = clk_set_parent(lzma->clk_ispa, clk_get(NULL, "vpll"));
	clk_enable(lzma->clk_ispa);
	clk_enable(lzma->clk_lzma);
#endif

	printk("@@@@  jz_lzma driver  probe ok(version %s) @@@@@ \n", LZMA_DRIVER_VERSION);
	return 0;

failed_clk:
	misc_deregister(&lzma->misc_dev);
	iounmap(lzma->iomem0);
	iounmap(lzma->iomem1);

failed_misc:
	release_mem_region(JZ_LZMA0_START_ADDRESS, JZ_LZMA_LENGTH_ADDRESS);

iomem_failed:
	return -1;
}

static int jz_lzma_remove(struct platform_device *pdev)
{
	struct jz_lzma *lzma = platform_get_drvdata(pdev);
	//misc unregister
	misc_deregister(&lzma->misc_dev);

	//io unmap
	iounmap(lzma->iomem0);
	iounmap(lzma->iomem1);

	//mem release
	release_mem_region(JZ_LZMA0_START_ADDRESS, JZ_LZMA_LENGTH_ADDRESS);
	release_mem_region(JZ_LZMA1_START_ADDRESS, JZ_LZMA_LENGTH_ADDRESS);

	return 0;
}

static struct platform_driver jz_lzma_driver = {
	.probe = jz_lzma_probe,
	.remove = jz_lzma_remove,
	.driver = {
		.name = "jz_lzma",
		.owner = THIS_MODULE,
	},
};

static struct platform_device jz_lzma_device = {
	.name = "jz_lzma",
};

static int __init jz_lzma_init(void)
{
	platform_device_register(&jz_lzma_device);
	return platform_driver_register(&jz_lzma_driver);
}

static void __exit jz_lzma_exit(void)
{
	platform_driver_unregister(&jz_lzma_driver);
	platform_device_unregister(&jz_lzma_device);
}


module_init(jz_lzma_init);
module_exit(jz_lzma_exit);

MODULE_DESCRIPTION("lzma kernel hardware decompression.");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("damon.jszhang@ingenic.com");
