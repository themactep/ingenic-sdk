#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/hrtimer.h>
#include <dt-bindings/dma/ingenic-pdma.h>
#include <linux/of.h>
#include "hdmi_dsp.h"
#include "hdmi_aic.h"
#include "hdmi_audio_debug.h"


static int total_fragment_cnt = 50;
module_param(total_fragment_cnt, int, S_IRUGO);
MODULE_PARM_DESC(total_fragment_cnt, "the fragment cnt of buf");

static int fragment_time = 2;
module_param(fragment_time, int, S_IRUGO);
MODULE_PARM_DESC(fragment_time, "The unit of the time of fragment is 10ms");


static int max_sample_rate = 48000;
module_param(max_sample_rate, int, S_IRUGO);
MODULE_PARM_DESC(max_sample_rate, "the value of audio sample rate; ex: 96000");

void __iomem *aic_iomem;
static struct hdmi_dsp_device *g_hdmi_dspdev;
#if 0
extern void jzdma_dump(struct dma_chan *chan);
#endif
#define AUDIO_DRIVER_VERSION "H20220611a"
#define AUDIO_IO_LEADING_DMA (2)
#define CACHED_FRAGMENT (64)

static void dsp_workqueue_handle(struct work_struct *work)
{
	struct hdmi_dsp_device *dsp = container_of(work,struct hdmi_dsp_device, workqueue);
	struct audio_route *ao_route = &dsp->spk_route;
	unsigned int ao_new_tracer = 0;
	unsigned int dma_tracer = 0;
	unsigned int io_tracer = 0;
	unsigned long lock_flags;
	unsigned int io_late = 0;
	unsigned int cnt = 0;
	unsigned int index = 0;
	spin_lock_irqsave(&dsp->slock, lock_flags);
	ao_new_tracer = ao_route->manage.new_dma_tracer;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	mutex_lock(&ao_route->mlock);
	if(ao_route->state == AUDIO_BUSY_STATE){
		io_late = 0;
		dma_tracer = ao_route->manage.dma_tracer;
		if(ao_new_tracer < ao_route->manage.fragment_cnt && ao_new_tracer >= 0){
			while(dma_tracer != ao_new_tracer){
				if(dma_tracer == ao_route->manage.io_tracer){
					//				printk("%d: audio spk io late!\n", __LINE__);
					io_late = 1;
				}
				memset(ao_route->manage.fragments[dma_tracer].vaddr, 0, ao_route->manage.fragment_size);
				dma_cache_sync(NULL, ao_route->manage.fragments[dma_tracer].vaddr,
						ao_route->manage.fragment_size, DMA_TO_DEVICE);
				ao_route->manage.fragments[dma_tracer].state = false;
				dma_tracer = (dma_tracer + 1) % ao_route->manage.fragment_cnt;
			}
			ao_route->manage.dma_tracer = dma_tracer;
			/* clear dma prepare-buffer and sync io_tracer */
			for(cnt = 0; cnt <= AUDIO_IO_LEADING_DMA; cnt++){
				index = (ao_new_tracer + cnt) % ao_route->manage.fragment_cnt;
				if(ao_route->manage.fragments[index].state){
					ao_route->manage.fragments[index].state = false;
				}
				if(index == ao_route->manage.io_tracer)
					io_late = 1;
			}
			if(io_late){
				ao_route->manage.io_tracer = (index + 1) % ao_route->manage.fragment_cnt;
			}
		}else{
			printk("%d: audio spk dma transfer error!\n", __LINE__);
			memset(ao_route->manage.fragments[dma_tracer].vaddr, 0, ao_route->manage.fragment_size);
			dma_cache_sync(NULL, ao_route->manage.fragments[dma_tracer].vaddr,
					ao_route->manage.fragment_size, DMA_TO_DEVICE);
			memset(ao_route->manage.fragments[dma_tracer+1].vaddr, 0, ao_route->manage.fragment_size);
			dma_cache_sync(NULL, ao_route->manage.fragments[dma_tracer + 1].vaddr,
					ao_route->manage.fragment_size, DMA_TO_DEVICE);
			dma_tracer = (dma_tracer + 2) % ao_route->manage.fragment_cnt;
			ao_route->manage.dma_tracer = dma_tracer;
		}
	}
	/* wait second copy space */
	if(ao_route->wait_flag){
		cnt = 0;
		dma_tracer = ao_route->manage.dma_tracer;
		io_tracer  = ao_route->manage.io_tracer;
		while(io_tracer != dma_tracer){
			cnt++;
			io_tracer = (io_tracer + 1) % ao_route->manage.fragment_cnt;
		}
		if(cnt >= ao_route->wait_cnt){
			ao_route->wait_flag = false;
			complete(&ao_route->done_completion);
		}
	}
	mutex_unlock(&ao_route->mlock);
	return;
}

static enum hrtimer_restart hdmi_audio_hrtimer_callback(struct hrtimer *hr_timer)
{
	struct hdmi_dsp_device *dsp = container_of(hr_timer,struct hdmi_dsp_device, hr_timer);
	struct audio_route *route = &dsp->spk_route;
	dma_addr_t dma_currentaddr = 0;
	unsigned int index = 0;
	unsigned long lock_flags;

