/*
 * codec.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <soc/gpio.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include "../../host/A1/audio_aic.h"
#include "../../include/audio_debug.h"
#include "codec.h"

static int forced_stereo =  0;
module_param(forced_stereo, int, S_IRUGO);
MODULE_PARM_DESC(forced_stereo, "Forced codec to stereo:0 disable, !0 enable");

static int left_spk_gpio = -1;
module_param(left_spk_gpio, int, S_IRUGO);
MODULE_PARM_DESC(left_spk_gpio, "Left speaker GPIO Num");

static int left_spk_level = 1;
module_param(left_spk_level, int, S_IRUGO);
MODULE_PARM_DESC(left_spk_level, "left speaker active level: -1 disable, 0 low active, 1 high active");

static int right_spk_gpio =  -1;//default disable
module_param(right_spk_gpio, int, S_IRUGO);
MODULE_PARM_DESC(right_spk_gpio, "Right speaker GPIO Num");

static int right_spk_level = 1;
module_param(right_spk_level, int, S_IRUGO);
MODULE_PARM_DESC(right_spk_level, "Right speaker active level: -1 disable, 0 low active, 1 high active");

static int left_mic_gain = 2;
module_param(left_mic_gain, int, S_IRUGO);
MODULE_PARM_DESC(left_mic_gain, "ADCL MIC gain: 0:0db 1:6db 2:20db 3:30db");

static int right_mic_gain = 0;
module_param(right_mic_gain, int, S_IRUGO);
MODULE_PARM_DESC(right_mic_gain, "ADCL MIC gain: 0:0db 1:6db 2:20db 3:30db");

static int bist_mode = 0;
module_param(bist_mode, int, S_IRUGO);
MODULE_PARM_DESC(bist_mode, "1:enable bist mode 0:disable bist mode");

struct codec_device *g_codec_dev = NULL;

static inline unsigned int codec_reg_read(struct codec_device * ope, int offset)
{
	return readl(ope->iomem + offset);
}

static inline void codec_reg_write(struct codec_device * ope, int offset, int data)
{
	writel(data, ope->iomem + offset);
}

int codec_reg_set(unsigned int reg, int start, int end, int val)
{
	int ret = 0;
	int i = 0, mask = 0;
	unsigned int oldv = 0;
	unsigned int new = 0;
	if(reg == CODEC_PRECHARGE_ADDR_21){
		msleep(20);
	}else{
		msleep(1);
	}

	for (i = 0;  i < (end - start + 1); i++) {
		mask += (1 << (start + i));
	}

	oldv = codec_reg_read(g_codec_dev, reg);
	new = oldv & (~mask);
	new |= val << start;
	codec_reg_write(g_codec_dev, reg, new);

	if ((new & 0x000000FF) != codec_reg_read(g_codec_dev, reg)) {
		printk("%s(%d):codec write  0x%08x error!!\n",__func__, __LINE__ ,reg);
		printk("new = 0x%08x, read val = 0x%08x\n", new, codec_reg_read(g_codec_dev, reg));
		return -1;
	}

	return ret;
}

int dump_codec_regs(void)
{
	int i = 0;
	printk("\nDump Codec register:\n");
	for (i = 0; i < 0x170; i += 4) {
		printk("reg = 0x%04x [0x%04x]:		val = 0x%04x\n", i, i/4, codec_reg_read(g_codec_dev, i));
	}
	printk("Dump T31 Codec register End.\n");

	return i;
}

/* ADC left enable configuration */
static int codec_enable_left_record(void)
{
	/* select ADC mono mode */
	codec_reg_set(CODEC_ADC_CTL_ADDR_02, 0, 0, 1);//mono
	/* configure ADC I2S interface mode */
	codec_reg_set(CODEC_ADC_CTL_ADDR_02, 3, 4, 0x2);
	codec_reg_set(CODEC_MODE_CTL_ADDR_03, 0, 5, 0x3e);
	//ADC的偏置电流
	codec_reg_set(CODEC_ALC_CTL1_ADDR_24, 0, 3, 7);
	msleep(10);
	/* 1. enable mute of left channel */
	codec_reg_set(CODEC_MIC_CTL_ADDR_23, 7, 7, 1);

	/* 2. enable the current of audio */
	codec_reg_set(CODEC_ADC_ANL_ADDR_22, 4, 4, 1);
	codec_reg_set(CODEC_ADC_ANL_ADDR_22, 3, 3, 1);
	codec_reg_set(CODEC_ADC_ANL_ADDR_22, 0, 2, 7);//micbase voltage(default 1.755v)
	/* 3. enable the reference voltage buffer in ADC left chennel */
	codec_reg_set(CODEC_MIC_CTL_ADDR_23, 5, 5, 1);

	/* 4. enable the MIC module in ADC left channel*/
	codec_reg_set(CODEC_ALC_CTL2_ADDR_25, 2, 2, 1);

	/* 5. enable the ALC module in ADC left channel*/
	codec_reg_set(CODEC_ALC_CTL2_ADDR_25, 3, 3, 1);

	/* 6. enable the clock module in ADC left channel*/
	codec_reg_set(CODEC_AUDIO_ADDR_28, 6, 6, 1);

	/* 7. enable the ADC module in ADC left channel*/
	codec_reg_set(CODEC_AUDIO_ADDR_28, 5, 5, 1);

	/* 8. end the initialization of the ADCL module*/
	codec_reg_set(CODEC_AUDIO_ADDR_28, 4, 4, 1);

	/* 9. end the initialization of the ALC left module*/
	codec_reg_set(CODEC_AUDIO_ADDR_28, 7, 7, 1);

	/* 10. end the initialization of the MIC left module*/
	codec_reg_set(CODEC_MIC_CTL_ADDR_23, 6, 6, 1);

	/* 11. select the gain of the MIC left module*/
	codec_reg_set(CODEC_ALC_CTL1_ADDR_24, 6, 7, left_mic_gain);//00:0dB,01:6dB,10:20dB,11:30dB

	/* 12. select the gain of the ALC left module*/
	codec_reg_set(CODEC_ADC_SIGNAL_ADDR_26, 0, 4, 0x10);//-18.5 ~ 28dB,step = 1.5dB

	/* 13. enable the zero-crossing detection function in ADC left channel*/
	//if enable zero-crossing detection function,will cause small voice at first.
	codec_reg_set(CODEC_MIC_CTL_ADDR_23, 4, 4, 0);//must set 0
	codec_reg_set(CODEC_GAIN_VALUE_ADDR_4a, 0, 7, 0xf);
	codec_reg_set(CODEC_GAIN_ADDR_0a, 2, 2, 0);//enable hpf for Avoid offset
	msleep(20);
	//ADC的偏置电流
//	codec_reg_set(CODEC_ALC_CTL1_ADDR_24, 0, 3, 3);
//	msleep(10);
	if(bist_mode == 1){//bist mode for debug
		codec_reg_set(CODEC_GLB_BASE_00, 7, 7, 1);
		codec_reg_set(CODEC_AUDIO_SRC_ADDR_0d, 4, 4, 1);
		codec_reg_set(CODEC_AUDIO_SRC_ADDR_0d, 5, 5, 1);
		printk("****bist mode:produce sine wave****\n");
	}
	return 0;
}

