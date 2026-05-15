#ifndef __TX_ISP_MSCALER_H__
#define __TX_ISP_MSCALER_H__

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
/*#include <linux/seq_file.h>*/
/*#include <jz_proc.h>*/
#include <tx-isp-common.h>

enum isp_mscaler_output_id {
	ISP_MSCALER_OUTPUT_0,
	ISP_MSCALER_OUTPUT_1,
	ISP_MSCALER_OUTPUT_2,
};

struct isp_mscaler_output_channel {
	struct frame_image_format fmt;
	int index;
	int state;
	struct completion stop_comp;
	struct tx_isp_subdev_pad *pad;
	void *priv;

	unsigned int max_width;
	unsigned int max_height;
	unsigned int min_width;
	unsigned int min_height;
	bool	has_crop;
	bool	has_scaler;
	struct list_head fifo;
	spinlock_t slock;
	/*unsigned char bank_flag[ISP_DMA_WRITE_MAXBASE_NUM];*/
	/*unsigned char vflip_flag[ISP_DMA_WRITE_MAXBASE_NUM];*/
	unsigned int lineoffset;
	unsigned char dma_state;
	unsigned char reset_dma_flag;
	unsigned char vflip_state;
	unsigned char usingbanks;
	unsigned int frame_cnt;
};

struct isp_mscaler_input_channel {
	struct frame_image_format fmt;
	int index;
	int state;
	struct tx_isp_subdev_pad *pad;
	spinlock_t slock;
	void *priv;
};

struct tx_isp_mscaler_device {
	/* the common parameters */
	struct tx_isp_subdev sd;
	spinlock_t slock;
	struct mutex mlock;
	int state;
	struct tx_isp_video_in vin;
	void * pdata;

	struct isp_mscaler_output_channel *outputs;
	unsigned int num_outputs;
	int chan_rgb_flags;
	struct isp_mscaler_input_channel *inputs;
	unsigned int num_inputs;
};

#define sd_to_tx_isp_core_device(x) (container_of((x), struct tx_isp_core_device, sd))
int register_tx_isp_core_device(struct platform_device *pdev, struct v4l2_device *v4l2_dev);
void release_tx_isp_core_device(struct v4l2_subdev *sd);
#endif /* __TX_ISP_CORE_H__  */