	if (atomic_read(&dsp->timer_stopped))
		goto out;
	hrtimer_start(&dsp->hr_timer, dsp->expires, HRTIMER_MODE_REL);
	spin_lock_irqsave(&dsp->slock, lock_flags);
	if(route->state == AUDIO_BUSY_STATE){
		/* sync all routes dma */
		dma_currentaddr = route->dma_chan->device->get_current_trans_addr(route->dma_chan, NULL, NULL,
				route->dma_config.direction);
		index = (dma_currentaddr - route->paddr) / route->manage.fragment_size;
		route->manage.new_dma_tracer = index;
		//printk("dma_currentaddr=0x%08x\n",dma_currentaddr);
		//printk("route->manage.fragment_size=%d\n",route->manage.fragment_size);
		//printk("index=%d\n",index);
	}
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
	schedule_work(&dsp->workqueue);
out:
	return HRTIMER_NORESTART;
}

static inline unsigned int format_to_bytes(unsigned int format)
{
	if(format <= 8)
		return 1;
	else if(format <= 16)
		return 2;
	else
		return 4;
}

static int dsp_init_dma_chan(struct audio_route *route)
{
	struct dsp_data_manage *manage = NULL;
	struct dma_async_tx_descriptor *desc;
	unsigned long flags = DMA_CTRL_ACK;
	int index = 0;
	int ret = AUDIO_SUCCESS;

	if(route->state == AUDIO_BUSY_STATE)
		goto out;
	/* init fragments manage and dma channel */
	manage = &route->manage;
	if (!manage) {
		printk("%s, %d manage is NULL.\n", __func__, __LINE__);
		return AUDIO_EPERM;
	}

	INIT_LIST_HEAD(&manage->fragments_head);
	manage->sample_size   = route->channel*format_to_bytes(route->format);
	manage->fragment_size = (route->rate / 100) * manage->sample_size * fragment_time;
	manage->fragment_cnt  = route->reservesize / manage->fragment_size;

	//printk("manage->fragment_cnt=%d\n",manage->fragment_cnt);
	//printk("manage->fragment_size=%d\n",manage->fragment_size);

	if (manage->fragment_cnt >= total_fragment_cnt)
		manage->fragment_cnt = total_fragment_cnt;

	/* init fragments manage */
	for(index = 0; index < manage->fragment_cnt; index++){
		manage->fragments[index].vaddr = route->vaddr + manage->fragment_size * index;
		manage->fragments[index].paddr = route->paddr + manage->fragment_size * index;
		//	printk("manage->fragments[index].paddr = 0x%08x\n",manage->fragments[index].paddr);
		manage->fragments[index].state = false;
		list_add_tail(&manage->fragments[index].list, &manage->fragments_head);
	}
	manage->buffersize = manage->fragment_cnt * manage->fragment_size;
	memset(route->vaddr, 0, manage->buffersize);
	dma_cache_sync(NULL, route->vaddr, manage->buffersize, DMA_TO_DEVICE);
	dmaengine_slave_config(route->dma_chan, &route->dma_config);
	desc = route->dma_chan->device->device_prep_dma_cyclic(route->dma_chan,
			route->paddr,
			manage->buffersize,
			manage->buffersize,
			route->dma_config.direction,
			flags);
	if (!desc) {
		dev_err(NULL, "cannot prepare slave dma\n");
		ret = -EINVAL;
		goto out;
	}
	dmaengine_submit(desc);
out:
	return ret;
}

static int dsp_deinit_dma_chan(struct audio_route *route)
{
	struct dsp_data_manage *manage = NULL;
	long ret = AUDIO_SUCCESS;

	if(route->state != AUDIO_BUSY_STATE)
		goto out;
	/* deinit fragments manage and dma channel */
	manage = &route->manage;
	if (false != route->is_trans) {
		dmaengine_terminate_all(route->dma_chan);
		route->is_trans = false;
	}
out:
	return ret;
}

static bool dma_chan_filter(struct dma_chan *chan, void *filter_param)
{
	struct audio_route *route = filter_param;
	return (void*)route->dma_type == chan->private;
}


