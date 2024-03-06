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
#include <tx-mscaler-regs.h>
#include "tx-isp-mscaler.h"
#include "tx-isp-mscaler-coe.h"
#include "tx-isp-frame-channel.h"

unsigned int ispw = 0;
module_param(ispw, int, S_IRUGO);
MODULE_PARM_DESC(ispw, "The width of isp's output");

unsigned int isph = 0;
module_param(isph, int, S_IRUGO);
MODULE_PARM_DESC(isph, "The height of isp's output");

unsigned int isptop = 0;
module_param(isptop, int, S_IRUGO);
MODULE_PARM_DESC(isptop, "The top of isp's output");

unsigned int ispleft = 0;
module_param(ispleft, int, S_IRUGO);
MODULE_PARM_DESC(ispleft, "The left of isp's output");

/* 0 is disable crop, !0 is enable */
unsigned int ispcrop = 0;
module_param(ispcrop, int, S_IRUGO);
MODULE_PARM_DESC(ispcrop, "The function of isp's crop");

/* high 16bits is width, low 16bits is height */
unsigned int ispcropwh = 0;
module_param(ispcropwh, int, S_IRUGO);
MODULE_PARM_DESC(ispcropwh, "The size of isp's crop");

/* high 16bits is top, low 16bits is left */
unsigned int ispcroptl = 0;
module_param(ispcroptl, int, S_IRUGO);
MODULE_PARM_DESC(ispcroptl, "The begin point of isp's crop");

/* 0 is disable scaler, !0 is enable */
unsigned int ispscaler = 0;
module_param(ispscaler, int, S_IRUGO);
MODULE_PARM_DESC(ispscaler, "The function of isp's crop");

/* high 16bits is width, low 16bits is height */
unsigned int ispscalerwh = 0;
module_param(ispscalerwh, int, S_IRUGO);
MODULE_PARM_DESC(ispscalerwh, "The size of isp's scaler");

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

static void channel_dma_buffer_done(struct isp_mscaler_output_channel *chan)
{
	struct tx_isp_mscaler_device *mscaler = chan->priv;
	struct frame_image_format *fmt = &(chan->fmt);
	struct frame_channel_buffer buf;

	while(!(tx_isp_sd_readl(&(mscaler->sd), CHx_Y_LAST_ADDR_FIFO_STA(chan->index)) & CH_ADDR_FIFO_EMPTY)){
		switch(fmt->pix.pixelformat){
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV21:
				tx_isp_sd_readl(&(mscaler->sd), CHx_DMAOUT_UV_LAST_ADDR(chan->index));
				tx_isp_sd_readl(&(mscaler->sd), CHx_DMAOUT_UV_LAST_STATS_NUM(chan->index));
			default:
				buf.addr = tx_isp_sd_readl(&(mscaler->sd), CHx_DMAOUT_Y_LAST_ADDR(chan->index));
				buf.priv = tx_isp_sd_readl(&(mscaler->sd), CHx_DMAOUT_Y_LAST_STATS_NUM(chan->index));
				break;
		}
		tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER, &buf);
		chan->frame_cnt++;
	}
}

static void configure_channel_dma_addr(struct isp_mscaler_output_channel *chan)
{
	struct tx_isp_mscaler_device *mscaler = chan->priv;
	struct frame_image_format *fmt = &(chan->fmt);
	struct frame_channel_buffer *buf;
	unsigned int offset = 0;
	while((tx_isp_sd_readl(&(mscaler->sd), CHx_Y_ADDR_FIFO_STA(chan->index)) & CH_ADDR_FIFO_FULL) == 0){
		buf = pop_buffer_fifo(&chan->fifo);
		if(buf == NULL)
			break;
		/*printk("## %s %d, chanid = %d buf->addr = 0x%08x ##\n", __func__,__LINE__,chan->index, buf->addr);*/
		switch(fmt->pix.pixelformat){
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV21:
				offset = fmt->pix.width * ((fmt->pix.height + 0xf) & ~0xf);
				tx_isp_sd_writel(&(mscaler->sd), CHx_DMAOUT_UV_ADDR(chan->index), buf->addr + offset);
				//	printk("(offset = 0x%08x)\n",offset);
			default:
				tx_isp_sd_writel(&(mscaler->sd), CHx_DMAOUT_Y_ADDR(chan->index), buf->addr);
				break;
		}
	}
}

/*
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
   interrupt handler
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 */
static unsigned int chan_done_state = 0;
static void msclaer_notify_front_module(struct tx_isp_mscaler_device *mscaler, unsigned int chan_id)
{
	struct isp_mscaler_input_channel *input = mscaler->inputs;
//	unsigned int chans_enable = tx_isp_sd_readl(&(mscaler->sd), MSCA_CH_EN) & 0x7;
	unsigned int chans_status = tx_isp_sd_readl(&(mscaler->sd), MSCA_CH_STAT) & 0x7;
	chan_done_state |= 1<<chan_id;
	if((chans_status & chan_done_state) == chans_status){
		tx_isp_send_event_to_remote(input->pad, TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER, NULL);
		chan_done_state = 0;
	}
}
//static unsigned int mscaler_done_cnt = 0;
static irqreturn_t mscaler_core_interrupt_service_routine(struct tx_isp_subdev *sd, u32 status, bool *handled)
{
	struct tx_isp_mscaler_device *mscaler = tx_isp_get_subdevdata(sd);
	volatile unsigned int pending, state, mask;
	unsigned int index = 0;

	state = tx_isp_sd_readl(&(mscaler->sd), MSCA_IRQ_STAT);
	mask = tx_isp_sd_readl(&(mscaler->sd), MSCA_IRQ_MASK);
	pending = state & (~mask);
	tx_isp_sd_writel(&(mscaler->sd), MSCA_CLR_IRQ, state);
	/*printk("@@ state =0x%08x mask = 0x%08x pending= 0x%08x @@\n", state, mask, pending);*/
	for(index = 0; index < MS_IRQ_MAX_BIT; index++){
		if(pending & (0x1<<index)){
			switch(index){
				case MS_IRQ_CH2_DONE_BIT:
					channel_dma_buffer_done(&(mscaler->outputs[ISP_MSCALER_OUTPUT_2]));
					configure_channel_dma_addr(&(mscaler->outputs[ISP_MSCALER_OUTPUT_2]));
					msclaer_notify_front_module(mscaler, MS_IRQ_CH2_DONE_BIT);
					break;
				case MS_IRQ_CH1_DONE_BIT:
					channel_dma_buffer_done(&(mscaler->outputs[ISP_MSCALER_OUTPUT_1]));
					configure_channel_dma_addr(&(mscaler->outputs[ISP_MSCALER_OUTPUT_1]));
					msclaer_notify_front_module(mscaler, MS_IRQ_CH1_DONE_BIT);
					break;
				case MS_IRQ_CH0_DONE_BIT:
					channel_dma_buffer_done(&(mscaler->outputs[ISP_MSCALER_OUTPUT_0]));
					configure_channel_dma_addr(&(mscaler->outputs[ISP_MSCALER_OUTPUT_0]));
					msclaer_notify_front_module(mscaler, MS_IRQ_CH0_DONE_BIT);
				//	tx_isp_send_event_to_remote(input->pad, TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER, NULL);
					break;
				case MS_IRQ_OVF_BIT:
				case MS_IRQ_CSC_BIT:
					break;
				case MS_IRQ_FRM_BIT:
				case MS_IRQ_CH2_CROP_BIT:
				case MS_IRQ_CH1_CROP_BIT:
				case MS_IRQ_CH0_CROP_BIT:
				default:
					break;
			}
		}
	}

	return IRQ_HANDLED;
}

