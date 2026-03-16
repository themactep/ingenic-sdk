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

#define EXCODEC_ID_REG 0x00
#define EXCODEC_ID_VAL 0x03

static int reset_gpio = -1;
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

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

struct excodec_driver_data *es8374_data;

static unsigned int es8374_i2c_read(u8 *reg,int reglen,u8 *data,int datalen)
{
	struct i2c_msg xfer[2];
	int ret;
	struct i2c_client *client = get_codec_devdata(es8374_data->attrs);

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

static int es8374_i2c_write(unsigned int reg,unsigned int value)
{
	int ret = -1;
	unsigned char tx[2];
	size_t	len = 2;
	struct i2c_client *client = get_codec_devdata(es8374_data->attrs);
#ifdef EXCODEC_I2C_CHECK
	unsigned char value_read;
#endif

	tx[0] = reg;
	tx[1] = value;
	ret = i2c_master_send(client,tx, len );

#ifdef EXCODEC_I2C_CHECK
	value_read = es8374_reg_read(reg);
	if(value != value_read)
		printk("\t[EXCODEC_I2C_CHECK] %s address = %02X Data = %02X data_check = %02X\n",__FUNCTION__, (int)reg, value,(int)value_read);
#endif
	if( ret != len ){
		return -EIO;
	}
	return 0;
}

static int es8374_reg_read(unsigned int reg)
{
	unsigned char tx[1], rx[1];
	int	wlen, rlen;
	int ret = 0;
	unsigned int rdata;

	wlen = 1;
	rlen = 1;
	tx[0] = (unsigned char)(0x7F & reg);

	ret = es8374_i2c_read(tx, wlen, rx, rlen);

	if (ret < 0) {
		printk("\t[EXCODEC] %s error ret = %d\n",__FUNCTION__, ret);
		rdata = -EIO;
	}
	else {
		rdata = (unsigned int)rx[0];
	}
	return rdata;
}

static int es8374_reg_write(unsigned int reg,unsigned int value)
{
	return es8374_i2c_write(reg,value);
}

static int es8374_reg_set(unsigned char reg, int start, int end, int val)
{
	int ret;
	int i = 0, mask = 0;
	unsigned char oldv = 0, new = 0;
	for(i = 0; i < (end-start + 1); i++) {
		mask += (1 << (start + i));
	}
	oldv = es8374_reg_read(reg);
	new = oldv & (~mask);
	new |= val << start;
	ret = es8374_reg_write(reg, new);
	if(ret < 0) {
		printk("fun:%s,EXCODEC I2C Write error.\n",__func__);
	}
	return ret;
}

static void es8374_dac_mute(int mute)
{
	if(mute) { //mute dac
		es8374_reg_write(0x36, 0x20);
	}
	else {    //unmute
		es8374_reg_write(0x36, 0x00);
	}
}

static struct codec_attributes es8374_attrs;
static int es8374_init(void)
{
	es8374_reg_write(0x00, 0x3F); //IC Rst start //ERROR:0000
	es8374_reg_write(0x00, 0x03); //IC Rst stop
	/*
	 *	user can decide the valule of MCLKDIV2 according to the frequency of MCLK clock.
	 */
	es8374_reg_write(0x01, 0x7F); //IC clk on, MCLK DIV2 =0 IF MCLK is lower than 16MHZ

	es8374_reg_write(0x6F, 0xA0); //pll set:mode enable
	es8374_reg_write(0x72, 0x41); //pll set:mode set :ERROR:0000
	es8374_reg_write(0x09, 0x01); //pll set:reset on ,set start
	/* PLL FOR 12MHZ/48KHZ */
	es8374_reg_write(0x0C, 0x08); //pll set:k
	es8374_reg_write(0x0D, 0x13); //pll set:k
	es8374_reg_write(0x0E, 0xE0); //pll set:k
	es8374_reg_write(0x0A, 0x8A); //pll set:
	es8374_reg_write(0x0B, 0x08);//pll set:n

	/* PLL FOR 12MHZ/44.1KHZ */
	es8374_reg_write(0x09, 0x41); //pll set:reset off ,set stop

	es8374_reg_write(0x05, 0x11); //clk div =1
	es8374_reg_write(0x03, 0x20); //osr =32
	es8374_reg_write(0x06, 0x01); //LRCK div =0100H = 256D
	es8374_reg_write(0x07, 0x00);

	/*
	 *	user can select master or slave mode for ES8374
	 */
	if (es8374_attrs.mode == CODEC_IS_MASTER_MODE)
		es8374_reg_write(0x0F, 0x84); //MASTER MODE, BCLK = MCLK/4
	else es8374_reg_write(0x0F, 0x04); //SLAVE MODE, BCLK = MCLK/4


	es8374_reg_write(0x10, 0x0C); //I2S-16BIT, ADC
	es8374_reg_write(0x11, 0x0C); //I2S-16BIT, DAC
	/*
	 *	user can decide to use PLL clock or not.
	 */
	es8374_reg_write(0x02, 0x00);  //don't select PLL clock, use clock from MCLK pin.

	es8374_reg_write(0x24, 0x08); //adc set
	es8374_reg_write(0x36, 0x40); //dac set
	es8374_reg_write(0x12, 0x30); //timming set
	es8374_reg_write(0x13, 0x20); //timming set
	/*
	 *	by default, select LIN1&RIN1 for ADC recording
	 */
	es8374_reg_write(0x21, 0x50); //adc set: SEL LIN1 CH+PGAGAIN=0DB
	es8374_reg_write(0x22, 0x55); //adc set: PGA GAIN=15DB//ERROR:0000
	es8374_reg_write(0x21, 0x14); //adc set: SEL LIN1 CH+PGAGAIN=18DB
	/*
	 *	chip start
	 */
	es8374_reg_write(0x00, 0x80); // IC START
	msleep(50); //DELAY_MS
	es8374_reg_write(0x14, 0x8A); // IC START
	es8374_reg_write(0x15, 0x40); // IC START
	es8374_reg_write(0x1C, 0x90); // spk set
	es8374_reg_write(0x1D, 0x02); // spk set
	es8374_reg_write(0x1F, 0x00); // spk set
	es8374_reg_write(0x1E, 0xA0); // spk on
	es8374_reg_write(0x28, 0x00); // alc set
	es8374_reg_write(0x25, 0x00); // ADCVOLUME on
	es8374_reg_write(0x38, 0x00); // DACVOLUMEL on
	es8374_reg_write(0x37, 0x30); // dac set
	es8374_reg_write(0x6D, 0x60); //SEL:GPIO1=DMIC CLK OUT+SEL:GPIO2=PLL CLK OUT

	es8374_reg_write(0x71, 0x05);
	es8374_reg_write(0x73, 0x70);
	es8374_reg_write(0x36, 0x00); //dac set
	es8374_reg_write(0x37, 0x00); // dac set

	es8374_reg_set(0x10, 6, 7, 0x3); //I2S-16BIT, ADC, MUTE ADC SDP
	es8374_reg_set(0x11, 6, 6, 0x1); //I2S-16BIT, DAC, MUTE DAC SDP

	return 0;
}

static int es8374_enable_record(void)
{
	es8374_reg_set(0x10, 6, 7, 0x3); //I2S-16BIT, ADC, MUTE ADC SDP

	es8374_reg_write(0x21, 0x24); //adc set: SEL LIN2&RIN2 for buildin mic Recording
	es8374_reg_write(0x26, 0x4E); // alc set
	es8374_reg_write(0x27, 0x08); // alc set
	es8374_reg_write(0x28, 0x70); // alc set
	es8374_reg_write(0x29, 0x00); // alc set
	es8374_reg_write(0x2b, 0x00); // alc set
	//eq filter
	es8374_reg_write(0x45, 0x03);
	es8374_reg_write(0x46, 0xF7);
	es8374_reg_write(0x47, 0xFD);
	es8374_reg_write(0x48, 0xFF);
	es8374_reg_write(0x49, 0x1F);
	es8374_reg_write(0x4A, 0xF7);
	es8374_reg_write(0x4B, 0xFD);
	es8374_reg_write(0x4C, 0xFF);
	es8374_reg_write(0x4D, 0x03);
	es8374_reg_write(0x4E, 0xF7);
	es8374_reg_write(0x4F, 0xFD);
	es8374_reg_write(0x50, 0xFF);
	es8374_reg_write(0x51, 0x1F);
	es8374_reg_write(0x52, 0xF7);
	es8374_reg_write(0x53, 0xFD);
	es8374_reg_write(0x54, 0xFF);
	es8374_reg_write(0x55, 0x1F);
	es8374_reg_write(0x56, 0xF7);
	es8374_reg_write(0x57, 0xFD);
	es8374_reg_write(0x58, 0xFF);
	es8374_reg_write(0x59, 0x03);
	es8374_reg_write(0x5A, 0xF7);
	es8374_reg_write(0x5B, 0xFD);
	es8374_reg_write(0x5C, 0xFF);
	es8374_reg_write(0x5D, 0x1F);
	es8374_reg_write(0x5E, 0xF7);
	es8374_reg_write(0x5F, 0xFD);
	es8374_reg_write(0x60, 0xFF);
	es8374_reg_write(0x61, 0x03);
	es8374_reg_write(0x62, 0xF7);
	es8374_reg_write(0x63, 0xFD);
	es8374_reg_write(0x64, 0xFF);
	es8374_reg_write(0x65, 0x1F);
	es8374_reg_write(0x66, 0xF7);
	es8374_reg_write(0x67, 0xFD);
	es8374_reg_write(0x68, 0xFF);
	es8374_reg_write(0x69, 0x1F);
	es8374_reg_write(0x6A, 0xF7);
	es8374_reg_write(0x6B, 0xFD);
	es8374_reg_write(0x6C, 0xFF);
	es8374_reg_write(0x2D, 0x85);

	es8374_reg_set(0x10, 6, 7, 0x0); //I2S-16BIT, ADC, un-MUTE ADC SDP

	return AUDIO_SUCCESS;
}

static int es8374_enable_playback(void)
{
	es8374_reg_set(0x11, 6, 6, 0x0); //I2S-16BIT, DAC, MUTE DAC SDP
	es8374_reg_write(0x1D, 0x02);
	es8374_reg_write(0x1E, 0xA0);
	es8374_dac_mute(0);
	return AUDIO_SUCCESS;
}

static int es8374_disable_record(void)
{
	es8374_reg_set(0x10, 6, 7, 0x3); //I2S-16BIT, ADC, MUTE ADC SDP
	es8374_reg_write(0x25, 0xC0);
	es8374_reg_write(0x28, 0x1C);
	es8374_reg_write(0x21, 0xD4);
	return AUDIO_SUCCESS;
}

static int es8374_disable_playback(void)
{
	es8374_dac_mute(1);
	es8374_reg_set(0x11, 6, 6, 0x1); //I2S-16BIT, DAC, MUTE DAC SDP
	es8374_reg_write(0x1D, 0x10);
	es8374_reg_write(0x1E, 0x40);
	return AUDIO_SUCCESS;
}

static int es8374_set_sample_rate(unsigned int rate)
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
    		/*
    		*	set pll, 12MHZ->12.288MHZ
    		*/
				es8374_dac_mute(1);
				es8374_reg_write(0x09, 0x01); //pll set:reset on ,set start

				es8374_reg_write(0x0C, 0x08); //pll set:k
				es8374_reg_write(0x0D, 0x13); //pll set:k
				es8374_reg_write(0x0E, 0xE0); //pll set:k
				es8374_reg_write(0x0A, 0x8A); //pll set:
				es8374_reg_write(0x0B, 0x08);//pll set:n

				es8374_reg_write(0x09, 0x41); //pll set:reset off ,set stop

				es8374_reg_write(0x05, 0x11); //adc&dac clkdiv
				es8374_reg_write(0x02, 0x00);//
				es8374_reg_write(0x06, 0x06); //LRCK div =0600H = 1536D, lrck = 12.288M/1536 = 8K
				es8374_reg_write(0x06, 0x01); //add by sxzhang
				es8374_reg_write(0x07, 0x00); //add by sxzhang
				es8374_reg_write(0x0f, 0x9d);  //bclk = mclk/ 48 = 256K
				es8374_reg_set(0x0f, 0, 4, 0x04);
				es8374_dac_mute(0);
				break;
    	case 6: //12000
    		/*
    		*	set pll, 12MHZ->12.288MHZ
    		*/
				es8374_dac_mute(1);
				es8374_reg_write(0x09, 0x01); //pll set:reset on ,set start

				es8374_reg_write(0x0C, 0x08); //pll set:k
				es8374_reg_write(0x0D, 0x13); //pll set:k
				es8374_reg_write(0x0E, 0xE0); //pll set:k
				es8374_reg_write(0x0A, 0x8A); //pll set:
				es8374_reg_write(0x0B, 0x08); //pll set:n

				es8374_reg_write(0x09, 0x41); //pll set:reset on ,set start

				es8374_reg_write(0x05, 0x11);//adc&dac clkdiv
				es8374_reg_write(0x02, 0x00);//pll enable
				es8374_reg_write(0x06, 0x01); //LRCK div =0400H = 1024D, lrck = 12.288M/1024 = 12K
				es8374_reg_write(0x07, 0x00);
				es8374_reg_write(0x0f, 0x84); //bclk = mclk/ 16 = 768K
				es8374_dac_mute(0);
				break;
    	case 5: //16000
    		/*
    		*	set pll, 26MHZ->12.288MHZ
    		*/

				es8374_dac_mute(1);
				es8374_reg_write(0x09, 0x01); //pll set:reset on ,set start

				es8374_reg_write(0x0C, 0x08); //pll set:k
				es8374_reg_write(0x0D, 0x13); //pll set:k
				es8374_reg_write(0x0E, 0xE0); //pll set:k
				es8374_reg_write(0x0A, 0x8A); //pll set:
				es8374_reg_write(0x0B, 0x08); //pll set:n

				es8374_reg_write(0x09, 0x41); //pll set:reset on ,set start

				es8374_reg_write(0x05, 0x11);//adc&dac clkdiv
				es8374_reg_write(0x02, 0x00);
				es8374_reg_write(0x06, 0x01);
				es8374_reg_write(0x07, 0x00);

				es8374_reg_write(0x0f, 0x8c);
				es8374_reg_set(0x0f, 0, 4, 0x04);
				es8374_dac_mute(0);
				break;
    	case 4: //24000
    		/*
    		*	set pll, 12MHZ->12.288MHZ
    		*/
				es8374_dac_mute(1);
				es8374_reg_write(0x09, 0x01); //pll set:reset on ,set start

				es8374_reg_write(0x0C, 0x08); //pll set:k
				es8374_reg_write(0x0D, 0x13); //pll set:k
				es8374_reg_write(0x0E, 0xE0); //pll set:k
				es8374_reg_write(0x0A, 0x8A); //pll set:
				es8374_reg_write(0x0B, 0x08); //pll set:n

				es8374_reg_write(0x09, 0x41); //pll set:reset on ,set start

				es8374_reg_write(0x05, 0x11);//adc&dac clkdiv
				es8374_reg_write(0x02, 0x00);
				es8374_reg_write(0x06, 0x01);
				es8374_reg_write(0x07, 0x00);
				es8374_reg_write(0x0f, 0x84);
				es8374_dac_mute(0);
				break;
    	case 3: //32000
    		/*
    		*	set pll, 12MHZ->8.192MHZ
    		*/
				es8374_dac_mute(1);
				es8374_reg_write(0x09, 0x01); //pll set:reset on ,set start

				es8374_reg_write(0x0C, 0x08); //pll set:k
				es8374_reg_write(0x0D, 0x13); //pll set:k
				es8374_reg_write(0x0E, 0xe0); //pll set:k
				es8374_reg_write(0x0A, 0x8A); //pll set:
				es8374_reg_write(0x0B, 0x08); //pll set:n

				es8374_reg_write(0x09, 0x41); //pll set:reset on ,set start

				es8374_reg_write(0x05, 0x11);//adc&dac clkdiv
				es8374_reg_write(0x02, 0x00);//pll enable
				es8374_reg_write(0x06, 0x01); //LRCK div =0100H = 256D, lrck = 12.288M/256 = 32K
				es8374_reg_write(0x07, 0x00);
				es8374_reg_write(0x0f, 0x84);  //bclk = mclk/4 = 2048k
				es8374_dac_mute(0);
				break;
		case 2: //44100;
    		/*
    		*	set pll, 12MHZ->11.2896MHZ
    		*/
				es8374_dac_mute(1);
				es8374_reg_write(0x09, 0x01); //pll set:reset on ,set start

				es8374_reg_write(0x0C, 0x08); //pll set:k
				es8374_reg_write(0x0D, 0x13); //pll set:k
				es8374_reg_write(0x0E, 0xe0); //pll set:k
				es8374_reg_write(0x0A, 0x8A); //pll set:
				es8374_reg_write(0x0B, 0x08); //pll set:n

				es8374_reg_write(0x09, 0x41); //pll set:reset on ,set start

				es8374_reg_write(0x05, 0x11);//adc&dac clkdiv
				es8374_reg_write(0x02, 0x00);
				es8374_reg_write(0x06, 0x01); //LRCK div =0100H = 256D, lrck = 11.2896M/256 = 44.1K
				es8374_reg_write(0x07, 0x00);
				es8374_reg_write(0x0f, 0x84);  //bclk = mclk/4 = 2822.4k
				es8374_dac_mute(0);
				break;
     	case 1: //48000
				es8374_dac_mute(1);
				es8374_reg_write(0x09, 0x01); //pll set:reset on ,set start

				es8374_reg_write(0x0C, 0x08); //pll set:k
				es8374_reg_write(0x0D, 0x13); //pll set:k
				es8374_reg_write(0x0E, 0xE0); //pll set:k
				es8374_reg_write(0x0A, 0x8A); //pll set:
				es8374_reg_write(0x0B, 0x08); //pll set:n

				es8374_reg_write(0x09, 0x41); //pll set:reset on ,set start

				es8374_reg_write(0x05, 0x11);//adc&dac clkdiv
				es8374_reg_write(0x02, 0x00);//pll enable
				es8374_reg_write(0x06, 0x01); //LRCK div =0100H = 256D, lrck = 12.288M/256 = 48K
				es8374_reg_write(0x07, 0x00);
				es8374_reg_write(0x0f, 0x84);  //bclk = mclk/ 8 = 3072k
				es8374_dac_mute(0);
				break;
     	case 0: //96000
    		/*
    		*	set pll, 12MHZ->11.2896MHZ
    		*/
				es8374_dac_mute(1);
				es8374_reg_write(0x09, 0x01); //pll set:reset on ,set start

				es8374_reg_write(0x0C, 0x08); //pll set:k
				es8374_reg_write(0x0D, 0x13); //pll set:k
				es8374_reg_write(0x0E, 0xE0); //pll set:k
				es8374_reg_write(0x0A, 0x8A); //pll set:
				es8374_reg_write(0x0B, 0x08); //pll set:n

				es8374_reg_write(0x09, 0x41); //pll set:reset on ,set start

				es8374_reg_write(0x05, 0x11);//adc&dac clkdiv
				es8374_reg_set(0x02, 4, 5, 0x03);//double speed
				es8374_reg_set(0x02, 3, 3, 0x00);//pll enable
				es8374_reg_write(0x06, 0x01); //LRCK div =0080H = 128D, lrck = 12.288M/128 = 96K
				es8374_reg_write(0x07, 0x00);
				es8374_reg_write(0x0f, 0x84);  //bclk = mclk/ 2 = 6144k
				es8374_dac_mute(0);
				break;
    	default:
				printk("error:(%s,%d), error audio sample rate.\n",__func__,__LINE__);
				break;
	}
	return AUDIO_SUCCESS;
}

