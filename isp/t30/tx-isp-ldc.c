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
#include "tx-isp-ldc.h"
#include "tx-isp-frame-channel.h"
#include "tx-isp-videobuf.h"

static int isp_m1_bufs = 2;
module_param(isp_m1_bufs, int, S_IRUGO);
MODULE_PARM_DESC(isp_m1_bufs, "isp m1 inter buffers");

static char ldc_params_version[16] = "20190610a";
tx_isp_ldc_opt *ldc_user_params = NULL;
tx_isp_ldc_opt ldc_default_params[] = {
	/* 1280x720 */
	{
		.width		= 1280,
		.height		= 720,
		.w_str		= 1280,
		.r_str		= 1280,
		.k1_x		= 352,
		.k2_x		= 139,
		.k1_y		= 352,
		.k2_y		= 139,
		.p1_val_x	= 2212,
		.p1_val_y	= 2212,
		.r2_rep		= 1991,
		.wr_len		= 13 - 1,
		.wr_side_len	= 2 - 1,
		.rd_len		= 16 - 1,
		.rd_side_len	= 16 - 1,
		.y_fill		= 255,
		.u_fill		= 128,
		.v_fill		= 128,
		.view_mode	= 1,
		.udis_r		= 0,
		.y_shift_lut	= {0,0,0,0,221,0,285},
		.uv_shift_lut	= {0,0,0,0,109,0,144},
	},
	/* 1920x1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.w_str		= 1920,
		.r_str		= 1920,
		.k1_x		= 352,
		.k2_x		= 139,
		.k1_y		= 352,
		.k2_y		= 139,
		.p1_val_x	= 2212,
		.p1_val_y	= 2212,
		.r2_rep		= 885,
		.wr_len		= 13 - 1,
		.wr_side_len	= 3 - 1,
		.rd_len		= 16 - 1,
		.rd_side_len	= 16 - 1,
		.y_fill		= 255,
		.u_fill 	= 128,
		.v_fill		= 128,
		.view_mode	= 1,
		.udis_r		= 0,
		.y_shift_lut 	= {0,0,421,0,373,0,308,0,228,379,0,243,0,206,0,325},
		.uv_shift_lut 	= {0,0,206,0,184,0,152,0,113,188,0,120,0,103,0,163},
	},
	/* 1920x1280 */
	{
		.width		= 1920,
		.height		= 1280,
		.w_str		= 1920,
		.r_str		= 1920,
		.k1_x		= 410,
		.k2_x		= 102,
		.k1_y		= 410,
		.k2_y		= 102,
		.p1_val_x	= 2310,
		.p1_val_y	= 2310,
		.r2_rep		= 808,
		.wr_len		= 12 - 1,
		.wr_side_len	= 12 - 1,
		.rd_len		= 16 - 1,
		.rd_side_len	= 16 - 1,
		.y_fill		= 128,
		.u_fill		= 128,
		.v_fill		= 128,
		.view_mode	= 1,
		.udis_r		= 0,
		.y_shift_lut	= {447,544,0,374,438,541,0,362,433,553,0,297,358,439,0,289,361,466,0,286,376,532,0,278,395,0,245,394,0,301},
		.uv_shift_lut	= {221,267,0,184,216,266,0,179,214,271,0,147,177,218,309,0,143,179,231,0,142,186,264,0,138,196,0,122,197,0,151},
	},
	/* 2048x1536 */
	{
		.width		= 2048,
		.height		= 1536,
		.w_str		= 2048,
		.r_str		= 2048,
		.k1_x		= 256,
		.k2_x		= 51,
		.k1_y		= 256,
		.k2_y		= 51,
		.p1_val_x	= 2234,
		.p1_val_y	= 2234,
		.r2_rep		= 656,
		.wr_len		= 12 - 1,
		.wr_side_len	= 8 - 1,
		.rd_len		= 16 - 1,
		.rd_side_len	= 16 - 1,
		.y_fill		= 128,
		.u_fill		= 128,
		.v_fill		= 128,
		.view_mode	= 1,
		.udis_r		= 0,
		.y_shift_lut	= {424,515,674,0,368,451,566,0,362,453,586,0,326,419,550,0,344,460,678,0,324,456,0,373,586,0,351,641,0,419},
		.uv_shift_lut	= {209,254,331,0,182,223,280,0,180,224,290,0,161,207,273,0,170,229,335,0,161,227,0,186,293,0,175,322,0,210},
	},
	/* 2304x1536 */
	{
		.width		= 2304,
		.height		= 1536,
		.w_str		= 2304,
		.r_str		= 2304,
		.k1_x		= 410,
		.k2_x		= 102,
		.k1_y		= 410,
		.k2_y		= 102,
		.p1_val_x	= 2310,
		.p1_val_y	= 2310,
		.r2_rep		= 561,
		.wr_len		= 12 - 1,
		.wr_side_len	= 12 - 1,
		.rd_len		= 16 - 1,
		.rd_side_len	= 16 - 1,
		.y_fill		= 128,
		.u_fill		= 128,
		.v_fill		= 128,
		.view_mode	= 1,
		.udis_r		= 0,
		.y_shift_lut	= {442,497,568,689,0,378,426,482,554,672,0,351,399,456,527,635,0,316,365,422,493,591,0,274,323,381,450,543,0,271,329,397,485,624,0,260,328,411,525,0,234,311,409,556,0,259,368,534,0,253,407,0,339},
		.uv_shift_lut	= {219,246,281,337,0,210,238,273,330,0,174,197,226,261,313,0,157,181,209,244,293,0,135,160,189,223,270,376,0,134,163,197,241,309,0,129,162,205,262,0,116,155,204,277,0,129,184,267,0,126,204,0,170},
	},
	/* 2592x2048 */
	{
		.width		= 2592,
		.height		= 2048,
		.w_str		= 2592,
		.r_str		= 2592,
		.k1_x		= 352,
		.k2_x		= 139,
		.k1_y		= 352,
		.k2_y		= 139,
		.p1_val_x	= 2305,
		.p1_val_y	= 2305,
		.r2_rep		= 393,
		.wr_len		= 12 - 1,
		.wr_side_len	= 6 - 1,
		.rd_len		= 16 - 1,
		.rd_side_len	= 16 - 1,
		.y_fill		= 255,
		.u_fill		= 128,
		.v_fill		= 128,
		.view_mode	= 1,
		.udis_r		= 0,
		.y_shift_lut	= {441,521,611,720,903,0,421,492,574,671,804,0,350,414,485,564,658,787,0,364,430,503,587,692,851,0,337,403,476,560,664,814,0,272,336,407,486,580,700,916,0,297,370,453,549,674,883,0,286,367,461,574,730,0,312,411,531,696,0,284,400,546,771,0,290,444,665,0,361,634,0,714},
		.uv_shift_lut	= {259,303,358,445,0,208,244,285,333,398,0,173,206,241,280,327,391,0,180,213,250,292,344,422,0,167,200,237,278,330,405,0,135,167,202,242,289,349,454,0,147,184,225,274,335,440,0,143,183,229,287,365,0,155,205,265,348,0,141,199,273,386,0,145,222,334,0,181,319,0,363},
	},
};

