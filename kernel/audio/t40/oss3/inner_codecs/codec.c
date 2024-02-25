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
#include "../host/audio_aic.h"
#include "../include/audio_debug.h"
#include "codec.h"

static int spk_gpio =  GPIO_PB(31);
module_param(spk_gpio, int, S_IRUGO);
MODULE_PARM_DESC(spk_gpio, "Speaker GPIO NUM");

static int spk_level = 1;
module_param(spk_level, int, S_IRUGO);
MODULE_PARM_DESC(spk_level, "Speaker active level: -1 disable, 0 low active, 1 high active");

static int left_mic_gain = 3;
module_param(left_mic_gain, int, S_IRUGO);
MODULE_PARM_DESC(left_mic_gain, "ADCL MIC gain: 0:0db 1:6db 2:20db 3:30db");

static int right_mic_gain = 3;
module_param(right_mic_gain, int, S_IRUGO);
MODULE_PARM_DESC(right_mic_gain, "ADCL MIC gain: 0:0db 1:6db 2:20db 3:30db");


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

	msleep(1);

	return ret;
}

int dump_codec_regs(void)
{
	int i = 0;
	printk("\nDump Codec register:\n");
#if 0
	int codec_reg_buf[] = {TS_CODEC_CGR_00,	   TS_CODEC_CACR_02,   TS_CODEC_CMCR_03,    TS_CODEC_CDCR1_04,
		TS_CODEC_CDCR2_05,  TS_CODEC_CADR_07,   TS_CODEC_CGAINR_0a,  TS_CODEC_CDPR_0e,
		TS_CODEC_CDDPR2_0f, TS_CODEC_CDDPR1_10, TS_CODEC_CDDPR0_11,  TS_CODEC_CAACR_21,
		TS_CODEC_CMICCR_22, TS_CODEC_CACR2_23,  TS_CODEC_CAMPCR_24,  TS_CODEC_CAR_25,
		TS_CODEC_CHR_26,    TS_CODEC_CHCR_27,   TS_CODEC_CCR_28,	    TS_CODEC_CMR_40,
		TS_CODEC_CTR_41,    TS_CODEC_CAGCCR_42, TS_CODEC_CPGR_43,    TS_CODEC_CSRR_44,
		TS_CODEC_CALMR_45,  TS_CODEC_CAHMR_46,  TS_CODEC_CALMINR_47, TS_CODEC_CAHMINR_48,
		TS_CODEC_CAFG_49,   TS_CODEC_CCAGVR_4c, 0xffff};
	for(i = 0; codec_reg_buf[i] != 0xffff; i++) {
		printk("reg = 0x%04x [0x%04x]:		val = 0x%04x\n", codec_reg_buf[i], codec_reg_buf[i]/4, codec_reg_read(g_codec_dev, codec_reg_buf[i]));
	}
#endif
	for (i = 0; i < 0x170; i += 4) {
		printk("reg = 0x%04x [0x%04x]:		val = 0x%04x\n", i, i/4, codec_reg_read(g_codec_dev, i));
	}
	printk("Dump T31 Codec register End.\n");

	return i;
}

int codec_init(void)
{
	int i = 0;
	char value = 0;

	/* step1. reset codec */
	codec_reg_set(TS_CODEC_CGR_00, 0, 1, 0);
	msleep(1);
	codec_reg_set(TS_CODEC_CGR_00, 0, 1, 0x3);
	/* step2. setup dc voltags of DAC channel output */
	codec_reg_set(TS_CODEC_CHR_2a, 0, 1, 0x1);
	codec_reg_set(TS_CODEC_CHR_2a, 4, 5, 0x1);
//	msleep(10);
	/* step3. no desc */
	codec_reg_set(TS_CODEC_CCR_21, 0, 7, 0x1);
//	msleep(10);
	codec_reg_set(TS_CODEC_CAACR_22, 5, 5, 0x1);//setup reference voltage
//	msleep(10);
	//codec_reg_set(TS_CODEC_CCR_21, 0, 7, 0xff);  //precharge

	for (i = 0; i <= 7; i++) {
		value |= value<<1 | 0x01;
		codec_reg_set(TS_CODEC_CCR_21, 0, 7, value);
		msleep(20);
	}
	msleep(20);
	codec_reg_set(TS_CODEC_CCR_21, 0, 7, 0x02);//Set the min charge current for reduce power.

	return 0;
}

