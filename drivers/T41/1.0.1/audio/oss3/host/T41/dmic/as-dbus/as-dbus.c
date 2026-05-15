#include <linux/module.h>
#include <linux/device.h>
#include <linux/list.h>
#include <sound/soc.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/thermal.h>
#ifdef CONFIG_KERNEL_3_10
#include <linux/io.h>
#endif
#include "as-dbus.h"
#include "../../../../include/audio_debug.h"

static struct ingenic_dmic_dbus *globle_dmic_dbus = NULL;

static int ingenic_dbus_flush_fifo(struct ingenic_dmic_dbus *dmic_dbus,int is_out, int id)
{
	regmap_update_bits(dmic_dbus->regmap, BFCR0, BFCR0_DEV_TF_MSK|BFCR0_DEV_RF_MSK,
				is_out ? BFCR0_DEV_TF(id) : BFCR0_DEV_RF(id));
	return 0;
}

static struct ingenic_dbus_port *ingenic_dbus_get_port(struct ingenic_dmic_dbus *dmic_dbus,
		u8 dev_id, bool is_out)
{
	int idx;

	if (!dmic_dbus)
		return NULL;

	if (!is_out) {
		if (unlikely(dev_id >= LI_PORT_MAX_NUM))
			return NULL;
		idx = LI_DEVID_TO_IDX(dev_id);
	} else {
		if (unlikely(dev_id >= LO_PORT_MAX_NUM))
			return NULL;
		idx = LO_DEVID_TO_IDX(dev_id);
	}

	if (dmic_dbus->port[idx].vaild)
		return &dmic_dbus->port[idx];
	else
		return NULL;
}

static inline u8 ingenic_timeslot_alloc(struct ingenic_dmic_dbus *dmic_dbus)
{
	int id;
	id = find_first_zero_bit(dmic_dbus->slot_bitmap, SLOT_NUM);
	bitmap_set(dmic_dbus->slot_bitmap, id, 1);
	return id + 1;	/*skip unused slot id 0*/
}

static inline void ingenic_timeslot_free(struct ingenic_dmic_dbus *dmic_dbus, u8 id)
{
	if (!id)  /*skip unused slot id 0*/
		return;
	bitmap_clear(dmic_dbus->slot_bitmap, (id - 1), 1);
}

static int ingenic_dbus_request_timesolt(struct ingenic_dmic_dbus *dmic_dbus,int dma_dev_id, int dmic_dev_id)
{

	struct ingenic_dbus_port *lo, *li;
	unsigned long reg, sft;

	li = ingenic_dbus_get_port(dmic_dbus, dmic_dev_id, false);
	lo = ingenic_dbus_get_port(dmic_dbus, dma_dev_id, true);
	if (!li || !lo) {
		return -ENODEV;
	}

	/*config bus source timeslot*/
	li->timeslot = ingenic_timeslot_alloc(dmic_dbus);
	reg = ingenic_get_dbus_port_register(li->dev_id, &sft, false);
	regmap_update_bits(dmic_dbus->regmap, reg, BSORTT_MSK(sft), BSORTT(li->timeslot, sft));

	/*config bus target timeslot*/
	lo->timeslot = li->timeslot;
	reg = ingenic_get_dbus_port_register(lo->dev_id, &sft, true);
	regmap_update_bits(dmic_dbus->regmap, reg, BSORTT_MSK(sft), BSORTT(lo->timeslot, sft));

	/*enable timeslot*/
	regmap_update_bits(dmic_dbus->regmap, BTSET, BT_TSLOT(li->timeslot), BT_TSLOT(li->timeslot));

	list_add_tail(&lo->node, &li->head);
	lo->private = (void *)li;
	return 0;
}

