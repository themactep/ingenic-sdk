/*
 * * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "asm-generic/getorder.h"
#include "asm/fcntl.h"
#include "asm/page.h"
#include "ingenic_lzma.h"
#include "linux/hrtimer.h"
#include "linux/interrupt.h"
#include "linux/ktime.h"
#include "linux/mutex.h"
#include <linux/clk.h>
#include <linux/version.h>

#ifdef DEBUG_LZMA
#define JZ_LZMA_LENGTH_ADDRESS 0x8000
#define lzma_reg_dump() reg_dump()
#else
#define JZ_LZMA_LENGTH_ADDRESS 28
#endif

// #define FPGA_TEST
#define complt_chn(IDX) lzma##IDX##_completion
#define LZMA_MAX_FREQ (500*1000*1000) //max support 600MHZ

DECLARE_COMPLETION(lzma0_completion);
DECLARE_COMPLETION(lzma1_completion);

void *lzma0_ioaddr;
void *lzma1_ioaddr;
void *cpm_ioaddr;
struct platform_device *pgdev;

struct lzma_clk_param{
	struct clk	*gate_tnpu_clk;
	struct clk	*gate_lzma0_clk;
	struct clk	*gate_lzma1_clk;
};

static void lzma_writel(unsigned long val, unsigned long offset,char channel)
{
	switch(channel){
		case 0:
			writel(val, lzma0_ioaddr + offset);
			break;
		case 1:
			writel(val, lzma1_ioaddr + offset);
			break;
		default:
			printk("chan:%d not support yet\n",channel);
			return;
	}
}

static unsigned int lzma_readl(unsigned long offset,char channel)
{
	switch(channel){
		case 0:
			return readl(lzma0_ioaddr + offset);
		case 1:
			return readl(lzma1_ioaddr + offset);
		default:
			printk("chan:%d,not support yet\n",channel);
			return -1;
	}
}

static int lzma_open(struct inode *inode, struct file *pfil)
{
	return 0;
}

static int lzma_close(struct inode *inode, struct file *pfil)
{
	return 0;
}

static ssize_t lzma_read(struct file *pfil, char __user *user, size_t size, loff_t *offset)
{
	return 0;
}

static ssize_t lzma_write(struct file *pfil, const char __user *user, size_t size, loff_t *offset)
{
	return 0;
}

#if 0
static void reg_dump(void)
{
	printk("=============================================\n");
	printk("LZMA_WORKTIME = %d\n", (0xffffffff - lzma_readl(LZMA_TIMEOUT)) * 1666);
	printk("LZMA_BS_BASE = 0x%x\n", lzma_readl(LZMA_BS_BASE));
	printk("LZMA_BS_SIZE = 0x%x\n", lzma_readl(LZMA_BS_SIZE));
	printk("LZMA_DST_BASE = 0x%x\n", lzma_readl(LZMA_DST_BASE));
	printk("LZMA_TIMEOUT = 0x%x\n", lzma_readl(LZMA_TIMEOUT));
	printk("LZMA_FINAL_SIZE = 0x%x\n", lzma_readl(LZMA_FINAL_SIZE));
	printk("LZMA_FSM_DBG = 0x%x\n", lzma_readl(LZMA_FSM_DBG));
	printk("=============================================\n");
	printk("LZMA_RADDR_CNT_DBG = %d\n", lzma_readl(LZMA_RADDR_CNT_DBG));
	printk("LZMA_RADDR_CFG_NUM = %d\n", lzma_readl(LZMA_RADDR_CFG_NUM));
	printk("LZMA_RADDR0_DBG = 0x%x\n", lzma_readl(LZMA_RADDR0_DBG));
	printk("LZMA_RADDR1_DBG = 0x%x\n", lzma_readl(LZMA_RADDR1_DBG));
	printk("LZMA_RDATA_CNT_DBG = %d\n", lzma_readl(LZMA_RDATA_CNT_DBG));
	printk("LZMA_RDATA_CFG_NUM = %d\n", lzma_readl(LZMA_RDATA_CFG_NUM));
	printk("LZMA_RDATAL0_DBG = 0x%x\n", lzma_readl(LZMA_RDATAL0_DBG));
	printk("LZMA_RDATAH0_DBG = 0x%x\n", lzma_readl(LZMA_RDATAH0_DBG));
	printk("=============================================\n");
	printk("LZMA_CRC_RDATA = 0x%x\n", lzma_readl(LZMA_CRC_RDATA));
	printk("LZMA_CRC_HDEC_DATA = 0x%x\n", lzma_readl(LZMA_CRC_HDEC_DATA));
	printk("LZMA_CRC_FSM_DATA = 0x%x\n", lzma_readl(LZMA_CRC_FSM_DATA));
	printk("LZMA_CRC_DIST_DATA = 0x%x\n", lzma_readl(LZMA_CRC_DIST_DATA));
	printk("LZMA_CRC_LEN_DATA = 0x%x\n", lzma_readl(LZMA_CRC_LEN_DATA));
	printk("LZMA_CRC_LIT_DATA = 0x%x\n", lzma_readl(LZMA_CRC_LIT_DATA));
	printk("LZMA_CRC_WDATA = 0x%x\n", lzma_readl(LZMA_CRC_WDATA));
	printk("LZMA_IRQ_MASK 1 = 0x%x\n", lzma_readl(LZMA_CTRL) >> 2);
	printk("LZMA_IRQ = 0x%x\n", lzma_readl(LZMA_CTRL) >> 3);
	printk("=============================================\n");
}
#endif

static unsigned long lzma_virt_to_phy(void *viraddr)
{
	struct page *page = virt_to_page(viraddr);
	phys_addr_t physical_address = page_to_phys(page);
	if((physical_address & (128-1)) != 0){
		printk("warning,this addr not 128 align");
	}
	// printk("lzma vir:%p,phy:%#x\n",viraddr,physical_address);
	return physical_address;
}


#ifdef FPGA_TEST
static void lzma_clock_on(void *cpm_ioaddr)
{
	unsigned int clk_gate = 0;
	/* tnpu clk on,low is valid*/
	clk_gate = readl(cpm_ioaddr + CPM_CLKGR1);
	clk_gate &= ~(1 << 11);//tnpu disable
	writel(clk_gate, cpm_ioaddr + CPM_CLKGR1);

	//set hoclk
	writel(readl(cpm_ioaddr + CPM_CPCCR) & (~(1<<11)), cpm_ioaddr + CPM_CPCCR);

	//setting tnpu_clk
	writel(0x20000011, cpm_ioaddr + CPM_TNPUCDR); //tnpu enalbe

	//lzma0_clk
	writel(readl(cpm_ioaddr + CPM_CLKGR1) & (~(0x1 << 4)), cpm_ioaddr + CPM_CLKGR1);
	//lzma1_clk
	writel(readl(cpm_ioaddr + CPM_CLKGR1) & (~(0x1 << 15)), cpm_ioaddr + CPM_CLKGR1);

	writel(readl(cpm_ioaddr + CPM_SRBC) & (~(0x1 << 25)), cpm_ioaddr + CPM_SRBC);

	/*printk("tnpu_clk_en:%#x,ahbo_clk_freq:%#x,tnpu_clk_freq:%#x,lzma_clock_en:%#x\n",\
	readl(cpm_ioaddr + CPM_CLKGR1) >> 11 & 0x1,\
	printk("ahbo_clk_freq:%#x\n",readl(cpm_ioaddr + CPM_CPCCR)),\
	printk("tnpu_clk_freq:%#x\n",readl(cpm_ioaddr + CPM_TNPUCDR)),\
	readl(cpm_ioaddr + CPM_SRBC)>>25 & 0x1);*/
}

