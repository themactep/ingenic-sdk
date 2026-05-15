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
#include "../../host/PRJ008/audio_aic.h"
#include "../../include/audio_debug.h"
#include "codec.h"

static int spk_gpio =  GPIO_PB(31);
module_param(spk_gpio, int, S_IRUGO);
MODULE_PARM_DESC(spk_gpio, "Speaker GPIO NUM");

static int spk_level = 1;
module_param(spk_level, int, S_IRUGO);
MODULE_PARM_DESC(spk_level, "Speaker active level: -1 disable, 0 low active, 1 high active");

#if 0
static int protocel_mode = 2;
module_param(protocel_mode, int, S_IRUGO);
MODULE_PARM_DESC(protocel_mode, "I2S protocel type,1:msb justified 2:i2s mode");
#endif

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

	return ret;
}

int dump_codec_regs(void)
{
	int i = 0;
	printk("\nDump Codec register:\n");
	for (i = 0; i < 0x124; i += 4) {
		printk("reg = 0x%04x [0x%04x]:		val = 0x%04x\n", i, i/4, codec_reg_read(g_codec_dev, i));
	}
	printk("Dump PRJ008 Codec register End.\n");

	return 0;
}

int incodec_spk_gpio_request(void)
{
	if (spk_gpio != -1) {
		if (gpio_request(spk_gpio,"gpio_spk_en") < 0) {
			gpio_free(spk_gpio);
			printk(KERN_DEBUG"request spk en gpio %d error!\n", spk_gpio);
			return -1;
		}
		printk("inner_codec spk gpio request success!\n");
	}
	return 0;
}

int incodec_spk_gpio_free(void)
{
	if (spk_gpio > 0)
		gpio_free(spk_gpio);

	return 0;
}

int codec_init(void)
{
	int i = 0;
	char value = 0;

	/* step1. Supply the power of the digital part and reset the audio codec*/
	codec_reg_set(CODEC_CDGR, 0, 7, 0xc0);
	msleep(1);
	codec_reg_set(CODEC_CDGR, 0, 7, 0xf1);
	msleep(1);

	/* step2. Configure the register POP_CTRL_DACL[1:0] 0x28[6:5] to 2’b01, to setup the output
	   dc voltage of DAC left channel.*/
	codec_reg_set(CODEC_CADACR1, 5, 6, 0x1);
	msleep(1);

	/* Step3: Configure the register SEL_VREF[7:0] reg0x21[7:0] to 8’b000_0001. */
	codec_reg_set(CODEC_CAGR2, 0, 7, 0x1);
	msleep(1);

	/* step4. Configure the register EN_VREF reg0x20[0] to 1 to setup reference voltage.*/
	codec_reg_set(CODEC_CAGR1, 0, 0, 0x1);
	msleep(1);

	/* step5. Configure select current to pre-charge/dis-charge*/
	for (i = 0; i <= 7; i++) {
		value |= value<<1 | 0x01;
		codec_reg_set(CODEC_CAGR2, 0, 7, value);
		msleep(10);
	}

	/*step6.Set the min charge current for reduce power.*/
	codec_reg_set(CODEC_CAGR2, 0, 7, 0x02);
	msleep(10);

	codec_reg_set(CODEC_CAGR1, 1, 1, 0x1);
    codec_reg_set(CODEC_CADACR1, 2, 2, 0x1);
    codec_reg_set(CODEC_CADACR1, 5, 6, 0x2);

	return 0;
}

int codec_deinit(void)
{
	int i = 0;
	char value = 0;
    codec_reg_set(CODEC_CADACR1, 5, 6, 0x1);
    codec_reg_set(CODEC_CADACR1, 2, 2, 0x0);
    codec_reg_set(CODEC_CAGR1, 1, 1, 0x0);

	/*step1. Configure the register SEL_VREF[7:0] reg0x21[7:0] to 8’b0000_0001.*/
	codec_reg_set(CODEC_CAGR2, 0, 7, 0x01);
	msleep(1);

	/*step2. Configure the register EN_VREF reg0x20[0] to 1’b0 to disable reference voltage*/
	codec_reg_set(CODEC_CAGR1, 0, 0, 0x0);
	msleep(1);

	/* step3. Configure select current to pre-charge/dis-charge*/
	for (i = 0; i <= 7; i++) {
		value |= value<<1 | 0x01;
		codec_reg_set(CODEC_CAGR2, 0, 7, value);
		msleep(10);
	}

	return 0;
}