/* ADC right enable configuration */
static int codec_enable_right_record(void)
{
	return AUDIO_SUCCESS;
}

/* ADC stereo enable configuration */
static int codec_enable_stereo_record(void)
{
	return AUDIO_SUCCESS;
}

/* ADC left disable cofiguration */
static int codec_disable_left_record(void)
{
	/* 1. disable the zero-crossing detection func in ADC left channel*/
	codec_reg_set(CODEC_MIC_CTL_ADDR_23, 4, 4, 0);

	/* 2. disable the ADC module in ADC left channel*/
	codec_reg_set(CODEC_AUDIO_ADDR_28, 5, 5, 0);

	/* 3. disable the clock module in ADC left channel*/
	codec_reg_set(CODEC_AUDIO_ADDR_28, 6, 6, 0);

	/* 4. disable the ALC module in ADC left channel*/
	codec_reg_set(CODEC_ALC_CTL2_ADDR_25, 3, 3, 0);

	/* 5. disable the MIC module in ADC left channel*/
	codec_reg_set(CODEC_ALC_CTL2_ADDR_25, 2, 2, 0);

	/* 6. disable the reference voltage buffer in ADC left channel.*/
	codec_reg_set(CODEC_MIC_CTL_ADDR_23, 5, 5, 0);

	/* 7. disable the current source of audio*/
	codec_reg_set(CODEC_ADC_ANL_ADDR_22, 4, 4, 0);

	/* 8. begin the initialization of the ADC left module*/
	codec_reg_set(CODEC_AUDIO_ADDR_28, 4, 4, 0);

	/* 9. begin the initialization of the ALC left module*/
	codec_reg_set(CODEC_AUDIO_ADDR_28, 7, 7, 0);

	/* 10. begin the initialization of the MIC left module*/
	codec_reg_set(CODEC_MIC_CTL_ADDR_23, 6, 6, 0);

	return AUDIO_SUCCESS;
}

/* ADC right disable cofiguration */
static int codec_disable_right_record(void)
{
	return AUDIO_SUCCESS;
}

/* ADC stereo disable cofiguration */
static int codec_disable_stereo_record(void)
{
	return AUDIO_SUCCESS;
}