static void lzma_clock_off(void *cpm_ioaddr)
{
	unsigned int clk_gate = 0;
	/* open clk gate */
	clk_gate = readl(cpm_ioaddr + CPM_CLKGR1);
	clk_gate |= 1 << 11;//tnpu disable
	writel(clk_gate, cpm_ioaddr + CPM_CLKGR1);

	//set ahb0clk
	writel(readl(cpm_ioaddr + CPM_CPCCR) | (1<<11), cpm_ioaddr + CPM_CPCCR);

	//setting tnpu_clk
	writel(readl(cpm_ioaddr + CPM_TNPUCDR) & (~(1<<29)), cpm_ioaddr + CPM_TNPUCDR); //tnpu enalbe

	//lzma0_clk
	writel(readl(cpm_ioaddr + CPM_CLKGR1) | (0x1 << 4), cpm_ioaddr + CPM_CLKGR1);
	//lzma1_clk
	writel(readl(cpm_ioaddr + CPM_CLKGR1) | (0x1 << 15), cpm_ioaddr + CPM_CLKGR1);

	writel(readl(cpm_ioaddr + CPM_SRBC) | (0x1 << 25), cpm_ioaddr + CPM_SRBC);

	/*printk("tnpu_clk_en:%#x,ahbo_clk_freq:%#x,tnpu_clk_freq:%#x,lzma_clock_en:%#x\n",\
	readl(cpm_ioaddr + CPM_CLKGR1) >> 11 & 0x1,\
	printk("ahbo_clk_freq:%#x\n",readl(cpm_ioaddr + CPM_CPCCR)),\
	printk("tnpu_clk_freq:%#x\n",readl(cpm_ioaddr + CPM_TNPUCDR)),\
	readl(cpm_ioaddr + CPM_SRBC)>>25 & 0x1);*/
}

