/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/delay.h>
#include <asm/mipsregs.h>
#include <linux/clk.h>
#include <tx-isp-list.h>
#include "tx-isp-ncu.h"
#include "tx-isp-frame-channel.h"
#include "tx-isp-videobuf.h"

static int isp_m2_bufs = 2;
module_param(isp_m2_bufs, int, S_IRUGO);
MODULE_PARM_DESC(isp_m2_bufs, "isp inter buffers");

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

static int inline is_empty_fifo(struct list_head *fifo)
{
	return tx_list_empty(fifo);
}

/*
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
   interrupt handler
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 */
static irqreturn_t ncu_core_interrupt_service_routine(struct tx_isp_subdev *sd, u32 status, bool *handled)
{
#if 0
	struct tx_isp_ncu_device *ncu = tx_isp_get_subdevdata(sd);
	struct tx_isp_subdev_pad *pad = sd->inpads;
	struct frame_channel_buffer *buf = NULL;

	if(tx_isp_sd_readl((&ncu->sd), NCU_START) & NCU_START_IDLE_MASK){
		/*if(ncu->state == TX_ISP_MODULE_INIT)*/
			/*private_complete(&ncu->stop_comp);*/
		if(ncu->state == TX_ISP_MODULE_RUNNING){
			if(pad->link.flag & TX_ISP_PADLINK_DDR){
				buf = pop_buffer_fifo(&ncu->infifo);
				if(buf){
					tx_isp_sd_writel(&ncu->sd, Y_CUR_ADDR, buf->addr);
					tx_isp_sd_writel(&ncu->sd, UV_CUR_ADDR, buf->addr + ncu->uv_offset);
					tx_isp_reg_set(&ncu->sd, NCU_START, 0, 0, 1); // start ncu
				}
			}
		}
	}
#endif
	tx_isp_reg_set(sd, NCU_INT_CNTRL, 1, 1, 1); // clear interrupt

	return IRQ_HANDLED;
}

//static unsigned int ncu_start_cnt = 0, ncu_done_cnt = 0;
static unsigned int last_ncu_start_cnt = 0;
static unsigned int lost_cnt = 0;
static struct tx_isp_ncu_device *g_ncu = NULL;
void tx_isp_sync_ncu(void)
{
	if(g_ncu && (g_ncu->state == TX_ISP_MODULE_RUNNING)){
		if((last_ncu_start_cnt != g_ncu->start_cnt) || (g_ncu->start_cnt == g_ncu->done_cnt)){
			last_ncu_start_cnt = g_ncu->start_cnt;
			lost_cnt = 0;
		}else{
			lost_cnt++;
		}

		if((lost_cnt) >= 3){
			g_ncu->start_cnt = g_ncu->done_cnt;
			*(volatile unsigned int *)0xb30b0000 = 0x2;
			tx_isp_reg_set(&g_ncu->sd, NCU_START, 0, 0, 1); // start ncu
			g_ncu->ms_flag = 1; // msclaer is busy
			g_ncu->start_cnt++;
			g_ncu->reset_cnt++;
			/*printk("####### reset ncu and mscaler ######\n");*/
		}
	}
}

static int ncu_frame_channel_dqbuf(struct tx_isp_subdev_pad *pad, void *data)
{
	struct frame_channel_buffer *buf = NULL;
	struct tx_isp_ncu_device *ncu = pad->priv;
	unsigned long flags = 0;

	buf = data;
	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		return 0;
	}

	spin_lock_irqsave(&ncu->slock, flags);
	if(buf && ncu){
		push_buffer_fifo(&ncu->infifo, buf);
	}

	if(ncu->state == TX_ISP_MODULE_RUNNING){
		if((tx_isp_sd_readl((&ncu->sd), NCU_START) & NCU_START_IDLE_MASK) &&
				(ncu->ms_flag == 0)){	// when mscaler is idle state.
			buf = pop_buffer_fifo(&ncu->infifo);
			if(buf){
				tx_isp_sd_writel(&ncu->sd, Y_CUR_ADDR, buf->addr);
				tx_isp_sd_writel(&ncu->sd, UV_CUR_ADDR, buf->addr + ncu->uv_offset);
				/*printk("%s[%d]: ncu start addr = 0x%08x\n",__func__,__LINE__, buf->addr);*/
				ncu->current_inbuf = buf;
				tx_isp_reg_set(&ncu->sd, NCU_START, 0, 0, 1); // start ncu
				ncu->ms_flag = 1; // msclaer is busy
				ncu->start_cnt++;
			}
		}
	}
	spin_unlock_irqrestore(&ncu->slock, flags);

	return 0;
}

