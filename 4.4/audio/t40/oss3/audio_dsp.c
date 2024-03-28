/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2020, <xianghui.shen@ingenic.com>
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "include/audio_dsp.h"
#include "include/audio_debug.h"

static int fragment_time = 2; // the unit is 10ms.
module_param(fragment_time, int, S_IRUGO);
MODULE_PARM_DESC(fragment_time, "The unit of the time of fragment is ms");

static int dmic_enable = 0; // disable dmic
module_param(dmic_enable, int, S_IRUGO);
MODULE_PARM_DESC(dmic_enable, "Enable or disable dmic");

static int aic_enable = 1; // enable aic
module_param(aic_enable, int, S_IRUGO);
MODULE_PARM_DESC(aic_enable, "Enable or disable aic");

#define AUDIO_IO_LEADING_DMA (2)

#define AUDIO_DRIVER_VERSION "H20210524a"
static struct audio_dsp_device* globe_dspdev = NULL;


static void dsp_workqueue_handle(struct work_struct *work)
{
	struct audio_dsp_device *dsp = container_of(work,
			struct audio_dsp_device, workqueue);
	struct audio_route *amic_route = NULL;
	struct audio_route *dmic_route = NULL;
	struct audio_route *aec_route = NULL;
	struct audio_route *ao_route = NULL;
	unsigned int amic_new_tracer = 0;
	unsigned int dmic_new_tracer = 0;
	unsigned int aec_new_tracer = 0;
	unsigned int ao_new_tracer = 0;
	unsigned int dma_tracer = 0;
	unsigned int aec_tracer = 0;
	unsigned int io_tracer = 0;
	unsigned long lock_flags;
	unsigned int io_late = 0;
	unsigned int cnt = 0;
	unsigned int index = 0;

	/* first: save new dma tracer */
	spin_lock_irqsave(&dsp->slock, lock_flags);

	/* amic */
	amic_route = &(dsp->routes[AUDIO_ROUTE_AMIC_ID]);
	if (amic_route && amic_route->state == AUDIO_BUSY_STATE) {
		amic_new_tracer = amic_route->manage.new_dma_tracer;
	}

	/* dmic */
	dmic_route = &(dsp->routes[AUDIO_ROUTE_DMIC_ID]);
	if (dmic_route && dmic_route->state == AUDIO_BUSY_STATE) {
		dmic_new_tracer = dmic_route->manage.new_dma_tracer;
	}

	/* aec */
	aec_route = &(dsp->routes[AUDIO_ROUTE_AEC_ID]);
	if (aec_route && aec_route->state == AUDIO_BUSY_STATE) {
		aec_new_tracer = aec_route->manage.new_dma_tracer;
	}

	/* ao */
	ao_route = &(dsp->routes[AUDIO_ROUTE_SPK_ID]);
	if (ao_route && ao_route->state == AUDIO_BUSY_STATE) {
		ao_new_tracer = ao_route->manage.new_dma_tracer;
	}
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	/* sync amic and amic's aec */
	if (amic_route && aec_route) {
		mutex_lock(&amic_route->mlock);
		if (amic_route->state == AUDIO_BUSY_STATE) {
			io_late = 0;
			dma_tracer = amic_route->manage.dma_tracer;
			aec_tracer = amic_route->manage.aec_dma_tracer;

			while (dma_tracer != amic_new_tracer && aec_tracer != aec_new_tracer) {
				amic_route->manage.fragments[dma_tracer].priv = &(aec_route->manage.fragments[aec_tracer]);
				amic_route->manage.fragments[dma_tracer].state = true;
				if (dma_tracer == amic_route->manage.io_tracer)
					io_late = 1;
				dma_tracer = (dma_tracer + 1) % amic_route->manage.fragment_cnt;
				aec_tracer = (aec_tracer + 1) % aec_route->manage.fragment_cnt;
			}
			amic_route->manage.dma_tracer = dma_tracer;
			amic_route->manage.aec_dma_tracer = aec_tracer;

			for (cnt = 1; cnt <= AUDIO_IO_LEADING_DMA; cnt++) {
				index = (amic_new_tracer + cnt) % amic_route->manage.fragment_cnt;
				if (amic_route->manage.fragments[index].state) {
					amic_route->manage.fragments[index].state = false;
					amic_route->manage.fragments[index].priv = NULL;
				}
				if (index == amic_route->manage.io_tracer)
					io_late = 1;
			}
			if (io_late) {
				amic_route->manage.io_tracer = (index + 1) % amic_route->manage.fragment_cnt;
			}
		}
		/* wait second copy data */
		if (amic_route->wait_flag) {
			cnt = 0;
			dma_tracer = amic_route->manage.dma_tracer;
			io_tracer = amic_route->manage.io_tracer;
			while (io_tracer != dma_tracer) {
				cnt++;
				io_tracer = (io_tracer + 1) % amic_route->manage.fragment_cnt;
			}
			if (cnt >= amic_route->wait_cnt) {
				amic_route->wait_flag = false;
				complete(&amic_route->done_completion);
			}
		}
		mutex_unlock(&amic_route->mlock);
	}

	/* sync dmic and dmic's aec */
	if (dmic_route->pipe) {
		mutex_lock(&dmic_route->mlock);
		if (dmic_route->state == AUDIO_BUSY_STATE) {
			io_late = 0;
			if (dsp->dmic_aec && aec_route) {
				/* dmic enable aec */
				dma_tracer = dmic_route->manage.dma_tracer;
				aec_tracer = dmic_route->manage.aec_dma_tracer;
				while (dma_tracer != dmic_new_tracer && aec_tracer != aec_new_tracer) {
					dmic_route->manage.fragments[dma_tracer].priv = &(aec_route->manage.fragments[aec_tracer]);
					dmic_route->manage.fragments[dma_tracer].state = true;
					if (dma_tracer == dmic_route->manage.io_tracer)
						io_late = 1;
					dma_tracer = (dma_tracer + 1) % dmic_route->manage.fragment_cnt;
					aec_tracer = (aec_tracer + 1) % aec_route->manage.fragment_cnt;
				}
				dmic_route->manage.dma_tracer = dma_tracer;
				dmic_route->manage.aec_dma_tracer = aec_tracer;
			} else {
				/* dmic disable aec */
				dma_tracer = dmic_route->manage.dma_tracer;
				while (dma_tracer != dmic_new_tracer) {
					dmic_route->manage.fragments[dma_tracer].priv = NULL;
					dmic_route->manage.fragments[dma_tracer].state = true;
					if (dma_tracer == dmic_route->manage.io_tracer)
						io_late = 1;
					dma_tracer = (dma_tracer + 1) % dmic_route->manage.fragment_cnt;
				}
				dmic_route->manage.dma_tracer = dma_tracer;
			}
			/* clear dma prepare-buffer and sync io_tracer */
			for (cnt = 1; cnt <= AUDIO_IO_LEADING_DMA; cnt++) {
				index = (dmic_new_tracer + cnt) % dmic_route->manage.fragment_cnt;
				if (dmic_route->manage.fragments[index].state) {
					dmic_route->manage.fragments[index].state = false;
					dmic_route->manage.fragments[index].priv = NULL;
				}
				if (index == dmic_route->manage.io_tracer)
					io_late = 1;
			}
			if (io_late) {
				dmic_route->manage.io_tracer = (index + 1) % dmic_route->manage.fragment_cnt;
			}
		}
		/* wait second copy data */
		if (dmic_route->wait_flag) {
			cnt = 0;
			dma_tracer = dmic_route->manage.dma_tracer;
			io_tracer = dmic_route->manage.io_tracer;
			while (io_tracer != dma_tracer) {
				cnt++;
				io_tracer = (io_tracer + 1) % dmic_route->manage.fragment_cnt;
			}
			if (cnt >= dmic_route->wait_cnt) {
				dmic_route->wait_flag = false;
				complete(&dmic_route->done_completion);
			}
		}
		mutex_unlock(&dmic_route->mlock);
	}

	/* ao */
	if (ao_route) {
		mutex_lock(&ao_route->mlock);
		if (ao_route->state == AUDIO_BUSY_STATE) {
			io_late = 0;
			dma_tracer = ao_route->manage.dma_tracer;

			if (ao_new_tracer < ao_route->manage.fragment_cnt && ao_new_tracer >= 0) {
				while (dma_tracer != ao_new_tracer) {
					if (dma_tracer == ao_route->manage.io_tracer) {
						printk("%d: audio spk io late!\n", __LINE__);
						io_late = 1;
					}
					memset(ao_route->manage.fragments[dma_tracer].vaddr, 0, ao_route->manage.fragment_size);
					dma_sync_single_for_device(NULL, ao_route->manage.fragments[dma_tracer].paddr,
							ao_route->manage.fragment_size, DMA_TO_DEVICE);
					ao_route->manage.fragments[dma_tracer].state = false;
					dma_tracer = (dma_tracer + 1) % ao_route->manage.fragment_cnt;
				}
				ao_route->manage.dma_tracer = dma_tracer;
				/* clear dma prepare-buffer and sync io_tracer */
				for (cnt = 0; cnt <= AUDIO_IO_LEADING_DMA; cnt++) {
					index = (ao_new_tracer + cnt) % ao_route->manage.fragment_cnt;
					if (ao_route->manage.fragments[index].state) {
						ao_route->manage.fragments[index].state = false;
					}
					if (index == ao_route->manage.io_tracer)
						io_late = 1;
				}
				if (io_late) {
					ao_route->manage.io_tracer = (index + 1) % ao_route->manage.fragment_cnt;
				}
			} else {
				printk("%d: audio spk dma transfer error!\n", __LINE__);
				memset(ao_route->manage.fragments[dma_tracer].vaddr, 0, ao_route->manage.fragment_size);
				dma_sync_single_for_device(NULL, ao_route->manage.fragments[dma_tracer].paddr,
						ao_route->manage.fragment_size, DMA_TO_DEVICE);
				memset(ao_route->manage.fragments[dma_tracer+1].vaddr, 0, ao_route->manage.fragment_size);
				dma_sync_single_for_device(NULL, ao_route->manage.fragments[dma_tracer+1].paddr,
						ao_route->manage.fragment_size, DMA_TO_DEVICE);
				dma_tracer = (dma_tracer + 2) % ao_route->manage.fragment_cnt;
				ao_route->manage.dma_tracer = dma_tracer;
			}
		}
		/* wait second copy space */
		if (ao_route->wait_flag) {
			cnt = 0;
			dma_tracer = ao_route->manage.dma_tracer;
			io_tracer = ao_route->manage.io_tracer;
			while (io_tracer != dma_tracer) {
				cnt++;
				io_tracer = (io_tracer + 1) % ao_route->manage.fragment_cnt;
			}
			if (cnt >= ao_route->wait_cnt) {
				ao_route->wait_flag = false;
				complete(&ao_route->done_completion);
			}
		}

		mutex_unlock(&ao_route->mlock);
	}

	/* aec */
	if (aec_route) {
		mutex_lock(&aec_route->mlock);
		aec_route->manage.dma_tracer = aec_new_tracer;
		aec_route->manage.io_tracer = aec_new_tracer;
		mutex_unlock(&aec_route->mlock);
	}

	return;
}

