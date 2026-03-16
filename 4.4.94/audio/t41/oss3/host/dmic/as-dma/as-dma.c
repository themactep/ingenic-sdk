#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/vmalloc.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/delay.h>
#include <linux/lcm.h>
#include <sound/soc.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <linux/types.h>
#include "as-dma.h"

struct ingenic_dmic_dma *globe_dmic_dma = NULL;
int ingenic_dmic_dma_start(void)
{
	struct ingenic_dmic_dma *dmic_dma = globe_dmic_dma;
	regmap_write(dmic_dma->dma_regmap, DSR(5), DSR_TT_INT|DSR_LTT_INT|DSR_LTT|DSR_TT);
	//regmap_update_bits(dmic_dma->dma_regmap, DCM(5), DCM_NDES|DCM_TIE, DCM_TIE);

	/* Note: DMA enable first FIFO enable, read operation
	 * to ensure that the DMA module work
	 * */
	regmap_write(dmic_dma->dma_regmap, DDA(5), dmic_dma->descs_phy[0]);

	//ingenic_as_fmtcov_enable(dev_id, true);

	regmap_write(dmic_dma->dma_regmap, DCR(5), DCR_CTE);
	regmap_write(dmic_dma->fifo_regmap, FCR(5), FCR_FIFO_EN);
	return 0;
}

int ingenic_dmic_dma_stop(void)
{
	struct ingenic_dmic_dma *dmic_dma = globe_dmic_dma;
	u32 val, timeout = 0xfff;

	regmap_update_bits(dmic_dma->dma_regmap, DCM(5), DCM_TIE, 0);
	regmap_write(dmic_dma->dma_regmap, DSR(5), DSR_TT_INT|DSR_LTT_INT|DSR_LTT|DSR_TT);

	regmap_write(dmic_dma->fifo_regmap, FCR(5), 0);

	regmap_write(dmic_dma->dma_regmap, DCR(5), 0);
	do {
		regmap_read(dmic_dma->dma_regmap, DSR(5), &val);
	} while ((val & DSR_RST_EN) && (--timeout));

	regmap_write(dmic_dma->dma_regmap, DCR(5), DCR_RESET);
	udelay(10);

	//ingenic_as_fmtcov_enable(dev_id, false);

	return 0;
}
dma_addr_t ingenic_dmic_get_dma_current_trans_addr(void)
{
        struct ingenic_dmic_dma *dmic_dma = globe_dmic_dma;
	    dma_addr_t dma_current_addr = readl_relaxed(dmic_dma->dma_base + DBA(5));
        return dma_current_addr;
}


static void ingenic_dmic_dma_cyclic_descs_init(struct ingenic_dmic_dma *dmic_dma,dma_addr_t dma_addr, unsigned int fragment_size)
{
	int i, tsz, ts_size;
	unsigned int frame_size = fragment_size;

	if (unlikely(!dmic_dma->dma_fth_quirk)) {
		tsz = ingenic_as_dma_get_tsz(dmic_dma->fifo_depth, frame_size, &ts_size);
	} else {
		ts_size = DCM_TSZ_MAX_WORD << 2;
		tsz = DCM_TSZ_MAX;
	}

     //printk("tsz = %d,frame_size = %d, ts_size = %d \n",tsz, frame_size, ts_size);
	for (i = 0; i < dmic_dma->desc_cnts; i++) {
		memset(dmic_dma->descs[i], 0, sizeof(dmic_dma->descs[i]));
	//	dmic_dma->descs[i]->link = dmic_dma->descs[i]->tie = dmic_dma->descs[i]->bai = 1;
		dmic_dma->descs[i]->link =1;
		dmic_dma->descs[i]->tie = 0;
		dmic_dma->descs[i]->bai = 1;
		dmic_dma->descs[i]->tsz = tsz;
		dmic_dma->descs[i]->dba = dma_addr + i * frame_size;
//		printk("dmic_dma->descs[%d]=%d\n",i, dma_addr + i * frame_size);
		dmic_dma->descs[i]->ndoa = *( dmic_dma->descs_phy + ((i + 1)%dmic_dma->desc_cnts));
//		printk("dmic_dma->descs[%d]->ndoa=%d\n",i, dmic_dma->descs[i]->ndoa);
		dmic_dma->descs[i]->dtc = (frame_size/ts_size);
	}
}