static int ncu_frame_channel_qbuf(struct tx_isp_subdev_pad *pad, void *data)
{
	struct frame_channel_buffer *buf = NULL;
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(pad) ? NULL : pad->sd;
	struct tx_isp_ncu_device *ncu = IS_ERR_OR_NULL(pad) ? NULL : pad->priv;
	struct tx_isp_subdev_pad *inpad = IS_ERR_OR_NULL(sd) ? NULL : sd->inpads;
	unsigned long flags = 0;

	if(inpad->link.flag & TX_ISP_PADLINK_LFB){
		return 0;
	}

    /*printk("%s[%d]: ncu end\n",__func__,__LINE__);*/
	spin_lock_irqsave(&ncu->slock, flags);
	ncu->ms_flag = 0; //mscaler is idle
	if(ncu->current_inbuf){
		tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER, ncu->current_inbuf);
		ncu->current_inbuf = NULL;
		ncu->done_cnt++;
	}
	if(ncu->state == TX_ISP_MODULE_RUNNING){
		if(tx_isp_sd_readl((&ncu->sd), NCU_START) & NCU_START_IDLE_MASK){
			buf = pop_buffer_fifo(&ncu->infifo);
			if(buf){
				tx_isp_sd_writel(&ncu->sd, Y_CUR_ADDR, buf->addr);
				tx_isp_sd_writel(&ncu->sd, UV_CUR_ADDR, buf->addr + ncu->uv_offset);
				/*printk("%s[%d]: ncu start addr = 0x%08x\n",__func__,__LINE__, buf->addr);*/
				ncu->current_inbuf = buf;
				tx_isp_reg_set(&ncu->sd, NCU_START, 0, 0, 1); // start ncu
				ncu->ms_flag = 1; // msclaer is busy
				ncu->start_cnt++;
			}
		}
	}
	spin_unlock_irqrestore(&ncu->slock, flags);

	return 0;
}

static int ncu_core_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
static void ncu_clks_ops(struct tx_isp_subdev *sd, int on)
{
	struct clk **clks = sd->clks;
	int i = 0;

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

static int ncu_frame_channel_streamoff(struct tx_isp_subdev_pad *pad);
static int ncu_core_ops_init(struct tx_isp_subdev *sd, int on)
{
	struct tx_isp_ncu_device *ncu = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;
	int ret = 0;

	if(IS_ERR_OR_NULL(ncu)){
		return -EINVAL;
	}

	if(ncu->state == TX_ISP_MODULE_SLAKE)
		return 0;
	/*printk("## %s %d on=%d ##\n", __func__,__LINE__, on);*/


	if(on){
		if(private_reset_tx_isp_module(TX_ISP_NCU_SUBDEV_ID)){
			ISP_ERROR("Failed to reset %s\n", sd->module.name);
			ret = -EINVAL;
			goto exit;
		}
		private_spin_lock_irqsave(&ncu->slock, flags);
		if(ncu->state != TX_ISP_MODULE_ACTIVATE){
			private_spin_unlock_irqrestore(&ncu->slock, flags);
			ISP_ERROR("Can't init ispcore when its state is %d\n!", ncu->state);
			ret = -EPERM;
			goto exit;
		}
		ncu->state = TX_ISP_MODULE_INIT;
		private_spin_unlock_irqrestore(&ncu->slock, flags);
	}else{
		if(ncu->state == TX_ISP_MODULE_RUNNING){
			ncu_frame_channel_streamoff(sd->outpads);
		}
		private_spin_lock_irqsave(&ncu->slock, flags);
		ncu->state = TX_ISP_MODULE_DEINIT;
		private_spin_unlock_irqrestore(&ncu->slock, flags);
	}
	return 0;
exit:
	return ret;
}

static struct tx_isp_subdev_core_ops ncu_subdev_core_ops ={
	.init = ncu_core_ops_init,
	.ioctl = ncu_core_ops_ioctl,
	.interrupt_service_routine = ncu_core_interrupt_service_routine,
};

static int ncu_sync_sensor_attr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_ncu_device *ncu = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	if(IS_ERR_OR_NULL(ncu)){
		ISP_ERROR("The parameter is invalid!\n");
		return -EINVAL;
	}
	if(arg){
		memcpy(&ncu->vin, (void *)arg, sizeof(struct tx_isp_video_in));
	}else
		memset(&ncu->vin, 0, sizeof(struct tx_isp_video_in));
	return 0;
}

