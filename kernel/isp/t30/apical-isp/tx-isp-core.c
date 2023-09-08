/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <media/v4l2-common.h>
#include <linux/v4l2-mediabus.h>
#include <asm/mipsregs.h>
#include <linux/mm.h>
#include <linux/clk.h>

#include <tx-isp-list.h>
#include "tx-isp-core.h"

#include <apical-isp/apical_math.h>
#include "system_i2c.h"
#include <apical-isp/apical_isp_io.h>
#include <apical-isp/system_io.h>
#include <apical-isp/apical_configuration.h>
#include <apical-isp/system_interrupts.h>
#include <apical-isp/apical_isp_config.h>
#include "system_semaphore.h"
#include "apical_command_api.h"
#include <apical-isp/apical_isp_core_nomem_settings.h>
#include "apical_scaler_lut.h"
#include "sensor_drv.h"
#include <apical-isp/apical_firmware_config.h>
#include "tx-isp-core-tuning.h"

#include "../videoin/tx-isp-vic.h"

#if ISP_HAS_CONNECTION_DEBUG
#include "apical_cmd_interface.h"
#endif

system_tab stab ;
#define SOFT_VERSION "H20190523a"
#define FIRMWARE_VERSION "H01-380"

#if defined(CONFIG_SOC_T10)
static int isp_clk = ISP_CLK_960P_MODE;
#elif defined(CONFIG_SOC_T20)
static int isp_clk = ISP_CLK_1080P_MODE;
#elif (defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21))
static int isp_clk = ISP_CLK_1080P_MODE;
#endif
module_param(isp_clk, int, S_IRUGO);
MODULE_PARM_DESC(isp_clk, "isp core clock");

/*
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
   manager the buffer of frame channels
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 */
static inline void init_buffer_fifo(struct list_head *fifo)
{
	TX_INIT_LIST_HEAD(fifo);
}

static struct frame_channel_buffer *pop_buffer_fifo(struct list_head *fifo)
{
	struct frame_channel_buffer *buf;
	if(!tx_list_empty(fifo)){
		buf = tx_list_first_entry(fifo, struct frame_channel_buffer, entry);
		//		printk("^-^ %s %d %p %p^-^\n",__func__,__LINE__, fifo, buf);
		tx_list_del(&(buf->entry));
	}else
		buf = NULL;

	return buf;
}

static void inline push_buffer_fifo(struct list_head *fifo, struct frame_channel_buffer *buf)
{
	//	printk("^@^ %s %d %p %p^-^\n",__func__,__LINE__, fifo, buf);
	tx_list_add_tail(&(buf->entry), fifo);
}

static void inline cleanup_buffer_fifo(struct list_head *fifo)
{
	struct frame_channel_buffer *buf;
	while(!tx_list_empty(fifo)){
		buf = tx_list_first_entry(fifo, struct frame_channel_buffer, entry);
		tx_list_del(&(buf->entry));
	}
}

/*
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
   interrupt handler
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 */
static system_interrupt_handler_t isr_func[APICAL_IRQ_COUNT] = {NULL};
static void* isr_param[APICAL_IRQ_COUNT] = {NULL};
//static int interrupt_line[APICAL_IRQ_COUNT] = {0};
static struct tx_isp_subdev *use_to_intc_sd = NULL;

static void inline isp_set_interrupt_ops(struct tx_isp_subdev *sd)
{
	use_to_intc_sd = sd;
}
static inline unsigned short isp_intc_state(void)
{
	return apical_isp_interrupts_interrupt_status_read();
}

void system_program_interrupt_event(uint8_t event, uint8_t id)
{
	switch(event)
	{
		case 0: apical_isp_interrupts_interrupt0_source_write(id); break;
		case 1: apical_isp_interrupts_interrupt1_source_write(id); break;
		case 2: apical_isp_interrupts_interrupt2_source_write(id); break;
		case 3: apical_isp_interrupts_interrupt3_source_write(id); break;
		case 4: apical_isp_interrupts_interrupt4_source_write(id); break;
		case 5: apical_isp_interrupts_interrupt5_source_write(id); break;
		case 6: apical_isp_interrupts_interrupt6_source_write(id); break;
		case 7: apical_isp_interrupts_interrupt7_source_write(id); break;
		case 8: apical_isp_interrupts_interrupt8_source_write(id); break;
		case 9: apical_isp_interrupts_interrupt9_source_write(id); break;
		case 10: apical_isp_interrupts_interrupt10_source_write(id); break;
		case 11: apical_isp_interrupts_interrupt11_source_write(id); break;
		case 12: apical_isp_interrupts_interrupt12_source_write(id); break;
		case 13: apical_isp_interrupts_interrupt13_source_write(id); break;
		case 14: apical_isp_interrupts_interrupt14_source_write(id); break;
		case 15: apical_isp_interrupts_interrupt15_source_write(id); break;
	}
}
static inline void isp_clear_irq_source(void)
{
	int event = 0;
	for(event = 0; event < APICAL_IRQ_COUNT; event++){
		system_program_interrupt_event(event, 0);
	}
}

void system_set_interrupt_handler(uint8_t source,
		system_interrupt_handler_t handler, void* param)
{
	isr_func[source] = handler;
	isr_param[source] = param;
}
void system_init_interrupt(void)
{
	int i;
	for (i = 0; i < APICAL_IRQ_COUNT; i++)
	{
		isr_func[i] = NULL;
		isr_param[i] = NULL;
	}

	isp_clear_irq_source();
}

void system_hw_interrupts_enable(void)
{
	tx_vic_enable_irq(TX_ISP_TOP_IRQ_ISP);
}

void system_hw_interrupts_disable(void)
{
	tx_vic_disable_irq(TX_ISP_TOP_IRQ_ISP);
}

/*
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
   v4l2_subdev_ops will be defined as follows
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 */
static int isp_set_buffer_lineoffset_vflip_disable(struct isp_core_output_channel *chan)
{
	struct frame_image_format *fmt = &(chan->fmt);
	unsigned int regw = 0xb00;

	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			APICAL_WRITE_32(regw + 0xa0 + 0x100 * (chan->index), chan->lineoffset);//lineoffset
			//	printk("(offset = 0x%08x)\n",offset);
		default:
			APICAL_WRITE_32(regw + 0x20 + 0x100 * (chan->index), chan->lineoffset);//lineoffset
			break;
	}
	return 0;
}
static int isp_set_buffer_lineoffset_vflip_enable(struct isp_core_output_channel *chan)
{
	struct frame_image_format *fmt = &(chan->fmt);
	unsigned int regw = 0xb00;
	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			/* UV */
			APICAL_WRITE_32(regw + 0xa0 + 0x100 * (chan->index), -chan->lineoffset);//lineoffset
			/* Y */
			APICAL_WRITE_32(regw + 0x20 + 0x100 * (chan->index), -chan->lineoffset);//lineoffset
			break;
		default:
			APICAL_WRITE_32(regw + 0x20 + 0x100 * (chan->index), -chan->lineoffset);//lineoffset
			break;
	}
	return 0;
}

static int isp_set_buffer_address_vflip_disable(struct isp_core_output_channel *chan, struct frame_channel_buffer *buf, unsigned char bank_id)
{
	struct frame_image_format *fmt = &(chan->fmt);
	struct tx_isp_subdev_pad *pad = chan->pad;
	unsigned int regw = 0xb00;
	unsigned int offset = 0;

	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			if(pad->link.flag & TX_ISP_PADLINK_FS)
				offset = fmt->pix.width * ((fmt->pix.height + 0xf) & ~0xf);
			else
				offset = fmt->pix.width * fmt->pix.height;
			APICAL_WRITE_32((regw + 0x88 + 0x100 * (chan->index) + 0x04 * bank_id), buf->addr + offset);
			//	printk("(offset = 0x%08x)\n",offset);
		default:
			APICAL_WRITE_32((regw + 0x08 + 0x100 * (chan->index) + 0x04 * bank_id), buf->addr);
			break;
	}

	/*printk("## %s %d ##\n",__func__,__LINE__);	*/
	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		isp_lfb_config_default_dma(buf->addr, offset, chan->lineoffset, bank_id);
	}
	return 0;
}
static int isp_set_buffer_address_vflip_enable(struct isp_core_output_channel *chan, struct frame_channel_buffer *buf, unsigned char bank_id)
{
	struct frame_image_format *fmt = &(chan->fmt);
	struct tx_isp_subdev_pad *pad = chan->pad;
	unsigned int regw = 0xb00;
	unsigned int offset;
	unsigned int offset_uv;
	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			/* UV */
			offset = fmt->pix.width * fmt->pix.height;
			if(pad->link.flag & TX_ISP_PADLINK_FS)
				offset_uv = (fmt->pix.height & 0xf) ? fmt->pix.width * (0x10 - (fmt->pix.height & 0xf)) :0; /* align at 16 lines */
			else
				offset_uv = 0;
			APICAL_WRITE_32((regw + 0x88 + 0x100 * (chan->index) + 0x04 * bank_id),
											buf->addr + fmt->pix.sizeimage + offset_uv - chan->lineoffset);
			/* Y */
			APICAL_WRITE_32((regw + 0x08 + 0x100 * (chan->index) + 0x04 * bank_id), buf->addr + offset - chan->lineoffset);
			break;
		default:
			APICAL_WRITE_32((regw + 0x08 + 0x100 * (chan->index) + 0x04 * bank_id), buf->addr + fmt->pix.sizeimage - chan->lineoffset);
			break;
	}
	return 0;
}

static int isp_enable_dma_transfer(struct isp_core_output_channel *chan, int onoff)
{
	struct frame_image_format *fmt = &(chan->fmt);
	unsigned int regw = 0xb00;
	//	unsigned int primary;
	//	printk("^~^ %s[%d] index = %d onoff = %d ^~^\n",__func__, __LINE__, video->index, onoff);
	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			if(onoff)
				APICAL_WRITE_32(regw + 0xa4 + 0x100*(chan->index), 0x02);// axi_port_enable set 1, frame_write_cancel set 0.
			else
				APICAL_WRITE_32(regw + 0xa4 + 0x100*(chan->index), 0x03);// axi_port_enable set 1, frame_write_cancel set 0.

		default:
			if(onoff)
				APICAL_WRITE_32(regw + 0x24 + 0x100*(chan->index), 0x02);// axi_port_enable set 1, frame_write_cancel set 0.
			else
				APICAL_WRITE_32(regw + 0x24 + 0x100*(chan->index), 0x03);// axi_port_enable set 1, frame_write_cancel set 0.
			//	printk("[%s]dma enable(0x%08x is wrote 0x%08x)\n",onoff ? "on":"off",regw + 0x100*(video->index), primary);
			break;
	}
	return 0;
}

static int isp_configure_base_addr(struct tx_isp_core_device *core)
{
	struct isp_core_output_channel *chan;
	struct frame_channel_buffer *buf;
	unsigned int hw_dma = 0;
	unsigned char current_bank = 0;
	unsigned char bank_id = 0;
	unsigned char i = 0;
	int index = 0;
	for(index = 0; index < core->num_chans; index++){
		chan = &(core->chans[index]);
		if(chan->state == TX_ISP_MODULE_RUNNING){
			if(chan->pad->link.flag & TX_ISP_PADLINK_LFB)
				continue;

			/*printk("## %s %d banks = %d ##\n",__func__,__LINE__, chan->usingbanks);	*/
			hw_dma = APICAL_READ_32(0xb24 + 0x100*index);
			current_bank = (hw_dma >> 8) & 0x7;
			/* The begin pointer is next bank. */
			for(i = 0, bank_id = current_bank; i < chan->usingbanks; i++, bank_id++){
				bank_id = bank_id % chan->usingbanks;
				if(chan->bank_flag[bank_id] == 0){
					buf = pop_buffer_fifo(&chan->fifo);
					if(buf != NULL){
#if 0
						if(core->vflip_state){
							isp_set_buffer_address_vflip_enable(chan, buf, bank_id);
						}else{
							isp_set_buffer_address_vflip_disable(chan, buf, bank_id);
						}
#else
						isp_set_buffer_address_vflip_disable(chan, buf, bank_id);
#endif
						chan->vflip_flag[bank_id] = core->vflip_state;
						chan->bank_flag[bank_id] = 1;
						chan->banks_addr[bank_id] = buf->addr;
					}else
						break;
				}
			}
		}
	}
	return 0;
}

