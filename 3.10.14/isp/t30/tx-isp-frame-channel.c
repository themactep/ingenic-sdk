/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/bug.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/fs.h>

#include <tx-isp-list.h>
#include "tx-isp-frame-channel.h"
#include "tx-isp-videobuf.h"
#include "tx-isp-debug.h"

#define V4L2_BUFFER_MASK_FLAGS	(V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED | \
				 V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR | \
				 V4L2_BUF_FLAG_PREPARED | \
				 V4L2_BUF_FLAG_TIMESTAMP_MASK)

static int frame_channel_buffer_done(struct tx_isp_frame_channel *chan, void *arg)
{
	unsigned long flags = 0;
	struct frame_channel_buffer *buf = arg;
	struct fs_vb2_queue *q = &chan->vbq;
	struct fs_vb2_buffer *vb = NULL;
	struct fs_vb2_buffer *pos = NULL;

	if(buf == NULL)
		return 0;

	private_spin_lock_irqsave(&chan->slock, flags);
	tx_list_for_each_entry(pos, &q->queued_list, queued_entry){
		if(pos->v4l2_buf.m.userptr == buf->addr){
			vb = pos;
			break;
		}
	}
	private_spin_unlock_irqrestore(&chan->slock, flags);

	if(vb && vb->state == FS_VB2_BUF_STATE_ACTIVE){
		struct timespec ts;
		getrawmonotonic(&ts);

		vb->v4l2_buf.timestamp.tv_sec = ts.tv_sec;
		vb->v4l2_buf.timestamp.tv_usec = ts.tv_nsec / 1000;

		vb->v4l2_buf.sequence = buf->priv;
		/* Add the buffer to the done buffers list */
		private_spin_lock_irqsave(&q->done_lock, flags);
		vb->state = FS_VB2_BUF_STATE_DONE;
		tx_list_add_tail(&vb->done_entry, &q->done_list);
		q->done_count++;
		/* Remove from videobuf queue */
		tx_list_del(&vb->queued_entry);
		q->queued_count--;
		private_spin_unlock_irqrestore(&q->done_lock, flags);

		/* Inform any processes that may be waiting for buffers */
		wake_up(&q->done_wq);
		private_complete(&chan->comp);
		if(chan->out_frames && (chan->out_frames + 1 != buf->priv)){
			ISP_INFO("chan%d: source frames %d, output frames %d\n", chan->index, buf->priv, chan->out_frames + 1);
		}
		chan->out_frames = buf->priv;
	//	printk("bufdone chan%d buf.index = %d\n", chan->index, buf->vb.v4l2_buf.index);
	}else{
		chan->losed_frames++;
	}

	return 0;
}

static int frame_chan_event(struct tx_isp_subdev_pad *pad, unsigned int event, void *arg)
{
	int ret = 0;
	struct tx_isp_subdev *sd = NULL;
	struct tx_isp_frame_sources *fs = NULL;
	struct tx_isp_frame_channel *chan = NULL;

	if(IS_ERR_OR_NULL(pad))
		return -EINVAL;

	sd = pad->sd;
	fs = sd_to_frame_sources(sd);
	chan = &(fs->chans[pad->index]);

	switch(event){
		case TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER:
			ret = frame_channel_buffer_done(chan, arg);
				break;
		default:
			break;
	}
	return ret;
}


static inline void frame_imagefmt_to_v4l2_format(struct frame_image_format *fmt, struct v4l2_format *vfmt)
{
	if(IS_ERR_OR_NULL(fmt) || IS_ERR_OR_NULL(vfmt))
		return;
	vfmt->type = fmt->type;
	memcpy(&vfmt->fmt, &fmt->pix, sizeof(fmt->pix));
}


static inline void v4l2_format_to_frame_imagefmt(struct v4l2_format *vfmt, struct frame_image_format *fmt)
{
	if(IS_ERR_OR_NULL(fmt) || IS_ERR_OR_NULL(vfmt))
		return;
	fmt->type = vfmt->type;
	memcpy(&fmt->pix, &vfmt->fmt, sizeof(fmt->pix));
}


static long frame_channel_vidioc_set_fmt(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	struct frame_image_format fmt;
	long ret = 0;
	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	ret = copy_from_user(&fmt, (void __user *)arg, sizeof(fmt));
	if(ret){
		ISP_ERROR("Failed to copy from user\n");
		return -ENOMEM;
	}

	if(fmt.type != V4L2_BUF_TYPE_VIDEO_CAPTURE){
		ISP_ERROR("The frame type is invalid!\n");
		return -EINVAL;
	}

	if (fmt.pix.field == V4L2_FIELD_ANY)
		fmt.pix.field = V4L2_FIELD_INTERLACED;
	else if (V4L2_FIELD_INTERLACED != fmt.pix.field){
		ISP_ERROR("The field is invalid!\n");
		return -EINVAL;
	}

	if(fmt.pix.colorspace != V4L2_COLORSPACE_SRGB){
		ISP_ERROR("The colorspace is invalid!\n");
		return -EINVAL;
	}

	ret = tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_SET_FMT, &fmt);
	if(ret && ret != -ENOIOCTLCMD){
		ISP_ERROR("Failed to set fmt to frame chan%d!\n", chan->index);
		return ret;
	};

	ret = copy_to_user((void __user *)arg, &fmt, sizeof(fmt));
	if(ret){
		ISP_ERROR("Failed to copy to user\n");
		return -ENOMEM;
	}

	chan->fmt = fmt;

	return 0;
}


static long frame_channel_vidioc_get_fmt(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	struct frame_image_format fmt;
	long ret = 0;

	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	ret = tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_GET_FMT, &fmt);
	if(ret && ret != -ENOIOCTLCMD){
		ISP_ERROR("Failed to get fmt to frame chan%d!\n", chan->index);
		return ret;
	};

	fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	fmt.pix.field = V4L2_FIELD_INTERLACED;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = copy_to_user((void __user *)arg, &fmt, sizeof(fmt));
	if(ret){
		ISP_ERROR("Failed to copy to user\n");
		return -ENOMEM;
	}
	chan->fmt = fmt;

	return 0;
}