static struct tx_isp_subdev_sensor_ops ncu_sensor_ops = {
	.sync_sensor_attr = ncu_sync_sensor_attr,
};

static int ncu_activate_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_ncu_device *ncu = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;

	if(IS_ERR_OR_NULL(ncu))
		return -EINVAL;
	if(ncu->state != TX_ISP_MODULE_SLAKE)
		return 0;

	private_spin_lock_irqsave(&ncu->slock, flags);
	/* clk ops */
	ncu_clks_ops(sd, 1);
	init_buffer_fifo(&ncu->infifo);
	ncu->state = TX_ISP_MODULE_ACTIVATE;
	private_spin_unlock_irqrestore(&ncu->slock, flags);
	return 0;
}

static int ncu_slake_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_ncu_device *ncu = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;

	if(IS_ERR_OR_NULL(ncu))
		return -EINVAL;
	if(ncu->state == TX_ISP_MODULE_SLAKE)
		return 0;

	if(ncu->state > TX_ISP_MODULE_ACTIVATE)
		ncu_core_ops_init(sd, 0);

	private_spin_lock_irqsave(&ncu->slock, flags);
	/* clk ops */
	ncu_clks_ops(sd, 0);
	ncu->state = TX_ISP_MODULE_SLAKE;
	private_spin_unlock_irqrestore(&ncu->slock, flags);
	return 0;
}


int ncu_link_setup(const struct tx_isp_subdev_pad *local,
			  const struct tx_isp_subdev_pad *remote, u32 flags)
{
	return 0;
}


static struct tx_isp_subdev_video_ops ncu_subdev_video_ops = {
	/*.s_stream = ncu_video_s_stream,*/
	.link_setup = ncu_link_setup,
};

static struct tx_isp_subdev_internal_ops ncu_internal_ops = {
	.activate_module = &ncu_activate_module,
	.slake_module = &ncu_slake_module,
};

static struct tx_isp_subdev_ops ncu_subdev_ops = {
	.internal = &ncu_internal_ops,
	.core = &ncu_subdev_core_ops,
	.video = &ncu_subdev_video_ops,
	.sensor = &ncu_sensor_ops,
};

/*
 * ----------------- init device ------------------------
 */
static inline int ncu_frame_channel_get_fmt(struct tx_isp_subdev_pad *pad, void *data)
{
	struct tx_isp_ncu_device *ncu = pad->priv;

	if(data && ncu)
		memcpy(data, &(ncu->fmt), sizeof(struct frame_image_format));
	return 0;
}

static int ncu_frame_channel_set_fmt(struct tx_isp_subdev_pad *pad, void *data)
{
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(pad) ? NULL : pad->sd;
	struct tx_isp_ncu_device *ncu = pad->priv;
	struct frame_image_format *fmt = NULL;
	int ret = 0;

	if(pad->type == TX_ISP_PADTYPE_UNDEFINE)
		return -EPERM;

	fmt = data;
	if(pad->type == TX_ISP_PADTYPE_OUTPUT){
		if(fmt->pix.pixelformat != V4L2_PIX_FMT_NV12
				&& fmt->pix.pixelformat != V4L2_PIX_FMT_NV21){
			char *value = (char *)&(fmt->pix.pixelformat);
			ISP_ERROR("%s can't support the fmt(%c%c%c%c)\n", sd->module.name, value[0], value[1], value[2], value[3]);
			return -EINVAL;
		}
		ret = tx_isp_send_event_to_remote(sd->inpads, TX_ISP_EVENT_FRAME_CHAN_SET_FMT, data);
		if(ret && ret != -ENOIOCTLCMD){
			return ret;
		};
		if(fmt->pix.width > ncu->vin.vi_max_width || fmt->pix.height > ncu->vin.vi_max_height)
		{
			ISP_ERROR("The resolution is invalid, set(%d*%d), sensor(%d*%d)\n",
					fmt->pix.width, fmt->pix.height, ncu->vin.vi_max_width, ncu->vin.vi_max_height);
			return -EINVAL;
		}
		memcpy(&ncu->fmt, fmt, sizeof(*fmt));
		ncu->uv_offset = ncu->fmt.pix.width * ncu->fmt.pix.height;
	}
	return 0;
}