static int mscaler_frame_channel_qbuf(struct tx_isp_subdev_pad *pad, void *data)
{
	struct frame_channel_buffer *buf = NULL;
	struct isp_mscaler_output_channel *chan = NULL;
	unsigned long flags;

	buf = data;
	chan = pad->priv;

	if(pad->type != TX_ISP_PADTYPE_OUTPUT){
		ISP_ERROR("The type of pad isn't OUTPUT!\n");
		return -EINVAL;
	}

	if(buf && chan){
		/*printk("## %s %d, chanid = %d addr = 0x%08x ##\n", __func__,__LINE__,chan->index, buf->addr);*/
		spin_lock_irqsave(&chan->slock, flags);
		push_buffer_fifo(&chan->fifo, buf);
		configure_channel_dma_addr(chan);
		spin_unlock_irqrestore(&chan->slock, flags);
	}
	return 0;
}

static int mscaler_frame_channel_freebufs(struct tx_isp_subdev_pad *pad, void *data)
{
	struct isp_mscaler_output_channel *chan = NULL;
	unsigned long flags = 0;

	if(pad->type != TX_ISP_PADTYPE_OUTPUT){
		ISP_ERROR("The type of pad isn't OUTPUT!\n");
		return -EINVAL;
	}
	chan = pad->priv;
	if(chan){
		private_spin_lock_irqsave(&chan->slock, flags);
		cleanup_buffer_fifo(&chan->fifo);
		private_spin_unlock_irqrestore(&chan->slock, flags);
	}
	return 0;
}

static int inline mscaler_video_streamon(struct tx_isp_mscaler_device *mscaler)
{
	struct isp_mscaler_input_channel *input = mscaler->inputs;
	struct tx_isp_subdev *sd = &mscaler->sd;
	struct tx_isp_irq_device *irqdev = &(sd->irqdev);
	struct frame_image_format *fmt = &input->fmt;
//	unsigned int width = 0, height = 0;
//	unsigned int top = 0, left = 0;
	int ret = 0;
	int i = 0;

	private_mutex_lock(&mscaler->mlock);
	if(input->pad->state < TX_ISP_PADSTATE_LINKED){
		ISP_ERROR("Please setup link firstly!\n");
		private_mutex_unlock(&mscaler->mlock);
		return -EPERM;
	}

	if(input->state < TX_ISP_MODULE_INIT){
		ISP_ERROR("Please init the module firstly!\n");
		private_mutex_unlock(&mscaler->mlock);
		return -EPERM;
	}
	/* input pad has been streamon */
	if(input->state == TX_ISP_MODULE_RUNNING){
		private_mutex_unlock(&mscaler->mlock);
		return 0;
	}

	ret = tx_isp_send_event_to_remote(input->pad, TX_ISP_EVENT_FRAME_CHAN_SET_FMT, fmt);
	if(ret && ret != -ENOIOCTLCMD){
		ISP_ERROR("Failed to set ms input format!\n");
		ret = -EINVAL;
		goto failed_s_fmt;
	}
	/*printk("## %s %d clk = 0x%08x ##\n", __func__,__LINE__,*(volatile unsigned int*)(0xb0000028));*/
	/*printk("## %s %d base = 0x%p ##\n", __func__,__LINE__, sd->base);*/
	tx_isp_sd_writel(sd, MSCA_SRC_IN, 0x4); //set LFB  and NV12
	tx_isp_sd_writel(sd, MSCA_SRC_SIZE, fmt->pix.width << 16 | fmt->pix.height); //set input size

	/* set csc regs */
	tx_isp_sd_writel(sd, CSC_C0_COEF, 0x4ad);
	tx_isp_sd_writel(sd, CSC_C1_COEF, 0x669);
	tx_isp_sd_writel(sd, CSC_C2_COEF, 0x193);
	tx_isp_sd_writel(sd, CSC_C3_COEF, 0x344);
	tx_isp_sd_writel(sd, CSC_C4_COEF, 0x81a);
	tx_isp_sd_writel(sd, CSC_OFFSET_PARA, (128 << 16)+ 16);
	tx_isp_sd_writel(sd, CSC_GLO_ALPHA, 128);

	/* set resize coefficients */
	for (i = 0; i < 512; i++)
		tx_isp_sd_writel(sd, GLO_RSZ_COEF_WR, mscaler_coefficient[i]);


	/* enable interrupt */
	tx_isp_sd_writel(sd, MSCA_CLR_IRQ, 0x1ff);
	tx_isp_sd_writel(sd, MSCA_IRQ_MASK, 0x0);
	if(irqdev->irq)
		irqdev->enable_irq(irqdev);
	tx_isp_reg_set(sd, DMA_OUT_ARB, 7, 7, 1); // dma depend on axi
	/* mscaler chn 0 1 2, support max resolution 1920*1080 */
	tx_isp_reg_set(sd, DMA_OUT_ARB, 8, 10, 7);

	ret = tx_isp_send_event_to_remote(input->pad, TX_ISP_EVENT_FRAME_CHAN_STREAM_ON, NULL);
	if(ret && ret != -ENOIOCTLCMD){
		ISP_ERROR("Failed to enable ms input!\n");
		ret = -EINVAL;
		goto failed_streamon;
	}

	input->pad->state = TX_ISP_PADSTATE_STREAM;
	input->state = TX_ISP_MODULE_RUNNING;
	mscaler->state = TX_ISP_MODULE_RUNNING;
//	input->fmt = fmt;

	private_mutex_unlock(&mscaler->mlock);
	return 0;

failed_streamon:
	tx_isp_sd_writel(sd, MSCA_IRQ_MASK, 0x1ff);
	if(irqdev->irq)
		irqdev->disable_irq(irqdev);
failed_s_fmt:
	private_mutex_unlock(&mscaler->mlock);
	return ret;
}