static int codec_enable_stereo_playback(void)
{
	/* DAC i2S interface */
	codec_reg_set(CODEC_MODE_CTL_ADDR_03, 6, 7, 0x3);
//	codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 0, 7, 0x10);
	codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 3, 4, 2);
	codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 0, 2, 0);
	codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 7, 7, 0);
	codec_reg_set(CODEC_DAC_CTL2_ADDR_05, 0, 7, 0xe);
	//DAC的偏置电流
	codec_reg_set(CODEC_DAC_POWER_ADDR_20, 0, 3, 0x05);
	msleep(10);
	/* 1. enable current source of DAC*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 7, 7, 1);

	/* 2. enable the reference voltage buffer of DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 6, 6, 1);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 6, 6, 1);

	/* 3. enable the reference voltage buffer of DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 4, 5, 2);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 4, 5, 2);

	/* 4. enable the HPDRV module in the DACL/DACR channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 4, 4, 1);
	codec_reg_set(CODEC_HPOUT_ADDR_2c, 4, 4, 1);

	/* 5. end the initialization of HPDRV module in the DACR/DACL channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 5, 5, 1);
	codec_reg_set(CODEC_HPOUT_ADDR_2c, 5, 5, 1);

	/* 6. enable the reference voltage buffer of DACR/DACL module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 3, 3, 1);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 3, 3, 1);

	/* 7. enable the clock module of DACR/DACL */
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 2, 2, 1);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 2, 2, 1);

	/* 8. enable DACR/DACL module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 1, 1, 1);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 1, 1, 1);

	/* 9. end the initialization of DAC module in the DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 0, 0, 1);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 0, 0, 1);

	/* 10. end the mute station of DRV module in the DACL/DACR module*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 6, 6, 1);
	codec_reg_set(CODEC_HPOUT_ADDR_2c, 6, 6, 1);

	/* 11. select the gain of HPDRV module in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTR_GAIN_ADDR_2b, 0, 4, 0x18);
	codec_reg_set(CODEC_HPOUT_CTL_ADDR_2d, 0, 4, 0x18);

	msleep(20);
	//DAC的偏置电流
//	codec_reg_set(CODEC_DAC_POWER_ADDR_20, 0, 3, 0x07);
//	msleep(10);
	if (g_codec_dev->spk_en.left_spk_gpio > 0 ) {
		gpio_direction_output(g_codec_dev->spk_en.left_spk_gpio, g_codec_dev->spk_en.left_spk_active_level);
	}
	if (g_codec_dev->spk_en.right_spk_gpio > 0) {
		gpio_direction_output(g_codec_dev->spk_en.right_spk_gpio, g_codec_dev->spk_en.right_spk_active_level);
	}

	return AUDIO_SUCCESS;
}

static int codec_enable_left_playback(void)
{
	/* DAC i2S interface */
	codec_reg_set(CODEC_MODE_CTL_ADDR_03, 6, 7, 0x3);
