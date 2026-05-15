#ifndef __AS_DMIC_H__
#define __AS_DMIC_H__

#include <linux/bitops.h>
struct dmic_device_info {
	char name[20];
	unsigned long record_rate;
	int record_channel;
	int record_format;
	unsigned int gain;
	unsigned int record_trigger;
};
#ifdef CONFIG_KERNEL_4_4_94
struct jz_gpio_func_def {
	char *name;
	int port;
	int func;
	unsigned long pins;
};
#endif
struct ingenic_dmic {
	struct device   *dev;
	spinlock_t pipe_lock;

	struct regmap *regmap;
	void __iomem    *io_base;
	struct resource *io_res;

	struct clk *clk;
	struct clk *clk_gate;
	struct audio_pipe *pipe;
	struct dmic_device_info *info;
	struct jz_gpio_func_def dmic_gpio_func;

};

#define DMIC_DMA_MAX_BURSTSIZE 128
#define SND_DMIC_DMA_BUFFER_SIZE (48000*2*4)
#define DEFAULT_SAMPLERATE 0
#define DEFAULT_CHANNEL 0
#define DEFAULT_FORMAT 16
#define DEFAULT_GAIN 4
#define DEFAULT_TRIGGER 32

#define MAX_GAIN 0x1f
#define DMA5_ID 5
#define DMIC_DEV_ID 0

enum dmic_rate {
	DMIC_RATE_8000 = 8000,
	DMIC_RATE_16000 = 16000,
	DMIC_RATE_48000 = 48000,
};

#define DMIC_CR0	(0x0)
#define DMIC_GCR	(0x4)
#define DMIC_IER	(0x8)
#define DMIC_ICR	(0xc)
#define DMIC_TCR	(0x10)

/* DMIC_CR0 */
#define DMIC_RESET		BIT(31)
#define HPF2_EN_SFT		(22)
#define HPF2_EN			BIT(22)
#define LPF_EN_SFT		(21)
#define LPF_EN			BIT(21)
#define HPF1_EN_SFT		(20)
#define HPF1_EN			BIT(20)
#define CHNUM_SFT		(16)
#define CHNUM_MASK		GENMASK(19, CHNUM_SFT)
#define DMIC_SET_CHL(chl)	(((chl) - 1) << CHNUM_SFT)
#define OSS_SFT			(12)
#define OSS_MASK		GENMASK(13, OSS_SFT)
#define OSS_16BIT		(0x0 << OSS_SFT)
#define OSS_24BIT		(0x1 << OSS_SFT)
#define DMIC_SET_OSS(x) ((x == 16) ? OSS_16BIT : (x == 24) ? OSS_24BIT : OSS_16BIT)
#define SW_LR_SFT		(11)
#define SW_LR			BIT(11)
#define SR_SFT			(6)
#define SR_MASK			GENMASK(8, SR_SFT)
#define SR_8k			(0x0 << SR_SFT)
#define SR_16k			(0x1 << SR_SFT)
#define SR_48k			(0x2 << SR_SFT)
#define SR_96k			(0x3 << SR_SFT)
#define DMIC_SET_SR(r)	((r) == 8000 ? SR_8k : (r == 16000) ? SR_16k : (r == 48000) ? SR_48k : (r == 96000) ? SR_96k : SR_48k)
#define DMIC_EN			BIT(0)

/* DMIC_GCR */
#define DGAIN_SFT		(0)
#define DMIC_SET_GAIN(x)	((x) << DGAIN_SFT)

#endif //__AS_DMIC_H__
