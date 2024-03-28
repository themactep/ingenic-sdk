/*
 * Linux/sound/oss/codec/
 *
 * Copyright (c) Ingenic Semiconductor Co., Ltd.
 */

#ifndef __TX_CODEC_H__
#define __TX_CODEC_H__

#include <linux/gpio.h>
#include <linux/bitops.h>
#include <codec-common.h>

#define BASE_ADDR_CODEC			0x10021000

#define TS_CODEC_CGR_00			0x00
#define TS_CODEC_CACR_02		0x08
#define TS_CODEC_CACR2_03       0x0c
#define TS_CODEC_CDCR1_04       0x10
#define TS_CODEC_CDCR2_05       0x14
#define TS_CODEC_CDCR2_06       0x18
#define TS_CODEC_CAVR_08        0x20
#define TS_CODEC_CGAINLR_09     0x24
#define TS_CODEC_CGAINLR_0a     0x28
#define TS_CODEC_POWER_20       0x80
#define TS_CODEC_CCR_21         0x84
#define TS_CODEC_CAACR_22       0x88
#define TS_CODEC_CMICCR_23      0x8c
#define TS_CODEC_CMICGAINR_24   0x90
#define TS_CODEC_CALCGR_25      0x94
#define TS_CODEC_CANACR_26      0x98
#define TS_CODEC_CANACR2_27     0x9c
#define TS_CODEC_CHR_28         0xA0
#define TS_CODEC_CHR_29         0xA4
#define TS_CODEC_CHR_2a         0xA8
#define TS_CODEC_CHR_2b         0xAC
#define TS_CODEC_CMR_40			0x100
#define TS_CODEC_CTR_41			0x104
#define TS_CODEC_CAGCCR_42		0x108
#define TS_CODEC_CPGR_43		0x10c
#define TS_CODEC_CSRR_44		0x110
#define TS_CODEC_CALMR_45		0x114
#define TS_CODEC_CAHMR_46		0x118
#define TS_CODEC_CALMINR_47		0x11c
#define TS_CODEC_CAHMINR_48		0x120
#define TS_CODEC_CAFG_49		0x124
#define TS_CODEC_CCAGVR_4c		0x130
#define TS_CODEC_CMR_50			0x140
#define TS_CODEC_CTR_51			0x144
#define TS_CODEC_CAGCCR_52		0x148
#define TS_CODEC_CPGR_53		0x14c
#define TS_CODEC_CSRR_54		0x150
#define TS_CODEC_CALMR_55		0x154
#define TS_CODEC_CAHMR_56		0x158
#define TS_CODEC_CALMINR_57		0x15c
#define TS_CODEC_CAHMINR_58		0x160
#define TS_CODEC_CAFG_59		0x164
#define TS_CODEC_CCAGVR_5c		0x170

#define ADC_VALID_DATA_LEN_16BIT	0x0
#define ADC_VALID_DATA_LEN_20BIT	0x1
#define ADC_VALID_DATA_LEN_24BIT	0x2
#define ADC_VALID_DATA_LEN_32BIT	0x3

#define ADC_I2S_INTERFACE_RJ_MODE	0x0
#define ADC_I2S_INTERFACE_LJ_MODE	0x1
#define ADC_I2S_INTERFACE_I2S_MODE	0x2
#define ADC_I2S_INTERFACE_PCM_MODE	0x3

#define CHOOSE_ADC_CHN_STEREO		0x0
#define CHOOSE_ADC_CHN_MONO			0x1

#define SAMPLE_RATE_8K		0x7
#define SAMPLE_RATE_12K		0x6
#define SAMPLE_RATE_16K		0x5
#define SAMPLE_RATE_24K		0x4
#define SAMPLE_RATE_32K		0x3
#define SAMPLE_RATE_44_1K	0x2
#define SAMPLE_RATE_48K		0x1
#define SAMPLE_RATE_96K		0x0

struct codec_device {
	struct resource *res;
	void __iomem *iomem;
	struct device *dev;
	struct codec_attributes *attr;
	struct audio_data_type record_type;
	struct audio_data_type playback_type;
	struct codec_spk_gpio spk_en;
	void *priv;
};
#endif /*end __T10_CODEC_H__*/