static int inline mscaler_video_streamoff(struct tx_isp_mscaler_device *mscaler)
{
	struct isp_mscaler_input_channel *input = mscaler->inputs;
	/*struct isp_mscaler_output_channel *outputs = mscaler->outputs;*/
	struct tx_isp_subdev *sd = &mscaler->sd;
	struct tx_isp_irq_device *irqdev = &(sd->irqdev);

	private_mutex_lock(&mscaler->mlock);
	if(input->state != TX_ISP_MODULE_RUNNING){
		private_mutex_unlock(&mscaler->mlock);
		return 0;
	}

	if(input->pad->state < TX_ISP_PADSTATE_LINKED){
		ISP_INFO("Don't use the %s!\n", sd->module.name);
		private_mutex_unlock(&mscaler->mlock);
		return 0;
	}

	tx_isp_send_event_to_remote(input->pad, TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF, NULL);
	mscaler->state = TX_ISP_MODULE_INIT;
	input->state = TX_ISP_MODULE_INIT;
	input->pad->state = TX_ISP_PADSTATE_LINKED;
	private_mutex_unlock(&mscaler->mlock);

	/*printk("## %s %d irq = %d ##\n", __func__,__LINE__, irqdev->irq);*/
	/* disable interrupt */
	tx_isp_sd_writel(sd, MSCA_IRQ_MASK, 0x1ff);
	if(irqdev->irq)
		irqdev->disable_irq(irqdev);
	return 0;
}

static int mscaler_video_s_stream(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_mscaler_device *mscaler = tx_isp_get_subdevdata(sd);
	struct isp_mscaler_input_channel *input = mscaler->inputs;
	int ret = ISP_SUCCESS;

	private_mutex_lock(&mscaler->mlock);
	if(mscaler->state < TX_ISP_MODULE_INIT){
		ISP_ERROR("%s[%d] the device hasn't been inited!\n",__func__,__LINE__);
		private_mutex_unlock(&mscaler->mlock);
		return -EPERM;
	}

	if(input->pad->state < TX_ISP_PADSTATE_LINKED){
		ISP_INFO("Don't use the %s!\n", sd->module.name);
		private_mutex_unlock(&mscaler->mlock);
		return 0;
	}

	if(!(input->pad->link.flag & TX_ISP_PADLINK_LFB)){
		ISP_ERROR("The link must be LFB!\n");
		private_mutex_unlock(&mscaler->mlock);
		return -EINVAL;
	}
	private_mutex_unlock(&mscaler->mlock);

	if(enable){
		/* streamon */
		if(mscaler->state == TX_ISP_MODULE_INIT){
			ret = mscaler_video_streamon(mscaler);
		}
	}else{
		/* streamoff */
		if(mscaler->state == TX_ISP_MODULE_RUNNING){
			ret = mscaler_video_streamoff(mscaler);
		}
	}
	return ret;
}

static int mscaler_frame_channel_streamon(struct tx_isp_subdev_pad *pad)
{
	struct isp_mscaler_output_channel *chan = pad->priv;
	struct tx_isp_mscaler_device *mscaler = chan->priv;
	unsigned long flags;
	int ret = ISP_SUCCESS;

	if(pad->state != TX_ISP_PADSTATE_LINKED)
		return ret;

	spin_lock_irqsave(&chan->slock, flags);
	if(chan->state == TX_ISP_MODULE_RUNNING){
		spin_unlock_irqrestore(&chan->slock, flags);
		return ISP_SUCCESS;
	}
	/* configure dma */
	tx_isp_sd_writel(&(mscaler->sd), CHx_DMAOUT_Y_STRI(chan->index), chan->lineoffset);
	tx_isp_sd_writel(&(mscaler->sd), CHx_DMAOUT_UV_STRI(chan->index), chan->lineoffset);
	configure_channel_dma_addr(chan);

	/* enable channel */
	tx_isp_reg_set(&(mscaler->sd), MSCA_CH_EN, chan->index, chan->index, 1);
	chan->frame_cnt = 0;
	chan->state = TX_ISP_MODULE_RUNNING;
	pad->state = TX_ISP_PADSTATE_STREAM;
	spin_unlock_irqrestore(&chan->slock, flags);
	return ret;
}

