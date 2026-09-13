#ifndef HDMI_DSP_H__
#define HDMI_DSP_H__
#include <linux/miscdevice.h>
#include <linux/soundcard.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <jz_proc.h>
#define MONO   1
#define STEREO 2

#define AIC_TX_FIFO_DEPTH 64
#define AIC_PROC_CMD_SIZE 32
/*Audio*/
/*************************************/
struct audio_parameter {
	unsigned int rate;
	unsigned short format;
	unsigned short channel;
};

struct audio_stream {
	void __user * data;
	unsigned int size;
};

struct volume {
	unsigned int channel;   //1-left 2-right 3-stereo
	unsigned int gain[2];   //0-left 1-right
};

struct audio_ouput_stream {
	void __user * data;
	unsigned int size;
	unsigned int block;
};

typedef enum {
	NOBLOCK = 0,
	BLOCK   = 1,
	QUERY   = 2
}DMA_ST;

//HDMI AO
#define AMIC_HDMI_AO_SYNC_STREAM         _SIOR ('P', 122, int32_t)
#define AMIC_HDMI_AO_CLEAR_STREAM        _SIOR ('P', 121, int32_t)
#define AMIC_HDMI_AO_DISABLE_STREAM      _SIOR ('P', 120, int32_t)
#define AMIC_HDMI_AO_ENABLE_STREAM       _SIOR ('P', 119, int32_t)
#define AMIC_HDMI_AO_SET_STREAM          _SIOR ('P', 118, struct audio_ouput_stream)
#define AMIC_HDMI_AO_SET_PARAM           _SIOR ('P', 117, struct audio_parameter)
#define AMIC_HDMI_AO_GET_PARAM           _SIOR ('P', 116, struct audio_parameter)

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

struct dsp_data_fragment {
	struct list_head    list;
	bool                state;
	void                *vaddr;
	dma_addr_t          paddr;
};

struct dsp_data_manage {
	/* buffer manager */
	struct dsp_data_fragment *fragments;
	unsigned int        sample_size; // samplesize = channel * format_to_bytes(format) / 8;
	unsigned int        fragment_size;
	unsigned int        fragment_cnt;
	struct list_head    fragments_head;
	unsigned int        buffersize;         /* current using the total size of fragments data */
	unsigned int        dma_tracer;                /* It's offset in buffer */
	unsigned int	    new_dma_tracer;            /* It's offset in buffer */
	unsigned int	    io_tracer;             /* It's offset in buffer */
};

struct audio_route {
	enum audio_state state;
	struct mutex stream_mlock;
	struct mutex mlock;
	/* audio parameter */
	unsigned int rate;
	unsigned int channel;
	unsigned int format;

	/* dma */
	volatile bool is_trans;
	struct dma_chan *dma_chan;
	struct dma_slave_config dma_config;     /* define by device */
	int dma_type;

	/* dma buffer */
	void                *vaddr;
	dma_addr_t          paddr;
	unsigned int        reservesize;        /* the size must be setted when initing this driver */
	unsigned int refcnt;

	/* manage fragments */
	struct dsp_data_manage manage;
	unsigned int wait_cnt;
	bool wait_flag;
	struct completion done_completion;

	/* debug parameters */
	struct file *proc_savefd;
	bool	save_debugdata;
};

struct hdmi_dsp_device {
	char *version;
	struct platform_device *pdev;
	struct miscdevice miscdev;
	enum audio_state state;
	struct mutex mlock;
	spinlock_t slock;

	int irq;
	unsigned int refcnt;
	struct resource *res;
	void __iomem *iomem;
	/*clock i2s1*/
	struct clk *spk_clk;
	struct clk *aic_clk;
	struct clk *ce_tclk;

	/* task manage */
	struct hrtimer hr_timer;
	ktime_t expires;
	atomic_t timer_stopped;
	struct work_struct workqueue;
	struct audio_route spk_route;

	/* debug parameters */
	struct proc_dir_entry *proc;

	void *priv;
};

#define misc_get_audiodsp(x) (container_of((x), struct hdmi_dsp_device, miscdev))

int hdmi_dsp_init(void);
void hdmi_dsp_exit(void);
int hdmi_dsp_set_param(struct audio_parameter *param);
int hdmi_dsp_enable_ao(void);
int hdmi_dsp_disable_ao(void);
int hdmi_dsp_sync_stream(void);
int hdmi_dsp_clear_stream(void);
int hdmi_dsp_set_stream(unsigned long arg);
#endif
