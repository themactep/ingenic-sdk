#ifndef __TX_ISP_LDC_H__
#define __TX_ISP_LDC_H__

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <tx-isp-common.h>
#include <tx-ldc-regs.h>

#define TX_ISP_LDC_MIN_WIDTH 640
#define TX_ISP_LDC_MIN_HEIGHT 480
#define TX_ISP_LDC_MAX_WIDTH 2592
#define TX_ISP_LDC_MAX_HEIGHT 2048
#define TX_ISP_LDC_ALIGN_WIDTH 16

typedef struct _ldc_opt_ {
	uint32_t width;
	uint32_t height;
	uint32_t w_str;
	uint32_t r_str;
	uint32_t k1_x;
	uint32_t k2_x;
	uint32_t k1_y;
	uint32_t k2_y;
	uint32_t p1_val_x;
	uint32_t p1_val_y;
	uint32_t r2_rep;
	uint32_t wr_len;
	uint32_t wr_side_len;
	uint32_t rd_len;
	uint32_t rd_side_len;
	uint32_t y_fill;
	uint32_t u_fill;
	uint32_t v_fill;
	uint32_t view_mode;
	uint32_t udis_r;
	int16_t y_shift_lut[256];
	int16_t uv_shift_lut[256];
} tx_isp_ldc_opt;

struct ldc_params_header {
	char version[16];
	unsigned int flags;
	unsigned int crc;
};

struct tx_isp_ldc_param {
	unsigned int width;
	unsigned int height;
	tx_isp_ldc_opt *opt;
};

struct ldc_save_regs {
	unsigned int ctrl;
	unsigned int resolution;
	unsigned int coef_x;
	unsigned int coef_y;
	unsigned int udis_r;
	unsigned int len;
	unsigned int p1;
	unsigned int r2;
	unsigned int fill;
	unsigned int stride;
};

struct tx_isp_ldc_device {
	/* the common parameters */
	struct tx_isp_subdev sd;
	struct tx_isp_video_in vin;
	struct frame_image_format fmt;
	unsigned int uv_offset;
	tx_isp_ldc_opt *current_param;
	int state;
	struct completion stop_comp;
	int frame_state;
	void * pdata;
	struct ldc_save_regs regs;

	/* the special parameters of a instance */
	struct frame_channel_buffer *inbufs;
	unsigned int buf_addr;
	int num_inbufs;
	struct list_head infifo;
	struct frame_channel_buffer *cur_inbuf;

	struct list_head outfifo;
	struct frame_channel_buffer *cur_outbuf;

	spinlock_t slock;
	struct mutex mlock;

	/* debug parameters */
	unsigned long long start_cnt;
	unsigned long long done_cnt;
	unsigned int reset_cnt;

	/* ldc algorithm data */
	char *udata;

	/* the private parameters */
	struct task_struct *process_thread;
};


#endif /* __TX_ISP_NCU_H__  */
