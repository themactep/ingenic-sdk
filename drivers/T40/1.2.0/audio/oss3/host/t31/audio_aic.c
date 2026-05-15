/**
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/sound.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <linux/switch.h>
#include <linux/dma-mapping.h>
//#include <linux/soundcard.h>
#include <linux/wait.h>
#include <mach/jzsnd.h>
#include <soc/irq.h>
#include <soc/base.h>
#include <linux/i2c.h>
#include "audio_aic.h"
#include "../include/audio_common.h"
#include "../include/codec-common.h"
#include "../include/audio_dsp.h"
#include "../../include/audio_debug.h"

#define AUDIO_DRIVER_VERSION "V30-20201030a"
#define DEFAULT_EXCODEC_ADDR 0xff

static int mono_channel = 2;
module_param(mono_channel, int, S_IRUGO);
MODULE_PARM_DESC(mono_channel, "mic record channel select:1 left mono 2 right mono");

static int samplerate = 0;
module_param(samplerate, int, S_IRUGO);
MODULE_PARM_DESC(samplerate, "max samplerate you use");

static int i2c_bus = 0x01;
module_param(i2c_bus, int, S_IRUGO);
MODULE_PARM_DESC(i2c_bus, "the i2c bus of extern codec");

static int excodec_addr = DEFAULT_EXCODEC_ADDR;
module_param(excodec_addr, int, S_IRUGO);
MODULE_PARM_DESC(excodec_addr, "the i2c addr of extern codec");

static char excodec_name[I2C_NAME_SIZE];
module_param_string(excodec_name, excodec_name, sizeof(excodec_name), S_IRUGO);
MODULE_PARM_DESC(excodec_name, "the name of extern codec");

static struct audio_aic_device *globe_aic = NULL;

struct codec_sign_configure aic2codec_signs[4] = {
	/* I2S LR MODE */
	{
		.data = DATA_I2S_MODE,
		.seq = LEFT_FIRST_MODE,
		.sync = LEFT_LOW_RIGHT_HIGH,
		.rate2mclk = 256,
	},
	/* I2S RL MODE */
	{
		.data = DATA_I2S_MODE,
		.seq = RIGHT_FIRST_MODE,
		.sync = LEFT_LOW_RIGHT_HIGH,
		.rate2mclk = 256,
	},
	/* MSB-justified LR MODE */
	{
		.data = DATA_LEFT_JUSTIFIED_MODE,
		.seq = LEFT_FIRST_MODE,
		.sync = LEFT_HIGH_RIGHT_LOW,
		.rate2mclk = 256,
	},
	/* MSB-justified RL MODE */
	{
		.data = DATA_LEFT_JUSTIFIED_MODE,
		.seq = RIGHT_FIRST_MODE,
		.sync = LEFT_HIGH_RIGHT_LOW,
		.rate2mclk = 256,
	},
};

/* Load an i2c sub-device. */
static struct codec_attributes *i2c_new_subdev_board(struct i2c_adapter *adapter, struct i2c_board_info *info,
		const unsigned short *probe_addrs)
{
	struct codec_attributes * attrs = NULL;
	struct i2c_client *client;

	request_module(I2C_MODULE_PREFIX "%s", info->type);
	/* Create the i2c client */
	if (info->addr == 0)
		return NULL;
	else
		client = i2c_new_device(adapter, info);

	/* Note: by loading the module first we are certain that c->driver
	   will be set if the driver was found. If the module was not loaded
	   first, then the i2c core tries to delay-load the module for us,
	   and then c->driver is still NULL until the module is finally
	   loaded. This delay-load mechanism doesn't work if other drivers
	   want to use the i2c device, so explicitly loading the module
	   is the best alternative. */
	if (client == NULL || client->driver == NULL)
		goto error;

	/* Lock the module so we can safely get the tx_isp_subdev pointer */
	if (!try_module_get(client->driver->driver.owner))
		goto error;
	attrs = i2c_get_clientdata(client);

	/* Decrease the module use count to match the first try_module_get. */
	module_put(client->driver->driver.owner);
	return attrs;

error:
	/* If we have a client but no subdev, then something went wrong and
	   we must unregister the client. */
	if (client && attrs == NULL)
		i2c_unregister_device(client);
	return attrs;
}

static int extern_codec_register(struct audio_aic_device *aic, int i2c_adapter, char *name, int i2c_addr)
{
	struct codec_attributes * attrs = NULL;
	int ret = AUDIO_SUCCESS;

	struct i2c_adapter *adapter;
	struct i2c_board_info board_info;

	adapter = i2c_get_adapter(i2c_adapter);
	if (!adapter) {
		printk("Failed to get I2C adapter %d, deferring probe\n",
				i2c_adapter);
		return -EINVAL;
	}
	memset(&board_info, 0 , sizeof(board_info));
	memcpy(&board_info.type, name, I2C_NAME_SIZE);
	board_info.addr = i2c_addr;
	audio_info_print("excodec addr=%p, excodec_name=%s\n", board_info.addr, board_info.type);
	attrs = i2c_new_subdev_board(adapter, &board_info, NULL);
	if (IS_ERR_OR_NULL(attrs)) {
		i2c_put_adapter(adapter);
		printk("Failed to acquire subdev %s, deferring probe\n",
				name);
		return -EINVAL;
	}

	if(attrs->detect){
		ret = attrs->detect(attrs);
		if(ret != AUDIO_SUCCESS){
			struct i2c_client *client;
			struct i2c_adapter *adapter = client->adapter;
			if (adapter)
				i2c_put_adapter(adapter);
			i2c_unregister_device(client);
			return -EINVAL;
		}
	}

	aic->excodec = attrs;
	aic->livingcodec = attrs;
	audio_info_print("Registered extern codec %s\n", name);
	return AUDIO_SUCCESS;
}

static int extern_codec_release(struct audio_aic_device *aic)
{
	struct codec_attributes * attrs = aic->excodec;
	struct i2c_client *client = get_codec_devdata(attrs);
	struct i2c_adapter *adapter = client->adapter;

	if (adapter)
		i2c_put_adapter(adapter);
	i2c_unregister_device(client);

	if(globe_aic->livingcodec == attrs)
		globe_aic->livingcodec = NULL;
	return AUDIO_SUCCESS;
}