static inline int isp_enable_channel(struct isp_core_output_channel *chan)
{
	unsigned int hw_dma = 0;
	unsigned char next_bank = 0;
	hw_dma = APICAL_READ_32(0xb24 + 0x100 * (chan->index));
	next_bank = (((hw_dma >> 8) & 0x7) + 1) % chan->usingbanks;
	if(chan->pad->link.flag & TX_ISP_PADLINK_LFB){
		if(chan->state == TX_ISP_MODULE_RUNNING){
			if(chan->dma_state == 0)
				isp_enable_dma_transfer(chan, 1);
			chan->dma_state = 1;
		}else{
			if(chan->dma_state)
				isp_enable_dma_transfer(chan, 0);
			chan->dma_state = 0;
		}
		return 0;
	}

	if(chan->bank_flag[next_bank] ^ chan->dma_state){
		/*printk("## %s %d ##\n",__func__,__LINE__);	*/
		chan->dma_state = chan->bank_flag[next_bank];
		isp_enable_dma_transfer(chan, chan->dma_state);
	}
	return 0;
}

static int isp_modify_dma_direction(struct isp_core_output_channel *chan)
{
	unsigned int hw_dma = 0;
	unsigned char next_bank = 0;
	if(chan->state == TX_ISP_MODULE_RUNNING){
		if(chan->pad->link.flag & TX_ISP_PADLINK_LFB)
			return 0;
		hw_dma = APICAL_READ_32(0xb24 + 0x100 * (chan->index));
		next_bank = (((hw_dma >> 8) & 0x7) + 1) % chan->usingbanks;
		if(chan->vflip_flag[next_bank] ^ chan->vflip_state){
			chan->vflip_state = chan->vflip_flag[next_bank];
#if 0
			if(chan->vflip_state){
				isp_set_buffer_lineoffset_vflip_enable(chan);
			}else{
				isp_set_buffer_lineoffset_vflip_disable(chan);
			}
#else
			isp_set_buffer_lineoffset_vflip_disable(chan);
#endif
		}
	}
	return 0;
}

static inline void cleanup_chan_banks(struct isp_core_output_channel *chan)
{
	int bank_id = 0;
	struct tx_isp_core_device *core = chan->priv;
	struct frame_channel_buffer buf;
	while(bank_id < chan->usingbanks){
		if(chan->bank_flag[bank_id]){
			buf.addr = chan->banks_addr[bank_id];
			buf.priv = core->frame_sequeue;
			tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER, &buf);
			chan->bank_flag[bank_id] = 0;
		}
		bank_id++;
	}
}

#define ISP_CHAN_DMA_STAT (1<<16)
#define ISP_CHAN_DMA_ACTIVE (1<<16)
static inline void isp_core_update_addr(struct isp_core_output_channel *chan)
{
	struct tx_isp_core_device *core = chan->priv;
	struct frame_image_format *fmt = &(chan->fmt);
	struct frame_channel_buffer buf;
	unsigned int y_hw_dma = 0;
	unsigned int uv_hw_dma = 0;
	unsigned char current_bank = 0;
	unsigned char uv_bank = 0;
	unsigned char last_bank = 0;
	unsigned char next_bank = 0;
	unsigned char bank_id = 0;
	unsigned int current_active = 0;
	unsigned int value = 0;
	unsigned int isnv = 0;

	if(chan->pad->link.flag & TX_ISP_PADLINK_LFB){
		if(chan->state == TX_ISP_MODULE_RUNNING){
			if(chan->dma_state == 0)
				isp_enable_dma_transfer(chan, 1);
			chan->dma_state = 1;
		}else{
			if(chan->dma_state)
				isp_enable_dma_transfer(chan, 0);
			chan->dma_state = 0;
		}
		return;
	}

	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			uv_hw_dma = APICAL_READ_32(0xba4 + 0x100 * chan->index);
			current_active |= uv_hw_dma;
			isnv = 1;
		default:
			y_hw_dma = APICAL_READ_32(0xb24 + 0x100 * chan->index);
			current_active |= y_hw_dma;
			break;
	}

	current_bank = (y_hw_dma >> 8) & 0x7;
	uv_bank = (uv_hw_dma >> 8) & 0x7;

	if(isnv && (uv_bank != current_bank)){
		/*printk("##### y_bank = %d, nv_bank = %d\n",current_bank,uv_bank);*/
		value = (0x1<<3) | (chan->usingbanks - 1);
		switch(fmt->pix.pixelformat){
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV21:
				APICAL_WRITE_32(0xb9c + 0x100*(chan->index), value);
			default:
				APICAL_WRITE_32(0xb1c + 0x100*(chan->index), value);
				break;
		}
		chan->reset_dma_flag = 1;
		return;
	}
	if(chan->reset_dma_flag){
		/*printk("##### y_bank = %d, nv_bank = %d\n",current_bank,uv_bank);*/
		value = (0x0<<3) | (chan->usingbanks - 1);
		switch(fmt->pix.pixelformat){
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV21:
				APICAL_WRITE_32(0xb9c + 0x100*(chan->index), value);
			default:
				APICAL_WRITE_32(0xb1c + 0x100*(chan->index), value);
				break;
		}
		chan->reset_dma_flag = 0;
	}

	if((current_active & ISP_CHAN_DMA_STAT) == ISP_CHAN_DMA_ACTIVE){
		last_bank = (y_hw_dma >> 11) & 0x7;
		bank_id = last_bank;
	}else{
		bank_id = current_bank;
	}
	if(chan->bank_flag[bank_id]){
		buf.addr = chan->banks_addr[bank_id];
		buf.priv = core->frame_sequeue;
		tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER, &buf);
		chan->bank_flag[bank_id] = 0;
	}else{
		tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER, NULL);
	}

	next_bank = (current_bank + 1) % chan->usingbanks;
	if(chan->bank_flag[next_bank] != 1){
		isp_enable_channel(chan);
	}
	return;
}
static char g_switch_lfb_off = 0;
static char g_switch_lfb_on = 0;
extern void tx_isp_sync_ldc(void);
static irqreturn_t ispcore_interrupt_service_routine(struct tx_isp_subdev *sd, u32 status, bool *handled)
{
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	/*struct isp_core_input_control *contrl = &core->contrl;*/
	struct isp_core_output_channel *chan;
	unsigned short isp_irq_status = 0;
	unsigned char color = apical_isp_top_rggb_start_read();
	unsigned int i = 0;
	irqreturn_t ret = IRQ_HANDLED;
	if((isp_irq_status = isp_intc_state()) != 0)
	{
		apical_isp_interrupts_interrupt_clear_write(0);
		apical_isp_interrupts_interrupt_clear_write(isp_irq_status);
		/* printk("0xb00 = 0x%0x state = 0x%x\n", APICAL_READ_32(0xb00), isp_irq_status); */
		for(i = 0; i < APICAL_IRQ_COUNT; i++){
			if(isp_irq_status & (1 << i)){
				switch(i){
					case APICAL_IRQ_FRAME_START:
						if(isr_func[i])
							isr_func[i](isr_param[i]);
						/*printk("^~^ frame start ^~^\n");*/
						isp_configure_base_addr(core);
						core->frame_state = 1;
						core->frame_sequeue++;
						ret = IRQ_WAKE_THREAD;
						break;
					case APICAL_IRQ_FRAME_WRITER_FR:
						if(isr_func[i])
							isr_func[APICAL_IRQ_FRAME_WRITER_FR](isr_param[APICAL_IRQ_FRAME_WRITER_FR]);
						chan = &core->chans[ISP_FR_VIDEO_CHANNEL];
						/*printk("^~^ fr dma done ^~^\n");*/
						isp_core_update_addr(chan);
						isp_modify_dma_direction(chan);
						break;
					case APICAL_IRQ_FRAME_WRITER_DS:
						if(isr_func[i])
							isr_func[APICAL_IRQ_FRAME_WRITER_DS](isr_param[APICAL_IRQ_FRAME_WRITER_DS]);
						chan = &core->chans[ISP_DS1_VIDEO_CHANNEL];
						isp_core_update_addr(chan);
						isp_modify_dma_direction(chan);
						break;
					case APICAL_IRQ_FRAME_WRITER_DS2:
						if(isr_func[i])
							isr_func[i](isr_param[i]);
						#if TX_ISP_EXIST_DS2_CHANNEL
						chan = &core->chans[ISP_DS2_VIDEO_CHANNEL];
						isp_core_update_addr(chan);
						isp_modify_dma_direction(chan);
						#endif
						break;
					case APICAL_IRQ_FRAME_END:
						if(core->hflip_change == 1){
							core->hflip_change = 0;
							color ^= 1;
							apical_isp_top_bypass_mirror_write(core->hflip_state ?0:1);
						}
						if(core->vflip_change == 1){
							core->vflip_change = 0;
							if(core->vin.mbus_change == 1)
								color ^= 2;
						}
						apical_isp_top_rggb_start_write(color);
						/* APICAL_WRITE_32(0x18,2);  */
						/*printk("^~^ frame done ^~^\n");*/
						chan = &core->chans[ISP_FR_VIDEO_CHANNEL];
						core->frame_state = 0;
						isp_configure_base_addr(core);
						isp_modify_dma_direction(chan);
						if(chan->dma_state != 1){
							isp_enable_channel(chan);
						}

						if(core->tuning)
							core->tuning->event(core->tuning, TX_ISP_EVENT_CORE_FRAME_DONE, NULL);

						if (1 == core->isp_daynight_switch) {
							if(core->tuning)
								core->tuning->event(core->tuning, TX_ISP_EVENT_CORE_DAY_NIGHT, NULL);
							core->isp_daynight_switch = 0;
						}
						tx_isp_sync_ldc();
						if (g_switch_lfb_off) {
						    isp_lfb_ctrl_flb_enable(0);
						    g_switch_lfb_off = 0;
						}
						if (g_switch_lfb_on) {
						    isp_lfb_ctrl_flb_enable(1);
						    g_switch_lfb_on = 0;
						}
					case APICAL_IRQ_AE_STATS:
					case APICAL_IRQ_AWB_STATS:
					case APICAL_IRQ_AF_STATS:
					case APICAL_IRQ_FPGA_FRAME_START:
					case APICAL_IRQ_FPGA_FRAME_END:
					case APICAL_IRQ_FPGA_WDR_BUF:
						if(isr_func[i])
							isr_func[i](isr_param[i]);
						break;
					case APICAL_IRQ_DS1_OUTPUT_END:
						chan = &core->chans[ISP_DS1_VIDEO_CHANNEL];
						isp_modify_dma_direction(chan);
						if(chan->dma_state != 1){
							isp_enable_channel(chan);
						}
						break;
					#if TX_ISP_EXIST_DS2_CHANNEL
					case APICAL_IRQ_DS2_OUTPUT_END:
						chan = &core->chans[ISP_DS2_VIDEO_CHANNEL];
						isp_modify_dma_direction(chan);
						if(chan->dma_state != 1){
							isp_enable_channel(chan);
						}
						break;
					#endif
					default:
						break;
				}
			}
		}
	}
	return ret;
}

static int ispcore_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg);

static irqreturn_t ispcore_irq_thread_handle(struct tx_isp_subdev *sd, void *data)
{
	struct tx_isp_core_device *core = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned int cmd = 0;
	unsigned int value = 0;
	int i = 0;

	if(core){
		for(i = 0; i < TX_ISP_I2C_SET_BUTTON; i++){
			if(core->i2c_msgs[i].flag == 0)
				continue;
			core->i2c_msgs[i].flag = 0;
			switch(i){
				case TX_ISP_I2C_SET_AGAIN:
					cmd = TX_ISP_EVENT_SENSOR_AGAIN;
					break;
				case TX_ISP_I2C_SET_DGAIN:
					cmd = TX_ISP_EVENT_SENSOR_DGAIN;
					break;
				case TX_ISP_I2C_SET_INTEGRATION:
					cmd = TX_ISP_EVENT_SENSOR_INT_TIME;
					break;
				default:
					break;
			}
			value = core->i2c_msgs[i].value;
			ispcore_sensor_ops_ioctl(sd, cmd, &value);
		}
	}
	return 0;
}