static enum hrtimer_restart jz_audio_hrtimer_callback(struct hrtimer *hr_timer) {
	struct audio_dsp_device *dsp = container_of(hr_timer, struct audio_dsp_device, hr_timer);
	struct audio_route *route = NULL;
	struct audio_pipe *pipe = NULL;
	dma_addr_t dma_currentaddr = 0;
	unsigned int index = 0;
	unsigned int id = 0;
	unsigned long lock_flags;

	if (atomic_read(&dsp->timer_stopped))
		goto out;
	hrtimer_start(&dsp->hr_timer, dsp->expires, HRTIMER_MODE_REL);

	spin_lock_irqsave(&dsp->slock, lock_flags);

	/* sync all routes dma */
	for (id = 0; id < AUDIO_ROUTE_MAX_ID; id++) {
		route = &(dsp->routes[id]);
		if (route && route->state == AUDIO_BUSY_STATE) {
			pipe = route->pipe;
			dma_currentaddr = pipe->dma_chan->device->get_current_trans_addr(pipe->dma_chan, NULL, NULL,
					pipe->dma_config.direction);
			index = (dma_currentaddr - pipe->paddr) / route->manage.fragment_size;
			route->manage.new_dma_tracer = index;
		}
	}

	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	schedule_work(&dsp->workqueue);
out:
	return HRTIMER_NORESTART;
}

static inline long dsp_ioctl_sync_ao_stream(struct audio_dsp_device *dsp)
{
	long ret = 0;
	unsigned long lock_flags;
	unsigned int new_dma_tracer = 0;
	unsigned int dma_tracer = 0;
	unsigned int io_tracer = 0;
	unsigned int wait_cnt = 0;
	unsigned int cnt_flag = 0;
	struct audio_route *route = dsp == NULL ? NULL : &(dsp->routes[AUDIO_ROUTE_SPK_ID]);

	if (!route) {
		audio_warn_print("%d; Can't support audio speaker!\n", __LINE__);
		return -EPERM;
	}

	mutex_lock(&route->mlock);
	if (route->state != AUDIO_BUSY_STATE) {
		audio_warn_print("%d; Please enable audio speaker firstly!\n", __LINE__);
		ret = -EPERM;
		goto out;
	}
	spin_lock_irqsave(&dsp->slock, lock_flags);
	new_dma_tracer = route->manage.new_dma_tracer;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
	dma_tracer = route->manage.dma_tracer;
	io_tracer = route->manage.io_tracer;
	while (dma_tracer != io_tracer) {
		if (dma_tracer == new_dma_tracer) {
			cnt_flag = 1;
		}
		if (cnt_flag)
			wait_cnt++;
		dma_tracer = (dma_tracer + 1) % route->manage.fragment_cnt;
	}
out:
	mutex_unlock(&route->mlock);
	if (wait_cnt) {
		msleep((wait_cnt + 1)*10*fragment_time);
	}
	return ret;
}

static inline long dsp_ioctl_clear_ao_stream(struct audio_dsp_device *dsp)
{
	long ret = 0;
	unsigned int dma_tracer = 0;
	unsigned int io_tracer = 0;
	struct audio_route *route = dsp == NULL ? NULL : &(dsp->routes[AUDIO_ROUTE_SPK_ID]);

	if (!route) {
		audio_warn_print("%d; Can't support audio speaker!\n", __LINE__);
		return -EPERM;
	}

	mutex_lock(&route->mlock);
	if (route->state != AUDIO_BUSY_STATE) {
		audio_warn_print("%d; Please enable audio speaker firstly!\n", __LINE__);
		ret = -EPERM;
		goto out;
	}
	dma_tracer = route->manage.dma_tracer;
	io_tracer = route->manage.io_tracer;
	while (dma_tracer != io_tracer) {
		memset(route->manage.fragments[dma_tracer].vaddr, 0, route->manage.fragment_size);
		dma_sync_single_for_device(NULL, route->manage.fragments[dma_tracer].paddr,
				route->manage.fragment_size, DMA_TO_DEVICE);
		dma_tracer = (dma_tracer + 1) % route->manage.fragment_cnt;
	}
out:
	mutex_unlock(&route->mlock);
	return ret;
}


static long dsp_config_route_param(struct audio_dsp_device *dsp, enum auido_route_index index,
						unsigned int cmd, struct audio_parameter *param)
{
	struct audio_route *route = NULL;
	long ret = AUDIO_SUCCESS;

	route = &(dsp->routes[index]);
	if (route->rate == param->rate && route->format == param->format
			&& route->channel == param->channel) {
		goto out;
	}

	/* the route is AUDIO_CONFIG_STATE or AUDIO_BUSY_STATE now.*/
	if (route->state >= AUDIO_CONFIG_STATE) {
		audio_warn_print("Can't modify audio parameters when audio is running! index = %d\n", index);
		ret = -EBUSY;
		goto out;
	}

	ret = route->pipe->ioctl(route->pipe, cmd, param);
	if (ret == AUDIO_SUCCESS) {
		route->rate = param->rate;
		route->format = param->format;
		route->channel = param->channel;
		route->state = AUDIO_CONFIG_STATE;
	}
out:
	return ret;
}

static long dsp_config_aec_route_param(struct audio_dsp_device *dsp, unsigned int cmd, struct audio_parameter *param)
{
	struct audio_route *route = NULL;
	long ret = AUDIO_SUCCESS;

	route = &(dsp->routes[AUDIO_ROUTE_AEC_ID]);
	if (route->rate == param->rate && route->format == param->format) {
		goto out;
	}

	/* the route is AUDIO_CONFIG_STATE or AUDIO_BUSY_STATE now.*/
	if (route->state >= AUDIO_CONFIG_STATE) {
		audio_warn_print("Can't modify audio parameters when audio is running! index = %d\n", AUDIO_ROUTE_AEC_ID);
		ret = -EBUSY;
		goto out;
	}

	/* AEC and AI must be have some parameters */
	ret = dsp_config_route_param(dsp, AUDIO_ROUTE_AMIC_ID, cmd, param);
	if (ret != AUDIO_SUCCESS) {
		audio_warn_print("Failed to sync parameters!\n");
		ret = -EPERM;
		goto out;
	}

	ret = route->pipe->ioctl(route->pipe, cmd, param);
	if (ret == AUDIO_SUCCESS) {
		route->rate = param->rate;
		route->format = param->format;
		route->channel = 1; // because AEC only support mono!
		route->state = AUDIO_CONFIG_STATE;
	}
out:
	return ret;
}

static inline unsigned int format_to_bytes(unsigned int format)
{
	if (format <= 8)
		return 1;
	else if (format <= 16)
		return 2;
	else
		return 4;
}

