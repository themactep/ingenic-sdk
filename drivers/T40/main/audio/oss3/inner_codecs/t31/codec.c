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
#include "../../include/audio_debug.h"
#include "codec.h"

static int spk_gpio =  GPIO_PB(31);
module_param(spk_gpio, int, S_IRUGO);
MODULE_PARM_DESC(spk_gpio, "Speaker GPIO NUM");

static int spk_level = 1;
module_param(spk_level, int, S_IRUGO);
MODULE_PARM_DESC(spk_level, "Speaker active level: -1 disable, 0 low active, 1 high active");

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

	if (new != codec_reg_read(g_codec_dev, reg)) {
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
	for (i = 0; i < 0x140; i += 4) {
		printk("reg = 0x%04x [0x%04x]:		val = 0x%04x\n", i, i/4, codec_reg_read(g_codec_dev, i));
	}
	printk("Dump T31 Codec register End.\n");

	return i;
}

int codec_init(void)
{
	int i = 0;
	char value = 0;
	codec_reg_set(TS_CODEC_CGR_00, 0, 1, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CGR_00, 0, 1, 0x3);
	msleep(10);
	/* Choose DAC I2S Master Mode */
	codec_reg_set(TS_CODEC_CACR2_03, 0, 7, 0x3e);
	codec_reg_set(TS_CODEC_CDCR1_04, 0, 7, 0x10);//DAC I2S interface is I2S Mode.
	codec_reg_set(TS_CODEC_CDCR2_05, 0, 7, 0x3e);

	codec_reg_set(TS_CODEC_CANACR_26, 0, 1, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CCR_21, 0, 6, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CAACR_22, 5, 5, 1);//setup reference voltage
	msleep(10);
	for (i = 0; i <= 6; i++) {
		value |= value<<1 | 1;
		codec_reg_set(TS_CODEC_CCR_21, 0, 6, value);
		msleep(30);
	}
	msleep(20);

	return 0;
}

static int codec_enable_record(void)
{
	/* enable POP sound control, low vcm power */
	codec_reg_set(TS_CODEC_CANACR_26, 0, 1, 0x2);
	msleep(10);

	/* enable hpf and set mode 0 */
	codec_reg_set(TS_CODEC_CGAINLR_09, 0, 4, 0x8);
	msleep(10);
	/* Choose ADC I2S interface mode */
	codec_reg_set(TS_CODEC_CACR_02, 3, 4, 0x2);
	msleep(10);
	codec_reg_set(TS_CODEC_CACR_02, 7, 7, 0x1);
	msleep(10);
	/* Choose ADC I2S Master Mode */
	codec_reg_set(TS_CODEC_CACR2_03, 0, 7, 0x3e);
	msleep(10);

	/* Enable ADC */
	codec_reg_set(TS_CODEC_CMICCR_23, 7, 7, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CAACR_22, 4, 4, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CAACR_22, 3, 3, 0x1);   //micbias enable
	msleep(10);
	codec_reg_set(TS_CODEC_CAACR_22, 0, 2, 0x7);   //micbias
	msleep(10);
	codec_reg_set(TS_CODEC_CMICCR_23, 5, 5, 0x1);  //ref voltage
	msleep(10);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 4, 4, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 5, 5, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR_26, 6, 6, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR_26, 5, 5, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR_26, 4, 4, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR_26, 7, 7, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CMICCR_23, 6, 6, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 6, 7, 0); //mic hw gain
	msleep(10);
	codec_reg_set(TS_CODEC_CALCGR_25, 0, 4, 0x10);
	msleep(10);

	/* low power consumption */
	codec_reg_set(TS_CODEC_CCR_21, 0, 6, 0x1);
	msleep(11);
	codec_reg_set(TS_CODEC_CMICCR_23, 4, 4, 1);
	msleep(10);

	return 0;
}

