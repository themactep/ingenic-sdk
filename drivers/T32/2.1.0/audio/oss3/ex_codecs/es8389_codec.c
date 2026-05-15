#include <linux/module.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/gfp.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include "../include/codec-common.h"
#include "../include/audio_common.h"

#define EXCODEC_ID_REG 0xfe
#define EXCODEC_ID_VAL 0xff

static int reset_gpio = -1;
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int spk_level = 1;
module_param(spk_level, int, S_IRUGO);
MODULE_PARM_DESC(spk_level, "Speaker active level: -1 disable, 0 low active, 1 high active");

#ifdef CONFIG_SOC_PRJ008
static int spk_gpio =  GPIO_PC(4);
#elif defined(CONFIG_SOC_PRJ007)
static int spk_gpio =  GPIO_PB(31);
#endif

module_param(spk_gpio, int, S_IRUGO);
MODULE_PARM_DESC(spk_gpio, "Speaker GPIO NUM");

/*example:
 *
 * gpio_func=0,1,0x1f mean that GPIOA(0) ~ GPIOA(4), FUNCTION1
 *
 * */
static char gpio_func[16] = "0,0,0x0";
module_param_string(gpio_func, gpio_func, sizeof(gpio_func), S_IRUGO);
MODULE_PARM_DESC(gpio_func, "the configure of codec gpio function;\nexample: insmod xxx.ko gpio_func=0,0,0x1f");


struct excodec_driver_data {
	struct codec_attributes *attrs;
	/* gpio definition */
	int reset_gpio;
	int port;
	int func;
	unsigned int gpios;
};

struct excodec_driver_data *es8389_data;

static int es8389_spk_gpio_request(void)
{
	if(spk_gpio != -1) {
		if (gpio_request(spk_gpio,"gpio_spk_en") < 0) {
			gpio_free(spk_gpio);
			printk(KERN_DEBUG"request spk en gpio %d error!\n", spk_gpio);
			return -1;
		}
		printk("excodec spk gpio request success!\n");
	}
	return 0;
}

static int es8389_spk_gpio_free(void)
{
	if (spk_gpio != -1) {
		gpio_set_value(spk_gpio, !spk_level);
		gpio_free(spk_gpio);
	}
	return 0;
}

static unsigned int es8389_i2c_read(u8 *reg,int reglen,u8 *data,int datalen)
{
	struct i2c_msg xfer[2];
	int ret;
	struct i2c_client *client = get_codec_devdata(es8389_data->attrs);

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reglen;
	xfer[0].buf = reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = datalen;
	xfer[1].buf = data;

	ret = i2c_transfer(client->adapter, xfer, 2);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return -ret;
	else
		return -EIO;
}

static int es8389_i2c_write(unsigned int reg,unsigned int value)
{
	int ret = -1;
	unsigned char tx[2];
	size_t	len = 2;
	struct i2c_client *client = get_codec_devdata(es8389_data->attrs);
#ifdef EXCODEC_I2C_CHECK
	unsigned char value_read;
#endif

	tx[0] = reg;
	tx[1] = value;
	ret = i2c_master_send(client,tx, len );

#ifdef EXCODEC_I2C_CHECK
	value_read = es8389_reg_read(reg);
	if(value != value_read)
		printk("\t[EXCODEC_I2C_CHECK] %s address = %02X Data = %02X data_check = %02X\n",__FUNCTION__, (int)reg, value,(int)value_read);
#endif
	if( ret != len ){
		return -EIO;
	}
	return 0;
}

static int es8389_reg_read(unsigned int reg)
{
	unsigned char tx[1], rx[1];
	int	wlen, rlen;
	int ret = 0;
	unsigned int rdata;

	wlen = 1;
	rlen = 1;
	tx[0] = (unsigned char)(0x7F & reg);

	ret = es8389_i2c_read(tx, wlen, rx, rlen);

	if (ret < 0) {
		printk("\t[EXCODEC] %s error ret = %d\n",__FUNCTION__, ret);
		rdata = -EIO;
	}
	else {
		rdata = (unsigned int)rx[0];
	}
	return rdata;
}

static int es8389_reg_write(unsigned int reg,unsigned int value)
{
	return es8389_i2c_write(reg,value);
}