//	codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 0, 7, 0x10);
	codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 3, 4, 2);
	codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 0, 2, 0);
	codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 7, 7, 0);
	codec_reg_set(CODEC_DAC_CTL2_ADDR_05, 0, 7, 0xe);

	//DAC的偏置电流
	codec_reg_set(CODEC_DAC_POWER_ADDR_20, 0, 3, 0x05);
	msleep(10);
	/* 1. enable current source of DAC*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 7, 7, 1);

	/* 2. enable the reference voltage buffer of DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 6, 6, 1);

	/* 3. enable the reference voltage buffer of DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 4, 5, 2);

	/* 4. enable the HPDRV module in the DACL/DACR channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 4, 4, 1);

	/* 5. end the initialization of HPDRV module in the DACR/DACL channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 5, 5, 1);

	/* 6. enable the reference voltage buffer of DACR/DACL module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 3, 3, 1);

	/* 7. enable the clock module of DACR/DACL */
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 2, 2, 1);

	/* 8. enable DACR/DACL module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 1, 1, 1);

	/* 9. end the initialization of DAC module in the DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 0, 0, 1);

	/* 10. end the mute station of DRV module in the DACL/DACR module*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 6, 6, 1);

	/* 11. select the gain of HPDRV module in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTR_GAIN_ADDR_2b, 0, 4, 0x18);
	msleep(20);
	//DAC的偏置电流
//	codec_reg_set(CODEC_DAC_POWER_ADDR_20, 0, 3, 0x07);
//	msleep(10);
	if (g_codec_dev->spk_en.left_spk_gpio > 0) {
		gpio_direction_output(g_codec_dev->spk_en.left_spk_gpio, g_codec_dev->spk_en.left_spk_active_level);
	}
	return AUDIO_SUCCESS;
}

static int codec_enable_right_playback(void)
{
	return AUDIO_SUCCESS;
}

static int codec_disable_stereo_playback(void)
{
	if (g_codec_dev->spk_en.left_spk_gpio > 0 ) {
		gpio_direction_output(g_codec_dev->spk_en.left_spk_gpio,
				!g_codec_dev->spk_en.left_spk_active_level);
	}
	if(g_codec_dev->spk_en.right_spk_gpio > 0){
		gpio_direction_output(g_codec_dev->spk_en.right_spk_gpio,
				!g_codec_dev->spk_en.right_spk_active_level);
	}
	/* 1. select the gain of the HPDRV in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTR_GAIN_ADDR_2b, 0, 4, 0);
	codec_reg_set(CODEC_HPOUT_CTL_ADDR_2d, 0, 4, 0);

	/* 2. mute the HPDRV module in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 6, 6, 0);
	codec_reg_set(CODEC_HPOUT_ADDR_2c, 6, 6, 0);

	/* 3. begin the initialization of the HPDRV module in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 5, 5, 0);
	codec_reg_set(CODEC_HPOUT_ADDR_2c, 5, 5, 0);


	/* 4. disable the HPDRV module in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 4, 4, 0);
	codec_reg_set(CODEC_HPOUT_ADDR_2c, 4, 4, 0);

	/* 5. disable the DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 1, 1, 0);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 1, 1, 0);

	/* 6. disable the clock module of DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 2, 2, 0);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 2, 2, 0);

	/* 7. disable the reference voltage of DACL/DACR module.*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 3, 3, 0);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 3, 3, 0);

	/* 8. initialize the POP sound in the left/right DAC channel*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 4, 5, 1);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 4, 5, 1);

	/* 9. disable the reference voltage buffer of the left/right DAC channel*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 6, 6, 0);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 6, 6, 0);

	/* 10. disable the current source of DAC*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 7, 7, 0);

	/* 11. initialize the DAC module in the left/right DAC channel*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 0, 0, 0);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 0, 0, 0);

	msleep(10);
	return AUDIO_SUCCESS;
}

static int codec_disable_left_playback(void)
{
	if (g_codec_dev->spk_en.left_spk_gpio > 0) {
		gpio_direction_output(g_codec_dev->spk_en.left_spk_gpio,
				!g_codec_dev->spk_en.left_spk_active_level);
	}
	/* 1. select the gain of the HPDRV in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTR_GAIN_ADDR_2b, 0, 4, 0);

	/* 2. mute the HPDRV module in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 6, 6, 0);

	/* 3. begin the initialization of the HPDRV module in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 5, 5, 0);

	/* 4. disable the HPDRV module in the left/right DAC channel*/
	codec_reg_set(CODEC_HPOUTL_GAIN_ADDR_2a, 4, 4, 0);

	/* 5. disable the DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 1, 1, 0);

	/* 6. disable the clock module of DACL/DACR module*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 2, 2, 0);

	/* 7. disable the reference voltage of DACL/DACR module.*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 3, 3, 0);

	/* 8. initialize the POP sound in the left/right DAC channel*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 4, 5, 1);

	/* 9. disable the reference voltage buffer of the left/right DAC channel*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 6, 6, 0);

	/* 10. disable the current source of DAC*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 7, 7, 0);

	/* 11. initialize the DAC module in the left/right DAC channel*/
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 0, 0, 0);
	msleep(10);

	return AUDIO_SUCCESS;
}

static int codec_disable_right_playback(void)
{
	return AUDIO_SUCCESS;
}

static int codec_set_sample_rate(unsigned int rate)
{
	int i = 0;
	unsigned int mrate[8] = {8000, 12000, 16000, 24000, 32000, 44100, 48000, 96000};
	unsigned int rate_fs[8] = {7, 6, 5, 4, 3, 2, 1, 0};
	for (i = 0; i < 8; i++) {
		if (rate <= mrate[i])
			break;
	}
	if (i == 8)
		i = 0;
	codec_reg_set(CODEC_SAMPLE_RATE_ADDR_44, 0, 2, rate_fs[i]);

	return 0;
}

#define INCODEC_CHANNEL_MONO_LEFT 1
#define INCODEC_CHANNEL_MONO_RIGHT 2
#define INCODEC_CHANNEL_STEREO 3
#define INCODEC_CODEC_VALID_DATA_SIZE_16BIT 16
#define INCODEC_CODEC_VALID_DATA_SIZE_20BIT 20
#define INCODEC_CODEC_VALID_DATA_SIZE_24BIT 24