/* ADC left enable configuration */
static int codec_enable_left_record(void)
{
	/* select ADC mono mode */
	codec_reg_set(TS_CODEC_CACR_02, 0, 0, 1);//mono

	/* configure ADC I2S interface mode */
	codec_reg_set(TS_CODEC_CACR_02, 3, 4, 0x2);
	codec_reg_set(TS_CODEC_CACR2_03, 0, 5, 0x3e);

	/* end mute station of ADC left channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 7, 7, 1);

	/* enable current source of audio */
	codec_reg_set(TS_CODEC_CAACR_22, 4, 4, 1);

	/* bias current:reduce power,max is 7*/
	codec_reg_set(TS_CODEC_CMICGAINR_24, 0, 3, 7);

	codec_reg_set(TS_CODEC_CAACR_22, 3, 3, 1);
	codec_reg_set(TS_CODEC_CAACR_22, 0, 2, 0);

	/* enable reference voltage buffer in ADC left channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 5, 5, 1);

	/* enable MIC module in ADC left channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 2, 2, 1);

	/* enable ALC module in ADC left channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 3, 3, 1);

	/* enable clock module in ADC left channel */
	codec_reg_set(TS_CODEC_CHR_28, 6, 6, 1);

	/* enable ADC module in ADC left channel */
	codec_reg_set(TS_CODEC_CHR_28, 5, 5, 1);

	/* end initialization of ADCL module */
	codec_reg_set(TS_CODEC_CHR_28, 4, 4, 1);

	/* end initialization of ALC left module */
	codec_reg_set(TS_CODEC_CHR_28, 7, 7, 1);

	/* end initialization of mic left module */
	codec_reg_set(TS_CODEC_CMICCR_23, 6, 6, 1);

	/* set default mic gain of left module */
	codec_reg_set(TS_CODEC_CMICGAINR_24, 6, 7, left_mic_gain);

	/* set default codec gain of left module */
	codec_reg_set(TS_CODEC_CANACR_26, 0, 4, 0x10);

	/* enable zero-crossing detection function in ADC left channel */
	//if enable zero-crossing detection function,will cause small voice at first.
	codec_reg_set(TS_CODEC_CMICCR_23, 4, 4, 0);
	msleep(20);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 0, 3, 3);
	msleep(10);
	return 0;
}

/* ADC right enable configuration */
static int codec_enable_right_record(void)
{
#if 0
	/* select ADC mono mode */
	codec_reg_set(TS_CODEC_CACR_02, 0, 0, 1);

	/* configure ADC I2S interface mode */
	codec_reg_set(TS_CODEC_CACR_02, 3, 4, 0x2);
	codec_reg_set(TS_CODEC_CACR_02, 1, 1, 1);
	codec_reg_set(TS_CODEC_CACR2_03, 0, 5, 0x3e);

	/* end mute station of ADC right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 3, 3, 1);

	/* enable current source of audio */
	codec_reg_set(TS_CODEC_CAACR_22, 4, 4, 1);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 0, 3, 0x07);

	codec_reg_set(TS_CODEC_CAACR_22, 3, 3, 1);
	codec_reg_set(TS_CODEC_CAACR_22, 0, 2, 7);

	/* enable reference voltage buffer in ADC left and right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 1, 1, 1);

	/* enable MIC module in ADC right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 0, 0, 1);
	codec_reg_set(TS_CODEC_CALCGR_25, 2, 2, 1);
	codec_reg_set(TS_CODEC_CALCGR_25, 4, 4, 1);

	/* enable ALC module in ADC right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 1, 1, 1);
	codec_reg_set(TS_CODEC_CALCGR_25, 3, 3, 1);
	codec_reg_set(TS_CODEC_CALCGR_25, 5, 5, 1);

	/* enable clock module in ADC right channel */
	codec_reg_set(TS_CODEC_CHR_28, 2, 2, 1);

	/* enable ADC module in ADC right channel */
	codec_reg_set(TS_CODEC_CHR_28, 1, 1, 1);

	/* end initialization of ADCR module */
	codec_reg_set(TS_CODEC_CHR_28, 0, 0, 1);

	/* end initialization of ALC right module */
	codec_reg_set(TS_CODEC_CHR_28, 3, 3, 1);

	/* end initialization of mic right module */
	codec_reg_set(TS_CODEC_CMICCR_23, 2, 2, 1);

	/* set default mic gain of right module */
	codec_reg_set(TS_CODEC_CMICGAINR_24, 4, 5, right_mic_gain);

	/* set default codec gain of right module */
	codec_reg_set(TS_CODEC_CANACR2_27, 0, 4, 0x10);

	/* enable zero-crossing detection function in ADC right channel */
	//if enable zero-crossing detection function,will cause small voice at first.
	codec_reg_set(TS_CODEC_CMICCR_23, 0, 0, 0);
	msleep(1);
