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

#define BASE_ADDR_CODEC         0x10021000

#define CODEC_CDGR		0x00
#define CODEC_CDDACR1		0x04
#define CODEC_CDDACR2		0x08
#define CODEC_CDDACR3		0x0C
#define CODEC_CDDACR4		0x10
#define CODEC_CDDACR5		0x14
#define CODEC_CDDACR6		0x18
#define CODEC_CDADCR1		0x24
#define CODEC_CDADCR2		0x28
#define CODEC_CDADCR3		0x2C
#define CODEC_CDADCR4		0x30
#define CODEC_CDADCR5		0x34
#define CODEC_CAGR1		0x80
#define CODEC_CAGR2		0x84
#define CODEC_CAGR3		0x88
#define CODEC_CADACR1		0xA0
#define CODEC_CADACR2		0xA4
#define CODEC_CADACR3		0xA8
#define CODEC_CADACR4		0xAC
#define CODEC_CAADCR1		0xC0
#define CODEC_CAADCR2		0xC4
#define CODEC_CAADCR3		0xC8
#define CODEC_CAADCR4		0xCC
#define CODEC_CAGCR1		0x100
#define CODEC_CAGCR2		0x104
#define CODEC_CAGCR3		0x108
#define CODEC_CAGCR4		0x10C
#define CODEC_CAGCR5		0x114
#define CODEC_CAGCR6		0x118
#define CODEC_CAGCR7		0x11C
#define CODEC_CAGCR8		0x120
#define CODEC_CAGCR9		0x124
#define CODEC_CAGCR10		0x128
#define CODEC_CAGCR11		0x12C
#define CODEC_CAGCR12		0x130
#define CODEC_CAGCR13		0x134
#define CODEC_CAGCR14		0x138

#define ADC_VALID_DATA_LEN_16BIT    0x0
#define ADC_VALID_DATA_LEN_20BIT    0x1
#define ADC_VALID_DATA_LEN_24BIT    0x2
#define ADC_VALID_DATA_LEN_32BIT    0x3

#define ADC_I2S_INTERFACE_RJ_MODE   0x0
#define ADC_I2S_INTERFACE_LJ_MODE   0x1
#define ADC_I2S_INTERFACE_I2S_MODE  0x2
#define ADC_I2S_INTERFACE_PCM_MODE  0x3

#define CHOOSE_ADC_CHN_STEREO       0x0
#define CHOOSE_ADC_CHN_MONO         0x1

#define SAMPLE_RATE_8K      0x7
#define SAMPLE_RATE_12K     0x6
#define SAMPLE_RATE_16K     0x5
#define SAMPLE_RATE_24K     0x4
#define SAMPLE_RATE_32K     0x3
#define SAMPLE_RATE_44_1K   0x2
#define SAMPLE_RATE_48K     0x1
#define SAMPLE_RATE_96K     0x0

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

#endif /*end __PRJ007_CODEC_H__*/