static int codec_record_set_datatype(struct audio_data_type *type)
{
	struct audio_data_type *this = NULL;
	int ret = 0;
	if(!type || !g_codec_dev)
		return -AUDIO_EPARAM;
	this = &g_codec_dev->record_type;

	if(this->frame_size != type->frame_size){
		printk("inner codec only support 32bit in 1/2 frame!\n");
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}
	if(type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_16BIT
			&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_20BIT
			&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_24BIT ){
		printk("inner codec ADC only support 16bit,20bit,24bit for record!\n");
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}
	if(type->sample_channel != INCODEC_CHANNEL_MONO_LEFT &&
			type->sample_channel != INCODEC_CHANNEL_MONO_RIGHT &&
			type->sample_channel != INCODEC_CHANNEL_STEREO){
		printk("inner codec only support mono or stereo for record!\n");
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}

	if(this->frame_vsize != type->frame_vsize){
		/* ADC valid data length */
		if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_16BIT)
			codec_reg_set(CODEC_ADC_CTL_ADDR_02, 5, 6, ADC_VALID_DATA_LEN_16BIT);
		else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_20BIT)
			codec_reg_set(CODEC_ADC_CTL_ADDR_02, 5, 6, ADC_VALID_DATA_LEN_20BIT);
		else
			codec_reg_set(CODEC_ADC_CTL_ADDR_02, 5, 6, ADC_VALID_DATA_LEN_24BIT);
		msleep(1);
		this->frame_vsize = type->frame_vsize;
	}

	if(this->sample_rate != type->sample_rate){
		this->sample_rate = type->sample_rate;
		ret |= codec_set_sample_rate(type->sample_rate);
	}

	if(this->sample_channel != type->sample_channel){
		/* Choose ADC chn */
		if(type->sample_channel == INCODEC_CHANNEL_MONO_LEFT ||
				type->sample_channel == INCODEC_CHANNEL_MONO_RIGHT)
			codec_reg_set(CODEC_ADC_CTL_ADDR_02, 0, 0, CHOOSE_ADC_CHN_MONO);
		else
			codec_reg_set(CODEC_ADC_CTL_ADDR_02, 0, 0, CHOOSE_ADC_CHN_STEREO);
		msleep(1);
		this->sample_channel = type->sample_channel;
		audio_info_print("%s %d: inner codec set record channel = %d\n",__func__ , __LINE__,type->sample_channel);
	}
	codec_reg_set(CODEC_GAIN_ADDR_0a, 2, 2, 1);//disable hpf for Avoid offset
ERR_PARAM:
	return ret;
}

static int codec_record_set_endpoint(int enable)
{
	int ret = 0;
	if(!g_codec_dev)
		return -AUDIO_EPERM;

	if(enable){
		if(g_codec_dev->record_type.sample_channel==STEREO)
			ret = codec_enable_stereo_record();
		else if(g_codec_dev->record_type.sample_channel==MONO_LEFT)
			ret = codec_enable_left_record();
		else if(g_codec_dev->record_type.sample_channel==MONO_RIGHT)
			ret = codec_enable_right_record();
	}else{
		if(g_codec_dev->record_type.sample_channel==STEREO)
			ret = codec_disable_stereo_record();
		else if(g_codec_dev->record_type.sample_channel==MONO_LEFT)
			ret = codec_disable_left_record();
		else if(g_codec_dev->record_type.sample_channel==MONO_RIGHT)
			ret = codec_disable_right_record();
	}
	return ret;
}

static int codec_record_set_again(int channel, int gain)
{
	/******************************************\
	| vol :0	1		2	..	30  31	       |
	| gain:-18  -16.5  -15	..	27  28.5 (+DB) |
	\******************************************/
	int analog_gain = 0;
	if(!g_codec_dev)	return -AUDIO_EPARAM;
	analog_gain = gain < 0 ? 0 : gain;
	analog_gain = gain > 0x1f ? 0x1f : gain;
	if(channel == MONO_LEFT){
		codec_reg_set(CODEC_AGC_FUNC_ADDR_49, 6, 6, 0);
		codec_reg_set(CODEC_GAIN_ADDR_0a, 5, 5, 0);
		codec_reg_set(CODEC_PGA_GAIN_ADDR_43, 5, 5, 0);
		codec_reg_set(CODEC_ADC_SIGNAL_ADDR_26, 0, 4, analog_gain);
	}else if(channel == MONO_RIGHT){
		/*A1 only support record MONO*/
		/*....*/
	}
	return 0;
}

static int codec_record_set_dgain(int channel, int gain)
{
	int digital_gain = 0;
	/******************************************\
	| vol :0	1		2	    0xc3..	0xff       |
	| gain:mute -97    -96.5	0    ..	30    (+DB) |
	\******************************************/
	digital_gain = gain < 0 ? 0 : gain;
	digital_gain = gain > 0xFF ? 0xFF : gain;
	if(channel == MONO_LEFT){
		codec_reg_set(CODEC_ADC_VOL_ADDR_08, 0, 7, digital_gain);
	}else if(channel == MONO_RIGHT){
		/*A1 only support record MONO*/
		/*....*/
	}
	return AUDIO_SUCCESS;
}

static int codec_record_set_hpf_enable(int en)
{
	codec_reg_set(CODEC_GAIN_ADDR_0a, 2, 2, !en);
	return AUDIO_SUCCESS;
}