static void isp_core_config_top_ctl_register(unsigned int name, unsigned int value)
{
	apical_isp_top_ctl_t top;
	top.reg.low = APICAL_READ_32(0x40);
	top.reg.high = APICAL_READ_32(0x44);

	switch(name){
		case TEST_GEN_BIT:
			top.bits.test_gen = value;
			break;
		case MIRROR_BIT:
			top.bits.mirror = value;
			break;
		case SENSOR_OFFSET_BIT:
			top.bits.sensor_offset = value;
			break;
		case DIG_GAIN_BIT:
			top.bits.dig_gain = value;
			break;
		case GAMMA_FE_BIT:
			top.bits.gamma_fe = value;
			break;
		case RAW_FRONT_BIT:
			top.bits.raw_front = value;
			break;
		case DEFECT_PIXEL_BIT:
			top.bits.defect_pixel = value;
			break;
		case FRAME_STITCH_BIT:
			top.bits.frame_stitch = value;
			break;
		case GAMMA_FE_POS_BIT:
			top.bits.gamma_fe_pos = value;
			break;
		case SINTER_BIT:
			top.bits.sinter = value;
			break;
		case TEMPER_BIT:
			top.bits.temper = value;
			break;
		case ORDER_BIT:
			top.bits.order = value;
			break;
		case WB_BIT:
			top.bits.wb = value;
			break;
		case RADIAL_BIT:
			top.bits.radial = value;
			break;
		case MESH_BIT:
			top.bits.mesh = value;
			break;
		case IRIDIX_BIT:
			top.bits.iridix = value;
			break;
		case DEMOSAIC_BIT:
			top.bits.demosaic = value;
			break;
		case MATRIX_BIT:
			top.bits.matrix = value;
			break;
		case FR_CROP_BIT:
			top.bits.fr_crop = value;
			break;
		case FR_GAMMA_BIT:
			top.bits.fr_gamma = value;
			break;
		case FR_SHARPEN_BIT:
			top.bits.fr_sharpen = value;
			break;
		case FR_LOGO_BIT:
			top.bits.fr_logo = value;
			break;
		case FR_CSC_BIT:
			top.bits.fr_csc = value;
			break;
		case DS1_CROP_BIT:
			top.bits.ds1_crop = value;
			break;
		case DS1_SCALER_BIT:
			top.bits.ds1_scaler = value;
			break;
		case DS1_GAMMA_BIT:
			top.bits.ds1_gamma = value;
			break;
		case DS1_SHARPEN_BIT:
			top.bits.ds1_sharpen = value;
			break;
		case DS1_LOGO_BIT:
			top.bits.ds1_logo = value;
			break;
		case DS1_CSC_BIT:
			top.bits.ds1_csc = value;
			break;
		case DS1_DMA_BIT:
			top.bits.ds1_dma = value;
			break;
		case DS2_SCALER_SOURCE_BIT:
			top.bits.ds2_scaler_source = value;
			break;
		case DS2_CROP_BIT:
			top.bits.ds2_crop = value;
			break;
		case DS2_SCALER_BIT:
			top.bits.ds2_scaler = value;
			break;
		case DS2_GAMMA_BIT:
			top.bits.ds2_gamma = value;
			break;
		case DS2_SHARPEN_BIT:
			top.bits.ds2_sharpen = value;
			break;
		case DS2_LOGO_BIT:
			top.bits.ds2_logo = value;
			break;
		case DS2_CSC_BIT:
			top.bits.ds2_csc = value;
			break;
		case RAW_BYPASS_BIT:
			top.bits.raw_bypass = value;
			break;
		case DEBUG_BIT:
			top.bits.debug = value;
			break;
		case PROC_BYPASS_MODE_BIT:
			top.bits.proc_bypass_mode = value;
			break;
		default:
			break;
	}

	APICAL_WRITE_32(0x40, top.reg.low);
	APICAL_WRITE_32(0x44, top.reg.high);
}

static int ispcore_config_dma_channel_write(struct isp_core_output_channel *chan, struct frame_image_format *fmt,
		struct frame_channel_format *cfmt)
{
	struct tx_isp_core_device *core = chan->priv;
	struct isp_core_input_control *contrl = &core->contrl;
	struct v4l2_mbus_framefmt *mbus = &core->vin.mbus;
	unsigned int base = 0x00b00 + 0x100 * chan->index; // the base of address of dma channel write
	apical_api_control_t api;
	unsigned char status = 0;
	int ret = ISP_SUCCESS;
	unsigned int csc = 0xf;

	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			/* dma channel uv write */
			APICAL_WRITE_32(base + 0x80, cfmt->priv);
			APICAL_WRITE_32(base + 0x84, fmt->pix.height << 16 | fmt->pix.width);
			APICAL_WRITE_32(base + 0xa0, chan->lineoffset);//lineoffset
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YUV444:
			switch(chan->index){
				case ISP_FR_VIDEO_CHANNEL:
					if(mbus->code == V4L2_MBUS_FMT_YUYV8_1X16)
						csc = 0x08;
					else
						csc = 0x0f;

					isp_core_config_top_ctl_register(FR_CSC_BIT,
							ISP_MODULE_BYPASS_DISABLE);
					break;
				case ISP_DS1_VIDEO_CHANNEL:
					if(mbus->code == V4L2_MBUS_FMT_YUYV8_1X16)
						csc = 0x08;
					else
						csc = 0x0f;
					isp_core_config_top_ctl_register(DS1_CSC_BIT,
							ISP_MODULE_BYPASS_DISABLE);
					break;
				case ISP_DS2_VIDEO_CHANNEL:
					if(mbus->code == V4L2_MBUS_FMT_YUYV8_1X16)
						csc = 0x08;
					else
						csc = 0x0f;
					isp_core_config_top_ctl_register(DS2_CSC_BIT,
							ISP_MODULE_BYPASS_DISABLE);
					break;
				default:
					break;
			}
			if(contrl->fmt_end == APICAL_ISP_INPUT_RAW_FMT_INDEX_END){
				isp_core_config_top_ctl_register(DEMOSAIC_BIT, ISP_MODULE_BYPASS_DISABLE);
			}
			APICAL_WRITE_32(base, cfmt->priv & 0x0f);
			APICAL_WRITE_32(base + 0x04, fmt->pix.height << 16 | fmt->pix.width);
			APICAL_WRITE_32(base + 0x20, chan->lineoffset);//lineoffset
			break;
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_BGR24:
		case V4L2_PIX_FMT_BGR32:
		case V4L2_PIX_FMT_RGB310:
			switch(chan->index){
				case ISP_FR_VIDEO_CHANNEL:
					isp_core_config_top_ctl_register(FR_CSC_BIT,
							ISP_MODULE_BYPASS_ENABLE);
					break;
				case ISP_DS1_VIDEO_CHANNEL:
					isp_core_config_top_ctl_register(DS1_CSC_BIT,
							ISP_MODULE_BYPASS_ENABLE);
					break;
				#if TX_ISP_EXIST_DS2_CHANNEL
				case ISP_DS2_VIDEO_CHANNEL:
					isp_core_config_top_ctl_register(DS2_CSC_BIT,
							ISP_MODULE_BYPASS_ENABLE);
					break;
				#endif
				default:
					break;
			}
			if(contrl->fmt_end == APICAL_ISP_INPUT_RAW_FMT_INDEX_END){
				isp_core_config_top_ctl_register(DEMOSAIC_BIT, ISP_MODULE_BYPASS_DISABLE);
			}
		case V4L2_PIX_FMT_SBGGR12:
		case V4L2_PIX_FMT_SGBRG12:
		case V4L2_PIX_FMT_SGRBG12:
		case V4L2_PIX_FMT_SRGGB12:
			APICAL_WRITE_32(base, cfmt->priv & 0x0f);
			APICAL_WRITE_32(base + 0x04, fmt->pix.height << 16 | fmt->pix.width);
			APICAL_WRITE_32(base + 0x20, chan->lineoffset);//lineoffset
			break;
		default:
			break;
	}

	api.type = TSCENE_MODES;
	api.dir = COMMAND_SET;
	api.id = -1;
	api.value = -1;
	switch(chan->index){
		case ISP_FR_VIDEO_CHANNEL:
			api.id = FR_OUTPUT_MODE_ID;
			break;
		case ISP_DS1_VIDEO_CHANNEL:
			api.id = DS1_OUTPUT_MODE_ID;
			break;
		case ISP_DS2_VIDEO_CHANNEL:
			api.id = DS2_OUTPUT_MODE_ID;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			api.value = YUV420;
			break;
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
			api.value = YUV422;
			break;
		case V4L2_PIX_FMT_YUV444:
			api.value = YUV444;
			break;
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_BGR24:
		case V4L2_PIX_FMT_BGR32:
		case V4L2_PIX_FMT_RGB310:
			api.value = RGB;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	if(api.value != -1 && api.id != -1){
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
	}

	switch(chan->index){
	case ISP_FR_VIDEO_CHANNEL:
		APICAL_WRITE_32(0x570, csc);
		break;
	case ISP_DS1_VIDEO_CHANNEL:
		APICAL_WRITE_32(0x6b0, csc);
		break;
	case ISP_DS2_VIDEO_CHANNEL:
		APICAL_WRITE_32(0x7b0, csc);
		break;
	default:
		break;
	}

	return ISP_SUCCESS;
}

static struct frame_channel_format isp_output_fmt[APICAL_ISP_FMT_MAX_INDEX] = {
	{
		.name     = "YUV 4:2:0 semi planar, Y/CbCr",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
		.priv     = DMA_FORMAT_NV12_UV,
	},
	{
		.name     = "YUV 4:2:0 semi planar, Y/CrCb",
		.fourcc   = V4L2_PIX_FMT_NV21,
		.depth    = 12,
		.priv     = DMA_FORMAT_NV12_VU,
	},
	{
		.name     = "YUV 4:2:2 packed, YCbYCr",
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.depth    = 16,
		.priv     = DMA_FORMAT_YUY2,
	},
	{
		.name     = "YUV 4:2:2 packed, CbYCrY",
		.fourcc   = V4L2_PIX_FMT_UYVY,
		.depth    = 16,
		.priv	  = DMA_FORMAT_UYVY,
	},
	{
		.name     = "YUV 4:4:4 packed, YCbCr",
		.fourcc   = V4L2_PIX_FMT_YUV444,
		.depth    = 32,
		.priv     = DMA_FORMAT_AYUV,
	},
	{
		.name     = "RGB565, RGB-5-6-5",
		.fourcc   = V4L2_PIX_FMT_RGB565,
		.depth    = 16,
		.priv     = DMA_FORMAT_RGB565,
	},
	{
		.name     = "BGR24, RGB-8-8-8-3",
		.fourcc   = V4L2_PIX_FMT_BGR24,
		.depth    = 24,
		.priv     = DMA_FORMAT_RGB24,
	},
	{
		.name     = "BGR32, RGB-8-8-8-4",
		.fourcc   = V4L2_PIX_FMT_BGR32,
		.depth    = 32,
		.priv     = DMA_FORMAT_RGB32,
	},
	{
		.name     = "RGB101010, RGB-10-10-10",
		.fourcc   = V4L2_PIX_FMT_RGB310,
		.depth    = 32,
		.priv     = DMA_FORMAT_A2R10G10B10,
	},
	/* the last member will be determined when isp input confirm.*/
	{
		.name     = "undetermine",
		.fourcc   = 0,
		.depth    = 0,
		.priv     = 0,
	},
};

static int ispcore_frame_channel_try_fmt (struct tx_isp_core_device *core, struct frame_image_format *fmt)
{
	struct isp_core_input_control *contrl = &core->contrl;
	struct frame_channel_format *cfmt;
	int i = 0;

	if(core->bypass == TX_ISP_FRAME_CHANNEL_BYPASS_ISP_ENABLE){
		i = contrl->fmt_end;
	}else{
		i = contrl->fmt_start;
	}
	/*printk("^^^^ %s[%d] i = %d^^^^\n",__func__,__LINE__,i);*/
	for(; i <= contrl->fmt_end; i++){
		cfmt = &(isp_output_fmt[i]);
		if(fmt->pix.pixelformat == cfmt->fourcc){
			break;
		}
	}
	//	printk("^^^^ %s[%d] i = %d^^^^\n",__func__,__LINE__,i);
	if(i > contrl->fmt_end){
		printk("%s[%d], don't support the format(0x%08x), i = %d start = %d end = %d\n",
				__func__, __LINE__, fmt->pix.pixelformat, i, contrl->fmt_start, contrl->fmt_end);
		return -EINVAL;
	}

	return 0;
}

static int ispcore_frame_channel_s_fmt(struct isp_core_output_channel *chan, struct frame_image_format *fmt)
{
	struct tx_isp_core_device *core = chan->priv;
	struct isp_core_input_control *contrl = &core->contrl;
	struct frame_channel_format *cfmt = NULL;
	int i = 0;

	for(i = contrl->fmt_start; i <= contrl->fmt_end; i++){
		cfmt = &(isp_output_fmt[i]);
		if(fmt->pix.pixelformat == cfmt->fourcc)
			break;
	}

	if (i > contrl->fmt_end) {
		printk("%s[%d] unfound the pixelformat = %d\n", __func__, __LINE__, fmt->pix.pixelformat);
		return -EINVAL;
	}

	fmt->pix.bytesperline = fmt->pix.width * cfmt->depth / 8;

	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			fmt->pix.sizeimage = fmt->pix.bytesperline * ((fmt->pix.height + 0xf) & (~0xf)); // the size must be algin 16 lines.
			break;
		default:
			fmt->pix.sizeimage = fmt->pix.bytesperline * fmt->pix.height;
			break;
	}
	chan->lineoffset = fmt->pix.width * (cfmt->depth / 8);
	fmt->pix.priv = (unsigned int)cfmt;
	return ispcore_config_dma_channel_write(chan, fmt, cfmt);
}