/**
 * __fill_v4l2_buffer() - fill in a struct v4l2_buffer with information to be
 * returned to userspace
 */
static void __fill_v4l2_buffer(struct fs_vb2_buffer *vb, struct v4l2_buffer *b)
{
	struct fs_vb2_queue *q = vb->vb2_queue;

	/* Copy back data such as timestamp, flags, etc. */
	memcpy(b, &vb->v4l2_buf, offsetof(struct v4l2_buffer, m));
	b->reserved2 = vb->v4l2_buf.reserved2;
	b->reserved = vb->v4l2_buf.reserved;

	/*
	 * Clear any buffer state related flags.
	 */
	b->flags &= ~V4L2_BUFFER_MASK_FLAGS;
	b->flags |= q->timestamp_type;

	switch (vb->state) {
	case FS_VB2_BUF_STATE_QUEUED:
	case FS_VB2_BUF_STATE_ACTIVE:
		b->flags |= V4L2_BUF_FLAG_QUEUED;
		break;
	case FS_VB2_BUF_STATE_ERROR:
		b->flags |= V4L2_BUF_FLAG_ERROR;
		/* fall through */
	case FS_VB2_BUF_STATE_DONE:
		b->flags |= V4L2_BUF_FLAG_DONE;
		break;
	case FS_VB2_BUF_STATE_DEQUEUED:
	default:
		/* nothing */
		break;
	}
}

/**
 * __fill_vb2_buffer() - fill a vb2_buffer with information provided in a
 * v4l2_buffer by the userspace. The caller has already verified that struct
 * v4l2_buffer has a valid number of planes.
 */
static inline void __fill_vb2_buffer(struct fs_vb2_buffer *vb, const struct v4l2_buffer *b)
{
	vb->v4l2_buf.m.userptr = b->m.userptr;
	vb->v4l2_buf.length	= b->length;
	vb->v4l2_buf.field = b->field;
	vb->v4l2_buf.timestamp = b->timestamp;
	vb->v4l2_buf.flags = b->flags & ~V4L2_BUFFER_MASK_FLAGS;
}


/**
 * __vb2_queue_alloc() - allocate videobuf buffer structures
 * video buffer memory for all buffers on the queue and initializes the
 * queue
 *
 * Returns the number of buffers successfully allocated.
 */
static int __vb2_queue_alloc(struct fs_vb2_queue *q, unsigned int num_buffers)
{
	unsigned int buffer;
	struct fs_vb2_buffer *vb;

	for (buffer = 0; buffer < num_buffers; ++buffer) {
		/* Allocate videobuf buffer structures */
		vb = kzalloc(q->buf_struct_size, GFP_KERNEL);
		if (!vb) {
			ISP_ERROR("Memory alloc for buffer struct failed\n");
			break;
		}

		vb->state = FS_VB2_BUF_STATE_DEQUEUED;
		vb->vb2_queue = q;
		vb->v4l2_buf.index = q->num_buffers + buffer;
		vb->v4l2_buf.type = q->type;
		vb->v4l2_buf.memory = q->memory;

		q->bufs[q->num_buffers + buffer] = vb;
	}

	ISP_INFO("Allocated %d buffers!\n",buffer);

	return buffer;
}


/**
 * __vb2_queue_free() - free buffers at the end of the queue - video memory and
 * related information, if no buffers are left return the queue to an
 * uninitialized state. Might be called even if the queue has already been freed.
 */
static void __vb2_queue_free(struct fs_vb2_queue *q, unsigned int buffers)
{
	unsigned int buffer;

	/* Free videobuf buffers */
	for (buffer = q->num_buffers - buffers; buffer < q->num_buffers;
	     ++buffer) {
		kfree(q->bufs[buffer]);
		q->bufs[buffer] = NULL;
	}

	q->num_buffers -= buffers;

	TX_INIT_LIST_HEAD(&q->queued_list);
}


/**
 * __reqbufs() - Initiate streaming
 * @q:		videobuf2 queue
 * @req:	struct passed from userspace to vidioc_reqbufs handler in driver
 *
 * Should be called from vidioc_reqbufs ioctl handler of a driver.
 * This function:
 * 1) verifies streaming parameters passed from the userspace,
 * 2) sets up the queue,
 * 3) negotiates number of buffers and planes per buffer with the driver
 *    to be used during streaming,
 * 4) allocates internal buffer structures (struct vb2_buffer), according to
 *    the agreed parameters,
 * 5) for MMAP memory type, allocates actual video memory, using the
 *    memory handling/allocation routines provided during queue initialization
 *
 * If req->count is 0, all the memory will be freed instead.
 * If the queue has been allocated previously (by a previous vb2_reqbufs) call
 * and the queue is not busy, memory will be reallocated.
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_reqbufs handler in driver.
 */