static void ingenic_dbus_free_timesolt(struct ingenic_dmic_dbus *dmic_dbus,
		int dma_dev_id)
{
	struct ingenic_dbus_port *lo, *li;
	unsigned long reg, sft;

	lo = ingenic_dbus_get_port(dmic_dbus, dma_dev_id, true);
	if (!lo || !lo->timeslot)
		return;

	/*clear target device timeslot*/
	reg = ingenic_get_dbus_port_register(dma_dev_id, &sft, true);
	regmap_update_bits(dmic_dbus->regmap, reg, BSORTT_MSK(sft), 0);
	lo->timeslot = 0;
	list_del(&lo->node);

	li = (struct ingenic_dbus_port *)lo->private;
	if (li && list_empty(&li->head)) {
		/*clear source device timeslot*/
		reg = ingenic_get_dbus_port_register(li->dev_id, &sft, false);
		regmap_update_bits(dmic_dbus->regmap, reg, BSORTT_MSK(sft), 0);

		/*disable and free timeslot*/
		regmap_update_bits(dmic_dbus->regmap, BTCLR, BT_TSLOT(li->timeslot), BT_TSLOT(li->timeslot));

		ingenic_timeslot_free(dmic_dbus, li->timeslot);

		li->timeslot = 0;
	}
	lo->private = NULL;
}
#if 0
static void dump_dsp_register (struct ingenic_as_dsp *as_dsp)
{
	unsigned int val;

	printk("==>DSP\n");
	regmap_read(as_dsp->regmap, BTSR, &val);
	printk ("BTSR(0x%x):	0x%x\n", BTSR, val);
	regmap_read(as_dsp->regmap, BFSR, &val);
	printk ("BFSR(0x%x):	0x%x\n", BFSR, val);
	regmap_read(as_dsp->regmap, BFCR0, &val);
	printk ("BFCR0(0x%x):	0x%x\n", BFCR0, val);
	regmap_read(as_dsp->regmap, BFCR1, &val);
	printk ("BFCR1(0x%x):	0x%x\n", BFCR1, val);
	regmap_read(as_dsp->regmap, BFCR2, &val);
	printk ("BFCR2(0x%x):	0x%x\n", BFCR2, val);
	regmap_read(as_dsp->regmap, BST0, &val);
	printk ("BST0(0x%x):	0x%x\n", BST0, val);
	regmap_read(as_dsp->regmap, BST1, &val);
	printk ("BST1(0x%x):	0x%x\n", BST1, val);
	regmap_read(as_dsp->regmap, BST2, &val);
	printk ("BST2(0x%x):	0x%x\n", BST2, val);
	regmap_read(as_dsp->regmap, BTT0, &val);
	printk ("BTT0(0x%x):	0x%x\n", BTT0, val);
	regmap_read(as_dsp->regmap, BTT1, &val);
	printk ("BTT1(0x%x):	0x%x\n", BTT1, val);
}
#endif
 int ingenic_dmic_dbus_init(int dma_dev_id, int dmic_dev_id)
{
	struct ingenic_dmic_dbus *dmic_dbus;
	dmic_dbus = globle_dmic_dbus;
	//Ë¢ÐÂDMICºÍDMA5 FIFO
	ingenic_dbus_flush_fifo(dmic_dbus, false, dmic_dev_id);
	ingenic_dbus_flush_fifo(dmic_dbus, true, dma_dev_id);
	//·¢ËÍDBUS FIFO ÎÞÊý¾Ý·¢ËÍ0
	regmap_write(dmic_dbus->regmap, BFCR1, BFCR1_DEV_DBE_MSK);
	//½ÓÊÕDBUS FIFO Òç³ö×èÈû
	regmap_write(dmic_dbus->regmap, BFCR2, BFCR2_DEV_DBE_MSK);
	//»ñÈ¡·ÖÅätimesolt
	//ingenic_dbus_request_timesolt(dmic_dbus, DMA5_ID, DMIC_DEV_ID);
	ingenic_dbus_request_timesolt(dmic_dbus, dma_dev_id, dmic_dev_id);
	return 0;
}