static int ncu_frame_channel_streamon(struct tx_isp_subdev_pad *pad)
{
	int ret = 0;
	struct tx_isp_ncu_device *ncu = pad->priv;
	struct tx_isp_subdev_pad *inpad = ncu->sd.inpads;
	struct tx_isp_irq_device *irqdev = &(ncu->sd.irqdev);
	/* unsigned int uv_offset = ncu->fmt.pix.width * ncu->fmt.pix.height; */
	unsigned int addr = 0;
	volatile unsigned int control = 0;//NCU_INT_CNTRL_DEFAULT_VALUE;
	unsigned long flags = 0;
	int index = 0;

	if(pad->type != TX_ISP_PADTYPE_OUTPUT){
		return 0;
	}

	private_mutex_lock(&ncu->mlock);
	if(inpad->state != TX_ISP_PADSTATE_LINKED
			|| pad->state != TX_ISP_PADSTATE_LINKED){
		ISP_ERROR("Please setup link firstly!\n");
		private_mutex_unlock(&ncu->mlock);
		return -EPERM;
	}

	if(pad->link.flag & TX_ISP_PADLINK_DDR){
		ISP_ERROR("The link must be LFB!\n");
		private_mutex_unlock(&ncu->mlock);
		return -EINVAL;
	}else{
#if 0
		ncu->ref_frame_addr = isp_malloc_buffer(ncu->fmt.pix.sizeimage);
		if(ncu->ref_frame_addr == 0){
			ISP_ERROR("ispmem isn't enough!\n");
			return -ENOMEM;
		}
#endif
		tx_isp_sd_writel(&ncu->sd, Y_CUR_STRIDE, ncu->fmt.pix.width);
		tx_isp_sd_writel(&ncu->sd, UV_CUR_STRIDE, ncu->fmt.pix.width);

		/*tx_isp_sd_writel(&ncu->sd, Y_REF_BUF0_ADDR, ncu->ref_frame_addr);*/
		tx_isp_sd_writel(&ncu->sd, Y_REF_BUF0_STRIDE, ncu->fmt.pix.width);
		/*tx_isp_sd_writel(&ncu->sd, UV_REF_BUF0_ADDR, ncu->ref_frame_addr + uv_offset);*/
		tx_isp_sd_writel(&ncu->sd, UV_REF_BUF0_STRIDE, ncu->fmt.pix.width);

		/*tx_isp_sd_writel(&ncu->sd, Y_REF_BUF1_ADDR, ncu->ref_frame_addr);*/
		tx_isp_sd_writel(&ncu->sd, Y_REF_BUF1_STRIDE, ncu->fmt.pix.width);
		/*tx_isp_sd_writel(&ncu->sd, UV_REF_BUF1_ADDR, ncu->ref_frame_addr + uv_offset);*/
		tx_isp_sd_writel(&ncu->sd, UV_REF_BUF1_STRIDE, ncu->fmt.pix.width);
	}

	if(inpad->link.flag & TX_ISP_PADLINK_DDR){
		if(ncu->num_inbufs == 0){
			ISP_ERROR("Please config isp_m2_bufs when insmod driver!\n");
			goto exit;
		}
		addr = isp_malloc_buffer(ncu->fmt.pix.sizeimage * ncu->num_inbufs);
		if(addr == 0){
			ISP_ERROR("ispmem isn't enough!\n");
			goto exit;
		}
		for(index = 0; index < ncu->num_inbufs; index++){
			INIT_LIST_HEAD(&(ncu->inbufs[index].entry));
			ncu->inbufs[index].addr = addr + index * ncu->fmt.pix.sizeimage;
			ret = tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER, &(ncu->inbufs[index]));
			if(ret && ret != -ENOIOCTLCMD){
				goto failed_qbuf;
			}
		}
		ncu->buf_addr = addr;
		/* set input ddr */
		control |= NCU_INT_CNTRL_INPUT_DDR;

		tx_isp_sd_writel(&ncu->sd, Y_CUR_STRIDE, ncu->fmt.pix.width);
		tx_isp_sd_writel(&ncu->sd, UV_CUR_STRIDE, ncu->fmt.pix.width);
	}

	/*printk("## %s %d control = 0x%08x ##\n", __func__,__LINE__,control);*/
	tx_isp_reg_set(&ncu->sd, NCU_RESET, 0, 0, 0);
	tx_isp_reg_set(&ncu->sd, NCU_FUN_CNTRL, 0, 7, control);
	/*tx_isp_reg_set(&ncu->sd, NCU_FUN_CNTRL, 0, 31, 0x55cf00);*/
	tx_isp_sd_writel(&ncu->sd, NCU_FRAME_SIZE, ncu->fmt.pix.height<<16 | ncu->fmt.pix.width);
	tx_isp_reg_set(&ncu->sd, NCU_STOP, 0, 0, 0); // enable ncu
	tx_isp_reg_set(&ncu->sd, NCU_START, 1, 1, 1); // set first frame bit
	tx_isp_reg_set(&ncu->sd, NCU_INT_CNTRL, 0, 0, 1); // enable interrupt
	/*tx_isp_reg_set(&ncu->sd, NCU_SREG_CNTRL, 2, 2, 0); // select phase*/
	tx_isp_reg_set(&ncu->sd, NCU_SREG_CNTRL, 0, 0, 1); // enable function register
	if(irqdev->irq)
		irqdev->enable_irq(irqdev);