static long dsp_create_dma_chan(struct audio_route *route)
{
	struct dsp_data_manage *manage = NULL;
	struct audio_pipe *pipe = NULL;
	struct audio_route *parent = NULL;
	struct dma_async_tx_descriptor *desc;
	unsigned long flags = DMA_CTRL_ACK;
	int index = 0;
	long ret = AUDIO_SUCCESS;

	if (route->state == AUDIO_BUSY_STATE)
		goto out;
	/* init fragments manage and dma channel */
	manage = &route->manage;
	pipe = route->pipe;

	if (manage == NULL || pipe == NULL) {
		audio_err_print("%s, %d manage or pipe is NULL.\n", __func__, __LINE__);
		return AUDIO_EPERM;
	}

	/* create fragments manage */
	if (manage->fragments) {
		pr_kfree(manage->fragments);
		manage->fragment_cnt = 0;
		manage->fragment_size = 0;
	}
	INIT_LIST_HEAD(&manage->fragments_head);
	if (route->index == AUDIO_ROUTE_AEC_ID) {
		manage->sample_size = format_to_bytes(route->format);
	} else {
		manage->sample_size = route->channel*format_to_bytes(route->format);
	}
	manage->fragment_size = (route->rate / 100) * manage->sample_size * fragment_time;
	if (route->index == AUDIO_ROUTE_AEC_ID) {
		parent = route->parent;
		manage->fragment_cnt = parent->manage.fragment_cnt;
	} else
		manage->fragment_cnt = pipe->reservesize / manage->fragment_size;

	if (manage->fragment_cnt >= CACHED_FRAGMENT)
		manage->fragment_cnt = CACHED_FRAGMENT;

	manage->fragments = pr_kzalloc(sizeof(struct dsp_data_fragment) * manage->fragment_cnt); //20ms data volume
	if (manage->fragments == NULL) {
		audio_warn_print("%d, Can't malloc manage!\n",__LINE__);
		ret = -ENOMEM;
		goto out;
	}

	for (index = 0; index < manage->fragment_cnt; index++) {
		manage->fragments[index].vaddr = pipe->vaddr + manage->fragment_size * index;
		manage->fragments[index].paddr = pipe->paddr + manage->fragment_size * index;
		manage->fragments[index].priv = NULL;
		manage->fragments[index].state = false;
		list_add_tail(&manage->fragments[index].list, &manage->fragments_head);
	}
	manage->buffersize = manage->fragment_cnt * manage->fragment_size;
	memset(pipe->vaddr, 0, manage->buffersize);
	dma_sync_single_for_device(NULL, pipe->paddr, manage->buffersize, DMA_TO_DEVICE);

	dmaengine_slave_config(pipe->dma_chan, &pipe->dma_config);
#if (defined(CONFIG_SOC_T31) || defined(CONFIG_SOC_C100))
	desc = pipe->dma_chan->device->device_prep_dma_cyclic(pipe->dma_chan,
			pipe->paddr,
			manage->buffersize,
			manage->buffersize,
			pipe->dma_config.direction,
			flags,
			NULL);
#else
	desc = pipe->dma_chan->device->device_prep_dma_cyclic(pipe->dma_chan,
			pipe->paddr,
			manage->buffersize,
			manage->buffersize,
			pipe->dma_config.direction,
			flags);
#endif
	if (!desc) {
		dev_err(NULL, "cannot prepare slave dma\n");
		ret = -EINVAL;
		goto out;
	}
	dmaengine_submit(desc);

out:
	return ret;
}

static int dsp_destroy_dma_chan(struct audio_route *route)
{
	struct dsp_data_manage *manage = NULL;
	struct audio_pipe *pipe = NULL;
	long ret = AUDIO_SUCCESS;

	if (route->state != AUDIO_BUSY_STATE)
		goto out;
	/* deinit fragments manage and dma channel */
	manage = &route->manage;
	pipe = route->pipe;
	dmaengine_terminate_all(pipe->dma_chan);

	/* destroy fragments manage */
	if (manage->fragments) {
		pr_kfree(manage->fragments);
		manage->fragment_cnt = 0;
		manage->fragment_size = 0;
		INIT_LIST_HEAD(&manage->fragments_head);
	}
	manage->fragments = NULL;
out:
	return ret;
}

static long dsp_enable_amic_ai_and_aec(struct audio_dsp_device *dsp)
{
	unsigned long lock_flags;
	struct audio_route *ai_route = NULL;
	struct audio_route *aec_route = NULL;
	long ret = AUDIO_SUCCESS;

	ai_route = &(dsp->routes[AUDIO_ROUTE_AMIC_ID]);
	aec_route = &(dsp->routes[AUDIO_ROUTE_AEC_ID]);
	if (ai_route == NULL || aec_route == NULL) {
		audio_warn_print("The route of amic record hasn't been created!\n");
		ret = -EPERM;
		return ret;
	}

	if (ai_route->state == AUDIO_BUSY_STATE) {
		ai_route->refcnt++;
		aec_route->refcnt++;
		return AUDIO_SUCCESS;
	}

	/* config the dma channels of  ai and aec */
	ret = dsp_create_dma_chan(ai_route);
	if (ret != AUDIO_SUCCESS) {
		goto out;
	}

	ret = dsp_create_dma_chan(aec_route);
	if (ret != AUDIO_SUCCESS) {
		goto out_aec;
	}

	/* enable dma chan */
	dma_async_issue_pending(ai_route->pipe->dma_chan);
	dma_async_issue_pending(aec_route->pipe->dma_chan);

	/* enable hardware */
	ret = ai_route->pipe->ioctl(ai_route->pipe, AUDIO_CMD_ENABLE_STREAM, NULL);//here delay very long,be careful
	if (ret != AUDIO_SUCCESS) {
		goto out_cmd;
	}
	ret = aec_route->pipe->ioctl(aec_route->pipe, AUDIO_CMD_ENABLE_STREAM, NULL);
	if (ret != AUDIO_SUCCESS) {
		goto out_cmd;
	}
	mutex_unlock(&ai_route->mlock);
	spin_lock_irqsave(&dsp->slock, lock_flags);
	ai_route->state = AUDIO_BUSY_STATE;
	aec_route->state = AUDIO_BUSY_STATE;
	ai_route->aec_sample_offset = 0;
	dsp->amic_aec = true;
	ai_route->manage.dma_tracer = 0;
	//ai_route->manage.io_tracer = ai_route->manage.fragment_cnt - 1 - AUDIO_IO_LEADING_DMA;
	ai_route->manage.io_tracer = ai_route->manage.fragment_cnt - AUDIO_IO_LEADING_DMA;
	ai_route->manage.new_dma_tracer = 0;
	aec_route->manage.dma_tracer = 0;
	//aec_route->manage.io_tracer = aec_route->manage.fragment_cnt - 1 - AUDIO_IO_LEADING_DMA;
	aec_route->manage.io_tracer = aec_route->manage.fragment_cnt  - AUDIO_IO_LEADING_DMA;
	aec_route->manage.new_dma_tracer = 0;
	ai_route->manage.aec_dma_tracer = 0;
	ai_route->refcnt++;
	aec_route->refcnt++;
	init_completion(&(ai_route->done_completion));
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
	mutex_unlock(&ai_route->mlock);
	return ret;
out_cmd:
	dsp_destroy_dma_chan(aec_route);
out_aec:
	dsp_destroy_dma_chan(ai_route);
out:
	spin_lock_irqsave(&dsp->slock, lock_flags);
	ai_route->state = AUDIO_OPEN_STATE;
	aec_route->state = AUDIO_OPEN_STATE;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	mutex_unlock(&ai_route->mlock);
	return ret;
}

static long dsp_disable_amic_ai_and_aec(struct audio_dsp_device *dsp)
{
	unsigned long lock_flags;
	struct audio_route *ai_route = NULL;
	struct audio_route *aec_route = NULL;
	long ret = AUDIO_SUCCESS;

	ai_route = &(dsp->routes[AUDIO_ROUTE_AMIC_ID]);
	aec_route = &(dsp->routes[AUDIO_ROUTE_AEC_ID]);
	if (ai_route == NULL || aec_route == NULL) {
		audio_warn_print("%d; The route of amic record hasn't been created!\n",__LINE__);
		ret = -EPERM;
		goto exit;
	}

	mutex_lock(&ai_route->mlock);
	if (ai_route->refcnt == 0)
		goto out;
	ai_route->refcnt--;
	aec_route->refcnt--;
	if (ai_route->state != AUDIO_BUSY_STATE || ai_route->refcnt) {
		mutex_unlock(&ai_route->mlock);
		return AUDIO_SUCCESS;
	}

	/* disable hardware */
	ret = ai_route->pipe->ioctl(ai_route->pipe, AUDIO_CMD_DISABLE_STREAM, NULL);
	if (ret != AUDIO_SUCCESS) {
		goto out_cmd;
	}
	ret = aec_route->pipe->ioctl(aec_route->pipe, AUDIO_CMD_DISABLE_STREAM, NULL);
	if (ret != AUDIO_SUCCESS) {
		goto out_cmd;
	}

	/* destroy the dma channels of  ai and aec */
	ret = dsp_destroy_dma_chan(ai_route);
	ret = dsp_destroy_dma_chan(aec_route);

	spin_lock_irqsave(&dsp->slock, lock_flags);
	ai_route->state = AUDIO_OPEN_STATE;
	aec_route->state = AUDIO_OPEN_STATE;
	ai_route->aec_sample_offset = 0;
	dsp->amic_aec = false;
	ai_route->manage.dma_tracer = 0;
	ai_route->manage.io_tracer = 0;
	aec_route->manage.dma_tracer = 0;
	aec_route->manage.io_tracer = 0;
	ai_route->manage.aec_dma_tracer = 0;
	ai_route->refcnt = 0;
	aec_route->refcnt = 0;
	ai_route->rate = 0;
	aec_route->rate = 0;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);


	if (ai_route->wait_flag) {
		ai_route->wait_flag = false;
		complete(&ai_route->done_completion);
	}

	mutex_unlock(&ai_route->mlock);
	return ret;
