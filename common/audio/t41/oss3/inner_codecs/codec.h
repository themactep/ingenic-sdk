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

#define CODEC_CGR         0x00
#define CODEC_CDCFGR      0x04
#define CODEC_CACR        0x08
#define CODEC_CACR1       0x0c
#define CODEC_CDCR        0x10
#define CODEC_CDCR1       0x14
#define CODEC_CDGSR       0x18
#define CODEC_CDLCBMSR    0x1c
#define CODEC_CLAVR       0x20
#define CODEC_CGAINR      0x28
#define CODEC_CDBCR       0x80
#define CODEC_CCR         0x84
#define CODEC_CAACR       0x88
#define CODEC_CMICCR      0x8c
#define CODEC_CMICGAINR   0x90
#define CODEC_CAEC      0x94
#define CODEC_CALCGR      0x98
#define CODEC_CANACR      0xA0
#define CODEC_CANACR1     0xA4
#define CODEC_CHR         0xA8
#define CODEC_CHPOUTLGR   0xAC
#define CODEC_CMR         0x100
#define CODEC_CTR         0x104
#define CODEC_CAGCCR      0x108
#define CODEC_CPGR        0x10c
#define CODEC_CSRR        0x110
#define CODEC_CALMAXR     0x114
#define CODEC_CAHMAXR     0x118
#define CODEC_CALMINR     0x11c
#define CODEC_CAHMINR     0x120
#define CODEC_CAFG        0x124

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

#endif /*end __T41_CODEC_H__*/
