/*
 *  Video for ingenic tisp header file
 *
 */
#ifndef __TISP_VIDEODEV_H__
#define __TISP_VIDEODEV_H__


#define VIDEO_MAX_PLANES               8

/*  Flags for 'flags' field */
#define TISP_BUF_FLAG_MAPPED	0x0001  /* Buffer is mapped (flag) */
#define TISP_BUF_FLAG_QUEUED	0x0002	/* Buffer is queued for processing */
#define TISP_BUF_FLAG_DONE	0x0004	/* Buffer is ready */
#define TISP_BUF_FLAG_KEYFRAME	0x0008	/* Image is a keyframe (I-frame) */
#define TISP_BUF_FLAG_PFRAME	0x0010	/* Image is a P-frame */
#define TISP_BUF_FLAG_BFRAME	0x0020	/* Image is a B-frame */
/* Buffer is ready, but the data contained within is corrupted. */
#define TISP_BUF_FLAG_ERROR	0x0040
#define TISP_BUF_FLAG_TIMECODE	0x0100	/* timecode field is valid */
#define TISP_BUF_FLAG_PREPARED	0x0400	/* Buffer is prepared for queuing */
/* Cache handling flags */
#define TISP_BUF_FLAG_NO_CACHE_INVALIDATE	0x0800
#define TISP_BUF_FLAG_NO_CACHE_CLEAN		0x1000
/* Timestamp type */
#define TISP_BUF_FLAG_TIMESTAMP_MASK		0xe000
#define TISP_BUF_FLAG_TIMESTAMP_UNKNOWN		0x0000
#define TISP_BUF_FLAG_TIMESTAMP_MONOTONIC	0x2000
#define TISP_BUF_FLAG_TIMESTAMP_COPY		0x4000


enum tisp_buf_type {
	TISP_BUF_TYPE_VIDEO_CAPTURE        = 1,
	TISP_BUF_TYPE_VIDEO_OUTPUT         = 2,
	TISP_BUF_TYPE_VIDEO_OVERLAY        = 3,
	TISP_BUF_TYPE_VBI_CAPTURE          = 4,
	TISP_BUF_TYPE_VBI_OUTPUT           = 5,
	TISP_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,
	TISP_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,
	TISP_BUF_TYPE_VIDEO_OUTPUT_OVERLAY = 8,
	TISP_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9,
	TISP_BUF_TYPE_VIDEO_OUTPUT_MPLANE  = 10,
	TISP_BUF_TYPE_SDR_CAPTURE 		   = 11,
	TISP_BUF_TYPE_SDR_OUTPUT  		   = 12,
	/* Deprecated, do not use */
	TISP_BUF_TYPE_PRIVATE              = 0x80,
};


enum tisp_memory {
	TISP_MEMORY_MMAP             = 1,
	TISP_MEMORY_USERPTR          = 2,
	TISP_MEMORY_OVERLAY          = 3,
	TISP_MEMORY_DMABUF           = 4,
};

enum tisp_colorspace {
	TISP_COLORSPACE_DEFAULT	      = 0,
	TISP_COLORSPACE_SMPTE170M     = 1,
	TISP_COLORSPACE_SMPTE240M     = 2,
	TISP_COLORSPACE_REC709	      = 3,
	TISP_COLORSPACE_BT878	      = 4,
	TISP_COLORSPACE_470_SYSTEM_M  = 5,
	TISP_COLORSPACE_470_SYSTEM_BG = 6,
	TISP_COLORSPACE_JPEG	      = 7,
	TISP_COLORSPACE_SRGB	      = 8,
	TISP_COLORSPACE_ADOBERGB      = 9,
	TISP_COLORSPACE_BT2020	      = 10,
	TISP_COLORSPACE_RAW	      = 11,
	TISP_COLORSPACE_DCI_P3	      = 12,
};

enum tisp_field {
	TISP_FIELD_ANY		 = 0,
	TISP_FIELD_NONE		 = 1,
	TISP_FIELD_TOP		 = 2,
	TISP_FIELD_BOTTOM	 = 3,
	TISP_FIELD_INTERLACED	 = 4,
	TISP_FIELD_SEQ_TB	 = 5,
	TISP_FIELD_SEQ_BT	 = 6,
	TISP_FIELD_ALTERNATE	 = 7,
	TISP_FIELD_INTERLACED_TB = 8,
	TISP_FIELD_INTERLACED_BT = 9,
};