#else
	/* select ADC stereo mode */
	codec_reg_set(TS_CODEC_CACR_02, 0, 0, 0);

	/* configure ADC I2S interface mode */
	codec_reg_set(TS_CODEC_CACR_02, 3, 4, 0x2);
	codec_reg_set(TS_CODEC_CACR2_03, 0, 5, 0x3e);

	/* end mute station of ADC left and right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 7, 7, 1);
	codec_reg_set(TS_CODEC_CMICCR_23, 3, 3, 1);

	/* enable current source of audio */
	codec_reg_set(TS_CODEC_CAACR_22, 4, 4, 1);
	/* bias current:reduce power,max is 7*/
	codec_reg_set(TS_CODEC_CMICGAINR_24, 0, 3, 7);//add 10:57

	codec_reg_set(TS_CODEC_CAACR_22, 3, 3, 1);
	codec_reg_set(TS_CODEC_CAACR_22, 0, 2, 0);

	/* enable reference voltage buffer in ADC left and right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 5, 5, 1);
	codec_reg_set(TS_CODEC_CMICCR_23, 1, 1, 1);

	/* enable MIC module in ADC left and right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 0, 0, 1);
	codec_reg_set(TS_CODEC_CALCGR_25, 2, 2, 1);

	/* enable ALC module in ADC left and right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 1, 1, 1);
	codec_reg_set(TS_CODEC_CALCGR_25, 3, 3, 1);

	/* enable clock module in ADC left and right channel */
	codec_reg_set(TS_CODEC_CHR_28, 2, 2, 1);
	codec_reg_set(TS_CODEC_CHR_28, 6, 6, 1);

	/* enable ADC module in ADC left and right channel */
	codec_reg_set(TS_CODEC_CHR_28, 1, 1, 1);
	codec_reg_set(TS_CODEC_CHR_28, 5, 5, 1);

	/* end initialization of ADCR and ADCL module */
	codec_reg_set(TS_CODEC_CHR_28, 0, 0, 1);
	codec_reg_set(TS_CODEC_CHR_28, 4, 4, 1);

	/* end initialization of ALC left and right module */
	codec_reg_set(TS_CODEC_CHR_28, 3, 3, 1);
	codec_reg_set(TS_CODEC_CHR_28, 7, 7, 1);

	/* end initialization of mic left and right module */
	codec_reg_set(TS_CODEC_CMICCR_23, 2, 2, 1);
	codec_reg_set(TS_CODEC_CMICCR_23, 6, 6, 1);

	/* set default mic gain of left and right module */
	codec_reg_set(TS_CODEC_CMICGAINR_24, 4, 5, right_mic_gain);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 6, 7, left_mic_gain);

	/* set default codec gain of left and right module */
	codec_reg_set(TS_CODEC_CANACR_26, 0, 4, 0x10);
	codec_reg_set(TS_CODEC_CANACR2_27, 0, 4, 0x10);

	/* disable zero-crossing detection function in ADC left and right channel */
	//if enable zero-crossing detection function,will cause small voice at first.
	codec_reg_set(TS_CODEC_CMICCR_23, 0, 0, 0);
	codec_reg_set(TS_CODEC_CMICCR_23, 4, 4, 0);


	codec_reg_set(TS_CODEC_CMICCR_23, 0, 7, 0x0e);
	codec_reg_set(TS_CODEC_CALCGR_25, 0, 7, 0x03);
	codec_reg_set(TS_CODEC_CHR_28, 0, 7, 0x0F);
	msleep(20);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 0, 3, 3);

	msleep(10);
	/* now begin recording... */
#endif
	return 0;
}