static int es8389_reg_set(unsigned char reg, int start, int end, int val)
{
	int ret;
	int i = 0, mask = 0;
	unsigned char oldv = 0, new = 0;
	for(i = 0; i < (end-start + 1); i++) {
		mask += (1 << (start + i));
	}
	oldv = es8389_reg_read(reg);
	new = oldv & (~mask);
	new |= val << start;
	ret = es8389_reg_write(reg, new);
	if(ret < 0) {
		printk("fun:%s,EXCODEC I2C Write error.\n",__func__);
	}
	return ret;
}

static int dump_es8389_codec_regs(void)
{
	unsigned int i = 0;
	unsigned char value = 0;
	for (i = 0x00; i <= 0xff; i++) {
		value = es8389_reg_read(i);
		printk("es8389 reg_addr=0x%02x, value=0x%02x\n", i, value);
	}

	return 0;
}

static void es8389_adc_mute(int mute)
{
	if(mute) {
		es8389_reg_set(0x20, 0, 1, 0x3);
	}
	else {
		es8389_reg_set(0x20, 0, 1, 0x0);
	}
}

static void es8389_dac_mute(int mute)
{
	if(mute) {
		es8389_reg_set(0x40, 0, 1, 0x3);
	}
	else {
		es8389_reg_set(0x40, 0, 1, 0x0);
	}
}

static struct codec_attributes es8389_attrs;
static int es8389_init(void)
{
	es8389_reg_write(0xf3, 0x00);
	es8389_reg_write(0x00, 0x7e);
	es8389_reg_write(0xf3, 0x38);
	es8389_reg_write(0x45, 0x03);
	es8389_reg_write(0x60, 0x2a);
	es8389_reg_write(0x61, 0xc9);
	es8389_reg_write(0x62, 0x7f);
	es8389_reg_write(0x63, 0x06);
	es8389_reg_write(0x6b, 0x00);
	es8389_reg_write(0x6d, 0x28);
	es8389_reg_write(0x72, 0x10);
	es8389_reg_write(0x73, 0x10);
	es8389_reg_write(0x10, 0xc4);
	es8389_reg_write(0x01, 0x08);
	es8389_reg_write(0xf1, 0xc0);
	es8389_reg_write(0x12, 0x01);
	es8389_reg_write(0x13, 0x01);
	es8389_reg_write(0x14, 0x01);
	es8389_reg_write(0x15, 0x01);
	es8389_reg_write(0x16, 0x3f);
	es8389_reg_write(0x17, 0xf9);
	es8389_reg_write(0x18, 0x09);
	es8389_reg_write(0x19, 0x01);
	es8389_reg_write(0x1a, 0x01);
	es8389_reg_write(0x1b, 0x3f);
	es8389_reg_write(0x1c, 0x11);
	es8389_reg_write(0xf0, 0x12);
	es8389_reg_write(0x02, 0x00);
	es8389_reg_write(0x04, 0x00);
	es8389_reg_write(0x05, 0x10);
	es8389_reg_write(0x06, 0x00);
	es8389_reg_write(0x07, 0xc0);
	es8389_reg_write(0x08, 0x00);
	es8389_reg_write(0x09, 0xc0);
	es8389_reg_write(0x0a, 0x80);
	es8389_reg_write(0x0b, 0x04);
	es8389_reg_write(0x0c, 0x01);
	es8389_reg_write(0x0d, 0x00);
	es8389_reg_write(0x0f, 0x00);
	es8389_reg_write(0x21, 0x1f);
	es8389_reg_write(0x22, 0x7f);
	es8389_reg_write(0x41, 0x7f);
	es8389_reg_write(0x42, 0x7f);
	es8389_reg_write(0x00, 0x00);
	es8389_reg_write(0x03, 0xc1);
	es8389_reg_write(0x00, 0x01);
	es8389_reg_write(0x43, 0x10);
	es8389_reg_write(0x49, 0x0f);
	es8389_reg_set(0x20, 0, 1, 0x3);
	es8389_reg_set(0x40, 0, 1, 0x3);
	//adc dac 16bit
	es8389_reg_set(0x20, 5, 7, 0x3);
	es8389_reg_set(0x40, 5, 7, 0x3);
	if (es8389_attrs.mode == CODEC_IS_MASTER_MODE) {
		es8389_reg_set(0x01, 0, 0, 0x1);
	}
	else {
		es8389_reg_set(0x01, 0, 0, 0x0);
	}
	//dac dgain
	es8389_reg_write(0x46, 0x96);
	es8389_reg_write(0x47, 0x96);
	//adc dgain
	es8389_reg_write(0x27, 0xff);
	es8389_reg_write(0x28, 0xff);

	return 0;
}