static unsigned int ldc_params_nums = ARRAY_SIZE(ldc_default_params);
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

static struct frame_channel_buffer *addr_to_inbuf(struct tx_isp_ldc_device *ldc, unsigned int addr)
{
	struct frame_channel_buffer *buf = NULL;
	int index = 0;
	if(!ldc)
		return NULL;
	for(index = 0; index < ldc->num_inbufs; index++)
		if(ldc->inbufs[index].addr == addr){
			buf = &(ldc->inbufs[index]);
			break;
		}
	return buf;
}

static void ldc_restart_module(struct tx_isp_ldc_device *ldc)
{
	struct frame_channel_buffer *tmp = NULL;
#if 0
	tx_isp_reg_set(&ldc->sd, LDC_CTR, 1, 1, 1); // reset ldc

	tx_isp_sd_writel(&ldc->sd, LDC_CTR, ldc->regs.ctrl);
	tx_isp_sd_writel(&ldc->sd, LDC_RES_VAL, ldc->regs.resolution);
	tx_isp_sd_writel(&ldc->sd, LDC_DIS_COEFFX, ldc->regs.coef_x);
	tx_isp_sd_writel(&ldc->sd, LDC_DIS_COEFFY, ldc->regs.coef_y);
	tx_isp_sd_writel(&ldc->sd, LDC_UDIS_R, ldc->regs.udis_r);
	tx_isp_sd_writel(&ldc->sd, LDC_LINE_LEN, ldc->regs.len);
	tx_isp_sd_writel(&ldc->sd, LDC_P1_VAL, ldc->regs.p1);
	tx_isp_sd_writel(&ldc->sd, LDC_R2_REP, ldc->regs.r2);
	tx_isp_sd_writel(&ldc->sd, LDC_FILL_VAL, ldc->regs.fill);
	tx_isp_sd_writel(&ldc->sd, LDC_Y_IN_STR, ldc->regs.stride);
	tx_isp_sd_writel(&ldc->sd, LDC_UV_IN_STR, ldc->regs.stride);
	tx_isp_sd_writel(&ldc->sd, LDC_Y_OUT_STR, ldc->regs.stride);
	tx_isp_sd_writel(&ldc->sd, LDC_UV_OUT_STR, ldc->regs.stride);
#endif
	if(!is_empty_fifo(&ldc->outfifo) && !is_empty_fifo(&ldc->infifo)){
		tmp = pop_buffer_fifo(&ldc->outfifo);
		tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAOUT, tmp->addr);
		tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAOUT, tmp->addr + ldc->uv_offset);
		/*printk("%s[%d]: outbuf = 0x%08x\n",__func__,__LINE__,tmp->addr);*/
		ldc->cur_outbuf = tmp;
		tmp = pop_buffer_fifo(&ldc->infifo);
		tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAIN, tmp->addr);
		tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAIN, tmp->addr + ldc->uv_offset);
		/*printk("%s[%d]: inbuf = 0x%08x\n",__func__,__LINE__,tmp->addr);*/
		ldc->cur_inbuf = tmp;
		tx_isp_reg_set(&ldc->sd, LDC_CTR, 0, 0, 1); // start ldc
		ldc->frame_state = 1;
		ldc->start_cnt++;
	}
	return;
}

/*
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
   interrupt handler
   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 */
static irqreturn_t ldc_core_interrupt_service_routine(struct tx_isp_subdev *sd, u32 status, bool *handled)
{
	struct tx_isp_ldc_device *ldc = tx_isp_get_subdevdata(sd);
	/*struct frame_channel_buffer *tmp = NULL;*/
	unsigned int stat = tx_isp_sd_readl((&ldc->sd), LDC_SAT);

	if(stat & LDC_STAT_FRAME_DONE){
		tx_isp_send_event_to_remote(sd->outpads, TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER, ldc->cur_outbuf);
		ldc->cur_outbuf = NULL;
		tx_isp_send_event_to_remote(sd->inpads, TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER, ldc->cur_inbuf);
		ldc->cur_inbuf = NULL;
		ldc->frame_state = 0;
		ldc->done_cnt++;
		/*printk("%s[%d]: \n",__func__,__LINE__);*/
	}

	if(ldc->state == TX_ISP_MODULE_RUNNING){
		if(((stat & LDC_STAT_ST_MASK) != LDC_STAT_ST_RUN) && (!ldc->frame_state)){
#if 0
			if(!is_empty_fifo(&ldc->outfifo) && !is_empty_fifo(&ldc->infifo)){
				tmp = pop_buffer_fifo(&ldc->outfifo);
				tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAOUT, tmp->addr);
				tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAOUT, tmp->addr + ldc->uv_offset);
    			printk("%s[%d]: outbuf = 0x%08x\n",__func__,__LINE__,tmp->addr);
				ldc->cur_outbuf = tmp;
				tmp = pop_buffer_fifo(&ldc->infifo);
				tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAIN, tmp->addr);
				tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAIN, tmp->addr + ldc->uv_offset);
    			printk("%s[%d]: inbuf = 0x%08x\n",__func__,__LINE__,tmp->addr);
				ldc->cur_inbuf = tmp;
				tx_isp_reg_set(&ldc->sd, LDC_CTR, 0, 0, 1); // start ldc
			}