int inner_codec_register(struct codec_attributes *codec_attr)
{
	struct proc_dir_entry *codec_dir = NULL;
	struct proc_dir_entry *codec_file = NULL;

	if(globe_aic){
		globe_aic->incodec = codec_attr;
		if(!globe_aic->livingcodec)
			globe_aic->livingcodec = codec_attr;

		/* register codec debug proc */
		if (NULL != codec_attr->debug_ops){
			codec_dir = jz_proc_mkdir("codecs");
			if (!codec_dir)
				return -ENOMEM;
			codec_file = proc_create("codec", 0, codec_dir,
						codec_attr->debug_ops);
			if (!codec_file) {
				printk("%s,%d: codec proc file create error!\n", __func__, __LINE__);
				return -1;
			}
			globe_aic->codec_dir = codec_dir;
			globe_aic->codec_file = codec_file;
		}
		return AUDIO_SUCCESS;
	}
	audio_warn_print("aic hasn't been inited!\n");
	return -EPERM;
}

int inner_codec_release(struct codec_attributes *codec_attr)
{
	if(globe_aic){
		globe_aic->incodec = NULL;
		if(globe_aic->livingcodec == codec_attr)
			globe_aic->livingcodec = NULL;
	}
	return AUDIO_SUCCESS;
}