static int es8389_enable_record(void)
{
	es8389_reg_write(0x69, 0x23);
	es8389_reg_write(0x61, 0xf9);
	es8389_reg_write(0x64, 0x8f);
	es8389_reg_write(0x10, 0xe4);
	es8389_reg_write(0x00, 0x01);
	es8389_reg_write(0x03, 0xc3);
	msleep(50);
	es8389_reg_write(0x4d, 0x00);
	es8389_adc_mute(0);
	return AUDIO_SUCCESS;
}

static int es8389_enable_playback(void)
{
	int val = -1;
	es8389_reg_write(0x69, 0x23);
	es8389_reg_write(0x61, 0xf9);
	es8389_reg_write(0x64, 0x8f);
	es8389_reg_write(0x10, 0xe4);
	es8389_reg_write(0x00, 0x01);
	es8389_reg_write(0x03, 0xc3);
	msleep(50);
	es8389_reg_write(0x4d, 0x00);
	es8389_dac_mute(0);

	if (spk_gpio > 0) {
		val = gpio_get_value(spk_gpio);
		gpio_direction_output(spk_gpio, spk_level);
		val = gpio_get_value(spk_gpio);
	}

	return AUDIO_SUCCESS;
}

static int es8389_disable_record(void)
{
	//dump_es8389_codec_regs();
	es8389_adc_mute(1);
	es8389_reg_write(0x10, 0xd4);
	msleep(50);
	es8389_reg_write(0x61, 0x59);
	es8389_reg_write(0x64, 0x00);
	es8389_reg_write(0x03, 0x00);
	es8389_reg_write(0x00, 0x7e);
	return AUDIO_SUCCESS;
}

static int es8389_disable_playback(void)
{
	//dump_es8389_codec_regs();
	if (spk_gpio > 0)
		gpio_direction_output(spk_gpio,!spk_level);

	es8389_dac_mute(1);
	es8389_reg_write(0x10, 0xd4);
	msleep(50);
	es8389_reg_write(0x61, 0x59);
	es8389_reg_write(0x64, 0x00);
	es8389_reg_write(0x03, 0x00);
	es8389_reg_write(0x00, 0x7e);
	return AUDIO_SUCCESS;
}

static int es8389_set_sample_rate(unsigned int rate)
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

	switch(rate_fs[i]) {
    	case  7: //8000
				es8389_reg_write(0x04, 0x01);
				es8389_reg_write(0x05, 0x45);
				es8389_reg_write(0x06, 0x04);
				es8389_reg_write(0x07, 0xd0);
				es8389_reg_write(0x08, 0x03);
				es8389_reg_write(0x09, 0xc1);
				es8389_reg_write(0x0a, 0xb0);
				es8389_reg_write(0x11, 0x00);
				es8389_reg_write(0x21, 0x1f);
				es8389_reg_write(0x22, 0x7f);
				es8389_reg_write(0x41, 0xff);
				es8389_reg_write(0x42, 0x7f);
				es8389_reg_write(0x43, 0x01);
				es8389_reg_set(0x44, 6, 7, 0x2);
				break;
    	case 6: //12000
				break;
    	case 5: //16000
				es8389_reg_write(0x04, 0x00);
				es8389_reg_write(0x05, 0x41);
				es8389_reg_write(0x06, 0x04);
				es8389_reg_write(0x07, 0xc0);
				es8389_reg_write(0x08, 0x01);
				es8389_reg_write(0x09, 0xd1);
				es8389_reg_write(0x0a, 0x90);
				es8389_reg_write(0x11, 0x00);
				es8389_reg_write(0x21, 0x1f);
				es8389_reg_write(0x22, 0x7f);
				es8389_reg_write(0x41, 0xff);
				es8389_reg_write(0x42, 0x7f);
				es8389_reg_write(0x43, 0x00);
				es8389_reg_set(0x44, 6, 7, 0x2);
				break;
    	case 4: //24000
				break;
    	case 3: //32000
				break;
		case 2: //44100;
				break;
     	case 1: //48000
				es8389_reg_write(0x04, 0x01);
				es8389_reg_write(0x05, 0x01);
				es8389_reg_write(0x06, 0x04);
				es8389_reg_write(0x07, 0xd0);
				es8389_reg_write(0x08, 0x00);
				es8389_reg_write(0x09, 0xd1);
				es8389_reg_write(0x0a, 0x80);
				es8389_reg_write(0x11, 0x00);
				es8389_reg_write(0x21, 0x1f);
				es8389_reg_write(0x22, 0x7f);
				es8389_reg_write(0x41, 0x7f);
				es8389_reg_write(0x42, 0x7f);
				es8389_reg_write(0x43, 0x00);
				es8389_reg_set(0x44, 6, 7, 0x0);
				break;
     	case 0: //96000
				es8389_reg_write(0x04, 0x00);
				es8389_reg_write(0x05, 0x40);
				es8389_reg_write(0x06, 0x00);
				es8389_reg_write(0x07, 0xc0);
				es8389_reg_write(0x08, 0x10);
				es8389_reg_write(0x09, 0xc1);
				es8389_reg_write(0x0a, 0x80);
				es8389_reg_write(0x11, 0x00);
				es8389_reg_write(0x21, 0x9f);
				es8389_reg_write(0x22, 0x7f);
				es8389_reg_write(0x41, 0x7f);
				es8389_reg_write(0x42, 0x7f);
				es8389_reg_write(0x43, 0x80);
				es8389_reg_set(0x44, 6, 7, 0x0);
				break;
    	default:
				printk("error:(%s,%d), error audio sample rate.\n",__func__,__LINE__);
				break;
	}
	return AUDIO_SUCCESS;
}