static int mscaler_frame_channel_streamoff(struct tx_isp_subdev_pad *pad)
{
	struct isp_mscaler_output_channel *chan = pad->priv;
	struct tx_isp_mscaler_device *mscaler = chan->priv;
	unsigned int loop = 200;
	unsigned long flags;
	int ret = ISP_SUCCESS;

	if(pad->state != TX_ISP_PADSTATE_STREAM)
		return ret;

	spin_lock_irqsave(&chan->slock, flags);
	if(chan->state != TX_ISP_MODULE_RUNNING){
		spin_unlock_irqrestore(&chan->slock, flags);
		return ISP_SUCCESS;
	}
	/* disable channel */
	tx_isp_reg_set(&(mscaler->sd), MSCA_CH_EN, chan->index, chan->index, 0);
	chan->state = TX_ISP_MODULE_INIT;
	spin_unlock_irqrestore(&chan->slock, flags);
#if 0
	do{
		ret = wait_for_completion_interruptible_timeout(&chan->stop_comp, msecs_to_jiffies(500));
	}while(ret == -ERESTARTSYS);
#endif

	while((tx_isp_sd_readl(&(mscaler->sd), MSCA_CH_STAT) & (0x1<<chan->index)) && loop--)
		msleep(2);

	/* clear channel */
	spin_lock_irqsave(&chan->slock, flags);
	switch(chan->fmt.pix.pixelformat){
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_BGR32:
			mscaler->chan_rgb_flags = 0;
			break;
		default:
			break;
	}

	/* streamoff */
	pad->state = TX_ISP_PADSTATE_LINKED;
	cleanup_buffer_fifo(&chan->fifo);
	tx_isp_sd_writel(&(mscaler->sd), CHx_DMAOUT_Y_ADDR_CLR(chan->index), 1); // clear Y fifo
	tx_isp_sd_writel(&(mscaler->sd), CHx_DMAOUT_UV_ADDR_CLR(chan->index), 1); // clear UV fifo
	spin_unlock_irqrestore(&chan->slock, flags);

	/* reset output parameters */
	memset(&chan->fmt, 0, sizeof(chan->fmt));
	return ret;
}

static int mscaler_core_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = ISP_SUCCESS;

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

	ret = ret == -ENOIOCTLCMD ? 0 : ret;

	return ret;
}

/*
 * the ops of isp's clks
 */
static void mscaler_clks_ops(struct tx_isp_subdev *sd, int on)
{
	struct clk **clks = sd->clks;
	int i = 0;

	/*printk("## %s %d; clk_num = %d, on =%d ##\n", __func__,__LINE__,sd->clk_num, on);*/
	if(on){
		for(i = 0; i < sd->clk_num; i++){
			private_clk_enable(clks[i]);
		}
	}else{
		for(i = sd->clk_num - 1; i >=0; i--)
			private_clk_disable(clks[i]);
	}
	return;
}

static int mscaler_core_ops_init(struct tx_isp_subdev *sd, int on)
{
	struct tx_isp_mscaler_device *mscaler = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;
	int ret = ISP_SUCCESS;
	int index = 0;

	/*printk("## %s %d on=%d ##\n", __func__,__LINE__, on);*/
	if(IS_ERR_OR_NULL(mscaler)){
		return -EINVAL;
	}
	if(mscaler->state == TX_ISP_MODULE_SLAKE)
		return 0;

	if(on){
		if(private_reset_tx_isp_module(TX_ISP_MSCALER_SUBDEV_ID)){
			ISP_ERROR("Failed to reset %s\n", sd->module.name);
			ret = -EINVAL;
			goto exit;
		}
		private_spin_lock_irqsave(&mscaler->slock, flags);
		if(mscaler->state != TX_ISP_MODULE_ACTIVATE){
			private_spin_unlock_irqrestore(&mscaler->slock, flags);
			ISP_ERROR("Can't init ms when its state is %d\n!", mscaler->state);
			ret = -EPERM;
			goto exit;
		}
		mscaler->state = TX_ISP_MODULE_INIT;
		for(index = 0; index < mscaler->num_outputs; index++){
			mscaler->outputs[index].state = TX_ISP_MODULE_INIT;
		}
		for(index = 0; index < mscaler->num_inputs; index++){
			mscaler->inputs[index].state = TX_ISP_MODULE_INIT;
		}
		private_spin_unlock_irqrestore(&mscaler->slock, flags);
	}else{
		if(mscaler->state == TX_ISP_MODULE_RUNNING){
			for(index = 0; index < mscaler->num_outputs; index++){
				mscaler_frame_channel_streamoff(mscaler->outputs[index].pad);
				mscaler->outputs[index].state = TX_ISP_MODULE_DEINIT;
			}
			mscaler_video_s_stream(sd, 0);
		}
		private_spin_lock_irqsave(&mscaler->slock, flags);
		mscaler->state = TX_ISP_MODULE_DEINIT;
		for(index = 0; index < mscaler->num_inputs; index++){
			mscaler->inputs[index].state = TX_ISP_MODULE_DEINIT;
		}
		private_spin_unlock_irqrestore(&mscaler->slock, flags);
	}
	return 0;
exit:
	return ret;
}

static struct tx_isp_subdev_core_ops mscaler_subdev_core_ops ={
	.init = mscaler_core_ops_init,
	.ioctl = mscaler_core_ops_ioctl,
	.interrupt_service_routine = mscaler_core_interrupt_service_routine,
};