int hdmi_dsp_set_param(struct audio_parameter *param)
{
	int ret = AUDIO_SUCCESS;
	struct audio_route *route = NULL;
	int sample_bytes = 0;
	struct hdmi_dsp_device *dsp = g_hdmi_dspdev;
	route = &dsp->spk_route;
	mutex_lock(&route->mlock);
	if(route->rate == param->rate && route->format == param->format
			&& route->channel == param->channel){
		goto out;
	}

	if(route->state >= AUDIO_CONFIG_STATE){
		printk("Can't modify audio parameters when audio is running!\n");
		ret = -EBUSY;
		goto out;
	}

	if (param->channel != MONO && param->channel != STEREO) {
		printk("spk only support stereo or mono.\n");
		goto out;
	}

	//set sample_rate
	clk_set_rate(dsp->spk_clk, param->rate*256);
	clk_enable(dsp->spk_clk);
	//set channel
	if(STEREO == param->channel){
		__i2s_out_channel_select(0x01);//max two channel
	}else{
		__i2s_out_channel_select(0x00);//one channel
		__i2s_enable_mono2stereo();
	}
	//set format
	switch(param->format){
		case VALID_8BIT:
			__i2s_set_oss_sample_size(0);
			route->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
			route->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
			sample_bytes = 1;
			break;
		case VALID_16BIT:
			__i2s_set_oss_sample_size(1);
			route->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			route->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			sample_bytes = 2;
			break;
		case VALID_20BIT:
			__i2s_set_oss_sample_size(3);
			route->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			route->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			sample_bytes = 4;
			break;
		case VALID_24BIT:
			__i2s_set_oss_sample_size(4);
			route->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			route->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			sample_bytes = 4;
			break;
		default:
			break;

	}
	route->dma_config.direction = DMA_MEM_TO_DEV;
	route->dma_config.src_maxburst = AIC_TX_FIFO_DEPTH/2;
	route->dma_config.dst_maxburst = AIC_TX_FIFO_DEPTH/2;
	route->dma_config.dst_addr     = dsp->res->start + AICDR;
	route->dma_config.src_addr     = 0;
	__i2s_disable_signadj();
	__i2s_disable_byteswap();

	route->rate   = param->rate;
	route->format = param->format;
	route->channel= param->channel;
	route->state  = AUDIO_CONFIG_STATE;

	mutex_unlock(&route->mlock);
	return ret;
out:
	mutex_unlock(&route->mlock);
	return ret;
}

int hdmi_dsp_enable_ao(void)
{
	unsigned long lock_flags;
	struct audio_route *route = NULL;
	struct hdmi_dsp_device *dsp = g_hdmi_dspdev;
	int ret = AUDIO_SUCCESS;
	route = &dsp->spk_route;
	mutex_lock(&route->mlock);

	if(AUDIO_BUSY_STATE == route->state) {
		mutex_unlock(&route->mlock);
		printk("The route of spk is busy now!\n");
		return AUDIO_SUCCESS;
	}

	ret = dsp_init_dma_chan(route);
	if(ret != AUDIO_SUCCESS){
		printk("config ao dma channel error.\n");
		goto out;
	}

	//enable hrtimer
	if(atomic_read(&dsp->timer_stopped)){
		atomic_set(&dsp->timer_stopped, 0);
		hrtimer_start(&dsp->hr_timer, dsp->expires , HRTIMER_MODE_REL);
	}

	//aic i2s enable
	__i2s_enable();
	__i2s_flush_tfifo();
	__i2s_enable_transmit_dma();
	__i2s_enable_replay();
	//dma start transfer
	dma_async_issue_pending(route->dma_chan);
	route->is_trans = true;
#if 0
	jzdma_dump(route->dma_chan);
#endif
	spin_lock_irqsave(&dsp->slock, lock_flags);
	route->state = AUDIO_BUSY_STATE;
	route->manage.dma_tracer = 0;
	route->manage.new_dma_tracer = 0;
	route->manage.io_tracer = AUDIO_IO_LEADING_DMA;
	init_completion(&route->done_completion);
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
out:
	mutex_unlock(&route->mlock);
	return ret;
}

int hdmi_dsp_disable_ao(void)
{
	unsigned long lock_flags;
	struct audio_route *route = NULL;
	int ret = AUDIO_SUCCESS;
	struct hdmi_dsp_device *dsp = g_hdmi_dspdev;
	route = &dsp->spk_route;
	mutex_lock(&route->mlock);

	if(route->state != AUDIO_BUSY_STATE){
		ret =  AUDIO_SUCCESS;
		goto out;
	}
	__i2s_disable_transmit_dma();
	__i2s_disable_replay();

	//avoid pop
	__i2s_write_tfifo(0);
	__i2s_write_tfifo(0);
	__i2s_write_tfifo(0);
	__i2s_write_tfifo(0);
	__i2s_enable_replay();
	msleep(2);
	__i2s_disable_replay();

	atomic_set(&dsp->timer_stopped, 1);
	ret = dsp_deinit_dma_chan(route);
	if(ret != AUDIO_SUCCESS){
		printk("destroy dma channel error.\n");
		goto out;
	}

	spin_lock_irqsave(&dsp->slock, lock_flags);
	route->state = AUDIO_OPEN_STATE;
	route->manage.dma_tracer = 0;
	route->manage.io_tracer  = 0;
	route->rate   = 0;
	route->format = 0;
	route->channel= 0;
	route->refcnt = 0;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	if(route->wait_flag){
		route->wait_flag = false;
		complete(&route->done_completion);
	}
out:
	mutex_unlock(&route->mlock);
	return ret;
}