static int ispcore_frame_channel_set_crop(struct isp_core_output_channel *chan, struct frame_image_format *fmt)
{
	apical_api_control_t api;
	unsigned int index = 0;
	unsigned char status = 0;
	int ret = ISP_SUCCESS;

	switch(chan->index){
		case ISP_FR_VIDEO_CHANNEL:
			apical_isp_top_bypass_fr_crop_write((fmt && fmt->crop_enable)?0:1);
			index = CROP_FR;
			break;
		case ISP_DS1_VIDEO_CHANNEL:
			apical_isp_top_bypass_ds1_crop_write((fmt && fmt->crop_enable)?0:1);
			index = CROP_DS;
			break;
		case ISP_DS2_VIDEO_CHANNEL:
			apical_isp_top_bypass_ds2_crop_write((fmt && fmt->crop_enable)?0:1);
			index = CROP_DS2;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	if(ret < 0)
		return ret;
	api.type = TIMAGE;
	api.dir = COMMAND_SET;
	if(fmt && fmt->crop_enable){
		api.value = (index << 16) + fmt->crop_width;
		api.id = IMAGE_RESIZE_WIDTH_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
		//	printk("[%d]apical command: status = %d, ret = 0x%08x\n",__LINE__, status, ret);

		api.value = (index << 16) + fmt->crop_height;
		api.id = IMAGE_RESIZE_HEIGHT_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
		//	printk("[%d]apical command: status = %d, ret = 0x%08x\n",__LINE__, status, ret);

		api.value = (index << 16) + fmt->crop_left;
		api.id = IMAGE_CROP_XOFFSET_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
		//	printk("[%d]apical command: status = %d, ret = 0x%08x\n",__LINE__, status, ret);

		api.value = (index << 16) + fmt->crop_top;
		api.id = IMAGE_CROP_YOFFSET_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
		//	printk("[%d]apical command: status = %d, ret = 0x%08x\n",__LINE__, status, ret);

		api.value = (index << 16) + ENABLE;
		api.id = IMAGE_RESIZE_ENABLE_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
		//	printk("[%d]apical command: status = %d, ret = 0x%08x\n",__LINE__, status, ret);

	}else{
		api.value = (index << 16) + DISABLE;
		api.id = IMAGE_RESIZE_ENABLE_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
	}
	return ret;
}

static int ispcore_frame_channel_set_scaler(struct isp_core_output_channel *chan, struct frame_image_format *fmt)
{
	apical_api_control_t api;
	unsigned int index;
	unsigned char status = 0;
	int ret = ISP_SUCCESS;

	switch(chan->index){
	case ISP_DS1_VIDEO_CHANNEL:
		apical_isp_top_bypass_ds1_scaler_write((fmt && fmt->scaler_enable)?0:1);
		apical_isp_ds1_scaler_imgrst_write(1);
		index = SCALER;
		break;
	case ISP_DS2_VIDEO_CHANNEL:
		apical_isp_top_bypass_ds2_scaler_write((fmt && fmt->scaler_enable)?0:1);
		apical_isp_ds2_scaler_imgrst_write(1);
		index = SCALER2;
		break;
	case ISP_FR_VIDEO_CHANNEL:
	default:
		ret = -EINVAL;
		break;
	}
	if(ret < 0)
		return ret;
	api.type = TIMAGE;
	api.dir = COMMAND_SET;
	if(fmt && fmt->scaler_enable){
		api.value = (index << 16) + DISABLE;
		api.id = IMAGE_RESIZE_ENABLE_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);

		api.value = (index << 16) + fmt->scaler_out_width;
		api.id = IMAGE_RESIZE_WIDTH_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
		//	printk("[%d]apical command: status = %d, ret = 0x%08x\n",__LINE__, status, ret);

		api.value = (index << 16) + fmt->scaler_out_height;
		api.id = IMAGE_RESIZE_HEIGHT_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
		//	printk("[%d]apical command: status = %d, ret = 0x%08x\n",__LINE__, status, ret);
		switch(chan->index){
		case ISP_DS1_VIDEO_CHANNEL:
			apical_isp_ds1_scaler_imgrst_write(0);
			break;
		case ISP_DS2_VIDEO_CHANNEL:
			apical_isp_ds2_scaler_imgrst_write(0);
			break;
		case ISP_FR_VIDEO_CHANNEL:
		default:
			ret = -EINVAL;
			break;
		}

		api.value = (index << 16) + ENABLE;
		api.id = IMAGE_RESIZE_ENABLE_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
		//	printk("[%d]apical command: status = %d, ret = 0x%08x\n",__LINE__, status, ret);	return ret;

	}else{
		api.value = (index << 16) + DISABLE;
		api.id = IMAGE_RESIZE_ENABLE_ID;
		status = apical_command(api.type, api.id, api.value, api.dir, &ret);
	}

	return ret;
}

static int ispcore_frame_channel_streamon(struct tx_isp_subdev_pad *pad)
{
	struct isp_core_output_channel *chan = pad->priv;
	struct tx_isp_core_device *core = chan->priv;
	/*struct isp_core_input_control *contrl = &core->contrl;*/
	struct frame_image_format *fmt = NULL;
	unsigned int base = 0x00b00 + 0x100 * chan->index; // the base of address of dma channel write
	unsigned int primary;
	unsigned long flags = 0;
	int retry = 0;

	if(pad->state != TX_ISP_PADSTATE_LINKED){
		ISP_ERROR("ISP pad state is wrong!\n");
		return -EPERM;
	}

	if(core->state != TX_ISP_MODULE_RUNNING){
		ISP_ERROR("Please enable isp firstly!\n");
		return -EPERM;
	}

	fmt = &(chan->fmt);

	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		struct frame_channel_buffer buf;
		chan->usingbanks = 2;
		buf.addr = ISP_LFB_DEFAULT_BUF_BASE0;
		/*buf.addr = 0x04800000;*/
		isp_set_buffer_address_vflip_disable(chan, &buf, 0);
		/*buf.addr = 0x04af7600;*/
		buf.addr = ISP_LFB_DEFAULT_BUF_BASE1;
		isp_set_buffer_address_vflip_disable(chan, &buf, 1);
		/* configure lfb */
		switch(chan->index){
			case ISP_FR_VIDEO_CHANNEL:
				isp_lfb_ctrl_select_fr();
				break;
			case ISP_DS1_VIDEO_CHANNEL:
				isp_lfb_ctrl_select_ds1();
				break;
			default:
				break;
		}
		isp_lfb_config_resolution(fmt->pix.width, fmt->pix.height);
		isp_lfb_ctrl_hw_recovery(1);
		//isp_lfb_ctrl_flb_enable(1);
		g_switch_lfb_on = 1;
		retry = 100;
		while (g_switch_lfb_on) {
			private_msleep(5);
			retry--;
			if (0 == retry) {
				ISP_ERROR("error: lfb open failed!\n");
				break;
			}
		}
	}

	memset(chan->bank_flag, 0 ,sizeof(chan->bank_flag));
	memset(chan->vflip_flag, 0 ,sizeof(chan->vflip_flag));
	memset(chan->banks_addr, 0 ,sizeof(chan->banks_addr));
	chan->dma_state = 0;
	chan->vflip_state = 0xff;
	chan->state = TX_ISP_MODULE_ACTIVATE;

	switch(fmt->pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			/* dma channel uv write */
			//			APICAL_WRITE_32(base + 0xa4, 0x03);// axi_port_enable set 1, frame_write_cancel set 1.
			primary = APICAL_READ_32(base + 0x80);
			primary |= (1 << 9);
			APICAL_WRITE_32(base + 0x80, primary);
			APICAL_WRITE_32(base + 0x9c, chan->usingbanks > 0 ? chan->usingbanks -1 : 0);//MAX BANK
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YUV444:
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_BGR24:
		case V4L2_PIX_FMT_BGR32:
		case V4L2_PIX_FMT_RGB310:
		case V4L2_PIX_FMT_SBGGR12:
		case V4L2_PIX_FMT_SGBRG12:
		case V4L2_PIX_FMT_SGRBG12:
		case V4L2_PIX_FMT_SRGGB12:
			/* dma channel y write */
			//			APICAL_WRITE_32(base + 0x24, 0x03);// axi_port_enable set 1, frame_write_cancel set 1.
			primary = APICAL_READ_32(base);
			primary |= (1 << 9);
			APICAL_WRITE_32(base, primary);
			APICAL_WRITE_32(base + 0x1c, chan->usingbanks > 0 ? chan->usingbanks -1 : 0);//MAX BANK
			break;
		default:
			break;
	}

	/* streamon */
	private_spin_lock_irqsave(&chan->slock, flags);
	chan->state = TX_ISP_MODULE_RUNNING;
	pad->state = TX_ISP_PADSTATE_STREAM;
	private_spin_unlock_irqrestore(&chan->slock, flags);
	apical_isp_input_port_field_mode_write(0); // Temporary measures

#if 0
	printk("0x%08x = 0x%08x\n",0x40, APICAL_READ_32(0x40));
	printk("0x%08x = 0x%08x\n",0x44, APICAL_READ_32(0x44));
	printk("0x%08x = 0x%08x\n", base + 0x00, APICAL_READ_32(base + 0x00));
	printk("0x%08x = 0x%08x\n", base + 0x04, APICAL_READ_32(base + 0x04));
	printk("0x%08x = 0x%08x\n", base + 0x1c, APICAL_READ_32(base + 0x1c));
	printk("0x%08x = 0x%08x\n", base + 0x20, APICAL_READ_32(base + 0x20));
	printk("0x%08x = 0x%08x\n", base + 0x80 + 0x00, APICAL_READ_32(base + 0x80 + 0x00));
	printk("0x%08x = 0x%08x\n", base + 0x80 + 0x04, APICAL_READ_32(base + 0x80 + 0x04));
	printk("0x%08x = 0x%08x\n", base + 0x80 + 0x1c, APICAL_READ_32(base + 0x80 + 0x1c));
	printk("0x%08x = 0x%08x\n", base + 0x80 + 0x20, APICAL_READ_32(base + 0x80 + 0x20));
	{
		unsigned int reg;
		for(reg = 0x80; reg < 0xa4; reg = reg + 4)
			printk("0x%08x = 0x%08x\n", reg, APICAL_READ_32(reg));
		for(reg = 0x640; reg < 0x66c; reg = reg + 4)
			printk("0x%08x = 0x%08x\n", reg, APICAL_READ_32(reg));
	}
#endif
	return ISP_SUCCESS;
}

static int ispcore_frame_channel_streamoff(struct tx_isp_subdev_pad *pad)
{
	struct isp_core_output_channel *chan = pad->priv;
	struct tx_isp_core_device *core = chan->priv;
	struct isp_core_input_control *contrl = &core->contrl;
	struct frame_channel_format *cfmt;
	unsigned int base = 0x00b00 + 0x100 * chan->index; // the base of address of dma channel write
	unsigned int primary;
	unsigned long flags = 0;
	int ret = ISP_SUCCESS;
	int retry = 0;

	if(pad->state != TX_ISP_PADSTATE_STREAM)
		return ret;

	spin_lock_irqsave(&chan->slock, flags);
	if(chan->state != TX_ISP_MODULE_RUNNING){
		private_spin_unlock_irqrestore(&chan->slock, flags);
		return ISP_SUCCESS;
	}
	private_spin_unlock_irqrestore(&chan->slock, flags);

	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		g_switch_lfb_off = 1;
		retry = 100;
		while (g_switch_lfb_off) {
			private_msleep(5);
			retry--;
			if (0 == retry) {
				ISP_ERROR("error: lfb close failed!\n");
				break;
			}
		}
		//isp_lfb_ctrl_flb_enable(0);
	}
	switch(chan->fmt.pix.pixelformat){
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			/* dma channel uv write */
			while(APICAL_READ_32(base + 0xa4) & (1<<16));
			primary = APICAL_READ_32(base + 0x80);
			primary &= ~(1 << 9);
			APICAL_WRITE_32(base + 0x80, primary);
			APICAL_WRITE_32(base + 0x9c, 0x08);// axi_port_enable set 0, frame_write_cancel set 1.
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YUV444:
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_BGR24:
		case V4L2_PIX_FMT_BGR32:
		case V4L2_PIX_FMT_RGB310:
			/* dma channel y write */
			while(APICAL_READ_32(base + 0x24) & (1<<16));
			primary = APICAL_READ_32(base);
			primary &= ~(1 << 9);
			APICAL_WRITE_32(base, primary);
			APICAL_WRITE_32(base + 0x1c, 0x08);// axi_port_enable set 0, frame_write_cancel set 1.
			break;
		default:
			break;
	}

	/* streamoff */
	private_spin_lock_irqsave(&chan->slock, flags);
	chan->state = TX_ISP_MODULE_ACTIVATE;
	pad->state = TX_ISP_PADSTATE_LINKED;
	cleanup_buffer_fifo(&chan->fifo);
	cleanup_chan_banks(chan);
	private_spin_unlock_irqrestore(&chan->slock, flags);

	/* reset output parameters */
	cfmt = &(isp_output_fmt[contrl->fmt_end]);
	chan->fmt.pix.width = contrl->inwidth;
	chan->fmt.pix.height = contrl->inheight;
	chan->fmt.pix.pixelformat = cfmt->fourcc;
	chan->fmt.pix.bytesperline = chan->fmt.pix.width * cfmt->depth / 8;
	chan->fmt.pix.sizeimage = chan->fmt.pix.bytesperline * chan->fmt.pix.height;
	chan->fmt.crop_enable = false;
	chan->fmt.scaler_enable = false;
	chan->usingbanks = 0;

	return ret;
}