out_cmd:
	audio_err_print("%d; Failed to disable hardware!\n",__LINE__);
	spin_lock_irqsave(&dsp->slock, lock_flags);
	ai_route->state = AUDIO_OPEN_STATE;
	aec_route->state = AUDIO_OPEN_STATE;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
out:
	mutex_unlock(&ai_route->mlock);
exit:
	return ret;
}

static long dsp_enable_dmic_ai(struct audio_dsp_device *dsp)
{
	unsigned long lock_flags;
	struct audio_route *ai_route = NULL;
	long ret = AUDIO_SUCCESS;

	ai_route = &(dsp->routes[AUDIO_ROUTE_DMIC_ID]);
	if (ai_route == NULL) {
		audio_warn_print("The route of dmic record hasn't been created!\n");
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&ai_route->mlock);
	if (ai_route->state == AUDIO_BUSY_STATE) {
		ai_route->refcnt++;
		mutex_unlock(&ai_route->mlock);
		return AUDIO_SUCCESS;
	}
	/* config the dma channels of  ai and aec */
	ret = dsp_create_dma_chan(ai_route);
	if (ret != AUDIO_SUCCESS) {
		goto out;
	}

	/* enable dma chan */
	dma_async_issue_pending(ai_route->pipe->dma_chan);

	/* enable hardware */
	ret = ai_route->pipe->ioctl(ai_route->pipe, AUDIO_CMD_ENABLE_STREAM, NULL);
	if (ret != AUDIO_SUCCESS) {
		goto out_cmd;
	}

	spin_lock_irqsave(&dsp->slock, lock_flags);
	ai_route->state = AUDIO_BUSY_STATE;
	ai_route->manage.dma_tracer = 0;
	ai_route->manage.new_dma_tracer = 0;
	//ai_route->manage.io_tracer = ai_route->manage.fragment_cnt - 1 - AUDIO_IO_LEADING_DMA;
	ai_route->manage.io_tracer = ai_route->manage.fragment_cnt  - AUDIO_IO_LEADING_DMA;
	ai_route->refcnt++;
	init_completion(&(ai_route->done_completion));
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	mutex_unlock(&ai_route->mlock);
	return ret;
out_cmd:
	dsp_destroy_dma_chan(ai_route);
out:
	spin_lock_irqsave(&dsp->slock, lock_flags);
	ai_route->state = AUDIO_OPEN_STATE;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
	mutex_unlock(&ai_route->mlock);
	return ret;
}

static long dsp_enable_dmic_aec(struct audio_dsp_device *dsp, unsigned long arg)
{
	unsigned long lock_flags;
	struct audio_route *ai_route = NULL;
	struct audio_route *aec_route = NULL;
	struct audio_pipe *ai_pipe = NULL;
	struct audio_pipe *aec_pipe = NULL;
	dma_addr_t ai_currentaddr = 0;
	dma_addr_t aec_currentaddr = 0;
	int ai_offset = 0, aec_offset = 0;
	long ret = AUDIO_SUCCESS;
	int dsp_amic_aec_status = 0;

	ai_route = &(dsp->routes[AUDIO_ROUTE_DMIC_ID]);
	if (ai_route == NULL) {
		audio_warn_print("The route of dmic record hasn't been created!\n");
		ret = -EPERM;
		return ret;
	}
	dsp_amic_aec_status = dsp->amic_aec;

	mutex_lock(&ai_route->mlock);
	if (ai_route->state != AUDIO_BUSY_STATE) {
		audio_warn_print("please enable dmic firstly!\n");
		ret = -EPERM;
		goto out;
	}

	ai_pipe = ai_route->pipe;
	aec_route = &(dsp->routes[AUDIO_ROUTE_AEC_ID]);

	{
		/* config aec route */
		struct audio_parameter param;
		param.rate = ai_route->rate;
		param.format = ai_route->format;
		param.channel = 1;	// default value
		mutex_lock(&dsp->mlock);
		ret = dsp_config_aec_route_param(dsp, AUDIO_CMD_CONFIG_PARAM, &param);
		if (ret != AUDIO_SUCCESS) {
			audio_warn_print("%d; Failed to config aec parameter!\n",__LINE__);
			audio_warn_print("%d; The parameters are invalid, DMIC(rate = %d, format = %d) AEC(rate = %d, format = %d)!\n",
					__LINE__, ai_route->rate, ai_route->format, aec_route->rate, aec_route->format);
			ret = -EPERM;
			mutex_unlock(&dsp->mlock);
			goto out;
		}
		mutex_unlock(&dsp->mlock);
		ret = dsp_enable_amic_ai_and_aec(dsp);
		if (ret != AUDIO_SUCCESS) {
			audio_warn_print("%d; Failed to enable aec route!\n",__LINE__);
			ret = -EPERM;
			goto out_failed;
		}
	}

	aec_pipe = aec_route->pipe;
	spin_lock_irqsave(&dsp->slock, lock_flags);
	dsp->dmic_aec = true;
	/* sync dma_tracer and aec_dma_tracer */
	ai_currentaddr = ai_pipe->dma_chan->device->get_current_trans_addr(ai_pipe->dma_chan, NULL, NULL,
				ai_pipe->dma_config.direction);
	aec_currentaddr = aec_pipe->dma_chan->device->get_current_trans_addr(aec_pipe->dma_chan, NULL, NULL,
				aec_pipe->dma_config.direction);
#if 0
	ai_route->manage.dma_tracer = (ai_currentaddr - ai_pipe->paddr) / ai_route->manage.fragment_size;
	ai_route->manage.aec_dma_tracer = (aec_currentaddr - aec_pipe->paddr) / aec_route->manage.fragment_size;
	/* calculate sample offset between dmic and aec */
	ai_offset = (ai_currentaddr - ai_pipe->paddr) % ai_route->manage.fragment_size;
	aec_offset = (aec_currentaddr - aec_pipe->paddr) % aec_route->manage.fragment_size;
	//ai_route->manage.io_tracer = ((ai_route->manage.dma_tracer+ai_route->manage.fragment_cnt-1)%ai_route->manage.fragment_cnt);   //still right
#else
	ai_offset = (ai_currentaddr - ai_pipe->paddr)/ai_route->manage.fragment_size;  //dmic offset
	aec_offset = (aec_currentaddr - aec_pipe->paddr)/aec_route->manage.fragment_size;  //aec offset
	/* now is dmic first open and aec last open */
	if (dsp_amic_aec_status == false) {
		ai_route->manage.dma_tracer = (ai_offset-aec_offset)>=0?(ai_offset-aec_offset):(ai_route->manage.fragment_cnt-1-(aec_offset-ai_offset)+ai_offset);
		ai_route->manage.aec_dma_tracer = 0;
		ai_route->manage.io_tracer = ((ai_route->manage.io_tracer+ai_route->manage.dma_tracer)%(ai_route->manage.fragment_cnt-1)) - 1;   //maybe error
		//ai_route->manage.io_tracer = ((ai_route->manage.dma_tracer+ai_route->manage.fragment_cnt-1)%ai_route->manage.fragment_cnt);   //still right
	} else {
	/* now is aec first open and dmic last open */
		ai_route->manage.dma_tracer = 0;
		ai_route->manage.aec_dma_tracer = (aec_offset-ai_offset)>=0?(aec_offset-ai_offset):(ai_route->manage.fragment_cnt-1+aec_offset-(ai_offset-aec_offset));
		ai_route->manage.io_tracer = ai_route->manage.fragment_cnt - 1 - AUDIO_IO_LEADING_DMA;
	}

	/* calculate sample offset between dmic and aec */
	ai_offset = (ai_currentaddr - ai_pipe->paddr) % ai_route->manage.fragment_size;
	aec_offset = (aec_currentaddr - aec_pipe->paddr) % aec_route->manage.fragment_size;
#endif
	ai_route->aec_sample_offset = (ai_offset / ai_route->manage.sample_size) - (aec_offset / aec_route->manage.sample_size);
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	if (arg)
		copy_to_user((__user void*)arg, &ai_route->aec_sample_offset, sizeof(ai_route->aec_sample_offset));
	mutex_unlock(&ai_route->mlock);
	return ret;
out_failed:
	dsp_config_aec_route_param(dsp, AUDIO_CMD_CONFIG_PARAM, NULL);
out:
	mutex_unlock(&ai_route->mlock);
	return ret;
}