struct tisp_requestbuffers {
	uint32_t			count;
	uint32_t		type;		/* enum tisp_buf_type */
	uint32_t			memory;		/* enum tisp_memory */
	uint32_t			reserved[2];
};

struct tisp_timecode {
	uint32_t	type;
	uint32_t	flags;
	uint8_t	frames;
	uint8_t	seconds;
	uint8_t	minutes;
	uint8_t	hours;
	uint8_t	userbits[4];
};

/**
 * struct tisp_plane - plane info for multi-planar buffers
 * @bytesused:		number of bytes occupied by data in the plane (payload)
 * @length:		size of this plane (NOT the payload) in bytes
 * @mem_offset:		when memory in the associated struct tisp_buffer is
 *			tisp_MEMORY_MMAP, equals the offset from the start of
 *			the device memory for this plane (or is a "cookie" that
 *			should be passed to mmap() called on the video node)
 * @userptr:		when memory is tisp_MEMORY_USERPTR, a userspace pointer
 *			pointing to this plane
 * @fd:			when memory is tisp_MEMORY_DMABUF, a userspace file
 *			descriptor associated with this plane
 * @data_offset:	offset in the plane to the start of data; usually 0,
 *			unless there is a header in front of the data
 *
 * Multi-planar buffers consist of one or more planes, e.g. an YCbCr buffer
 * with two planes can have one plane for Y, and another for interleaved CbCr
 * components. Each plane can reside in a separate memory buffer, or even in
 * a completely separate memory node (e.g. in embedded devices).
 */
struct tisp_plane {
	uint32_t			bytesused;
	uint32_t			length;
	union {
		uint32_t		mem_offset;
		unsigned long	userptr;
		__s32		fd;
	} m;
	uint32_t			data_offset;
	uint32_t			reserved[11];
};


/**
 * struct tisp_buffer - video buffer info
 * @index:	id number of the buffer
 * @type:	enum tisp_buf_type; buffer type (type == *_MPLANE for
 *		multiplanar buffers);
 * @bytesused:	number of bytes occupied by data in the buffer (payload);
 *		unused (set to 0) for multiplanar buffers
 * @flags:	buffer informational flags
 * @field:	enum tisp_field; field order of the image in the buffer
 * @timestamp:	frame timestamp
 * @timecode:	frame timecode
 * @sequence:	sequence count of this frame
 * @memory:	enum tisp_memory; the method, in which the actual video data is
 *		passed
 * @offset:	for non-multiplanar buffers with memory == tisp_MEMORY_MMAP;
 *		offset from the start of the device memory for this plane,
 *		(or a "cookie" that should be passed to mmap() as offset)
 * @userptr:	for non-multiplanar buffers with memory == tisp_MEMORY_USERPTR;
 *		a userspace pointer pointing to this buffer
 * @fd:		for non-multiplanar buffers with memory == tisp_MEMORY_DMABUF;
 *		a userspace file descriptor associated with this buffer
 * @planes:	for multiplanar buffers; userspace pointer to the array of plane
 *		info structs for this buffer
 * @length:	size in bytes of the buffer (NOT its payload) for single-plane
 *		buffers (when type != *_MPLANE); number of elements in the
 *		planes array for multi-plane buffers
 * @input:	input number from which the video data has has been captured
 *
 * Contains data exchanged by application and driver using one of the Streaming
 * I/O methods.
 */
struct tisp_buffer {
	uint32_t			index;
	uint32_t			type;
	uint32_t			bytesused;
	uint32_t			flags;
	uint32_t			field;
	struct timeval		timestamp;
	struct tisp_timecode	timecode;
	uint32_t			sequence;

	/* memory location */
	uint32_t			memory;
	union {
		uint32_t           offset;
		unsigned long   userptr;
		struct tisp_plane *planes;
		__s32		fd;
	} m;
	uint32_t			length;
	uint32_t			reserved2;
	uint32_t			reserved;
};

