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
#include <linux/input-polldev.h>
#include <linux/input.h>
#include <linux/gfp.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include "../include/codec-common.h"
#include "../include/audio_common.h"

#define EXCODEC_I2C_CHECK
#define EXCODEC_ID_REG 0x00
#define EXCODEC_ID_VAL 0x06

static int reset_gpio = -1;
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

/*example:
 *
 * gpio_func=0,1,0x1f mean that GPIOA(0) ~ GPIOA(4), FUNCTION1
 *
 * */
static char gpio_func[16] = "2,1,0x1F00";
module_param_string(gpio_func, gpio_func, sizeof(gpio_func), S_IRUGO);
MODULE_PARM_DESC(gpio_func, "the configure of codec gpio function;\nexample: insmod xxx.ko gpio_func=0,0,0x1f");

struct _coeff_div {
    u32 mclk;
    u32 rate;
    u16 fs;
    u8 sr:4;
    u8 usb:1;
};

struct excodec_driver_data {
    struct codec_attributes *attrs;
    /* gpio definition */
    int reset_gpio;
    int port;
    int func;
    unsigned int gpios;
};

struct excodec_driver_data *es8388_data;

static unsigned int es8388_i2c_read(u8 *reg,int reglen,u8 *data,int datalen)
{
    struct i2c_msg xfer[2];
    int ret;
    struct i2c_client *client = get_codec_devdata(es8388_data->attrs);

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

static int es8388_reg_read(unsigned int reg);
static int es8388_i2c_write(unsigned int reg,unsigned int value)
{
	int ret = -1;
	unsigned char tx[2];
	size_t  len = 2;
	struct i2c_client *client = get_codec_devdata(es8388_data->attrs);
#ifdef EXCODEC_I2C_CHECK
	unsigned char value_read;
#endif

	tx[0] = reg;
	tx[1] = value;
	ret = i2c_master_send(client, tx, len);

#ifdef EXCODEC_I2C_CHECK
	value_read = es8388_reg_read(reg);
	if(value != value_read)
		printk("\t[EXCODEC_I2C_CHECK] %s address = %02X Data = %02X data_check = %02X\n",\
				__func__, (int)reg, value,(int)value_read);
#endif

	if(ret != len){
		return -EIO;
	}

	return 0;
}

static int es8388_reg_read(unsigned int reg)
{
    unsigned char tx[1], rx[1];
    int wlen, rlen;
    int ret = 0;
    unsigned int rdata;

    wlen = 1;
    rlen = 1;
    tx[0] = (unsigned char)(0x7F & reg);

    ret = es8388_i2c_read(tx, wlen, rx, rlen);

    if (ret < 0) {
        printk("\t[EXCODEC] %s error ret = %d\n",__FUNCTION__, ret);
        rdata = -EIO;
    }
    else {
        rdata = (unsigned int)rx[0];
    }
    return rdata;
}

static int es8388_reg_write(unsigned int reg,unsigned int value)
{
    return es8388_i2c_write(reg,value);
}

static int es8388_reg_set(unsigned char reg, int start, int end, int val)
{
    int ret;
    int i = 0, mask = 0;
    unsigned char oldv = 0, new = 0;
    for(i = 0; i < (end-start + 1); i++) {
        mask += (1 << (start + i));
    }
    oldv = es8388_reg_read(reg);
    new = oldv & (~mask);
    new |= val << start;
    ret = es8388_reg_write(reg, new);
    if(ret < 0) {
        printk("fun:%s,EXCODEC I2C Write error.\n",__func__);
    }
    return ret;
}

static struct codec_attributes es8388_attrs;
static int es8388_init(void)
{
	int ret = 0;

	/* reset */
	es8388_reg_write(0x00,0x80);//reset control port register to default
	ret = es8388_reg_write(0x00,0x00);
	if (ret < 0){
		printk("Failed to issue reset!\n");
		return ret;
	}

	es8388_reg_write(0x01,0x60);
    es8388_reg_write(0x02,0xF3);
    es8388_reg_write(0x02,0xF0);
    es8388_reg_write(0x2B,0x80);
    es8388_reg_write(0x00,0x36);//ADC clock is same as DAC. Use DAC MCLK as internal MCLK
    es8388_reg_write(0x08,0x00);//Master

	es8388_reg_write(0x04,0x00);//power up DAC
	// 0x05,0x00 ->low power
    es8388_reg_write(0x06,0xC3);//LPPGA & LPLMIX
    es8388_reg_write(0x19,0x02);//DAC no Mute
    es8388_reg_write(0x09,0x11);//ADC PGA GAIN
#if 0 //mic
	/*Differential*/
    es8388_reg_write(0x0A,0xF8);
    es8388_reg_write(0x0B,0x82);
#else //line in
    es8388_reg_write(0x0A,0x00);
    es8388_reg_write(0x0B,0x00);//接line in
#endif
    es8388_reg_write(0x0C,0x0C);//ADC I2S - 16Bit
    es8388_reg_write(0x0D,0x02);//ADCLRCK = MCLK/256
    es8388_reg_write(0x10,0x00);//LADC Vol 0dB
    es8388_reg_write(0x11,0x00);//RADC Vol 0dB

	es8388_reg_write(0x17,0x18);//DAC I2S - 16Bit
    es8388_reg_write(0x18,0x02);//DACLRCK = MCLK/256
    es8388_reg_write(0x1A,0x00);//LDAC Vol 0dB
    es8388_reg_write(0x1B,0x00);//RDAC Vol 0dB

    es8388_reg_write(0x27,0xB8);//Mix - L
    es8388_reg_write(0x2A,0xB8);//Mix - R

    es8388_reg_write(0x35,0xA0);//reserve
    es8388_reg_write(0x37,0xD0);
    es8388_reg_write(0x39,0xD0);
    usleep_range(18000, 20000);
    es8388_reg_write(0x2E,0x1E);//LOUT1VOL
    es8388_reg_write(0x2F,0x1E);//ROUT1VOL
    es8388_reg_write(0x30,0x1E);//LOUT2VOL
    es8388_reg_write(0x31,0x1E);//ROUT2VOL
	//Power up ADC, Enable LIN&RIN, Power down MICBIAS, set int1lp to low power mode
    es8388_reg_write(0x03,0x09);
    es8388_reg_write(0x02,0x00);//Start up FSM and DLL
    usleep_range(18000, 20000);

	return 0;
}

static int es8388_enable_record(void)
{
	es8388_reg_set(0x16, 1, 2, 0x00);//No Mute ADC outout

#if 0
	//debug
    int status;
	status = es8388_reg_read(0x3C);
	printk("in  status(0x3C):0x%x\n", status);
    status = es8388_reg_read(0x3D);
	printk("out status(0x3D):0x%x\n", status);
#endif
	return AUDIO_SUCCESS;
}

static int es8388_disable_record(void)
{
	es8388_reg_set(0x16, 1, 2, 0x01);//Mute ADC outout
	return AUDIO_SUCCESS;
}

static int es8388_enable_playback(void)
{
	//es8388_set_gpio(SET_SPK, !es8388->spk_gpio_level);
	es8388_reg_write(0x19,0x02);//DAC no mute
	es8388_reg_write(0x04,0x3C);//DAC Power up
	return AUDIO_SUCCESS;
}

static int es8388_disable_playback(void)
{
	es8388_reg_write(0x19,0x06);//DAC mute
	//es8388_set_gpio(SET_SPK, es8388->spk_gpio_level);
    es8388_reg_write(0x04,0x00);
	es8388_reg_write(0x04,0xC0);//DAC Power down
	return AUDIO_SUCCESS;
}

static int es8388_record_set_datatype(struct audio_data_type *type)
{
	int ret = AUDIO_SUCCESS;

	if (!type)return -AUDIO_EPARAM;
/*
	if (type->frame_vsize == 16)
		es8388_reg_set(0x0c, 2, 4, 0x03);//Set SFI for ADC(I2S-16bit)
	else if (type->frame_vsize == 20)
		es8388_reg_set(0x0c, 2, 4, 0x01);//Set SFI for ADC(I2S-20bit)
	else if (type->frame_vsize == 24)
		es8388_reg_set(0x0c, 2, 4, 0x00);//Set SFI for ADC(I2S-24bit)
	else {
		printk("ES8388 codec ADC only support:16,20,24bit for record\n");
		return -AUDIO_EPARAM;
	}
*/
#define CHANNEL_MONO_LEFT 1
#define CHANNEL_MONO_RIGHT 2
#define CHANNEL_STEREO 3
	if (type->sample_channel == CHANNEL_MONO_LEFT)
		es8388_reg_set(0x0B, 3, 4, 0x1);
	else if (type->sample_channel == CHANNEL_MONO_RIGHT)
		es8388_reg_set(0x0B, 3, 4, 0x2);
	else if (type->sample_channel == CHANNEL_STEREO)
		es8388_reg_set(0x0B, 3, 4, 0x0);

	return ret;
}

static int es8388_playback_set_datatype(struct audio_data_type *type)
{
	int ret = AUDIO_SUCCESS;

	if (!type)return -AUDIO_EPARAM;

	return ret;
}

void show_reg(void)
{
	int i = 0;
	int value = 0;

	for(;i < 52; i++){
		value = es8388_reg_read(i);
		printk("0x%02x value:%08x\n", i, value);
	}
}

static int es8388_record_set_endpoint(int enable)
{
	if (enable)
		es8388_enable_record();
	else
		es8388_disable_record();

	//show_reg();
	return AUDIO_SUCCESS;
}

static int es8388_playback_set_endpoint(int enable)
{
	if (enable)
		es8388_enable_playback();
	else
		es8388_disable_playback();
	return AUDIO_SUCCESS;
}

static int es8388_record_set_again(int channel, int again)
{
	unsigned char value;

	if (again < 3)
		value = 0x00;
	else if (again < 6)
		value = 0x11;
	else if (again < 9)
		value = 0x22;
	else if (again < 12)
		value = 0x33;
	else if (again < 15)
		value = 0x44;
	else if (again < 18)
		value = 0x55;
	else if (again < 21)
		value = 0x66;
	else if (again < 24)
		value = 0x77;
	else
		value = 0x88;

    es8388_reg_write(0x09, value);//ADC PGA GAIN
	printk("%s, channel:%d again:%x\n", __func__, channel, value);
	return AUDIO_SUCCESS;
}

static int es8388_record_set_dgain(int channel, int dgain)
{
	return AUDIO_SUCCESS;
}

static int es8388_playback_set_again(int channel, int again)
{
	return AUDIO_SUCCESS;
}

static int es8388_playback_set_dgain(int channel, int dgain)
{
	return AUDIO_SUCCESS;
}

static struct codec_sign_configure es8388_codec_sign = {
    .data = DATA_I2S_MODE,
    .seq = LEFT_FIRST_MODE,
    .sync = LEFT_LOW_RIGHT_HIGH,
    .rate2mclk = 256,
};


static struct codec_endpoint es8388_record = {
    .set_datatype = es8388_record_set_datatype,
    .set_endpoint = es8388_record_set_endpoint,
    .set_again = es8388_record_set_again,
    .set_dgain = es8388_record_set_dgain,
};

static struct codec_endpoint es8388_playback = {
    .set_datatype = es8388_playback_set_datatype,
    .set_endpoint = es8388_playback_set_endpoint,
    .set_again = es8388_playback_set_again,
    .set_dgain = es8388_playback_set_dgain,
};

static int es8388_detect(struct codec_attributes *attrs)
{
    struct i2c_client *client = get_codec_devdata(attrs);
    unsigned int ident = 0;
    int ret = AUDIO_SUCCESS;

    if(reset_gpio != -1){
        ret = gpio_request(reset_gpio,"es8388_reset");
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

    ident = es8388_reg_read(EXCODEC_ID_REG);
	/*
	 * TODO:
	 * According to the manual, this register should read the default
	 * value of 0x6, but in practice most read it as 0x36
	 *
	 * */
    if (ident < 0 || ident != EXCODEC_ID_VAL || ident != 0x36){
        printk("chip found @ 0x%x (%s) is not an %s chip.\n",
                client->addr, client->adapter->name,"es8388");
        return ret;
    }
    return 0;
}

static int es8388_set_sign(struct codec_sign_configure *sign)
{
    struct codec_sign_configure *this = &es8388_codec_sign;
    if(!sign)
        return -AUDIO_EPARAM;
    if(sign->data != this->data || sign->seq != this->seq || sign->sync != this->sync || sign->rate2mclk != this->rate2mclk)
        return -AUDIO_EPARAM;
    return AUDIO_SUCCESS;
}

static int es8388_set_power(int enable)
{
    if(enable){
        es8388_init();
    }else{
        es8388_disable_record();
        es8388_disable_playback();
    }
    return AUDIO_SUCCESS;
}

static struct codec_attributes es8388_attrs = {
    .name = "es8388",
    .mode = CODEC_IS_SLAVE_MODE,
    .type = CODEC_IS_EXTERN_MODULE,
    .pins = CODEC_IS_4_LINES,
    .transfer_protocol = DATA_I2S_MODE,
    .set_sign = es8388_set_sign,
    .set_power = es8388_set_power,
    .detect = es8388_detect,
    .record = &es8388_record,
    .playback = &es8388_playback,
};

static int es8388_gpio_init(struct excodec_driver_data *es8388_data)
{
    es8388_data->reset_gpio = reset_gpio;
    sscanf(gpio_func,"%d,%d,0x%x", &es8388_data->port, &es8388_data->func, &es8388_data->gpios);
    if(es8388_data->gpios){
        return audio_set_gpio_function(es8388_data->port, es8388_data->func, es8388_data->gpios);
    }
    return AUDIO_SUCCESS;
}

/*
 *	TODO: The current code only supports two-channel recording.
 *	Single channel recording and playback and dual channel playback are not implemented.
 *	And volume adjustment is not implemented.
 *
 * */
static int es8388_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
    int ret = 0;
    es8388_data = (struct excodec_driver_data *)kzalloc(sizeof(struct excodec_driver_data), GFP_KERNEL);
    if(!es8388_data) {
        printk("failed to alloc es8388 driver data\n");
        return -ENOMEM;
	}
	memset(es8388_data, 0, sizeof(*es8388_data));
	es8388_data->attrs = &es8388_attrs;
	if(es8388_gpio_init(es8388_data) < 0){
		printk("failed to init es8388 gpio function\n");
		ret = -EPERM;
		goto failed_init_gpio;
	}
	set_codec_devdata(&es8388_attrs,i2c);
	set_codec_hostdata(&es8388_attrs, &es8388_data);
	i2c_set_clientdata(i2c, &es8388_attrs);
	printk("probe ok -------->es8388.\n");
    return 0;

failed_init_gpio:
    kfree(es8388_data);
    es8388_data = NULL;
    return ret;
}

static int es8388_i2c_remove(struct i2c_client *client)
{
    struct excodec_driver_data *excodec = es8388_data;
    if(excodec){
        kfree(excodec);
        es8388_data = NULL;
    }
    return 0;
}

static const struct i2c_device_id es8388_i2c_id[] = {
    { "es8388", 0 },
    {}
};

MODULE_DEVICE_TABLE(i2c, es8388_i2c_id);

static struct i2c_driver es8388_i2c_driver = {
    .driver = {
        .name = "es8388",
        .owner = THIS_MODULE,
    },
    .probe = es8388_i2c_probe,
    .remove = es8388_i2c_remove,
    .id_table = es8388_i2c_id,
};

static int __init init_es8388(void)
{
    int ret = 0;

    ret = i2c_add_driver(&es8388_i2c_driver);
    if ( ret != 0 ) {
        printk(KERN_ERR "Failed to register external codec I2C driver: %d\n", ret);
    }
    return ret;
}

static void __exit exit_es8388(void)
{
    i2c_del_driver(&es8388_i2c_driver);
}

module_init(init_es8388);
module_exit(exit_es8388);

MODULE_DESCRIPTION("ASoC external codec driver");
MODULE_LICENSE("GPL v2");