#if 1
	ret = tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_STREAM_ON, NULL);
	if(ret && ret != -ENOIOCTLCMD){
		goto failed_streamon;
	}
#endif
	private_spin_lock_irqsave(&ncu->slock, flags);
	inpad->state = TX_ISP_PADSTATE_STREAM;
	pad->state = TX_ISP_PADSTATE_STREAM;
	ncu->state = TX_ISP_MODULE_RUNNING;
	ncu->ms_flag = 0;
	ncu->start_cnt = 0;
	ncu->done_cnt = 0;
	ncu->reset_cnt = 0;
	private_spin_unlock_irqrestore(&ncu->slock, flags);

	private_mutex_unlock(&ncu->mlock);
	return 0;
failed_streamon:
	if(irqdev->irq)
		irqdev->disable_irq(irqdev);
failed_qbuf:
	if(ncu->num_inbufs){
		tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_FREE_BUFFER, NULL);
		isp_free_buffer(addr);
	}
exit:
	if(ncu->ref_frame_addr){
		isp_free_buffer(ncu->ref_frame_addr);
		ncu->ref_frame_addr = 0;
	}
	private_mutex_unlock(&ncu->mlock);
	return ret;
}

static int ncu_frame_channel_streamoff(struct tx_isp_subdev_pad *pad)
{
	int ret = 0;
	struct tx_isp_ncu_device *ncu = pad->priv;
	struct tx_isp_subdev_pad *inpad = ncu->sd.inpads;
	struct tx_isp_irq_device *irqdev = &(ncu->sd.irqdev);
	unsigned long flags = 0;
	unsigned int loop = 200;
	int index = 0;

	if(pad->type != TX_ISP_PADTYPE_OUTPUT){
		return 0;
	}

	private_mutex_lock(&ncu->mlock);
	if(inpad->state != TX_ISP_PADSTATE_STREAM
			|| pad->state != TX_ISP_PADSTATE_STREAM){
		private_mutex_unlock(&ncu->mlock);
		return 0;
	}

	private_spin_lock_irqsave(&ncu->slock, flags);
	ncu->state = TX_ISP_MODULE_INIT;
	tx_isp_reg_set(&ncu->sd, NCU_STOP, 0, 0, 1);
	private_spin_unlock_irqrestore(&ncu->slock, flags);

	while(!(tx_isp_sd_readl((&ncu->sd), NCU_START) & NCU_START_IDLE_MASK) && loop--){
		msleep(2);
	}
#if 0
	do{
		ret = wait_for_completion_interruptible_timeout(&ncu->stop_comp, msecs_to_jiffies(500));
	}while(ret == -ERESTARTSYS);
#endif
	if(inpad->link.flag & TX_ISP_PADLINK_DDR){
		tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_FREE_BUFFER, NULL);
	}

	if(ncu->ref_frame_addr){
		isp_free_buffer(ncu->ref_frame_addr);
		ncu->ref_frame_addr = 0;
	}

	ret = tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF, NULL);
	if(ret && ret != -ENOIOCTLCMD){
		goto failed_streamoff;
	}

	if(ncu->num_inbufs){
		isp_free_buffer(ncu->buf_addr);
		for(index = 0; index < ncu->num_inbufs; index++){
			INIT_LIST_HEAD(&(ncu->inbufs[index].entry));
			ncu->inbufs[index].addr = 0;
		}
		ncu->buf_addr = 0;
	}

	tx_isp_reg_set(&ncu->sd, NCU_INT_CNTRL, 0, 0, 0); // disable interrupt
	if(irqdev->irq)
		irqdev->disable_irq(irqdev);
	private_spin_lock_irqsave(&ncu->slock, flags);
	inpad->state = TX_ISP_PADSTATE_LINKED;
	pad->state = TX_ISP_PADSTATE_LINKED;
	private_spin_unlock_irqrestore(&ncu->slock, flags);
	private_mutex_unlock(&ncu->mlock);
	return 0;