static int ingenic_dmic_dma_alloc_descs(struct ingenic_dmic_dma *dmic_dma, int cnt)
{
    int i = 0;
	for (i = 0; i < cnt; i++) {
		dmic_dma->descs[i] = dma_pool_alloc(dmic_dma->desc_pool, GFP_KERNEL, &dmic_dma->descs_phy[i]);
		if (!dmic_dma->descs[i])
			return -ENOMEM;
		dmic_dma->desc_cnts++;
	}

	return 0;
}
static void ingenic_dmic_dma_free_descs(struct ingenic_dmic_dma *dmic_dma)
{
	int i = 0;

	for (i = 0; i < dmic_dma->desc_cnts; i++) {
		dma_pool_free(dmic_dma->desc_pool, dmic_dma->descs[i], dmic_dma->descs_phy[i]);
		dmic_dma->descs[i] = NULL;
		dmic_dma->descs_phy[i] = 0;
	}
	dmic_dma->desc_cnts = 0;
}

int ingenic_dmic_dma_init(dma_addr_t dma_addr, unsigned int fragment_size,unsigned int fragment_cnt)
{
	struct ingenic_dmic_dma *dmic_dma = globe_dmic_dma;
	int ret = 0;
	ret = ingenic_dmic_dma_alloc_descs(dmic_dma,fragment_cnt);
	if (ret < 0)
		return ret;
	ingenic_dmic_dma_cyclic_descs_init(dmic_dma,dma_addr, fragment_size);
	//ret = ingenic_as_fmtcov_cfg(substream, params);
	if (ret < 0)
		return ret;
	if(fragment_size < 640)
		regmap_write(dmic_dma->fifo_regmap, FFR(5), FFR_FTH(16) | (FFR_FIFO_TD));
	else
		regmap_write(dmic_dma->fifo_regmap, FFR(5), FFR_FTH(32) | (FFR_FIFO_TD));

	return 0;
}
int ingenic_dmic_dma_deinit(void)
{
	struct ingenic_dmic_dma *dmic_dma = globe_dmic_dma;
	ingenic_dmic_dma_free_descs(dmic_dma);

	return 0;
}

static irqreturn_t ingenic_dmic_dma_irq_handler(int irq, void *dev_id)
{
//	printk("enter: xxxxxx interrupt\n");

	struct ingenic_dmic_dma *dmic_dma = (void *)dev_id;
	unsigned int dma_status, pending, pending1;
    int ch, i = 0;
	for (i = 0; i < dmic_dma->desc_cnts; i++) {
		printk("(dmic_dma->descs[i]->dtc=%d\n",dmic_dma->descs[i]->dtc);
	}
	regmap_read(dmic_dma->dma_regmap, AIPR, &pending);
	pending &= AIPR_DMA_INT_MSK;
	pending1 = pending;
       //清中断
	while((ch = ffs(pending1))) {
		ch -= 1;
		pending1 &= ~BIT(ch);
		regmap_read(dmic_dma->dma_regmap, DSR(5), &dma_status);
		regmap_write(dmic_dma->dma_regmap, DSR(5), dma_status);
		if (!(dma_status & DSR_TT_INT)){
			continue;
		}
	}
	dmic_dma->hw_ptr = readl_relaxed(dmic_dma->dma_base + DBA(5));
	return IRQ_HANDLED;
}