static int frame_channel_reqbufs(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	struct fs_vb2_queue *q = NULL;
	struct v4l2_requestbuffers req;
	unsigned int num_buffers, allocated_buffers;
	int ret;

	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	if(IS_ERR_OR_NULL((void*)arg)){
		ISP_ERROR("The parameter from user is invalid!\n");
		return -EINVAL;
	}

	ret = copy_from_user(&req, (void __user *)arg, sizeof(req));
	if(ret){
		ISP_ERROR("Failed to copy from user\n");
		return -ENOMEM;
	}

	q = &chan->vbq;

	if (q->streaming) {
		ISP_ERROR("reqbufs: streaming active\n");
		return -EBUSY;
	}

	if (req.count == 0 || q->num_buffers != 0 || q->memory != req.memory) {

		__vb2_queue_free(q, q->num_buffers);

		/*
		 * In case of REQBUFS(0) return immediately without calling
		 * driver's queue_setup() callback and allocating resources.
		 */
		if (req.count == 0)
			return 0;
	}

	if(q->memory != req.memory){
		ISP_ERROR("reqbufs: the memory type isn't userptr!\n");
		return -EINVAL;
	}

	/*
	 * Make sure the requested values and current defaults are sane.
	 */
	num_buffers = min_t(unsigned int, req.count, ISP_VIDEO_MAX_FRAME);

	/* Finally, allocate buffers and video memory */
	ret = __vb2_queue_alloc(q, num_buffers);
	if (ret == 0) {
		ISP_ERROR("Memory allocation failed\n");
		return -ENOMEM;
	}

	allocated_buffers = ret;

	/*
	 * Check if driver can handle the allocated number of buffers.
	 */
	if (allocated_buffers < num_buffers) {
		__vb2_queue_free(q, allocated_buffers);
		ISP_ERROR("Can't allocation %d buffers!\n", num_buffers);
		return -ENOMEM;
	}

	q->num_buffers = allocated_buffers;
	req.count = allocated_buffers;

	ret = tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_SET_BANKS, &allocated_buffers);
	if (ret && ret != -ENOIOCTLCMD) {
		ISP_ERROR("streamon: driver refused to start streaming\n");
		return ret;
	}

	ret = copy_to_user((void __user *)arg, &req, sizeof(req));
	if(ret){
		ISP_ERROR("Failed to copy to user\n");
		return -ENOMEM;
	}

	frame_imagefmt_to_v4l2_format(&chan->fmt, &q->format);
	return 0;
}


static int __buf_prepare(struct fs_vb2_buffer *vb, const struct v4l2_buffer *b)
{
	struct frame_channel_video_buffer *buf = vb_to_video_buffer(vb);
	/*struct fs_vb2_queue *q = vb->vb2_queue;*/
	dma_addr_t addr = 0;

	__fill_vb2_buffer(vb, b);

	addr = (dma_addr_t)vb->v4l2_buf.m.userptr;

	dma_sync_single_for_device(NULL, addr, vb->v4l2_buf.length, DMA_FROM_DEVICE);

	buf->buf.addr = (unsigned int)addr;
	INIT_LIST_HEAD(&buf->buf.entry);

	vb->state = FS_VB2_BUF_STATE_PREPARED;

	return 0;
}

/**
 * __enqueue_in_driver() - enqueue a vb2_buffer in driver for processing
 */
static int __enqueue_in_driver(struct fs_vb2_buffer *vb)
{
	struct fs_vb2_queue *q = vb->vb2_queue;
	struct frame_channel_video_buffer *buffer = vb_to_video_buffer(vb);
	struct tx_isp_frame_channel *chan = vbq_to_frame_chan(q);
	int ret = 0;

	private_mutex_lock(&q->mlock);
	vb->state = FS_VB2_BUF_STATE_ACTIVE;
	private_mutex_unlock(&q->mlock);

	ret = tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER, &buffer->buf);
	if(ret && ret != -ENOIOCTLCMD){
		ISP_ERROR("Failed to qbuf to driver; chan%d!\n", chan->index);
		return ret;
	};
	return ret;
}

/**
 * frame_channel_vb2_qbuf() - Queue a buffer from userspace
 * @q:		videobuf2 queue
 * @buf:		buffer structure passed from userspace to vidioc_qbuf handler
 *		in driver
 *
 * Should be called from vidioc_qbuf ioctl handler of a driver.
 * This function:
 * 1) verifies the passed buffer,
 * 2) if necessary, calls buf_prepare callback in the driver (if provided), in
 *    which driver-specific buffer initialization can be performed,
 * 3) if streaming is on, queues the buffer in driver by the means of buf_queue
 *    callback for processing.
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_qbuf handler in driver.
 */
static int frame_channel_vb2_qbuf(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	struct fs_vb2_queue *q = NULL;
	struct fs_vb2_buffer *vb;
	struct v4l2_buffer buf;
	unsigned long flags = 0;
	int ret = 0;

	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	if(IS_ERR_OR_NULL((void*)arg)){
		ISP_ERROR("The parameter from user is invalid!\n");
		return -EINVAL;
	}

	ret = copy_from_user(&buf, (void __user *)arg, sizeof(buf));
	if(ret){
		ISP_ERROR("Failed to copy from user\n");
		return -ENOMEM;
	}

	q = &chan->vbq;
	if (buf.type != q->type) {
		ISP_ERROR("qbuf: invalid buffer type\n");
		ret = -EINVAL;
		goto unlock;
	}

	if (buf.index >= q->num_buffers) {
		ISP_ERROR("qbuf: buffer index(%d) out of range(%d) \n", buf.index, q->num_buffers);
		ret = -EINVAL;
		goto unlock;
	}

	vb = q->bufs[buf.index];
	if (NULL == vb) {
		/* Should never happen */
		ISP_ERROR("qbuf: buffer is NULL\n");
		ret = -EINVAL;
		goto unlock;
	}

	if (buf.memory != q->memory) {
		ISP_ERROR("qbuf: invalid memory type\n");
		ret = -EINVAL;
		goto unlock;
	}

	if(buf.length != q->format.fmt.pix.sizeimage)
	{
		ISP_ERROR("qbuf: invalid memory size, length = %d sizeimage = %d\n", buf.length, q->format.fmt.pix.sizeimage);
		ret = -EINVAL;
		goto unlock;
	}

	if(vb->state != FS_VB2_BUF_STATE_DEQUEUED)
	{
		ISP_ERROR("qbuf: buffer already in use\n");
		ret = -EINVAL;
		goto unlock;
	}

	__buf_prepare(vb, &buf);

	/*
	 * Add to the queued buffers list, a buffer will stay on it until
	 * dequeued in dqbuf.
	 */
	private_spin_lock_irqsave(&chan->slock, flags);
	tx_list_add_tail(&vb->queued_entry, &q->queued_list);
	vb->state = FS_VB2_BUF_STATE_QUEUED;
	q->queued_count++;
	private_spin_unlock_irqrestore(&chan->slock, flags);

	/*
	 * If already streaming, give the buffer to driver for processing.
	 * If not, the buffer will be given to driver on next streamon.
	 */
	if (q->streaming)
		__enqueue_in_driver(vb);

	/* Fill buffer information for the userspace */
	__fill_v4l2_buffer(vb, &buf);

	ret = copy_to_user((void __user *)arg, &buf, sizeof(buf));
	if(ret){
		ISP_ERROR("Failed to copy to user\n");
		return -ENOMEM;
	}
unlock:
	return ret;
}


