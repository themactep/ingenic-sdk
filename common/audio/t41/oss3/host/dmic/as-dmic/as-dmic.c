#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <sound/tlv.h>
#include <sound/control.h>
#include <soc/gpio.h>
#ifdef CONFIG_KERNEL_4_4_94
#include <dt-bindings/dma/ingenic-pdma.h>
#endif
#ifdef CONFIG_KERNEL_3_10
#include <linux/io.h>
#endif
#include "../as-dbus/as-dbus.h"
#include "../as-fmtcov/as-fmtcov.h"
#include "../as-dma/as-dma.h"
#include "as-dmic.h"
#include "../../../include/audio_common.h"
#include "../../../include/audio_debug.h"
#include "../../../include/audio_dsp.h"

static int dmic_gpio = 1;  //GPIO_A:0 GPIO_B:1 GPIO_C:2 GPIO_D:3
module_param(dmic_gpio, int, S_IRUGO);
MODULE_PARM_DESC(dmic_gpio, "dmic gpio port select.");

#define DMIC_PORTA_PINS 0xd<<12
#define DMIC_PORTB_PINS 0x7<<28
struct audio_route *g_dmic_route = NULL;
static int set_dmic_gain(struct ingenic_dmic *dmic, void *data)
{
    unsigned int gain = 0;
    //printk("set dmic gain start:\n");
	if (dmic == NULL) {
		audio_err_print("dmic is null.\n");
		return -1;
	}

	gain = *(unsigned int*)data;
    //printk("set dmic gain start: gain =%d\n", gain);
	if (gain < 0)
		gain = 0;
	else if (gain > MAX_GAIN)
		gain = MAX_GAIN;
	/*gain: 0, ..., 1F*/
	regmap_write(dmic->regmap, DMIC_GCR, DMIC_SET_GAIN(gain));
	dmic->info->gain = gain;

    //printk("set dmic gain: end\n");
	return 0;
}
static int set_dmic_params_check(int channels, int rate, int fmt_width)
{
	if (channels < 1 && channels > 12){
		audio_err_print("error dmic channel : %d.\n", channels);
		return -1;
	}

	if ((rate != 8000) && (rate != 16000) && (rate != 48000) && (rate != 96000)){
		audio_err_print("error dmic samplerate : %lu.\n", rate);
		return -1;
	}

	if ((fmt_width != 16) && (fmt_width != 24)){
		audio_info_print("dmic set format is illegal. DMIC  support 16bits/24bit format!\n");
		return -1;
	}

	return 0;
}
static int set_dmic_param(struct ingenic_dmic *dmic, void *data)
{
	unsigned int samplerate = 0;
	unsigned int format = 0;
	unsigned short channel = 0;
	u32 dmic_cr = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;

	if (dmic == NULL || data == NULL) {
		audio_err_print("dmic is null or data is null.parameter is wrong!\n");
		return -1;
	}
       //ÅäÖÃdmic ²ÎÊý
	samplerate = param->rate;
	format = param->format;
	channel = param->channel;
    if (set_dmic_params_check(channel, samplerate, format))
		return -1;
	dmic_cr = DMIC_SET_CHL(channel);
	dmic_cr |= DMIC_SET_SR(samplerate);
	dmic_cr |= DMIC_SET_OSS(format);
	regmap_update_bits(dmic->regmap, DMIC_CR0, CHNUM_MASK|SR_MASK|OSS_MASK, dmic_cr);
	dmic->info->record_rate = samplerate;
	dmic->info->record_channel = channel;
	dmic->info->record_format = format;
	//ÅäÖÃfmtcov²ÎÊý
	ingenic_dmic_fmtcov_cfg(DMA5_ID, format, channel);
	return 0;
}

static int set_dmic_reset(struct ingenic_dmic *dmic)
{
	unsigned int val, time_out = 0xfff;

	regmap_update_bits(dmic->regmap, DMIC_CR0, DMIC_RESET, DMIC_RESET);
	do {
		regmap_read(dmic->regmap, DMIC_CR0, &val);
	} while((val & DMIC_RESET) && (--time_out));
	if(!time_out) {
	dev_err(dmic->dev, "DMIC reset fail ...\n");
		return -EIO;
	}
	return 0;
}

static int set_dmic_start(struct ingenic_dmic *dmic, void *data)
{
	if (dmic == NULL) {
		audio_err_print("dmic is null.\n");
		return -1;
	}

	//¿ªÆôdma
	ingenic_dmic_dma_start();
	//¿ªÆôfmtcov
	ingenic_dmic_fmtcov_enable(DMA5_ID, 1);
	//¿ªÆôdmic
    regmap_update_bits(dmic->regmap, DMIC_CR0, DMIC_EN, DMIC_EN);
	return 0;
}