struct tisp_rect {
	__s32   left;
	__s32   top;
	__s32   width;
	__s32   height;
};

struct tisp_fract {
	uint32_t   numerator;
	uint32_t   denominator;
};

struct tisp_clip {
	struct tisp_rect        c;
	struct tisp_clip	__user *next;
};

struct tisp_window {
	struct tisp_rect        w;
	uint32_t			field;	 /* enum tisp_field */
	uint32_t			chromakey;
	struct tisp_clip	__user *clips;
	uint32_t			clipcount;
	void			__user *bitmap;
	uint8_t                    global_alpha;
};

struct tisp_vbi_format {
	uint32_t	sampling_rate;		/* in 1 Hz */
	uint32_t	offset;
	uint32_t	samples_per_line;
	uint32_t	sample_format;		/* tisp_PIX_FMT_* */
	__s32	start[2];
	uint32_t	count[2];
	uint32_t	flags;			/* tisp_VBI_* */
	uint32_t	reserved[2];		/* must be zero */
};

struct tisp_sliced_vbi_format {
	uint16_t   service_set;
	/* service_lines[0][...] specifies lines 0-23 (1-23 used) of the first field
	   service_lines[1][...] specifies lines 0-23 (1-23 used) of the second field
				 (equals frame lines 313-336 for 625 line video
				  standards, 263-286 for 525 line standards) */
	uint16_t   service_lines[2][24];
	uint32_t   io_size;
	uint32_t   reserved[2];            /* must be zero */
};

struct tisp_pix_format {
	uint32_t         		width;
	uint32_t			height;
	uint32_t			pixelformat;
	uint32_t			field;		/* enum tisp_field */
	uint32_t            	bytesperline;	/* for padding, zero if unused */
	uint32_t          		sizeimage;
	uint32_t			colorspace;	/* enum tisp_colorspace */
	uint32_t			priv;		/* private data, depends on pixelformat */
	uint32_t			flags;		/* format flags (V4L2_PIX_FMT_FLAG_*) */
	uint32_t			ycbcr_enc;	/* enum v4l2_ycbcr_encoding */
	uint32_t			quantization;	/* enum v4l2_quantization */
	uint32_t			xfer_func;	/* enum v4l2_xfer_func */
};

/**
 * struct tisp_plane_pix_format - additional, per-plane format definition
 * @sizeimage:		maximum size in bytes required for data, for which
 *			this plane will be used
 * @bytesperline:	distance in bytes between the leftmost pixels in two
 *			adjacent lines
 */
struct tisp_plane_pix_format {
	uint32_t		sizeimage;
	uint16_t		bytesperline;
	uint16_t		reserved[6];
} __attribute__ ((packed));


/**
 * struct tisp_pix_format_mplane - multiplanar format definition
 * @width:		image width in pixels
 * @height:		image height in pixels
 * @pixelformat:	little endian four character code (fourcc)
 * @field:		enum tisp_field; field order (for interlaced video)
 * @colorspace:		enum tisp_colorspace; supplemental to pixelformat
 * @plane_fmt:		per-plane information
 * @num_planes:		number of planes for this format
 */
struct tisp_pix_format_mplane {
	uint32_t				width;
	uint32_t				height;
	uint32_t				pixelformat;
	uint32_t				field;
	uint32_t				colorspace;

	struct tisp_plane_pix_format	plane_fmt[VIDEO_MAX_PLANES];
	uint8_t				num_planes;
	uint8_t				flags;
	uint8_t				ycbcr_enc;
	uint8_t				quantization;
	uint8_t				xfer_func;
	uint8_t				reserved[7];
} __attribute__ ((packed));


/**
 * struct tisp_format - stream data format
 * @type:	enum tisp_buf_type; type of the data stream
 * @pix:	definition of an image format
 * @pix_mp:	definition of a multiplanar image format
 * @win:	definition of an overlaid image
 * @vbi:	raw VBI capture or output parameters
 * @sliced:	sliced VBI capture or output parameters
 * @raw_data:	placeholder for future extensions and custom formats
 */