/**
 * __vb2_wait_for_done_vb() - wait for a buffer to become available
 * for dequeuing
 *
 */
static int __vb2_wait_for_done_vb(struct fs_vb2_queue *q)
{
	/*
	 * All operations on vb_done_list are performed under done_lock
	 * spinlock protection. However, buffers may be removed from
	 * it and returned to userspace only while holding both driver's
	 * lock and the done_lock spinlock. Thus we can be sure that as
	 * long as we hold the driver's lock, the list will remain not
	 * empty if list_empty() check succeeds.
	 */

	for (;;) {
		int ret;

		if (!q->streaming) {
			ISP_ERROR("Streaming off, will not wait for buffers\n");
			return -EINVAL;
		}

		if (!tx_list_empty(&q->done_list)) {
			/*
			 * Found a buffer that we were waiting for.
			 */
			break;
		}

		/*
		 * All locks have been released, it is safe to sleep now.
		 */
		ret = wait_event_interruptible(q->done_wq,
				!tx_list_empty(&q->done_list) || !q->streaming);
		if(ret == -ERESTARTSYS){
			/*ISP_WRANING("systemcall happens when wait queue!\n");*/
			continue;
		}

		/*
		 * We need to reevaluate both conditions again after reacquiring
		 * the locks or return an error if one occurred.
		 */
		if (ret) {
			ISP_ERROR("Sleep was interrupted\n");
			return ret;
		}
	}
	return 0;
}

/**
 * __vb2_get_done_vb() - get a buffer ready for dequeuing
 *
 */
static int __vb2_get_done_vb(struct fs_vb2_queue *q, struct fs_vb2_buffer **vb)
{
	unsigned long flags;
	int ret = 0;

	/*
	 * Wait for at least one buffer to become available on the done_list.
	 */
	ret = __vb2_wait_for_done_vb(q);
	if (ret)
		return ret;

	/*
	 * Driver's lock has been held since we last verified that done_list
	 * is not empty, so no need for another list_empty(done_list) check.
	 */
	private_spin_lock_irqsave(&q->done_lock, flags);
	*vb = tx_list_first_entry(&q->done_list, struct fs_vb2_buffer, done_entry);
	tx_list_del(&(*vb)->done_entry);
	q->done_count--;
	private_spin_unlock_irqrestore(&q->done_lock, flags);

	return ret;
}

/**
 * vb2_wait_for_all_buffers() - wait until all buffers are given back to vb2
 * @q:		videobuf2 queue
 *
 * This function will wait until all buffers that have been given to the driver
 * by buf_queue() are given back to vb2 with vb2_buffer_done(). It doesn't call
 * wait_prepare, wait_finish pair. It is intended to be called with all locks
 * taken, for example from stop_streaming() callback.
 */
#if 0
static int tx_vb2_wait_for_all_buffers(struct fs_vb2_queue *q)
{
	if (!q->streaming) {
		ISP_ERROR("Streaming off, will not wait for buffers\n");
		return -EINVAL;
	}

	wait_event(q->done_wq, !q->queued_count);
	return 0;
}
#endif
/**
 * vb2_dqbuf() - Dequeue a buffer to the userspace
 * @q:		videobuf2 queue
 * @b:		buffer structure passed from userspace to vidioc_dqbuf handler
 *		in driver
 * @nonblocking: if true, this call will not sleep waiting for a buffer if no
 *		 buffers ready for dequeuing are present. Normally the driver
 *		 would be passing (file->f_flags & O_NONBLOCK) here
 *
 * Should be called from vidioc_dqbuf ioctl handler of a driver.
 * This function:
 * 1) verifies the passed buffer,
 * 2) calls buf_finish callback in the driver (if provided), in which
 *    driver can perform any additional operations that may be required before
 *    returning the buffer to userspace, such as cache sync,
 * 3) the buffer struct members are filled with relevant information for
 *    the userspace.
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_dqbuf handler in driver.
 */
static int frame_channel_vb2_dqbuf(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	struct fs_vb2_queue *q = NULL;
	struct fs_vb2_buffer *vb;
	struct v4l2_buffer buf;
	int ret = 0;

	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	if(IS_ERR_OR_NULL((void*)arg)){
		ISP_ERROR("The parameter from user is invalid!\n");
		return -EINVAL;
	}

	ret = copy_from_user(&buf, (void __user *)arg, sizeof(buf));
	if(ret){
		ISP_ERROR("Failed to copy from user\n");
		return -ENOMEM;
	}

	q = &chan->vbq;
	if (buf.type != q->type) {
		ISP_ERROR("dqbuf: invalid buffer type\n");
		return -EINVAL;
	}

	ret = __vb2_get_done_vb(q, &vb);
	if (ret < 0)
		return ret;

	/* Fill buffer information for the userspace */
	__fill_v4l2_buffer(vb, &buf);
	/* go back to dequeued state */
	vb->state = FS_VB2_BUF_STATE_DEQUEUED;

	ret = copy_to_user((void __user *)arg, &buf, sizeof(buf));
	if(ret){
		ISP_ERROR("Failed to copy to user\n");
		return -ENOMEM;
	}

	return 0;
}