static int codec_enable_record(void)
{
	/* Choose ADC I2S interface mode */
	codec_reg_set(CODEC_CDADCR1, 2, 3, 0x2);
	codec_reg_set(CODEC_CDADCR2, 7, 7, 0x0);

	/* Choose ADC I2S Master Mode */
	codec_reg_set(CODEC_CDADCR2, 0, 1, 0x1);
	codec_reg_set(CODEC_CDADCR2, 4, 5, 0x3);

	/* Enable ADC */
	/*Step1. Configure the register EN_IBIAS_ADC 0x20[2] to 1, to enable the current source of
	  ADC.*/
	codec_reg_set(CODEC_CAGR1, 2, 2, 0x1);

	codec_reg_set(CODEC_CAGR1, 3, 3, 0x1);   //micbias enable
	codec_reg_set(CODEC_CAGR1, 5, 7, 0x7);   //micbias

	/*Step2. Configure the register EN_BUF_ADCL 0x30[2] to 1, to enable the reference
	  voltage buffer in ADC left channel.*/
	codec_reg_set(CODEC_CAADCR1, 2, 2, 0x1);

	/*Step3. Configure the register EN_MICL 0x31[1] to 1, to enable the MIC module in ADC
	  left channel.*/
	codec_reg_set(CODEC_CAADCR2, 1, 1, 0x1);

	/*Step4. Configure the register EN_ALCL 0x31[0] to 1, to enable the ALC module in ADC
	  left channel.*/
	codec_reg_set(CODEC_CAADCR2, 0, 0, 0x1);
	msleep(10);

	/*Step5. Configure the register EN_CLK_ADCL 0x30[1] to 1, to enable the clock module in
	  ADC left channel.*/
	codec_reg_set(CODEC_CAADCR1, 1, 1, 0x1);

	/*Step6. Configure the register EN_ADCL 0x30[0] to 1, to enable the ADC module in ADC
	  left channel.*/
	codec_reg_set(CODEC_CAADCR1, 0, 0, 0x1);
	msleep(1);

	/*Step7. Configure the register INITIAL_ADCL 0x30[4] to 1, to end the initialization of the
	  ADCL module.*/
	codec_reg_set(CODEC_CAADCR1, 4, 4, 0x1);

	/*Step8. Configure the register INITIAL_ALCL 0x31[2] to 1, to end the initialization of the
	  left ALC module.*/
	codec_reg_set(CODEC_CAADCR2, 2, 2, 0x1);

	/*Step9. Configure the register INITIAL_MICL 0x31[3] to 1, to end the initialization of the
	  left MIC module.*/
	codec_reg_set(CODEC_CAADCR2, 3, 3, 0x1);

	/*Step10. Configure the register MUTE_MICL 0x31[4] to 1, to end the mute station of the
	  ADC left channel.*/
	codec_reg_set(CODEC_CAADCR2, 4, 4, 0x1);

	/*Step11. Configure the register GAIN_MICL 0x33[1:0], to select the gain of the left MIC
	  module.*/
	codec_reg_set(CODEC_CAADCR4, 0, 1, 0x3); //mic hw gain

	/*Step12. Configure the register GAIN_ALCL 0x32[4:0], to select the gain of the left ALC
	  module.*/
	codec_reg_set(CODEC_CAADCR3, 0, 4, 0x1f);

	/*Step13. Configure the register EN_ZeroDET_ADCL 0x30[3] to 1, to enable the zerocrossing detection function in ADC left channel.*/
	codec_reg_set(CODEC_CAGCR4, 0, 5, 0x3f);

	codec_reg_set(CODEC_CAGR3, 4, 7, 0x3);
	codec_reg_set(CODEC_CDADCR3, 2, 3, 0x3);
	msleep(1);

	return 0;
}