static int codec_record_set_alc_threshold(int channel,int min_gain,int max_gain)
{
	if(channel == MONO_LEFT){
		codec_reg_set(CODEC_AGC_FUNC_ADDR_49, 6, 6, 1);
		codec_reg_set(CODEC_GAIN_ADDR_0a, 5, 5, 1);
		codec_reg_set(CODEC_AGC_FUNC_ADDR_49, 3, 5, min_gain);
		codec_reg_set(CODEC_AGC_FUNC_ADDR_49, 0, 2, max_gain);
	}else if(channel == MONO_RIGHT){
		/*A1 only support record MONO*/
		/*....*/
	}
	return AUDIO_SUCCESS;
}

static int codec_record_set_mute(int channel)
{
	if(channel == MONO_LEFT){
		codec_reg_set(CODEC_ADC_VOL_ADDR_08, 0, 7, 0);
	}else if (channel == MONO_RIGHT){
		/*A1 only support record MONO*/
		/*....*/
	}
	return AUDIO_SUCCESS;
}

static int codec_playback_set_datatype(struct audio_data_type *type)
{
	int ret = 0;
	struct audio_data_type *this = NULL;
	if(!type || !g_codec_dev)
		return -AUDIO_EPARAM;
	this = &g_codec_dev->playback_type;

	if(this->frame_size != type->frame_size){
		printk("inner codec only support 32bit in 1/2 frame!\n");
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}
	if(type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_16BIT
			&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_20BIT
			&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_24BIT ){
		printk("inner codec DAC only support 16bit,20bit,24bit for record!type->frame_vsize=%u\n", type->frame_vsize);
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}

	if(forced_stereo){
		type->sample_channel = INCODEC_CHANNEL_STEREO;
	}

	if(type->sample_channel != INCODEC_CHANNEL_MONO_LEFT && type->sample_channel != INCODEC_CHANNEL_STEREO){
		printk("inner codec only support stereo of mono_left hpout for playback!\n");
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}

	if(this->frame_vsize != type->frame_vsize){
		audio_info_print("type->frame_vsize=%u, we support %d %d %d\n",
				type->frame_vsize, INCODEC_CODEC_VALID_DATA_SIZE_16BIT,
				INCODEC_CODEC_VALID_DATA_SIZE_20BIT,
				INCODEC_CODEC_VALID_DATA_SIZE_24BIT);
		/* DAC valid data length */
		if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_16BIT)
			codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 5, 6, ADC_VALID_DATA_LEN_16BIT);
		else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_20BIT)
			codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 5, 6, ADC_VALID_DATA_LEN_20BIT);
		else
			codec_reg_set(CODEC_DAC_CTL1_ADDR_04, 5, 6, ADC_VALID_DATA_LEN_24BIT);
		msleep(10);
		this->frame_vsize = type->frame_vsize;
	}

	if(this->sample_rate != type->sample_rate){
		this->sample_rate = type->sample_rate;
		//ret |= codec_set_sample_rate(type->sample_rate);
	}

	this->sample_channel = type->sample_channel;

ERR_PARAM:
	return ret;
}

static int codec_playback_set_endpoint(int enable)
{
	int ret = 0;
	if(!g_codec_dev)
		return -AUDIO_EPERM;
	if(enable){
		if(g_codec_dev->playback_type.sample_channel==STEREO)
			ret = codec_enable_stereo_playback();
		else if(g_codec_dev->playback_type.sample_channel==MONO_LEFT)
			ret = codec_enable_left_playback();
		else if(g_codec_dev->playback_type.sample_channel==MONO_RIGHT)
			ret = codec_enable_right_playback();
	}else{
		if(g_codec_dev->playback_type.sample_channel==STEREO)
			ret = codec_disable_stereo_playback();
		else if(g_codec_dev->playback_type.sample_channel==MONO_LEFT)
			ret = codec_disable_left_playback();
		else if(g_codec_dev->playback_type.sample_channel==MONO_RIGHT)
			ret = codec_disable_right_playback();
	}
	return ret;
}

static int codec_playback_set_again(int channel ,int gain)
{
	int analog_gain = 0;
	if(!g_codec_dev)
		return -AUDIO_EPARAM;
	analog_gain = gain < 0 ? 0 : gain;
	analog_gain = gain > 0x1f ? 0x1f : gain;

	if(forced_stereo){
		codec_reg_set(CODEC_HPOUTR_GAIN_ADDR_2b, 0, 4, analog_gain);
		codec_reg_set(CODEC_HPOUT_CTL_ADDR_2d, 0, 4, analog_gain);
	}else{
		if (channel == MONO_LEFT){
			codec_reg_set(CODEC_HPOUTR_GAIN_ADDR_2b, 0, 4, analog_gain);
		}else if(channel == MONO_RIGHT){
			codec_reg_set(CODEC_HPOUT_CTL_ADDR_2d, 0, 4, analog_gain);
		}
	}
	return 0;
}