/**
 * vb2_querybuf() - query video buffer information
 * @q:		videobuf queue
 * @b:		buffer struct passed from userspace to vidioc_querybuf handler
 *		in driver
 *
 * Should be called from vidioc_querybuf ioctl handler in driver.
 * This function will verify the passed v4l2_buffer structure and fill the
 * relevant information for the userspace.
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_querybuf handler in driver.
 */
static long frame_channel_vb2_querybuf(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	struct fs_vb2_queue *q = NULL;
	struct fs_vb2_buffer *vb;
	struct v4l2_buffer buf;
	long ret = 0;

	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	if(IS_ERR_OR_NULL((void*)arg)){
		ISP_ERROR("The parameter from user is invalid!\n");
		return -EINVAL;
	}

	ret = copy_from_user(&buf, (void __user *)arg, sizeof(buf));
	if(ret){
		ISP_ERROR("Failed to copy from user\n");
		return -ENOMEM;
	}

	q = &chan->vbq;
	if (buf.type != q->type) {
		ISP_ERROR("dqbuf: invalid buffer type\n");
		return -EINVAL;
	}

	if (buf.index >= q->num_buffers) {
		ISP_ERROR("querybuf: buffer index out of range\n");
		return -EINVAL;
	}
	vb = q->bufs[buf.index];
	__fill_v4l2_buffer(vb, &buf);

	ret = copy_to_user((void __user *)arg, &buf, sizeof(buf));
	if(ret){
		ISP_ERROR("Failed to copy to user\n");
		return -ENOMEM;
	}
	return ret;
}

/**
 * __vb2_queue_cancel() - cancel and stop (pause) streaming
 *
 * Removes all queued buffers from driver's queue and all buffers queued by
 * userspace from videobuf's queue. Returns to state after reqbufs.
 */
static void __vb2_queue_cancel(struct fs_vb2_queue *q)
{
	struct tx_isp_frame_channel *chan = vbq_to_frame_chan(q);
	unsigned long flags = 0;
	unsigned int i;

	if(IS_ERR_OR_NULL(q)){
		return;
	}

	/*
	 * Tell driver to stop all transactions and release all queued
	 * buffers.
	 */
	if (q->streaming)
		tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF, NULL);
	/*tx_vb2_wait_for_all_buffers(q);*/

	q->streaming = 0;

	/*
	 * Remove all buffers from videobuf's list...
	 */
	TX_INIT_LIST_HEAD(&q->queued_list);
	/*
	 * ...and done list; userspace will not receive any buffers it
	 * has not already dequeued before initiating cancel.
	 */
	private_spin_lock_irqsave(&q->done_lock, flags);
	TX_INIT_LIST_HEAD(&q->done_list);
	q->queued_count = 0;
	q->done_count = 0;
	private_spin_unlock_irqrestore(&q->done_lock, flags);
	wake_up_all(&q->done_wq);

	/*
	 * Reinitialize all buffers for next use.
	 */
	for (i = 0; i < q->num_buffers; ++i)
		q->bufs[i]->state = FS_VB2_BUF_STATE_DEQUEUED;
}


/*
 * vb2_streamon - start streaming
 * @q:		videobuf2 queue
 * @type:	type argument passed from userspace to vidioc_streamon handler
 *
 * Should be called from vidioc_streamon handler of a driver.
 * This function:
 * 1) verifies current state
 * 2) passes any previously queued buffers to the driver and starts streaming
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_streamon handler in the driver.
 */
static int frame_channel_vb2_streamon(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	struct fs_vb2_queue *q = NULL;
	struct fs_vb2_buffer *vb;
	enum v4l2_buf_type type;
	int ret = 0;

	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	if(chan->state != TX_ISP_MODULE_INIT){
		ISP_ERROR("The state of frame channel%d is invalid(%d)!\n", chan->index, chan->state);
		return -EPERM;
	}

	if(IS_ERR_OR_NULL((void*)arg)){
		ISP_ERROR("The parameter from user is invalid!\n");
		return -EINVAL;
	}

	ret = copy_from_user(&type, (void __user *)arg, sizeof(type));
	if(ret){
		ISP_ERROR("Failed to copy from user\n");
		return -ENOMEM;
	}

	q = &chan->vbq;
	if (type != q->type) {
		ISP_ERROR("streamon: invalid stream type\n");
		return -EINVAL;
	}

	if (q->streaming) {
		ISP_ERROR("streamon: already streaming\n");
		return -EBUSY;
	}

	/*
	 * If any buffers were queued before streamon,
	 * we can now pass them to driver for processing.
	 */
	tx_list_for_each_entry(vb, &q->queued_list, queued_entry)
		__enqueue_in_driver(vb);

	/*
	 * Let driver notice that streaming state has been enabled.
	 */
	ret = tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_STREAM_ON, NULL);
	if (ret && ret != -ENOIOCTLCMD) {
		ISP_ERROR("streamon: driver refused to start streaming\n");
		__vb2_queue_cancel(q);
		return ret;
	}

	q->streaming = 1;
	chan->state = TX_ISP_MODULE_RUNNING;
	ISP_INFO("Streamon successful\n");
	return 0;
}

/**
 * vb2_streamoff - stop streaming
 * @q:		videobuf2 queue
 * @type:	type argument passed from userspace to vidioc_streamoff handler
 *
 * Should be called from vidioc_streamoff handler of a driver.
 * This function:
 * 1) verifies current state,
 * 2) stop streaming and dequeues any queued buffers, including those previously
 *    passed to the driver (after waiting for the driver to finish).
 *
 * This call can be used for pausing playback.
 * The return values from this function are intended to be directly returned
 * from vidioc_streamoff handler in the driver
 */