/* ADC stereo enable configuration */
static int codec_enable_stereo_record(void)
{
	/* select ADC stereo mode */
	codec_reg_set(TS_CODEC_CACR_02, 0, 0, 0);

	/* configure ADC I2S interface mode */
	codec_reg_set(TS_CODEC_CACR_02, 3, 4, 0x2);
	codec_reg_set(TS_CODEC_CACR2_03, 0, 5, 0x3e);

	/* end mute station of ADC left and right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 7, 7, 1);
	codec_reg_set(TS_CODEC_CMICCR_23, 3, 3, 1);

	/* enable current source of audio */
	codec_reg_set(TS_CODEC_CAACR_22, 4, 4, 1);
	/* bias current:reduce power,max is 7*/
	codec_reg_set(TS_CODEC_CMICGAINR_24, 0, 3, 7);//add 10:57

	codec_reg_set(TS_CODEC_CAACR_22, 3, 3, 1);
	codec_reg_set(TS_CODEC_CAACR_22, 0, 2, 0);

	/* enable reference voltage buffer in ADC left and right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 5, 5, 1);
	codec_reg_set(TS_CODEC_CMICCR_23, 1, 1, 1);

	/* enable MIC module in ADC left and right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 0, 0, 1);
	codec_reg_set(TS_CODEC_CALCGR_25, 2, 2, 1);

	/* enable ALC module in ADC left and right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 1, 1, 1);
	codec_reg_set(TS_CODEC_CALCGR_25, 3, 3, 1);

	/* enable clock module in ADC left and right channel */
	codec_reg_set(TS_CODEC_CHR_28, 2, 2, 1);
	codec_reg_set(TS_CODEC_CHR_28, 6, 6, 1);

	/* enable ADC module in ADC left and right channel */
	codec_reg_set(TS_CODEC_CHR_28, 1, 1, 1);
	codec_reg_set(TS_CODEC_CHR_28, 5, 5, 1);

	/* end initialization of ADCR and ADCL module */
	codec_reg_set(TS_CODEC_CHR_28, 0, 0, 1);
	codec_reg_set(TS_CODEC_CHR_28, 4, 4, 1);

	/* end initialization of ALC left and right module */
	codec_reg_set(TS_CODEC_CHR_28, 3, 3, 1);
	codec_reg_set(TS_CODEC_CHR_28, 7, 7, 1);

	/* end initialization of mic left and right module */
	codec_reg_set(TS_CODEC_CMICCR_23, 2, 2, 1);
	codec_reg_set(TS_CODEC_CMICCR_23, 6, 6, 1);

	/* set default mic gain of left and right module */
	codec_reg_set(TS_CODEC_CMICGAINR_24, 4, 5, right_mic_gain);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 6, 7, left_mic_gain);

	/* set default codec gain of left and right module */
	codec_reg_set(TS_CODEC_CANACR_26, 0, 4, 0x10);
	codec_reg_set(TS_CODEC_CANACR2_27, 0, 4, 0x10);

	/* disable zero-crossing detection function in ADC left and right channel */
	//if enable zero-crossing detection function,will cause small voice at first.
	codec_reg_set(TS_CODEC_CMICCR_23, 0, 0, 0);
	codec_reg_set(TS_CODEC_CMICCR_23, 4, 4, 0);

	msleep(20);
	/* now begin recording... */

	codec_reg_set(TS_CODEC_CMICGAINR_24, 0, 3, 3);

	msleep(10);
	return 0;
}

/* ADC left disable cofiguration */
static int codec_disable_left_record(void)
{
	/* disable zero-crossing detection function in ADC left channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 4, 4, 0);

	/* disable ADC module for left channel */
	codec_reg_set(TS_CODEC_CHR_28, 5, 5, 0);

	codec_reg_set(TS_CODEC_CHR_28, 6, 6, 0);

	/* disable ALC module for left channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 5, 5, 0);

	/* disable MIC module for left channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 4, 4, 0);

	/* disable reference voltage buffer for left channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 5, 5, 0);

	/* disable mic bias voltage buffer */
	codec_reg_set(TS_CODEC_CAACR_22, 4, 4, 0);

	/* begin initiialization ADC left module */
	codec_reg_set(TS_CODEC_CHR_28, 4, 4, 0);

	/* begin initialization ALC left module */
	codec_reg_set(TS_CODEC_CHR_28, 7, 7, 0);

	/* begin initialization MIC left module */
	codec_reg_set(TS_CODEC_CMICCR_23, 6, 6, 0);
	msleep(20);

	return AUDIO_SUCCESS;
}