static int es8374_record_set_datatype(struct audio_data_type *type)
{
	int ret = AUDIO_SUCCESS;
	if(!type) return -AUDIO_EPARAM;
	ret |= es8374_set_sample_rate(type->sample_rate);
	return ret;
}

static int es8374_record_set_endpoint(int enable)
{
	if(enable){
		es8374_enable_record();
	}else{
		es8374_disable_record();
	}
	return AUDIO_SUCCESS;
}

static int es8374_record_set_again(struct codec_analog_gain *again)
{
	int volume = 0;
	if (again->gain.gain[0] < 0) again->gain.gain[0] = 0;
	if (again->gain.gain[0] > 0x1f) again->gain.gain[0] = 0x1f;

	volume = again->gain.gain[0];
	volume = 124 - volume * 4;
	es8374_reg_write(0x25, volume);
	return 0;
}

static int es8374_record_set_dgain(struct volume gain)
{
	return 0;
}

static int es8374_playback_set_datatype(struct audio_data_type *type)
{
	int ret = AUDIO_SUCCESS;
	if(!type) return -AUDIO_EPARAM;
	ret |= es8374_set_sample_rate(type->sample_rate);
	return ret;
}

static int es8374_playback_set_endpoint(int enable)
{
	if(enable){
		es8374_enable_playback();
	}else{
		es8374_disable_playback();
	}
	return AUDIO_SUCCESS;
}