#else

static int lzma_clock_enable(struct lzma_clk_param *param)
{
	if(!pgdev){
		printk("[%s]err,device is not init\n",__func__);
		return -1;
	}
	param->gate_tnpu_clk = devm_clk_get(&pgdev->dev, "div_tnpu");
	if (IS_ERR(param->gate_tnpu_clk))
		return PTR_ERR(param->gate_tnpu_clk);
	param->gate_lzma0_clk = devm_clk_get(&pgdev->dev, "gate_lzma0");
	if (IS_ERR(param->gate_lzma0_clk))
		return PTR_ERR(param->gate_lzma0_clk);
	param->gate_lzma1_clk = devm_clk_get(&pgdev->dev, "gate_lzma1");
	if (IS_ERR(param->gate_lzma1_clk))
		return PTR_ERR(param->gate_lzma1_clk);

	if(clk_get_rate(param->gate_lzma0_clk) > LZMA_MAX_FREQ || \
		clk_get_rate(param->gate_lzma0_clk) > LZMA_MAX_FREQ ||
		clk_get_rate(param->gate_tnpu_clk) > LZMA_MAX_FREQ){

		clk_set_rate(param->gate_tnpu_clk, LZMA_MAX_FREQ);
		clk_set_rate(param->gate_lzma0_clk, LZMA_MAX_FREQ);
		clk_set_rate(param->gate_lzma1_clk, LZMA_MAX_FREQ);
	}

	clk_prepare_enable(param->gate_tnpu_clk);
	clk_prepare_enable(param->gate_lzma0_clk);
	clk_prepare_enable(param->gate_lzma1_clk);

	return 0;
}

static int lzma_clock_disable(struct lzma_clk_param *param)
{
	if(!pgdev){
		printk("[%s]err,device is not init\n",__func__);
		return -1;
	}
	if(!param){
		printk("[%s]err,clk param not init\n",__func__);
		return -1;
	}

	clk_disable_unprepare(param->gate_tnpu_clk);
	clk_disable_unprepare(param->gate_lzma0_clk);
	clk_disable_unprepare(param->gate_lzma1_clk);

	devm_clk_put(&pgdev->dev,param->gate_tnpu_clk);
	devm_clk_put(&pgdev->dev,param->gate_lzma0_clk);
	devm_clk_put(&pgdev->dev,param->gate_lzma1_clk);

	return 0;
}
#endif