/* ADC right disable cofiguration */
static int codec_disable_right_record(void)
{
	/* disable zero-crossing detection function in ADC right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 0, 0, 0);

	/* disable ADC module for right channel */
	codec_reg_set(TS_CODEC_CHR_28, 1, 1, 0);

	codec_reg_set(TS_CODEC_CHR_28, 2, 2, 0);

	/* disable ALC module for right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 1, 1, 0);

	/* disable MIC module for right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 0, 0, 0);

	/* disable reference voltage buffer for right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 1, 1, 0);

	/* disable mic bias voltage buffer */
	codec_reg_set(TS_CODEC_CAACR_22, 4, 4, 0);

	/* begin initiialization ADC right module */
	codec_reg_set(TS_CODEC_CHR_28, 0, 0, 0);

	/* begin initialization ALC right module */
	codec_reg_set(TS_CODEC_CHR_28, 3, 3, 0);

	/* begin initialization MIC right module */
	codec_reg_set(TS_CODEC_CMICCR_23, 2, 2, 0);
	msleep(20);

	return AUDIO_SUCCESS;
}

/* ADC stereo disable cofiguration */
static int codec_disable_stereo_record(void)
{
	/* disable zero-crossing detection function in ADC left and right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 0, 0, 0);
	codec_reg_set(TS_CODEC_CMICCR_23, 4, 4, 0);

	/* disable ADC module for left and right channel */
	codec_reg_set(TS_CODEC_CHR_28, 1, 1, 0);
	codec_reg_set(TS_CODEC_CHR_28, 5, 5, 0);

	codec_reg_set(TS_CODEC_CHR_28, 2, 2, 0);
	codec_reg_set(TS_CODEC_CHR_28, 6, 6, 0);

	/* disable ALC module for left and right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 1, 1, 0);
	codec_reg_set(TS_CODEC_CALCGR_25, 5, 5, 0);

	/* disable MIC module for left and right channel */
	codec_reg_set(TS_CODEC_CALCGR_25, 0, 0, 0);
	codec_reg_set(TS_CODEC_CALCGR_25, 4, 4, 0);

	/* disable reference voltage buffer for left and right channel */
	codec_reg_set(TS_CODEC_CMICCR_23, 1, 1, 0);
	codec_reg_set(TS_CODEC_CMICCR_23, 5, 5, 0);

	/* disable mic bias voltage buffer */
	codec_reg_set(TS_CODEC_CAACR_22, 4, 4, 0);

	/* begin initiialization ADC left and right module */
	codec_reg_set(TS_CODEC_CHR_28, 0, 0, 0);
	codec_reg_set(TS_CODEC_CHR_28, 4, 4, 0);

	/* begin initialization ALC left and right module */
	codec_reg_set(TS_CODEC_CHR_28, 3, 3, 0);
	codec_reg_set(TS_CODEC_CHR_28, 7, 7, 0);

	/* begin initialization MIC left and right module */
	codec_reg_set(TS_CODEC_CMICCR_23, 2, 2, 0);
	codec_reg_set(TS_CODEC_CMICCR_23, 6, 6, 0);
	msleep(20);

	return AUDIO_SUCCESS;
}

static int codec_enable_playback(void)
{
	int val = -1;

	/* DAC i2S interface */
	codec_reg_set(TS_CODEC_CACR2_03, 6, 7, 0x3);
	codec_reg_set(TS_CODEC_CDCR1_04, 0, 7, 0x10);
	codec_reg_set(TS_CODEC_CDCR2_05, 0, 7, 0x0e);

	/* enable current source */
	codec_reg_set(TS_CODEC_CHR_29, 7, 7, 0x1);
	codec_reg_set(TS_CODEC_POWER_20, 0, 3, 0x07);

	/* enable reference voltage */
	codec_reg_set(TS_CODEC_CHR_29, 6, 6, 0x1);

	/* enable POP sound */
	codec_reg_set(TS_CODEC_CHR_29, 4, 5, 0x2);

	/* enable HPDRV module */
	codec_reg_set(TS_CODEC_CHR_2a, 4, 4, 1);

	/* end initialization HPDRV module */
	codec_reg_set(TS_CODEC_CHR_2a, 5, 5, 1);

	/* enable reference voltage */
	codec_reg_set(TS_CODEC_CHR_29, 3, 3, 1);

	/* enable DACL clock module */
	codec_reg_set(TS_CODEC_CHR_29, 2, 2, 1);

	/* enable DACL module */
	codec_reg_set(TS_CODEC_CHR_29, 1, 1, 1);

	/* end initialization DAC module */
	codec_reg_set(TS_CODEC_CHR_29, 0, 0, 1);

	/* end mute station DRV module */
	codec_reg_set(TS_CODEC_CHR_2a, 6, 6, 1);

	/* set default gain of HPDRV module */
	codec_reg_set(TS_CODEC_CHR_2b, 0, 4, 0x18);

	msleep(1);
	/* now playing music */
	if (g_codec_dev->spk_en.gpio > 0) {
		val = gpio_get_value(g_codec_dev->spk_en.gpio);
		gpio_direction_output(g_codec_dev->spk_en.gpio, g_codec_dev->spk_en.active_level);
		val = gpio_get_value(g_codec_dev->spk_en.gpio);
	}

	return AUDIO_SUCCESS;
}