struct tisp_format {
	uint32_t	 type;
	union {
		struct tisp_pix_format		pix;     /* tisp_BUF_TYPE_VIDEO_CAPTURE */
		struct tisp_pix_format_mplane	pix_mp;  /* tisp_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
		struct tisp_window		win;     /* tisp_BUF_TYPE_VIDEO_OVERLAY */
		struct tisp_vbi_format		vbi;     /* tisp_BUF_TYPE_VBI_CAPTURE */
		struct tisp_sliced_vbi_format	sliced;  /* tisp_BUF_TYPE_SLICED_VBI_CAPTURE */
		uint8_t	raw_data[200];                   /* user-defined */
	} fmt;
};

struct tisp_input {
	uint32_t	     	index;		/*  Which input */
	uint8_t	     		name[32];		/*  Label */
	uint32_t	     	type;		/*  Type of input */
	uint32_t	     	audioset;		/*  Associated audios (bitfield) */
	uint32_t        	tuner;             /*  enum tisp_tuner_type */
	uint64_t  			std;
	uint32_t	     	status;
	uint32_t	     	capabilities;
	uint32_t	     	reserved[3];
};

struct tisp_control {
	uint32_t		     id;
	__s32		     value;
};

struct tisp_mbus_framefmt {
	uint32_t			width;
	uint32_t			height;
	uint32_t			code;
	uint32_t			field;
	uint32_t			colorspace;
	uint16_t			ycbcr_enc;
	uint16_t			quantization;
	uint16_t			xfer_func;
	uint16_t			reserved[11];
};

/* External v4l2 format info. */
#define TISP_I2C_REG_MAX		(150)
#define TISP_I2C_ADDR_16BIT		(0x0002)
#define TISP_I2C_DATA_16BIT		(0x0004)
#define TISP_SBUS_MASK_SAMPLE_8BITS	0x01
#define TISP_SBUS_MASK_SAMPLE_16BITS	0x02
#define TISP_SBUS_MASK_SAMPLE_32BITS	0x04
#define TISP_SBUS_MASK_ADDR_8BITS	0x08
#define TISP_SBUS_MASK_ADDR_16BITS	0x10
#define TISP_SBUS_MASK_ADDR_32BITS	0x20
#define TISP_SBUS_MASK_ADDR_STEP_16BITS 0x40
#define TISP_SBUS_MASK_ADDR_STEP_32BITS 0x80
#define TISP_SBUS_MASK_SAMPLE_SWAP_BYTES 0x100
#define TISP_SBUS_MASK_SAMPLE_SWAP_WORDS 0x200
#define TISP_SBUS_MASK_ADDR_SWAP_BYTES	0x400
#define TISP_SBUS_MASK_ADDR_SWAP_WORDS	0x800
#define TISP_SBUS_MASK_ADDR_SKIP	0x1000
#define TISP_SBUS_MASK_SPI_READ_MSB_SET 0x2000
#define TISP_SBUS_MASK_SPI_INVERSE_DATA 0x4000
#define TISP_SBUS_MASK_SPI_HALF_ADDR	0x8000
#define TISP_SBUS_MASK_SPI_LSB		0x10000

//RGBIR
enum output_mbus_fmt {
	TISP_VO_FMT_YUV_SEMIPLANAR_420 = 0, //NV12
	TISP_VO_FMT_YVU_SEMIPLANAR_420, //NV21
	TISP_VO_FMT_YUV_SEMIPLANAR_422,
	TISP_VO_FMT_YVU_SEMIPLANAR_422,
	TISP_VO_FMT_UVY_SEMIPLANAR_422,
	TISP_VO_FMT_VUY_SEMIPLANAR_422,

	TISP_VO_FMT_RAW8_1X8,
	TISP_VO_FMT_RAW16_1X16,
	TISP_VO_FMT_END,
};

enum input_mbus_fmt {
	TISP_VI_FMT_UYVY8_1X16 = 0x5000,
	TISP_VI_FMT_VYUY8_1X16,
	TISP_VI_FMT_YUYV8_1X16,
	TISP_VI_FMT_YVYU8_1X16,

