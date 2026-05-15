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

#define BASE_ADDR_CODEC				0x10021000

#define CODEC_GLB_BASE_00			(0x0000 << 2)
#define CODEC_ADC_CTL_ADDR_02			(0x0002 << 2)
#define CODEC_MODE_CTL_ADDR_03			(0x0003 << 2)
#define CODEC_DAC_CTL1_ADDR_04			(0x0004 << 2)
#define CODEC_DAC_CTL2_ADDR_05			(0x0005 << 2)
#define CODEC_DAC_VOL_ADDR_06			(0x0006 << 2)
#define CODEC_ADC_DAC_ADDR_07			(0x0007 << 2)
#define CODEC_ADC_VOL_ADDR_08			(0x0008 << 2)
#define CODEC_GAIN_ADDR_0a			(0x000a << 2)
#define CODEC_AUDIO_SRC_ADDR_0d			(0x000d << 2)
#define CODEC_DATA_PATH_ADDR_0e			(0x000e << 2)
#define CODEC_DAC_D3_CH_ADDR_0f			(0x000f << 2)
#define CODEC_DAC_D2_CH_ADDR_10			(0x0010 << 2)
#define CODEC_DAC_D1_CH_ADDR_11			(0x0011 << 2)
#define CODEC_DAC_POWER_ADDR_20			(0x0020 << 2)
#define CODEC_PRECHARGE_ADDR_21			(0x0021 << 2)
#define CODEC_ADC_ANL_ADDR_22			(0x0022 << 2)
#define CODEC_MIC_CTL_ADDR_23			(0x0023 << 2)
#define CODEC_ALC_CTL1_ADDR_24			(0x0024 << 2)
#define CODEC_ALC_CTL2_ADDR_25			(0x0025 << 2)
#define CODEC_ADC_SIGNAL_ADDR_26        	(0x0026 << 2)
#define CODEC_AMP_CTL_ADDR_27			(0x0027 << 2)
#define CODEC_AUDIO_ADDR_28			(0x0028 << 2)
#define CODEC_DAC_CLK_ADDR_29			(0x0029 << 2)
#define CODEC_HPOUTL_GAIN_ADDR_2a		(0x002a << 2)
#define CODEC_HPOUTR_GAIN_ADDR_2b		(0x002b << 2)
#define CODEC_HPOUT_ADDR_2c			(0x002c << 2)
#define CODEC_HPOUT_CTL_ADDR_2d			(0x002d << 2)
#define CODEC_DAC_POP_ADDR_2e			(0x002e << 2)
#define CODEC_CHARGE_ADDR_2f			(0x002f << 2)
#define CODEC_METHOD_ADDR_40			(0x0040 << 2)
#define CODEC_TIME_ADDR_41			(0x0041 << 2)
#define CODEC_AGC_CTL_ADDR_42			(0x0042 << 2)
#define CODEC_PGA_GAIN_ADDR_43			(0x0043 << 2)
#define CODEC_SAMPLE_RATE_ADDR_44		(0x0044 << 2)
#define CODEC_AGC_LMAX_ADDR_45			(0x0045 << 2)
#define CODEC_AGC_HMAX_ADDR_46			(0x0046 << 2)
#define CODEC_AGC_LMIN_ADDR_47			(0x0047 << 2)
#define CODEC_AGC_HMIN_ADDR_48			(0x0048 << 2)
#define CODEC_AGC_FUNC_ADDR_49			(0x0049 << 2)
#define CODEC_GAIN_VALUE_ADDR_4c		(0x004c << 2)
#define CODEC_GAIN_VALUE_ADDR_4a		(0x004a << 2)

#define ADC_VALID_DATA_LEN_16BIT	0x0
#define ADC_VALID_DATA_LEN_20BIT	0x1
#define ADC_VALID_DATA_LEN_24BIT	0x2
#define ADC_VALID_DATA_LEN_32BIT	0x3

#define ADC_I2S_INTERFACE_RJ_MODE	0x0
#define ADC_I2S_INTERFACE_LJ_MODE	0x1
#define ADC_I2S_INTERFACE_I2S_MODE	0x2
#define ADC_I2S_INTERFACE_PCM_MODE	0x3

#define CHOOSE_ADC_CHN_STEREO		0x0
#define CHOOSE_ADC_CHN_MONO		0x1

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