static int mscaler_sync_sensor_attr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_mscaler_device *mscaler = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct isp_mscaler_input_channel *input = mscaler->inputs;
	struct frame_image_format fmt;
	unsigned int width = 0, height = 0;
	unsigned int top = 0, left = 0;
	if(IS_ERR_OR_NULL(mscaler)){
		ISP_ERROR("The parameter is invalid!\n");
		return -EINVAL;
	}
	if(arg){
		memcpy(&mscaler->vin, (void *)arg, sizeof(struct tx_isp_video_in));
		/* set input format */
		memset(&fmt, 0, sizeof(fmt));
		fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
		if(ispcrop || ispscaler){
			if(ispcrop){
				fmt.crop_enable = 1;
				width = (ispcropwh >> 16) & 0xffff;
				height = ispcropwh & 0xffff;
				fmt.pix.width = width ? width : mscaler->vin.mbus.width;
				fmt.pix.height = height ? height : mscaler->vin.mbus.height;
				if(width > mscaler->vin.mbus.width || height > mscaler->vin.mbus.height){
					ISP_ERROR("ISP crop output resolution is wrong(%d*%d)!\n", width, height);
					private_mutex_unlock(&mscaler->mlock);
					return -EPERM;
				}
				top = (ispcroptl >> 16) & 0xffff;
				left = ispcroptl & 0xffff;
				if(left){
					fmt.crop_left = left;
				} else {
					fmt.crop_left = (mscaler->vin.mbus.width - width) >> 1;
				}
				if(top){
					fmt.crop_top = top;
				} else {
					fmt.crop_top = (mscaler->vin.mbus.height - height) >> 1;
				}
				fmt.crop_width = width;
				fmt.crop_height = height;
			}else{
				fmt.crop_enable = 0;
			}
			if(ispscaler){
				fmt.scaler_enable = 1;
				width = (ispscalerwh >> 16) & 0xffff;
				height = ispscalerwh & 0xffff;
				fmt.pix.width = width ? width : mscaler->vin.mbus.width;
				fmt.pix.height = height ? height : mscaler->vin.mbus.height;
				if(width > mscaler->vin.mbus.width || height > mscaler->vin.mbus.height){
					ISP_ERROR("ISP scaler output resolution is wrong(%d*%d)!\n", width, height);
					private_mutex_unlock(&mscaler->mlock);
					return -EPERM;
				}
				fmt.scaler_out_width = width;
				fmt.scaler_out_height = height;
			}else{
				fmt.scaler_enable = 0;
			}
			fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
		}else{
			/* default setting */
			fmt.pix.width = mscaler->vin.mbus.width;
			fmt.pix.height = mscaler->vin.mbus.height;
			fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
			fmt.scaler_enable = 0;
			fmt.crop_enable = 0;
		}

		ispw = fmt.pix.width;
		isph = fmt.pix.height;

		if(fmt.pix.width > MSCALER_INPUT_MAX_WIDTH || fmt.pix.height > MSCALER_INPUT_MAX_HEIGHT){
			ISP_ERROR("ISP output resolution is wrong(%d*%d)!!!\n", fmt.pix.width, fmt.pix.height);
			private_mutex_unlock(&mscaler->mlock);
			return -EPERM;
		}

		memcpy(&input->fmt, &fmt, sizeof(fmt));
	}else
		memset(&mscaler->vin, 0, sizeof(struct tx_isp_video_in));
	return 0;
}

static struct tx_isp_subdev_sensor_ops mscaler_sensor_ops = {
	.sync_sensor_attr = mscaler_sync_sensor_attr,
};

static int mscaler_activate_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_mscaler_device *mscaler = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;
	int index = 0;

	if(IS_ERR_OR_NULL(mscaler))
		return -EINVAL;
	if(mscaler->state != TX_ISP_MODULE_SLAKE)
		return 0;

	/* clk ops */
	mscaler_clks_ops(sd, 1);
	tx_isp_sd_writel(&(mscaler->sd), MSCA_CTRL, 0);
	private_spin_lock_irqsave(&mscaler->slock, flags);
	mscaler->state = TX_ISP_MODULE_ACTIVATE;
	for(index = 0; index < mscaler->num_outputs; index++){
		mscaler->outputs[index].state = TX_ISP_MODULE_ACTIVATE;
	}
	for(index = 0; index < mscaler->num_inputs; index++){
		mscaler->inputs[index].state = TX_ISP_MODULE_ACTIVATE;
	}
	private_spin_unlock_irqrestore(&mscaler->slock, flags);
	return 0;
}

static int mscaler_slake_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_mscaler_device *mscaler = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;
	int index = 0;

	if(IS_ERR_OR_NULL(mscaler))
		return -EINVAL;
	if(mscaler->state == TX_ISP_MODULE_SLAKE)
		return 0;

	if(mscaler->state > TX_ISP_MODULE_ACTIVATE)
		mscaler_core_ops_init(sd, 0);

	tx_isp_sd_writel(&(mscaler->sd), MSCA_CTRL, 2);
	private_spin_lock_irqsave(&mscaler->slock, flags);
	mscaler->state = TX_ISP_MODULE_SLAKE;
	for(index = 0; index < mscaler->num_outputs; index++){
		mscaler->outputs[index].state = TX_ISP_MODULE_SLAKE;
	}
	for(index = 0; index < mscaler->num_inputs; index++){
		mscaler->inputs[index].state = TX_ISP_MODULE_SLAKE;
	}

	/* clk ops */
	mscaler_clks_ops(sd, 0);
	private_spin_unlock_irqrestore(&mscaler->slock, flags);
	return 0;
}

int mscaler_link_setup(const struct tx_isp_subdev_pad *local,
			  const struct tx_isp_subdev_pad *remote, u32 flags)
{
	return 0;
}


static struct tx_isp_subdev_video_ops mscaler_subdev_video_ops = {
	.link_stream = mscaler_video_s_stream,
	.link_setup = mscaler_link_setup,
};

struct tx_isp_subdev_internal_ops mscaler_internal_ops = {
	.activate_module = &mscaler_activate_module,
	.slake_module = &mscaler_slake_module,
};

static struct tx_isp_subdev_ops mscaler_subdev_ops = {
	.internal = &mscaler_internal_ops,
	.core = &mscaler_subdev_core_ops,
	.video = &mscaler_subdev_video_ops,
	.sensor = &mscaler_sensor_ops,
};

/*
 * ----------------- init device ------------------------
 */
static inline int mscaler_frame_channel_get_fmt(struct tx_isp_subdev_pad *pad, void *data)
{
	struct isp_mscaler_output_channel *chan = pad->priv;

	if(pad->type != TX_ISP_PADTYPE_OUTPUT)
		return 0;

	if(data && chan)
		memcpy(data, &(chan->fmt), sizeof(struct frame_image_format));
	return 0;
}