static int __frame_channel_vb2_streamoff(struct tx_isp_frame_channel *chan, enum v4l2_buf_type type)
{
	struct fs_vb2_queue *q = &chan->vbq;
	if (type != q->type) {
		ISP_ERROR("streamon: invalid stream type\n");
		return -EINVAL;
	}

	if (!q->streaming) {
		ISP_WRANING("streamoff: not streaming\n");
		return -EINVAL;
	}

	/*
	 * Cancel will pause streaming and remove all buffers from the driver
	 * and videobuf, effectively returning control over them to userspace.
	 */
	__vb2_queue_cancel(q);

	chan->state = TX_ISP_MODULE_INIT;
	/*ISP_INFO("Streamoff successful\n");*/
	return 0;
}

static int frame_channel_vb2_streamoff(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	/*struct fs_vb2_queue *q = NULL;*/
	enum v4l2_buf_type type;
	int ret = 0;

	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	if(IS_ERR_OR_NULL((void*)arg)){
		ISP_ERROR("The parameter from user is invalid!\n");
		return -EINVAL;
	}

	ret = copy_from_user(&type, (void __user *)arg, sizeof(type));
	if(ret){
		ISP_ERROR("Failed to copy from user\n");
		return -ENOMEM;
	}

	return __frame_channel_vb2_streamoff(chan, type);
}

/**
 * isp_vb2_queue_init() - initialize a videobuf2 queue
 * @q:		videobuf2 queue; this structure should be allocated in driver
 *
 * The vb2_queue structure should be allocated by the driver. The driver is
 * responsible of clearing it's content and setting initial values for some
 * required entries before calling this function.
 * q->ops, q->mem_ops, q->type and q->io_modes are mandatory. Please refer
 * to the struct fs_vb2_queue description in include/media/videobuf2-core.h
 * for more information.
 */
static int isp_vb2_queue_init(struct fs_vb2_queue *q)
{
	if(IS_ERR_OR_NULL(q))
		return -EINVAL;

	memset(q, 0, sizeof(*q));
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->memory = V4L2_MEMORY_USERPTR;
	q->buf_struct_size = sizeof(struct frame_channel_video_buffer);
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	TX_INIT_LIST_HEAD(&q->queued_list);
	TX_INIT_LIST_HEAD(&q->done_list);
	spin_lock_init(&q->done_lock);
	private_mutex_init(&q->mlock);
	q->queued_count = 0;
	q->done_count = 0;
	init_waitqueue_head(&q->done_wq);

	return 0;
}

static void tx_vb2_queue_release(struct fs_vb2_queue *q)
{
	__vb2_queue_cancel(q);
	__vb2_queue_free(q, q->num_buffers);
}

static int frame_channel_set_channel_banks(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	int count = 0;
	int ret = 0;
	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	if(IS_ERR_OR_NULL((void*)arg)){
		ISP_ERROR("The parameter from user is invalid!\n");
		return -EINVAL;
	}

	ret = copy_from_user(&count, (void __user *)arg, sizeof(count));
	if(ret){
		ISP_ERROR("Failed to copy from user\n");
		return -ENOMEM;
	}

	ret = tx_isp_send_event_to_remote(chan->pad, TX_ISP_EVENT_FRAME_CHAN_SET_BANKS, &count);
	if (ret && ret != -ENOIOCTLCMD) {
		ISP_ERROR("streamon: driver refused to start streaming\n");
		return ret;
	}
	return ISP_SUCCESS;
}

static int frame_channel_listen_buffer(struct tx_isp_frame_channel *chan, unsigned long arg)
{
	int ret = ISP_SUCCESS;
	int err = ISP_SUCCESS;
	int buffers = 0;
#if 0
	if(chan->state != TX_ISP_MODULE_RUNNING){
		ret = -EPERM;
		buffers = -1;
		goto done;
	}
#endif
	ret = private_wait_for_completion_interruptible(&chan->comp);
	if (ret < 0)
		buffers = ret;
	else
		buffers = chan->comp.done + 1;

#if 0
done:
#endif
	err = copy_to_user((void __user *)arg, &buffers, sizeof(buffers));
	if(err){
		ISP_ERROR("Failed to copy to user\n");
		return -ENOMEM;
	}
	return ret;
}

static long frame_channel_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *mdev = file->private_data;
	struct tx_isp_frame_channel *chan = IS_ERR_OR_NULL(mdev) ? NULL : miscdev_to_frame_chan(mdev);
	long ret = ISP_SUCCESS;

	switch(cmd){
		case VIDIOC_SET_FRAME_FORMAT:
			ret = frame_channel_vidioc_set_fmt(chan, arg);
			break;
		case VIDIOC_GET_FRAME_FORMAT:
			ret = frame_channel_vidioc_get_fmt(chan, arg);
			break;
		case VIDIOC_REQBUFS:
			ret = frame_channel_reqbufs(chan, arg);
			break;
		case VIDIOC_QUERYBUF:
			ret = frame_channel_vb2_querybuf(chan, arg);
			break;
		case VIDIOC_QBUF:
			ret = frame_channel_vb2_qbuf(chan, arg);
			break;
		case VIDIOC_DQBUF:
			ret = frame_channel_vb2_dqbuf(chan, arg);
			break;
		case VIDIOC_STREAMON:
			ret = frame_channel_vb2_streamon(chan, arg);
			break;
		case VIDIOC_STREAMOFF:
			ret = frame_channel_vb2_streamoff(chan, arg);
			break;
		case VIDIOC_DEFAULT_CMD_SET_BANKS:
			ret = frame_channel_set_channel_banks(chan, arg);
			break;
		case VIDIOC_DEFAULT_CMD_LISTEN_BUF:
			ret = frame_channel_listen_buffer(chan, arg);
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
	}
	return (long)ret;
}