static long dsp_enable_amic_ao(struct audio_dsp_device *dsp)
{
	unsigned long lock_flags;
	struct audio_route *ao_route = NULL;
	long ret = AUDIO_SUCCESS;

	ao_route = &(dsp->routes[AUDIO_ROUTE_SPK_ID]);
	if (ao_route == NULL) {
		audio_warn_print("The route of amic speaker hasn't been created!\n");
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&ao_route->mlock);
	if (AUDIO_BUSY_STATE == ao_route->state) {
		ao_route->refcnt++;
		mutex_unlock(&ao_route->mlock);
		audio_warn_print("The route of amic speaker is busy now!\n");
		return AUDIO_SUCCESS;
	}

	/* config the dma channels of  ai and aec */
	ret = dsp_create_dma_chan(ao_route);
	if (ret != AUDIO_SUCCESS) {
		printk("config ao dma channel error.\n");
		goto out;
	}

	/* enable hardware */
	ret = ao_route->pipe->ioctl(ao_route->pipe, AUDIO_CMD_ENABLE_STREAM, NULL);
	if (ret != AUDIO_SUCCESS) {
		printk("IOCTL Enable ao stream error.\n");
		goto out_cmd;
	}

	/* enable dma chan */
	dma_async_issue_pending(ao_route->pipe->dma_chan);

	spin_lock_irqsave(&dsp->slock, lock_flags);
	ao_route->state = AUDIO_BUSY_STATE;
	ao_route->manage.dma_tracer = 0;
	ao_route->manage.new_dma_tracer = 0;
	ao_route->manage.io_tracer = AUDIO_IO_LEADING_DMA;
	ao_route->refcnt++;
	init_completion(&(ao_route->done_completion));
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
	mutex_unlock(&ao_route->mlock);
	return ret;
out_cmd:
	dsp_destroy_dma_chan(ao_route);
out:
	spin_lock_irqsave(&dsp->slock, lock_flags);
	ao_route->state = AUDIO_OPEN_STATE;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
	mutex_unlock(&ao_route->mlock);
	return ret;
}

static long dsp_disable_amic_ao(struct audio_dsp_device *dsp)
{
	unsigned long lock_flags;
	struct audio_route *ao_route = NULL;
	long ret = AUDIO_SUCCESS;

	ao_route = &(dsp->routes[AUDIO_ROUTE_SPK_ID]);
	if (ao_route == NULL) {
		audio_warn_print("%d; The route of spk hasn't been created!\n",__LINE__);
		ret = -EPERM;
		goto exit;
	}

	mutex_lock(&ao_route->mlock);
	if (ao_route->refcnt == 0)
		goto out;
	ao_route->refcnt--;
	if (ao_route->state != AUDIO_BUSY_STATE || ao_route->refcnt) {
		mutex_unlock(&ao_route->mlock);
		return AUDIO_SUCCESS;
	}

	/* disable hardware */
	ret = ao_route->pipe->ioctl(ao_route->pipe, AUDIO_CMD_DISABLE_STREAM, NULL);
	if (ret != AUDIO_SUCCESS) {
		goto out_cmd;
	}

	/* destroy the dma channels of  ai and aec */
	ret = dsp_destroy_dma_chan(ao_route);

	spin_lock_irqsave(&dsp->slock, lock_flags);
	ao_route->state = AUDIO_OPEN_STATE;
	ao_route->manage.dma_tracer = 0;
	ao_route->manage.io_tracer = 0;
	ao_route->manage.aec_dma_tracer = 0;
	ao_route->refcnt = 0;
	ao_route->rate = 0;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	if (ao_route->wait_flag) {
		ao_route->wait_flag = false;
		complete(&ao_route->done_completion);
	}
	mutex_unlock(&ao_route->mlock);
	return ret;
out_cmd:
	audio_err_print("%d; Failed to disable hardware!\n",__LINE__);
	spin_lock_irqsave(&dsp->slock, lock_flags);
	ao_route->state = AUDIO_OPEN_STATE;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
out:
	mutex_unlock(&ao_route->mlock);
exit:
	return ret;
}


static long dsp_enable_amic_aec(struct audio_dsp_device *dsp, unsigned long arg)
{
	unsigned long lock_flags;
	struct audio_route *ai_route = NULL;
	long ret = AUDIO_SUCCESS;

	ai_route = &(dsp->routes[AUDIO_ROUTE_AMIC_ID]);
	if (ai_route == NULL) {
		audio_warn_print("The route of amic record hasn't been created!\n");
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&ai_route->mlock);
	if (ai_route->state != AUDIO_BUSY_STATE) {
		goto out;
	}

	spin_lock_irqsave(&dsp->slock, lock_flags);
	dsp->amic_aec = true;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
	if (arg)
		copy_to_user((__user void*)arg, &ai_route->aec_sample_offset, sizeof(ai_route->aec_sample_offset));
out:
	mutex_unlock(&ai_route->mlock);
	return ret;
}

static long dsp_disable_amic_aec(struct audio_dsp_device *dsp)
{
	unsigned long lock_flags;
	struct audio_route *ai_route = NULL;
	long ret = AUDIO_SUCCESS;

	if (dsp->amic_aec == false)
		return ret;

	ai_route = &(dsp->routes[AUDIO_ROUTE_AMIC_ID]);
	if (ai_route == NULL) {
		audio_warn_print("The route of amic hasn't been created!\n");
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&ai_route->mlock);
	if (ai_route->state != AUDIO_BUSY_STATE) {
		goto out;
	}

	spin_lock_irqsave(&dsp->slock, lock_flags);
	dsp->amic_aec = false;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
out:
	mutex_unlock(&ai_route->mlock);
	return ret;
}

static long dsp_disable_dmic_aec(struct audio_dsp_device *dsp)
{
	unsigned long lock_flags;
	struct audio_route *ai_route = NULL;
	long ret = AUDIO_SUCCESS;

	if (dsp->dmic_aec == false)
		return ret;

	ai_route = &(dsp->routes[AUDIO_ROUTE_DMIC_ID]);
	if (ai_route == NULL) {
		audio_warn_print("The route of dmic record hasn't been created!\n");
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&ai_route->mlock);
	if (ai_route->state != AUDIO_BUSY_STATE) {
		goto out;
	}

	if (dsp->dmic_aec)
		dsp_disable_amic_ai_and_aec(dsp);

	spin_lock_irqsave(&dsp->slock, lock_flags);
	dsp->dmic_aec = false;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

out:
	mutex_unlock(&ai_route->mlock);
	return ret;
}

static long dsp_disable_dmic_ai(struct audio_dsp_device *dsp)
{
	unsigned long lock_flags;
	struct audio_route *ai_route = NULL;
	long ret = AUDIO_SUCCESS;

	ai_route = &(dsp->routes[AUDIO_ROUTE_DMIC_ID]);
	if (ai_route == NULL) {
		audio_warn_print("%d; The route of dmic record hasn't been created!\n",__LINE__);
		ret = -EPERM;
		goto exit;
	}

	mutex_lock(&ai_route->mlock);
	if (ai_route->refcnt == 0)
		goto out;
	ai_route->refcnt--;
	if (ai_route->state != AUDIO_BUSY_STATE || ai_route->refcnt) {
		mutex_unlock(&ai_route->mlock);
		return AUDIO_SUCCESS;
	}

	/* disable hardware */
	ret = ai_route->pipe->ioctl(ai_route->pipe, AUDIO_CMD_DISABLE_STREAM, NULL);
	if (ret != AUDIO_SUCCESS) {
		goto out_cmd;
	}

	/* destroy the dma channels of  ai and aec */
	ret = dsp_destroy_dma_chan(ai_route);

	if (dsp->dmic_aec)
		dsp_disable_amic_ai_and_aec(dsp);

	spin_lock_irqsave(&dsp->slock, lock_flags);
	ai_route->state = AUDIO_OPEN_STATE;
	ai_route->aec_sample_offset = 0;
	dsp->dmic_aec = false;
	ai_route->manage.dma_tracer = 0;
	ai_route->manage.io_tracer = 0;
	ai_route->manage.aec_dma_tracer = 0;
	ai_route->refcnt = 0;
	ai_route->rate = 0;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	if (ai_route->wait_flag) {
		ai_route->wait_flag = false;
		complete(&ai_route->done_completion);
	}
	mutex_unlock(&ai_route->mlock);
	return ret;
out_cmd:
	audio_err_print("%d; Failed to disable hardware!\n",__LINE__);
	spin_lock_irqsave(&dsp->slock, lock_flags);
	ai_route->state = AUDIO_OPEN_STATE;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
out:
	mutex_unlock(&ai_route->mlock);
exit:
	return ret;
}