static int codec_playback_set_dgain(int channel, int gain)
{
	/******************************************\
	| vol :0	1		2	    0xf1..	0xff       |
	| gain:mute -120    -119.5	0    ..	7    (+DB) |
	\******************************************/
	int digital_gain = 0;
	if(!g_codec_dev)
		return -AUDIO_EPARAM;
	digital_gain = gain < 0 ? 0 : gain;
	digital_gain = gain > 0xFF ? 0xFF : gain;

	codec_reg_set(CODEC_DAC_VOL_ADDR_06, 0, 7, digital_gain);
	return AUDIO_SUCCESS;
}

static int codec_playback_set_mute(int channel)
{
	codec_reg_set(CODEC_DAC_VOL_ADDR_06, 0, 7, 0);
	return AUDIO_SUCCESS;
}

/* debug codec info */
static int inner_codec_show(struct seq_file *m, void *v)
{
	int len = 0;

	seq_printf(m ,"The name of codec is ingenic inner codec\n");

	dump_codec_regs();

	return len;
}

static int dump_inner_codec_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, inner_codec_show, PDE_DATA(inode), 2048);
}

static struct file_operations inner_codec_fops = {
	.read = seq_read,
	.open = dump_inner_codec_open,
	.llseek = seq_lseek,
	.release = single_release,
};

struct codec_sign_configure inner_codec_sign = {
	.data = DATA_I2S_MODE,
	.seq = LEFT_FIRST_MODE,
	.sync = LEFT_LOW_RIGHT_HIGH,
	.rate2mclk = 256,
};

static struct codec_endpoint codec_record = {
	.set_datatype = codec_record_set_datatype,
	.set_endpoint = codec_record_set_endpoint,
	.set_again = codec_record_set_again,
	.set_dgain = codec_record_set_dgain,
	.set_mic_hpf_en = codec_record_set_hpf_enable,
	.set_mute = codec_record_set_mute,
	.set_alc_threshold = codec_record_set_alc_threshold,
};

static struct codec_endpoint codec_playback = {
	.set_datatype = codec_playback_set_datatype,
	.set_endpoint = codec_playback_set_endpoint,
	.set_again = codec_playback_set_again,
	.set_dgain = codec_playback_set_dgain,
	.set_mute = codec_playback_set_mute,
};

static int codec_set_sign(struct codec_sign_configure *sign)
{
	struct codec_sign_configure *this = &inner_codec_sign;
	if(!sign)
		return -AUDIO_EPARAM;
	if(sign->data != this->data || sign->seq != this->seq || sign->sync != this->sync || sign->rate2mclk != this->rate2mclk)
		return -AUDIO_EPARAM;
	return AUDIO_SUCCESS;
}

int codec_init(void)
{
	int i = 0;
	unsigned char value = 0;

	/* step1. Supply the power of the digital part and reset the audio codec*/
	codec_reg_set(CODEC_GLB_BASE_00, 0, 1, 0);
	msleep(1);
	codec_reg_set(CODEC_GLB_BASE_00, 0, 1, 0x3);

	/* step2. setup dc voltags of DAC channel output */
	codec_reg_set(CODEC_DAC_CLK_ADDR_29, 4, 5, 0x1);
	codec_reg_set(CODEC_DAC_POP_ADDR_2e, 4, 5, 0x1);

	/* step3. no desc */
	codec_reg_set(CODEC_PRECHARGE_ADDR_21, 0, 7, 0x1);

	/* step4. setup reference voltage*/
	codec_reg_set(CODEC_ADC_ANL_ADDR_22, 5, 5, 0x1);
	for (i = 0; i <= 7; i++) {
		value |= value<<1 | 0x01;
		codec_reg_set(CODEC_PRECHARGE_ADDR_21, 0, 7, value);
	//	msleep(20);
	}
	msleep(20);
	//Set the min charge current for reduce power.
	codec_reg_set(CODEC_PRECHARGE_ADDR_21, 0, 7, 0x02);
	return 0;
}

int codec_deinit(void)
{
	int i = 0;
	unsigned char value = 0;
	/* step0.disable the DAC and ADC */
	codec_disable_stereo_record();
	codec_disable_stereo_playback();
	/* step1. no desc */
	codec_reg_set(CODEC_PRECHARGE_ADDR_21, 0, 7, 0x1);
	/* step2. setup reference voltage*/
	codec_reg_set(CODEC_ADC_ANL_ADDR_22, 5, 5, 0x1);
	for (i = 0; i <= 7; i++) {
		value |= value<<1 | 0x01;
		codec_reg_set(CODEC_PRECHARGE_ADDR_21, 0, 7, value);
		msleep(20);
	}
	return 0;
}

static int codec_set_power(int enable)
{
	if(enable){
		codec_init();
	}else{
//		codec_disable_stereo_record();
//		codec_disable_stereo_playback();
		codec_deinit();
	}
	return AUDIO_SUCCESS;
}