#else
		ldc_restart_module(ldc);
#endif
		}
	}

	tx_isp_reg_set(&ldc->sd, LDC_SAT, 0, 0, 1); // clear interrupt

	return IRQ_HANDLED;
}

extern void tx_isp_sync_ncu(void);
static struct tx_isp_ldc_device *g_ldc = NULL;
static unsigned long long tx_isp_last_done = 0;
static unsigned int lost_cnt = 0;
void tx_isp_sync_ldc(void)
{
	struct tx_isp_ldc_device *ldc = g_ldc;

	if((ldc == NULL) || (ldc->state != TX_ISP_MODULE_RUNNING))
		return;
	if((tx_isp_last_done != ldc->done_cnt) || (ldc->start_cnt == ldc->done_cnt)){
		tx_isp_last_done = ldc->done_cnt;
		lost_cnt = 0;;
	}else{
		lost_cnt++;
	}
	if((lost_cnt >= 3) && g_ldc){
		/* reset ldc */
		unsigned int ldc_y_dmaout = tx_isp_sd_readl(&ldc->sd, LDC_Y_DMAOUT);
		unsigned int ldc_uv_dmaout = tx_isp_sd_readl(&ldc->sd, LDC_UV_DMAOUT);
		unsigned int ldc_y_dmain = tx_isp_sd_readl(&ldc->sd, LDC_Y_DMAIN);
		unsigned int ldc_uv_dmain = tx_isp_sd_readl(&ldc->sd, LDC_UV_DMAIN);
		ldc->start_cnt = ldc->done_cnt;
		tx_isp_reg_set(&ldc->sd, LDC_CTR, 1, 1, 1); // reset ldc

		tx_isp_sd_writel(&ldc->sd, LDC_RES_VAL, ldc->regs.resolution);
		tx_isp_sd_writel(&ldc->sd, LDC_DIS_COEFFX, ldc->regs.coef_x);
		tx_isp_sd_writel(&ldc->sd, LDC_DIS_COEFFY, ldc->regs.coef_y);
		tx_isp_sd_writel(&ldc->sd, LDC_UDIS_R, ldc->regs.udis_r);
		tx_isp_sd_writel(&ldc->sd, LDC_LINE_LEN, ldc->regs.len);
		tx_isp_sd_writel(&ldc->sd, LDC_P1_VAL, ldc->regs.p1);
		tx_isp_sd_writel(&ldc->sd, LDC_R2_REP, ldc->regs.r2);
		tx_isp_sd_writel(&ldc->sd, LDC_FILL_VAL, ldc->regs.fill);
		tx_isp_sd_writel(&ldc->sd, LDC_Y_IN_STR, ldc->regs.stride);
		tx_isp_sd_writel(&ldc->sd, LDC_UV_IN_STR, ldc->regs.stride);
		tx_isp_sd_writel(&ldc->sd, LDC_Y_OUT_STR, ldc->regs.stride);
		tx_isp_sd_writel(&ldc->sd, LDC_UV_OUT_STR, ldc->regs.stride);
		tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAOUT, ldc_y_dmaout);
		tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAOUT, ldc_uv_dmaout);
		tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAIN, ldc_y_dmain);
		tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAIN, ldc_uv_dmain);

		tx_isp_sd_writel(&ldc->sd, LDC_CTR, ldc->regs.ctrl | 1);
		ldc->frame_state = 1;
		ldc->start_cnt++;
		ldc->reset_cnt++;
	}

	tx_isp_sync_ncu();
}


static int ldc_frame_channel_dqbuf(struct tx_isp_subdev_pad *pad, void *data)
{
	struct frame_channel_buffer *buf = NULL, *tmp = NULL;
	struct tx_isp_ldc_device *ldc = pad->priv;
	unsigned long flags = 0;
//	unsigned int stat = 0;

	tmp = data;
	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		return 0;
	}

	spin_lock_irqsave(&ldc->slock, flags);
	if(tmp && ldc){
		buf = addr_to_inbuf(ldc, tmp->addr);
		if(buf)
			push_buffer_fifo(&ldc->infifo, buf);
		else
			ISP_ERROR("Can't find the addr(0x%08x) in bufs\n", tmp->addr);
	}

	if(ldc->state == TX_ISP_MODULE_RUNNING){
//		stat = tx_isp_sd_readl((&ldc->sd), LDC_SAT);
  //  	printk("%s[%d]: stat = 0x%08x\n",__func__,__LINE__,stat);
		if(((tx_isp_sd_readl((&ldc->sd), LDC_SAT) & LDC_STAT_ST_MASK)
				!= LDC_STAT_ST_RUN)  && (!ldc->frame_state)){
#if 0
			if(!is_empty_fifo(&ldc->outfifo) && !is_empty_fifo(&ldc->infifo)){
				buf = pop_buffer_fifo(&ldc->outfifo);
				tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAOUT, buf->addr);
				tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAOUT, buf->addr + ldc->uv_offset);
				ldc->cur_outbuf = buf;
    			printk("%s[%d]: outbuf = 0x%08x\n",__func__,__LINE__,buf->addr);
				buf = pop_buffer_fifo(&ldc->infifo);
				tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAIN, buf->addr);
				tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAIN, buf->addr + ldc->uv_offset);
				tx_isp_reg_set(&ldc->sd, LDC_CTR, 0, 0, 1); // start ldc
    			printk("%s[%d]: inbuf = 0x%08x\n",__func__,__LINE__,buf->addr);
				ldc->cur_inbuf = buf;
			}