static int codec_disable_record(void)
{
	/* Step1. Configure the register EN_ZERODET_ADCL 0x30[3] to 0, to disable the zerocrossing detection function in ADC left channel.*/
	codec_reg_set(CODEC_CAADCR1, 3, 3, 0x0);

	/* Step2. Configure the register EN_ADCL 0x30[0] to 0, to disable the ADC module in ADC
	   left channel.*/
	codec_reg_set(CODEC_CAADCR1, 0, 0, 0x0);

	/* Step3. Configure the register EN_CLK_ADCL 0x30[1] to 0, to disable the clock module in
	   ADC left channel.*/
	codec_reg_set(CODEC_CAADCR1, 1, 1, 0x0);

	/* Step4. Configure the register EN_ALCL 0x31[0] to 0, to disable the ALC module in ADC
	   left channel.*/
	codec_reg_set(CODEC_CAADCR2, 0, 0, 0x0);

	/* Step5. Configure the register EN_MICL 0x31[1] to 0, to disable the MIC module in ADC
	   left channel.*/
	codec_reg_set(CODEC_CAADCR2, 1, 1, 0x0);

	/* Step6. Configure the register EN_BUF_ADCL 0x30[2] to 0, to disable the reference
	   voltage buffer in ADC left channel.*/
	codec_reg_set(CODEC_CAADCR1, 2, 2, 0x0);

	/* Step7. Configure the register EN_IBIAS_ADC 0x20[2] to 0, to disable the current source
	   of ADC.*/
	codec_reg_set(CODEC_CAGR1, 2, 2, 0x0);

	/* Step8. Configure the register INITIAL_ADCL 0x30[4] to 0, to begin the initialization of
	   the ADCL module.*/
	codec_reg_set(CODEC_CAADCR1, 4, 4, 0x0);

	/* Step9. Configure the register INITIAL_ALCL 0x31[2] to 0, to begin the initialization of
	   the left ALC module.*/
	codec_reg_set(CODEC_CAADCR2, 2, 2, 0x0);

	/*Step10. Configure the register INITIAL_MICL 0x31[3] to 0, to begin the initialization of
	  the left MIC module.*/
	codec_reg_set(CODEC_CAADCR2, 3, 3, 0x0);
	//hpf dsiable
	codec_reg_set(CODEC_CDADCR3, 2, 3, 0x0);

	return AUDIO_SUCCESS;
}

static int codec_enable_playback(void)
{
    int val = -1;
	/* Choose ADC I2S interface mode */
    codec_reg_set(CODEC_CDDACR1, 2, 3, 0x2);
    /* Choose ADC I2S Master Mode */
    codec_reg_set(CODEC_CDDACR2, 4, 5, 0x3);
	/*
	codec_reg_set(CODEC_CAGR1, 1, 1, 0x1);
    codec_reg_set(CODEC_CADACR1, 2, 2, 0x1);
    codec_reg_set(CODEC_CADACR1, 5, 6, 0x2);
	*/
    /*Step5: Set the register CHR[4] to 1, to enable the HPDRV module in the DAC channel. */
    codec_reg_set(CODEC_CADACR2, 0, 0, 0x1);
    /*Step6: Set the register CHR[5] to 1, to end the initialization of the HPDRV module in the DAC channel. */
    codec_reg_set(CODEC_CADACR2, 2, 2, 0x1);
    /*Step7: Set the register CANACR1[3] to 1, to enable the reference voltage of DAC module. */
    codec_reg_set(CODEC_CADACR1, 3, 3, 0x1);
    /*Step8: Set the register CANACR1[2] to 1, to enable the clock module of DAC module. */
    codec_reg_set(CODEC_CADACR1, 1, 1, 0x1);
    /*Step9: Set the register CANACR1[1] to 1, to enable the DAC module. */
    codec_reg_set(CODEC_CADACR1, 0, 0, 0x1);
	msleep(1);
    /*Step10: Set the register CANACR1[0] to 1, to end the initialization of the DAC module in the left DAC channel. */
    codec_reg_set(CODEC_CADACR1, 4, 4, 0x1);
    /*Step11: Set the register CHR[6] to 1, to end the mute station of the DRV module in the DAC channel. */
    codec_reg_set(CODEC_CADACR2, 5, 5, 0x1);
    /*Step12: Set the register CHPOUTLGR[4:0], to select the gain of HPDRV module in the DAC channel. */
    codec_reg_set(CODEC_CADACR4, 0, 4, 0x18);
    /* low power consumption */
    //codec_reg_set(CODEC_CAGR2, 0, 7, 0x1);

    if (spk_gpio > 0) {
        val = gpio_get_value(spk_gpio);
        gpio_direction_output(spk_gpio, spk_level);
        val = gpio_get_value(spk_gpio);
    }

    return 0;
}