static struct frame_channel_format mscaler_output_fmt[] = {
	{
		.name     = "YUV 4:2:0 semi planar, Y/CbCr",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
		.priv     = MSCALER_OUTPUT_NV12,
	},
	{
		.name     = "YUV 4:2:0 semi planar, Y/CrCb",
		.fourcc   = V4L2_PIX_FMT_NV21,
		.depth    = 12,
		.priv     = MSCALER_OUTPUT_NV21,
	},
	{
		.name     = "RGB565, RGB-5-6-5",
		.fourcc   = V4L2_PIX_FMT_RGB565,
		.depth    = 16,
		.priv     = MSCALER_OUTPUT_RGB565,
	},
	{
		.name     = "BGR32, RGB-8-8-8-4",
		.fourcc   = V4L2_PIX_FMT_BGR32,
		.depth    = 32,
		.priv     = MSCALER_OUTPUT_BGR32,
	},
	/* the last member will be determined when isp input confirm.*/
	{
		.name     = "undetermine",
		.fourcc   = 0,
		.depth    = 0,
		.priv     = 0,
	},
};

static void mscaler_output_channel_config(struct isp_mscaler_output_channel *chan, struct frame_image_format *fmt)
{
	struct tx_isp_mscaler_device *mscaler = chan->priv;
	struct frame_channel_format *cfmt = (struct frame_channel_format *)(fmt->pix.priv);
	struct isp_mscaler_input_channel *input = mscaler->inputs;
	unsigned int step_w, step_h;
	unsigned int width = 0, height = 0;

	width = input->fmt.pix.width;
	height = input->fmt.pix.height;

	/* set scaler */
	if(fmt->scaler_enable){
		step_w = width * 512 / fmt->scaler_out_width;
		step_h = height * 512 / fmt->scaler_out_height;
		width = fmt->scaler_out_width;
		height = fmt->scaler_out_height;
	}else{
		step_w = 512;
		step_h = 512;
	}
	tx_isp_sd_writel(&(mscaler->sd), CHx_RSZ_STEP(chan->index), step_w<<16|step_h);
	tx_isp_sd_writel(&(mscaler->sd), CHx_RSZ_OSIZE(chan->index), width<<16|height);

	/* set crop */
	if(fmt->crop_enable){
		tx_isp_sd_writel(&(mscaler->sd), CHx_CROP_OPOS(chan->index), fmt->crop_left << 16 | fmt->crop_top);
		tx_isp_sd_writel(&(mscaler->sd), CHx_CROP_OSIZE(chan->index), fmt->crop_width << 16 | fmt->crop_height);
	}else{
		tx_isp_sd_writel(&(mscaler->sd), CHx_CROP_OPOS(chan->index), 0);
		tx_isp_sd_writel(&(mscaler->sd), CHx_CROP_OSIZE(chan->index), width << 16 | height);
	}

	/* set rate */
	fmt->rate_bits = fmt->rate_bits > 31 ? 31 : fmt->rate_bits;
	fmt->rate_mask = fmt->rate_mask == 0 ? 1 : fmt->rate_mask;
	tx_isp_sd_writel(&(mscaler->sd), CHx_FRA_CTRL_LOOP(chan->index), fmt->rate_bits);
	tx_isp_sd_writel(&(mscaler->sd), CHx_FRA_CTRL_MASK(chan->index), fmt->rate_mask);

	/* set output format */
	tx_isp_sd_writel(&(mscaler->sd), CHx_OUT_FMT(chan->index), cfmt->priv);
}

static int mscaler_frame_channel_set_fmt(struct tx_isp_subdev_pad *pad, void *data)
{
	/*struct tx_isp_subdev *sd = IS_ERR_OR_NULL(pad) ? NULL : pad->sd;*/
	struct isp_mscaler_output_channel *chan = pad->priv;
	struct frame_image_format *fmt = NULL;
	unsigned int width = 0;
	unsigned int height = 0;
	struct frame_channel_format *cfmt = mscaler_output_fmt;
	struct tx_isp_mscaler_device *mscaler = chan->priv;
	struct isp_mscaler_input_channel *input = mscaler->inputs;

	if(pad->type == TX_ISP_PADTYPE_UNDEFINE)
		return -EPERM;

	if(pad->type != TX_ISP_PADTYPE_OUTPUT)
		return 0;

	fmt = data;

	/* check format */
	if(fmt->crop_enable == true && chan->has_crop == false){
		ISP_ERROR("Chan%d can't be cropped!\n", chan->index);
		return -EINVAL;
	}
	if(fmt->scaler_enable == true && chan->has_scaler == false){
		ISP_ERROR("Chan%d can't be scaler!\n", chan->index);
		return -EINVAL;
	}

	/*width = input->fmt.pix.width > chan->max_width ? chan->max_width : input->fmt.pix.width;*/
	/*height = input->fmt.pix.height > chan->max_height ? chan->max_height : input->fmt.pix.height;*/
	width = chan->max_width;
	height = chan->max_height;

	/*printk("#### %s %d; w = %d h =%d ####\n",__func__,__LINE__,width,height);*/
	if(fmt->scaler_enable){
		if(width < fmt->scaler_out_width || height < fmt->scaler_out_height){
			ISP_ERROR("The chan%d in-scaler %d*%d out-scaler %d*%d!\n", chan->index, width, height, fmt->scaler_out_width, fmt->scaler_out_height);
			return -EINVAL;
		}
		width = fmt->scaler_out_width;
		height = fmt->scaler_out_height;
	}else{
		width = input->fmt.pix.width > width ? width : input->fmt.pix.width;
		height = input->fmt.pix.height > height ? height : input->fmt.pix.height;
	}

	/*printk("#### %s %d; w = %d h =%d ####\n",__func__,__LINE__,width,height);*/
	if(fmt->crop_enable){
		if(fmt->crop_top + fmt->crop_height > height ||
				fmt->crop_left + fmt->crop_width > width){
		ISP_ERROR("The chan%d crop %d*%d, %d*%d!\n", chan->index, fmt->crop_left, fmt->crop_top, fmt->crop_width, fmt->crop_height);
		return -EINVAL;
		}
		width = fmt->crop_width;
		height = fmt->crop_height;
	}

	/*printk("#### %s %d; w = %d h =%d ####\n",__func__,__LINE__,width,height);*/
	if(width < chan->min_width || height < chan->min_height){
			ISP_ERROR("The chan%d Can't output %d*%d!\n", chan->index, width, height);
			return -EINVAL;
	}

	if(width != fmt->pix.width || height != fmt->pix.height){
			ISP_ERROR("The chan%d output %d*%d error!\n", chan->index, width, height);
			return -EINVAL;
	}

	/* try fmt */
	while(cfmt->fourcc){
		if(cfmt->fourcc == fmt->pix.pixelformat)
			break;
		cfmt++;
	}

	/* Can't support the format */
	if(cfmt->fourcc == 0){
		char *value = (char *)&(fmt->pix.pixelformat);
		ISP_ERROR("OUTPUT can't support the fmt(%c%c%c%c)\n", value[0], value[1], value[2], value[3]);
		return -EINVAL;
	}
	private_mutex_lock(&mscaler->mlock);
	if((fmt->pix.pixelformat == V4L2_PIX_FMT_RGB565)
				|| (fmt->pix.pixelformat == V4L2_PIX_FMT_BGR32)){
		if(mscaler->chan_rgb_flags){
			ISP_ERROR("ms output RGB format only one channel!\n");
			private_mutex_unlock(&mscaler->mlock);
			return -EPERM;
		}else{
			mscaler->chan_rgb_flags = 1;
		}
	}
	private_mutex_unlock(&mscaler->mlock);

	/* set fmt */
	fmt->pix.bytesperline = fmt->pix.width * cfmt->depth / 8;
	fmt->pix.sizeimage = fmt->pix.bytesperline * ((fmt->pix.height + 0xf) & (~0xf)); // the size must be algin 16 lines.
	fmt->pix.priv = (unsigned int)cfmt;
	chan->lineoffset = fmt->pix.width * (cfmt->depth / 8);

	mscaler_output_channel_config(chan, fmt);
	chan->fmt = *fmt;
	return 0;
}