static int work_lzma(char *input_name, off_t in_file_size, char *output_name,char channel)
{
	struct file *in_fp = NULL;
	struct file *out_fp = NULL;
	int cnt = 0;
	loff_t pos = 0;
	int fread_size = 0;
	unsigned char *file_fp = NULL;
	unsigned char *file_fp_out = NULL;
	dma_addr_t dma_handle_in, dma_handle_out;
	loff_t actual_file_size;

	unsigned long long file_in_addr;
	unsigned long long file_out_addr;

	struct lzma_clk_param clkparam = {0};
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	mm_segment_t fs;
#endif


	in_fp = filp_open(input_name, O_RDONLY, 0);
	if (IS_ERR(in_fp) || !in_fp)
	{
		printk("file: %s open failed...\n", input_name);
		return -1;
	}

	pos = vfs_llseek(in_fp, 0, SEEK_END);
	actual_file_size = pos;
	vfs_llseek(in_fp, 0, SEEK_SET);

	if (in_file_size > 0 && in_file_size != actual_file_size) {
		printk("warning: input size %ld doesn't match actual file size %lld\n",
		       in_file_size, actual_file_size);
		in_file_size = actual_file_size;
	} else if (in_file_size <= 0) {
		in_file_size = actual_file_size;
	}

	// printk("inputfile:%s,outputfile:%s,filesize:%ld,chan:%d\n",input_name,output_name,in_file_size,channel);

#ifdef FPGA_TEST
	lzma_clock_on(cpm_ioaddr);
#else
	lzma_clock_enable(&clkparam);
#endif
	// malloc input file
	if(get_order(in_file_size) > 10){
		printk("WRNING!!!,order:%d to big out of 10,,system may not support\n",get_order(in_file_size));
	}

	file_fp = dma_alloc_coherent(&pgdev->dev, in_file_size, &dma_handle_in, GFP_KERNEL);
	if (!file_fp)
	{
		printk("------ file_fp dma alloc failed.\n");
		filp_close(in_fp, NULL);
		return -1;
	}


#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	fs = get_fs();
	set_fs(KERNEL_DS);
#endif

	pos = in_fp->f_pos; //Used to change the way the kernel checks memory addresses
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	fread_size = vfs_read(in_fp, file_fp, in_file_size, &pos); //Pass the unsigned char * pointer as a parameter to vfs_read()
#else
	fread_size = kernel_read(in_fp, file_fp, in_file_size, &pos); //Pass the unsigned char * pointer as a parameter to vfs_read()
#endif
	if(in_file_size != fread_size){
		printk("warning!!!!!!,not read all data,check (file_size,read_size)(%ld,%d)\n",in_file_size,fread_size);
	}

	in_fp->f_pos = pos;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	set_fs(fs);
#endif

	filp_close(in_fp, NULL);

	printk("----- LZMA_VERISON : 0x%x\n", lzma_readl(LZMA_VERISON,channel));

	/* step1. start lzma decoder */
	while (((lzma_readl(LZMA_CTRL,channel) >> 31) & 0x1) != 1)
	{
		lzma_writel(0x1 << 31, LZMA_CTRL,channel);
	}

	//ram config
	while (((readl(lzma0_ioaddr+LZMA_CTRL) >> 7) & 0x1) != 1)
	{
		writel(0x1 << 7, lzma0_ioaddr + LZMA_CTRL);
	}

	/* step2. reset lzma module */
	lzma_writel(0x1 << 1, LZMA_CTRL,channel);
	while ((lzma_readl(LZMA_CTRL,channel) >> 1) & 0x1)
		;
	lzma_writel(0, LZMA_RADDR_CFG_NUM,channel);
	lzma_writel(0, LZMA_RDATA_CFG_NUM,channel);

	/* step3. set bs_base, bs_size, dst_base */
	file_in_addr = virt_to_phys(file_fp);
	file_in_addr = lzma_virt_to_phy(file_fp);
	lzma_writel(file_in_addr, LZMA_BS_BASE,channel);
	if (file_in_addr != lzma_readl(LZMA_BS_BASE,channel))
	{
		printk("lzma reg set failed now, file_in_addr=%#08llx, reg_val=0x%x\n", file_in_addr, lzma_readl(LZMA_BS_BASE,channel));
		return -1;
	}

	lzma_writel(in_file_size, LZMA_BS_SIZE,channel);
	if (in_file_size != lzma_readl(LZMA_BS_SIZE,channel))
	{
		printk("lzma reg set failed now, in_file_size=0x%ld, reg_val=0x%x\n", in_file_size, lzma_readl(LZMA_BS_SIZE,channel));
		return -1;
	}

	file_fp_out = dma_alloc_coherent(&pgdev->dev, (1 << OUT_SIZE) * PAGE_SIZE, &dma_handle_out, GFP_KERNEL);
	if (!file_fp_out)
	{
		dma_free_coherent(&pgdev->dev, in_file_size, file_fp, dma_handle_in);
		printk("------ file_fp_out dma alloc failed.\n");
		return -1;
	}

	file_out_addr = virt_to_phys(file_fp_out);
	file_out_addr = lzma_virt_to_phy(file_fp_out);
	lzma_writel(file_out_addr, LZMA_DST_BASE,channel);
	if (file_out_addr != lzma_readl(LZMA_DST_BASE,channel))
	{
		printk("lzma reg set failed now, file_out_addr=0x%llx, reg_val=0x%x\n", file_out_addr, lzma_readl(LZMA_DST_BASE,channel));
		return -1;
	}

	/* step4. lzma start */
	lzma_writel(0x1 << 0, LZMA_CTRL,channel);

	/* step5. wait lzma end */
	while (lzma_readl(LZMA_CTRL,channel) & 0x00000001){
		if(cnt++ == 0){
			printk("idata0:0x%08x,idata1:0x%08x\n",lzma_readl(LZMA_RDATAH0_DBG,channel),lzma_readl(LZMA_RDATAL0_DBG,channel));
			printk("iaddr0:0x%08x,iaddr1:0x%08x\n",lzma_readl(LZMA_RADDR0_DBG,channel),lzma_readl(LZMA_RADDR1_DBG,channel));
		}
	}

	if(0 == channel){
		wait_for_completion_interruptible(&complt_chn(0));
	} else {
		wait_for_completion_interruptible(&complt_chn(1));
	}

	//lzma_reg_dump();
	out_fp = filp_open(output_name, O_RDWR | O_CREAT, 0644);
	if (out_fp == NULL)
	{
		printk("file: uImage.lzma open failed...\n");
		return -1;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	fs = get_fs();
	set_fs(KERNEL_DS);
#endif

	// printk("writefile_size:%d\n",lzma_readl(LZMA_FINAL_SIZE,channel));
	pos = out_fp->f_pos;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	vfs_write(out_fp, file_fp_out, lzma_readl(LZMA_FINAL_SIZE,channel), &pos);
#else
	kernel_write(out_fp, file_fp_out, lzma_readl(LZMA_FINAL_SIZE,channel), &pos);
#endif
	out_fp->f_pos = pos;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	set_fs(fs);
#endif

	filp_close(out_fp, NULL);

	dma_free_coherent(&pgdev->dev, in_file_size, file_fp, dma_handle_in);
	dma_free_coherent(&pgdev->dev, (1 << OUT_SIZE) * PAGE_SIZE, file_fp_out, dma_handle_out);
#ifdef FPGA_TEST
	lzma_clock_off(cpm_ioaddr);
#else
	lzma_clock_disable(&clkparam);
#endif
	memset(&clkparam,0,sizeof(clkparam));

	file_fp = NULL;
	file_fp_out = NULL;

	return 0;
}

static long lzma_ioctl(struct file *pfil, unsigned int cmd, unsigned long val)
{
	void __user *argp = NULL;
	int err = 0;
	argp = (void __user *)val;

	switch (cmd)
	{
	case IOCTL_LAMA_WORK:
	{
		struct lzma_file_info info;

		if (copy_from_user(&info, argp, sizeof(struct lzma_file_info)) != 0)
		{
			err = -EFAULT;
			break;
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
		if (!access_ok(VERIFY_READ, argp, sizeof(struct lzma_file_info)))
#else
		if (!access_ok(argp, sizeof(struct lzma_file_info)))
#endif
		{
			err = -EFAULT;
			break;
		}
		err = work_lzma(info.input_name, info.size, info.output_name,info.channel);
		break;
	}
	default:
		err = -ENOTTY;
		break;
	}

	return err;
}

static const struct file_operations jz_lzma_ops = {
	.owner = THIS_MODULE,
	.open = lzma_open,
	.release = lzma_close,
	.read = lzma_read,
	.write = lzma_write,
	.unlocked_ioctl = lzma_ioctl,
};

static struct miscdevice jz_lzma_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "jz_lzma",
	.fops = &jz_lzma_ops,
};

irqreturn_t lzma0_handirq(int irq,void *data)
{
	unsigned long ret = 0;
	char channel = 0;
	ret = lzma_readl(LZMA_CTRL,channel);
	if( ret & (0x1)){
		printk("lzma0 is working,but receive interrupt\n");
	}else {
		complete(&lzma0_completion);
	}
	lzma_writel(0<<3, LZMA_CTRL,channel);//write 0 is clear interrupt
	return IRQ_HANDLED;
}

irqreturn_t lzma1_handirq(int irq,void *data)
{
	unsigned long ret = 0;
	char channel = 1;
	ret = lzma_readl(LZMA_CTRL,channel);
	if( ret & (0x1)){
		printk("lzma1 is working,but receive interrupt\n");
	}else {
		complete(&lzma1_completion);
	}
	lzma_writel(0<<3, LZMA_CTRL,channel);//write 0 is clear interrupt
	return IRQ_HANDLED;
}

static int jz_lzma_probe(struct platform_device *pdev)
{
	// request mem
	int ret = 0;
	int irq0 = 0,irq1;
	struct resource *res0 = NULL, *res1 = NULL;

	res0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!res0){
		printk("get resource0 fail\n");
		return -1;
	}

	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if(!res1){
		printk("get resource fail\n");
		return -1;
	}
	// printk("resource start:%#x,end:%#x, res1->start:%#x,res1->end:%#x\n",res0->start,res0->end,res1->start,res1->end);

	irq0 = platform_get_irq(pdev, 0);
	if(!irq0){
		printk("bad irq0 num\n");
		return -1;
	}

	irq1 = platform_get_irq(pdev, 1);
	if(!irq1){
		printk("bad irq1 num\n");
		return -1;
	}

	printk("irq0:%d,irq1:%d\n",irq0,irq1);

	lzma0_ioaddr = ioremap(res0->start, resource_size(res0));
	if (lzma0_ioaddr == NULL)
	{
		printk("[%s][%d] ioremap failed.\n",__FUNCTION__,__LINE__);
		goto ERROR_2;
	}

	lzma1_ioaddr = ioremap(res1->start, resource_size(res1));
	if (lzma1_ioaddr == NULL)
	{
		printk("[%s][%d] ioremap failed.\n",__FUNCTION__,__LINE__);
		goto ERROR_3;
	}

	ret = request_irq(irq0, lzma0_handirq, IRQF_SHARED, dev_name(&pdev->dev), &irq0);
	if(ret < 0){
		printk("request_irq err\n");
		goto ERROR_4;
	}
	ret = request_irq(irq1, lzma1_handirq, IRQF_SHARED, dev_name(&pdev->dev), &irq1);
	if(ret < 0){
		printk("request_irq err\n");
		goto ERROR_4;
	}

	// misc register
	ret = misc_register(&jz_lzma_misc);
	if (ret){
		goto ERROR_4;
	}

	cpm_ioaddr = ioremap(JZ_CPM_START_ADDRESS, 0XFF);
	if (cpm_ioaddr == NULL){
		printk("------ ioremap failed.\n");
		goto ERROR_2;
	}
	pgdev = pdev;
	printk("clkgr1:%#08x\n", readl(cpm_ioaddr + CPM_CLKGR1));
	writel(readl(cpm_ioaddr + CPM_CLKGR1) & (~(0x1 << 11)), cpm_ioaddr + CPM_CLKGR1);
	printk("clkgr1:%#08x\n", readl(cpm_ioaddr + CPM_CLKGR1));
	printk("----- ingenic_lzma probe ok.\n");
	return 0;

ERROR_4:
	iounmap(lzma1_ioaddr);

ERROR_3:
	iounmap(lzma0_ioaddr);

ERROR_2:
	iounmap(cpm_ioaddr);
	return -1;
}