static int codec_disable_playback(void)
{
    if (spk_gpio > 0)
        gpio_direction_output(spk_gpio,!spk_level);
    /*Step2: Set the register CHPOUTLGR[4:0] to 5’b0_0000, to select the gain of the HPDRV in the DAC channel.*/
    codec_reg_set(CODEC_CADACR4, 0, 4, 0);
    /*Step3: Set the register CHR[6] to 0, to mute the HPDRV module in the DAC channel.*/
    codec_reg_set(CODEC_CADACR2, 5, 5, 0);
    /*Step4: Set the register CHR[5] to 0, to begin the initialization of the HPDRV module in the DAC channel.*/
    codec_reg_set(CODEC_CADACR2, 2, 2, 0);
    /*Step5: Set the register CHR[4] to 0, to disable the HPDRV module in the DAC channel.*/
    codec_reg_set(CODEC_CADACR2, 0, 0, 0);
	/*Step6: Set the register CANACR1[1] to 0 to disable the DAC module.*/
    codec_reg_set(CODEC_CADACR1, 0, 0, 0);
   /*Step7: Set the register CANACR1[2] to 0, to disable the clock module of DAC module.*/
    codec_reg_set(CODEC_CADACR1, 1, 1, 0);
   /*Step8: Set the register CANACR1[3] to 0, to disable the reference voltage of DAC module.*/
    codec_reg_set(CODEC_CADACR1, 3, 3, 0);
   /*Step9: Set the register CANACR1[5:4] to 2’b01 to initialize the POP sound in the DAC channel. */
	/*
	codec_reg_set(CODEC_CADACR1, 5, 6, 0x1);
    codec_reg_set(CODEC_CADACR1, 2, 2, 0x0);
    codec_reg_set(CODEC_CAGR1, 1, 1, 0x0);
	*/

    /*Step12: Set the register CANACR1[0] to 0, to initialize the DAC module in the DAC channel. */
    codec_reg_set(CODEC_CADACR1, 4, 4, 0);

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
	codec_reg_set(CODEC_CAGCR10, 0, 2, rate_fs[i]);
	msleep(1);
	return 0;
}

#define INCODEC_CHANNEL_MONO_LEFT 1
#define INCODEC_CHANNEL_MONO_RIGHT 2
#define INCODEC_CHANNEL_STEREO 3
#define INCODEC_CODEC_VALID_DATA_SIZE_16BIT 16
#define INCODEC_CODEC_VALID_DATA_SIZE_20BIT 20
#define INCODEC_CODEC_VALID_DATA_SIZE_24BIT 24
#define INCODEC_CODEC_VALID_DATA_SIZE_32BIT 32

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
			&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_24BIT
			&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_32BIT){
		printk("inner codec ADC only support 16bit,20bit,24bit,32bit for record!\n");
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}
	if(type->sample_channel != INCODEC_CHANNEL_MONO_LEFT){
		printk("inner codec only support left mono for record!\n");
		ret |= AUDIO_EPARAM;
		goto ERR_PARAM;
	}

	if(this->frame_vsize != type->frame_vsize){
		/* ADC valid data length */
		if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_16BIT)
			codec_reg_set(CODEC_CDADCR1, 4, 5, ADC_VALID_DATA_LEN_16BIT);
		else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_20BIT)
			codec_reg_set(CODEC_CDADCR1, 4, 5, ADC_VALID_DATA_LEN_20BIT);
		else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_24BIT)
			codec_reg_set(CODEC_CDADCR1, 4, 5, ADC_VALID_DATA_LEN_24BIT);
		else
			codec_reg_set(CODEC_CDADCR1, 4, 5, ADC_VALID_DATA_LEN_32BIT);
		msleep(1);
		this->frame_vsize = type->frame_vsize;
	}

	if(this->sample_rate != type->sample_rate){
		this->sample_rate = type->sample_rate;
		ret |= codec_set_sample_rate(type->sample_rate);
	}

	if(this->sample_channel != type->sample_channel){
		/* Choose ADC chn */
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
		ret = codec_enable_record();
	}else{
		ret = codec_disable_record();
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
		codec_reg_set(CODEC_CAGCR9, 6, 6, 0);
		msleep(1);
		codec_reg_set(CODEC_CDADCR4, 0, 0, 0);
		msleep(1);
		codec_reg_set(CODEC_CAADCR3, 0, 4, analog_gain);
		msleep(1);
	}else if(channel == MONO_RIGHT){
		printk("T32 only support record MONO LEFT\n");
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
		codec_reg_set(CODEC_CDADCR5, 0, 7, digital_gain);
		msleep(1);
	}else if(channel == MONO_RIGHT){
		printk("T32 only support record MONO LEFT\n");
	}

	return AUDIO_SUCCESS;
}