int hdmi_dsp_sync_stream(void)
{
	long ret = 0;
	unsigned long lock_flags;
	unsigned int new_dma_tracer = 0;
	unsigned int dma_tracer = 0;
	unsigned int io_tracer = 0;
	unsigned int wait_cnt = 0;
	unsigned int cnt_flag = 0;
	struct hdmi_dsp_device *dsp = g_hdmi_dspdev;
	struct audio_route *route = &dsp->spk_route;

	mutex_lock(&route->mlock);
	if(route->state != AUDIO_BUSY_STATE){
		printk("%d; Please enable audio speaker firstly!\n", __LINE__);
		ret = -EPERM;
		goto out;
	}
	spin_lock_irqsave(&dsp->slock, lock_flags);
	new_dma_tracer = route->manage.new_dma_tracer;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);

	dma_tracer = route->manage.dma_tracer;
	io_tracer = route->manage.io_tracer;
	while(dma_tracer != io_tracer){
		if(dma_tracer == new_dma_tracer){
			cnt_flag = 1;
		}
		if(cnt_flag)
			wait_cnt++;
		dma_tracer = (dma_tracer + 1) % route->manage.fragment_cnt;
	}
out:
	mutex_unlock(&route->mlock);
	if(wait_cnt){
		msleep((wait_cnt + 1)*10*fragment_time);
	}
	return ret;
}

int hdmi_dsp_clear_stream(void)
{
	long ret = 0;
	unsigned long lock_flags;
	struct hdmi_dsp_device *dsp = g_hdmi_dspdev;
	unsigned int new_dma_tracer = 0;
	unsigned int dma_tracer = 0;
	unsigned int io_tracer = 0;
	unsigned int cnt_flag = 0;
	struct audio_route *route = &dsp->spk_route;

	mutex_lock(&route->mlock);
	if(route->state != AUDIO_BUSY_STATE){
		printk("%d; Please enable audio speaker firstly!\n", __LINE__);
		ret = -EPERM;
		goto out;
	}
	new_dma_tracer = route->manage.new_dma_tracer;
	dma_tracer = route->manage.dma_tracer;
	io_tracer  = route->manage.io_tracer;
	while(dma_tracer != io_tracer){
		if(dma_tracer == new_dma_tracer){
			cnt_flag = 1;
		}
		if(cnt_flag){
			memset(route->manage.fragments[dma_tracer].vaddr, 0, route->manage.fragment_size);
			dma_cache_sync(NULL, route->manage.fragments[dma_tracer].vaddr,
					route->manage.fragment_size, DMA_TO_DEVICE);
		}
		dma_tracer = (dma_tracer + 1) % route->manage.fragment_cnt;
	}
	spin_lock_irqsave(&dsp->slock, lock_flags);
	route->manage.io_tracer = (route->manage.new_dma_tracer + AUDIO_IO_LEADING_DMA) % route->manage.fragment_cnt;
	spin_unlock_irqrestore(&dsp->slock, lock_flags);
out:
	mutex_unlock(&route->mlock);
	return ret;
}

int hdmi_dsp_set_stream(unsigned long arg)
{
	struct hdmi_dsp_device *dsp = g_hdmi_dspdev;
	struct audio_route *route = NULL;
	int ret = AUDIO_SUCCESS;
	struct audio_ouput_stream stream;
	struct dsp_data_manage *manage = NULL;
	struct dsp_data_fragment *fragment = NULL;
	unsigned int dma_tracer = 0;
	unsigned int io_tracer = 0;
	int cnt = 0, i = 0;
	unsigned long time = 0;
	unsigned int block = 0;
	mm_segment_t old_fs;
	loff_t *pos;
	route = &dsp->spk_route;
	if(IS_ERR_OR_NULL((void __user *)arg)){
		printk("%d; invalid args!\n", __LINE__);
		ret = -EPERM;
		return ret;
	}
	mutex_lock(&route->stream_mlock);
	mutex_lock(&route->mlock);
	if(route->state != AUDIO_BUSY_STATE){
		printk("%d:please enable spk firstly!\n",__LINE__);
		ret = -EPERM;
		goto out;
	}

	ret = copy_from_user(&stream, (__user void*)arg, sizeof(struct audio_ouput_stream));
	if(ret){
		printk("%d: failed to copy_from_user!\n", __LINE__);
		ret = -EIO;
		goto out;
	}

	if(IS_ERR_OR_NULL(stream.data) || stream.size == 0){
		printk("%d; the parameter is invalid!\n", __LINE__);
		ret = -EPERM;
		goto out;
	}
	manage = &(route->manage);
	cnt    = stream.size / manage->fragment_size;
	block  = stream.block;
	if(block == NOBLOCK || block == QUERY){
		dma_tracer = manage->dma_tracer;
		io_tracer  = manage->io_tracer;
		while(i < cnt){
			if(io_tracer+1 == dma_tracer || (dma_tracer==0 && io_tracer==route->manage.fragment_cnt-1)){
				break;
			}
			i++;
			io_tracer = (io_tracer + 1) % manage->fragment_cnt;
		}

		if(i != cnt){
			ret = -1;
			goto out;//dma no space
		}else{
			if(block == QUERY){
				ret = 0;
				goto out;//QUERY MODE
			}else{
				i = 0;
			}
		}
	}

again:
	if(route->state != AUDIO_BUSY_STATE)
		goto out;
	dma_tracer = manage->dma_tracer;
	io_tracer  = manage->io_tracer;
	/* first copy */
	if(route->save_debugdata){
		old_fs = get_fs();
		set_fs(KERNEL_DS);
	}
	while(i < cnt){
		if(io_tracer+1 == dma_tracer || (dma_tracer==0 && io_tracer==route->manage.fragment_cnt-1))
			break;
		fragment = &(manage->fragments[io_tracer]);
		if(fragment->state == false){
			copy_from_user(fragment->vaddr, (stream.data + i * manage->fragment_size), manage->fragment_size);
			dma_cache_sync(NULL, fragment->vaddr, manage->fragment_size, DMA_TO_DEVICE);
			fragment->state = true;
			if(route->save_debugdata){
				pos = &(route->proc_savefd->f_pos);
				vfs_write(route->proc_savefd, fragment->vaddr, manage->fragment_size, pos);
			}
		}
		i++;
		io_tracer = (io_tracer + 1) % manage->fragment_cnt;
	}
	manage->io_tracer = io_tracer;
	if(route->save_debugdata){
		set_fs(old_fs);
	}

	/* second copy */
	if(i < cnt){
		route->wait_flag = true;
		route->wait_cnt = cnt - i - 1;
		mutex_unlock(&route->mlock);
		time = wait_for_completion_timeout(&route->done_completion, msecs_to_jiffies(800));
		if(!time){
			printk("set spk timeout!\n");
			ret = -ETIMEDOUT;
			goto exit;
		}
		mutex_lock(&route->mlock);
		goto again;
	}
out:
	mutex_unlock(&route->mlock);
exit:
	mutex_unlock(&route->stream_mlock);
	return ret;
}