static int codec_disable_playback(void)
{
	if (g_codec_dev->spk_en.gpio > 0)
		gpio_direction_output(g_codec_dev->spk_en.gpio,
				!g_codec_dev->spk_en.active_level);

	/* set DAC gain to 0 */
	codec_reg_set(TS_CODEC_CHR_2b, 0, 4, 0);

	/* mute HPDRV module */
	codec_reg_set(TS_CODEC_CHR_2a, 6, 6, 0);

	/* initialize DAC module */
	codec_reg_set(TS_CODEC_CHR_2a, 5, 5, 0);

	/* disable HPDRV module */
	codec_reg_set(TS_CODEC_CHR_2a, 4, 4, 0);

	/* disable DACL module */
	codec_reg_set(TS_CODEC_CHR_29, 1, 1, 0);

	/* disable DACL clock module */
	codec_reg_set(TS_CODEC_CHR_29, 2, 2, 0);

	/* disable DACL reference voltage */
	codec_reg_set(TS_CODEC_CHR_29, 3, 3, 0);

	/* initialize POP sound */
	codec_reg_set(TS_CODEC_CHR_29, 4, 5, 0x1);

	/* disable reference voltage buffer */
	codec_reg_set(TS_CODEC_CHR_29, 6, 6, 0);

	/* disable current source DAC */
	codec_reg_set(TS_CODEC_CHR_29, 7, 7, 0);

	/* begin initialization HPDRV module */
	codec_reg_set(TS_CODEC_CHR_29, 0, 0, 0);
	msleep(20);
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
	codec_reg_set(TS_CODEC_CSRR_44, 0, 2, rate_fs[i]);
	codec_reg_set(TS_CODEC_CSRR_54, 0, 2, rate_fs[i]);

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
			type->sample_channel != INCODEC_CHANNEL_MONO_RIGHT && type->sample_channel != INCODEC_CHANNEL_STEREO){
		printk("inner codec only support mono or stereo for record!\n");
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}

	if(this->frame_vsize != type->frame_vsize){
		/* ADC valid data length */
		if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_16BIT)
			codec_reg_set(TS_CODEC_CACR_02, 5, 6, ADC_VALID_DATA_LEN_16BIT);
		else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_20BIT)
			codec_reg_set(TS_CODEC_CACR_02, 5, 6, ADC_VALID_DATA_LEN_20BIT);
		else
			codec_reg_set(TS_CODEC_CACR_02, 5, 6, ADC_VALID_DATA_LEN_24BIT);
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
			codec_reg_set(TS_CODEC_CACR_02, 0, 0, CHOOSE_ADC_CHN_MONO);
		else
			codec_reg_set(TS_CODEC_CACR_02, 0, 0, CHOOSE_ADC_CHN_STEREO);
		msleep(1);
		this->sample_channel = type->sample_channel;
		audio_info_print("%s %d: inner codec set record channel = %d\n",__func__ , __LINE__,type->sample_channel);
	}

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

