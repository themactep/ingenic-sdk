#ifndef __TX_ISP_FRAME_CHANNEL_H__
#define __TX_ISP_FRAME_CHANNEL_H__

#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <linux/proc_fs.h>

#include <tx-isp-common.h>

#define ISP_VIDEO_MAX_FRAME 64
/**
 * enum fs_vb2_buffer_state - current video buffer state
 * @FS_VB2_BUF_STATE_DEQUEUED:	buffer under userspace control
 * @FS_VB2_BUF_STATE_QUEUED:	buffer queued in videobuf, but not in driver
 * @FS_VB2_BUF_STATE_ACTIVE:	buffer queued in driver and possibly used
 *				in a hardware operation
 * @FS_VB2_BUF_STATE_DONE:		buffer returned from driver to videobuf, but
 *				not yet dequeued to userspace
 * @FS_VB2_BUF_STATE_ERROR:	same as above, but the operation on the buffer
 *				has ended with an error, which will be reported
 *				to the userspace when it is dequeued
 */
enum fs_vb2_buffer_state {
	FS_VB2_BUF_STATE_DEQUEUED,
	FS_VB2_BUF_STATE_QUEUED,
	FS_VB2_BUF_STATE_PREPARED,
	FS_VB2_BUF_STATE_ACTIVE,
	FS_VB2_BUF_STATE_DONE,
	FS_VB2_BUF_STATE_ERROR,
};

struct fs_vb2_queue;

/**
 * struct fs_vb2_buffer - represents a video buffer
 * @v4l2_buf:		struct v4l2_buffer associated with this buffer; can
 *			be read by the driver and relevant entries can be
 *			changed by the driver in case of CAPTURE types
 *			(such as timestamp)
 * @vb2_queue:		the queue to which this driver belongs
 * @format:		the format of buffer, it maybe be checked at driver
 * @state:		current buffer state; do not change
 * @queued_entry:	entry on the queued buffers list, which holds all
 *			buffers queued from userspace
 * @done_entry:		entry on the list that stores all buffers ready to
 *			be dequeued to userspace
 */
struct fs_vb2_buffer {
	struct v4l2_buffer	v4l2_buf;
	struct fs_vb2_queue	*vb2_queue;

	/* Private: internal use only */
	enum fs_vb2_buffer_state	state;

	struct list_head	queued_entry;
	struct list_head	done_entry;
};


/**
 * struct fs_vb2_queue - a videobuf queue
 *
 * @type:	queue type (see V4L2_BUF_TYPE_* in linux/videodev2.h
 * @lock:	pointer to a mutex that protects the vb2_queue struct. The
 *		driver can set this to a mutex to let the v4l2 core serialize
 *		the queuing ioctls. If the driver wants to handle locking
 *		itself, then this should be set to NULL. This lock is not used
 *		by the videobuf2 core API.
 * @ops:	driver-specific callbacks
 * @mem_ops:	memory allocator specific callbacks
 * @buf_struct_size: size of the driver-specific buffer structure;
 *		"0" indicates the driver doesn't want to use a custom buffer
 *		structure type, so sizeof(struct vb2_buffer) will is used
 *
 * @memory:	current memory type used
 * @bufs:	videobuf buffer structures
 * @num_buffers: number of allocated/used buffers
 * @queued_list: list of buffers currently queued from userspace
 * @queued_count: number of buffers currently queued from userspace
 * @done_list:	list of buffers ready to be dequeued to userspace
 * @done_lock:	lock to protect done_list list
 * @done_count: number of buffers be done by the driver
 * @done_wq:	waitqueue for processes waiting for buffers ready to be dequeued
 * @streaming:	current streaming state
 */
struct fs_vb2_queue {
	enum v4l2_buf_type		type;
	struct mutex			mlock;

	unsigned int	buf_struct_size;
	u32				timestamp_type;

	/* private: internal use only */
	enum v4l2_memory		memory;
	struct v4l2_format		format;
	struct fs_vb2_buffer	*bufs[ISP_VIDEO_MAX_FRAME];
	unsigned int			num_buffers;

	struct list_head		queued_list;
	unsigned int			queued_count;

	struct list_head		done_list;
	spinlock_t			done_lock;
	unsigned int			done_count;

	wait_queue_head_t		done_wq;

	unsigned int			streaming:1;
};

struct frame_channel_format {
	unsigned char name[32];
	unsigned int fourcc;
	unsigned int depth;
	unsigned int priv;
};

/*struct tx_isp_frame_channel_video_device;*/

/*typedef struct tx_isp_frame_channel_video_device frame_chan_vdev_t;*/

struct frame_channel_buffer {
	struct list_head entry;
	unsigned int addr;
	unsigned int priv;
};

struct frame_channel_video_buffer{
	struct fs_vb2_buffer vb;
	struct frame_channel_buffer buf;
};

struct tx_isp_frame_channel {
	struct miscdevice	misc;
	struct fs_vb2_queue vbq;
	struct frame_image_format fmt;
	char name[TX_ISP_NAME_LEN];
	struct tx_isp_subdev_pad *pad;
	int index;

	/* The follow parameters descript current status */
	spinlock_t slock;
	struct mutex mlock;
	int state;
	struct completion comp;
	unsigned int out_frames;
	unsigned int losed_frames;
	void *priv;
};

struct tx_isp_frame_sources {
	struct tx_isp_subdev sd;
	struct tx_isp_frame_channel *chans;
	int num_chans;
	int state;
};

#define vbq_to_frame_chan(n)	(container_of((n), struct tx_isp_frame_channel, vbq))
#define vb_to_video_buffer(n)	(container_of((n), struct frame_channel_video_buffer, vb))
#define miscdev_to_frame_chan(n)	(container_of((n), struct tx_isp_frame_channel, misc))
#define sd_to_frame_sources(n) (container_of((n), struct tx_isp_frame_sources, sd))
/**
 * isp_vb2_is_streaming() - return streaming status of the queue
 * @q:		videobuf queue
 */
static inline bool isp_vb2_is_streaming(struct vb2_queue *q)
{
	return q->streaming;
}

/**
 * isp_vb2_is_busy() - return busy status of the queue
 * @q:		videobuf queue
 *
 * This function checks if queue has any buffers allocated.
 */
static inline bool isp_vb2_is_busy(struct vb2_queue *q)
{
	return (q->num_buffers > 0);
}


#endif/*__TX_ISP_FRAME_CHANNEL_H__*/