#else
		ldc_restart_module(ldc);
#endif
		}
	}
	spin_unlock_irqrestore(&ldc->slock, flags);

	return 0;
}

static int ldc_frame_channel_qbuf(struct tx_isp_subdev_pad *pad, void *data)
{
	struct frame_channel_buffer *buf = NULL;
	struct tx_isp_ldc_device *ldc = pad->priv;
	unsigned long flags = 0;

	buf = data;
	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		return 0;
	}

	spin_lock_irqsave(&ldc->slock, flags);
	if(buf && ldc){
		push_buffer_fifo(&ldc->outfifo, buf);
	}

	if(ldc->state == TX_ISP_MODULE_RUNNING){
		if(((tx_isp_sd_readl((&ldc->sd), LDC_SAT) & LDC_STAT_ST_MASK)
				!= LDC_STAT_ST_RUN) && (!ldc->frame_state)){
#if 0
			if(!is_empty_fifo(&ldc->infifo)){
				buf = pop_buffer_fifo(&ldc->outfifo);
				tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAOUT, buf->addr);
				tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAOUT, buf->addr + ldc->uv_offset);
    			printk("%s[%d]: outbuf = 0x%08x\n",__func__,__LINE__,buf->addr);
				ldc->cur_outbuf = buf;
				buf = pop_buffer_fifo(&ldc->infifo);
				tx_isp_sd_writel(&ldc->sd, LDC_Y_DMAIN, buf->addr);
				tx_isp_sd_writel(&ldc->sd, LDC_UV_DMAIN, buf->addr + ldc->uv_offset);
    			printk("%s[%d]: inbuf = 0x%08x\n",__func__,__LINE__,buf->addr);
				ldc->cur_inbuf = buf;
				tx_isp_reg_set(&ldc->sd, LDC_CTR, 0, 0, 1); // start ldc
			}
#else
		ldc_restart_module(ldc);
#endif
		}
	}
	spin_unlock_irqrestore(&ldc->slock, flags);

	return 0;
}

static int ldc_frame_channel_freebufs(struct tx_isp_subdev_pad *pad, void *data)
{
	struct tx_isp_ldc_device *ldc = pad->priv;
	unsigned long flags = 0;

	if(ldc){
		private_spin_lock_irqsave(&ldc->slock, flags);
		cleanup_buffer_fifo(&ldc->outfifo);
		private_spin_unlock_irqrestore(&ldc->slock, flags);
	}
	return 0;
}


static int ldc_core_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
static void ldc_clks_ops(struct tx_isp_subdev *sd, int on)
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

static const unsigned int crc_table[8] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL,
	0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
};

static unsigned int crc32(const unsigned int *p, unsigned int len)
{
	int i = 0;
	unsigned int crc = crc_table[0];
	for(i = 0; i < len; i++){
		crc ^= *p++;
		crc = crc ^ crc_table[crc & 0x7];
	}

	return crc;
}

static void ldc_load_parameters(struct tx_isp_ldc_device *ldc)
{
	struct tx_isp_sensor_attribute *attr = ldc->vin.attr;
	struct file *file = NULL;
	struct inode *inode = NULL;
	mm_segment_t old_fs;
	loff_t fsize;
	loff_t *pos;
	unsigned int crc = 0;
	struct ldc_params_header *header;
	char file_name[64];

	if(ldc_user_params)
		return;

	memset(file_name, 0, sizeof(file_name));
	/* open file */
	snprintf(file_name, sizeof(file_name), "/etc/sensor/ldc_%s.bin", attr->name);
	file = filp_open(file_name, O_RDONLY, 0);
	if (file < 0 || IS_ERR(file)) {
		goto failed_open_file;
	}
	/* read file */
	inode = file->f_dentry->d_inode;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = &(file->f_pos);

	/* check flags and crc */
	if(ldc->udata == NULL){
		ldc->udata = kmalloc(fsize, GFP_KERNEL);
		if(ldc->udata == NULL){
			printk("%s[%d]: Failed to alloc %lld KB buffer!\n",__func__,__LINE__, fsize >> 10);
			goto failed_malloc_data;
		}
	}
	vfs_read(file, ldc->udata, fsize, pos);
	filp_close(file, NULL);
	set_fs(old_fs);

	header = (struct ldc_params_header *)ldc->udata;
	if(header->flags != fsize - sizeof(*header)){
		printk("flags is error; LDC will use default parameter!\n");
		goto flags_error;
	}

	ldc_user_params = (tx_isp_ldc_opt *)(ldc->udata + sizeof(*header));
	crc = crc32((void *)ldc_user_params, header->flags / sizeof(int));
	if(header->crc != crc){
		printk("crc is error; LDC will use default parameter!\n");
		goto crc_error;
	}
	ldc_params_nums = header->flags / sizeof(tx_isp_ldc_opt);
	memcpy(ldc_params_version, header->version, sizeof(ldc_params_version));
	return;

crc_error:
	ldc_user_params = NULL;
flags_error:
	kfree(ldc->udata);
	ldc->udata = NULL;
failed_malloc_data:
failed_open_file:
	return;
}