static int ispcore_frame_channel_qbuf(struct tx_isp_subdev_pad *pad, void *data)
{
	struct frame_channel_buffer *buf = NULL;
	struct isp_core_output_channel *chan = NULL;
	unsigned long flags = 0;

	buf = data;
	chan = pad->priv;

	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		return 0;
	}

	/*printk("## %s %d buf = 0x%08x ##\n", __func__,__LINE__, buf->addr);*/
	if(buf && chan){
		private_spin_lock_irqsave(&chan->slock, flags);
		push_buffer_fifo(&chan->fifo, buf);
		private_spin_unlock_irqrestore(&chan->slock, flags);
	}
	return 0;
}

static int ispcore_frame_channel_freebufs(struct tx_isp_subdev_pad *pad, void *data)
{
	struct isp_core_output_channel *chan = NULL;
	unsigned long flags = 0;

	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		return 0;
	}

	chan = pad->priv;
	if(chan){
		private_spin_lock_irqsave(&chan->slock, flags);
		cleanup_buffer_fifo(&chan->fifo);
		private_spin_unlock_irqrestore(&chan->slock, flags);
	}
	return 0;
}

static inline int ispcore_frame_channel_get_fmt(struct tx_isp_subdev_pad *pad, void *data)
{
	struct isp_core_output_channel *chan = NULL;
	struct tx_isp_core_device *core = NULL;
	struct frame_image_format *fmt;
	struct isp_core_input_control *contrl = NULL;
	struct frame_channel_format *cfmt;

	chan = pad->priv;
	if(data && chan){
		core = chan->priv;
		contrl = &core->contrl;
		if(core->bypass == TX_ISP_FRAME_CHANNEL_BYPASS_ISP_ENABLE){
			cfmt = &(isp_output_fmt[contrl->fmt_end]);
			fmt = data;

			fmt->pix.width = chan->fmt.pix.width;
			fmt->pix.height = chan->fmt.pix.height;
			fmt->pix.pixelformat = cfmt->fourcc;
			fmt->pix.sizeimage = chan->fmt.pix.sizeimage;
			fmt->crop_enable = 0;
			fmt->scaler_enable = 0;
		}else{
			memcpy(data, &(chan->fmt), sizeof(struct frame_image_format));
		}
	}
	return 0;
}

static int ispcore_frame_channel_set_fmt(struct tx_isp_subdev_pad *pad, void *data)
{
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(pad) ? NULL : pad->sd;
	struct tx_isp_core_device *core = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct isp_core_output_channel *chan = NULL;
	struct frame_image_format *fmt = NULL;
	struct v4l2_mbus_framefmt *mbus = NULL;
	unsigned int width = 0;
	unsigned int height = 0;
	int ret = 0;

	if(IS_ERR_OR_NULL(core)){
		return -EINVAL;
	}

	chan = pad->priv;
	mbus = &core->vin.mbus;
	fmt = data;

	if((core->bypass == TX_ISP_FRAME_CHANNEL_BYPASS_ISP_ENABLE) && (fmt->crop_enable | fmt->scaler_enable)){
		ISP_ERROR("Can't set chan%d fmt when bypass ispcore!\n", chan->index);
		return -EPERM;
	}

	if((mbus->code == V4L2_MBUS_FMT_YUYV8_1X16) && (fmt->crop_enable | fmt->scaler_enable)){
		ISP_ERROR("Chan%d can't be set fmt when sensor is YUYV8_1x16!\n", chan->index);
		return -EINVAL;
	}

	/* check format */
	if(fmt->crop_enable == true && chan->has_crop == false){
		ISP_ERROR("Chan%d can't be cropped!\n", chan->index);
		return -EINVAL;
	}
	if(fmt->scaler_enable == true && chan->has_scaler == false){
		ISP_ERROR("Chan%d can't be scaler!\n", chan->index);
		return -EINVAL;
	}

	width = chan->fmt.pix.width;
	height = chan->fmt.pix.height;
	/*printk("## %s %d; width=%d height=%d ##\n",__func__,__LINE__,width,height);*/
	if(fmt->crop_enable){
		if(fmt->crop_top + fmt->crop_height > chan->max_height ||
				fmt->crop_left + fmt->crop_width > chan->max_width){
		ISP_ERROR("The chan%d crop %d*%d, %d*%d!\n", chan->index, fmt->crop_left, fmt->crop_top, fmt->crop_width, fmt->crop_height);
		return -EINVAL;
		}
		width = fmt->crop_width;
		height = fmt->crop_height;
	}

	if(fmt->scaler_enable){
		if(width < fmt->scaler_out_width || height < fmt->scaler_out_height){
			ISP_ERROR("The chan%d in-scaler %d*%d out-scaler %d*%d!\n", chan->index, width, height, fmt->scaler_out_width, fmt->scaler_out_height);
			return -EINVAL;
		}
		width = fmt->scaler_out_width;
		height = fmt->scaler_out_height;
	}

	if(width < chan->min_width || height < chan->min_height){
			ISP_ERROR("The chan%d Can't output %d*%d!\n", chan->index, width, height);
			return -EINVAL;
	}

	if(width != fmt->pix.width || height != fmt->pix.height){
			ISP_ERROR("The chan%d output %d*%d error!\n", chan->index, width, height);
			return -EINVAL;
	}
	/* try fmt */
	ret = ispcore_frame_channel_try_fmt(core, fmt);
	if(ret){
		char *value = (char *)&(fmt->pix.pixelformat);
		ISP_ERROR("ISP can't support the fmt(%c%c%c%c)\n", value[0], value[1], value[2], value[3]);
		return ret;
	}
	/* set fmt */
	if(fmt->crop_enable){
		ret = ispcore_frame_channel_set_crop(chan, fmt);
		if(ret){
			ISP_ERROR("Failed to set crop!\n");
			goto exit;
		}
	}

	if(fmt->scaler_enable){
		ret = ispcore_frame_channel_set_scaler(chan, fmt);
		if(ret){
			ISP_ERROR("Failed to set scaler!\n");
			goto failed_scaler;
		}
	}
	ret = ispcore_frame_channel_s_fmt(chan, fmt);
	if(ret){
		goto failed_s_fmt;
	}
	chan->fmt = *fmt;
	return 0;
failed_s_fmt:
	if(fmt->scaler_enable)
		ret = ispcore_frame_channel_set_scaler(chan, NULL);
failed_scaler:
	if(fmt->crop_enable)
		ispcore_frame_channel_set_crop(chan, NULL);
exit:
	return ret;
}

static inline int ispcore_frame_channel_s_banks(struct tx_isp_subdev_pad *pad, void *data)
{
	struct isp_core_output_channel *chan = pad->priv;
	int reqbufs = *(int*)data;

	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		return 0;
	}

	chan->usingbanks = reqbufs > ISP_DMA_WRITE_MAXBASE_NUM ? ISP_DMA_WRITE_MAXBASE_NUM : reqbufs;
	/*printk("## %s %d banks = %d ##\n", __func__,__LINE__, chan->usingbanks);*/
	return 0;
}

