#ifndef _JZ_AUDIO_DEVICE_H_
#define _JZ_AUDIO_DEVICE_H_

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <jz_proc.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/soundcard.h>
#include <asm/irq.h>
#include <asm/io.h>
#include "audio_common.h"
#include <linux/dma-mapping.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>

struct audio_ouput_stream {
	void __user * data;
	unsigned int size;
};

struct audio_input_stream {
	void __user *data;
	unsigned int size;
	void __user *aec;
	unsigned int aec_size;
#ifdef CONFIG_KERNEL_5_15
	struct __kernel_old_timeval *timestamp;
#else
	struct timeval *timestamp;
#endif
};

struct audio_parameter {
	unsigned int rate;
	unsigned short format;
	unsigned short channel;
};
#ifdef CONFIG_SOC_PRJ007
#define DMIC_AI_SET_PARAM			_SIOR ('P', 115, struct audio_parameter)
#define DMIC_AI_GET_PARAM			_SIOR ('P', 114, struct audio_parameter)
#endif
#define AMIC_AI_SET_PARAM			_SIOR ('P', 113, struct audio_parameter)
#define AMIC_AI_GET_PARAM			_SIOR ('P', 112, struct audio_parameter)
#define AMIC_AO_SET_PARAM			_SIOR ('P', 111, struct audio_parameter)
#define AMIC_AO_GET_PARAM			_SIOR ('P', 110, struct audio_parameter)
#ifdef CONFIG_SOC_PRJ007
#define DMIC_AI_DISABLE_STREAM		_SIOR ('P', 107, int)
#define DMIC_AI_ENABLE_STREAM		_SIOR ('P', 106, int)
#define DMIC_AI_GET_STREAM			_SIOR ('P', 105, struct audio_input_stream)
#define DMIC_ENABLE_AEC        		_SIOR ('P', 104, int)
#define DMIC_DISABLE_AEC        	_SIOR ('P', 103, int)
#define DMIC_SET_AI_VOLUME        	_SIOR ('P', 102, int)
#endif
#define AMIC_AO_SYNC_STREAM        	_SIOR ('P', 101, int)
#define AMIC_AO_CLEAR_STREAM        _SIOR ('P', 100, int)
#define AMIC_AO_SET_STREAM        	_SIOR ('P', 99, struct audio_ouput_stream)
#define AMIC_AI_GET_STREAM        	_SIOR ('P', 98, struct audio_input_stream)
#define AMIC_AI_DISABLE_STREAM		_SIOR ('P', 97, int)
#define AMIC_AI_ENABLE_STREAM		_SIOR ('P', 96, int)
#define AMIC_AO_DISABLE_STREAM		_SIOR ('P', 95, int)
#define AMIC_AO_ENABLE_STREAM		_SIOR ('P', 94, int)
#define AMIC_ENABLE_AEC        		_SIOR ('P', 93, int)
#define AMIC_DISABLE_AEC			_SIOR ('P', 92, int)
#define AMIC_AI_SET_VOLUME			_SIOR ('P', 91, struct volume)
#define AMIC_AI_SET_GAIN			_SIOR ('P', 90, struct volume)
#define AMIC_SPK_SET_VOLUME			_SIOR ('P', 89, struct volume)
#define AMIC_SPK_SET_GAIN			_SIOR ('P', 88, struct volume)
#ifdef CONFIG_SOC_PRJ007
#define DMIC_AI_SET_VOLUME			_SIOR ('P', 87, int)
#endif
#define AMIC_AI_GET_VOLUME			_SIOR ('P', 86, struct volume)
#define AMIC_AI_GET_GAIN			_SIOR ('P', 85, struct volume)
#define AMIC_SPK_GET_VOLUME			_SIOR ('P', 84, struct volume)
#define AMIC_SPK_GET_GAIN			_SIOR ('P', 83, struct volume)
#ifdef CONFIG_SOC_PRJ007
#define DMIC_AI_GET_VOLUME			_SIOR ('P', 82, int)
#endif
#define AMIC_AI_HPF_ENABLE	    	_SIOR ('P', 79, int)
#define AMIC_AI_SET_MUTE	    	_SIOR ('P', 78, int)
#define AMIC_SPK_SET_MUTE	    	_SIOR ('P', 77, int)
#define AMIC_AI_SET_ALC_GAIN	    _SIOR ('P', 76, struct alc_gain)
#define AMIC_AI_GET_ALC_GAIN	    _SIOR ('P', 75, struct alc_gain)
#define AMIC_AI_SET_CODEC_TYPE		_SIOR ('P', 74, int)

struct audio_route {
	enum auido_route_index index;
	enum audio_state state;
	struct mutex mlock;
	struct mutex stream_mlock;

	/* audio parameter */
	unsigned int rate;
	unsigned int channel;
	unsigned int format;
	unsigned int refcnt;

	/* manage fragments */
	struct dsp_data_manage manage;
	int aec_sample_offset;					/*When route is AMIC or DMIC, the value is sample offset in case AEC enable.
												0: ai and aec are synchronous;
												less than 0: the value is the count of invalid aec sample.
												greater than 0: the value is the count of invalid ai sample.
												*/
	unsigned int wait_cnt;
	bool wait_flag;
	struct completion done_completion;
	struct audio_pipe *pipe;
	void *parent;
	void *priv;
};

struct audio_dsp_device {
	char *version;
	enum audio_state state;
	unsigned int refcnt;
	struct platform_device *subdevs;

	struct miscdevice miscdev;
	struct mutex mlock;
	spinlock_t slock;

	/* task manage */
	struct hrtimer hr_timer;
	ktime_t expires;
	atomic_t	timer_stopped;
	struct work_struct workqueue;


	struct audio_route routes[AUDIO_ROUTE_MAX_ID];
	bool amic_aec;
#ifdef CONFIG_SOC_PRJ007
	bool dmic_aec;
#endif
	/* debug parameters */
	struct proc_dir_entry *proc;
	void *priv;
};

struct audio_dsp_debug {
	unsigned int io_tracer_debug;
	unsigned int dma_tracer_debug;
	unsigned int io_late_cnt;
	unsigned int frame_rate;
	ktime_t start_time;
	ktime_t end_time;
	unsigned long start_jiffies;
	unsigned int copy_to_user_calls;
	unsigned int copy_from_user_calls;
	s64 time_diff_ms;
	s64 time_debug;

};

#define misc_get_audiodsp(x) (container_of((x), struct audio_dsp_device, miscdev))

int register_audio_pipe(struct audio_pipe *pipe, enum auido_route_index index);
int release_audio_pipe(struct audio_pipe *pipe);
#ifdef CONFIG_KERNEL_5_15
int register_audio_debug_ops(char *name, struct proc_ops *debug_ops, void* data);
#else
int register_audio_debug_ops(char *name, struct file_operations *debug_ops, void* data);
#endif
#endif /* _JZ_AUDIO_DEVICE_H_ */