static int ldc_frame_channel_streamoff(struct tx_isp_subdev_pad *pad);
static int ldc_core_ops_init(struct tx_isp_subdev *sd, int on)
{
	struct tx_isp_ldc_device *ldc = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;
	int ret = 0;

	if(IS_ERR_OR_NULL(ldc)){
		return -EINVAL;
	}

	if(ldc->state == TX_ISP_MODULE_SLAKE)
		return 0;
	/*printk("## %s %d on=%d ##\n", __func__,__LINE__, on);*/

	if(on){
		if(private_reset_tx_isp_module(TX_ISP_LDC_SUBDEV_ID)){
			ISP_ERROR("Failed to reset %s\n", sd->module.name);
			printk("## %s %d on=%d ##\n", __func__,__LINE__, on);
			ret = -EINVAL;
			goto exit;
		}
		ldc_load_parameters(ldc);
		private_spin_lock_irqsave(&ldc->slock, flags);
		if(ldc->state != TX_ISP_MODULE_ACTIVATE){
			private_spin_unlock_irqrestore(&ldc->slock, flags);
			ISP_ERROR("Can't init ispcore when its state is %d\n!", ldc->state);
			ret = -EPERM;
			goto exit;
		}
		ldc->state = TX_ISP_MODULE_INIT;
		private_spin_unlock_irqrestore(&ldc->slock, flags);
	}else{
		if(ldc->state == TX_ISP_MODULE_RUNNING){
			ldc_frame_channel_streamoff(sd->outpads);
		}
		private_spin_lock_irqsave(&ldc->slock, flags);
		ldc->state = TX_ISP_MODULE_DEINIT;
		private_spin_unlock_irqrestore(&ldc->slock, flags);
	}
	return 0;
exit:
	return ret;
}

static struct tx_isp_subdev_core_ops ldc_subdev_core_ops ={
	.init = ldc_core_ops_init,
	.ioctl = ldc_core_ops_ioctl,
	.interrupt_service_routine = ldc_core_interrupt_service_routine,
};

static int ldc_sync_sensor_attr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_ldc_device *ldc = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	if(IS_ERR_OR_NULL(ldc)){
		ISP_ERROR("The parameter is invalid!\n");
		return -EINVAL;
	}
	if(arg){
		memcpy(&ldc->vin, (void *)arg, sizeof(struct tx_isp_video_in));
	}else
		memset(&ldc->vin, 0, sizeof(struct tx_isp_video_in));
	return 0;
}

static struct tx_isp_subdev_sensor_ops ldc_sensor_ops = {
	.sync_sensor_attr = ldc_sync_sensor_attr,
};

static int ldc_activate_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_ldc_device *ldc = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;

	if(IS_ERR_OR_NULL(ldc))
		return -EINVAL;
	if(ldc->state != TX_ISP_MODULE_SLAKE)
		return 0;

	private_spin_lock_irqsave(&ldc->slock, flags);
	/* clk ops */
	ldc_clks_ops(sd, 1);
	init_buffer_fifo(&ldc->outfifo);
	init_buffer_fifo(&ldc->infifo);
	ldc->state = TX_ISP_MODULE_ACTIVATE;
	private_spin_unlock_irqrestore(&ldc->slock, flags);
	return 0;
}

static int ldc_slake_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_ldc_device *ldc = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned long flags = 0;

	if(IS_ERR_OR_NULL(ldc))
		return -EINVAL;
	if(ldc->state == TX_ISP_MODULE_SLAKE)
		return 0;

	if(ldc->state > TX_ISP_MODULE_ACTIVATE)
		ldc_core_ops_init(sd, 0);

	private_spin_lock_irqsave(&ldc->slock, flags);
	/* clk ops */
	ldc_clks_ops(sd, 0);
	ldc->state = TX_ISP_MODULE_SLAKE;
	private_spin_unlock_irqrestore(&ldc->slock, flags);
	if(ldc->udata){
		kfree(ldc->udata);
		ldc->udata = NULL;
		ldc_user_params = NULL;
		ldc_params_nums = ARRAY_SIZE(ldc_default_params);
	}
	return 0;
}


int ldc_link_setup(const struct tx_isp_subdev_pad *local,
			  const struct tx_isp_subdev_pad *remote, u32 flags)
{
	return 0;
}


static struct tx_isp_subdev_video_ops ldc_subdev_video_ops = {
	/*.s_stream = ldc_video_s_stream,*/
	.link_setup = ldc_link_setup,
};

static struct tx_isp_subdev_internal_ops ldc_internal_ops = {
	.activate_module = &ldc_activate_module,
	.slake_module = &ldc_slake_module,
};

static struct tx_isp_subdev_ops ldc_subdev_ops = {
	.internal = &ldc_internal_ops,
	.core = &ldc_subdev_core_ops,
	.video = &ldc_subdev_video_ops,
	.sensor = &ldc_sensor_ops,
};

/*
 * ----------------- init device ------------------------
 */
static inline int ldc_frame_channel_get_fmt(struct tx_isp_subdev_pad *pad, void *data)
{
	struct tx_isp_ldc_device *ldc = pad->priv;

	if(data && ldc)
		memcpy(data, &(ldc->fmt), sizeof(struct frame_image_format));
	return 0;
}

static int ldc_frame_channel_set_fmt(struct tx_isp_subdev_pad *pad, void *data)
{
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(pad) ? NULL : pad->sd;
	struct tx_isp_ldc_device *ldc = IS_ERR_OR_NULL(pad) ? NULL : pad->priv;
	struct frame_image_format *fmt = NULL;
	tx_isp_ldc_opt *ldc_params = NULL;
	unsigned int index = 0;
	int ret = 0;

	if(IS_ERR_OR_NULL(pad) || (pad->type == TX_ISP_PADTYPE_UNDEFINE))
		return -EPERM;

	fmt = data;
	if(pad->type == TX_ISP_PADTYPE_OUTPUT){
		if(fmt->pix.pixelformat != V4L2_PIX_FMT_NV12
				&& fmt->pix.pixelformat != V4L2_PIX_FMT_NV21){
			char *value = (char *)&(fmt->pix.pixelformat);
			ISP_ERROR("%s can't support the fmt(%c%c%c%c)\n", sd->module.name, value[0], value[1], value[2], value[3]);
			return -EINVAL;
		}

		ldc->current_param = NULL;
		ldc_params = ldc_user_params ? ldc_user_params : ldc_default_params;
		for(index = 0; index < ldc_params_nums; index++){
			if((ldc_params[index].width == fmt->pix.width) && (ldc_params[index].height == fmt->pix.height)){
				ldc->current_param = &(ldc_params[index]);
				break;
			}
		}

		if(IS_ERR_OR_NULL(ldc->current_param)){
			ISP_ERROR("The module of %s couldn't support the resolution(%d*%d)\n",
					sd->module.name, fmt->pix.width, fmt->pix.height);
			return -EINVAL;
		}

		if(fmt->pix.width > ldc->vin.vi_max_width || fmt->pix.height > ldc->vin.vi_max_height)
		{
			ISP_ERROR("The resolution is invalid, set(%d*%d), sensor(%d*%d)\n",
					fmt->pix.width, fmt->pix.height, ldc->vin.vi_max_width, ldc->vin.vi_max_height);
			return -EINVAL;
		}
		ret = tx_isp_send_event_to_remote(sd->inpads, TX_ISP_EVENT_FRAME_CHAN_SET_FMT, data);
		if(ret && ret != -ENOIOCTLCMD){
			return ret;
		};

		memcpy(&ldc->fmt, fmt, sizeof(*fmt));
		ldc->uv_offset = fmt->pix.width * fmt->pix.height;
	}
	return 0;
}