static int ispcore_pad_event_handle(struct tx_isp_subdev_pad *pad, unsigned int event, void *data)
{
	int ret = 0;

	if(pad->type == TX_ISP_MODULE_UNDEFINE)
		return 0;

	switch(event){
		case TX_ISP_EVENT_FRAME_CHAN_BYPASS_ISP:
			break;
		case TX_ISP_EVENT_FRAME_CHAN_GET_FMT:
			ret = ispcore_frame_channel_get_fmt(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_SET_FMT:
			ret = ispcore_frame_channel_set_fmt(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_STREAM_ON:
			ret = ispcore_frame_channel_streamon(pad);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF:
			ret = ispcore_frame_channel_streamoff(pad);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER:
			ret = ispcore_frame_channel_qbuf(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_FREE_BUFFER:
			ret = ispcore_frame_channel_freebufs(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_SET_BANKS:
			ret = ispcore_frame_channel_s_banks(pad, data);
			break;
		default:
			break;
	}
	return ret;
}

static int ispcore_core_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = ISP_SUCCESS;
	/*struct tx_isp_core_device *core = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);	*/
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;

	/* This module operation */
	switch(cmd){
		case TX_ISP_EVENT_SYNC_SENSOR_ATTR:
			ret = tx_isp_subdev_call(sd, sensor, sync_sensor_attr, arg);
			break;
		case TX_ISP_EVENT_SUBDEV_INIT:
			ret = tx_isp_subdev_call(sd, core, init, *(int*)arg);
			break;
		default:
			break;
	}

	if(ret && ret != -ENOIOCTLCMD){
		ISP_ERROR("%s %d Failed to ioctl!\n", __func__, __LINE__);
		return ret;
	}

	/* its sub-modules are as follow */
	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		submod = sd->module.submods[index];
		if(submod){
			subdev = module_to_subdev(submod);
			switch(cmd){
				case TX_ISP_EVENT_SYNC_SENSOR_ATTR:
					ret = tx_isp_subdev_call(subdev, sensor, sync_sensor_attr, arg);
					break;
				case TX_ISP_EVENT_SUBDEV_INIT:
					ret = tx_isp_subdev_call(subdev, core, init, *(int*)arg);
					break;
				default:
					break;
			}
			/*ret = tx_isp_subdev_call(subdev, core, ioctl, cmd, arg);*/
			if(ret && ret != -ENOIOCTLCMD)
				break;
		}
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;

	return ret;
}

static void isp_core_config_frame_channel_attr(struct tx_isp_core_device *core)
{
	struct isp_core_input_control *contrl = &core->contrl;
	struct isp_core_output_channel *chan = core->chans;
	struct frame_channel_format *cfmt;
	int index = 0;

	/*printk("## %s %d; control->w = %d, h = %d ##\n",__func__,__LINE__,contrl->inwidth,contrl->inheight);*/
	cfmt = &(isp_output_fmt[contrl->fmt_end]);
	for(index = 0; index < core->num_chans; index++){
		chan = &(core->chans[index]);
		if(chan->state == TX_ISP_MODULE_UNDEFINE)
			continue;
		chan->fmt.pix.width = contrl->inwidth;
		chan->fmt.pix.height = contrl->inheight;
		chan->fmt.pix.pixelformat = cfmt->fourcc;
		chan->fmt.pix.bytesperline = chan->fmt.pix.width * cfmt->depth / 8;
		chan->fmt.pix.sizeimage = chan->fmt.pix.bytesperline * chan->fmt.pix.height;
		//	printk("## %s[%d] width = %d height = %d pixformat = 0x%08x ##\n",
		//		__func__,__LINE__,contrl->inwidth,contrl->inheight,cfmt->fourcc);
	}
}

static int isp_config_input_port(struct tx_isp_core_device *core)
{
	struct v4l2_mbus_framefmt *mbus = &core->vin.mbus;
	struct isp_core_input_control *contrl = &core->contrl;
	unsigned char color = APICAL_ISP_TOP_RGGB_START_DEFAULT;
	int ret = ISP_SUCCESS;

	if(mbus->width > TX_ISP_INPUT_PORT_MAX_WIDTH || mbus->height > TX_ISP_INPUT_PORT_MAX_HEIGHT){
		printk("Sensor outputs bigger resolution than that ISP device can't deal with!\n");
		return -1;
	}
	switch(mbus->code){
		case V4L2_MBUS_FMT_SBGGR8_1X8:
		case V4L2_MBUS_FMT_SBGGR10_DPCM8_1X8:
		case V4L2_MBUS_FMT_SBGGR10_2X8_PADHI_BE:
		case V4L2_MBUS_FMT_SBGGR10_2X8_PADHI_LE:
		case V4L2_MBUS_FMT_SBGGR10_2X8_PADLO_BE:
		case V4L2_MBUS_FMT_SBGGR10_2X8_PADLO_LE:
		case V4L2_MBUS_FMT_SBGGR10_1X10:
		case V4L2_MBUS_FMT_SBGGR12_1X12:
			color = APICAL_ISP_TOP_RGGB_START_B_GB_GR_R;
			memcpy(isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].name, "Bayer formats (sBGGR)", 32);
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].fourcc = V4L2_PIX_FMT_SBGGR12;
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].depth = 16;
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].priv = DMA_FORMAT_RAW16;
			contrl->fmt_start = APICAL_ISP_INPUT_RAW_FMT_INDEX_START;
			contrl->fmt_end = APICAL_ISP_INPUT_RAW_FMT_INDEX_END;
			break;
		case V4L2_MBUS_FMT_SGBRG8_1X8:
		case V4L2_MBUS_FMT_SGBRG10_DPCM8_1X8:
		case V4L2_MBUS_FMT_SGBRG10_1X10:
		case V4L2_MBUS_FMT_SGBRG12_1X12:
			color = APICAL_ISP_TOP_RGGB_START_GB_B_R_GR;
			memcpy(isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].name, "Bayer formats (sGBRG)", 32);
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].fourcc = V4L2_PIX_FMT_SGBRG12;
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].depth = 16;
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].priv = DMA_FORMAT_RAW16;
			contrl->fmt_start = APICAL_ISP_INPUT_RAW_FMT_INDEX_START;
			contrl->fmt_end = APICAL_ISP_INPUT_RAW_FMT_INDEX_END;
			break;
		case V4L2_MBUS_FMT_SGRBG8_1X8:
		case V4L2_MBUS_FMT_SGRBG10_DPCM8_1X8:
		case V4L2_MBUS_FMT_SGRBG10_1X10:
		case V4L2_MBUS_FMT_SGRBG12_1X12:
			color = APICAL_ISP_TOP_RGGB_START_GR_R_B_GB;
			memcpy(isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].name, "Bayer formats (sGRBG)", 32);
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].fourcc = V4L2_PIX_FMT_SGRBG12;
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].depth = 16;
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].priv = DMA_FORMAT_RAW16;
			contrl->fmt_start = APICAL_ISP_INPUT_RAW_FMT_INDEX_START;
			contrl->fmt_end = APICAL_ISP_INPUT_RAW_FMT_INDEX_END;
			break;
		case V4L2_MBUS_FMT_SRGGB8_1X8:
		case V4L2_MBUS_FMT_SRGGB10_DPCM8_1X8:
		case V4L2_MBUS_FMT_SRGGB10_1X10:
		case V4L2_MBUS_FMT_SRGGB12_1X12:
			color = APICAL_ISP_TOP_RGGB_START_R_GR_GB_B;
			memcpy(isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].name, "Bayer formats (sRGGB)", 32);
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].fourcc = V4L2_PIX_FMT_SRGGB12;
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].depth = 16;
			isp_output_fmt[APICAL_ISP_RAW_FMT_INDEX].priv = DMA_FORMAT_RAW16;
			contrl->fmt_start = APICAL_ISP_INPUT_RAW_FMT_INDEX_START;
			contrl->fmt_end = APICAL_ISP_INPUT_RAW_FMT_INDEX_END;
			break;
		case V4L2_MBUS_FMT_RGB565_2X8_LE:
			contrl->fmt_start = APICAL_ISP_INPUT_RGB565_FMT_INDEX_START;
			contrl->fmt_end = APICAL_ISP_INPUT_RGB565_FMT_INDEX_END;
			apical_isp_top_isp_raw_bypass_write(1);
			break;
		case V4L2_MBUS_FMT_RGB888_3X8_LE:
			contrl->fmt_start = APICAL_ISP_INPUT_RGB888_FMT_INDEX_START;
			contrl->fmt_end = APICAL_ISP_INPUT_RGB888_FMT_INDEX_END;
			apical_isp_top_isp_raw_bypass_write(1);
			break;
		case V4L2_MBUS_FMT_YUYV8_1X16:
			contrl->fmt_start = APICAL_ISP_INPUT_YUV_FMT_INDEX_START;
			contrl->fmt_end = APICAL_ISP_INPUT_YUV_FMT_INDEX_END;
			apical_isp_top_isp_processing_bypass_mode_write(3);
			apical_isp_top_isp_raw_bypass_write(1);
			break;
		default:
			contrl->fmt_start = contrl->fmt_end = APICAL_ISP_INPUT_RAW_FMT_INDEX_END;
			ISP_ERROR("%s[%d] the format(0x%08x) of input couldn't be handled!\n",
					__func__,__LINE__, mbus->code);
			ret = -EPERM;
			break;
	}
	if(ret == ISP_SUCCESS){
		contrl->inwidth = mbus->width;
		contrl->inheight = mbus->height;
		contrl->pattern = color;
		apical_isp_top_rggb_start_write(color); //Starting color of the rggb pattern
		if(core->state != TX_ISP_MODULE_RUNNING){
			apical_isp_top_active_width_write(contrl->inwidth);
			apical_isp_top_active_height_write(contrl->inheight);
			isp_core_config_frame_channel_attr(core);
		}
	}
	return ret;
}

static int inline isp_core_video_streamon(struct tx_isp_core_device *core)
{
	apical_api_control_t api;
	unsigned int status, reason;
	int ret = ISP_SUCCESS;

	api.type = TSYSTEM;
	api.id = ISP_SYSTEM_STATE;
	api.dir = COMMAND_SET;
	api.value = PAUSE;
	status = apical_command(api.type, api.id, api.value, api.dir, &ret);
	/* 2-->module config updates during local vertical blanking */
	apical_isp_top_config_buffer_mode_write(2);
	/* set input port mode, mode1 */
	APICAL_WRITE_32(0x100, 0x00100001);

	/* enable default isp modules and disable bypass isp process */
	core->top.reg.low = APICAL_READ_32(0x40);
	core->top.reg.high = APICAL_READ_32(0x44);
	core->bypass = TX_ISP_FRAME_CHANNEL_BYPASS_ISP_DISABLE;

	/* config isp input port */
	isp_config_input_port(core);

	/*
	 * clear interrupts state of isp-core.
	 * Interrupt event clear register writing 0-1 transition will clear the corresponding status bits.
	 */
	apical_isp_interrupts_interrupt_clear_write(0);
	apical_isp_interrupts_interrupt_clear_write(0xffff);
	/* unmask isp's top interrupts */
	api.type = TSYSTEM;
	api.id = ISP_SYSTEM_STATE;
	api.dir = COMMAND_SET;
	api.value = RUN;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if(status == ISP_SUCCESS){
		core->state = TX_ISP_MODULE_RUNNING;
	}else{
		ISP_ERROR("%s[%d] state = %d, reason = %d!\n",
				__func__,__LINE__, status, ret);
		ret = -EPERM;
	}
	return ret;
}


static int ispcore_video_s_stream(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	apical_api_control_t api;
	unsigned int status = ISP_SUCCESS, reason;
	unsigned long flags = 0;
	struct isp_core_output_channel *chan;
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev *subdev = NULL;
	int ret = ISP_SUCCESS;
	int index = 0;

	private_spin_lock_irqsave(&core->slock, flags);
	if(core->state < TX_ISP_MODULE_INIT){
		ISP_ERROR("%s[%d] the device hasn't been inited!\n",__func__,__LINE__);
		private_spin_unlock_irqrestore(&core->slock, flags);
		return -EPERM;
	}
	private_spin_unlock_irqrestore(&core->slock, flags);
	/*printk("~~~~~~ %s[%d] enable = %d ~~~~~~\n",__func__, __LINE__, enable);*/
	core->frame_state = 0;
	core->vflip_state = 0;
	core->hflip_state = 0;
	core->frame_sequeue = 0;
	if(enable){
		/* streamon */
		if(core->state == TX_ISP_MODULE_INIT){
			ret = isp_core_video_streamon(core);
		}
	}else{
		/* streamoff */
		if(core->state == TX_ISP_MODULE_RUNNING){
			/* At least one channel is in streamon state. */
			/* Firstly we must be streamoff them */
			for(index = 0; index < ISP_MAX_OUTPUT_VIDEOS; index++){
				chan = &(core->chans[index]);
				if(chan->state == TX_ISP_MODULE_RUNNING){
					ispcore_frame_channel_streamoff(chan->pad);
				}
			}
			/* all frame channels are streamoff state. */
			api.type = TSYSTEM;
			api.id = ISP_SYSTEM_STATE;
			api.dir = COMMAND_SET;
			api.value = PAUSE;
			status = apical_command(api.type, api.id, api.value, api.dir, &reason);
			if(status == ISP_SUCCESS){
				core->state = TX_ISP_MODULE_INIT;
			}else{
				ISP_ERROR("%s[%d] status = %d, reason = %d!\n",
						__func__,__LINE__, status, ret);
				ret = -EINVAL;
			}
		}
	}

	/* its sub-modules are as follow */
	for(index = TX_ISP_ENTITY_ENUM_MAX_DEPTH; index >= 0 ; index--){
		submod = sd->module.submods[index];
		if(submod){
			/*printk("## %s %d submod %s ##\n", __func__,__LINE__, submod->name);*/
			subdev = module_to_subdev(submod);
			ret = tx_isp_subdev_call(subdev, video, s_stream, enable);
			if(ret && ret != -ENOIOCTLCMD)
				break;
		}
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;
	return ret;
}

/*
 * the ops of isp's clks
 */
static void ispcore_clks_ops(struct tx_isp_subdev *sd, int on)
{
	struct clk **clks = sd->clks;
	unsigned long rate = 0;
	int i = 0;

	if(on){
		for(i = 0; i < sd->clk_num; i++){
			rate = private_clk_get_rate(clks[i]);
			if(rate != DUMMY_CLOCK_RATE)
				private_clk_set_rate(clks[i], isp_clk);
			private_clk_enable(clks[i]);
		}
	}else{
		for(i = sd->clk_num - 1; i >=0; i--)
			private_clk_disable(clks[i]);
	}
	return;
}

static int isp_fw_process(void *data)
{
	while(!kthread_should_stop()){
		apical_process();
#if ISP_HAS_CONNECTION_DEBUG && ISP_HAS_API
		apical_cmd_process();
#endif //ISP_HAS_API
#if ISP_HAS_STREAM_CONNECTION
		apical_connection_process();
#endif
	}
#if ISP_HAS_STREAM_CONNECTION
	apical_connection_destroy();
#endif
	return 0;
}

static int ispcore_core_ops_init(struct tx_isp_subdev *sd, int on)
{
	struct tx_isp_core_device *core = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;
	int ret = ISP_SUCCESS;

	if(IS_ERR_OR_NULL(core)){
		return -EINVAL;
	}

	if(core->state == TX_ISP_MODULE_SLAKE)
		return 0;
	/*printk("## %s %d on=%d ##\n", __func__,__LINE__, on);*/

	if(on){
		if(private_reset_tx_isp_module(TX_ISP_CORE_SUBDEV_ID)){
			ISP_ERROR("Failed to reset %s\n", sd->module.name);
			ret = -EINVAL;
			goto exit;
		}

		private_spin_lock_irqsave(&core->slock, flags);
		if(core->state != TX_ISP_MODULE_ACTIVATE){
			private_spin_unlock_irqrestore(&core->slock, flags);
			ISP_ERROR("Can't init ispcore when its state is %d \n!", core->state);
			ret = -EPERM;
			goto exit;
		}
		private_spin_unlock_irqrestore(&core->slock, flags);

		core->param = load_tx_isp_parameters(core->vin.attr);
		apical_init();
#if ISP_HAS_STREAM_CONNECTION
		apical_connection_init();
#endif
		system_program_interrupt_event(APICAL_IRQ_DS1_OUTPUT_END, 50);
#if TX_ISP_EXIST_DS2_CHANNEL
		system_program_interrupt_event(APICAL_IRQ_DS2_OUTPUT_END, 53);
#endif
		core->process_thread = kthread_run(isp_fw_process, NULL, "apical_isp_fw_process");
		if(IS_ERR_OR_NULL(core->process_thread)){
			ISP_ERROR("%s[%d] kthread_run was failed!\n",__func__,__LINE__);
			ret = -EINVAL;
			goto exit;
		}
		core->state = TX_ISP_MODULE_INIT;
		/*ispcore_video_s_stream(sd , 1);*/
	}else{
		if(core->state == TX_ISP_MODULE_RUNNING){
			ispcore_video_s_stream(sd, 0);
		}
		if(core->state == TX_ISP_MODULE_INIT){
			ret = kthread_stop(core->process_thread);
			isp_clear_irq_source();
			core->state = TX_ISP_MODULE_DEINIT;
		}
	}
	return 0;
exit:
	return ret;
}

static struct tx_isp_subdev_core_ops ispcore_subdev_core_ops ={
	.init = ispcore_core_ops_init,
	.ioctl = ispcore_core_ops_ioctl,
	.interrupt_service_routine = ispcore_interrupt_service_routine,
	.interrupt_service_thread = ispcore_irq_thread_handle,
};

static int ispcore_sync_sensor_attr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_core_device *core = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	if(IS_ERR_OR_NULL(core)){
		ISP_ERROR("The parameter is invalid!\n");
		return -EINVAL;
	}
	/*printk("## %s %d arg = %p ##\n", __func__,__LINE__, arg);*/
	if(arg){
		memcpy(&core->vin, (void *)arg, sizeof(struct tx_isp_video_in));
		/* isp_config_input_port(core); */
		stab.global_max_integration_time = core->vin.attr->max_integration_time;
	}else
		memset(&core->vin, 0, sizeof(struct tx_isp_video_in));
	return 0;
}