static struct codec_attributes inner_codec_attr = {
	.name = "inner_codec",
	.mode = CODEC_IS_MASTER_MODE,
	.type = CODEC_IS_INNER_MODULE,
	.pins = CODEC_IS_0_LINES,
	.set_sign = codec_set_sign,
	.set_power = codec_set_power,
	.record = &codec_record,
	.playback = &codec_playback,
	.debug_ops = &inner_codec_fops,
};

extern int inner_codec_register(struct codec_attributes *codec_attr);
static int codec_probe(struct platform_device *pdev)
{
	int ret = 0;
	g_codec_dev = kzalloc(sizeof(*g_codec_dev), GFP_KERNEL);
	if(!g_codec_dev) {
		printk("alloc codec device error\n");
		return -ENOMEM;
	}
	g_codec_dev->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!g_codec_dev->res) {
		dev_err(&pdev->dev, "failed to get dev resources\n");
		return -EINVAL;
	}
	g_codec_dev->res = request_mem_region(g_codec_dev->res->start,
			g_codec_dev->res->end - g_codec_dev->res->start + 1,
			pdev->name);
	if (!g_codec_dev->res) {
		dev_err(&pdev->dev, "failed to request regs memory region");
		return -EINVAL;
	}
	g_codec_dev->iomem = ioremap(g_codec_dev->res->start, resource_size(g_codec_dev->res));
	if (!g_codec_dev->iomem) {
		dev_err(&pdev->dev, "failed to remap regs memory region\n");
		return -EINVAL;
	}

	g_codec_dev->spk_en.left_spk_gpio = left_spk_gpio;
	g_codec_dev->spk_en.left_spk_active_level = left_spk_level;
	g_codec_dev->spk_en.right_spk_gpio = right_spk_gpio;
	g_codec_dev->spk_en.right_spk_active_level = right_spk_level;

	if (g_codec_dev->spk_en.left_spk_gpio != -1 ) {
		if (gpio_request(g_codec_dev->spk_en.left_spk_gpio,"left_spk_gpio_spk_en") < 0) {
			gpio_free(g_codec_dev->spk_en.left_spk_gpio);
			printk(KERN_DEBUG"request left spk en gpio %d error!\n", g_codec_dev->spk_en.left_spk_gpio);
			ret = -EPERM;
			goto failed_request_left_spk_gpio;
		}
		printk(KERN_DEBUG"request left spk en gpio %d ok!\n", g_codec_dev->spk_en.left_spk_gpio);
	}

	if (g_codec_dev->spk_en.right_spk_gpio != -1 ) {
		if (gpio_request(g_codec_dev->spk_en.right_spk_gpio,"gpio_spk_en") < 0) {
			gpio_free(g_codec_dev->spk_en.right_spk_gpio);
			printk(KERN_DEBUG"request spk en gpio %d error!\n", g_codec_dev->spk_en.right_spk_gpio);
			ret = -EPERM;
			goto failed_request_right_spk_gpio;
		}
		printk(KERN_DEBUG"request spk en gpio %d ok!\n", g_codec_dev->spk_en.right_spk_gpio);
	}

	platform_set_drvdata(pdev, &g_codec_dev);
	g_codec_dev->priv = pdev;
	g_codec_dev->attr = &inner_codec_attr;
	/*1/2 frame total size,only suppport 32bit*/
	g_codec_dev->record_type.frame_size = 32;
	g_codec_dev->playback_type.frame_size = 32;
	inner_codec_attr.host_priv = g_codec_dev;

	inner_codec_register(&inner_codec_attr);

	return ret;

failed_request_right_spk_gpio:
	gpio_free(g_codec_dev->spk_en.left_spk_gpio);
failed_request_left_spk_gpio:
	iounmap(g_codec_dev->iomem);
	release_mem_region(g_codec_dev->res->start,
			g_codec_dev->res->end - g_codec_dev->res->start + 1);
	kfree(g_codec_dev);
	g_codec_dev = NULL;
	return ret;
}

static int __exit codec_remove(struct platform_device *pdev)
{
	if(!g_codec_dev)
		return 0;
	iounmap(g_codec_dev->iomem);
	release_mem_region(g_codec_dev->res->start,
			g_codec_dev->res->end - g_codec_dev->res->start + 1);
	if (g_codec_dev->spk_en.left_spk_gpio > 0)
		gpio_free(g_codec_dev->spk_en.left_spk_gpio);
	if (g_codec_dev->spk_en.right_spk_gpio > 0)
		gpio_free(g_codec_dev->spk_en.right_spk_gpio);

	kfree(g_codec_dev);
	g_codec_dev = NULL;

	return 0;
}

struct platform_driver audio_codec_driver = {
	.probe = codec_probe,
	.remove = __exit_p(codec_remove),
	.driver = {
		.name = "jz-inner-codec",
		.owner = THIS_MODULE,
	},
};