static int codec_enable_playback(void)
{
	int val = -1;

	/* low power consumption */
	codec_reg_set(TS_CODEC_POWER_20, 0, 2, 0x7);
	codec_reg_set(TS_CODEC_CMICCR_23, 0, 3, 0xf);

	/* enable current source */
	codec_reg_set(TS_CODEC_CANACR_26, 3, 3, 0x1);
	msleep(10);

	/* enable reference voltage */
	codec_reg_set(TS_CODEC_CANACR_26, 2, 2, 0x1);
	msleep(10);
	/* enable POP sound control */
	codec_reg_set(TS_CODEC_CANACR_26, 0, 1, 0x2);
	msleep(10);

	codec_reg_set(TS_CODEC_CHR_28, 5, 5, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CHR_28, 6, 6, 0x1);
	msleep(10);

	codec_reg_set(TS_CODEC_CANACR2_27, 7, 7, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR2_27, 6, 6, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR2_27, 5, 5, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR2_27, 4, 4, 0x1);
	msleep(10);
	codec_reg_set(TS_CODEC_CHR_28, 7, 7, 0x1);
	msleep(10);

	/* set replay volume */
	codec_reg_set(TS_CODEC_CHR_28, 0, 4, 0x18);
	msleep(10);

	/* low power consumption */
	codec_reg_set(TS_CODEC_CCR_21, 0, 6, 0x1);
	msleep(10);

	if (g_codec_dev->spk_en.gpio > 0) {
		val = gpio_get_value(g_codec_dev->spk_en.gpio);
		gpio_direction_output(g_codec_dev->spk_en.gpio, g_codec_dev->spk_en.active_level);
		val = gpio_get_value(g_codec_dev->spk_en.gpio);
	}

	return 0;
}

static int codec_disable_record(void)
{
	codec_reg_set(TS_CODEC_CMICCR_23, 4, 4, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR_26, 5, 5, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR_26, 6, 6, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 5, 5, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CMICGAINR_24, 4, 4, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CMICCR_23, 5, 5, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CAACR_22, 4, 4, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR_26, 4, 4, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR_26, 7, 7, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CMICCR_23, 6, 6, 0);
	msleep(10);
	return AUDIO_SUCCESS;
}

static int codec_disable_playback(void)
{
	if (g_codec_dev->spk_en.gpio > 0)
		gpio_direction_output(g_codec_dev->spk_en.gpio,
				!g_codec_dev->spk_en.active_level);
	codec_reg_set(TS_CODEC_CHR_28, 0, 4, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CHR_28, 7, 7, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CHR_28, 6, 6, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CHR_28, 5, 5, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR2_27, 5, 5, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR2_27, 6, 6, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR2_27, 7, 7, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR_26, 3, 3, 0);
	msleep(10);
	codec_reg_set(TS_CODEC_CANACR2_27, 4, 4, 0);
	msleep(10);

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

	return 0;
}

#define INCODEC_CHANNEL_MONO 1
#define INCODEC_CHANNEL_STEREO 2
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
	}

	if(this->frame_vsize != type->frame_vsize){
		if(type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_16BIT
				&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_20BIT
				&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_24BIT ){
			printk("inner codec ADC only support 16bit,20bit,24bit for record!\n");
		}else{
			/* ADC valid data length */
			if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_16BIT)
				codec_reg_set(TS_CODEC_CACR_02, 5, 6, ADC_VALID_DATA_LEN_16BIT);
			else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_20BIT)
				codec_reg_set(TS_CODEC_CACR_02, 5, 6, ADC_VALID_DATA_LEN_20BIT);
			else
				codec_reg_set(TS_CODEC_CACR_02, 5, 6, ADC_VALID_DATA_LEN_24BIT);
			msleep(10);
			this->frame_vsize = type->frame_vsize;
		}
	}

	if(this->sample_rate != type->sample_rate){
		this->sample_rate = type->sample_rate;
		ret |= codec_set_sample_rate(type->sample_rate);
	}

	if(this->sample_channel != type->sample_channel){
		if(type->sample_channel != INCODEC_CHANNEL_MONO && type->sample_channel != INCODEC_CHANNEL_STEREO){
			printk("inner codec only support mono or stereo for record!\n");
			ret |= AUDIO_EPARAM;
		}else{
			/* Choose ADC chn */
			if(type->sample_channel == INCODEC_CHANNEL_MONO)
				codec_reg_set(TS_CODEC_CACR_02, 0, 0, CHOOSE_ADC_CHN_MONO);
			else
				codec_reg_set(TS_CODEC_CACR_02, 0, 0, CHOOSE_ADC_CHN_STEREO);
			msleep(10);
			this->sample_channel = type->sample_channel;
			printk("%s %d: inner codec set record channel = %d\n",__func__ , __LINE__,type->sample_channel);
		}
	}
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