static ssize_t dsp_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t dsp_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}
static int dsp_open(struct inode *inode, struct file *file)
{
#if 1
	struct miscdevice *dev = file->private_data;
	struct hdmi_dsp_device *dsp = misc_get_audiodsp(dev);
	struct audio_route *route = NULL;
	int ret = AUDIO_SUCCESS;

	mutex_lock(&dsp->mlock);
	if(dsp->state != AUDIO_IDLE_STATE){
		dsp->refcnt++;
		mutex_unlock(&dsp->mlock);
		return ret;
	}
	route = &(dsp->spk_route);
	route->state = AUDIO_OPEN_STATE;
	/* set default parameters */
	route->rate    = 8000;
	route->channel = 1;
	route->format  = 16;
	/* enable hrtimer */
	if(atomic_read(&dsp->timer_stopped)){
		atomic_set(&dsp->timer_stopped, 0);
		hrtimer_start(&dsp->hr_timer, dsp->expires , HRTIMER_MODE_REL);
		dsp->refcnt++;
	}
	dsp->state = AUDIO_OPEN_STATE;
	mutex_unlock(&dsp->mlock);
	return ret;
#endif
}

static int dsp_release(struct inode *inode, struct file *file)
{
#if 1
	struct miscdevice *dev = file->private_data;
	struct hdmi_dsp_device *dsp = misc_get_audiodsp(dev);
	struct audio_route *route = NULL;
	int ret = AUDIO_SUCCESS;
	mutex_lock(&dsp->mlock);
	if(dsp->refcnt == 0)
		goto out;
	dsp->refcnt--;
	if(dsp->refcnt == 0){
		if(dsp->state != AUDIO_IDLE_STATE){
			atomic_set(&dsp->timer_stopped, 1);
			dsp->state = AUDIO_IDLE_STATE;
			route = &(dsp->spk_route);
			if(route && route->state != AUDIO_IDLE_STATE){
				hdmi_dsp_disable_ao();
				route->state = AUDIO_IDLE_STATE;
			}
		}

	}
out:
	mutex_unlock(&dsp->mlock);
	return ret;
#endif
}

static long dsp_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
#if 1
	struct miscdevice *dev = file->private_data;
	struct hdmi_dsp_device *dsp = misc_get_audiodsp(dev);
	struct audio_parameter param;
	long ret = -EINVAL;

	/* O_RDWR mode operation, do not allowed */
	if ((file->f_mode & FMODE_READ) && (file->f_mode & FMODE_WRITE))
		return -EPERM;

	if(dsp->state == AUDIO_IDLE_STATE){
		audio_warn_print("please open /dev/dsp firstly!\n");
		return -EPERM;
	};

	switch (cmd) {
		case AMIC_HDMI_AO_SET_PARAM:
			mutex_lock(&dsp->mlock);
			copy_from_user(&param, (__user void*)arg, sizeof(param));
			ret = hdmi_dsp_set_param(&param);
			mutex_unlock(&dsp->mlock);
			break;
		case AMIC_HDMI_AO_SET_STREAM:
			ret = hdmi_dsp_set_stream(arg);
			break;
		case AMIC_HDMI_AO_ENABLE_STREAM:
			ret =  hdmi_dsp_enable_ao();
			break;
		case AMIC_HDMI_AO_DISABLE_STREAM:
			ret = hdmi_dsp_disable_ao();
			break;

		case AMIC_HDMI_AO_CLEAR_STREAM:
			ret = hdmi_dsp_clear_stream();
			break;
		case AMIC_HDMI_AO_SYNC_STREAM:
			ret = hdmi_dsp_sync_stream();
			break;
		default:
			audio_warn_print("SOUDND ERROR:ioctl command %d is not supported\n", cmd);
			ret = -1;
			goto EXIT_IOCTRL;
	}
EXIT_IOCTRL:
	return ret;
