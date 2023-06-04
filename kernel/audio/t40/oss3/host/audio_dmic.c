#include <dt-bindings/dma/ingenic-pdma.h>
#include "../include/audio_common.h"
#include "../include/audio_debug.h"
#include "../include/audio_dsp.h"
#include "audio_dmic.h"

static int dmic_gpio = 2;  //GPIO_A:0 GPIO_B:1 GPIO_C:2 GPIO_D:3
module_param(dmic_gpio, int, S_IRUGO);
MODULE_PARM_DESC(dmic_gpio, "dmic gpio port select.");

#define DMIC_PORTA_PINS 0x7<<12
#define DMIC_PORTC_PINS 0x7<<24


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

static int dmic_init(void *pipe)
{
	struct audio_dmic_device *dmic = NULL;
	struct dmic_device_info *info = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("route is null.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	dmic = (struct audio_dmic_device*)route->pipe->priv;
	info = dmic->info;

	//init parameters
	info->record_rate = DEFAULT_SAMPLERATE;
	info->record_channel = DEFAULT_CHANNEL;
	info->record_format = DEFAULT_FORMAT;
	info->gain = DEFAULT_GAIN;
	info->record_trigger = DEFAULT_TRIGGER;

	__dmic_reset(dmic);
	__dmic_set_gcr(dmic, info->gain);
	__dmic_set_request(dmic, info->record_trigger);
	__dmic_set_sr_8k(dmic);
	__dmic_set_chnum(dmic, info->record_channel);

	return 0;
}

static int set_dmic_samplerate(struct audio_dmic_device *dmic, void *data)
{
	unsigned int samplerate = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;

	if (dmic == NULL || data == NULL) {
		audio_err_print("dmic is null or data is null.parameter error!\n");
		return -1;
	}

	samplerate = param->rate;
	switch (samplerate) {
		case DMIC_RATE_8000:
			__dmic_set_sr_8k(dmic);
			break;

		case DMIC_RATE_16000:
			__dmic_set_sr_16k(dmic);
			break;

		case DMIC_RATE_48000:
			__dmic_set_sr_48k(dmic);
			break;

		default:
			audio_err_print("error dmic samplerate : %lu.\n", samplerate);
			return -1;
	}

	dmic->info->record_rate = samplerate;

	return 0;
}

static int set_dmic_format(struct audio_dmic_device *dmic, void *data)
{

	if (dmic == NULL || data == NULL) {
		audio_err_print("dmic is null or data is null.parameter error!\n");
		return -1;
	}

	//we only support 16bits mode, need no set
	audio_info_print("dmic set format. DMIC only support 16bits format!\n");

	return 0;
}

static int set_dmic_channel(struct audio_dmic_device *dmic, void *data)
{
	unsigned short channel = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;

	if (dmic == NULL || data == NULL) {
		audio_err_print("dmic is null or data is null.parameter error!\n");
		return -1;
	}

	channel = param->channel;
	if (channel <= 0 || channel > 4) {
		audio_err_print("error dmic channel : %d.\n", channel);
		return -1;
	}

	__dmic_set_chnum(dmic, channel-1);
	dmic->info->record_channel = channel;

	return 0;
}

static int set_dmic_param(struct audio_dmic_device *dmic, void *data)
{
	int ret = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;

	if (dmic == NULL || data == NULL) {
		audio_err_print("dmic is null or data is null.parameter is wrong!\n");
		return -1;
	}

	if (set_dmic_samplerate(dmic, param) != 0)
		ret |= -1;
	if (set_dmic_format(dmic, param) != 0)
		ret |= -1;
	if (set_dmic_channel(dmic, param) != 0)
		ret |= -1;

	return ret;
}

static int set_dmic_gain(struct audio_dmic_device *dmic, void *data)
{
	unsigned int gain = 0;
	if (dmic == NULL) {
		audio_err_print("dmic is null.\n");
		return -1;
	}

	gain = *(unsigned int*)data;
	if (gain < 0)
		gain = 0;
	else if (gain > MAX_GAIN)
		gain = MAX_GAIN;

	__dmic_set_gcr(dmic, gain);
	dmic->info->gain = gain;

	return 0;
}

static int set_dmic_trigger(struct audio_dmic_device *dmic, void *data)
{
	unsigned int trigger = 0;

	if (dmic == NULL) {
		audio_err_print("dmic is null.\n");
		return -1;
	}

	trigger = *(unsigned int*)data;
	if (trigger <= 0)
		trigger = 1;
	if (trigger >= MAX_TRIGGER)
		trigger = MAX_TRIGGER;

	__dmic_set_request(dmic, trigger);
	dmic->info->record_trigger = trigger;

	return 0;
}

static int set_dmic_start(struct audio_dmic_device *dmic, void *data)
{

	if (dmic == NULL) {
		audio_err_print("dmic is null.\n");
		return -1;
	}

	__dmic_reset(dmic);
	__dmic_unpack_msb(dmic);
	__dmic_unpack_dis(dmic);
	__dmic_enable_pack(dmic);

	__dmic_enable_rdms(dmic);

	__dmic_enable_hpf1(dmic);   //HPF

	__dmic_enable_lp(dmic);  //1.2MHz, default 2.4MHz
	__dmic_mask_all_int(dmic);
	__dmic_disable_sw_lr(dmic);

	__dmic_enable(dmic);

	return 0;
}

static int set_dmic_stop(struct audio_dmic_device *dmic, void *data)
{

	if (dmic == NULL) {
		audio_err_print("dmic is null.\n");
		return -1;
	}

	__dmic_disable_rdms(dmic);
	__dmic_disable(dmic);

	return 0;
}

static int dmic_ioctl(void *pipe, unsigned int cmd, void *data)
{
	int ret = 0;
	struct dmic_device_info *info = NULL;
	struct audio_dmic_device *dmic = NULL;
	struct audio_pipe *dmic_pipe = NULL;

	if (NULL == pipe) {
		audio_err_print("pipe is null.\n");
		return -1;
	}

	dmic_pipe = (struct audio_pipe *)pipe;
	dmic = (struct audio_dmic_device*)dmic_pipe->priv;
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

static int dmic_deinit(void *pipe)
{
	struct audio_dmic_device *dmic = NULL;
	struct dmic_device_info *info = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("route is null.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	dmic = (struct audio_dmic_device*)route->pipe->priv;
	info = dmic->info;

	info->record_rate = DEFAULT_SAMPLERATE;
	info->record_channel = DEFAULT_CHANNEL;
	info->record_format = DEFAULT_FORMAT;
	info->gain = DEFAULT_GAIN;
	info->record_trigger = DEFAULT_TRIGGER;

	return 0;
}

static bool dma_chan_filter(struct dma_chan *chan, void *filter_param)
{
	struct audio_pipe *pipe = filter_param;
	return (void*)pipe->dma_type == chan->private;
}

static int init_dma_config(struct audio_pipe *pipe, enum dma_data_direction direction, unsigned long iobase, int dma_type, struct device *dev)
{
	unsigned long lock_flags;
	dma_cap_mask_t mask;

	if (NULL == pipe) {
		audio_err_print("pipe is null.\n");
		return -EINVAL;
	}

	if (INGENIC_DMA_REQ_DMIC_RX != dma_type || DMA_FROM_DEVICE != direction) {
		audio_err_print("dma type or direction is invalid.\n");
		return -EINVAL;
	}

	pipe->dma_type = dma_type;
	pipe->dma_config.direction = DMA_DEV_TO_MEM;
	pipe->dma_config.src_addr = iobase + DMICDR;
	pipe->dma_config.src_maxburst = DMIC_DMA_MAX_BURSTSIZE;
	pipe->dma_config.dst_maxburst = DMIC_DMA_MAX_BURSTSIZE;
	pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;

	spin_lock_init(&pipe->pipe_lock);

	pipe->reservesize = PAGE_ALIGN(SND_DMIC_DMA_BUFFER_SIZE);
	pipe->vaddr = dma_alloc_noncoherent(dev, pipe->reservesize, &pipe->paddr, GFP_KERNEL | GFP_DMA);
	if (NULL == pipe->vaddr) {
		audio_err_print("failed to alloc dmic dma buffer.\n");
		return -ENOMEM;
	}

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	spin_lock_irqsave(&pipe->pipe_lock, lock_flags);
	pipe->is_trans = false;
	spin_unlock_irqrestore(&pipe->pipe_lock, lock_flags);

	pipe->dma_chan = dma_request_channel(mask, dma_chan_filter, (void*)pipe);
	if (NULL == pipe->dma_chan) {
		audio_err_print("failed to request dmic dma channel.\n");
		goto failed_request_channel;
	}
	dmaengine_slave_config(pipe->dma_chan, &pipe->dma_config);

	audio_info_print("init dev = %p, size = %d, pipe->vaddr = %p, pipe->paddr = 0x%08x.\n", dev, pipe->reservesize, pipe->vaddr, pipe->paddr);

	pipe->init = dmic_init;
	pipe->ioctl = dmic_ioctl;
	pipe->deinit = dmic_deinit;

	return 0;
failed_request_channel:
	dmam_free_noncoherent(dev, pipe->reservesize, pipe->vaddr, pipe->paddr);
	pipe->vaddr = NULL;
	return -1;
}

static void deinit_dma_config(struct audio_dmic_device *dmic, struct device *dev)
{
	unsigned long lock_flags;

	if (NULL == dmic) {
		audio_err_print("dmic is null.\n");
		return;
	}

	if (dmic->pipe) {
		spin_lock_irqsave(&dmic->pipe_lock, lock_flags);
		if (false != dmic->pipe->is_trans)  {
			dmaengine_terminate_all(dmic->pipe->dma_chan);
			dmic->pipe->is_trans = false;
		}
		spin_unlock_irqrestore(&dmic->pipe_lock, lock_flags);

		if (dmic->pipe->vaddr) {
			audio_info_print("deinit dev = %p, size = %d, pipe->vaddr = %p, pipe->paddr = 0x%08x.\n", dev, dmic->pipe->reservesize, dmic->pipe->vaddr, dmic->pipe->paddr);
		}
		dmic->pipe->vaddr = NULL;
		dma_release_channel(dmic->pipe->dma_chan);
		dmic->pipe->dma_chan = NULL;
	}
}

static int set_dmic_gpio_func(struct jz_gpio_func_def gpio_func)
{
	audio_info_print("dmic gpio port: %d, gpio_func: %d, gpio_pins= %lu\n", gpio_func.port, gpio_func.func, gpio_func.pins);
	jzgpio_set_func(gpio_func.port, gpio_func.func, gpio_func.pins);

	return 0;
}

static int dmic_probe(struct platform_device *pdev)
{
	//dmic probe
	struct clk *dmic_clk = NULL;
	struct audio_pipe *dmic_pipe = NULL;
	struct audio_dmic_device *dmic = NULL;
	struct dmic_device_info *dmic_info = NULL;

	dmic = (struct audio_dmic_device*)kzalloc(sizeof(struct audio_dmic_device), GFP_KERNEL);
	if (NULL == dmic) {
		audio_err_print("failed to kzallc dmic device.\n");
		return -ENOMEM;
	}

	dmic->io_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == dmic->io_res) {
		audio_err_print("failed to get dmic io resource.\n");
		goto err_dmic_get_ioresource;
	}

	if (NULL == devm_request_mem_region(&pdev->dev, dmic->io_res->start, resource_size(dmic->io_res), pdev->name)) {
		audio_err_print("failed to request dmic mem region.\n");
		goto err_dmic_request_ioresource;
	}

	dmic->iomem = devm_ioremap_nocache(&pdev->dev, dmic->io_res->start, resource_size(dmic->io_res));
	if (NULL == dmic->iomem) {
		audio_err_print("failed to remap dmic io memory.\n");
		goto err_dmic_ioremap;
	}

	dmic->dma_res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (NULL == dmic->dma_res) {
		audio_err_print("failed to get dmic dma resource.\n");
		goto err_dmic_get_dmaresource;
	}

	dmic_pipe = kzalloc(sizeof(struct audio_pipe), GFP_KERNEL);
	if (NULL == dmic_pipe) {
		audio_err_print("failed to kzalloc dmic pipe.\n");
		goto err_dmic_alloc_pipe;
	}
	//init dmic dma config
	if (0 != init_dma_config(dmic_pipe, DMA_FROM_DEVICE, dmic->io_res->start, INGENIC_DMA_REQ_DMIC_RX, &pdev->dev)) {
		audio_err_print("failed to init dmic dma config.\n");
		goto err_init_dmic_dma;
	}
	dmic_pipe->priv = dmic;
	dmic->pipe = dmic_pipe;

	dmic_info = kzalloc(sizeof(struct dmic_device_info), GFP_KERNEL);
	if (NULL == dmic_info) {
		audio_err_print("failed to kzalloc dmic info.\n");
		goto err_dmicinfo_alloc;
	}
	dmic->info = dmic_info;

	//set dmic gpio func
	if (dmic_gpio == 1) {
		dmic->dmic_gpio_func.port = GPIO_PORT_A;
		dmic->dmic_gpio_func.func = GPIO_FUNC_2;
		dmic->dmic_gpio_func.pins = DMIC_PORTA_PINS;
	} else if (dmic_gpio == 2) {
		dmic->dmic_gpio_func.port = GPIO_PORT_C;
		dmic->dmic_gpio_func.func = GPIO_FUNC_2;
		dmic->dmic_gpio_func.pins = DMIC_PORTC_PINS;
	}

	if (AUDIO_SUCCESS != set_dmic_gpio_func(dmic->dmic_gpio_func)) {
		audio_err_print("dmic gpio func set failed.\n");
		return -1;
	}

	//get clock
	dmic_clk = devm_clk_get(&pdev->dev, "gate_dmic");
	if (IS_ERR(dmic_clk)) {
		audio_err_print("failed to get dmic clock.\n");
		goto err_get_dmic_clk;
	}
	clk_prepare_enable(dmic_clk);
	dmic->clk = dmic_clk;

	platform_set_drvdata(pdev, dmic);
	if (AUDIO_SUCCESS != register_audio_pipe(dmic->pipe, AUDIO_ROUTE_DMIC_ID)) {
		audio_err_print("failed to register dmic pipe.\n");
		goto failed_register_dmic_pipe;
	}

	if (AUDIO_SUCCESS != register_audio_debug_ops("audio_dmic_info", &audio_dmic_debug_fops, dmic)) {
		audio_err_print("failed to register audio dmic debug proc file!");
		goto failed_register_dmic_pipe;
	}
	audio_info_print("dmic init success.\n");

	return 0;

failed_register_dmic_pipe:
	clk_put(dmic->clk);

err_get_dmic_clk:
	kfree(dmic_info);
err_dmicinfo_alloc:
err_init_dmic_dma:
	kfree(dmic_pipe);
err_dmic_alloc_pipe:
err_dmic_get_dmaresource:
	devm_iounmap(&pdev->dev, dmic->iomem);
err_dmic_ioremap:
	devm_release_mem_region(&pdev->dev, dmic->io_res->start, resource_size(dmic->io_res));
err_dmic_request_ioresource:
err_dmic_get_ioresource:
	kfree(dmic);
    return -EFAULT;
}

static int __exit dmic_remove(struct platform_device *pdev)
{
	struct audio_dmic_device *dmic = platform_get_drvdata(pdev);

	//set dmic gpio func
	if (dmic_gpio == 1) {
		dmic->dmic_gpio_func.port = GPIO_PORT_A;
		dmic->dmic_gpio_func.func = GPIO_FUNC_0;
		dmic->dmic_gpio_func.pins = DMIC_PORTA_PINS;
	} else if (dmic_gpio == 2) {
		dmic->dmic_gpio_func.port = GPIO_PORT_C;
		dmic->dmic_gpio_func.func = GPIO_FUNC_1;
		dmic->dmic_gpio_func.pins = DMIC_PORTC_PINS;
	}

	if (AUDIO_SUCCESS != set_dmic_gpio_func(dmic->dmic_gpio_func)) {
		audio_err_print("dmic gpio func set failed.\n");
		return -1;
	}
	//dma deinit
	deinit_dma_config(dmic, &pdev->dev);
	clk_disable(dmic->clk);
	clk_put(dmic->clk);

	kfree(dmic->info);
	kfree(dmic->pipe);
	devm_iounmap(&pdev->dev, dmic->iomem);
	devm_release_mem_region(&pdev->dev, dmic->io_res->start,resource_size(dmic->io_res));
	kfree(dmic);

	return 0;
}

struct platform_driver audio_dmic_driver = {
	.probe = dmic_probe,
	.remove = dmic_remove,
	.driver = {
		.name = "jz-dmic",
		.owner = THIS_MODULE,
	},
};