static int ldc_frame_channel_streamon(struct tx_isp_subdev_pad *pad)
{
	int ret = 0;
	struct tx_isp_ldc_device *ldc = pad->priv;
	struct tx_isp_subdev_pad *inpad = ldc->sd.inpads;
	struct tx_isp_irq_device *irqdev = &(ldc->sd.irqdev);
	unsigned int addr = 0;
	unsigned long flags = 0;
	tx_isp_ldc_opt *opt = NULL;
	int index = 0;

	if(pad->type != TX_ISP_PADTYPE_OUTPUT){
		return 0;
	}

	if(IS_ERR_OR_NULL(ldc->current_param)){
		ISP_ERROR("Please set fmt firstly!\n");
		return -EINVAL;
	}

	private_mutex_lock(&ldc->mlock);
	if(inpad->state != TX_ISP_PADSTATE_LINKED
			|| pad->state != TX_ISP_PADSTATE_LINKED){
		ISP_ERROR("Please setup link firstly!\n");
		private_mutex_unlock(&ldc->mlock);
		return -EPERM;
	}

	/* software reset */
	tx_isp_sd_writel(&ldc->sd, LDC_CTR, 1<<1);

	if(pad->link.flag & TX_ISP_PADLINK_LFB){
		ISP_ERROR("The link must be DDR!\n");
		private_mutex_unlock(&ldc->mlock);
		return -EINVAL;
	}else{
		tx_isp_sd_writel(&ldc->sd, LDC_Y_IN_STR, ldc->fmt.pix.width);
		tx_isp_sd_writel(&ldc->sd, LDC_UV_IN_STR, ldc->fmt.pix.width);
		tx_isp_sd_writel(&ldc->sd, LDC_Y_OUT_STR, ldc->fmt.pix.width);
		tx_isp_sd_writel(&ldc->sd, LDC_UV_OUT_STR, ldc->fmt.pix.width);
	}

	if(inpad->link.flag & TX_ISP_PADLINK_DDR){
		if(ldc->num_inbufs == 0){
			ISP_ERROR("Please config isp_m1_bufs when insmod driver!\n");
			goto exit;
		}
		addr = isp_malloc_buffer(ldc->fmt.pix.sizeimage * ldc->num_inbufs);
		if(addr == 0){
			ISP_ERROR("ispmem isn't enough!\n");
			goto exit;
		}
		for(index = 0; index < ldc->num_inbufs; index++){
			INIT_LIST_HEAD(&(ldc->inbufs[index].entry));
			ldc->inbufs[index].addr = addr + index * ldc->fmt.pix.sizeimage;
			ret = tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER, &(ldc->inbufs[index]));
			if(ret && ret != -ENOIOCTLCMD){
				goto failed_qbuf;
			}
		}

		ret = tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_SET_BANKS, &ldc->num_inbufs);
		if (ret && ret != -ENOIOCTLCMD) {
			ISP_ERROR("streamon: driver refused to start streaming\n");
			goto failed_qbuf;
		}
		ldc->buf_addr = addr;
	}

	opt = ldc->current_param;

	tx_isp_sd_writel(&ldc->sd, LDC_DIS_COEFFX, opt->k1_x | (opt->k2_x << 16));
	tx_isp_sd_writel(&ldc->sd, LDC_DIS_COEFFY, opt->k1_y | (opt->k2_y << 16));
	tx_isp_sd_writel(&ldc->sd, LDC_UDIS_R, opt->udis_r);
	tx_isp_sd_writel(&ldc->sd, LDC_LINE_LEN, opt->wr_len | (opt->wr_side_len << 8)
										| (opt->rd_len << 16) | (opt->rd_side_len << 24));

	tx_isp_sd_writel(&ldc->sd, LDC_P1_VAL, opt->p1_val_x | (opt->p1_val_y << 16));
	tx_isp_sd_writel(&ldc->sd, LDC_R2_REP, opt->r2_rep);
	tx_isp_sd_writel(&ldc->sd, LDC_FILL_VAL, opt->v_fill | (opt->u_fill << 8) | (opt->y_fill << 16));


	for(index = 0; index < 256; index++)
		tx_isp_sd_writel(&ldc->sd, LDC_Y_ROW_POS_BADDR + index * 4, opt->y_shift_lut[index]);
	for(index = 0; index < 256; index++)
		tx_isp_sd_writel(&ldc->sd, LDC_UV_ROW_POS_BADDR + index * 4, opt->uv_shift_lut[index]);

	tx_isp_sd_writel(&ldc->sd, LDC_RES_VAL, ldc->fmt.pix.height<<16 | ldc->fmt.pix.width);
	tx_isp_sd_writel(&ldc->sd, LDC_CTR, LDC_CTRL_INTC_ENABLE | LDC_CTRL_MODE_CROP | LDC_CTRL_STOP_WAIT_AXI);

	{
		/* save ldc registers */
		ldc->regs.ctrl = tx_isp_sd_readl(&ldc->sd, LDC_CTR);
		ldc->regs.resolution = tx_isp_sd_readl(&ldc->sd, LDC_RES_VAL);
		ldc->regs.coef_x = tx_isp_sd_readl(&ldc->sd, LDC_DIS_COEFFX);
		ldc->regs.coef_y = tx_isp_sd_readl(&ldc->sd, LDC_DIS_COEFFY);
		ldc->regs.udis_r = tx_isp_sd_readl(&ldc->sd, LDC_UDIS_R);
		ldc->regs.len = tx_isp_sd_readl(&ldc->sd, LDC_LINE_LEN);
		ldc->regs.p1 = tx_isp_sd_readl(&ldc->sd, LDC_P1_VAL);
		ldc->regs.r2 = tx_isp_sd_readl(&ldc->sd, LDC_R2_REP);
		ldc->regs.fill = tx_isp_sd_readl(&ldc->sd, LDC_FILL_VAL);
		ldc->regs.stride = ldc->fmt.pix.width;
	}

	if(irqdev->irq)
		irqdev->enable_irq(irqdev);

	private_spin_lock_irqsave(&ldc->slock, flags);
	inpad->state = TX_ISP_PADSTATE_STREAM;
	pad->state = TX_ISP_PADSTATE_STREAM;
	ldc->state = TX_ISP_MODULE_RUNNING;
	ldc->cur_outbuf = NULL;
	ldc->cur_inbuf = NULL;
	ldc->done_cnt = 0;
	ldc->start_cnt = 0;
	ldc->reset_cnt = 0;
	private_spin_unlock_irqrestore(&ldc->slock, flags);

	ret = tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_STREAM_ON, NULL);
	if(ret && ret != -ENOIOCTLCMD){
		goto failed_streamon;
	}

	private_mutex_unlock(&ldc->mlock);
	return 0;