static int es8389_record_set_datatype(struct audio_data_type *type)
{
	int ret = AUDIO_SUCCESS;
	if(!type) return -AUDIO_EPARAM;
	ret |= es8389_set_sample_rate(type->sample_rate);
	return ret;
}

static int es8389_record_set_endpoint(int enable)
{
	if(enable){
		es8389_enable_record();
	}else{
		es8389_disable_record();
	}
	return AUDIO_SUCCESS;
}

static int es8389_record_set_again(int channel, int again)
{
	int volume = 0;
	if (again < 0) again = 0;
	if (again > 0xe) again = 0xe;
	volume = again;
	es8389_reg_set(0x72, 0, 3, volume);
	es8389_reg_set(0x73, 0, 3, volume);
	return 0;
}

static int es8389_record_set_dgain(int channel, int dgain)
{
	return 0;
}

static int es8389_playback_set_datatype(struct audio_data_type *type)
{
	int ret = AUDIO_SUCCESS;
	if(!type) return -AUDIO_EPARAM;
	ret |= es8389_set_sample_rate(type->sample_rate);
	return ret;
}

static int es8389_playback_set_endpoint(int enable)
{
	if(enable){
		es8389_enable_playback();
	}else{
		es8389_disable_playback();
	}
	return AUDIO_SUCCESS;
}

static int es8389_playback_set_again(int channel, int again)
{
	return 0;
}

static int es8389_playback_set_dgain(int channel, int dgain)
{
	int volume = 0;
	if (dgain < 0) dgain = 0;
	if (dgain > 0xff) dgain = 0xff;

	volume = dgain;
	es8389_reg_write(0x46, volume);
	es8389_reg_write(0x47, volume);
	return 0;
}

static struct codec_sign_configure es8389_codec_sign = {
	.data = DATA_I2S_MODE,
	.seq = LEFT_FIRST_MODE,
	.sync = LEFT_LOW_RIGHT_HIGH,
	.rate2mclk = 256,
};

static struct codec_endpoint es8389_record = {
	.set_datatype = es8389_record_set_datatype,
	.set_endpoint = es8389_record_set_endpoint,
	.set_again = es8389_record_set_again,
	.set_dgain = es8389_record_set_dgain,
};

static struct codec_endpoint es8389_playback = {
	.set_datatype = es8389_playback_set_datatype,
	.set_endpoint = es8389_playback_set_endpoint,
	.set_again = es8389_playback_set_again,
	.set_dgain = es8389_playback_set_dgain,
};