#endif
}
const struct file_operations audio_dsp_fops = {
	.owner = THIS_MODULE,
	.read  = dsp_read,
	.write = dsp_write,
	.open  = dsp_open,
	.unlocked_ioctl = dsp_ioctl,
	.release = dsp_release,
};

/* debug audio aic info */
static ssize_t audio_aic_cmd_set(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos)
{
	int len = 0;
	struct seq_file *m = file->private_data;
	struct hdmi_dsp_device* dspdev = (struct hdmi_dsp_device*)(m->private);
	struct audio_route *spk_route = NULL;
	char cmd_buf[AIC_PROC_CMD_SIZE];
	struct file *fd = NULL;

	if (NULL == dspdev) {
		audio_warn_print("error, dspdev is null\n");
		return len;
	}

	spk_route = &dspdev->spk_route;

	memset(cmd_buf, 0, AIC_PROC_CMD_SIZE);
	if(copy_from_user(cmd_buf, buffer, count > AIC_PROC_CMD_SIZE ? AIC_PROC_CMD_SIZE : count)){
		return -EFAULT;
	}

	if (!strncmp(cmd_buf, "start-save-ao-data", sizeof("start-save-ao-data")-1)) {
		/* save raw */
		fd = filp_open("/tmp/hdmi_audio.pcm", O_CREAT | O_WRONLY | O_TRUNC, 00766);
		if (fd < 0) {
			printk("Failed to open /tmp/hdmi_audio.pcm\n");
			goto exit;
		}

		/* write file */
		/*inode = file->f_dentry->d_inode;*/
		spk_route->proc_savefd = fd;
		mutex_lock(&spk_route->stream_mlock);
		spk_route->save_debugdata = true;
		mutex_unlock(&spk_route->stream_mlock);
	}else if(!strncmp(cmd_buf, "stop-save-ao-data", sizeof("stop-save-ao-data")-1)){
		mutex_lock(&spk_route->stream_mlock);
		spk_route->save_debugdata = false;
		mutex_unlock(&spk_route->stream_mlock);
		filp_close(spk_route->proc_savefd, NULL);
		spk_route->proc_savefd = NULL;
	}else{
		printk("Invalid command(%s)\n", cmd_buf);
	}
exit:
	return count;
}

static int audio_aic_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct hdmi_dsp_device* dspdev = (struct hdmi_dsp_device*)(m->private);
	struct audio_route *spk_route = NULL;

	if (NULL == dspdev) {
		audio_warn_print("error, dspdev is null\n");
		return len;
	}

	spk_route = &dspdev->spk_route;
	seq_printf(m ,"\nThe version of audio driver is : %s\n", AUDIO_DRIVER_VERSION);

	if(spk_route->is_trans){
		seq_printf(m, "Replay attr list : \n");
		seq_printf(m, "The living rate of replay : %u\n", spk_route->rate);
		seq_printf(m, "The living channel of replay : %d\n", spk_route->channel);
		seq_printf(m, "The living format of replay : %d\n", spk_route->format);
		seq_printf(m, "The reservesize is %uBytes\n", spk_route->reservesize);
	}else{
		seq_printf(m, "HDMI audio is disabled!\n");
	}
	return len;
}

static int dump_audio_aic_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, audio_aic_show, PDE_DATA(inode), 2048);
}
static const struct file_operations audio_aic_debug_fops = {
	.read = seq_read,
	.open = dump_audio_aic_open,
	.llseek = seq_lseek,
	.release = single_release,
	.write = audio_aic_cmd_set,
};

static int dsp_create_dma_chan(struct audio_route *route, struct device *dev)
{
	int ret = AUDIO_SUCCESS;
	struct dsp_data_manage *manage = NULL;
	dma_cap_mask_t mask;

	manage = &route->manage;
	if (!manage) {
		printk("%s, %d manage is NULL.\n", __func__, __LINE__);
		return AUDIO_EPERM;
	}

	route->is_trans = false;
	route->reservesize = PAGE_ALIGN(max_sample_rate/100*2*2*total_fragment_cnt);//16bit and stereo
	route->vaddr = dma_alloc_noncoherent(dev, route->reservesize, &route->paddr, GFP_KERNEL | GFP_DMA);
	if (NULL == route->vaddr) {
		printk("failed to alloc dma buffer.\n");
		ret = -ENOMEM;
		goto failed_alloc_dma_buf;
	}
	printk("%s %d\n",__func__,__LINE__);
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	route->dma_type = INGENIC_DMA_REQ_AIC1_TX;
	route->dma_chan = dma_request_channel(mask, dma_chan_filter, (void*)route);
	if (NULL == route->dma_chan) {
		printk("dma channel request failed\n");
		ret = -ENXIO;
		goto failed_request_channel;
	}
	/*printk("size = 0x%08x, route->vaddr = %p, route->paddr = 0x%08x.\n", route->reservesize, route->vaddr, route->paddr);*/

	/* create fragments manage */
	manage->fragments = kzalloc(sizeof(struct dsp_data_fragment) * total_fragment_cnt, GFP_KERNEL);
	if(manage->fragments == NULL){
		printk("%d, Can't malloc manage!\n",__LINE__);
		ret = -ENOMEM;
		goto out;
	}

	return AUDIO_SUCCESS;
out:
	dma_release_channel(route->dma_chan);
	route->dma_chan = NULL;
failed_request_channel:
	dmam_free_noncoherent(dev, route->reservesize, route->vaddr, route->paddr);
	route->vaddr = NULL;
failed_alloc_dma_buf:
	return ret;
}