static long dsp_get_mic_stream(struct audio_dsp_device *dsp, enum auido_route_index index, unsigned long arg)
{
	struct audio_route *ai_route = NULL;
	struct audio_route *aec_route = NULL;
	int cnt = 0, aec_cnt = 0, i = 0;
	unsigned int dma_tracer = 0;
	unsigned int io_tracer = 0;
	struct audio_input_stream stream;
	struct dsp_data_manage *manage = NULL;
	struct dsp_data_fragment *fragment = NULL;
	struct dsp_data_fragment *aec_fragment = NULL;
	unsigned long time = 0;
	long ret = AUDIO_SUCCESS;

	ai_route = &(dsp->routes[index]);
	aec_route = &(dsp->routes[AUDIO_ROUTE_AEC_ID]);
	if (ai_route == NULL || aec_route == NULL) {
		audio_warn_print("%d;The route of amic record hasn't been created!\n", __LINE__);
		ret = -EPERM;
		return ret;
	}

	if (IS_ERR_OR_NULL((void __user *)arg)) {
		audio_warn_print("%d; the parameter is invalid!\n", __LINE__);
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&ai_route->stream_mlock);
	mutex_lock(&ai_route->mlock);
	if (ai_route->state != AUDIO_BUSY_STATE) {
		audio_warn_print("%d:please enable amic firstly!\n",__LINE__);
		ret = -EPERM;
		goto out;
	}

	ret = copy_from_user(&stream, (__user void*)arg, sizeof(stream));
	if (ret) {
		audio_warn_print("%d: failed to copy_from_user!\n", __LINE__);
		ret = -EIO;
		goto out;
	}

	if (IS_ERR_OR_NULL(stream.data) || stream.size == 0) {
		audio_warn_print("%d; the parameter is invalid!\n", __LINE__);
		ret = -EPERM;
		goto out;
	}
	manage = &(ai_route->manage);
	cnt = stream.size / manage->fragment_size;
	if (dsp->amic_aec && (stream.aec != NULL)) {
		aec_cnt = stream.aec_size / aec_route->manage.fragment_size;
		if (cnt != aec_cnt) {
			audio_warn_print("%d; the parameter is invalid! cnt = %d, aec_cnt = %d, stream.aec = %p\n",
					__LINE__, cnt, aec_cnt, stream.aec);
			ret = -EPERM;
			goto out;
		}
	}
again:
	if (ai_route->state != AUDIO_BUSY_STATE)
		goto out;

	dma_tracer = manage->dma_tracer;
	io_tracer = manage->io_tracer;
	/* first copy */
	while (i < cnt) {
		if (io_tracer+1 == dma_tracer || (dma_tracer==0 && io_tracer==ai_route->manage.fragment_cnt-1))
			break;
		fragment = &(manage->fragments[io_tracer]);
		if (fragment->state) {
			dma_sync_single_for_device(NULL, fragment->paddr, manage->fragment_size, DMA_FROM_DEVICE);
			copy_to_user((stream.data + i * manage->fragment_size), fragment->vaddr, manage->fragment_size);
			/* copy aec data */
			if (dsp->amic_aec && (stream.aec != NULL)) {
				aec_fragment = fragment->priv;
				if (aec_fragment) {
					dma_sync_single_for_device(NULL, aec_fragment->paddr, aec_route->manage.fragment_size, DMA_FROM_DEVICE);
					copy_to_user((stream.aec + i * aec_route->manage.fragment_size), aec_fragment->vaddr, aec_route->manage.fragment_size);
				} else
					memset((stream.aec + i * aec_route->manage.fragment_size), 0, aec_route->manage.fragment_size);
			}
			fragment->state = false;
		} else {
			memset((stream.data + i * manage->fragment_size), 0, manage->fragment_size);
			/* copy aec data */
			if (dsp->amic_aec && (stream.aec != NULL))
				memset((stream.aec + i * aec_route->manage.fragment_size), 0, aec_route->manage.fragment_size);
		}
		fragment->priv = NULL;
		i++;
		io_tracer = (io_tracer + 1) % manage->fragment_cnt;
	}
	manage->io_tracer = io_tracer;
	/* second copy */
	if (i < cnt) {
		ai_route->wait_flag = true;
		ai_route->wait_cnt = cnt - i - 1;
		mutex_unlock(&ai_route->mlock);
		time = wait_for_completion_timeout(&ai_route->done_completion, msecs_to_jiffies(800));
		if (!time) {
			audio_err_print("get mic timeout!\n");
			ret = -ETIMEDOUT;
			goto exit;
		}
		mutex_lock(&ai_route->mlock);
		goto again;
	}
out:
	mutex_unlock(&ai_route->mlock);
exit:
	mutex_unlock(&ai_route->stream_mlock);
	return ret;
}

static long dsp_set_spk_stream(struct audio_dsp_device *dsp, unsigned long arg)
{
	struct audio_route *ao_route = NULL;
	int cnt = 0, i = 0;
	unsigned int dma_tracer = 0;
	unsigned int io_tracer = 0;
	struct audio_input_stream stream;
	struct dsp_data_manage *manage = NULL;
	struct dsp_data_fragment *fragment = NULL;
	unsigned long time = 0;
	long ret = AUDIO_SUCCESS;

	ao_route = &(dsp->routes[AUDIO_ROUTE_SPK_ID]);
	if (ao_route == NULL) {
		audio_warn_print("%d;The route of amic record hasn't been created!\n", __LINE__);
		ret = -EPERM;
		return ret;
	}

	if (IS_ERR_OR_NULL((void __user *)arg)) {
		audio_warn_print("%d; the parameter is invalid!\n", __LINE__);
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&ao_route->stream_mlock);
	mutex_lock(&ao_route->mlock);
	if (ao_route->state != AUDIO_BUSY_STATE) {
		audio_warn_print("%d:please enable spk firstly!\n",__LINE__);
		ret = -EPERM;
		goto out;
	}

	ret = copy_from_user(&stream, (__user void*)arg, sizeof(stream));
	if (ret) {
		audio_warn_print("%d: failed to copy_from_user!\n", __LINE__);
		ret = -EIO;
		goto out;
	}

	if (IS_ERR_OR_NULL(stream.data) || stream.size == 0) {
		audio_warn_print("%d; the parameter is invalid!\n", __LINE__);
		ret = -EPERM;
		goto out;
	}
	manage = &(ao_route->manage);
	cnt = stream.size / manage->fragment_size;
again:
	if (ao_route->state != AUDIO_BUSY_STATE)
		goto out;
	dma_tracer = manage->dma_tracer;
	io_tracer = manage->io_tracer;
	/* first copy */
	while (i < cnt) {
		if (io_tracer+1 == dma_tracer || (dma_tracer==0 && io_tracer==ao_route->manage.fragment_cnt-1))
			break;
		fragment = &(manage->fragments[io_tracer]);
		if (fragment->state == false) {
			copy_from_user(fragment->vaddr, (stream.data + i * manage->fragment_size), manage->fragment_size);
			dma_sync_single_for_device(NULL, fragment->paddr, manage->fragment_size, DMA_TO_DEVICE);
			fragment->state = true;
		}
		i++;
		io_tracer = (io_tracer + 1) % manage->fragment_cnt;
	}
	manage->io_tracer = io_tracer;

	/* second copy */
	if (i < cnt) {
		ao_route->wait_flag = true;
		ao_route->wait_cnt = cnt - i - 1;
		mutex_unlock(&ao_route->mlock);
		time = wait_for_completion_timeout(&ao_route->done_completion, msecs_to_jiffies(800));
		if (!time) {
			audio_err_print("set spk timeout!\n");
			ret = -ETIMEDOUT;
			goto exit;
		}
		mutex_lock(&ao_route->mlock);
		goto again;
	}
out:
	mutex_unlock(&ao_route->mlock);
exit:
	mutex_unlock(&ao_route->stream_mlock);
	return ret;
}

static int disable_route_stream(struct audio_route *route)
{
	int ret = AUDIO_SUCCESS;
	if (route == NULL)
		return AUDIO_SUCCESS;

	switch (route->index) {
		case AUDIO_ROUTE_AEC_ID:
			ret = dsp_disable_amic_aec(route->priv);
			ret = dsp_disable_dmic_aec(route->priv);
			break;
		case AUDIO_ROUTE_AMIC_ID:
			ret = dsp_disable_amic_ai_and_aec(route->priv);
			break;
		case AUDIO_ROUTE_DMIC_ID:
			ret = dsp_disable_dmic_ai(route->priv);
			break;
		case AUDIO_ROUTE_SPK_ID:
			ret = dsp_disable_amic_ao(route->priv);
			break;
		default:
			break;
	}
	return ret;
}


static int dsp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct audio_dsp_device *dsp = misc_get_audiodsp(dev);
	struct audio_route *route = NULL;
	int index = 0;
	int ret = AUDIO_SUCCESS;

	mutex_lock(&dsp->mlock);
	if (dsp->state != AUDIO_IDLE_STATE) {
		dsp->refcnt++;
		mutex_unlock(&dsp->mlock);
		return 0;
	}

	for (index = 0; index < AUDIO_ROUTE_MAX_ID; index++) {
		if (index == AUDIO_ROUTE_DMIC_ID && dmic_enable == 0)
			continue;
		route = &(dsp->routes[index]);
		if (route && route->state == AUDIO_IDLE_STATE) {
			if (route->pipe && route->pipe->init)
				ret = route->pipe->init(route);
			if (ret != AUDIO_SUCCESS)
				goto error;
			route->state = AUDIO_OPEN_STATE;
			/* set default parameters */
			route->rate = 0;
			route->channel = 1;
			route->format = 16;
		}
	}

	/* enable hrtimer */
	if (atomic_read(&dsp->timer_stopped)) {
		atomic_set(&dsp->timer_stopped, 0);
		hrtimer_start(&dsp->hr_timer, dsp->expires , HRTIMER_MODE_REL);
		dsp->refcnt++;
	}
	dsp->state = AUDIO_OPEN_STATE;
	mutex_unlock(&dsp->mlock);
	return 0;

error:
	while (index--) {
		route = &(dsp->routes[index]);
		if (route) {
			if (route->pipe && route->pipe->deinit)
				ret = route->pipe->deinit(route->pipe);
			route->state = AUDIO_IDLE_STATE;
		}
	}

	mutex_unlock(&dsp->mlock);
	return -EPERM;
}