static int es8389_detect(struct codec_attributes *attrs)
{
	struct i2c_client *client = get_codec_devdata(attrs);
	unsigned int ident = 0;
	int ret = AUDIO_SUCCESS;

	if(reset_gpio != -1){
		ret = gpio_request(reset_gpio,"es8389_reset");
		if(!ret){
			/*different codec operate differently*/
			gpio_direction_output(reset_gpio, 0);
			msleep(5);
			gpio_set_value(reset_gpio, 0);
			msleep(10);
			gpio_set_value(reset_gpio, 1);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	ident = es8389_reg_read(EXCODEC_ID_REG);
	if (ident < 0 || ident != EXCODEC_ID_VAL){
		printk("chip found @ 0x%x (%s) is not an %s chip.\n",
				client->addr, client->adapter->name,"es8389");
		return ret;
	}
	return 0;
}

static int es8389_set_sign(struct codec_sign_configure *sign)
{
	struct codec_sign_configure *this = &es8389_codec_sign;
	if(!sign)
		return -AUDIO_EPARAM;
	if(sign->data != this->data || sign->seq != this->seq || sign->sync != this->sync || sign->rate2mclk != this->rate2mclk)
		return -AUDIO_EPARAM;
	return AUDIO_SUCCESS;
}

static int es8389_set_power(int enable)
{
	if(enable){
		es8389_init();
	}else{
		es8389_disable_record();
		es8389_disable_playback();
	}
	return AUDIO_SUCCESS;
}

static struct codec_attributes es8389_attrs = {
	.name = "es8389",
	.mode = CODEC_IS_MASTER_MODE,
	.type = CODEC_IS_EXTERN_MODULE,
	.pins = CODEC_IS_4_LINES,
	.transfer_protocol = DATA_I2S_MODE,
	.set_sign = es8389_set_sign,
	.set_power = es8389_set_power,
	.detect = es8389_detect,
	.record = &es8389_record,
	.playback = &es8389_playback,
	.spk_gpio_request = es8389_spk_gpio_request,
	.spk_gpio_free = es8389_spk_gpio_free,
};

static int es8389_gpio_init(struct excodec_driver_data *es8389_data)
{
	es8389_data->reset_gpio = reset_gpio;
	sscanf(gpio_func,"%d,%d,0x%x", &es8389_data->port, &es8389_data->func, &es8389_data->gpios);
	if(es8389_data->gpios){
		return audio_set_gpio_function(es8389_data->port, es8389_data->func, es8389_data->gpios);
	}
	return AUDIO_SUCCESS;
}

static int es8389_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
	int ret = 0;
	es8389_data = (struct excodec_driver_data *)kzalloc(sizeof(struct excodec_driver_data), GFP_KERNEL);
	if(!es8389_data) {
		printk("failed to alloc es8389 driver data\n");
		return -ENOMEM;
	}
	memset(es8389_data, 0, sizeof(*es8389_data));
	es8389_data->attrs = &es8389_attrs;
	if(es8389_gpio_init(es8389_data) < 0){
		printk("failed to init es8389 gpio function\n");
		ret = -EPERM;
		goto failed_init_gpio;
	}
	set_codec_devdata(&es8389_attrs,i2c);
	set_codec_hostdata(&es8389_attrs, &es8389_data);
	i2c_set_clientdata(i2c, &es8389_attrs);

	printk("probe ok -------->es8389.\n");
	return 0;

failed_init_gpio:
	kfree(es8389_data);
	es8389_data = NULL;
	return ret;
}

static int es8389_i2c_remove(struct i2c_client *client)
{
	struct excodec_driver_data *excodec = es8389_data;
	if(excodec){
		kfree(excodec);
		es8389_data = NULL;
	}
	es8389_spk_gpio_free();

	return 0;
}

static const struct i2c_device_id es8389_i2c_id[] = {
	{ "es8389", 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, es8389_i2c_id);

static struct i2c_driver es8389_i2c_driver = {
	.driver = {
		.name = "es8389",
		.owner = THIS_MODULE,
	},
	.probe = es8389_i2c_probe,
	.remove = es8389_i2c_remove,
	.id_table = es8389_i2c_id,
};

static int __init init_es8389(void)
{
	int ret = 0;

	ret = i2c_add_driver(&es8389_i2c_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "Failed to register external codec I2C driver: %d\n", ret);
	}
	return ret;
}

static void __exit exit_es8389(void)
{
	i2c_del_driver(&es8389_i2c_driver);
}


module_init(init_es8389);
module_exit(exit_es8389);

MODULE_DESCRIPTION("ASoC external codec driver");
MODULE_LICENSE("GPL v2");