static int dsp_destroy_dma_chan(struct audio_route *route)
{
	struct dsp_data_manage *manage = NULL;
	long ret = AUDIO_SUCCESS;
	struct hdmi_dsp_device *dsp = g_hdmi_dspdev;
	struct device *dev = &dsp->pdev->dev;

	/* deinit fragments manage and dma channel */
	manage = &route->manage;
	if (false != route->is_trans) {
		dmaengine_terminate_all(route->dma_chan);
		route->is_trans = false;
	}
	if(route->dma_chan){
		dma_release_channel(route->dma_chan);
		route->dma_chan = NULL;
	}
	if(route->vaddr){
		dmam_free_noncoherent(dev, route->reservesize, route->vaddr, route->paddr);
	}
	route->vaddr = NULL;
	/* destroy fragments manage */
	if(manage->fragments){
		kfree(manage->fragments);
		manage->fragment_cnt = 0;
		manage->fragment_size = 0;
		INIT_LIST_HEAD(&manage->fragments_head);
	}
	manage->fragments = NULL;
	return ret;
}

static int hdmi_dsp_probe(struct platform_device *pdev)
{
	struct hdmi_dsp_device* dspdev = NULL;
	struct device *dev = &pdev->dev;
	int ret = AUDIO_SUCCESS;

	dspdev = (struct hdmi_dsp_device*)kzalloc(sizeof(*dspdev), GFP_KERNEL);
	if (!dspdev) {
		printk("Failed to allocate hdmi dsp device\n");
		return -ENOMEM;
	}
	/* init self */
	spin_lock_init(&dspdev->slock);
	mutex_init(&dspdev->mlock);
	dspdev->miscdev.minor = MISC_DYNAMIC_MINOR;
	dspdev->miscdev.name  = "dsp_hdmi_audio";
	dspdev->miscdev.fops  = &audio_dsp_fops;
	ret = misc_register(&dspdev->miscdev);
	if (ret < 0) {
		ret = -ENOENT;
		audio_err_print("Failed to register hdmi audio miscdev!\n");
		goto failed_misc_register;
	}
	dspdev->pdev = pdev;
	dspdev->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!dspdev->res) {
		dev_err(dev, "Unable to get register resource\n");
		ret = -ENXIO;
		goto failed_get_res;
	}
	dspdev->iomem = devm_ioremap_resource(dev, dspdev->res);
	if (IS_ERR(dspdev->iomem)) {
		ret = PTR_ERR(dspdev->iomem);
		dev_err(dev,"cannot ioremap registers,err=%d\n", ret);
		goto failed_ioremap;
	}
	aic_iomem = dspdev->iomem;
	dspdev->ce_tclk = devm_clk_get(&pdev->dev, "ce_i2st1");
	if (IS_ERR(dspdev->ce_tclk)) {
		printk("dspdev get ce i2s tclock failed.\n");
		goto err_get_ce_i2st;
	}
	dspdev->spk_clk = devm_clk_get(&pdev->dev, "div_i2st1");
	if (IS_ERR(dspdev->spk_clk)) {
		printk("dspdev get i2s spk clock failed.\n");
		goto err_get_i2s_spk;
	}
	dspdev->aic_clk = devm_clk_get(&pdev->dev, "gate_aic1");
	if (IS_ERR(dspdev->aic_clk)) {
		printk("dspdev get aic clock failed.\n");
		goto err_get_aic_clk;
	}
	clk_prepare_enable(dspdev->ce_tclk);
	clk_prepare_enable(dspdev->aic_clk);
	clk_set_rate(dspdev->spk_clk, DEFAULT_REPLAY_CLK);
	clk_prepare_enable(dspdev->spk_clk);
	__i2s_disable();
	schedule_timeout(5);
	__i2s_disable();
	//using as external codec
	__i2s_master_clkset();
	__i2s_reset();
	while((i2s_read_reg(AICFR) & I2S_FIFO_RESET_STAT) != 0)
		msleep(1);

	__aic_select_i2s();
	//__i2s_select_msbjustified();
	__i2s_select_i2s();
	__i2s_external_codec();
	__i2s_select_share_clk();

	__i2s_disable_receive_dma();
	__i2s_disable_transmit_dma();
	__i2s_disable_tloop();
	__i2s_disable_record();
	__i2s_disable_replay();
	__i2s_disable_loopback();
	__i2s_disable_etfl();
	__i2s_clear_ror();
	__i2s_clear_tur();
	__i2s_set_transmit_trigger(DEFAULT_REPLAY_TRIGGER);
	__i2s_disable_overrun_intr();
	__i2s_disable_underrun_intr();
	__i2s_disable_transmit_intr();
	__i2s_disable_receive_intr();
	__i2s_send_rfirst();

	//__i2s_play_lastsample();
	__i2s_play_zero();
	__i2s_enable();
	//set timer
	atomic_set(&dspdev->timer_stopped, 1);
	hrtimer_init(&dspdev->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dspdev->hr_timer.function = hdmi_audio_hrtimer_callback;
	dspdev->expires = ns_to_ktime(1000*1000*fragment_time*10*2);
	INIT_WORK(&dspdev->workqueue, dsp_workqueue_handle);
	dspdev->state = AUDIO_OPEN_STATE;
	//spk route init
	dspdev->spk_route.state = AUDIO_OPEN_STATE;
	mutex_init(&dspdev->spk_route.mlock);
	mutex_init(&dspdev->spk_route.stream_mlock);
	dspdev->spk_route.rate    = 0;
	dspdev->spk_route.format  = 0;
	dspdev->spk_route.channel = 0;
	dspdev->spk_route.wait_flag = false;
	dspdev->spk_route.wait_cnt  = 0;
	dspdev->spk_route.reservesize = 0;
	dspdev->spk_route.state  = AUDIO_IDLE_STATE;
	dspdev->spk_route.refcnt = 0;

	init_completion(&(dspdev->spk_route.done_completion));

	ret = dsp_create_dma_chan(&dspdev->spk_route, &pdev->dev);
	if(ret != AUDIO_SUCCESS){
		audio_err_print("Failed to create audio dma channel!\n");
		goto failed_create_dma;
	}

	dspdev->proc = jz_proc_mkdir("hdmi_audio");
	if (!dspdev->proc) {
		dspdev->proc = NULL;
		audio_err_print("Failed to create debug directory of tx-isp!\n");
		goto failed_to_proc;
	}

	dspdev->spk_route.save_debugdata = false;
	proc_create_data("info", S_IRUGO, dspdev->proc, &audio_aic_debug_fops, dspdev);

	platform_set_drvdata(pdev, dspdev);
	dspdev->version = AUDIO_DRIVER_VERSION;
	g_hdmi_dspdev   = dspdev;
	printk("@@@@ hdmi audio driver ok(version %s) @@@@@\n", dspdev->version);
	return ret;
failed_to_proc:
	dsp_destroy_dma_chan(&dspdev->spk_route);
failed_create_dma:
	devm_clk_put(&pdev->dev, dspdev->aic_clk);
err_get_aic_clk:
	devm_clk_put(&pdev->dev, dspdev->spk_clk);
err_get_i2s_spk:
	devm_clk_put(&pdev->dev, dspdev->ce_tclk);
err_get_ce_i2st:
	iounmap(dspdev->iomem);
failed_ioremap:
	release_resource(dspdev->res);
failed_get_res:
failed_misc_register:
	kfree(dspdev);
	return ret;
}