static int es8374_playback_set_again(struct codec_analog_gain *again)
{
	int volume = 0;
	if (again->gain.gain[0] < 0) again->gain.gain[0] = 0;
	if (again->gain.gain[0] > 0x1f) again->gain.gain[0] = 0x1f;

	volume = again->gain.gain[0];
	volume = 124 - volume * 4;
	es8374_reg_write(0x38, volume);
	return 0;
}

static int es8374_playback_set_dgain(struct volume gain)
{
	return 0;
}

static struct codec_sign_configure es8374_codec_sign = {
	.data = DATA_I2S_MODE,
	.seq = LEFT_FIRST_MODE,
	.sync = LEFT_LOW_RIGHT_HIGH,
	.rate2mclk = 256,
};

static struct codec_endpoint es8374_record = {
	.set_datatype = es8374_record_set_datatype,
	.set_endpoint = es8374_record_set_endpoint,
	.set_again = es8374_record_set_again,
	.set_dgain = es8374_record_set_dgain,
};

static struct codec_endpoint es8374_playback = {
	.set_datatype = es8374_playback_set_datatype,
	.set_endpoint = es8374_playback_set_endpoint,
	.set_again = es8374_playback_set_again,
	.set_dgain = es8374_playback_set_dgain,
};

static int es8374_detect(struct codec_attributes *attrs)
{
	struct i2c_client *client = get_codec_devdata(attrs);
	unsigned int ident = 0;
	int ret = AUDIO_SUCCESS;

	if(reset_gpio != -1){
		ret = gpio_request(reset_gpio,"es8374_reset");
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
	ident = es8374_reg_read(EXCODEC_ID_REG);
	if (ident < 0 || ident != EXCODEC_ID_VAL){
		printk("chip found @ 0x%x (%s) is not an %s chip.\n",
				client->addr, client->adapter->name,"es8374");
		return ret;
	}
	return 0;
}

static int es8374_set_sign(struct codec_sign_configure *sign)
{
	struct codec_sign_configure *this = &es8374_codec_sign;
	if(!sign)
		return -AUDIO_EPARAM;
	if(sign->data != this->data || sign->seq != this->seq || sign->sync != this->sync || sign->rate2mclk != this->rate2mclk)
		return -AUDIO_EPARAM;
	return AUDIO_SUCCESS;
}

static int es8374_set_power(int enable)
{
	if(enable){
		es8374_init();
	}else{
		es8374_disable_record();
		es8374_disable_playback();
	}
	return AUDIO_SUCCESS;
}

static struct codec_attributes es8374_attrs = {
	.name = "es8374",
	.mode = CODEC_IS_MASTER_MODE,
	.type = CODEC_IS_EXTERN_MODULE,
	.pins = CODEC_IS_4_LINES,
	.transfer_protocol = DATA_I2S_MODE,
	.set_sign = es8374_set_sign,
	.set_power = es8374_set_power,
	.detect = es8374_detect,
	.record = &es8374_record,
	.playback = &es8374_playback,
};

static int es8374_gpio_init(struct excodec_driver_data *es8374_data)
{
	es8374_data->reset_gpio = reset_gpio;
	sscanf(gpio_func,"%d,%d,0x%x", &es8374_data->port, &es8374_data->func, &es8374_data->gpios);
	if(es8374_data->gpios){
		return audio_set_gpio_function(es8374_data->port, es8374_data->func, es8374_data->gpios);
	}
	return AUDIO_SUCCESS;
}

static int es8374_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
	int ret = 0;
	es8374_data = (struct excodec_driver_data *)kzalloc(sizeof(struct excodec_driver_data), GFP_KERNEL);
	if(!es8374_data) {
		printk("failed to alloc es8374 driver data\n");
		return -ENOMEM;
	}
	memset(es8374_data, 0, sizeof(*es8374_data));
	es8374_data->attrs = &es8374_attrs;
	if(es8374_gpio_init(es8374_data) < 0){
		printk("failed to init es8374 gpio function\n");
		ret = -EPERM;
		goto failed_init_gpio;
	}
	set_codec_devdata(&es8374_attrs,i2c);
	set_codec_hostdata(&es8374_attrs, &es8374_data);
	i2c_set_clientdata(i2c, &es8374_attrs);
	printk("probe ok -------->es8374.\n");
	return 0;

failed_init_gpio:
	kfree(es8374_data);
	es8374_data = NULL;
	return ret;
}

static int es8374_i2c_remove(struct i2c_client *client)
{
	struct excodec_driver_data *excodec = es8374_data;
	if(excodec){
		kfree(excodec);
		es8374_data = NULL;
	}
	return 0;
}

static const struct i2c_device_id es8374_i2c_id[] = {
	{ "es8374", 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, es8374_i2c_id);

static struct i2c_driver es8374_i2c_driver = {
	.driver = {
		.name = "es8374",
		.owner = THIS_MODULE,
	},
	.probe = es8374_i2c_probe,
	.remove = es8374_i2c_remove,
	.id_table = es8374_i2c_id,
};

static int __init init_es8374(void)
{
	int ret = 0;

	ret = i2c_add_driver(&es8374_i2c_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "Failed to register external codec I2C driver: %d\n", ret);
	}
	return ret;
}

static void __exit exit_es8374(void)
{
	i2c_del_driver(&es8374_i2c_driver);
}


module_init(init_es8374);
module_exit(exit_es8374);

MODULE_DESCRIPTION("ASoC external codec driver");
MODULE_LICENSE("GPL v2");