static int ingenic_dmic_dma_probe(struct platform_device *pdev)
{
	struct ingenic_dmic_dma *dmic_dma;
	int ret;
	//u32 fifo_depth;
	struct regmap_config regmap_config = {
		.reg_bits = 32,
		.reg_stride = 4,
		.val_bits = 32,
		.cache_type = REGCACHE_NONE,
	};
	dmic_dma = (struct ingenic_dmic_dma *)devm_kzalloc(&pdev->dev, sizeof(*dmic_dma), GFP_KERNEL);
	if (!dmic_dma)
		return -ENOMEM;

	dmic_dma->dev = &pdev->dev;
	spin_lock_init(&dmic_dma->lock);

	dmic_dma->dma_res= platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dmic_dma->dma_base = devm_ioremap_resource(&pdev->dev, dmic_dma->dma_res);
	if (IS_ERR(dmic_dma->dma_base))
		return PTR_ERR(dmic_dma->dma_base);
	regmap_config.max_register = resource_size(dmic_dma->dma_res) - 0x4;
	regmap_config.name = dmic_dma->dma_res->name;
	dmic_dma->dma_regmap = devm_regmap_init_mmio(&pdev->dev,
			dmic_dma->dma_base,
			&regmap_config);
	if (IS_ERR(dmic_dma->dma_regmap))
		return PTR_ERR(dmic_dma->dma_regmap);
	dmic_dma->fifo_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	dmic_dma->fifo_base = devm_ioremap_resource(&pdev->dev, dmic_dma->fifo_res);
	if (IS_ERR(dmic_dma->fifo_base))
		return PTR_ERR(dmic_dma->fifo_base);
	regmap_config.max_register = resource_size(dmic_dma->fifo_res) - 0x4;
	regmap_config.name = "dmic-fifo";
	dmic_dma->fifo_regmap = devm_regmap_init_mmio(&pdev->dev,
			dmic_dma->fifo_base,
			&regmap_config);
	if (IS_ERR(dmic_dma->fifo_regmap))
		return PTR_ERR(dmic_dma->fifo_regmap);

	dmic_dma->irq = platform_get_irq(pdev, 0);
	if (dmic_dma->irq < 0)
		return dmic_dma->irq;
#if 0
	dmic_dma->audio_clk = devm_clk_get(&pdev->dev, "gate_audio");
	if(IS_ERR_OR_NULL(dmic_dma->audio_clk)) {
		dev_err(&pdev->dev, "Failed to get gate_audio clk!\n");
	}
	clk_prepare_enable(dmic_dma->audio_clk);

#endif
	platform_set_drvdata(pdev, dmic_dma);
	//DMA 全局复位
	regmap_write(dmic_dma->dma_regmap, DGRR, DGRR_RESET);

	/*INIT FIFO*/
	dmic_dma->fifo_total_depth = INGENIC_DEF_FIFO_DEPTH;

	dmic_dma->dma_fth_quirk = false;
	dmic_dma->fifo_depth = 384;//256;
	//配置FIFO Address Depth.
	regmap_write(dmic_dma->fifo_regmap, FAS(5), dmic_dma->fifo_depth);

	ret = devm_request_threaded_irq(&pdev->dev, dmic_dma->irq, NULL,
			ingenic_dmic_dma_irq_handler, IRQF_SHARED|IRQF_ONESHOT,
			"dmic-dma", dmic_dma);
	if (ret){
		printk("request irq failure");
		return ret;
	}
	dmic_dma->desc_pool = dma_pool_create("dmic_dma_desc_pool", &pdev->dev,sizeof(struct ingenic_dmic_dma_desc),__alignof__(struct ingenic_dmic_dma_desc), 0);
	if (!dmic_dma->desc_pool)
		return -ENOMEM;
	globe_dmic_dma = dmic_dma;
	printk("dmic dma probe success!!!\n");
	return 0;
}


static int ingenic_dmic_dma_remove(struct platform_device *pdev)
{
	//struct ingenic_dmic_dma *as_dma = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int ingenic_dmic_dma_suspend(struct device *dev)
{
	return 0;
}

static int ingenic_dmic_dma_resume(struct device *dev)
{
	return 0;
}
#endif
static const struct dev_pm_ops ingenic_dmic_dma_pm_ops = {
    .suspend = ingenic_dmic_dma_suspend,
    .resume = ingenic_dmic_dma_resume,
};


struct platform_driver ingenic_dmic_dma_driver = {
	.probe = ingenic_dmic_dma_probe,
	.remove = ingenic_dmic_dma_remove,
	.driver = {
		.name = "dmic-dma",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &ingenic_dmic_dma_pm_ops,
#endif
	},
};