static int set_dmic_stop(struct ingenic_dmic *dmic, void *data)
{

	if (dmic == NULL) {
		audio_err_print("dmic is null.\n");
		return -1;
	}
	//¹Ø±Õdmic
	regmap_update_bits(dmic->regmap, DMIC_CR0, DMIC_EN, 0);
	//¹Ø±Õfmtcov
	ingenic_dmic_fmtcov_enable(DMA5_ID, 0);
	//¹Ø±Õdma
	ingenic_dmic_dma_stop();
	return 0;
}

static int dmic_ioctl(void *pipe, unsigned int cmd, void *data)
{

	int ret = 0;

	struct dmic_device_info *info = NULL;
	struct ingenic_dmic *dmic = NULL;
	struct audio_pipe *dmic_pipe = NULL;

	if (NULL == pipe) {
		audio_err_print("pipe is null.\n");
		return -1;
	}

	dmic_pipe = (struct audio_pipe *)pipe;
	dmic = (struct ingenic_dmic*)dmic_pipe->priv;
	info = dmic->info;

	if (NULL == dmic || NULL == info) {
		audio_err_print("dmic is null or info is null.\n");
		return -1;
	}

	switch(cmd) {
		case AUDIO_CMD_CONFIG_PARAM:
			ret = set_dmic_param(dmic, data);
			break;

		case AUDIO_CMD_GET_GAIN:
			*(unsigned long *)data = info->gain;
			break;

		case AUDIO_CMD_SET_GAIN:
			ret = set_dmic_gain(dmic, data);
			break;

		case AUDIO_CMD_ENABLE_STREAM:
			ret = set_dmic_start(dmic, data);
			break;

		case AUDIO_CMD_DISABLE_STREAM:
			ret = set_dmic_stop(dmic, data);
			break;


		default:
			break;
	}

	return ret;
}
#if 0
/* debug audio dmic info */
static int audio_dmic_show(struct seq_file *m, void *v)
{
    int len = 0, i;
    struct dmic_device_info *info = NULL;
    struct audio_dmic_device *dmic = (struct audio_dmic_device*)(m->private);

    if (NULL == dmic) {
        audio_warn_print("error, dmic is null\n");
        return len;
    }

    info = dmic->info;
    if (NULL == info) {
        audio_warn_print("error, dmic info is null");
        return len;
    }

    seq_printf(m, "\nDmic attr list : \n");
    seq_printf(m, "The living rate of dmic : %lu\n", info->record_rate);
    seq_printf(m, "The living channel of dmic : %d\n", info->record_channel);
    seq_printf(m, "The living format of dmic : %d\n", info->record_format);
    seq_printf(m, "The living datatype of dmic : 16\n");
    seq_printf(m, "The living gain of dmic : %u\n", info->gain);

    //dump dmic regs
    printk("\nDump Dmic Regs:\n");
    for (i = 0  ; i <= 0x20; i+=4)
        printk("reg[%04x], value = %04x\n", i, readl(dmic->iomem + i));
    printk("reg[%04x], value = %04x\n", 0x30, readl(dmic->iomem + 0x30));
    printk("reg[%04x], value = %04x\n", 0x34, readl(dmic->iomem + 0x34));
    printk("reg[%04x], value = %04x\n", 0x38, readl(dmic->iomem + 0x38));
    printk("reg[%04x], value = %04x\n", 0x50, readl(dmic->iomem + 0x50));
    printk("Dump Dmic Regs End.\n");
    return len;
}

static int dump_audio_dmic_open(struct inode *inode, struct file *file)
{
    return single_open_size(file, audio_dmic_show, PDE_DATA(inode), 2048);
}

static struct file_operations audio_dmic_debug_fops = {
    .read = seq_read,
    .open = dump_audio_dmic_open,
    .llseek = seq_lseek,
    .release = single_release,
};

#endif