static int mscaler_pad_event_handle(struct tx_isp_subdev_pad *pad, unsigned int event, void *data)
{
	int ret = 0;

	if(pad->type == TX_ISP_MODULE_UNDEFINE)
		return 0;

	switch(event){
		case TX_ISP_EVENT_FRAME_CHAN_GET_FMT:
			ret = mscaler_frame_channel_get_fmt(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_SET_FMT:
			ret = mscaler_frame_channel_set_fmt(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_STREAM_ON:
			ret = mscaler_frame_channel_streamon(pad);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF:
			ret = mscaler_frame_channel_streamoff(pad);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER:
			ret = mscaler_frame_channel_qbuf(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_FREE_BUFFER:
			ret = mscaler_frame_channel_freebufs(pad, data);
			break;
		default:
			break;
	}
	return ret;
}

/* init */
static int mscaler_output_channel_init(struct tx_isp_mscaler_device *mscaler)
{
	struct tx_isp_subdev *sd = &mscaler->sd;
	struct isp_mscaler_output_channel *chans = NULL;
	struct isp_mscaler_output_channel *chan = NULL;
	int ret = ISP_SUCCESS;
	int index = 0;

	mscaler->num_outputs = sd->num_outpads;
	chans = (struct isp_mscaler_output_channel *)kzalloc(sizeof(*chans) * sd->num_outpads, GFP_KERNEL);
	if(!chans){
		ISP_ERROR("Failed to allocate sensor device\n");
		ret = -ENOMEM;
		goto exit;
	}

	for(index = 0; index < mscaler->num_outputs; index++){
		chan = &chans[index];
		chan->index = index;
		chan->pad = &(sd->outpads[index]);
		if(sd->outpads[index].type == TX_ISP_PADTYPE_UNDEFINE){
			chan->state = TX_ISP_MODULE_UNDEFINE;
			continue;
		}
		switch(index){
			case ISP_MSCALER_OUTPUT_0:
				chan->max_width = MSCALER_OUTPUT0_MAX_WIDTH;
				chan->max_height = MSCALER_OUTPUT0_MAX_HEIGHT;
				chan->has_crop = true;
				chan->has_scaler = true;
				break;
			case ISP_MSCALER_OUTPUT_1:
				chan->max_width = MSCALER_OUTPUT1_MAX_WIDTH;
				chan->max_height = MSCALER_OUTPUT1_MAX_HEIGHT;
				chan->has_crop = true;
				chan->has_scaler = true;
				break;
			case ISP_MSCALER_OUTPUT_2:
				chan->max_width = MSCALER_OUTPUT1_MAX_WIDTH;
				chan->max_height = MSCALER_OUTPUT1_MAX_HEIGHT;
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
		private_init_completion(&chan->stop_comp);
		chan->priv = mscaler;
		sd->outpads[index].event = mscaler_pad_event_handle;
		sd->outpads[index].priv = chan;
	}

	mscaler->outputs = chans;
	return ISP_SUCCESS;
exit:
	return ret;
}

static int mscaler_output_channel_deinit(struct tx_isp_mscaler_device *mscaler)
{
	struct isp_mscaler_output_channel *chans = mscaler->outputs;

	kfree(chans);
	mscaler->outputs = NULL;

	return 0;
}

static int mscaler_input_channel_init(struct tx_isp_mscaler_device *mscaler)
{
	struct tx_isp_subdev *sd = &mscaler->sd;
	struct isp_mscaler_input_channel *chans = NULL;
	struct isp_mscaler_input_channel *chan = NULL;
	int ret = ISP_SUCCESS;
	int index = 0;

	mscaler->num_inputs = sd->num_inpads;
	chans = (struct isp_mscaler_input_channel *)kzalloc(sizeof(*chans) * sd->num_inpads, GFP_KERNEL);
	if(!chans){
		ISP_ERROR("Failed to allocate ms input\n");
		ret = -ENOMEM;
		goto exit;
	}

	for(index = 0; index < mscaler->num_inputs; index++){
		chan = &chans[index];
		chan->index = index;
		chan->pad = &(sd->inpads[index]);

		if(sd->inpads[index].type == TX_ISP_PADTYPE_UNDEFINE){
			chan->state = TX_ISP_MODULE_UNDEFINE;
			continue;
		}

		private_spin_lock_init(&(chan->slock));
		chan->priv = mscaler;
		sd->inpads[index].event = mscaler_pad_event_handle;
		sd->inpads[index].priv = chan;
	}

	mscaler->inputs = chans;
	return ISP_SUCCESS;
exit:
	return ret;
}

static int mscaler_input_channel_deinit(struct tx_isp_mscaler_device *mscaler)
{
	struct isp_mscaler_input_channel *chans = mscaler->inputs;

	kfree(chans);
	mscaler->inputs = NULL;

	return 0;
}


/* debug msclaer info */
static int isp_mscaler_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_mscaler_device *mscaler = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct isp_mscaler_output_channel *output = NULL;
	char *fmt = NULL;
	int index = 0;

	if(IS_ERR_OR_NULL(mscaler)){
		ISP_ERROR("The parameter is invalid!\n");
		return 0;
	}
	for(index = 0; index < mscaler->num_outputs; index++){
		len += seq_printf(m ,"############## chan %d ###############\n", index);
		output = &(mscaler->outputs[index]);
		len += seq_printf(m ,"chan status: %s\n", output->state == TX_ISP_MODULE_RUNNING?"running":"stop");
		if(output->state != TX_ISP_MODULE_RUNNING)
			continue;
		len += seq_printf(m ,"output frames: %d\n", output->frame_cnt);
		fmt = (char *)(&output->fmt.pix.pixelformat);
		len += seq_printf(m ,"output pixformat: %c%c%c%c\n", fmt[0],fmt[1],fmt[2],fmt[3]);
		len += seq_printf(m ,"output resolution: %d * %d\n", output->fmt.pix.width, output->fmt.pix.height);
		len += seq_printf(m ,"scaler : %s\n", output->fmt.scaler_enable?"enable":"disable");
		if(output->fmt.scaler_enable){
			len += seq_printf(m ,"scaler width: %d\n", output->fmt.scaler_out_width);
			len += seq_printf(m ,"scaler height: %d\n", output->fmt.scaler_out_height);
		}
		len += seq_printf(m ,"crop : %s\n", output->fmt.crop_enable?"enable":"disable");
		if(output->fmt.crop_enable){
			len += seq_printf(m ,"crop top: %d\n", output->fmt.crop_top);
			len += seq_printf(m ,"crop left: %d\n", output->fmt.crop_left);
			len += seq_printf(m ,"crop width: %d\n", output->fmt.crop_width);
			len += seq_printf(m ,"crop height: %d\n", output->fmt.crop_height);
		}
	}
	return len;
}
static int dump_isp_mscaler_open(struct inode *inode, struct file *file)
{
	return private_single_open_size(file, isp_mscaler_show, PDE_DATA(inode), 2048);
}
static struct file_operations isp_mscaler_fops ={
	.read = private_seq_read,
	.open = dump_isp_mscaler_open,
	.llseek = private_seq_lseek,
	.release = private_single_release,
};

static int tx_isp_mscaler_probe(struct platform_device *pdev)
{
	struct tx_isp_mscaler_device *mscaler = NULL;
	struct tx_isp_subdev *sd = NULL;
	struct tx_isp_device_descriptor *desc = NULL;
	int ret = ISP_SUCCESS;

	mscaler = (struct tx_isp_mscaler_device *)kzalloc(sizeof(*mscaler), GFP_KERNEL);
	if(!mscaler){
		ISP_ERROR("Failed to allocate ms device\n");
		ret = -ENOMEM;
		goto exit;
	}

	desc = pdev->dev.platform_data;
	sd = &mscaler->sd;
	ret = tx_isp_subdev_init(pdev, sd, &mscaler_subdev_ops);
	if(ret){
		ISP_ERROR("Failed to init isp module(%d.%d)\n", desc->parentid, desc->unitid);
		ret = -ENOMEM;
		goto failed_to_ispmodule;
	}
	ret = mscaler_output_channel_init(mscaler);
	if(ret){
		ISP_ERROR("Failed to init output channels!\n");
		ret = -ENOMEM;
		goto failed_to_output_channel;
	}

	ret = mscaler_input_channel_init(mscaler);
	if(ret){
		ISP_ERROR("Failed to init output channels!\n");
		ret = -ENOMEM;
		goto failed_to_input_channel;
	}

	private_spin_lock_init(&mscaler->slock);
	private_mutex_init(&mscaler->mlock);
	mscaler->pdata = pdev->dev.platform_data;

	/*private_init_completion(&ncu->stop_comp);*/
	mscaler->state = TX_ISP_MODULE_SLAKE;
	private_platform_set_drvdata(pdev, &sd->module);
	tx_isp_set_subdev_debugops(sd, &isp_mscaler_fops);
	tx_isp_set_subdevdata(sd, mscaler);

	if(ispw || isph){
		ispcrop = 1;
		ispcropwh = (ispw<<16) | isph;
		ispcroptl = (isptop<<16) | ispleft;
	}

	return ISP_SUCCESS;
failed_to_input_channel:
	mscaler_output_channel_deinit(mscaler);
failed_to_output_channel:
	tx_isp_subdev_deinit(sd);
failed_to_ispmodule:
	kfree(mscaler);
exit:
	return ret;
}

static int __exit tx_isp_mscaler_remove(struct platform_device *pdev)
{
	struct tx_isp_module *module = private_platform_get_drvdata(pdev);
	struct tx_isp_subdev *sd = module_to_subdev(module);
	struct tx_isp_mscaler_device *mscaler = tx_isp_get_subdevdata(sd);

	mscaler_input_channel_deinit(mscaler);
	mscaler_output_channel_deinit(mscaler);
	tx_isp_subdev_deinit(sd);
	kfree(mscaler);
	return 0;
}

struct platform_driver tx_isp_mscaler_driver = {
	.probe = tx_isp_mscaler_probe,
	.remove = __exit_p(tx_isp_mscaler_remove),
	.driver = {
		.name = TX_ISP_MSCALER_NAME,
		.owner = THIS_MODULE,
	},
};
