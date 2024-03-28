#ifndef __TX_ISP_NCU_H__
#define __TX_ISP_NCU_H__

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <tx-isp-common.h>
#include <tx-ncu-regs.h>

struct tx_isp_ncu_device {
	/* the common parameters */
	struct tx_isp_subdev sd;
	struct tx_isp_video_in vin;
	struct frame_image_format fmt;
	unsigned int uv_offset;
	int state;
	struct completion stop_comp;
	void * pdata;

	/* the special parameters of a instance */
	struct frame_channel_buffer *inbufs;
	unsigned int buf_addr;
	int num_inbufs;
	struct list_head infifo;
	struct frame_channel_buffer *current_inbuf;
	int ms_flag;

	unsigned int ref_frame_addr;
	spinlock_t slock;
	struct mutex mlock;

	/*debug parameters */
	unsigned long long start_cnt;
	unsigned long long done_cnt;
	unsigned int reset_cnt;

	/* the private parameters */
	struct task_struct *process_thread;
};


#endif /* __TX_ISP_NCU_H__  */