static int dsp_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct audio_dsp_device *dsp = misc_get_audiodsp(dev);
	struct audio_route *route = NULL;
	int index = 0;

	mutex_lock(&dsp->mlock);
	if (dsp->refcnt == 0)
		goto out;
	dsp->refcnt--;
	if (dsp->refcnt == 0) {
		if (dsp->state != AUDIO_IDLE_STATE) {
			atomic_set(&dsp->timer_stopped, 1);
			dsp->state = AUDIO_IDLE_STATE;
			for (index = 0; index < AUDIO_ROUTE_MAX_ID; index++) {
				route = &(dsp->routes[index]);
				if (route && route->state != AUDIO_IDLE_STATE) {
					disable_route_stream(route);
					if (route->pipe && route->pipe->deinit)
						route->pipe->deinit(route);
					route->state = AUDIO_IDLE_STATE;
				}
			}
		}
	}
out:
	mutex_unlock(&dsp->mlock);
	return 0;
}

static ssize_t dsp_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t dsp_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}


static long dsp_route_ioctl(struct audio_dsp_device *dsp, enum auido_route_index index, unsigned int cmd, void *arg)
{
	struct audio_route *route = NULL;
	long ret = AUDIO_SUCCESS;

	route = &(dsp->routes[index]);
	if (route->priv) {
		/* the route has been registered */
		mutex_lock(&route->mlock);
		if (route->state == AUDIO_IDLE_STATE) {
			audio_warn_print("%d; Please open the route%d firstly\n", __LINE__, index);
			ret = -EPERM;
			mutex_unlock(&route->mlock);
			goto out;
		}

		if (route->pipe && route->pipe->ioctl)
			ret = route->pipe->ioctl(route->pipe, cmd, arg);
		mutex_unlock(&route->mlock);
	}
out:
	return ret;

}

static long dsp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = file->private_data;
	struct audio_dsp_device *dsp = misc_get_audiodsp(dev);
	struct audio_route *route = NULL;
	struct audio_parameter param;
	struct volume vol;
	struct channel_mute mute;
	struct alc_gain alc;
	int dmic_vol = 0;
	int alc_en = 0;
	int channel = 0;
	long ret = -EINVAL;

	/* O_RDWR mode operation, do not allowed */
	if ((file->f_mode & FMODE_READ) && (file->f_mode & FMODE_WRITE))
		return -EPERM;

	if (dsp->state == AUDIO_IDLE_STATE) {
		audio_warn_print("please open /dev/dsp firstly!\n");
		return -EPERM;
	};

	switch (cmd) {
		case AMIC_AI_SET_PARAM:
			mutex_lock(&dsp->mlock);
			copy_from_user(&param, (__user void*)arg, sizeof(param));
			ret = dsp_config_route_param(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_CONFIG_PARAM, &param);
			if (ret == AUDIO_SUCCESS)
				ret = dsp_config_aec_route_param(dsp, AUDIO_CMD_CONFIG_PARAM, &param);
			mutex_unlock(&dsp->mlock);
			break;
		case AMIC_AI_GET_PARAM:
			mutex_lock(&dsp->mlock);
			route = &(dsp->routes[AUDIO_ROUTE_AMIC_ID]);
			if (route && arg) {
				param.rate = route->rate;
				param.channel = route->channel;
				param.format = route->format;
				copy_to_user((__user void*)arg, &param, sizeof(param));
			}
			mutex_unlock(&dsp->mlock);
			break;
		case AMIC_AO_SET_PARAM:
			mutex_lock(&dsp->mlock);
			copy_from_user(&param, (__user void*)arg, sizeof(param));
			ret = dsp_config_route_param(dsp, AUDIO_ROUTE_SPK_ID, AUDIO_CMD_CONFIG_PARAM, &param);
			mutex_unlock(&dsp->mlock);
			break;
		case AMIC_AO_GET_PARAM:
			mutex_lock(&dsp->mlock);
			route = &(dsp->routes[AUDIO_ROUTE_SPK_ID]);
			if (route && arg) {
				param.rate = route->rate;
				param.channel = route->channel;
				param.format = route->format;
				copy_to_user((__user void*)arg, &param, sizeof(param));
			}
			mutex_unlock(&dsp->mlock);
			break;
		case DMIC_AI_SET_PARAM:
			mutex_lock(&dsp->mlock);
			copy_from_user(&param, (__user void*)arg, sizeof(param));
			ret = dsp_config_route_param(dsp, AUDIO_ROUTE_DMIC_ID, AUDIO_CMD_CONFIG_PARAM, &param);
			mutex_unlock(&dsp->mlock);
			break;
		case DMIC_AI_GET_PARAM:
			mutex_lock(&dsp->mlock);
			route = &(dsp->routes[AUDIO_ROUTE_DMIC_ID]);
			if (route && arg) {
				param.rate = route->rate;
				param.channel = route->channel;
				param.format = route->format;
				copy_to_user((__user void*)arg, &param, sizeof(param));
			}
			mutex_unlock(&dsp->mlock);
			break;
		case AMIC_AI_ENABLE_STREAM:
			ret = dsp_enable_amic_ai_and_aec(dsp);
			break;
		case AMIC_ENABLE_AEC:
			ret = dsp_enable_amic_aec(dsp, arg);
			break;
		case DMIC_AI_ENABLE_STREAM:
			ret = dsp_enable_dmic_ai(dsp);
			break;
		case DMIC_ENABLE_AEC:
			ret = dsp_enable_dmic_aec(dsp, arg);
			break;
		case AMIC_AO_ENABLE_STREAM:
			ret = dsp_enable_amic_ao(dsp);
			break;
		case AMIC_AO_SYNC_STREAM:
			ret = dsp_ioctl_sync_ao_stream(dsp);
			break;
		case AMIC_AO_CLEAR_STREAM:
			ret = dsp_ioctl_clear_ao_stream(dsp);
			break;
		case AMIC_DISABLE_AEC:
			ret = dsp_disable_amic_aec(dsp);
			break;
		case DMIC_DISABLE_AEC:
			ret = dsp_disable_dmic_aec(dsp);
			break;
		case AMIC_AI_DISABLE_STREAM:
			ret = dsp_disable_amic_ai_and_aec(dsp);
			break;
		case DMIC_AI_DISABLE_STREAM:
			ret = dsp_disable_dmic_ai(dsp);
			break;
		case AMIC_AO_DISABLE_STREAM:
			ret = dsp_disable_amic_ao(dsp);
			break;
		case AMIC_AI_GET_STREAM:
			ret = dsp_get_mic_stream(dsp, AUDIO_ROUTE_AMIC_ID, arg);
			break;
		case DMIC_AI_GET_STREAM:
			ret = dsp_get_mic_stream(dsp, AUDIO_ROUTE_DMIC_ID, arg);
			break;
		case AMIC_AO_SET_STREAM:
			ret = dsp_set_spk_stream(dsp, arg);
			break;
		case AMIC_AI_HPF_ENABLE:
			if (get_user(channel, (int*)arg)) {
				ret = -EFAULT;
				goto EXIT_IOCTRL;
			}
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_SET_MIC_HPF_EN, &alc_en);
			break;
		case AMIC_AI_ENABLE_ALC_L:
			if (get_user(alc_en, (int*)arg)) {
				ret = -EFAULT;
				goto EXIT_IOCTRL;
			}
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_ENABLE_ALC_L, &alc_en);
			break;
		case AMIC_AI_ENABLE_ALC_R:
			if (get_user(alc_en, (int*)arg)) {
				ret = -EFAULT;
				goto EXIT_IOCTRL;
			}
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_ENABLE_ALC_R, &alc_en);
			break;
		case AMIC_AI_SET_ALC_GAIN:
			copy_from_user(&alc, (__user void*)arg, sizeof(alc));
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_SET_ALC_GAIN, &alc);
			break;
		case AMIC_AI_GET_ALC_GAIN:
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_GET_ALC_GAIN, &alc);
			if (ret == AUDIO_SUCCESS)
				copy_to_user((__user void*)arg, &alc, sizeof(alc));
			break;
		case AMIC_AI_SET_VOLUME:
			copy_from_user(&vol, (__user void*)arg, sizeof(vol));
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_SET_VOLUME, &vol);
			break;
		case AMIC_AI_GET_VOLUME:
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_GET_VOLUME, &vol);
			if (ret == AUDIO_SUCCESS)
				copy_to_user((__user void*)arg, &vol, sizeof(vol));
			break;
		case AMIC_AI_SET_GAIN:
			copy_from_user(&vol, (__user void*)arg, sizeof(vol));
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_SET_GAIN, &vol);
			break;
		case AMIC_AI_GET_GAIN:
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_GET_GAIN, &vol);
			if (ret == AUDIO_SUCCESS)
				copy_to_user((__user void*)arg, &vol, sizeof(vol));
			break;
		case AMIC_AI_SET_MUTE:
			copy_from_user(&mute, (__user void*)arg, sizeof(mute));
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_AMIC_ID, AUDIO_CMD_SET_MUTE, &mute);
			break;
		case AMIC_SPK_SET_VOLUME:
			copy_from_user(&vol, (__user void*)arg, sizeof(vol));
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_SPK_ID, AUDIO_CMD_SET_VOLUME, &vol);
			break;
		case AMIC_SPK_GET_VOLUME:
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_SPK_ID, AUDIO_CMD_GET_VOLUME, &vol);
			if (ret == AUDIO_SUCCESS)
				copy_to_user((__user void*)arg, &vol, sizeof(vol));
			break;
		case AMIC_SPK_SET_GAIN:
			copy_from_user(&vol, (__user void*)arg, sizeof(vol));
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_SPK_ID, AUDIO_CMD_SET_GAIN, &vol);
			break;
		case AMIC_SPK_GET_GAIN:
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_SPK_ID, AUDIO_CMD_GET_GAIN, &vol);
			if (ret == AUDIO_SUCCESS)
				copy_to_user((__user void*)arg, &vol, sizeof(vol));
			break;
		case AMIC_SPK_SET_MUTE:
			copy_from_user(&mute, (__user void*)arg, sizeof(mute));
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_SPK_ID, AUDIO_CMD_SET_MUTE, &mute);
			break;
		case DMIC_AI_SET_VOLUME:
			if (get_user(dmic_vol, (int*)arg)) {
				ret = -EFAULT;
				goto EXIT_IOCTRL;
			}
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_DMIC_ID, AUDIO_CMD_SET_GAIN, &dmic_vol);
			break;
		case DMIC_AI_GET_VOLUME:
			ret = dsp_route_ioctl(dsp, AUDIO_ROUTE_DMIC_ID, AUDIO_CMD_GET_GAIN, &dmic_vol);
			if (ret == AUDIO_SUCCESS)
				ret = put_user(dmic_vol, (int *)arg);
			break;
		default:
			audio_warn_print("SOUDND ERROR:ioctl command %d is not supported\n", cmd);
			ret = -1;
			goto EXIT_IOCTRL;
	}