static int dmic_init(void *route)
{

	struct ingenic_dmic *dmic = NULL;
	struct dmic_device_info *info = NULL;
	struct audio_route *dmic_route = NULL;
    unsigned int val = 0;
	if (route == NULL) {
		audio_err_print("route is null.\n");
		return -1;
	}

	dmic_route = (struct audio_route*)route;
	g_dmic_route = dmic_route;
	dmic = (struct ingenic_dmic*)dmic_route->pipe->priv;
	info = dmic->info;

	//init parameters
	info->record_rate = DEFAULT_SAMPLERATE;
	info->record_channel = DEFAULT_CHANNEL;
	info->record_format = DEFAULT_FORMAT;
	info->gain = DEFAULT_GAIN;
	info->record_trigger = DEFAULT_TRIGGER;

	set_dmic_reset(dmic);

	regmap_update_bits(dmic->regmap, DMIC_CR0, HPF2_EN|LPF_EN|HPF1_EN,HPF2_EN|LPF_EN|HPF1_EN);
	/*gain: 0, ..., 1F*/
	regmap_write(dmic->regmap, DMIC_GCR, DMIC_SET_GAIN(DEFAULT_GAIN));
    regmap_read(dmic->regmap,DMIC_GCR, &val);
	//printk("val = %d",val);
	//dbus ³õÊ¼»¯
	 ingenic_dmic_dbus_init(DMA5_ID,DMIC_DEV_ID);
   // printk("dmic init end\n");
	return 0;
}


static int dmic_deinit(void *route)
{
#if 1
	struct ingenic_dmic *dmic = NULL;
	struct dmic_device_info *info = NULL;
	struct audio_route *dmic_route = NULL;

	if (route == NULL) {
		audio_err_print("route is null.\n");
		return -1;
	}

	dmic_route = (struct audio_route*)route;
	dmic = (struct ingenic_dmic*)dmic_route->pipe->priv;
	info = dmic->info;

	info->record_rate = DEFAULT_SAMPLERATE;
	info->record_channel = DEFAULT_CHANNEL;
	info->record_format = DEFAULT_FORMAT;
	info->gain = DEFAULT_GAIN;
	info->record_trigger = DEFAULT_TRIGGER;
#endif
	//dbus ·´³õÊ¼»¯
	ingenic_dmic_dbus_deinit(DMA5_ID);
	ingenic_dmic_dma_deinit();
	return 0;
}

static int init_dma_config(struct audio_pipe *pipe, enum dma_data_direction direction, unsigned long iobase,  struct device *dev)
{
	unsigned long lock_flags;

	if (NULL == pipe) {
		audio_err_print("pipe is null.\n");
		return -EINVAL;
	}

	if (DMA_FROM_DEVICE != direction) {
		audio_err_print("direction is invalid.\n");
		return -EINVAL;
	}

	spin_lock_init(&pipe->pipe_lock);

	pipe->reservesize = PAGE_ALIGN(SND_DMIC_DMA_BUFFER_SIZE);
	pipe->vaddr = dma_alloc_noncoherent(dev, pipe->reservesize, &pipe->paddr, GFP_KERNEL | GFP_DMA);
	if (NULL == pipe->vaddr) {
		audio_err_print("failed to alloc dmic dma buffer.\n");
		return -ENOMEM;
	}
	spin_lock_irqsave(&pipe->pipe_lock, lock_flags);
	pipe->is_trans = false;
	spin_unlock_irqrestore(&pipe->pipe_lock, lock_flags);

	audio_info_print("init dev = %p, size = %d, pipe->vaddr = %p, pipe->paddr = 0x%08x.\n", dev, pipe->reservesize, pipe->vaddr, pipe->paddr);

	pipe->init = dmic_init;
	pipe->ioctl = dmic_ioctl;
	pipe->deinit = dmic_deinit;
    return 0;
}
static void deinit_dma_config(struct ingenic_dmic *dmic, struct device *dev)
{
	unsigned long lock_flags;

	if (NULL == dmic) {
		audio_err_print("dmic is null.\n");
		return;
	}
	if (dmic->pipe) {
		spin_lock_irqsave(&dmic->pipe_lock, lock_flags);
		if (false != dmic->pipe->is_trans)  {
			dmic->pipe->is_trans = false;
		}
		spin_unlock_irqrestore(&dmic->pipe_lock, lock_flags);

		if (dmic->pipe->vaddr) {
			audio_info_print("deinit dev = %p, size = %d, pipe->vaddr = %p, pipe->paddr = 0x%08x.\n", dev, dmic->pipe->reservesize, dmic->pipe->vaddr, dmic->pipe->paddr);
		}
		dmic->pipe->vaddr = NULL;
	}
}

static int set_dmic_gpio_func(struct jz_gpio_func_def gpio_func)
{
	audio_info_print("dmic gpio port: %d, gpio_func: %d, gpio_pins= %lu\n", gpio_func.port, gpio_func.func, gpio_func.pins);
	jzgpio_set_func(gpio_func.port, gpio_func.func, gpio_func.pins);

	return 0;
}
#if 0
static int ingenic_dmic_clk_init(struct platform_device *pdev,
	struct ingenic_dmic *dmic)
{
	const char *name;

