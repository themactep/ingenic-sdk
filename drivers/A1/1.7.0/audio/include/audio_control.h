#ifndef _JZ_AUDIO_CONTROL_H_
#define _JZ_AUDIO_CONTROL_H_

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <asm/irq.h>
#include <asm/io.h>
#include "tx-list-common.h"
#include "codec-common.h"

struct audio_control {
	/* */

	struct mutex mlock;
	spinlock_t slock;

	struct codec_attributes *excodec;
	struct codec_attributes *incodec;
	struct codec_attributes *livingcodec;
	void *priv;
};


#endif /* _JZ_AUDIO_CONTROL_H_ */
