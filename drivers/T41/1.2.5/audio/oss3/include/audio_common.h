#ifndef _JZ_AUDIO_COMMON_H_
#define _JZ_AUDIO_COMMON_H_

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/time.h>
#ifdef CONFIG_KERNEL_3_10
#include <mach/jzdma.h>
#endif
enum audio_error_value {
	AUDIO_SUCCESS,
	AUDIO_EPARAM,
	AUDIO_EPERM,
};

enum audio_state {
	AUDIO_IDLE_STATE,
	AUDIO_OPEN_STATE,
	AUDIO_CONFIG_STATE,
	AUDIO_BUSY_STATE,
	AUDIO_MAX_STATE,
};

enum auido_route_index {
	AUDIO_ROUTE_AMIC_ID,
	AUDIO_ROUTE_DMIC_ID,
	AUDIO_ROUTE_SPK_ID,
	AUDIO_ROUTE_AEC_ID,
	AUDIO_ROUTE_MAX_ID,
};

enum audio_inter_cmd {
	AUDIO_CMD_CONFIG_PARAM = 1,
	AUDIO_CMD_ENABLE_STREAM,
	AUDIO_CMD_DISABLE_STREAM,
	AUDIO_CMD_SET_GAIN,
	AUDIO_CMD_GET_GAIN,
	AUDIO_CMD_SET_VOLUME,
	AUDIO_CMD_GET_VOLUME,
	AUDIO_CMD_SET_ALC_GAIN,
	AUDIO_CMD_GET_ALC_GAIN,
	AUDIO_CMD_SET_MUTE,
	AUDIO_CMD_ENABLE_ALC_L,
	AUDIO_CMD_ENABLE_ALC_R,
	AUDIO_CMD_SET_MIC_HPF_EN,
};


/* the size of one data fragment is 10ms */
struct dsp_data_fragment {
	struct list_head	list;
	bool 				state;
	void 				*vaddr;
	dma_addr_t          paddr;
	void *priv;			/* when enable aec function, it points aec fragment */
	struct timeval		time_stamp;
};

#define DSP_NEXT_FRAGMENT(x) ((x)==NULL ? NULL : container_of((x)->list.next, struct dsp_data_fragment, list))
#define CACHED_FRAGMENT 64

struct dsp_data_manage {
	/* buffer manager */
	struct dsp_data_fragment *fragments;
	unsigned int 		sample_size; // samplesize = channel * format_to_bytes(format) / 8;
	unsigned int 		fragment_size;
	unsigned int 		fragment_cnt;
	struct list_head    fragments_head;
	unsigned int		buffersize;			/* current using the total size of fragments data */
	unsigned int dma_tracer;				/* It's offset in buffer */
	unsigned int new_dma_tracer;			/* It's offset in buffer */
	unsigned int io_tracer;				/* It's offset in buffer */
	unsigned int aec_dma_tracer;			/* It's valid in amic and dmic */
};

struct audio_pipe {
	char *name;
	/* dma */
	struct dma_chan     *dma_chan;
	struct dma_slave_config dma_config;     /* define by device */
	int dma_type;

	/* dma buffer */
	void				*vaddr;
	dma_addr_t          paddr;
	unsigned int		reservesize; 		/* the size must be setted when initing this driver */
	spinlock_t pipe_lock;
	volatile bool is_trans;
	/* operation */
	int (*init)(void *pipe);
	int (*deinit)(void *pipe);
	int (*ioctl)(void *pipe, unsigned int cmd, void *data);
	void *priv;
};

struct volume {
	unsigned int channel;   //1-left 2-right
	unsigned int gain[2];      //0-left 1-right
};

struct alc_gain
{
	unsigned int channel;
	unsigned int max_gain[2];
	unsigned int min_gain[2];
};

#endif /* _JZ_AUDIO_COMMON_H_ */