failed_streamoff:
	private_mutex_unlock(&ncu->mlock);
	return ret;
}


static int ncu_pad_event_handle(struct tx_isp_subdev_pad *pad, unsigned int event, void *data)
{
	int ret = 0;

	if(pad->type == TX_ISP_MODULE_UNDEFINE)
		return 0;

	switch(event){
		case TX_ISP_EVENT_FRAME_CHAN_GET_FMT:
			ret = ncu_frame_channel_get_fmt(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_SET_FMT:
			ret = ncu_frame_channel_set_fmt(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_STREAM_ON:
			ret = ncu_frame_channel_streamon(pad);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF:
			ret = ncu_frame_channel_streamoff(pad);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER:
			ret = ncu_frame_channel_dqbuf(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER:
			ret = ncu_frame_channel_qbuf(pad, data);
			break;
		default:
			break;
	}
	return ret;
}

/* debug framesource info */
static int tx_isp_ncu_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_ncu_device *ncu = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct tx_isp_subdev_pad *inpad = IS_ERR_OR_NULL(sd) ? NULL : sd->inpads;
	struct frame_channel_buffer *pos = NULL;
	unsigned long flags = 0;

	if(IS_ERR_OR_NULL(ncu)){
		ISP_ERROR("The parameter is invalid!\n");
		return 0;
	}

	len += seq_printf(m ,"############## %s is %s ###############\n", module->name,
					ncu->state == TX_ISP_MODULE_RUNNING ? "running" : "idle");
	if(ncu->state != TX_ISP_MODULE_RUNNING)
		return len;
	if(inpad->link.flag & TX_ISP_PADLINK_LFB)
		return len;
	private_spin_lock_irqsave(&ncu->slock, flags);
	tx_list_for_each_entry(pos, &ncu->infifo, entry){
		len += seq_printf(m ,"infifo addr: 0x%08x\n", pos->addr);
	}
	len += seq_printf(m ,"current inbuf addr: 0x%08x\n", ncu->current_inbuf ? ncu->current_inbuf->addr : 0);
	len += seq_printf(m ,"ms_flag = %d\n", ncu->ms_flag);
	len += seq_printf(m ,"start cnt = %lld, done_cnt = %lld\n", ncu->start_cnt, ncu->done_cnt);
	len += seq_printf(m ,"reset cnt = %d\n", ncu->reset_cnt);
	private_spin_unlock_irqrestore(&ncu->slock, flags);
	return len;
}

static int tx_isp_ncu_open(struct inode *inode, struct file *file)
{
	return private_single_open_size(file, tx_isp_ncu_show, PDE_DATA(inode), 2048);
}

static struct file_operations ncu_proc_fops ={
	.read = private_seq_read,
	.open = tx_isp_ncu_open,
	.llseek = private_seq_lseek,
	.release = private_single_release,
};

static int tx_isp_ncu_probe(struct platform_device *pdev)
{
	struct tx_isp_ncu_device *ncu_dev = NULL;
	struct tx_isp_subdev *sd = NULL;
	struct tx_isp_device_descriptor *desc = NULL;
	int ret = ISP_SUCCESS;
	int index = 0;

	ncu_dev = (struct tx_isp_ncu_device *)kzalloc(sizeof(*ncu_dev), GFP_KERNEL);
	if(!ncu_dev){
		ISP_ERROR("Failed to allocate sensor device\n");
		ret = -ENOMEM;
		goto exit;
	}

	desc = pdev->dev.platform_data;
	sd = &ncu_dev->sd;
	ret = tx_isp_subdev_init(pdev, sd, &ncu_subdev_ops);
	if(ret){
		ISP_ERROR("Failed to init isp module(%d.%d)\n", desc->parentid, desc->unitid);
		ret = -ENOMEM;
		goto failed_to_ispmodule;
	}

	ncu_dev->num_inbufs = isp_m2_bufs;
	if(ncu_dev->num_inbufs){
		ncu_dev->inbufs = kzalloc(sizeof(struct frame_channel_buffer)*ncu_dev->num_inbufs, GFP_KERNEL);
		if(ncu_dev->inbufs == NULL){
			ISP_ERROR("Can't alloc memory!\n");
			ret = -ENOMEM;
			goto failed_inbufs;
		}
		for(index = 0; index < ncu_dev->num_inbufs; index++){
			INIT_LIST_HEAD(&(ncu_dev->inbufs[index].entry));
			ncu_dev->inbufs[index].priv = (unsigned int)ncu_dev;
		}
	}
	init_buffer_fifo(&ncu_dev->infifo);
	private_spin_lock_init(&ncu_dev->slock);
	private_mutex_init(&ncu_dev->mlock);
	ncu_dev->pdata = pdev->dev.platform_data;


	/* init input */
	if(sd->num_inpads){
		sd->inpads->event = ncu_pad_event_handle;
		sd->inpads->priv = ncu_dev;
	}else{
		ret = -EINVAL;
		goto failed_inpads;
	}

	/* init output */
	if(sd->num_outpads){
		sd->outpads->event = ncu_pad_event_handle;
		sd->outpads->priv = ncu_dev;
	}else{
		ret = -EINVAL;
		goto failed_outpads;
	}

	private_init_completion(&ncu_dev->stop_comp);
	ncu_dev->state = TX_ISP_MODULE_SLAKE;
	private_platform_set_drvdata(pdev, &sd->module);
	tx_isp_set_subdevdata(sd, ncu_dev);
	tx_isp_set_subdev_debugops(sd, &ncu_proc_fops);
	g_ncu = ncu_dev;

	return ISP_SUCCESS;
failed_outpads:
failed_inpads:
failed_inbufs:
	tx_isp_subdev_deinit(sd);
failed_to_ispmodule:
	kfree(ncu_dev);
exit:
	return ret;
}

static int __exit tx_isp_ncu_remove(struct platform_device *pdev)
{
	struct tx_isp_module *module = private_platform_get_drvdata(pdev);
	struct tx_isp_subdev *sd = module_to_subdev(module);
	struct tx_isp_ncu_device *ncu = tx_isp_get_subdevdata(sd);
	g_ncu = NULL;
	tx_isp_subdev_deinit(sd);
	kfree(ncu);
	return 0;
}

struct platform_driver tx_isp_ncu_driver = {
	.probe = tx_isp_ncu_probe,
	.remove = __exit_p(tx_isp_ncu_remove),
	.driver = {
		.name = TX_ISP_NCU_NAME,
		.owner = THIS_MODULE,
	},
};