	TISP_VI_FMT_UYVY8_2X8 = 0x5100,
	TISP_VI_FMT_VYUY8_2X8,
	TISP_VI_FMT_YUYV8_2X8,
	TISP_VI_FMT_YVYU8_2X8,

	TISP_VI_FMT_SBGGR8_1X8 = 0x5200,
	TISP_VI_FMT_SGBRG8_1X8,
	TISP_VI_FMT_SGRBG8_1X8,
	TISP_VI_FMT_SRGGB8_1X8,
	TISP_VI_FMT_SBGGR10_1X10,
	TISP_VI_FMT_SGBRG10_1X10,
	TISP_VI_FMT_SGRBG10_1X10,
	TISP_VI_FMT_SRGGB10_1X10,
	TISP_VI_FMT_SBGGR12_1X12,
	TISP_VI_FMT_SGBRG12_1X12,
	TISP_VI_FMT_SGRBG12_1X12,
	TISP_VI_FMT_SRGGB12_1X12,
	TISP_VI_FMT_SBGGR14_1X14,
	TISP_VI_FMT_SGBRG14_1X14,
	TISP_VI_FMT_SGRBG14_1X14,
	TISP_VI_FMT_SRGGB14_1X14,
	TISP_VI_FMT_SBGGR16_1X16,
	TISP_VI_FMT_SGBRG16_1X16,
	TISP_VI_FMT_SGRBG16_1X16,
	TISP_VI_FMT_SRGGB16_1X16,

	TISP_VI_FMT_SRGIB8_1X8 = 0x5300,
	TISP_VI_FMT_SBGIR8_1X8,
	TISP_VI_FMT_SRIGB8_1X8,
	TISP_VI_FMT_SBIGR8_1X8,
	TISP_VI_FMT_SGRBI8_1X8,
	TISP_VI_FMT_SGBRI8_1X8,
	TISP_VI_FMT_SIRBG8_1X8,
	TISP_VI_FMT_SIBRG8_1X8,
	TISP_VI_FMT_SRGGI8_1X8,
	TISP_VI_FMT_SBGGI8_1X8,
	TISP_VI_FMT_SGRIG8_1X8,
	TISP_VI_FMT_SGBIG8_1X8,
	TISP_VI_FMT_SGIRG8_1X8,
	TISP_VI_FMT_SGIBG8_1X8,
	TISP_VI_FMT_SIGGR8_1X8,
	TISP_VI_FMT_SIGGB8_1X8,
	TISP_VI_FMT_SRGIB10_1X10,
	TISP_VI_FMT_SBGIR10_1X10,
	TISP_VI_FMT_SRIGB10_1X10,
	TISP_VI_FMT_SBIGR10_1X10,
	TISP_VI_FMT_SGRBI10_1X10,
	TISP_VI_FMT_SGBRI10_1X10,
	TISP_VI_FMT_SIRBG10_1X10,
	TISP_VI_FMT_SIBRG10_1X10,
	TISP_VI_FMT_SRGGI10_1X10,
	TISP_VI_FMT_SBGGI10_1X10,
	TISP_VI_FMT_SGRIG10_1X10,
	TISP_VI_FMT_SGBIG10_1X10,
	TISP_VI_FMT_SGIRG10_1X10,
	TISP_VI_FMT_SGIBG10_1X10,
	TISP_VI_FMT_SIGGR10_1X10,
	TISP_VI_FMT_SIGGB10_1X10,
	TISP_VI_FMT_SRGIB12_1X12,
	TISP_VI_FMT_SBGIR12_1X12,
	TISP_VI_FMT_SRIGB12_1X12,
	TISP_VI_FMT_SBIGR12_1X12,
	TISP_VI_FMT_SGRBI12_1X12,
	TISP_VI_FMT_SGBRI12_1X12,
	TISP_VI_FMT_SIRBG12_1X12,
	TISP_VI_FMT_SIBRG12_1X12,
	TISP_VI_FMT_SRGGI12_1X12,
	TISP_VI_FMT_SBGGI12_1X12,
	TISP_VI_FMT_SGRIG12_1X12,
	TISP_VI_FMT_SGBIG12_1X12,
	TISP_VI_FMT_SGIRG12_1X12,
	TISP_VI_FMT_SGIBG12_1X12,
	TISP_VI_FMT_SIGGR12_1X12,
	TISP_VI_FMT_SIGGB12_1X12,
	TISP_VI_FMT_SRGIB14_1X14,
	TISP_VI_FMT_SBGIR14_1X14,
	TISP_VI_FMT_SRIGB14_1X14,
	TISP_VI_FMT_SBIGR14_1X14,
	TISP_VI_FMT_SGRBI14_1X14,
	TISP_VI_FMT_SGBRI14_1X14,
	TISP_VI_FMT_SIRBG14_1X14,
	TISP_VI_FMT_SIBRG14_1X14,
	TISP_VI_FMT_SRGGI14_1X14,
	TISP_VI_FMT_SBGGI14_1X14,
	TISP_VI_FMT_SGRIG14_1X14,
	TISP_VI_FMT_SGBIG14_1X14,
	TISP_VI_FMT_SGIRG14_1X14,
	TISP_VI_FMT_SGIBG14_1X14,
	TISP_VI_FMT_SIGGR14_1X14,
	TISP_VI_FMT_SIGGB14_1X14,
	TISP_VI_FMT_SRGIB16_1X16,
	TISP_VI_FMT_SBGIR16_1X16,
	TISP_VI_FMT_SRIGB16_1X16,
	TISP_VI_FMT_SBIGR16_1X16,
	TISP_VI_FMT_SGRBI16_1X16,
	TISP_VI_FMT_SGBRI16_1X16,
	TISP_VI_FMT_SIRBG16_1X16,
	TISP_VI_FMT_SIBRG16_1X16,
	TISP_VI_FMT_SRGGI16_1X16,
	TISP_VI_FMT_SBGGI16_1X16,
	TISP_VI_FMT_SGRIG16_1X16,
	TISP_VI_FMT_SGBIG16_1X16,
	TISP_VI_FMT_SGIRG16_1X16,
	TISP_VI_FMT_SGIBG16_1X16,
	TISP_VI_FMT_SIGGR16_1X16,
	TISP_VI_FMT_SIGGB16_1X16,
};