static int ispcore_sensor_ops_release_all_sensor(struct tx_isp_subdev *sd)
{
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;
	long ret = 0;

	/* its sub-modules are as follow */
	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		submod = sd->module.submods[index];
		if(submod){
			subdev = module_to_subdev(submod);
			ret = tx_isp_subdev_call(subdev, sensor, release_all_sensor);
			if(ret && ret != -ENOIOCTLCMD)
				break;
		}
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;

	return ret;
}

static int ispcore_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;
	int ret = 0;

	/* This module operation */


	/* its sub-modules are as follow */
	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		submod = sd->module.submods[index];
		if(submod){
			subdev = module_to_subdev(submod);
			ret = tx_isp_subdev_call(subdev, sensor, ioctl, cmd, arg);
			if(ret && ret != -ENOIOCTLCMD)
				break;
		}
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;

	return ret;
}

static struct tx_isp_subdev_sensor_ops ispcore_sensor_ops = {
	.release_all_sensor = ispcore_sensor_ops_release_all_sensor,
	.sync_sensor_attr = ispcore_sync_sensor_attr,
	.ioctl = ispcore_sensor_ops_ioctl,
};

static int ispcore_activate_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_core_device *core = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct tx_isp_module *module = NULL;
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev *widget = NULL;
	int index = 0;
	int ret = 0;

	if(IS_ERR_OR_NULL(core))
		return -EINVAL;
	if(core->state != TX_ISP_MODULE_SLAKE)
		return 0;

	/* clk ops */
	ispcore_clks_ops(sd, 1);
	/* this module ops */
	for(index = 0; index < core->num_chans; index++){
		if(core->chans[index].state != TX_ISP_MODULE_SLAKE){
			ISP_ERROR("The state of channel%d is invalid when be activated!\n", index);
			return -1;
		}
		core->chans[index].state = TX_ISP_MODULE_ACTIVATE;
	}
	core->tuning->event(core->tuning, TX_ISP_EVENT_ACTIVATE_MODULE, NULL);

	/* sync all subwidget irqdev */
	module = &(core->sd.module);
	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(IS_ERR_OR_NULL(module->submods[index]))
			continue;
		submod = module->submods[index];
		widget = module_to_subdev(submod);
		memcpy(&widget->irqdev, &(sd->irqdev), sizeof(sd->irqdev));
		ret = tx_isp_subdev_call(widget, internal, activate_module);
		if(ret && ret != -ENOIOCTLCMD){
			ISP_ERROR("Failed to activate %s\n", submod->name);
			break;
		}
	}

	/* downscaler paramerters */
	for(index = 0; index < (sizeof(apical_downscaler_lut) / sizeof(apical_downscaler_lut[0])); index++)
		APICAL_WRITE_32(apical_downscaler_lut[index].reg, apical_downscaler_lut[index].value);
	core->state = TX_ISP_MODULE_ACTIVATE;
	return 0;
}

static int ispcore_slake_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_core_device *core = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct tx_isp_module *module = NULL;
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev *widget = NULL;
	int index = 0;
	int ret = 0;

	if(IS_ERR_OR_NULL(core))
		return -EINVAL;
	if(core->state == TX_ISP_MODULE_SLAKE)
		return 0;

	if(core->state > TX_ISP_MODULE_ACTIVATE){
		ispcore_core_ops_init(sd, 0);
	}

	for(index = 0; index < core->num_chans; index++){
		core->chans[index].state = TX_ISP_MODULE_SLAKE;
	}
	core->tuning->event(core->tuning, TX_ISP_EVENT_SLAVE_MODULE, NULL);
	core->state = TX_ISP_MODULE_SLAKE;

	/* sync all subwidget irqdev */
	module = &(core->sd.module);
	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(IS_ERR_OR_NULL(module->submods[index]))
			continue;
		submod = module->submods[index];
		widget = module_to_subdev(submod);
		widget->irqdev.irq = 0;
		ret = tx_isp_subdev_call(widget, internal, slake_module);
		if(ret && ret != -ENOIOCTLCMD){
			ISP_ERROR("Failed to slake %s\n", submod->name);
			break;
		}
	}

	/* clk ops */
	ispcore_clks_ops(sd, 0);
	return 0;
}

static int ispcore_link_setup(const struct tx_isp_subdev_pad *local,
			  const struct tx_isp_subdev_pad *remote, u32 flags)
{
	/*struct isp_core_output_channel *chan = local->priv;*/

	return 0;
}

static struct tx_isp_subdev_video_ops ispcore_subdev_video_ops = {
	.s_stream = ispcore_video_s_stream,
	.link_setup = ispcore_link_setup,
};

struct tx_isp_subdev_internal_ops ispcore_internal_ops = {
	.activate_module = &ispcore_activate_module,
	.slake_module = &ispcore_slake_module,
};

static struct tx_isp_subdev_ops core_subdev_ops = {
	.internal = &ispcore_internal_ops,
	.core = &ispcore_subdev_core_ops,
	.video = &ispcore_subdev_video_ops,
	.sensor = &ispcore_sensor_ops,
};

/*
 * ----------------- init device ------------------------
 */

static int isp_core_output_channel_init(struct tx_isp_core_device *core)
{
	struct tx_isp_subdev *sd = &core->sd;
	struct isp_core_output_channel *chans = NULL;
	struct isp_core_output_channel *chan = NULL;
	int ret = ISP_SUCCESS;
	int index = 0;

	core->num_chans = sd->num_outpads;
	chans = (struct isp_core_output_channel *)kzalloc(sizeof(*chans) * sd->num_outpads, GFP_KERNEL);
	if(!chans){
		ISP_ERROR("Failed to allocate sensor device\n");
		ret = -ENOMEM;
		goto exit;
	}

	for(index = 0; index < core->num_chans; index++){
		chan = &chans[index];
		chan->index = index;
		chan->pad = &(sd->outpads[index]);
		if(sd->outpads[index].type == TX_ISP_PADTYPE_UNDEFINE){
			chan->state = TX_ISP_MODULE_UNDEFINE;
			continue;
		}
		switch(index){
			case ISP_FR_VIDEO_CHANNEL:
				chan->max_width = TX_ISP_FR_CHANNEL_MAX_WIDTH;
				chan->max_height = TX_ISP_FR_CHANNEL_MAX_HEIGHT;
				chan->has_crop = true;
				chan->has_scaler = false;
				break;
			case ISP_DS1_VIDEO_CHANNEL:
				chan->max_width = TX_ISP_DS1_CHANNEL_MAX_WIDTH;
				chan->max_height = TX_ISP_DS1_CHANNEL_MAX_HEIGHT;
				chan->has_crop = true;
				chan->has_scaler = true;
				break;
			case ISP_DS2_VIDEO_CHANNEL:
				chan->max_width = TX_ISP_DS2_CHANNEL_MAX_WIDTH;
				chan->max_height = TX_ISP_DS2_CHANNEL_MAX_HEIGHT;
				chan->has_crop = true;
				chan->has_scaler = true;
				break;
			default:
				break;
		}
		chan->state = TX_ISP_MODULE_SLAKE;
		chan->min_width = 128;
		chan->min_height = 128;
		init_buffer_fifo(&(chan->fifo));
		private_spin_lock_init(&(chan->slock));
		chan->priv = core;
		sd->outpads[index].event = ispcore_pad_event_handle;
		sd->outpads[index].priv = chan;
	}

	core->chans = chans;
	return ISP_SUCCESS;
exit:
	return ret;
}

static int isp_core_output_channel_deinit(struct tx_isp_core_device *core)
{
	if(core->state > TX_ISP_MODULE_SLAKE)
		ispcore_slake_module(&core->sd);

	kfree(core->chans);
	core->chan_state = TX_ISP_MODULE_SLAKE;
	core->chans = NULL;

	return 0;
}