	of_property_read_string(dmic->dev->of_node, "ingenic,clk-name", &name);
	dmic->clk = devm_clk_get(dmic->dev, name);
	if(IS_ERR_OR_NULL(dmic->clk)) {
		dev_warn(dmic->dev, "Warning ... Failed to get %s!\n", name);
	}

	/* Setup dmic's Parent clk, fix to i2s2 */
	if(dmic->clk) {
		struct clk *parent;
		of_property_read_string(dmic->dev->of_node, "ingenic,clk-parent", &name);
		parent =  devm_clk_get(dmic->dev, name);
		if(IS_ERR_OR_NULL(parent)) {
			dev_warn(dmic->dev, "Warning ... Failed to get %s!\n", name);
		}
		clk_set_parent(dmic->clk, parent);
		devm_clk_put(dmic->dev, parent);
	}

	dmic->clk_gate = devm_clk_get(dmic->dev, "gate_dmic");
	if(IS_ERR_OR_NULL(dmic->clk_gate)) {
		dev_warn(dmic->dev, "Warning ... Failed to get gate_dmic!\n");
	}

	if(dmic->clk) {
		clk_set_rate(dmic->clk, 24000000);
		clk_prepare_enable(dmic->clk);
	}
	if(dmic->clk_gate) {
		clk_prepare_enable(dmic->clk_gate);
	}

	return 0;
}
#endif
extern struct platform_driver ingenic_dmic_dma_driver;
extern struct platform_driver ingenic_dmic_dbus_driver;
extern struct platform_driver ingenic_dmic_fmtcov_driver;
static int ingenic_dmic_platform_probe(struct platform_device *pdev)
{
    struct ingenic_dmic *dmic = NULL;
	struct audio_pipe *dmic_pipe = NULL;
	struct dmic_device_info *dmic_info = NULL;
	struct platform_device **subdevs = NULL;
	struct clk *dmic_clk = NULL;

	struct regmap_config regmap_config = {
		.reg_bits = 32,
		.reg_stride = 4,
		.val_bits = 32,
		.cache_type = REGCACHE_NONE,
	};
	dmic = (struct ingenic_dmic*)devm_kzalloc(&pdev->dev, sizeof(struct ingenic_dmic), GFP_KERNEL);
	if (NULL == dmic){
		audio_err_print("failed to kzallc dmic device.\n");
		return -1;
    }
#if 1
	dmic->io_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(NULL == dmic->io_res){
		audio_err_print("failed to get dmic io resource.\n");
		return -1;
	}

	dmic->io_base = devm_ioremap_resource(&pdev->dev, dmic->io_res);
	if (NULL == dmic->io_base){
		audio_err_print("failed to remap dmic io memory.\n");
		return -1;
	}

	regmap_config.max_register = resource_size(dmic->io_res) - 0x4;
	dmic->regmap = devm_regmap_init_mmio(&pdev->dev,dmic->io_base,&regmap_config);
	if (NULL == dmic->regmap){
		audio_err_print("failed to regmap dmic init_mmio.\n");
		return -1;
	}

	dmic_pipe = kzalloc(sizeof(struct audio_pipe), GFP_KERNEL);
	if (NULL == dmic_pipe) {
		audio_err_print("failed to kzalloc dmic pipe.\n");
		goto err_dmic_alloc_pipe;
	}

	dmic_pipe->priv = dmic;
	dmic->pipe = dmic_pipe;
	dmic->dev = &pdev->dev;

	dmic_info = kzalloc(sizeof(struct dmic_device_info), GFP_KERNEL);
	if (NULL == dmic_info) {
		audio_err_print("failed to kzalloc dmic info.\n");
		goto err_dmic_info_alloc;
	}
	dmic->info = dmic_info;

	//set dmic gpio func
	if (dmic_gpio == 0) {
		dmic->dmic_gpio_func.port = GPIO_PORT_A;
		dmic->dmic_gpio_func.func = GPIO_FUNC_2;
		dmic->dmic_gpio_func.pins = DMIC_PORTA_PINS;
	} else if (dmic_gpio == 1) {
		dmic->dmic_gpio_func.port = GPIO_PORT_B;
		dmic->dmic_gpio_func.func = GPIO_FUNC_0;
		dmic->dmic_gpio_func.pins = DMIC_PORTB_PINS;
	}

	if (0 != set_dmic_gpio_func(dmic->dmic_gpio_func)) {
		audio_err_print("dmic gpio func set failed.\n");
		goto err_set_gpio_func;
	}
	//init dmic dma config
	if(0 != init_dma_config(dmic_pipe, DMA_FROM_DEVICE, dmic->io_res->start,&pdev->dev)){
		audio_err_print("failed to init dmic dma config.\n");
		goto err_init_dma_config;
	}
	//ingenic_dmic_clk_init(pdev, dmic);
	//get clock
#ifdef CONFIG_KERNEL_4_4_94
	dmic_clk = devm_clk_get(&pdev->dev, "gate_dmic");
#endif
#ifdef CONFIG_KERNEL_3_10
	dmic_clk = devm_clk_get(&pdev->dev, "dmic");
#endif
	if (IS_ERR(dmic_clk)) {
		audio_err_print("failed to get dmic clock.\n");
		goto err_get_dmic_clk;
	}
	clk_prepare_enable(dmic_clk);
	dmic->clk = dmic_clk;
	platform_set_drvdata(pdev, dmic);

	subdevs = pdev->dev.platform_data;
	platform_driver_register(&ingenic_dmic_dma_driver);
	platform_device_register(subdevs[0]);
	platform_driver_register(&ingenic_dmic_dbus_driver);
	platform_device_register(subdevs[1]);
	platform_driver_register(&ingenic_dmic_fmtcov_driver);
	platform_device_register(subdevs[2]);
	if (0 != register_audio_pipe(dmic->pipe, AUDIO_ROUTE_DMIC_ID)) {
		audio_err_print("failed to register dmic pipe.\n");
		goto failed_register_dmic_pipe;
	}

//	if (AUDIO_SUCCESS != register_audio_debug_ops("audio_dmic_info", &audio_dmic_debug_fops, dmic)) {
//		audio_err_print("failed to register audio dmic debug proc file!");
//		goto failed_register_dmic_pipe;
//	}

	printk("dmic init success.\n");

	return 0;

failed_register_dmic_pipe:
	deinit_dma_config(dmic, &pdev->dev);
err_get_dmic_clk:
err_init_dma_config:
err_set_gpio_func:
	kfree(dmic_info);
err_dmic_info_alloc:
	kfree(dmic_pipe);
err_dmic_alloc_pipe:

	//devm_iounmap(&pdev->dev, dmic->io_base);
#endif
    return -1;
}