#define BASE_DEVICE_PRIVATE 	        0
#define BASE_TUNING_PRIVATE		50
#define BASE_FS_PRIVATE 		80

//device ioctl
#define TISP_VIDIOC_DRIVER_VERSION		 		_IOW('T', BASE_DEVICE_PRIVATE + 1, int)
#define TISP_VIDIOC_ENUMINPUT		  			_IOWR('T', BASE_DEVICE_PRIVATE + 2, int)
#define TISP_VIDIOC_G_INPUT         			        _IOW('T', BASE_DEVICE_PRIVATE + 3, struct tx_isp_initarg)
#define TISP_VIDIOC_S_INPUT					_IOWR('T', BASE_DEVICE_PRIVATE + 4, struct tx_isp_initarg)
#define TISP_VIDIOC_REGISTER_SENSOR				_IOW('T', BASE_DEVICE_PRIVATE + 5, struct tx_isp_sensor_register_info)
#define TISP_VIDIOC_RELEASE_SENSOR				_IOW('T', BASE_DEVICE_PRIVATE + 6, struct tx_isp_sensor_register_info)
#define TISP_VIDIOC_STREAMON					_IOW('T', BASE_DEVICE_PRIVATE + 7, struct tx_isp_initarg)
#define TISP_VIDIOC_STREAMOFF		 			_IOW('T', BASE_DEVICE_PRIVATE + 8, struct tx_isp_initarg)
#define TISP_VIDIOC_CREATE_SUBDEV_LINKS		 	        _IOW('T', BASE_DEVICE_PRIVATE + 9, struct tx_isp_initarg)
#define TISP_VIDIOC_DESTROY_SUBDEV_LINKS		        _IOW('T', BASE_DEVICE_PRIVATE + 10, struct tx_isp_initarg)
#define TISP_VIDIOC_LINKS_STREAMON				_IOWR('T',BASE_DEVICE_PRIVATE + 11, struct tx_isp_initarg)
#define TISP_VIDIOC_LINKS_STREAMOFF				_IOWR('T',BASE_DEVICE_PRIVATE + 12, struct tx_isp_initarg)
#define TISP_VIDIOC_SET_SENSOR_REGISTER			        _IOWR('T',BASE_DEVICE_PRIVATE + 13, struct tx_isp_dbg_register)
#define TISP_VIDIOC_GET_SENSOR_REGISTER		 	        _IOW('T', BASE_DEVICE_PRIVATE + 14, struct tx_isp_dbg_register)
#define TISP_VIDIOC_SET_MDNS_BUF_INFO	 		        _IOW('T', BASE_DEVICE_PRIVATE + 15, struct isp_buf_info)
#define TISP_VIDIOC_GET_MDNS_BUF_INFO			        _IOW('T', BASE_DEVICE_PRIVATE + 16, struct isp_buf_info)
#define TISP_VIDIOC_SET_WDR_BUF_INFO			        _IOW('T', BASE_DEVICE_PRIVATE + 17, struct isp_buf_info)
#define TISP_VIDIOC_GET_WDR_BUF_INFO		 	        _IOW('T', BASE_DEVICE_PRIVATE + 18, struct isp_buf_info)
#define TISP_VIDIOC_ISP_WDR_ENABLE				_IOW('T', BASE_DEVICE_PRIVATE + 19, int)
#define TISP_VIDIOC_ISP_WDR_DISABLE		 		_IOW('T', BASE_DEVICE_PRIVATE + 20, int)
#define TISP_VIDIOC_SET_DUALSENSOR_MODE			        _IOWR('T',BASE_DEVICE_PRIVATE + 21, int)
#define TISP_VIDIOC_SET_DUALSENSOR_BUF_INFO		        _IOWR('T',BASE_DEVICE_PRIVATE + 22, struct isp_buf_info)
#define TISP_VIDIOC_GET_DUALSENSOR_BUF_INFO		        _IOW('T', BASE_DEVICE_PRIVATE + 23, struct isp_buf_info)
#define TISP_VIDIOC_BYPASS_SENSOR                               _IOW('T', BASE_DEVICE_PRIVATE + 24, struct tx_isp_initarg)