static int __exit hdmi_dsp_remove(struct platform_device *pdev)
{
	struct hdmi_dsp_device* dspdev = platform_get_drvdata(pdev);
	__i2s_disable();
	proc_remove(dspdev->proc);
	dsp_destroy_dma_chan(&dspdev->spk_route);
	if(dspdev->spk_route.proc_savefd)
		filp_close(dspdev->spk_route.proc_savefd, NULL);
	dspdev->spk_route.proc_savefd = NULL;
	devm_clk_put(&pdev->dev, dspdev->spk_clk);
	devm_clk_put(&pdev->dev, dspdev->ce_tclk);
	devm_clk_put(&pdev->dev, dspdev->aic_clk);
	iounmap(dspdev->iomem);
	release_resource(dspdev->res);
	hrtimer_cancel(&dspdev->hr_timer);
	cancel_work_sync(&dspdev->workqueue);
	platform_set_drvdata(pdev, NULL);
	kfree(dspdev);
	g_hdmi_dspdev = NULL;
	return 0;
}

#ifdef CONFIG_PM
static int hdmi_dsp_suspend(struct device *dev)
{
	return 0;
}

static int hdmi_dsp_resume(struct device *dev)
{
	return 0;
}

static struct dev_pm_ops hdmi_dsp_pm_ops = {
	.suspend = hdmi_dsp_suspend,
	.resume = hdmi_dsp_resume,
};
#endif

static const struct of_device_id ingenic_hdmi_dsp_dt_ids[] = {
	{.compatible = "ingenic,aic1",},
	{}
};

static struct platform_driver hdmi_dsp_driver = {
	.probe = hdmi_dsp_probe,
	.remove = __exit_p(hdmi_dsp_remove),
	.driver = {
		.name = "ingenic-aic1",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &hdmi_dsp_pm_ops,
#endif
		.of_match_table = of_match_ptr(ingenic_hdmi_dsp_dt_ids),
	},
};

int hdmi_dsp_init(void)
{
	return platform_driver_register(&hdmi_dsp_driver);
}

void hdmi_dsp_exit(void)
{
	platform_driver_unregister(&hdmi_dsp_driver);
}
#if 1
module_init(hdmi_dsp_init);
module_exit(hdmi_dsp_exit);
MODULE_AUTHOR("Sadick<weijie.xu@ingenic.com>");
MODULE_DESCRIPTION("hdmi audio driver");
MODULE_LICENSE("GPL");
#endif
