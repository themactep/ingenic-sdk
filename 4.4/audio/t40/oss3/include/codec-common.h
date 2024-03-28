#ifndef __TX_CODEC_COMMON_H__
#define __TX_CODEC_COMMON_H__
#include <linux/gpio.h>
#include <soc/gpio.h>
#include "audio_common.h"

static inline int audio_set_gpio_function(int port, int func, unsigned int gpio)
{
	int ret = 0;
	enum gpio_port ports[] = {
		GPIO_PORT_A,
		GPIO_PORT_B,
		GPIO_PORT_C,
	};
	enum gpio_function funcs[] = {
		GPIO_FUNC_0,
		GPIO_FUNC_1,
		GPIO_FUNC_2,
		GPIO_FUNC_3,
	};

	if(port >= 3 || func >= 4){
		printk("Failed to set gpio function:port = %d, func = %d, gpio = 0x%08x\n", port, func, gpio);
		return -AUDIO_EPERM;
	}
	ret = jzgpio_set_func(ports[port], funcs[func], gpio);
	if(!ret)
		ret = AUDIO_SUCCESS;
	return ret;
}

enum codec_data_mode {
	DATA_RIGHT_JUSTIFIED_MODE,
	DATA_LEFT_JUSTIFIED_MODE,
	DATA_I2S_MODE,
};

enum codec_sync_polarity {
	LEFT_HIGH_RIGHT_LOW,
	LEFT_LOW_RIGHT_HIGH,
};

enum codec_sequence_mode {
	LEFT_FIRST_MODE,
	RIGHT_FIRST_MODE,
};

enum codec_mode {
	CODEC_IS_MASTER_MODE,
	CODEC_IS_SLAVE_MODE,
};

struct codec_sign_configure {
	enum codec_data_mode data;
	enum codec_sequence_mode seq;
	enum codec_sync_polarity sync;
	unsigned int rate2mclk;				// this is the coefficient rate to mclk, the default value is 256.
};

struct audio_data_type {
	unsigned int frame_size;
	unsigned int frame_vsize; // valid size in a frame size
	unsigned int sample_rate;
	unsigned int sample_channel;
};

enum codec_analog_mode {
	CODEC_ANALOG_AUTO = 1,
	CODEC_ANALOG_MANUAL,
};

/*
 * when mode is auto, gain is the default value of agc.
 * when mode is manaul, maxvol, minvol, maxgain and mingain are invalid.
 * */
struct codec_analog_gain {
	enum codec_analog_mode lmode;
	enum codec_analog_mode rmode;
	unsigned short maxvol;
	unsigned short minvol;
	unsigned short maxgain;
	unsigned short mingain;
	struct volume gain;
};

struct codec_endpoint {
	int (*set_datatype)(struct audio_data_type *type);
	int (*set_endpoint)(int enable);
	int (*set_again)(struct codec_analog_gain *gain);
	int (*set_dgain)(struct volume gain);
	int (*set_mic_hpf_en)(int en);
	int (*set_mute)(unsigned int channel, unsigned int en, unsigned int dgain);
	int (*set_alc_threshold)(struct alc_gain threshold);
};

enum codec_type {
	CODEC_IS_INNER_MODULE,
	CODEC_IS_EXTERN_MODULE,
};

enum codec_pins {
	CODEC_IS_0_LINES, // this is a inner codec.
	CODEC_IS_4_LINES,
	CODEC_IS_6_LINES,
};

struct codec_attributes {
	char name[16];
	enum codec_mode mode;
	enum codec_type type;
	enum codec_pins pins;
	enum codec_data_mode transfer_protocol;
	int (*set_sign)(struct codec_sign_configure *sign);
	int (*set_power)(int enable);
	int (*detect)(struct codec_attributes *attrs);
	struct file_operations *debug_ops;
	struct codec_endpoint *record;
	struct codec_endpoint *playback;
	void *host_priv;	/* save this module pointer */
	void *dev_priv;		/* save device message */
};

static inline void set_codec_devdata(struct codec_attributes *attrs, void *priv)
{
	if(attrs)
		attrs->dev_priv = priv;
}

static inline void *get_codec_devdata(struct codec_attributes *attrs)
{
	if(attrs)
		return attrs->dev_priv;
	else
		return NULL;
}

static inline void set_codec_hostdata(struct codec_attributes *attrs, void *priv)
{
	if(attrs)
		attrs->host_priv = priv;
}

static inline void *get_codec_hostdata(struct codec_attributes *attrs)
{
	if(attrs)
		return attrs->host_priv;
	else
		return NULL;
}

struct codec_spk_gpio {
	int gpio;
	int active_level;
};

#endif// __TX_CODEC_COMMON_H__