static int codec_record_set_again(struct codec_analog_gain *again)
{
	/******************************************\
	| vol :0	1		2	..	30  31	       |
	| gain:-18  -16.5  -15	..	27  28.5 (+DB) |
	\******************************************/
	if(!again || !g_codec_dev)
		return -AUDIO_EPARAM;

	if (again->gain.channel == INCODEC_CHANNEL_MONO_LEFT) {
		if (again->lmode == CODEC_ANALOG_AUTO) {
			/*keep agc range(min,max) default*/
			codec_reg_set(TS_CODEC_CAFG_49, 6, 6, 1);
			codec_reg_set(TS_CODEC_CGAINLR_0a, 5, 5, 1);   //ALCL
		} else {
			codec_reg_set(TS_CODEC_CAFG_49, 6, 6, 0);
			codec_reg_set(TS_CODEC_CGAINLR_0a, 5, 5, 0);
			if(again->gain.gain[0] < 0) again->gain.gain[0] = 0;
			if(again->gain.gain[0] > 31) again->gain.gain[0] = 31;
			codec_reg_set(TS_CODEC_CANACR_26, 0, 4, again->gain.gain[0]);   //ALCL
		}
	} else if (again->gain.channel == INCODEC_CHANNEL_MONO_RIGHT) {
		if (again->rmode == CODEC_ANALOG_AUTO) {
			/*keep agc range(min,max) default*/
			codec_reg_set(TS_CODEC_CAFG_59, 6, 6, 1);
			codec_reg_set(TS_CODEC_CGAINLR_0a, 4, 4, 1);   //ALCR
		} else {
			codec_reg_set(TS_CODEC_CAFG_59, 6, 6, 0);
			codec_reg_set(TS_CODEC_CGAINLR_0a, 4, 4, 0);
			if(again->gain.gain[1] < 0) again->gain.gain[1] = 0;
			if(again->gain.gain[1] > 31) again->gain.gain[1] = 31;
			codec_reg_set(TS_CODEC_CANACR2_27, 0, 4, again->gain.gain[1]);  //ALCR
		}
	} else {
		if (again->lmode == CODEC_ANALOG_AUTO) {
			/*keep agc range(min,max) default*/
			codec_reg_set(TS_CODEC_CAFG_49, 6, 6, 1);
			codec_reg_set(TS_CODEC_CGAINLR_0a, 5, 5, 1);   //ALCL
		} else {
			codec_reg_set(TS_CODEC_CAFG_49, 6, 6, 0);
			codec_reg_set(TS_CODEC_CGAINLR_0a, 5, 5, 0);
			if(again->gain.gain[0] < 0) again->gain.gain[0] = 0;
			if(again->gain.gain[0] > 31) again->gain.gain[0] = 31;
			codec_reg_set(TS_CODEC_CANACR_26, 0, 4, again->gain.gain[0]);   //ALCL
		}
		if (again->rmode == CODEC_ANALOG_AUTO) {
			/*keep agc range(min,max) default*/
			codec_reg_set(TS_CODEC_CAFG_59, 6, 6, 1);
			codec_reg_set(TS_CODEC_CGAINLR_0a, 4, 4, 1);   //ALCR
		} else {
			codec_reg_set(TS_CODEC_CAFG_59, 6, 6, 0);
			codec_reg_set(TS_CODEC_CGAINLR_0a, 4, 4, 0);
			if(again->gain.gain[1] < 0) again->gain.gain[1] = 0;
			if(again->gain.gain[1] > 31) again->gain.gain[1] = 31;
			codec_reg_set(TS_CODEC_CANACR2_27, 0, 4, again->gain.gain[1]);  //ALCR
		}
	}
	msleep(1);
	return 0;
}

static int codec_record_set_hpf_enable(int en)
{
	codec_reg_set(TS_CODEC_CGAINLR_0a, 2, 2, !en);

	return AUDIO_SUCCESS;
}

//static int codec_record_set_dgain(unsigned int gain)
static int codec_record_set_dgain(struct volume gain)
{
	//consider gain is valid?
	/******************************************\
	| vol :0	1		2	    0xc3..	0xff       |
	| gain:mute -97    -96.5	0    ..	30    (+DB) |
	\******************************************/
	if (gain.gain[0] > 0xff)
		gain.gain[0] = 0xff;
	if (gain.gain[1] > 0xff)
		gain.gain[1] = 0xff;

	if (gain.channel == INCODEC_CHANNEL_MONO_LEFT)
		codec_reg_set(TS_CODEC_CAVR_08, 0, 7, gain.gain[0]);     //ADC left volume
	else if (gain.channel == INCODEC_CHANNEL_MONO_RIGHT)
		codec_reg_set(TS_CODEC_CGAINLR_09, 0, 7, gain.gain[1]);  //ADC right volume
	else {
		codec_reg_set(TS_CODEC_CAVR_08, 0, 7, gain.gain[0]);     //ADC left volume
		codec_reg_set(TS_CODEC_CGAINLR_09, 0, 7, gain.gain[1]);  //ADC right volume
	}

	return AUDIO_SUCCESS;
}