static int ingenic_dmic_platform_remove(struct platform_device *pdev)
{
	struct ingenic_dmic *dmic = platform_get_drvdata(pdev);
    struct platform_device **subdevs = NULL;
	//set dmic gpio func
	if (dmic_gpio == 0) {
		dmic->dmic_gpio_func.port = GPIO_PORT_A;
		dmic->dmic_gpio_func.func = GPIO_FUNC_2;
		dmic->dmic_gpio_func.pins = DMIC_PORTA_PINS;
	} else if (dmic_gpio == 1) {
		dmic->dmic_gpio_func.port = GPIO_PORT_B;
		dmic->dmic_gpio_func.func = GPIO_FUNC_0;
		dmic->dmic_gpio_func.pins = DMIC_PORTB_PINS;
	}

	if (AUDIO_SUCCESS != set_dmic_gpio_func(dmic->dmic_gpio_func)) {
		audio_err_print("dmic gpio func set failed.\n");
		return -1;
	}
    subdevs = pdev->dev.platform_data;
	platform_driver_unregister(&ingenic_dmic_dma_driver);
	platform_device_unregister(subdevs[0]);
	platform_driver_unregister(&ingenic_dmic_dbus_driver);
	platform_device_unregister(subdevs[1]);
	platform_driver_unregister(&ingenic_dmic_fmtcov_driver);
	platform_device_unregister(subdevs[2]);
	deinit_dma_config(dmic, &pdev->dev);
	//clk_disable(dmic->clk);
	//clk_put(dmic->clk);

	kfree(dmic->info);
	kfree(dmic->pipe);
	devm_iounmap(&pdev->dev, dmic->io_base);
	kfree(dmic);
	return 0;
}


#ifdef CONFIG_PM
static int ingenic_dmic_suspend(struct device *dev)
{
	return 0;
}

static int ingenic_dmic_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops ingenic_dmic_pm_ops = {
	.suspend = ingenic_dmic_suspend,
	.resume = ingenic_dmic_resume,
};

struct platform_driver ingenic_dmic_platform_driver = {
	.probe = ingenic_dmic_platform_probe,
	.remove = ingenic_dmic_platform_remove,
	.driver = {
		.name = "jz-dmic",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &ingenic_dmic_pm_ops,
#endif
	},
};