//tuning ioctl
#define TISP_VIDIOC_S_CTRL					_IOWR('T', BASE_TUNING_PRIVATE + 1, struct tisp_control)
#define TISP_VIDIOC_G_CTRL					_IOWR('T', BASE_TUNING_PRIVATE + 2, struct tisp_control)
#define TISP_VIDIOC_DEFAULT_CMD_ISP_TUNING		        _IOWR('T', BASE_TUNING_PRIVATE + 3, struct isp_image_tuning_default_ctrl)

//frame channel ioctl
#define TISP_VIDIOC_SET_FRAME_FORMAT		 	        _IOWR('T', BASE_FS_PRIVATE + 1, struct frame_image_format)
#define TISP_VIDIOC_GET_FRAME_FORMAT			        _IOWR('T', BASE_FS_PRIVATE + 2, struct frame_image_format)
#define TISP_VIDIOC_REQBUFS		 						_IOWR('T', BASE_FS_PRIVATE + 3, struct tisp_requestbuffers)
#define TISP_VIDIOC_QUERYBUF							_IOWR('T', BASE_FS_PRIVATE + 4, struct tisp_buffer)
#define TISP_VIDIOC_QBUF								_IOWR('T', BASE_FS_PRIVATE + 5,	struct tisp_buffer)
#define TISP_VIDIOC_DQBUF		 						_IOWR('T', BASE_FS_PRIVATE + 6,	struct tisp_buffer)
#define TISP_VIDIOC_FRAME_STREAMON						_IOWR('T', BASE_FS_PRIVATE + 7, int)
#define TISP_VIDIOC_FRAME_STREAMOFF		 				_IOWR('T', BASE_FS_PRIVATE + 8, int)
#define TISP_VIDIOC_DEFAULT_CMD_SET_BANKS		        _IOWR('T', BASE_FS_PRIVATE + 9, int)
#define TISP_VIDIOC_DEFAULT_CMD_LISTEN_BUF		        _IOWR('T', BASE_FS_PRIVATE + 10, int)
#endif /* __TISP_VIDEODEV_H__*/