static int jz_lzma_remove(struct platform_device *pdev)
{
	// misc unregister
	misc_deregister(&jz_lzma_misc);
	// io unmap
	iounmap(lzma0_ioaddr);
	iounmap(lzma1_ioaddr);
	iounmap(cpm_ioaddr);

	return 0;
}

#define LZMA_IOBASE_UNIT(ID) (JZ_LZMA_START_ADDRESS + 0x8000 * ID)

static struct resource lzma_resource[] =
{
	//lzma0
	[0] = {
		.start = LZMA_IOBASE_UNIT(0),
		.end   = LZMA_IOBASE_UNIT(1) - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = 58 + 8,
		.end = 58 + 8,
		.flags = IORESOURCE_IRQ,
	},
	//lzma1
	[2] = {
		.start = LZMA_IOBASE_UNIT(1),
		.end   = LZMA_IOBASE_UNIT(2) - 1,
		.flags = IORESOURCE_MEM,
	},
	[3] = {
		.start = 53 + 8,
		.end = 53 + 8,
		.flags = IORESOURCE_IRQ,
	},
};

static void lzma_release(struct device *dev)
{
	return ;
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
	.id = 0,
	.dev = {
		.release = lzma_release,
	},
	.num_resources = ARRAY_SIZE(lzma_resource),
	.resource = lzma_resource,
};

static int __init jz_lzma_mod_init(void)
{
	platform_device_register(&jz_lzma_device);
	return platform_driver_register(&jz_lzma_driver);
}

static void __exit jz_lzma_mod_exit(void)
{
	platform_driver_unregister(&jz_lzma_driver);
	platform_device_unregister(&jz_lzma_device);
}

module_init(jz_lzma_mod_init);
module_exit(jz_lzma_mod_exit);

MODULE_DESCRIPTION("lzma kernel hardware decompression.");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("oywei@ingenic.com");
MODULE_AUTHOR("jim.jwang@ingenic.com");