static int frame_channel_open(struct inode *inode, struct file *file)
{
	struct miscdevice *mdev = file->private_data;
	struct tx_isp_frame_channel *chan = IS_ERR_OR_NULL(mdev) ? NULL : miscdev_to_frame_chan(mdev);

	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	private_mutex_lock(&chan->mlock);
	if(chan->state < TX_ISP_MODULE_ACTIVATE){
		private_mutex_unlock(&chan->mlock);
		ISP_ERROR("Frame channel%d is slake now, Please activate it firstly!\n", chan->index);
		return -EPERM;
	}
	private_mutex_unlock(&chan->mlock);

	memset(&chan->fmt, 0, sizeof(chan->fmt));
	chan->out_frames = 0;
	chan->losed_frames = 0;
	private_init_completion(&chan->comp);
	__vb2_queue_free(&chan->vbq, chan->vbq.num_buffers);
	chan->state = TX_ISP_MODULE_INIT;

	return ISP_SUCCESS;
}

static int frame_channel_release(struct inode *inode, struct file *file)
{
	struct miscdevice *mdev = file->private_data;
	struct tx_isp_frame_channel *chan = IS_ERR_OR_NULL(mdev) ? NULL : miscdev_to_frame_chan(mdev);

	if(IS_ERR_OR_NULL(chan)){
		return -EINVAL;
	}

	if(chan->state == TX_ISP_MODULE_RUNNING){
		__frame_channel_vb2_streamoff(chan, chan->vbq.type);
		__vb2_queue_free(&chan->vbq, chan->vbq.num_buffers);
		chan->state = TX_ISP_MODULE_ACTIVATE;
	}

	return ISP_SUCCESS;
}

static struct file_operations fs_channel_ops ={
	.open 		= frame_channel_open,
	.release 	= frame_channel_release,
	.unlocked_ioctl	= frame_channel_unlocked_ioctl,
};

static int fs_activate_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_frame_sources *fs = IS_ERR_OR_NULL(sd) ? NULL : sd_to_frame_sources(sd);
	int index = 0;

	if(IS_ERR_OR_NULL(fs))
		return -EINVAL;
	if(fs->state != TX_ISP_MODULE_SLAKE)
		return 0;

	for(index = 0; index < fs->num_chans; index++){
		if(fs->chans[index].state != TX_ISP_MODULE_SLAKE){
			ISP_ERROR("The state of channel%d is invalid when be activated!\n", index);
			return -1;
		}
		fs->chans[index].state = TX_ISP_MODULE_ACTIVATE;
	}
	fs->state = TX_ISP_MODULE_ACTIVATE;
	return 0;
}

static int fs_slake_module(struct tx_isp_subdev *sd)
{
	struct tx_isp_frame_sources *fs = IS_ERR_OR_NULL(sd) ? NULL : sd_to_frame_sources(sd);
	struct tx_isp_frame_channel *chan = NULL;
	int index = 0;

	if(IS_ERR_OR_NULL(fs))
		return -EINVAL;
	if(fs->state == TX_ISP_MODULE_SLAKE)
		return 0;

	for(index = 0; index < fs->num_chans; index++){
		chan = &(fs->chans[index]);
		if(chan->state == TX_ISP_MODULE_RUNNING){
			__frame_channel_vb2_streamoff(chan, chan->vbq.type);
			__vb2_queue_free(&chan->vbq, chan->vbq.num_buffers);
		}
		chan->state = TX_ISP_MODULE_SLAKE;
	}
	fs->state = TX_ISP_MODULE_SLAKE;
	return 0;
}

struct tx_isp_subdev_internal_ops fs_internal_ops = {
	.activate_module = &fs_activate_module,
	.slake_module = &fs_slake_module,
};

static struct tx_isp_subdev_ops fs_subdev_ops = {
	.internal = &fs_internal_ops,
};


static int tx_isp_frame_chan_init(struct tx_isp_frame_channel *chan, struct tx_isp_subdev_pad *pad)
{
	/*struct fs_vb2_queue *q = NULL;*/
	int ret = 0;

	if(IS_ERR_OR_NULL(chan) || IS_ERR_OR_NULL(pad)){
		return -EINVAL;
	}

	chan->index = pad->index;
	chan->pad = pad;
	if(pad->type == TX_ISP_PADTYPE_UNDEFINE){
		chan->state = TX_ISP_MODULE_UNDEFINE;
		return 0;
	}

	sprintf(chan->name, "framechan%d", chan->index);
	chan->misc.minor = MISC_DYNAMIC_MINOR;
	chan->misc.name = chan->name;
	chan->misc.fops = &fs_channel_ops;
	ret = private_misc_register(&chan->misc);
	if (ret < 0) {
		ret = -ENOENT;
		ISP_ERROR("Failed to register framechan%d!\n", chan->index);
		goto failed_misc_register;
	}

	ret = isp_vb2_queue_init(&chan->vbq);
	if(ret)
		goto failed_to_init_queue;

	private_spin_lock_init(&chan->slock);
	private_mutex_init(&chan->mlock);
	private_init_completion(&chan->comp);
	pad->event = frame_chan_event;
	chan->state = TX_ISP_MODULE_SLAKE;

	return 0;
failed_to_init_queue:
	private_misc_deregister(&chan->misc);
failed_misc_register:
	return ret;
}

static void tx_isp_frame_chan_deinit(struct tx_isp_frame_channel *chan)
{
	if(IS_ERR_OR_NULL(chan)){
		return;
	}

	private_misc_deregister(&chan->misc);
	tx_vb2_queue_release(&chan->vbq);
	chan->state = TX_ISP_MODULE_SLAKE;
}