/* dump aic controller registers */
static void dump_aic_regs(void)
{
	printk("T31 AIC registers list:\n");
	printk("AICFR: %04x\n" ,readl(globe_aic->i2s_iomem + 0));
	printk("AICCR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x4));
	printk("I2SCR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x10));
	printk("AICSR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x14));
	printk("I2SSR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x1c));
	printk("I2SDIV: %04x\n" ,readl(globe_aic->i2s_iomem + 0x30));
	printk("AICDR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x34));
	printk("AICLR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x38));
	printk("AICTFLR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x3c));
}

/* debug audio aic info */
static int audio_aic_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct codec_info *info = NULL;
	struct audio_aic_device *aic = (struct audio_aic_device*)(m->private);

	if (NULL == aic) {
		audio_warn_print("error, aic is null\n");
		return len;
	}

	len += seq_printf(m ,"\nThe version of audio driver is : %s\n", AUDIO_DRIVER_VERSION);

	info = aic->codec_mic_info;
	if (NULL == info) {
		audio_warn_print("error, codec mic info is null\n");
		return len;
	}
	len += seq_printf(m, "Record attr list : \n");
	len += seq_printf(m, "The living rate of record : %lu\n", info->rate);
	len += seq_printf(m, "The living channel of record : %d\n", info->channel);
	len += seq_printf(m, "The living format of record : %d\n", info->format);
	len += seq_printf(m, "The living datatype of record : %u\n", info->data_type.frame_vsize);
	len += seq_printf(m, "The living again of record : %d\n", info->again);
	len += seq_printf(m, "The living dgain of record : %d\n", info->dgain);

	info = aic->codec_spk_info;
	if (NULL == info) {
		audio_warn_print("error, codec spk info is null\n");
		return len;
	}
	len += seq_printf(m, "Replay attr list : \n");
	len += seq_printf(m, "The living rate of replay : %lu\n", info->rate);
	len += seq_printf(m, "The living channel of replay : %d\n", info->channel);
	len += seq_printf(m, "The living format of replay : %d\n", info->format);
	len += seq_printf(m, "The living datatype of replay : %u\n", info->data_type.frame_vsize);
	len += seq_printf(m, "The living again of replay : %d\n", info->again);
	len += seq_printf(m, "The living dgain of replay : %d\n", info->dgain);

	dump_aic_regs();

	return len;
}

static int dump_audio_aic_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, audio_aic_show, PDE_DATA(inode), 2048);
}

static struct file_operations audio_aic_debug_fops = {
	.read = seq_read,
	.open = dump_audio_aic_open,
	.llseek = seq_lseek,
	.release = single_release,
};

static irqreturn_t aic_irq_handler(int irq, void *dev_id)
{
	irqreturn_t ret = IRQ_HANDLED;

	return ret;
}

static int init_pipe(struct audio_pipe *pipe, enum dma_data_direction direction, unsigned long iobase, int dma_type)
{
	if (NULL == pipe || (direction != DMA_FROM_DEVICE && direction != DMA_TO_DEVICE))
		return -1;

	pipe->dma_type = dma_type;
	pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;

	if (DMA_FROM_DEVICE == direction) {
		pipe->dma_config.direction = DMA_DEV_TO_MEM;
		pipe->dma_config.src_maxburst = AIC_RX_FIFO_DEPTH;
		pipe->dma_config.dst_maxburst = AIC_RX_FIFO_DEPTH;
		if (JZDMA_REQ_I2S0 == dma_type)
			pipe->dma_config.src_addr = iobase + AICDR;
		else if (JZDMA_REQ_AEC == dma_type)
			pipe->dma_config.src_addr = iobase + AICLR;
		pipe->dma_config.dst_addr = 0;
	} else {
		pipe->dma_config.direction = DMA_MEM_TO_DEV;
		pipe->dma_config.src_maxburst = AIC_TX_FIFO_DEPTH;
		pipe->dma_config.dst_maxburst = AIC_TX_FIFO_DEPTH;
		pipe->dma_config.dst_addr = iobase + AICDR;
		pipe->dma_config.src_addr = 0;
	}

	spin_lock_init(&pipe->pipe_lock);

	return 0;
}

static bool dma_chan_filter(struct dma_chan *chan, void *filter_param)
{
	struct audio_pipe *pipe = filter_param;
	return (void*)pipe->dma_type == chan->private;
}

static int pipe_request_dma(struct audio_pipe *pipe, struct device *dev)
{
	int ret = 0;
	dma_cap_mask_t mask;
	unsigned long lock_flags;

	if (samplerate == 0)
		pipe->reservesize = PAGE_ALIGN(SND_DSP_DMA_BUFFER_SIZE);
	else
		pipe->reservesize = PAGE_ALIGN(samplerate*MAX_SAMPLE_BYTES*MAX_SAMPLE_CHANNEL);
	pipe->vaddr = dma_alloc_noncoherent(dev, pipe->reservesize, &pipe->paddr, GFP_KERNEL | GFP_DMA);
	if (NULL == pipe->vaddr) {
		audio_err_print("failed to alloc dma buffer.\n");
		return -ENOMEM;
	}

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	spin_lock_irqsave(&pipe->pipe_lock, lock_flags);
	pipe->is_trans = false;
	spin_unlock_irqrestore(&pipe->pipe_lock, lock_flags);

	pipe->dma_chan = dma_request_channel(mask, dma_chan_filter, (void*)pipe);
	if (NULL == pipe->dma_chan) {
		audio_err_print("dma channel request failed.\n");
		ret = -ENXIO;
		goto failed_request_channel;
	}
	dmaengine_slave_config(pipe->dma_chan, &pipe->dma_config);

	audio_info_print("init dev = %p, size = %d, pipe->vaddr = %p, pipe->paddr = 0x%08x.\n", dev, pipe->reservesize, pipe->vaddr, pipe->paddr);

	return 0;

failed_request_channel:
	dmam_free_noncoherent(dev, pipe->reservesize, pipe->vaddr, pipe->paddr);
	pipe->vaddr = NULL;

	return ret;
}

static void pipe_release_dma(struct audio_aic_device *aic, struct device *dev)
{
	unsigned long lock_flags;

	if (NULL == aic) {
		audio_err_print("aic is null\n");
		return;
	}

	if (aic->mic_pipe->dma_chan) {
		spin_lock_irqsave(&aic->mic_pipe->pipe_lock, lock_flags);
		if (false != aic->mic_pipe->is_trans) {
			dmaengine_terminate_all(aic->mic_pipe->dma_chan);
			aic->mic_pipe->is_trans = false;
		}
		spin_unlock_irqrestore(&aic->mic_pipe->pipe_lock, lock_flags);

		if (aic->mic_pipe->vaddr) {
			audio_info_print("deinit dev = %p, size = %d, dp->vaddr = %p, dp->paddr = 0x%08x.\n", dev, aic->mic_pipe->reservesize, aic->mic_pipe->vaddr, aic->mic_pipe->paddr);
			dmam_free_noncoherent(dev, aic->mic_pipe->reservesize, aic->mic_pipe->vaddr, aic->mic_pipe->paddr);
		}
		aic->mic_pipe->vaddr = NULL;
		dma_release_channel(aic->mic_pipe->dma_chan);
		aic->mic_pipe->dma_chan = NULL;
	}

	if (aic->spk_pipe->dma_chan) {
		spin_lock_irqsave(&aic->spk_pipe->pipe_lock, lock_flags);
		if (false != aic->spk_pipe->is_trans) {
			dmaengine_terminate_all(aic->spk_pipe->dma_chan);
			aic->spk_pipe->is_trans = false;
		}
		spin_unlock_irqrestore(&aic->spk_pipe->pipe_lock, lock_flags);

		if (aic->spk_pipe->vaddr) {
			audio_info_print("deinit dev = %p, size = %d, dp->vaddr = %p, dp->paddr = 0x%08x.\n", dev, aic->spk_pipe->reservesize, aic->spk_pipe->vaddr, aic->spk_pipe->paddr);
			dmam_free_noncoherent(dev, aic->spk_pipe->reservesize, aic->spk_pipe->vaddr, aic->spk_pipe->paddr);
		}
		aic->spk_pipe->vaddr = NULL;
		dma_release_channel(aic->spk_pipe->dma_chan);
		aic->spk_pipe->dma_chan = NULL;
	}

	if (aic->aec_pipe->dma_chan) {
		spin_lock_irqsave(&aic->aec_pipe->pipe_lock, lock_flags);
		if (false != aic->aec_pipe->is_trans) {
			dmaengine_terminate_all(aic->aec_pipe->dma_chan);
			aic->aec_pipe->is_trans = false;
		}
		spin_unlock_irqrestore(&aic->aec_pipe->pipe_lock, lock_flags);

		if (aic->aec_pipe->vaddr) {
			audio_info_print("deinit dev = %p, size = %d, dp->vaddr = %p, dp->paddr = 0x%08x.\n", dev, aic->aec_pipe->reservesize, aic->aec_pipe->vaddr, aic->aec_pipe->paddr);
			dmam_free_noncoherent(dev, aic->aec_pipe->reservesize, aic->aec_pipe->vaddr, aic->aec_pipe->paddr);
		}
		aic->aec_pipe->vaddr = NULL;
		dma_release_channel(aic->aec_pipe->dma_chan);
		aic->aec_pipe->dma_chan = NULL;
	}
}

/* mic operations */
static int mic_init(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("route is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_mic_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}

	//init parameters
	cur_codec->rate = DEFAULT_SAMPLERATE;
	/*
	 * The value of format reference are:
	 * AFMT_U8     0x00000001
	 * AFMT_S8     0x00000002
	 * AFMT_S16LE  0x00000010
	 * AFMT_S16BE  0x00000011
	 * */
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->again = DEFAULT_AGAIN;
	cur_codec->dgain = DEFAULT_DGAIN;
	cur_codec->alc_en = DEFAULT_ALC_EN;
	cur_codec->trigger = DEFAULT_RECORD_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

static int set_codec_mic_datatype(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;
	struct audio_data_type data_type;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	data_type.sample_rate = param->rate;
	data_type.sample_channel = param->channel;
	data_type.frame_vsize = param->format;
	data_type.frame_size = 32;
	if (data_type.sample_channel != MONO) {
		audio_err_print("We only support record mono channel now.\n");
		return -EINVAL;
	}

	//set samplerate
	if (CODEC_IS_0_LINES == aic->livingcodec->pins || CODEC_IS_6_LINES == aic->livingcodec->pins) {
		clk_set_rate(aic->mic_clock, data_type.sample_rate*256);
		clk_enable(aic->mic_clock);
	} else if (CODEC_IS_4_LINES == aic->livingcodec->pins) {
		clk_set_rate(aic->spk_clock, data_type.sample_rate*256);
		clk_enable(aic->spk_clock);
	}
	aic->codec_mic_info->rate = data_type.sample_rate;
	audio_info_print("audio mic clock rate: %d\n", data_type.sample_rate*256);

	//set channel
	if (MONO == data_type.sample_channel) {
		if (mono_channel == 2)
			__i2s_enable_monoctr_right(aic);
		else if (mono_channel == 1)
			__i2s_enable_monoctr_left(aic);
	}
	else if(STEREO == data_type.sample_channel)
		__i2s_enable_stereo(aic);
	aic->codec_mic_info->channel = data_type.sample_channel;

	//set format
	if (VALID_8BIT == data_type.frame_vsize) {
		__i2s_set_iss_sample_size(aic, 0);
		aic->mic_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		aic->mic_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	} else if (VALID_16BIT == data_type.frame_vsize) {
		__i2s_set_iss_sample_size(aic, 1);
		aic->mic_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		aic->mic_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else if (VALID_20BIT == data_type.frame_vsize) {
		__i2s_set_iss_sample_size(aic, 3);
		aic->mic_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->mic_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	} else if (VALID_24BIT == data_type.frame_vsize){
		__i2s_set_iss_sample_size(aic, 4);
		aic->mic_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->mic_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}
	__i2s_disable_signadj(aic);
	//set trigger? default value is 8
	//__i2s_set_receive_trigger(aic, trigger_value);
	aic->codec_mic_info->format = data_type.frame_vsize;

	//codec set data_type
	ret = aic->livingcodec->record->set_datatype(&data_type);

	aic->codec_mic_info->data_type = data_type;

	return ret;
}

static int set_codec_mic_protocol(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct codec_sign_configure *protocol = (struct codec_sign_configure*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	if (DATA_I2S_MODE == protocol->data)
		__i2s_select_i2s(aic);
	else if (DATA_RIGHT_JUSTIFIED_MODE == protocol->data || DATA_LEFT_JUSTIFIED_MODE == protocol->data)
		__i2s_select_msbjustified(aic);

	ret = aic->livingcodec->set_sign(protocol);

	aic->codec_mic_info->transfer_info = *protocol;

	return ret;
}

static int set_codec_mic_again(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume gain = *(struct volume*)data;
	struct codec_analog_gain again;

	if (aic->codec_mic_info->alc_en == 1)
		again.lmode = CODEC_ANALOG_AUTO;
	else
		again.lmode = CODEC_ANALOG_MANUAL;

	again.gain = gain;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	ret = aic->livingcodec->record->set_again(&again);

	aic->codec_mic_info->again = gain.gain[0];

	return ret;
}

static int set_codec_mic_dgain(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume dgain = *(struct volume*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	ret = aic->livingcodec->record->set_dgain(dgain);

	aic->codec_mic_info->dgain = dgain.gain[0];
	return ret;
}

static int set_codec_mic_start(struct audio_aic_device *aic, void *data)
{
	int ret = 0, val;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	__i2s_enable(aic);
	ret = aic->livingcodec->record->set_endpoint(CODEC_MIC_START);

	{
		__i2s_flush_rfifo(aic);
		val = __i2s_read_rfifo(aic);
		__i2s_enable_receive_dma(aic);
		__i2s_enable_record(aic);
		__i2s_enable_tloop(aic);
		__i2s_enable_etfl(aic);
	}

	return ret;
}

static int set_codec_mic_stop(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	//disable aic record bit
	__i2s_disable_record(aic);
	__i2s_disable_receive_dma(aic);
	__i2s_disable_tloop(aic);
	__i2s_disable_etfl(aic);
	__i2s_enable_stereo(aic);

	//mic stop
	ret = aic->livingcodec->record->set_endpoint(CODEC_MIC_STOP);

	return ret;
}

static int mic_ioctl(void *pipe, unsigned int cmd, void *data)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_pipe *mic_pipe = NULL;
	int ret = 0;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	mic_pipe = (struct audio_pipe *)pipe;
	aic = (struct audio_aic_device*)mic_pipe->priv;;
	cur_codec = aic->codec_mic_info;

	if (NULL == aic || NULL == cur_codec) {
		audio_err_print("aic is null or cur_codec is null.\n");
		return -1;
	}

	/*set parameters*/
	switch(cmd) {
		case AUDIO_CMD_CONFIG_PARAM:
			ret = set_codec_mic_datatype(aic, data);
			break;

		case AUDIO_CMD_GET_GAIN:
			{
				struct volume *vol= (struct volume*)data;
				vol->gain[0] = cur_codec->again;
				break;
			}

		case AUDIO_CMD_GET_VOLUME:
			{
				struct volume *vol= (struct volume*)data;
				vol->gain[0] = cur_codec->dgain;
				break;
			}

		case AUDIO_CMD_ENABLE_ALC_L:
			cur_codec->alc_en = *(int*)data;
			break;

		case AUDIO_CMD_SET_GAIN:
			ret = set_codec_mic_again(aic, data);
			break;

		case AUDIO_CMD_ENABLE_STREAM:
			ret = set_codec_mic_start(aic, data);
			break;

		case AUDIO_CMD_DISABLE_STREAM:
			ret = set_codec_mic_stop(aic, data);
			break;


	default:
		break;
	}


	return ret;
}

static int mic_deinit(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_mic_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("error, livingcodec is NULL.\n");
		return -1;
	}
	if (NULL == cur_codec) {
		audio_err_print("error, codec mic info is null\n");
		return -1;
	}
	cur_codec->rate = DEFAULT_SAMPLERATE;
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->again = DEFAULT_AGAIN;
	cur_codec->dgain = DEFAULT_DGAIN;
	cur_codec->alc_en = DEFAULT_ALC_EN;
	cur_codec->trigger = DEFAULT_RECORD_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

/* spk operations */
static int spk_init(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_spk_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}

	//init parameters
	cur_codec->rate = DEFAULT_SAMPLERATE;
	/*
	 * The value of format reference are:
	 * AFMT_U8     0x00000001
	 * AFMT_S8     0x00000002
	 * AFMT_S16LE  0x00000010
	 * AFMT_S16BE  0x00000011
	 * */
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->again = DEFAULT_AGAIN;
	cur_codec->dgain = DEFAULT_DGAIN;
	cur_codec->trigger = DEFAULT_REPLAY_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

static int set_codec_spk_datatype(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;
	struct audio_data_type data_type;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	data_type.sample_rate = param->rate;
	data_type.sample_channel = param->channel;
	data_type.frame_vsize = param->format;
	data_type.frame_size = 32;
	if (data_type.sample_channel != MONO) {
		audio_err_print("We only support replay mono channel now.\n");
		return -EINVAL;
	}

	//set samplerate
	clk_set_rate(aic->spk_clock, data_type.sample_rate*256);
	clk_enable(aic->spk_clock);
	aic->codec_spk_info->rate = data_type.sample_rate;
	audio_info_print("audio spk clock rate: %d\n", data_type.sample_rate*256);

	//set channel
	__i2s_out_channel_select(aic, MONO-1);
	__i2s_enable_mono2stereo(aic);
	aic->codec_spk_info->channel = data_type.sample_channel;

	//set format
	if (VALID_8BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 0);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	} else if (VALID_16BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 1);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else if (VALID_20BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 3);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	} else if (VALID_24BIT == data_type.frame_vsize){
		__i2s_set_oss_sample_size(aic, 4);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}
	__i2s_disable_byteswap(aic);
	//set trigger? default value is 8
	//__i2s_set_receive_trigger(aic, trigger_value);
	aic->codec_spk_info->format = data_type.frame_vsize;

	//codec set data_type
	ret = aic->livingcodec->playback->set_datatype(&data_type);

	aic->codec_spk_info->data_type = data_type;

	return ret;
}

static int set_codec_spk_protocol(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct codec_sign_configure *protocol = (struct codec_sign_configure*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	if (DATA_I2S_MODE == protocol->data)
		__i2s_select_i2s(aic);
	else if (DATA_RIGHT_JUSTIFIED_MODE == protocol->data || DATA_LEFT_JUSTIFIED_MODE == protocol->data)
		__i2s_select_msbjustified(aic);

	ret = aic->livingcodec->set_sign(protocol);

	aic->codec_spk_info->transfer_info = *protocol;

	return ret;
}

static int set_codec_spk_again(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume gain = *(struct volume*)data;
	struct codec_analog_gain again;

	again.gain = gain;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	ret = aic->livingcodec->playback->set_again(&again);

	aic->codec_spk_info->again = gain.gain[0];

	return ret;
}

static int set_codec_spk_dgain(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume dgain = *(struct volume*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	ret = aic->livingcodec->playback->set_dgain(dgain);

	aic->codec_spk_info->dgain = dgain.gain[0];
	return ret;
}

static int set_codec_spk_start(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	__i2s_enable(aic);
	ret = aic->livingcodec->playback->set_endpoint(CODEC_SPK_START);

	{
		__i2s_flush_tfifo(aic);
		__i2s_enable_transmit_dma(aic);
		__i2s_enable_replay(aic);
	}

	return ret;
}

static int set_codec_spk_stop(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	__i2s_disable_transmit_dma(aic);
	__i2s_disable_replay(aic);

	{
		//avoid pop
		__i2s_write_tfifo(aic, 0);
		__i2s_write_tfifo(aic, 0);
		__i2s_write_tfifo(aic, 0);
		__i2s_write_tfifo(aic, 0);
		__i2s_enable_replay(aic);
		msleep(2);
		__i2s_disable_replay(aic);
	}

	ret = aic->livingcodec->playback->set_endpoint(CODEC_SPK_STOP);

	return ret;
}

static int spk_ioctl(void *pipe, unsigned int cmd, void *data)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_pipe *spk_pipe = NULL;
	int ret = 0;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	spk_pipe = (struct audio_pipe *)pipe;
	aic = (struct audio_aic_device*)spk_pipe->priv;;
	cur_codec = aic->codec_spk_info;

	if (NULL == aic || NULL == cur_codec) {
		audio_err_print("aic is null or cur_codec is null.\n");
		return -1;
	}

	/*set parameters*/
	switch(cmd) {
		case AUDIO_CMD_CONFIG_PARAM:
			ret = set_codec_spk_datatype(aic, data);
			break;

		case AUDIO_CMD_GET_GAIN:
			{
				struct volume *vol= (struct volume*)data;
				vol->gain[0] = cur_codec->again;
				break;
			}

		case AUDIO_CMD_GET_VOLUME:
			{
				struct volume *vol= (struct volume*)data;
				vol->gain[0] = cur_codec->dgain;
				break;
			}

		case AUDIO_CMD_SET_GAIN:
			ret = set_codec_spk_again(aic, data);
			break;

		case AUDIO_CMD_ENABLE_STREAM:
			ret = set_codec_spk_start(aic, data);
			break;

		case AUDIO_CMD_DISABLE_STREAM:
			ret = set_codec_spk_stop(aic, data);
			break;


	default:
		break;
	}

	return ret;
}

static int spk_deinit(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_spk_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}
	if (NULL == cur_codec) {
		audio_err_print("error, codec mic info is null\n");
		return -1;
	}
	cur_codec->rate = DEFAULT_SAMPLERATE;
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->again = DEFAULT_AGAIN;
	cur_codec->dgain = DEFAULT_DGAIN;
	cur_codec->trigger = DEFAULT_RECORD_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

/* aec operations */
static int aec_init(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_aec_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}

	//init parameters
	cur_codec->rate = DEFAULT_SAMPLERATE;
	/*
	 * The value of format reference are:
	 * AFMT_U8     0x00000001
	 * AFMT_S8     0x00000002
	 * AFMT_S16LE  0x00000010
	 * AFMT_S16BE  0x00000011
	 * */
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->again = DEFAULT_AGAIN;
	cur_codec->dgain = DEFAULT_DGAIN;
	cur_codec->trigger = DEFAULT_AEC_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

static int set_codec_aec_datatype(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;
	struct audio_data_type data_type;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	data_type.sample_rate = param->rate;
	data_type.sample_channel = param->channel;
	data_type.frame_vsize = param->format;
	data_type.frame_size = 32;
	if (data_type.sample_channel != MONO) {
		audio_err_print("We only support replay mono channel now.\n");
		return -EINVAL;
	}

	//set samplerate
/*	clk_set_rate(aic->spk_clock, data_type.sample_rate*256);
	clk_enable(aic->spk_clock);
	aic->codec_spk_info->rate = data_type.sample_rate;

	//set channel
	__i2s_out_channel_select(aic, MONO-1);
	__i2s_enable_mono2stereo(aic);
	aic->codec_spk_info->channel = data_type.sample_channel;

	//set format
	if (VALID_8BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 0);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	} else if (VALID_16BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 1);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else if (VALID_20BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 3);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	} else if (VALID_24BIT == data_type.frame_vsize){
		__i2s_set_oss_sample_size(aic, 4);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}
	__i2s_disable_byteswap(aic);*/
	//set trigger? default value is 8
	//__i2s_set_receive_trigger(aic, trigger_value);
	aic->codec_aec_info->format = data_type.frame_vsize;

	//codec set data_type
//	ret = aic->livingcodec->aec.set_datatype(&data_type);

	aic->codec_aec_info->data_type = data_type;

	return ret;
}

static int set_codec_aec_protocol(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct codec_sign_configure *protocol = (struct codec_sign_configure*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

/*	if (DATA_I2S_MODE == protocol->data)
//		__aic_select_i2s(aic);
		__i2s_select_i2s(aic);
	else if (DATA_RIGHT_JUSTIFIED_MODE == protocol->data || DATA_LEFT_JUSTIFIED_MODE == protocol->data)
		__i2s_select_msbjustified(aic);

	ret = aic->livingcodec->set_sign(protocol);
*/
	aic->codec_spk_info->transfer_info = *protocol;

	return ret;
}

static int set_codec_aec_again(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume gain = *(struct volume*)data;
	struct codec_analog_gain again;

	again.gain = gain;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

//	ret = aic->livingcodec->playback.set_again(&again);

	aic->codec_aec_info->again = gain.gain[0];

	return ret;
}

static int set_codec_aec_dgain(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume dgain = *(struct volume*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

//	ret = aic->livingcodec->playback.set_dgain(dgain);

	aic->codec_aec_info->dgain = dgain.gain[0];

	return ret;
}

static int set_codec_aec_start(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

/*	//enable aic global bit
	__i2s_enable(aic);
//	ret = aic->livingcodec->playback.set_endpoint(CODEC_SPK_START);

	{
		__i2s_flush_rfifo(aic);
		__i2s_read_rfifo(aic);
//		__i2s_enable_monoctr_right(aic);
		__i2s_enable_receive_dma(aic);
		__i2s_enable_record(aic);
		__i2s_enable_tloop(aic);
		__i2s_enable_etfl(aic);
	}*/

	return ret;
}

static int set_codec_aec_stop(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

/*	//disable aic record bit
	__i2s_disable_record(aic);
	__i2s_disable_receive_dma(aic);
	__i2s_disable_tloop(aic);
	__i2s_disable_etfl(aic);
	__i2s_enable_stereo(aic);
//	__i2s_disable_monoctr_right(aic);
*/
	//spk stop
//	ret = aic->livingcodec->playback.set_endpoint(CODEC_SPK_STOP);

	return ret;
}

static int aec_ioctl(void *pipe, unsigned int cmd, void *data)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_pipe *aec_pipe = NULL;
	int ret = 0;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	aec_pipe = (struct audio_pipe *)pipe;
	aic = (struct audio_aic_device*)aec_pipe->priv;;
	cur_codec = aic->codec_spk_info;

	if (NULL == aic || NULL == cur_codec) {
		audio_err_print("aic is null or cur_codec is null.\n");
		return -1;
	}

	/*set parameters*/
	switch(cmd) {
		case AUDIO_CMD_CONFIG_PARAM:
			ret = set_codec_aec_datatype(aic, data);
			break;

		case AUDIO_CMD_GET_GAIN:
			//*(unsigned long*)data = cur_codec->again;
			break;

		case AUDIO_CMD_SET_GAIN:
			ret = set_codec_aec_again(aic, data);
			break;

		case AUDIO_CMD_ENABLE_STREAM:
			ret = set_codec_aec_start(aic, data);
			break;

		case AUDIO_CMD_DISABLE_STREAM:
			ret = set_codec_aec_stop(aic, data);
			break;


	default:
		break;
	}

	return ret;
}

static int aec_deinit(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_aec_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}
	if (NULL == cur_codec) {
		audio_err_print("error, codec mic info is null\n");
		return -1;
	}
	cur_codec->rate = DEFAULT_SAMPLERATE;
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->again = DEFAULT_AGAIN;
	cur_codec->dgain = DEFAULT_DGAIN;
	cur_codec->trigger = DEFAULT_RECORD_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	//codec need power down or deinit procedure?
//	aic->livingcodec->set_power(CODEC_POWER_DOWN);

	return 0;
}

static int init_dma_config(struct audio_aic_device *aic)
{
	struct audio_pipe *mic_pipe = NULL;
	struct audio_pipe *spk_pipe = NULL;
	struct audio_pipe *aec_pipe = NULL;

	if (NULL == aic) {
		audio_err_print("aic is null.\n");
		return -EINVAL;
	}

	//init mic pipe
	mic_pipe = (struct audio_pipe*)kzalloc(sizeof(struct audio_pipe), GFP_KERNEL);
	if (NULL == mic_pipe) {
		audio_err_print("mic pipe kzallc failed.\n");
		goto failed_alloc_micpipe;
	}
	if (init_pipe(mic_pipe, DMA_FROM_DEVICE, aic->res->start, JZDMA_REQ_I2S0) != 0) {
		audio_err_print("init mic pipe failed.\n");
		goto init_micpipe_failed;
	}

	//init spk pipe
	spk_pipe = (struct audio_pipe*)kzalloc(sizeof(struct audio_pipe), GFP_KERNEL);
	if (NULL == spk_pipe) {
		audio_err_print("spk pipe kzallc failed.\n");
		goto failed_alloc_spkpipe;
	}
	if (init_pipe(spk_pipe, DMA_TO_DEVICE, aic->res->start, JZDMA_REQ_I2S0) != 0) {
		audio_err_print("init spk pipe failed.\n");
		goto init_spkpipe_failed;
	}

	//init aec pipe
	aec_pipe = (struct audio_pipe*)kzalloc(sizeof(struct audio_pipe), GFP_KERNEL);
	if (NULL == aec_pipe) {
		audio_err_print("aec pipe kzallc failed.\n");
		goto failed_alloc_aecpipe;
	}
	if (init_pipe(aec_pipe, DMA_FROM_DEVICE, aic->res->start, JZDMA_REQ_AEC) != 0) {
		audio_err_print("init aec pipe failed.\n");
		goto init_aecpipe_failed;
	}

	if (0 != pipe_request_dma(mic_pipe, aic->dev))
		goto failed_request_channel;
	if (0 != pipe_request_dma(spk_pipe, aic->dev))
		goto failed_request_channel;
	if (0 != pipe_request_dma(aec_pipe, aic->dev))
		goto failed_request_channel;

	aic->codec_mic_info = (struct codec_info*)kzalloc(sizeof(struct codec_info), GFP_KERNEL);
	if (NULL == aic->codec_mic_info) {
		audio_err_print("codec mic info kzalloc failed.\n");
		goto failed_alloc_mic_info;
	}
	aic->codec_spk_info = (struct codec_info*)kzalloc(sizeof(struct codec_info), GFP_KERNEL);
	if (NULL == aic->codec_spk_info) {
		audio_err_print("codec spk info kzalloc failed.\n");
		goto failed_alloc_spk_info;
	}
	aic->codec_aec_info = (struct codec_info*)kzalloc(sizeof(struct codec_info), GFP_KERNEL);
	if (NULL == aic->codec_aec_info) {
		audio_err_print("codec aec info kzalloc failed.\n");
		goto failed_alloc_aec_info;
	}

	mic_pipe->init = mic_init;
	mic_pipe->ioctl = mic_ioctl;
	mic_pipe->deinit = mic_deinit;

	spk_pipe->init = spk_init;
	spk_pipe->ioctl = spk_ioctl;
	spk_pipe->deinit = spk_deinit;

	aec_pipe->init = aec_init;
	aec_pipe->ioctl = aec_ioctl;
	aec_pipe->deinit = aec_deinit;

	mic_pipe->priv = aic;
	spk_pipe->priv = aic;
	aec_pipe->priv = aic;

	aic->mic_pipe = mic_pipe;
	aic->spk_pipe = spk_pipe;
	aic->aec_pipe = aec_pipe;

	audio_info_print("dma config init over.\n");

	return 0;
failed_alloc_aec_info:
	kfree(aic->codec_spk_info);
failed_alloc_spk_info:
	kfree(aic->codec_mic_info);
failed_alloc_mic_info:

failed_request_channel:

init_aecpipe_failed:
	kfree(aec_pipe);
failed_alloc_aecpipe:

init_spkpipe_failed:
	kfree(spk_pipe);
failed_alloc_spkpipe:

init_micpipe_failed:
	kfree(mic_pipe);
failed_alloc_micpipe:
	return -1;
}


extern struct platform_driver audio_codec_driver;

static int audio_aic_probe(struct platform_device *pdev)
{
	struct audio_aic_device *aic = NULL;
	struct platform_device *codec_device = NULL;
	int ret = AUDIO_SUCCESS;
#define INNER_CODEC  0
#define EXTERNAL_CODEC 1
	int codec_type = INNER_CODEC;

	aic = (struct audio_aic_device*)kzalloc(sizeof(struct audio_aic_device), GFP_KERNEL);
	if (!aic) {
		audio_err_print("aic device kzallc failed..\n");
		return -ENOMEM;
	}
	memset(aic, 0, sizeof(*aic));

	platform_set_drvdata(pdev, aic);
	globe_aic = aic;
	globe_aic->codec_dir = NULL;
	globe_aic->codec_file = NULL;

	//init aic resources(IO, DMA, IRQ)
	aic->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == aic->res) {
		audio_err_print("aic resource get failed.\n");
		goto err_get_resource;
	}

	if (!request_mem_region(aic->res->start, resource_size(aic->res), pdev->name)) {
		audio_err_print("aic request mem region failed.\n");
		goto err_req_mem_region;
	}

	aic->i2s_iomem = ioremap(aic->res->start, resource_size(aic->res));
	if (!aic->i2s_iomem) {
		audio_err_print("aic ioremap failed.\n");
		goto err_aic_ioremap;
	}
	//DMA(init dma)
	aic->dev = &pdev->dev;
	if (0 != init_dma_config(aic))
		goto err_init_dma_config;

	spin_lock_init(&aic->i2s_lock);

	//IRQ
	aic->aic_irq = platform_get_irq(pdev, 0);
	if (request_irq(aic->aic_irq, aic_irq_handler, IRQF_DISABLED, "aic_irq", aic) < 0) {
		audio_err_print("aic request irq failed.\n");
		goto err_aic_irq;
	}

	//get clock
	aic->spk_clock = clk_get(&pdev->dev, "cgu_i2s_spk");
	if (IS_ERR(aic->spk_clock)) {
		audio_err_print("aic get i2s spk clock failed.\n");
		goto err_get_i2s_spk;
	}
	aic->mic_clock = clk_get(&pdev->dev, "cgu_i2s_mic");
	if (IS_ERR(aic->mic_clock)) {
		audio_err_print("aic get i2s mic clock failed.\n");
		goto err_get_i2s_mic;
	}
	aic->aic_clock = clk_get(&pdev->dev, "aic");
	if (IS_ERR(aic->aic_clock)) {
		audio_err_print("aic get aic clock failed.\n");
		goto err_get_aic_clk;
	}

	//init audio_pipe(dma init), AIC register init
	clk_enable(aic->aic_clock);
	if(excodec_addr != DEFAULT_EXCODEC_ADDR){
		audio_info_print("excodec name=%s, excodec_addr=%p\n", excodec_name, excodec_addr);
		ret = extern_codec_register(aic, i2c_bus, excodec_name, excodec_addr);
		codec_type = EXTERNAL_CODEC;
	} else {
		codec_device = pdev->dev.platform_data;
		platform_driver_register(&audio_codec_driver);
		platform_device_register(codec_device);
		codec_type = INNER_CODEC;
	}
	__i2s_disable(aic);
	schedule_timeout(5);
	__i2s_disable(aic);
	__i2s_stop_bitclk(aic);
	__i2s_stop_ibitclk(aic);
	__aic_select_i2s(aic);
	__i2s_select_i2s(aic);
	//set aic transfer protocol:I2S mode
	if (EXTERNAL_CODEC == codec_type) {
		__i2s_external_codec(aic);
		//have excodec. we configure master/slave mode, line mode
		if (CODEC_IS_MASTER_MODE == aic->livingcodec->mode) {
			//set codec master mode, AIC slave mode
			__i2s_slave_clkset(aic);
			__i2s_enable_sysclk_output(aic);
		} else if (CODEC_IS_SLAVE_MODE == aic->livingcodec->mode) {
			__i2s_master_clkset(aic);
			__i2s_disable_sysclk_output(aic);
		}
		if (CODEC_IS_4_LINES == aic->livingcodec->pins)
			__i2s_select_share_clk(aic);
		else if (CODEC_IS_6_LINES == aic->livingcodec->pins) {
			__i2s_select_spilt_clk(aic);
			clk_set_rate(aic->mic_clock, DEFAULT_RECORD_CLK);
			audio_info_print("i2s mic rate is %ld\n", clk_get_rate(aic->spk_clock));
			clk_enable(aic->mic_clock);
		}
		if (DATA_RIGHT_JUSTIFIED_MODE == aic->livingcodec->transfer_protocol ||
				DATA_LEFT_JUSTIFIED_MODE == aic->livingcodec->transfer_protocol)
			__i2s_select_msbjustified(aic);

		printk("@@@@@ excodec codec power up@@@@@@\n");
		aic->livingcodec->set_power(CODEC_POWER_UP);
	} else {
		//aic: slave mode, codec master mode
		__i2s_internal_codec_master(aic);
		__i2s_slave_clkset(aic);
		__i2s_enable_sysclk_output(aic);
		//aic: i2s transfer mode, codec i2s transfer mode
		//__i2s_select_i2s(aic);
		//aic->livingcodec.set_sign();
		//
		printk("@@@@@ inner codec power up@@@@@@\n");
		aic->livingcodec->set_power(CODEC_POWER_UP);
	}

	//enable clk
	clk_set_rate(aic->spk_clock, DEFAULT_REPLAY_CLK);
	audio_info_print("i2s spk rate is %ld\n", clk_get_rate(aic->spk_clock));
	clk_enable(aic->spk_clock);

	clk_set_rate(aic->mic_clock, DEFAULT_REPLAY_CLK);
	audio_info_print("i2s mic rate is %ld\n", clk_get_rate(aic->mic_clock));
	clk_enable(aic->mic_clock);

	__i2s_start_bitclk(aic);
	__i2s_start_ibitclk(aic);

	__i2s_disable_receive_dma(aic);
	__i2s_disable_transmit_dma(aic);
	__i2s_disable_tloop(aic);
	__i2s_disable_record(aic);
	__i2s_disable_replay(aic);
	__i2s_disable_loopback(aic);
	__i2s_disable_etfl(aic);
	__i2s_clear_ror(aic);
	__i2s_clear_tur(aic);
	__i2s_set_receive_trigger(aic, DEFAULT_RECORD_TRIGGER);
	__i2s_set_transmit_trigger(aic, DEFAULT_REPLAY_TRIGGER);
	__i2s_write_tfifo_loop(aic, DEFAULT_AEC_TRIGGER);
	__i2s_disable_overrun_intr(aic);
	__i2s_disable_underrun_intr(aic);
	__i2s_disable_transmit_intr(aic);
	__i2s_disable_receive_intr(aic);
	__i2s_send_rfirst(aic);

	__i2s_play_lastsample(aic);
	__i2s_enable(aic);
	__i2s_reset(aic);

	//call funtion:registe_aic_to_dsp in dsp module to init dsp route
	if (AUDIO_SUCCESS != register_audio_pipe(aic->mic_pipe, AUDIO_ROUTE_AMIC_ID)) {
		audio_err_print("failed to register amic pipe.\n");
		goto failed_register_amic_pipe;
	}
	if (AUDIO_SUCCESS != register_audio_pipe(aic->spk_pipe, AUDIO_ROUTE_SPK_ID)) {
		audio_err_print("failed to register spk pipe.\n");
		goto failed_register_spk_pipe;
	}
	if (AUDIO_SUCCESS != register_audio_pipe(aic->aec_pipe, AUDIO_ROUTE_AEC_ID)) {
		audio_err_print("failed to register aec pipe.\n");
		goto failed_register_aec_pipe;
	}

	if (AUDIO_SUCCESS != register_audio_debug_ops("audio_aic_info", &audio_aic_debug_fops, aic)) {
		audio_err_print("failed to register audio aic debug proc file!\n");
		goto failed_register_aec_pipe;
	}
	audio_info_print("aic init success.\n");

	return 0;
failed_register_aec_pipe:
failed_register_spk_pipe:
failed_register_amic_pipe:

err_get_aic_clk:
	clk_put(aic->mic_clock);
err_get_i2s_mic:
	clk_put(aic->spk_clock);
err_get_i2s_spk:
	free_irq(aic->aic_irq, aic);
err_aic_irq:
	pipe_release_dma(aic, &pdev->dev);
	kfree(aic->mic_pipe);
	kfree(aic->spk_pipe);
	kfree(aic->aec_pipe);
	kfree(aic->codec_mic_info);
	kfree(aic->codec_spk_info);
	kfree(aic->codec_aec_info);
err_init_dma_config:
	iounmap(aic->i2s_iomem);
err_aic_ioremap:
	release_mem_region(aic->res->start, resource_size(aic->res));
err_req_mem_region:
err_get_resource:
	kfree(aic);
	return -1;
}

static int __exit audio_aic_remove(struct platform_device *pdev)
{
	struct audio_aic_device *aic = platform_get_drvdata(pdev);

	if (NULL == aic) {
		audio_info_print("aic device is null.\n");
		return 0;
	}

	release_audio_pipe(aic->mic_pipe);
	release_audio_pipe(aic->spk_pipe);
	release_audio_pipe(aic->aec_pipe);

	if (aic->incodec)
		inner_codec_release(aic->livingcodec);
	if (aic->excodec)
		extern_codec_release(aic);

	//dma release
	pipe_release_dma(aic, &pdev->dev);

	__i2s_disable(aic);
	clk_disable(aic->aic_clock);
	clk_disable(aic->mic_clock);
	clk_disable(aic->spk_clock);

	free_irq(aic->aic_irq, aic);

	clk_put(aic->aic_clock);
	clk_put(aic->mic_clock);
	clk_put(aic->spk_clock);

	iounmap(aic->i2s_iomem);
	release_mem_region(aic->res->start, resource_size(aic->res));

	kfree(aic->mic_pipe);
	kfree(aic->spk_pipe);
	kfree(aic->aec_pipe);
	kfree(aic->codec_mic_info);
	kfree(aic->codec_spk_info);
	kfree(aic->codec_aec_info);
	kfree(aic);

	if (NULL != globe_aic->codec_file)
		proc_remove(globe_aic->codec_file);
	if (NULL != globe_aic->codec_dir)
		proc_remove(globe_aic->codec_dir);
	globe_aic = NULL;

	return 0;
}

struct platform_driver audio_aic_driver = {
	.probe = audio_aic_probe,
	.remove = __exit_p(audio_aic_remove),
	.driver = {
		.name = "jz-aic",
		.owner = THIS_MODULE,
	},
};