failed_streamon:
	if(irqdev->irq)
		irqdev->disable_irq(irqdev);
failed_qbuf:
	if(ldc->num_inbufs){
		tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_FREE_BUFFER, NULL);
		isp_free_buffer(addr);
	}
exit:
	private_mutex_unlock(&ldc->mlock);
	return ret;
}

static int ldc_frame_channel_streamoff(struct tx_isp_subdev_pad *pad)
{
	int ret = 0;
	struct tx_isp_ldc_device *ldc = pad->priv;
	struct tx_isp_subdev_pad *inpad = ldc->sd.inpads;
	struct tx_isp_irq_device *irqdev = &(ldc->sd.irqdev);
	unsigned long flags = 0;
	unsigned int loop = 200;
	int index = 0;

	if(pad->type != TX_ISP_PADTYPE_OUTPUT){
		return 0;
	}

	private_mutex_lock(&ldc->mlock);
	if(inpad->state != TX_ISP_PADSTATE_STREAM
			|| pad->state != TX_ISP_PADSTATE_STREAM){
		private_mutex_unlock(&ldc->mlock);
		return 0;
	}

	/* clear buffer of remote pad */
	tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_FREE_BUFFER, NULL);

	private_spin_lock_irqsave(&ldc->slock, flags);
	ldc->state = TX_ISP_MODULE_INIT;
	tx_isp_reg_set(&ldc->sd, LDC_CTR, 2, 2, 1); // stop ldc
	private_spin_unlock_irqrestore(&ldc->slock, flags);

	while((tx_isp_sd_readl((&ldc->sd), LDC_CTR) & LDC_CTRL_STOP_MASK) && loop--){
		msleep(2);
	}
#if 0
	do{
		ret = wait_for_completion_interruptible_timeout(&ldc->stop_comp, msecs_to_jiffies(500));
	}while(ret == -ERESTARTSYS);
#endif
	ret = tx_isp_send_event_to_remote(inpad, TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF, NULL);
	if(ret && ret != -ENOIOCTLCMD){
		goto failed_streamoff;
	}

	if(ldc->num_inbufs){
		isp_free_buffer(ldc->buf_addr);
		for(index = 0; index < ldc->num_inbufs; index++){
			INIT_LIST_HEAD(&(ldc->inbufs[index].entry));
			ldc->inbufs[index].addr = 0;
		}
		ldc->buf_addr = 0;
	}

	init_buffer_fifo(&ldc->outfifo);
	init_buffer_fifo(&ldc->infifo);
	tx_isp_reg_set(&ldc->sd, LDC_CTR, 3, 3, 0); // disable interrupt
	if(irqdev->irq)
		irqdev->disable_irq(irqdev);
	private_spin_lock_irqsave(&ldc->slock, flags);
	inpad->state = TX_ISP_PADSTATE_LINKED;
	pad->state = TX_ISP_PADSTATE_LINKED;
	private_spin_unlock_irqrestore(&ldc->slock, flags);
	private_mutex_unlock(&ldc->mlock);
	return 0;
failed_streamoff:
	private_mutex_unlock(&ldc->mlock);
	return ret;
}