int ingenic_dmic_dbus_deinit(int dma_dev_id)
{
	struct ingenic_dmic_dbus *dmic_dbus;
	dmic_dbus = globle_dmic_dbus;
	ingenic_dbus_free_timesolt(dmic_dbus, dma_dev_id);
	return 0;
}
static int ingenic_dmic_dbus_probe(struct platform_device *pdev)
{
	struct ingenic_dmic_dbus *dmic_dbus;
	unsigned int i = 0;
	struct regmap_config regmap_config = {
		.reg_bits = 32,
		.reg_stride = 4,
		.val_bits = 32,
		.cache_type = REGCACHE_NONE,
	};
	dmic_dbus = devm_kzalloc(&pdev->dev, sizeof(struct ingenic_dmic_dbus), GFP_KERNEL);
	if (!dmic_dbus){
		audio_err_print("failed to kzallc dbus device.\n");
		return -ENOMEM;
	}
	dmic_dbus->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(NULL == dmic_dbus->res){
		audio_err_print("failed to get dbus io resource.\n");
		return -1;
	}
	dmic_dbus->base_addr = devm_ioremap_resource(&pdev->dev, dmic_dbus->res);
	if (IS_ERR(dmic_dbus->base_addr)){
		audio_err_print("failed to get dbus ioremap resource.\n");
		return -1;
	}
	regmap_config.max_register = resource_size(dmic_dbus->res) - 0x4;
	dmic_dbus->regmap = devm_regmap_init_mmio(&pdev->dev, dmic_dbus->base_addr,
			&regmap_config);
	if (IS_ERR(dmic_dbus->regmap))
		return -1;
	dmic_dbus->dev = &pdev->dev;
	bitmap_zero(dmic_dbus->slot_bitmap, SLOT_NUM);
	/*init dsp port*/
	for (i = 0; i < LI_PORT_MAX_NUM; i++) {
		dmic_dbus->port[LI_DEVID_TO_IDX(i)].dev_id = i;
		dmic_dbus->port[LI_DEVID_TO_IDX(i)].is_out = false;
		INIT_LIST_HEAD(&dmic_dbus->port[LI_DEVID_TO_IDX(i)].head);
		if(i<15)
			dmic_dbus->port[LI_DEVID_TO_IDX(i)].vaild = true;
		else
			dmic_dbus->port[LI_DEVID_TO_IDX(i)].vaild = false;
	}

	for (i = 0; i < LO_PORT_MAX_NUM; i++) {
		dmic_dbus->port[LO_DEVID_TO_IDX(i)].dev_id = i;
		dmic_dbus->port[LO_DEVID_TO_IDX(i)].is_out = true;
		INIT_LIST_HEAD(&dmic_dbus->port[LO_DEVID_TO_IDX(i)].node);
		if(i<12)
			dmic_dbus->port[LO_DEVID_TO_IDX(i)].vaild = true;
		else
			dmic_dbus->port[LO_DEVID_TO_IDX(i)].vaild = false;
	}
	platform_set_drvdata(pdev, dmic_dbus);
	globle_dmic_dbus = dmic_dbus;
    printk("dbus init ok\n");
	return 0;

}

static int ingenic_dmic_dbus_remove(struct platform_device *pdev)
{
	struct ingenic_dmic_dbus *dbus = platform_get_drvdata(pdev);
 	devm_iounmap(&pdev->dev, dbus->base_addr);
	platform_set_drvdata(pdev, NULL);
	return 0;
}
#ifdef CONFIG_PM
static int ingenic_dmic_dbus_suspend(struct device *dev)
{
	return 0;
}

static int ingenic_dmic_dbus_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops ingenic_dmic_dbus_pm_ops = {
	.suspend = ingenic_dmic_dbus_suspend,
	.resume = ingenic_dmic_dbus_resume,
};

struct platform_driver ingenic_dmic_dbus_driver = {
	.probe = ingenic_dmic_dbus_probe,
	.remove = ingenic_dmic_dbus_remove,
	.driver = {
		.name = "dmic-dbus",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &ingenic_dmic_dbus_pm_ops,
#endif
	},
};