static int codec_record_set_again(struct codec_analog_gain *again)
{
	/******************************************\
	| vol :0	1		2	..	30  31	       |
	| gain:-18  -16.5  -15	..	27  28.5 (+DB) |
	\******************************************/
	if(!again || !g_codec_dev)
		return -AUDIO_EPARAM;
	if(again->lmode == CODEC_ANALOG_AUTO){/*use ALC*/
		/*keep agc range(min,max) default*/
		codec_reg_set(TS_CODEC_CAFG_49, 6, 6, 1);
		if(again->gain.gain[0] < 0) again->gain.gain[0] = 0;
		if(again->gain.gain[0] > 7) again->gain.gain[0] = 7;
		codec_reg_set(TS_CODEC_CAFG_49, 3, 5, again->gain.gain[0]);
		codec_reg_set(TS_CODEC_CAFG_49, 0, 2, again->gain.gain[0]);
		msleep(10);
		codec_reg_set(TS_CODEC_CGAINLR_09, 5, 5, 1);
	}else{
		codec_reg_set(TS_CODEC_CAFG_49, 6, 6, 0);
		msleep(10);
		if(again->gain.gain[0] < 0) again->gain.gain[0] = 0;
		if(again->gain.gain[0] > 31) again->gain.gain[0] = 31;
		codec_reg_set(TS_CODEC_CALCGR_25, 0, 4, again->gain.gain[0]);
	}
	msleep(10);
	return 0;
}

static int codec_record_set_dgain(struct volume gain)
{
	return 0;
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
	}
	if(this->frame_vsize != type->frame_vsize){
		audio_info_print("type->frame_vsize=%u, we support %d %d %d\n", type->frame_vsize, INCODEC_CODEC_VALID_DATA_SIZE_16BIT, INCODEC_CODEC_VALID_DATA_SIZE_20BIT, INCODEC_CODEC_VALID_DATA_SIZE_24BIT);
		if(type->frame_vsize != 16
				&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_20BIT
				&& type->frame_vsize != INCODEC_CODEC_VALID_DATA_SIZE_24BIT ){
			printk("inner codec DAC only support 16bit,20bit,24bit for record!type->frame_vsize=%u\n", type->frame_vsize);
		}else{
			/* ADC valid data length */
			if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_16BIT)
				codec_reg_set(TS_CODEC_CDCR1_04 , 5, 6, ADC_VALID_DATA_LEN_16BIT);
			else if(type->frame_vsize == INCODEC_CODEC_VALID_DATA_SIZE_20BIT)
				codec_reg_set(TS_CODEC_CDCR1_04 , 5, 6, ADC_VALID_DATA_LEN_20BIT);
			else
				codec_reg_set(TS_CODEC_CDCR1_04 , 5, 6, ADC_VALID_DATA_LEN_24BIT);
			msleep(10);
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
		}else{
			this->sample_channel = type->sample_channel;
		}
	}
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

	codec_reg_set(TS_CODEC_CHR_28, 0, 4, again->gain.gain[0]);
	return 0;
}

static int codec_playback_set_dgain(struct volume gain)
{
	return 0;
}

/* debug codec info */
static int inner_codec_show(struct seq_file *m, void *v)
{
	int len = 0;

	len += seq_printf(m ,"The name of codec is ingenic inner codec\n");

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
};

static struct codec_endpoint codec_playback = {
	.set_datatype = codec_playback_set_datatype,
	.set_endpoint = codec_playback_set_endpoint,
	.set_again = codec_playback_set_again,
	.set_dgain = codec_playback_set_dgain,
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