static int ldc_pad_event_handle(struct tx_isp_subdev_pad *pad, unsigned int event, void *data)
{
	int ret = 0;

	if(pad->type == TX_ISP_MODULE_UNDEFINE)
		return 0;

	switch(event){
		case TX_ISP_EVENT_FRAME_CHAN_GET_FMT:
			ret = ldc_frame_channel_get_fmt(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_SET_FMT:
			ret = ldc_frame_channel_set_fmt(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_STREAM_ON:
			ret = ldc_frame_channel_streamon(pad);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF:
			ret = ldc_frame_channel_streamoff(pad);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER:
			ret = ldc_frame_channel_dqbuf(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER:
			ret = ldc_frame_channel_qbuf(pad, data);
			break;
		case TX_ISP_EVENT_FRAME_CHAN_FREE_BUFFER:
			ret = ldc_frame_channel_freebufs(pad, data);
			break;
		default:
			break;
	}
	return ret;
}


/* debug framesource info */
static int tx_isp_ldc_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_ldc_device *ldc = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct frame_channel_buffer *pos = NULL;
	unsigned long flags = 0;

	if(IS_ERR_OR_NULL(ldc)){
		ISP_ERROR("The parameter is invalid!\n");
		return 0;
	}

	len += seq_printf(m ,"############## %s is %s ###############\n", module->name,
					ldc->state == TX_ISP_MODULE_RUNNING ? "running" : "idle");
	len += seq_printf(m ,"The version is %s\n", ldc_params_version);
	if(ldc->state != TX_ISP_MODULE_RUNNING)
		return len;
	if(ldc_user_params == NULL)
		len += seq_printf(m ,"LDC is using default parameter!\n");
	private_spin_lock_irqsave(&ldc->slock, flags);
	tx_list_for_each_entry(pos, &ldc->infifo, entry){
		len += seq_printf(m ,"infifo addr: 0x%08x\n", pos->addr);
	}
	tx_list_for_each_entry(pos, &ldc->outfifo, entry){
		len += seq_printf(m ,"outfifo addr: 0x%08x\n", pos->addr);
	}
	len += seq_printf(m ,"current inbuf addr: 0x%08x\n", ldc->cur_inbuf ? ldc->cur_inbuf->addr : 0);
	len += seq_printf(m ,"current outbuf addr: 0x%08x\n", ldc->cur_outbuf ? ldc->cur_outbuf->addr : 0);
	len += seq_printf(m ,"start cnt = %lld done cnt = %lld\n", ldc->start_cnt, ldc->done_cnt);
	len += seq_printf(m ,"reset cnt = %d\n", ldc->reset_cnt);
	private_spin_unlock_irqrestore(&ldc->slock, flags);
	return len;
}

static int tx_isp_ldc_open(struct inode *inode, struct file *file)
{
	return private_single_open_size(file, tx_isp_ldc_show, PDE_DATA(inode), 2048);
}

static struct file_operations ldc_proc_fops ={
	.read = private_seq_read,
	.open = tx_isp_ldc_open,
	.llseek = private_seq_lseek,
	.release = private_single_release,
};


static int tx_isp_ldc_probe(struct platform_device *pdev)
{
	struct tx_isp_ldc_device *ldc_dev = NULL;
	struct tx_isp_subdev *sd = NULL;
	struct tx_isp_device_descriptor *desc = NULL;
	int ret = ISP_SUCCESS;
	int index = 0;

	ldc_dev = (struct tx_isp_ldc_device *)kzalloc(sizeof(*ldc_dev), GFP_KERNEL);
	if(!ldc_dev){
		ISP_ERROR("Failed to allocate sensor device\n");
		ret = -ENOMEM;
		goto exit;
	}

	desc = pdev->dev.platform_data;
	sd = &ldc_dev->sd;
	ret = tx_isp_subdev_init(pdev, sd, &ldc_subdev_ops);
	if(ret){
		ISP_ERROR("Failed to init isp module(%d.%d)\n", desc->parentid, desc->unitid);
		ret = -ENOMEM;
		goto failed_to_ispmodule;
	}

	ldc_dev->num_inbufs = isp_m1_bufs;
	if(ldc_dev->num_inbufs){
		ldc_dev->inbufs = kzalloc(sizeof(struct frame_channel_buffer)*ldc_dev->num_inbufs, GFP_KERNEL);
		if(ldc_dev->inbufs == NULL){
			ISP_ERROR("Can't alloc memory!\n");
			ret = -ENOMEM;
			goto failed_inbufs;
		}
		for(index = 0; index < ldc_dev->num_inbufs; index++){
			INIT_LIST_HEAD(&(ldc_dev->inbufs[index].entry));
			ldc_dev->inbufs[index].priv = (unsigned int)ldc_dev;
		}
	}
	init_buffer_fifo(&ldc_dev->outfifo);
	init_buffer_fifo(&ldc_dev->infifo);
	private_spin_lock_init(&ldc_dev->slock);
	private_mutex_init(&ldc_dev->mlock);
	ldc_dev->pdata = pdev->dev.platform_data;


	/* init input */
	if(sd->num_inpads){
		sd->inpads->event = ldc_pad_event_handle;
		sd->inpads->priv = ldc_dev;
	}else{
		ret = -EINVAL;
		goto failed_inpads;
	}

	/* init output */
	if(sd->num_outpads){
		sd->outpads->event = ldc_pad_event_handle;
		sd->outpads->priv = ldc_dev;
	}else{
		ret = -EINVAL;
		goto failed_outpads;
	}

	private_init_completion(&ldc_dev->stop_comp);
	ldc_dev->state = TX_ISP_MODULE_SLAKE;
	private_platform_set_drvdata(pdev, &sd->module);
	tx_isp_set_subdevdata(sd, ldc_dev);
	tx_isp_set_subdev_debugops(sd, &ldc_proc_fops);
	g_ldc = ldc_dev;

	return ISP_SUCCESS;
failed_outpads:
failed_inpads:
failed_inbufs:
	tx_isp_subdev_deinit(sd);
failed_to_ispmodule:
	kfree(ldc_dev);
exit:
	return ret;
}

static int __exit tx_isp_ldc_remove(struct platform_device *pdev)
{
	struct tx_isp_module *module = private_platform_get_drvdata(pdev);
	struct tx_isp_subdev *sd = module_to_subdev(module);
	struct tx_isp_ldc_device *ldc = tx_isp_get_subdevdata(sd);

	g_ldc = NULL;
	tx_isp_subdev_deinit(sd);
	kfree(ldc);
	return 0;
}

struct platform_driver tx_isp_ldc_driver = {
	.probe = tx_isp_ldc_probe,
	.remove = __exit_p(tx_isp_ldc_remove),
	.driver = {
		.name = TX_ISP_LDC_NAME,
		.owner = THIS_MODULE,
	},
};