/* debug system node */
static int isp_info_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_core_device *core = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct tx_isp_video_in *vin = &core->vin;
	struct v4l2_mbus_framefmt *mbus = &core->vin.mbus;
	struct isp_core_input_control *contrl = &core->contrl;
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	char *colorspace = NULL;
	char *ae_strategy = NULL;
	unsigned int flicker = 0;
	unsigned int temperature = 0;
	unsigned int sensor_again = 0;
	unsigned int max_sensor_again = 0;
	unsigned int sensor_dgain = 0;
	unsigned int max_sensor_dgain = 0;
	unsigned int isp_dgain = 0;
	unsigned int max_isp_dgain = 0;
	unsigned int gain_log2_id = 0;
	unsigned int total_gain = 0;
	unsigned int exposure_log2_id = 0;
	unsigned int manual_exposure_time = 0;
	unsigned int saturation_target = 0;
	unsigned int saturation = 0;
	unsigned int contrast = 0;
	unsigned int sharpness = 0;
	unsigned int brightness = 0;
	int ret = 0;
	uint8_t evtolux = 0;

	len += seq_printf(m ,"****************** ISP INFO **********************\n");
	if(core->state < TX_ISP_MODULE_RUNNING){
		len += seq_printf(m ,"sensor doesn't work, please enable sensor\n");
		return len;
	}
	switch(contrl->pattern){
		case APICAL_ISP_TOP_RGGB_START_R_GR_GB_B:
			colorspace = "RGGB";
			break;
		case APICAL_ISP_TOP_RGGB_START_GR_R_B_GB:
			colorspace = "GRBG";
			break;
		case APICAL_ISP_TOP_RGGB_START_GB_B_R_GR:
			colorspace = "GBRB";
			break;
		case APICAL_ISP_TOP_RGGB_START_B_GB_GR_R:
			colorspace = "BGGR";
			break;
		default:
			colorspace = "The format of isp input is RGB or YUV422";
			break;
	}
	api.type = TALGORITHMS;
	api.dir = COMMAND_GET;
	api.id = ANTIFLICKER_MODE_ID;
	api.value = 0;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	flicker = reason;

	api.id = AWB_TEMPERATURE_ID;
	api.value = 0;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	temperature = reason * 100;

	api.id = AE_SPLIT_PRESET_ID;
	api.value = 0;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	switch(reason){
	case AE_SPLIT_BALANCED:
		ae_strategy = "AE_SPLIT_BALANCED";
		break;
	case AE_SPLIT_INTEGRATION_PRIORITY:
		ae_strategy = "AE_SPLIT_INTEGRATION_PRIORITY";
		break;
	default:
		ae_strategy = "AE_STRATEGY_BUTT";
		break;
	}

	api.type = TSYSTEM;
	api.dir = COMMAND_GET;
	api.value = 0;
	api.id = SYSTEM_SENSOR_ANALOG_GAIN;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	sensor_again = reason;

	api.id = SYSTEM_MAX_SENSOR_ANALOG_GAIN;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	max_sensor_again = reason;

	api.id = SYSTEM_SENSOR_DIGITAL_GAIN;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	sensor_dgain = reason;

	api.id = SYSTEM_MAX_SENSOR_DIGITAL_GAIN;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	max_sensor_dgain = reason;

	api.id = SYSTEM_ISP_DIGITAL_GAIN;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	isp_dgain = reason;

	api.id = SYSTEM_MAX_ISP_DIGITAL_GAIN;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	max_isp_dgain = reason;

	api.id = SYSTEM_SATURATION_TARGET;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	saturation_target = reason;

	api.id = SYSTEM_MANUAL_EXPOSURE_TIME;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	manual_exposure_time = reason;

	api.type = CALIBRATION;
	api.id = GAIN_LOG2_ID;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	gain_log2_id = reason;

	api.id = EXPOSURE_LOG2_ID;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	exposure_log2_id = reason;

	api.type = TSCENE_MODES;
	api.dir = COMMAND_GET;
	api.id = SHARPENING_STRENGTH_ID;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	sharpness = reason;

	api.id = BRIGHTNESS_STRENGTH_ID;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	brightness = reason;

	api.id = SATURATION_STRENGTH_ID;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	saturation = reason;

	api.id = CONTRAST_STRENGTH_ID;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	contrast = reason;

	apical_api_calibration(CALIBRATION_EVTOLUX_PROBABILITY_ENABLE,COMMAND_GET,(void *) (&evtolux), 1, &ret);
	total_gain =  stab.global_sensor_analog_gain  + stab.global_sensor_digital_gain + stab.global_isp_digital_gain;
	total_gain = math_exp2(total_gain, 5, 8);

	len += seq_printf(m ,"Software Version : %s\n", SOFT_VERSION);
	len += seq_printf(m ,"Firmware Version : %s\n", FIRMWARE_VERSION);
	len += seq_printf(m ,"SENSOR NAME : %s\n", vin->attr->name);
	len += seq_printf(m ,"SENSOR OUTPUT WIDTH : %d\n", mbus->width);
	len += seq_printf(m ,"SENSOR OUTPUT HEIGHT : %d\n", mbus->height);
	len += seq_printf(m ,"SENSOR OUTPUT RAW PATTERN : %s\n", colorspace);
	len += seq_printf(m ,"SENSOR Integration Time : %d lines\n", stab.global_integration_time);
	len += seq_printf(m ,"ISP Top Value : 0x%x\n", APICAL_READ_32(0x40));
	len += seq_printf(m ,"ISP Runing Mode : %s\n", ((apical_isp_ds1_cs_conv_clip_min_uv_read() == 512) ? "Night" : "Day"));
	len += seq_printf(m ,"ISP OUTPUT FPS : %d / %d\n", vin->fps >> 16, vin->fps & 0xffff);
	len += seq_printf(m ,"SENSOR analog gain : %d\n", sensor_again);
	len += seq_printf(m ,"MAX SENSOR analog gain : %d\n", max_sensor_again);
	len += seq_printf(m ,"SENSOR digital gain : %d\n", sensor_dgain);
	len += seq_printf(m ,"MAX SENSOR digital gain : %d\n", max_sensor_dgain);
	len += seq_printf(m ,"ISP digital gain : %d\n", isp_dgain);
	len += seq_printf(m ,"MAX ISP digital gain : %d\n", max_isp_dgain);
	len += seq_printf(m ,"ISP gain log2 id : %d\n", gain_log2_id);
	len += seq_printf(m ,"ISP total gain : %d\n", total_gain);
	len += seq_printf(m ,"ISP exposure log2 id: %d\n", exposure_log2_id);
	len += seq_printf(m ,"ISP EV Value: %d\n", manual_exposure_time);
	len += seq_printf(m ,"ISP AE strategy: %s\n", ae_strategy);
	len += seq_printf(m ,"ISP Antiflicker : %d[0 means disable]\n", flicker);
	len += seq_printf(m ,"Evtolux Probability Enable  : %d\n", evtolux);
	len += seq_printf(m ,"ISP WB Gain00 : %d\n", apical_isp_white_balance_gain_00_read());
	len += seq_printf(m ,"ISP WB Gain01 : %d\n", apical_isp_white_balance_gain_01_read());
	len += seq_printf(m ,"ISP WB Gain10 : %d\n", apical_isp_white_balance_gain_10_read());
	len += seq_printf(m ,"ISP WB Gain11 : %d\n", apical_isp_white_balance_gain_11_read());
	len += seq_printf(m ,"ISP WB rg : %d\n", apical_isp_metering_awb_rg_read());
	len += seq_printf(m ,"ISP WB bg : %d\n", apical_isp_metering_awb_bg_read());
	len += seq_printf(m ,"ISP WB Temperature : %d\n", temperature);
	len += seq_printf(m ,"ISP Black level 00 : %d\n", apical_isp_offset_black_00_read());
	len += seq_printf(m ,"ISP Black level 01 : %d\n", apical_isp_offset_black_01_read());
	len += seq_printf(m ,"ISP Black level 10 : %d\n", apical_isp_offset_black_10_read());
	len += seq_printf(m ,"ISP Black level 11 : %d\n", apical_isp_offset_black_11_read());
	len += seq_printf(m ,"ISP LSC mesh module : %s\n", apical_isp_mesh_shading_enable_read()?"enable":"disable");
	len += seq_printf(m ,"ISP LSC mesh blend mode : %d\n", apical_isp_mesh_shading_mesh_alpha_mode_read());
	len += seq_printf(m ,"ISP LSC mesh R table : %d\n", apical_isp_mesh_shading_mesh_alpha_bank_r_read());
	len += seq_printf(m ,"ISP LSC mesh G table : %d\n", apical_isp_mesh_shading_mesh_alpha_bank_g_read());
	len += seq_printf(m ,"ISP LSC mesh B table : %d\n", apical_isp_mesh_shading_mesh_alpha_bank_b_read());
	len += seq_printf(m ,"ISP LSC mesh R blend : %d\n", apical_isp_mesh_shading_mesh_alpha_r_read());
	len += seq_printf(m ,"ISP LSC mesh G blend : %d\n", apical_isp_mesh_shading_mesh_alpha_g_read());
	len += seq_printf(m ,"ISP LSC mesh B blend : %d\n", apical_isp_mesh_shading_mesh_alpha_b_read());
	len += seq_printf(m ,"ISP LSC mesh shading strength : %d\n", APICAL_READ_32(0x39c));
	len += seq_printf(m ,"ISP Sinter thresh1h : %d\n", apical_isp_sinter_thresh_1h_read());
	len += seq_printf(m ,"ISP Sinter thresh4h : %d\n", apical_isp_sinter_thresh_4h_read());
	len += seq_printf(m ,"ISP Sinter thresh1v : %d\n", apical_isp_sinter_thresh_1v_read());
	len += seq_printf(m ,"ISP Sinter thresh4v : %d\n", apical_isp_sinter_thresh_4v_read());
	len += seq_printf(m ,"ISP Sinter thresh short : %d\n", apical_isp_sinter_thresh_short_read());
	len += seq_printf(m ,"ISP Sinter thresh long : %d\n", apical_isp_sinter_thresh_long_read());
	len += seq_printf(m ,"ISP Sinter strength1 : %d\n", apical_isp_sinter_strength_1_read());
	len += seq_printf(m ,"ISP Sinter strength4 : %d\n", apical_isp_sinter_strength_4_read());
	len += seq_printf(m ,"ISP Temper thresh module :%s\n", apical_isp_temper_enable_read()?"enable":"disable");
	len += seq_printf(m ,"ISP Temper thresh short : %d\n", apical_isp_temper_thresh_short_read());
	len += seq_printf(m ,"ISP Temper thresh long : %d\n", apical_isp_temper_thresh_long_read());
	len += seq_printf(m ,"ISP Iridix strength : %d\n", apical_isp_iridix_strength_read());
	len += seq_printf(m ,"ISP Defect pixel threshold : %d\n", apical_isp_raw_frontend_dp_threshold_read());
	len += seq_printf(m ,"ISP Defect pixel slope : %d\n", apical_isp_raw_frontend_dp_slope_read());
	len += seq_printf(m ,"ISP sharpening directional : %d\n", apical_isp_demosaic_sharp_alt_d_read());
	len += seq_printf(m ,"ISP sharpening undirectional : %d\n", apical_isp_demosaic_sharp_alt_ud_read());
	len += seq_printf(m ,"ISP FR sharpen strength : %d\n", apical_isp_fr_sharpen_strength_read());
	len += seq_printf(m ,"ISP DS1 sharpen strength : %d\n", apical_isp_ds1_sharpen_strength_read());
#if TX_ISP_EXIST_DS2_CHANNEL
	len += seq_printf(m ,"ISP DS2 sharpen strength : %d\n", apical_isp_ds2_sharpen_strength_read());
#endif
	len += seq_printf(m ,"ISP CCM R_R : %d\n", apical_isp_matrix_rgb_coefft_r_r_read());
	len += seq_printf(m ,"ISP CCM R_G : %d\n", apical_isp_matrix_rgb_coefft_r_g_read());
	len += seq_printf(m ,"ISP CCM R_B : %d\n", apical_isp_matrix_rgb_coefft_r_b_read());
	len += seq_printf(m ,"ISP CCM G_R : %d\n", apical_isp_matrix_rgb_coefft_g_r_read());
	len += seq_printf(m ,"ISP CCM G_B : %d\n", apical_isp_matrix_rgb_coefft_g_b_read());
	len += seq_printf(m ,"ISP CCM G_G : %d\n", apical_isp_matrix_rgb_coefft_g_g_read());
	len += seq_printf(m ,"ISP CCM B_R : %d\n", apical_isp_matrix_rgb_coefft_b_r_read());
	len += seq_printf(m ,"ISP CCM B_G : %d\n", apical_isp_matrix_rgb_coefft_b_g_read());
	len += seq_printf(m ,"ISP CCM B_B : %d\n", apical_isp_matrix_rgb_coefft_b_b_read());
	len += seq_printf(m ,"Saturation Target : %d\n", saturation_target);
	len += seq_printf(m ,"Saturation : %d\n", saturation);
	len += seq_printf(m ,"Sharpness : %d\n", sharpness);
	len += seq_printf(m ,"Contrast : %d\n", contrast);
	len += seq_printf(m ,"Brightness : %d\n", brightness);

	return len;
}

static int dump_isp_info_open(struct inode *inode, struct file *file)
{
	return private_single_open_size(file, isp_info_show, PDE_DATA(inode),8192);
}

static struct file_operations isp_info_proc_fops ={
	.read = private_seq_read,
	.open = dump_isp_info_open,
	.llseek = private_seq_lseek,
	.release = private_single_release,
};

static int tx_isp_core_probe(struct platform_device *pdev)
{
	struct tx_isp_core_device *core_dev = NULL;
	struct tx_isp_subdev *sd = NULL;
	struct tx_isp_device_descriptor *desc = NULL;
	int ret = ISP_SUCCESS;

	core_dev = (struct tx_isp_core_device *)kzalloc(sizeof(*core_dev), GFP_KERNEL);
	if(!core_dev){
		ISP_ERROR("Failed to allocate sensor device\n");
		ret = -ENOMEM;
		goto exit;
	}

	/*printk("%s %d\n", __func__, __LINE__);*/
	desc = pdev->dev.platform_data;
	sd = &core_dev->sd;
	ret = tx_isp_subdev_init(pdev, sd, &core_subdev_ops);
	if(ret){
		ISP_ERROR("Failed to init isp module(%d.%d)\n", desc->parentid, desc->unitid);
		ret = -ENOMEM;
		goto failed_to_ispmodule;
	}

	/*printk("%s %d\n", __func__, __LINE__);*/
	private_spin_lock_init(&core_dev->slock);
	private_mutex_init(&core_dev->mlock);
	core_dev->pdata = pdev->dev.platform_data;

	ret = isp_core_output_channel_init(core_dev);
	if(ret){
		ISP_ERROR("Failed to init output channels!\n");
		ret = -EINVAL;
		goto failed_to_output_channel;
	}

	/*printk("%s %d\n", __func__, __LINE__);*/
	core_dev->tuning = isp_core_tuning_init(sd);
	if(core_dev->tuning == NULL){
		ISP_ERROR("Failed to init tuning module!\n");
		ret = -EINVAL;
		goto failed_to_tuning;
	}

	/*printk("%s %d sd->ops = %p %p\n", __func__, __LINE__,sd->ops, &core_subdev_ops);*/
	core_dev->state = TX_ISP_MODULE_SLAKE;
	private_platform_set_drvdata(pdev, &sd->module);
	tx_isp_set_subdevdata(sd, core_dev);
	tx_isp_set_module_nodeops(&sd->module, core_dev->tuning->fops);
	tx_isp_set_subdev_debugops(sd, &isp_info_proc_fops);

	/* apical init */
	system_isp_set_base_address(core_dev->sd.base);
	apical_sensor_early_init(core_dev);
	isp_set_interrupt_ops(sd);

	return ISP_SUCCESS;
failed_to_tuning:
	isp_core_output_channel_deinit(core_dev);
failed_to_output_channel:
	tx_isp_subdev_deinit(sd);
failed_to_ispmodule:
	kfree(core_dev);
exit:
	return ret;
}

static int __exit tx_isp_core_remove(struct platform_device *pdev)
{
	struct tx_isp_module *module = private_platform_get_drvdata(pdev);
	struct tx_isp_subdev *sd = module_to_subdev(module);
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);

	if(core->tuning){
		isp_core_tuning_deinit(core->tuning);
		core->tuning = NULL;
	}
	isp_core_output_channel_deinit(core);
	free_tx_isp_priv_param_manage();
	tx_isp_subdev_deinit(sd);
	kfree(core);
	return 0;
}

struct platform_driver tx_isp_core_driver = {
	.probe = tx_isp_core_probe,
	.remove = __exit_p(tx_isp_core_remove),
	.driver = {
		.name = TX_ISP_CORE_NAME,
		.owner = THIS_MODULE,
	},
};