/* debug framesource info */
static int isp_framesource_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_frame_sources *fs = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct tx_isp_frame_channel *chan = NULL;
	struct fs_vb2_queue *vbq = NULL;
	struct fs_vb2_buffer *pos = NULL;
	unsigned long flags = 0;
	char *fmt = NULL;
	int index = 0;

	if(IS_ERR_OR_NULL(fs)){
		ISP_ERROR("The parameter is invalid!\n");
		return 0;
	}
	for(index = 0; index < fs->num_chans; index++){
		len += seq_printf(m ,"############## framesource %d ###############\n", index);
		chan = &(fs->chans[index]);
		vbq = &(chan->vbq);
		len += seq_printf(m ,"chan status: %s\n", chan->state == TX_ISP_MODULE_RUNNING?"running":"stop");
		if(chan->state != TX_ISP_MODULE_RUNNING)
			continue;
		fmt = (char *)(&chan->fmt.pix.pixelformat);
		len += seq_printf(m ,"output pixformat: %c%c%c%c\n", fmt[0],fmt[1],fmt[2],fmt[3]);
		len += seq_printf(m ,"output resolution: %d * %d\n", chan->fmt.pix.width, chan->fmt.pix.height);
		len += seq_printf(m ,"scaler : %s\n", chan->fmt.scaler_enable?"enable":"disable");
		if(chan->fmt.scaler_enable){
			len += seq_printf(m ,"scaler width: %d\n", chan->fmt.scaler_out_width);
			len += seq_printf(m ,"scaler height: %d\n", chan->fmt.scaler_out_height);
		}
		len += seq_printf(m ,"crop : %s\n", chan->fmt.crop_enable?"enable":"disable");
		if(chan->fmt.crop_enable){
			len += seq_printf(m ,"crop top: %d\n", chan->fmt.crop_top);
			len += seq_printf(m ,"crop left: %d\n", chan->fmt.crop_left);
			len += seq_printf(m ,"crop width: %d\n", chan->fmt.crop_width);
			len += seq_printf(m ,"crop height: %d\n", chan->fmt.crop_height);
		}
		len += seq_printf(m ,"the state of buffers:\n");
		private_spin_lock_irqsave(&chan->slock, flags);
		len += seq_printf(m ,"queue count: %d\n", vbq->queued_count);
		tx_list_for_each_entry(pos, &vbq->queued_list, queued_entry){
			len += seq_printf(m ,"queue addr: 0x%08lx\n", pos->v4l2_buf.m.userptr);
		}
		pos = NULL;
		len += seq_printf(m ,"done count: %d\n", vbq->done_count);
		tx_list_for_each_entry(pos, &vbq->done_list, done_entry){
			len += seq_printf(m ,"done addr: 0x%08lx\n", pos->v4l2_buf.m.userptr);
		}
		private_spin_unlock_irqrestore(&chan->slock, flags);
		len += seq_printf(m ,"the output buffers is: %d\n", chan->out_frames);
		len += seq_printf(m ,"the losted buffers is: %d\n", chan->losed_frames);
	}
	return len;
}
static int dump_isp_framesource_open(struct inode *inode, struct file *file)
{
	return private_single_open_size(file, isp_framesource_show, PDE_DATA(inode), 2048);
}
static struct file_operations isp_framesource_fops ={
	.read = private_seq_read,
	.open = dump_isp_framesource_open,
	.llseek = private_seq_lseek,
	.release = private_single_release,
};


static int tx_isp_fs_probe(struct platform_device *pdev)
{
	struct tx_isp_frame_sources *fs = NULL;
	struct tx_isp_subdev *sd = NULL;
	struct tx_isp_device_descriptor *desc = NULL;
	int ret = ISP_SUCCESS;
	int i = 0;

	fs = (struct tx_isp_frame_sources *)kzalloc(sizeof(*fs), GFP_KERNEL);
	if(!fs){
		ISP_ERROR("Failed to allocate csi device\n");
		ret = -ENOMEM;
		goto exit;
	}
	desc = pdev->dev.platform_data;
	sd = &fs->sd;
	ret = tx_isp_subdev_init(pdev, sd, &fs_subdev_ops);
	if(ret){
		ISP_ERROR("Failed to init isp module(%d.%d)\n", desc->parentid, desc->unitid);
		ret = -ENOMEM;
		goto failed_to_ispmodule;
	}

	/*printk("## %s %d chans = %d ##\n",__func__,__LINE__, sd->num_outpads);*/
	fs->num_chans = sd->num_outpads;
	if(fs->num_chans){
		fs->chans = kzalloc(sizeof(struct tx_isp_frame_channel)*fs->num_chans, GFP_KERNEL);
		for(i = 0; i < fs->num_chans; i++){
			ret = tx_isp_frame_chan_init(&(fs->chans[i]), &(sd->outpads[i]));
			if(ret)
				goto failed_to_init_chan;
		}
	}
	fs->state = TX_ISP_MODULE_SLAKE;
	private_platform_set_drvdata(pdev, &sd->module);
	tx_isp_set_subdev_debugops(sd, &isp_framesource_fops);
	tx_isp_set_subdevdata(sd, fs);
	return 0;
failed_to_init_chan:
	while(--i)
		tx_isp_frame_chan_deinit(&(fs->chans[i]));
failed_to_ispmodule:
	kfree(fs);
exit:
	return ret;
}

static int __exit tx_isp_fs_remove(struct platform_device *pdev)
{
	struct tx_isp_module *module = private_platform_get_drvdata(pdev);
	struct tx_isp_subdev *sd = module_to_subdev(module);
	struct tx_isp_frame_sources *fs = tx_isp_get_subdevdata(sd);
	int index = 0;

	for(index = 0; index < fs->num_chans; index++)
		tx_isp_frame_chan_deinit(&(fs->chans[index]));
	if(fs->chans)
		kfree(fs->chans);
	tx_isp_subdev_deinit(sd);
	kfree(fs);
	return 0;
}

struct platform_driver tx_isp_fs_driver = {
	.probe = tx_isp_fs_probe,
	.remove = __exit_p(tx_isp_fs_remove),
	.driver = {
		.name = TX_ISP_FS_NAME,
		.owner = THIS_MODULE,
	},
};