static int codec_record_set_alc_threshold(struct alc_gain threshold)
{
	if (threshold.channel == INCODEC_CHANNEL_MONO_LEFT) {
		codec_reg_set(TS_CODEC_CAFG_49, 3, 5, threshold.maxgain[0]);
		codec_reg_set(TS_CODEC_CAFG_49, 0, 2, threshold.mingain[0]);
	} else if (threshold.channel == INCODEC_CHANNEL_MONO_RIGHT) {
		codec_reg_set(TS_CODEC_CAFG_59, 3, 5, threshold.maxgain[1]);
		codec_reg_set(TS_CODEC_CAFG_59, 0, 2, threshold.mingain[1]);
	} else {
		codec_reg_set(TS_CODEC_CAFG_49, 3, 5, threshold.maxgain[0]);
		codec_reg_set(TS_CODEC_CAFG_49, 0, 2, threshold.mingain[0]);
		codec_reg_set(TS_CODEC_CAFG_59, 3, 5, threshold.maxgain[1]);
		codec_reg_set(TS_CODEC_CAFG_59, 0, 2, threshold.mingain[1]);
	}

	return AUDIO_SUCCESS;
}

static int codec_record_set_mute(unsigned int channel, unsigned int mute_en, unsigned int dgain)
{
	unsigned int reg_addr = 0;

	if (channel == INCODEC_CHANNEL_MONO_LEFT)
		reg_addr = TS_CODEC_CAVR_08;
	else
		reg_addr = TS_CODEC_CGAINLR_09;

	if (mute_en)
		codec_reg_set(reg_addr, 0, 7, 0);
	else
		codec_reg_set(reg_addr, 0, 7, dgain);

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
	if(type->sample_channel != INCODEC_CHANNEL_MONO_LEFT){
		printk("inner codec only support one hpout for playback!\n");
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}

	if(this->frame_vsize != type->frame_vsize){
		audio_info_print("type->frame_vsize=%u, we support %d %d %d\n", type->frame_vsize, INCODEC_CODEC_VALID_DATA_SIZE_16BIT, INCODEC_CODEC_VALID_DATA_SIZE_20BIT, INCODEC_CODEC_VALID_DATA_SIZE_24BIT);
		/* ADC valid data length */
		if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_16BIT)
			codec_reg_set(TS_CODEC_CDCR1_04, 5, 6, ADC_VALID_DATA_LEN_16BIT);
		else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_20BIT)
			codec_reg_set(TS_CODEC_CDCR1_04, 5, 6, ADC_VALID_DATA_LEN_20BIT);
		else
			codec_reg_set(TS_CODEC_CDCR1_04, 5, 6, ADC_VALID_DATA_LEN_24BIT);
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
		ret = codec_enable_playback();
	}else{
		ret = codec_disable_playback();
	}
	return ret;
}

static int codec_playback_set_again(struct codec_analog_gain *again)
{
	if(!again || !g_codec_dev)
		return -AUDIO_EPARAM;
	if (again->gain.gain[0] < 0) again->gain.gain[0] = 0;
	if (again->gain.gain[0] > 0x1f) again->gain.gain[0] = 0x1f;

	codec_reg_set(TS_CODEC_CHR_2b, 0, 4, again->gain.gain[0]);
	return 0;
}

static int codec_playback_set_dgain(struct volume gain)
{
	//consider gain is valid?
	/******************************************\
	| vol :0	1		2	    0xf1..	0xff       |
	| gain:mute -120    -119.5	0    ..	7    (+DB) |
	\******************************************/
	if (gain.gain[0] > 0xff)
		gain.gain[0] = 0xff;

	codec_reg_set(TS_CODEC_CDCR2_06, 0, 7, gain.gain[0]);

	return AUDIO_SUCCESS;
}

static int codec_playback_set_mute(unsigned int channel, unsigned int mute_en, unsigned int dgain)
{
	if (mute_en)
		codec_reg_set(TS_CODEC_CDCR2_06, 0, 7, 0);
	else
		codec_reg_set(TS_CODEC_CDCR2_06, 0, 7, dgain);

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

static int codec_set_power(int enable)
{
	if(enable){
		codec_init();
	}else{
		codec_disable_stereo_record();
		codec_disable_playback();
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

	g_codec_dev->spk_en.gpio = spk_gpio;
	g_codec_dev->spk_en.active_level = spk_level;

	if (g_codec_dev->spk_en.gpio != -1 ) {
		if (gpio_request(g_codec_dev->spk_en.gpio,"gpio_spk_en") < 0) {
			gpio_free(g_codec_dev->spk_en.gpio);
			printk(KERN_DEBUG"request spk en gpio %d error!\n", g_codec_dev->spk_en.gpio);
			ret = -EPERM;
			goto failed;
		}
		printk(KERN_DEBUG"request spk en gpio %d ok!\n", g_codec_dev->spk_en.gpio);
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
failed:
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
	if (g_codec_dev->spk_en.gpio > 0)
		gpio_free(g_codec_dev->spk_en.gpio);
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