static int codec_record_set_hpf_enable(int en)
{
	return AUDIO_SUCCESS;
}

static int codec_record_set_alc_threshold(int channel,int min_gain,int max_gain)
{
	return AUDIO_SUCCESS;
}

static int codec_record_set_mute(int channel)
{
	if(channel == MONO_LEFT){
		codec_reg_set(CODEC_CDADCR5, 0, 7, 0);
		msleep(1);
	}else if (channel == MONO_RIGHT){
		printk("T32 only support record MONO LEFT\n");
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
    if(this->frame_vsize != type->frame_vsize){
        audio_info_print("type->frame_vsize=%u, we support %d %d %d %d\n", type->frame_vsize, INCODEC_CODEC_VALID_DATA_SIZE_16BIT,
        INCODEC_CODEC_VALID_DATA_SIZE_20BIT, INCODEC_CODEC_VALID_DATA_SIZE_24BIT,             INCODEC_CODEC_VALID_DATA_SIZE_32BIT);
        if(type->frame_vsize != 16
                && type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_20BIT
                && type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_24BIT
                && type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_32BIT){
            printk("inner codec DAC only support 16bit,20bit,24bit for record!type->frame_vsize=%u\n", type->frame_vsize);
			goto ERR_PARAM;
        }else{
            /* DAC valid data length */
            if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_16BIT)
                codec_reg_set(CODEC_CDDACR1, 4, 5, ADC_VALID_DATA_LEN_16BIT);
            else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_20BIT)
                codec_reg_set(CODEC_CDDACR1, 4, 5, ADC_VALID_DATA_LEN_20BIT);
            else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_24BIT)
                codec_reg_set(CODEC_CDDACR1, 4, 5, ADC_VALID_DATA_LEN_24BIT);
            else
                codec_reg_set(CODEC_CDDACR1, 4, 5, ADC_VALID_DATA_LEN_32BIT);
            msleep(1);
            this->frame_vsize = type->frame_vsize;
        }
    }
	if(this->sample_rate != type->sample_rate){
		this->sample_rate = type->sample_rate;
		ret |= codec_set_sample_rate(type->sample_rate);
	}
	if(this->sample_channel != type->sample_channel){
		if(type->sample_channel != 1){
			printk("inner codec only support one hpout for playback!\n");
			goto ERR_PARAM;
		}else{
			this->sample_channel = type->sample_channel;
		}
	}

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

static int codec_playback_set_again(int channel ,int gain)
{
	int analog_gain = 0;
	if(!g_codec_dev)
		return -AUDIO_EPARAM;
	analog_gain = gain < 0 ? 0 : gain;
	analog_gain = gain > 0x1f ? 0x1f : gain;
	codec_reg_set(CODEC_CADACR4, 0, 4, analog_gain);
    msleep(1);

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

	codec_reg_set(CODEC_CDDACR6, 0, 7, digital_gain);
    msleep(1);

	return AUDIO_SUCCESS;
}

static int codec_playback_set_mute(int channel)
{
	codec_reg_set(CODEC_CDDACR6, 0, 7, 0);
    msleep(1);

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

#ifdef CONFIG_KERNEL_5_15
static struct proc_ops inner_codec_fops = {
	.proc_read = seq_read,
	.proc_open = dump_inner_codec_open,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static struct file_operations inner_codec_fops = {
	.read = seq_read,
	.open = dump_inner_codec_open,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

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
		codec_disable_record();
		codec_disable_playback();
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
	.spk_gpio_request = incodec_spk_gpio_request,
	.spk_gpio_free = incodec_spk_gpio_free,
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

	platform_set_drvdata(pdev, &g_codec_dev);
	g_codec_dev->priv = pdev;
	g_codec_dev->attr = &inner_codec_attr;
	/*1/2 frame total size,only suppport 32bit*/
	g_codec_dev->record_type.frame_size = 32;
	g_codec_dev->playback_type.frame_size = 32;
	inner_codec_attr.host_priv = g_codec_dev;

	ret = inner_codec_register(&inner_codec_attr);
	if(ret != AUDIO_SUCCESS) {
		dev_err(&pdev->dev, "failed to register codec\n");
		goto failed;
	}

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
    kfree(g_codec_dev);
    g_codec_dev = NULL;
    incodec_spk_gpio_free();

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