EXIT_IOCTRL:
	return ret;
}

const struct file_operations audio_dsp_fops = {
	.owner = THIS_MODULE,
	.read = dsp_read,
	.write = dsp_write,
	.open = dsp_open,
	.unlocked_ioctl = dsp_ioctl,
	.release = dsp_release,
};

int register_audio_pipe(struct audio_pipe *pipe, enum auido_route_index index)
{
	struct audio_dsp_device* dsp = globe_dspdev;

	if (!dsp || index >= AUDIO_ROUTE_MAX_ID)
		return -AUDIO_EPERM;

	mutex_lock(&dsp->mlock);
	if (dsp->routes[index].pipe) {
		audio_warn_print("the pipe has been registered! index = %d\n", index);
		mutex_unlock(&dsp->mlock);
		return -AUDIO_EPERM;
	};

	dsp->routes[index].pipe = pipe;
	dsp->routes[index].index = index;
	dsp->routes[index].state = AUDIO_IDLE_STATE;
	dsp->routes[index].refcnt = 0;
	dsp->routes[index].wait_flag = false;
	mutex_init(&(dsp->routes[index].mlock));
	mutex_init(&(dsp->routes[index].stream_mlock));
	init_completion(&(dsp->routes[index].done_completion));

	if (index == AUDIO_ROUTE_AEC_ID)
		dsp->routes[index].parent = &(dsp->routes[AUDIO_ROUTE_AMIC_ID]);
	else
		dsp->routes[index].parent = NULL;
	dsp->routes[index].priv = dsp;

	mutex_unlock(&dsp->mlock);

	return AUDIO_SUCCESS;
}

int release_audio_pipe(struct audio_pipe *pipe)
{
	struct audio_dsp_device* dsp = globe_dspdev;
	struct audio_route *route = NULL;
	int index = 0;
	mutex_lock(&dsp->mlock);
	for (index = 0; index < AUDIO_ROUTE_MAX_ID; index++)
		if (dsp->routes[index].pipe == pipe) {
			route = &(dsp->routes[index]);
		}
	if (route && route->state > AUDIO_IDLE_STATE)
		disable_route_stream(route);
	if (route)
		memset(route, 0, sizeof(*route));

	mutex_unlock(&dsp->mlock);
	return AUDIO_SUCCESS;
}

int register_audio_debug_ops(char *name, struct file_operations *debug_ops, void* data)
{
	struct audio_dsp_device* dsp = globe_dspdev;
	if (!dsp)
		return -AUDIO_EPERM;

	mutex_lock(&dsp->mlock);
	proc_create_data(name, S_IRUGO, globe_dspdev->proc, debug_ops, data);
	mutex_unlock(&dsp->mlock);
	return AUDIO_SUCCESS;
}

extern struct platform_driver audio_aic_driver;
extern struct platform_driver audio_dmic_driver;

static int audio_dsp_probe(struct platform_device *pdev)
{
	struct audio_dsp_device* dspdev = NULL;
	struct platform_device **subdevs = NULL;
	int ret = AUDIO_SUCCESS;

	dspdev = (struct audio_dsp_device*)kzalloc(sizeof(*dspdev), GFP_KERNEL);
	if (!dspdev) {
		audio_err_print("Failed to allocate camera device\n");
		ret = -ENOMEM;
		goto exit;
	}

	/* init self */
	spin_lock_init(&dspdev->slock);
	mutex_init(&dspdev->mlock);
	dspdev->miscdev.minor = MISC_DYNAMIC_MINOR;
	dspdev->miscdev.name = "dsp";
	dspdev->miscdev.fops = &audio_dsp_fops;
	ret = misc_register(&dspdev->miscdev);
	if (ret < 0) {
		ret = -ENOENT;
		audio_err_print("Failed to register tx-isp miscdev!\n");
		goto failed_misc_register;
	}

	dspdev->proc = jz_proc_mkdir("audio");
	if (!dspdev->proc) {
		dspdev->proc = NULL;
		audio_err_print("Failed to create debug directory of tx-isp!\n");
		goto failed_to_proc;
	}

	atomic_set(&dspdev->timer_stopped, 1);
	hrtimer_init(&dspdev->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dspdev->hr_timer.function = jz_audio_hrtimer_callback;
	dspdev->expires = ns_to_ktime(1000*1000*fragment_time*10*2); // the time section is default 40ms.

	INIT_WORK(&dspdev->workqueue, dsp_workqueue_handle);

	globe_dspdev = dspdev;
	/* register subdev,AIC & DMIC*/
	subdevs = pdev->dev.platform_data;
	if (aic_enable) {
		platform_driver_register(&audio_aic_driver);
		platform_device_register(subdevs[0]);
	}

	if (dmic_enable) {
		platform_driver_register(&audio_dmic_driver);
		platform_device_register(subdevs[1]);
	}

	platform_set_drvdata(pdev, dspdev);
	globe_dspdev = dspdev;
	dspdev->refcnt = 0;
	dspdev->version = AUDIO_DRIVER_VERSION;

	printk("@@@@ audio driver ok(version %s) @@@@@\n", dspdev->version);

	return 0;

failed_to_proc:
	misc_deregister(&dspdev->miscdev);
failed_misc_register:
	kfree(dspdev);
exit:
	return ret;
}

static int __exit audio_dsp_remove(struct platform_device *pdev)
{
	struct audio_dsp_device* dspdev = platform_get_drvdata(pdev);
	struct platform_device **subdevs = NULL;

	misc_deregister(&dspdev->miscdev);
	proc_remove(dspdev->proc);
	subdevs = pdev->dev.platform_data;

	if (aic_enable) {
		platform_device_unregister(subdevs[0]);
		platform_driver_unregister(&audio_aic_driver);
	}

	if (dmic_enable) {
		platform_device_unregister(subdevs[1]);
		platform_driver_unregister(&audio_dmic_driver);
	}

	hrtimer_cancel(&dspdev->hr_timer);
	cancel_work_sync(&dspdev->workqueue);
	platform_set_drvdata(pdev, NULL);

	kfree(dspdev);
	globe_dspdev = NULL;
	return 0;
}

#ifdef CONFIG_PM
static int audio_dsp_suspend(struct device *dev)
{
	return 0;
}

static int audio_dsp_resume(struct device *dev)
{
	return 0;
}

static struct dev_pm_ops audio_dsp_pm_ops = {
	.suspend = audio_dsp_suspend,
	.resume = audio_dsp_resume,
};
#endif

static struct platform_driver audio_dsp_driver = {
	.probe = audio_dsp_probe,
	.remove = __exit_p(audio_dsp_remove),
	.driver = {
		.name = "jz-dsp",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &audio_dsp_pm_ops,
#endif
	},
};

extern struct platform_device audio_dsp_platform_device;
static int __init audio_dsp_init(void)
{
	int ret = AUDIO_SUCCESS;

	ret = platform_device_register(&audio_dsp_platform_device);
	if (ret) {
		printk("Failed to insmod dsp driver!!!\n");
		return ret;
	}

	ret = platform_driver_register(&audio_dsp_driver);
	if (ret) {
		platform_device_unregister(&audio_dsp_platform_device);
	}
	return ret;
}

static void __exit audio_dsp_exit(void)
{
	platform_driver_unregister(&audio_dsp_driver);
	platform_device_unregister(&audio_dsp_platform_device);
}

module_init(audio_dsp_init);
module_exit(audio_dsp_exit);

MODULE_AUTHOR("Ingenic xhshen");
MODULE_DESCRIPTION("audio driver");
MODULE_LICENSE("GPL");
