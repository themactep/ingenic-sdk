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
#include <linux/firmware.h>
#include <linux/input-polldev.h>
#include <linux/input.h>
#include <linux/gfp.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include "../include/codec-common.h"
#include "../include/audio_common.h"
#include "ak7755_codec.h"

#define EXCODEC_ID_REG 0x00
#define EXCODEC_ID_VAL 0x03

/*example:
 * PA(n) = 0*32 + n
 * PB(n) = 1*32 + n
 * PC(n) = 2*32 + n
 * PD(n) = 3*32 + n
 */
static int reset_gpio = GPIO_PC(23);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int spk_level = 1;
module_param(spk_level, int, S_IRUGO);
MODULE_PARM_DESC(spk_level, "Speaker active level: -1 disable, 0 low active, 1 high active");

static int spk_gpio =  GPIO_PB(31);
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
	int speaker_en;
	int port;
	int func;
	unsigned int gpios;
	int status;
};

struct excodec_driver_data *ak7755_data;

static unsigned int ak7755_i2c_read(u8 *reg, int reglen, u8* data, int datalen)
{
	int ret = 0;
	struct i2c_msg xfer[2];
	struct i2c_client *client = get_codec_devdata(ak7755_data->attrs);

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
	else return -EIO;
}

static unsigned int ak7755_reg_read(unsigned int reg)
{
	unsigned char tx[1], rx[1];
	int wlen, rlen;
	int ret;
	unsigned int rdata;

	wlen = 1;
	rlen = 1;
	tx[0] = (unsigned char)(0x7f & reg);

	ret = ak7755_i2c_read(tx, wlen, rx, rlen);
	if (ret < 0) {
		printk("%s, %d   ak7755 i2c read error!\n", __func__, __LINE__);
		rdata = -EIO;
	} else
		rdata = (unsigned int)rx[0];

	return rdata;
}

//#define AK7755_DEBUG

static int ak7755_reads(u8* tx, size_t wlen, u8 *rx, size_t rlen)
{
#ifdef AK7755_DEBUG
	printk("%s,%d  tx[0]=%x, %d, %d\n", __func__, __LINE__, tx[0], (int)wlen, (int)rlen);
#endif
	return ak7755_i2c_read(tx, wlen, rx, rlen);
}

static int ak7755_reg_write(unsigned int reg, unsigned long value)
{
	int ret = -1;
	unsigned char tx[2];
	size_t len = 2;
	struct i2c_client *client = get_codec_devdata(ak7755_data->attrs);

	tx[0] = reg;
	tx[1] = value;
	ret = i2c_master_send(client, tx, len);
	if (ret != len) {
		printk("%s,%d   ak7755 i2c write error!\n", __func__, __LINE__);
		return -EIO;
	}

	return 0;
}

static int ak7755_writes(const u8* tx, size_t wlen)
{
	struct i2c_client *client = get_codec_devdata(ak7755_data->attrs);

#ifdef AK7755_DEBUG
	printk("%s, %d   tx[0]=%x, tx[1]=%x, len=%d\n", __func__, __LINE__, (int)tx[0], (int)tx[1], (int)wlen);
#endif
	return i2c_master_send(client, tx, wlen);
}

static int ak7755_reg_set(unsigned char reg, int start, int end, int val)
{
	int ret;
	int i = 0, mask = 0;
	unsigned char oldv = 0, new = 0;
	for(i = 0; i < (end-start + 1); i++) {
		mask += (1 << (start + i));
	}
	oldv = ak7755_reg_read(reg);
	new = oldv & (~mask);
	new |= val << start;
	ret = ak7755_reg_write(reg, new);
	if(ret < 0) {
		printk("fun:%s,EXCODEC I2C Write error.\n",__func__);
	}
	return ret;
}

static int ak7755_reg_update_bits(unsigned char reg, unsigned int mask, unsigned int value)
{
	bool change;
	unsigned int old, new;
	int ret;

	old = ak7755_reg_read(reg);
	new = (old & ~mask) | (value & mask);
	change = (old != new);

	if (change)
		ret = ak7755_reg_write(reg, new);

	return change;
}

#ifdef AK7755_DEBUG
static int dump_ak7755_codec_regs(void)
{
	unsigned int i = 0;
	unsigned char value = 0;
	struct i2c_client *client = get_codec_devdata(ak7755_data->attrs);

	printk("%s,%d    ak7755 codec register list as: \n", __func__, __LINE__);
	if (!client)
		return 0;

	for (i = 0xc0; i <= AK7755_MAX_REGISTERS; i++) {
		value = ak7755_reg_read(i);
		printk("ak7755 reg_addr=0x%02x, value=0x%02x\n", i, value);
	}

	return 0;
}
#endif

static struct codec_attributes ak7755_attrs;

#define MUTE_ENABLE 1
#define MUTE_DISABLE 0
static void ak7755_adc_mute(int mute)
{
	if (mute){
		ak7755_reg_update_bits(AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0x80, 0x80);//Lch
		ak7755_reg_update_bits(AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0x40, 0x40);//Rch
	}else{
		ak7755_reg_update_bits(AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0x80, 0x00);
		ak7755_reg_update_bits(AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0x40, 0x00);
	}
}

static void ak7755_dac_mute(int mute)
{
	if (mute)
		ak7755_reg_update_bits(AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0x20, 0x20);
	else
		ak7755_reg_update_bits(AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0x20, 0x00);
}

static int ak7755_set_status(enum ak7755_status status)
{
	switch(status) {
		case RUN:
			ak7755_reg_update_bits(AK7755_C1_CLOCK_SETTING2, 0x01, 0x01);
			mdelay(15);
			ak7755_reg_update_bits(AK7755_CF_RESET_POWER_SETTING, 0x04, 0x04);
			ak7755_reg_update_bits(AK7755_CF_RESET_POWER_SETTING, 0x08, 0x08);
			break;
		case DOWNLOAD:
			ak7755_reg_update_bits(AK7755_CF_RESET_POWER_SETTING, 0x05, 0x01);
			mdelay(1);
			break;
		case STANDBY:
			ak7755_reg_update_bits(AK7755_C1_CLOCK_SETTING2, 0x01, 0x01);
			mdelay(15);
			ak7755_reg_update_bits(AK7755_CF_RESET_POWER_SETTING, 0x04, 0x00);
			ak7755_reg_update_bits(AK7755_CF_RESET_POWER_SETTING, 0x08, 0x00);
			break;
		case SUSPEND:
		case POWERDOWN:
			ak7755_reg_update_bits(AK7755_CF_RESET_POWER_SETTING, 0x04, 0x00);
			ak7755_reg_update_bits(AK7755_CF_RESET_POWER_SETTING, 0x08, 0x00);
			ak7755_reg_set(AK7755_CE_POWER_MANAGEMENT, 0, 7, 0x00);
			ak7755_reg_update_bits(AK7755_C1_CLOCK_SETTING2, 0x01, 0x00);
			break;
		default:
			return -EINVAL;
	}
	ak7755_data->status = status;
	mdelay(5);
	return 0;
}

static int ak7755_enable_record(void)
{
	/* AINE:analog input setting */
	ak7755_reg_update_bits(AK7755_C0_CLOCK_SETTING1, 0x08, 0x08);
	/* ADC Rch select */
	ak7755_reg_update_bits(AK7755_C9_ANALOG_IO_SETTING, 0xC0, 0x80);
	/* ADC Lch select */
	ak7755_reg_update_bits(AK7755_C9_ANALOG_IO_SETTING, 0x30, 0x20);
	/* SDOUT1 output enable*/
	ak7755_reg_update_bits(AK7755_CA_CLK_SDOUT_SETTING, 0x01,0x01);
	/* SDOUT1 select*/
	ak7755_reg_update_bits(AK7755_CC_VOLUME_TRANSITION, 0x07,0x03);

	/* MIC AMP Rch ADC Rch power up*/
	ak7755_reg_update_bits(AK7755_CE_POWER_MANAGEMENT, 0x80,0x80);
	/* MIC AMP Lch ADC Lch power up*/
	ak7755_reg_update_bits(AK7755_CE_POWER_MANAGEMENT, 0x40,0x40);

	ak7755_set_status(RUN);
	return AUDIO_SUCCESS;
}

static int ak7755_enable_playback(void)
{
	int val = -1;
	/* DAC input format select */
	ak7755_reg_update_bits(AK7755_C6_DAC_DEM_SETTING, 0x30, 0x00);
	/* DAC input select */
	ak7755_reg_update_bits(AK7755_C8_DAC_IN_SETTING, 0xC0, 0x80);

	/* Line out1 power up */
	ak7755_reg_update_bits(AK7755_CE_POWER_MANAGEMENT, 0x04,0x04);
	/* DAC Lch power up */
	ak7755_reg_update_bits(AK7755_CE_POWER_MANAGEMENT, 0x01,0x01);

	ak7755_set_status(RUN);
	/* enable AMP*/
	if (spk_gpio > 0) {
		val = gpio_get_value(spk_gpio);
		gpio_direction_output(spk_gpio, spk_level);
		val = gpio_get_value(spk_gpio);
	}
	return AUDIO_SUCCESS;
}

static int ak7755_disable_record(void)
{
	/* MIC AMP Rch ADC Rch power up*/
	ak7755_reg_update_bits(AK7755_CE_POWER_MANAGEMENT, 0x80,0x00);
	/* MIC AMP Lch ADC Lch power up*/
	ak7755_reg_update_bits(AK7755_CE_POWER_MANAGEMENT, 0x40,0x00);
	return AUDIO_SUCCESS;
}

static int ak7755_disable_playback(void)
{
	/* disable AMP*/
	if (spk_gpio > 0)
		gpio_direction_output(spk_gpio,!spk_level);
	/* Line out1 power up */
	ak7755_reg_update_bits(AK7755_CE_POWER_MANAGEMENT, 0x04,0x00);
	/* DAC Lch power up */
	ak7755_reg_update_bits(AK7755_CE_POWER_MANAGEMENT, 0x01,0x00);
	return AUDIO_SUCCESS;
}

static int ak7755_set_sample_rate(unsigned int rate)
{
	int i = 0;
	int fs = 0;
	unsigned int mrate[8] = {8000, 12000, 16000, 24000, 32000, 44100, 48000, 96000};
	unsigned int rate_fs[8] = {7, 6, 5, 4, 3, 2, 1, 0};
	for (i = 0; i < 8; i++) {
		if (rate <= mrate[i])
			break;
	}
	if (i == 8)
		i = 0;

	fs = ak7755_reg_read(AK7755_C0_CLOCK_SETTING1);
	fs &= ~AK7755_FS;
	switch(rate_fs[i]) {
    	case  7: //8000
				fs |= AK7755_FS_8KHZ;
				break;
    	case 6: //12000
				fs |= AK7755_FS_12KHZ;
				break;
    	case 5: //16000
				fs |= AK7755_FS_16KHZ;
				break;
    	case 4: //24000
				fs |= AK7755_FS_24KHZ;
				break;
    	case 3: //32000
				fs |= AK7755_FS_32KHZ;
				break;
		case 2: //44100;
				break;
     	case 1: //48000
				fs |= AK7755_FS_48KHZ;
				break;
     	case 0: //96000
				fs |= AK7755_FS_96KHZ;
				break;
    	default:
				printk("error:(%s,%d), error audio sample rate.\n",__func__,__LINE__);
				break;
	}
	ak7755_adc_mute(MUTE_ENABLE);
	ak7755_dac_mute(MUTE_ENABLE);
	ak7755_reg_set(AK7755_C0_CLOCK_SETTING1, 0, 2, fs);
	ak7755_adc_mute(MUTE_DISABLE);
	ak7755_dac_mute(MUTE_DISABLE);
	return AUDIO_SUCCESS;
}

static int ak7755_record_set_datatype(struct audio_data_type *type)
{
	int ret = AUDIO_SUCCESS;
	if(!type) return -AUDIO_EPARAM;
	ret |= ak7755_set_sample_rate(type->sample_rate);
	return ret;
}

static int ak7755_record_set_endpoint(int enable)
{
	if(enable){
		ak7755_enable_record();
	}else{
		ak7755_disable_record();
	}
	return AUDIO_SUCCESS;
}

static int ak7755_record_set_again(struct codec_analog_gain *again)
{
	/*******************************************
	  | value :0x00   0x01    0x02  ... 0xf    |
	  | gain:  0      2dB     4dB   ... 36dB   |
	 * ****************************************/
	if (again->gain.gain[0] < 0) again->gain.gain[0] = 0;
	if (again->gain.gain[0] > 0xf) again->gain.gain[0] = 0xf;
	if (again->gain.gain[1] < 0) again->gain.gain[1] = 0;
	if (again->gain.gain[1] > 0xf) again->gain.gain[1] = 0xf;
	if (again->gain.channel == 1) {
		/* left channel */
		ak7755_reg_set(AK7755_D2_MIC_GAIN_SETTING, 0, 3, again->gain.gain[0]);
	} else if (again->gain.channel == 2) {
		/* right channel */
		ak7755_reg_set(AK7755_D2_MIC_GAIN_SETTING, 4, 7, again->gain.gain[1]);
	} else {
		/* both */
		ak7755_reg_set(AK7755_D2_MIC_GAIN_SETTING, 0, 3, again->gain.gain[0]);
		ak7755_reg_set(AK7755_D2_MIC_GAIN_SETTING, 4, 7, again->gain.gain[1]);
	}
	return AUDIO_SUCCESS;
}

static int ak7755_record_set_dgain(struct volume gain)
{
	/*******************************************************
	 | value :0xff   0xfe    0xfd    0x30  0x1   0x0       |
	 | gain:  mute   -103.0  -102.5  0     +23.5 +24.0 (DB)|
	 * ****************************************************/
	if (gain.gain[0] > 0xff)
		gain.gain[0] = 0xff;
	if (gain.gain[1] > 0xff)
		gain.gain[1] = 0xff;
	if (gain.channel == 1) {
		/* left channel */
		ak7755_reg_set(AK7755_D5_ADC_DVOLUME_SETTING1, 0, 7, gain.gain[0]);
	} else if (gain.channel == 2) {
		/* right channel */
		ak7755_reg_set(AK7755_D6_ADC_DVOLUME_SETTING2, 0, 7, gain.gain[1]);
	} else {
		/* both */
		ak7755_reg_set(AK7755_D5_ADC_DVOLUME_SETTING1, 0, 7, gain.gain[0]);
		ak7755_reg_set(AK7755_D6_ADC_DVOLUME_SETTING2, 0, 7, gain.gain[1]);
	}
	return AUDIO_SUCCESS;
}

static int ak7755_playback_set_datatype(struct audio_data_type *type)
{
	int ret = AUDIO_SUCCESS;
	if(!type) return -AUDIO_EPARAM;
	ret |= ak7755_set_sample_rate(type->sample_rate);
	return ret;
}

static int ak7755_playback_set_endpoint(int enable)
{
	if(enable){
		ak7755_enable_playback();
	}else{
		ak7755_disable_playback();
	}
	return AUDIO_SUCCESS;
}

static int ak7755_playback_set_again(struct codec_analog_gain *again)
{
	/*************************************************************
	  | value :0x00   0x01    0x02    0x03   0x04 ...   0xf       |
	  | gain:  mute   -28     -26     -24    -22  ...   0     (DB)|
	 * ***********************************************************/
	if (again->gain.gain[0] < 0) again->gain.gain[0] = 0;
	if (again->gain.gain[0] > 0xf) again->gain.gain[0] = 0xf;
	ak7755_reg_update_bits(AK7755_D4_LO1_LO2_VOLUME_SETTING, 0x0F,again->gain.gain[0]);
	return AUDIO_SUCCESS;
}

static int ak7755_playback_set_dgain(struct volume gain)
{
	 /******************************************************
	 | value :0xff   0xfe    0xfd    0x18  0x1     0x0       |
	 | gain:  mute   -115.0  -114.5  0     +11.5   +12.0 (DB)|
	 * ****************************************************/
	if (gain.gain[0] > 0xff)
		gain.gain[0] = 0xff;
	if (gain.gain[1] > 0xff)
		gain.gain[1] = 0xff;
	ak7755_reg_update_bits(AK7755_D8_DAC_DVOLUME_SETTING1, 0xFF,gain.gain[0]);
	return AUDIO_SUCCESS;
}

static struct codec_sign_configure ak7755_codec_sign = {
	.data = DATA_I2S_MODE,
	.seq = LEFT_FIRST_MODE,
	.sync = LEFT_LOW_RIGHT_HIGH,
	.rate2mclk = 256,
};

static struct codec_endpoint ak7755_record = {
	.set_datatype = ak7755_record_set_datatype,
	.set_endpoint = ak7755_record_set_endpoint,
	.set_again = ak7755_record_set_again,
	.set_dgain = ak7755_record_set_dgain,
};

static struct codec_endpoint ak7755_playback = {
	.set_datatype = ak7755_playback_set_datatype,
	.set_endpoint = ak7755_playback_set_endpoint,
	.set_again = ak7755_playback_set_again,
	.set_dgain = ak7755_playback_set_dgain,
};

static int ak7755_set_sign(struct codec_sign_configure *sign)
{
	struct codec_sign_configure *this = &ak7755_codec_sign;
	if(!sign)
		return -AUDIO_EPARAM;
	if(sign->data != this->data || sign->seq != this->seq || sign->sync != this->sync || sign->rate2mclk != this->rate2mclk)
		return -AUDIO_EPARAM;
	return AUDIO_SUCCESS;
}

static int ak7755_set_power(int enable)
{
	if(enable){
		/* PDN */
		if (ak7755_data->reset_gpio > 0) {
			gpio_set_value(ak7755_data->reset_gpio, 0);
			msleep(10);
			gpio_set_value(ak7755_data->reset_gpio, 1);
		}
		mdelay(3);
		/* clock mode setting */
		ak7755_reg_update_bits(AK7755_C0_CLOCK_SETTING1, AK7755_M_S, 0x30);
		/* LRCK fs select */
		ak7755_reg_update_bits(AK7755_C1_CLOCK_SETTING2, 0x40, 0x00);
		/* BICK fs select */
		ak7755_reg_update_bits(AK7755_C1_CLOCK_SETTING2, 0x30, 0x00);
		/* LRCK format select */
		ak7755_reg_update_bits(AK7755_C2_SERIAL_DATA_FORMAT, 0x30, 0x00);
		/* LRCK output enable*/
		ak7755_reg_update_bits(AK7755_CA_CLK_SDOUT_SETTING, 0x40, 0x40);
		/* BICK output enable*/
		ak7755_reg_update_bits(AK7755_CA_CLK_SDOUT_SETTING, 0x20, 0x20);
		/* System reset must be 1*/
		ak7755_reg_update_bits(AK7755_CD_STO_DLS_SETTING, 0x40,0x40);
		/* System reset must be 1*/
		ak7755_reg_update_bits(AK7755_E6_CONT26, 0x01,0x01);
		/* System reset must be 1*/
		ak7755_reg_update_bits(AK7755_EA_CONT2A, 0x80,0x80);
		ak7755_set_status(STANDBY);
	}else{
		ak7755_set_status(POWERDOWN);
		if (reset_gpio > 0) {
			gpio_set_value(ak7755_data->reset_gpio, 1);
			msleep(1);
			gpio_set_value(ak7755_data->reset_gpio, 0);
		}
	}
	return AUDIO_SUCCESS;
}

static struct codec_attributes ak7755_attrs = {
	.name = "ak7755",
	.mode = CODEC_IS_SLAVE_MODE,
	.type = CODEC_IS_EXTERN_MODULE,
	.pins = CODEC_IS_4_LINES,
	.transfer_protocol = DATA_LEFT_JUSTIFIED_MODE,
	.set_sign = ak7755_set_sign,
	.set_power = ak7755_set_power,
	.detect = NULL,
	.record = &ak7755_record,
	.playback = &ak7755_playback,
};

static int ak7755_gpio_init(struct excodec_driver_data *ak7755_data)
{
//	ak7755_data->reset_gpio = reset_gpio;
	sscanf(gpio_func,"%d,%d,0x%x", &ak7755_data->port, &ak7755_data->func, &ak7755_data->gpios);
	if(ak7755_data->gpios){
		return audio_set_gpio_function(ak7755_data->port, ak7755_data->func, ak7755_data->gpios);
	}
	return AUDIO_SUCCESS;
}

static int ak7755_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
	int ret = 0;
	ak7755_data = (struct excodec_driver_data *)kzalloc(sizeof(struct excodec_driver_data), GFP_KERNEL);
	if(!ak7755_data) {
		printk("failed to alloc ak7755 driver data\n");
		return -ENOMEM;
	}
	memset(ak7755_data, 0, sizeof(*ak7755_data));
	ak7755_data->attrs = &ak7755_attrs;
	if(ak7755_gpio_init(ak7755_data) < 0){
		printk("failed to init ak7755 gpio function\n");
		ret = -EPERM;
		goto failed_init_gpio;
	}

	ret = gpio_request(reset_gpio, "AK7755-PDN");
	if (ret < 0) {
		ret = -EPERM;
		printk("request gpio AK7755 PDN failed!\n");
		goto failed_request_reset_gpio;
	}
	gpio_direction_output(reset_gpio, 0);
	ak7755_data->reset_gpio = reset_gpio;

	if(spk_gpio != -1){
		if (gpio_request(spk_gpio,"gpio_spk_en") < 0) {
			gpio_free(spk_gpio);
			printk(KERN_DEBUG"request spk en gpio %d error!\n", spk_gpio);
			ret = -EPERM;
			goto failed_request_spk_gpio;
		}
	}
	set_codec_devdata(&ak7755_attrs,i2c);
	set_codec_hostdata(&ak7755_attrs, &ak7755_data);
	i2c_set_clientdata(i2c, &ak7755_attrs);
	printk("probe ok -------->ak7755.\n");
	return AUDIO_SUCCESS;
failed_request_spk_gpio:
	gpio_free(reset_gpio);
failed_request_reset_gpio:
failed_init_gpio:
	kfree(ak7755_data);
	ak7755_data = NULL;
	return ret;
}

static int ak7755_i2c_remove(struct i2c_client *client)
{
	struct excodec_driver_data *excodec = ak7755_data;

	ak7755_set_status(POWERDOWN);
	if (reset_gpio > 0) {
		gpio_set_value(ak7755_data->reset_gpio, 1);
		msleep(1);
		gpio_set_value(ak7755_data->reset_gpio, 0);
	}

	if (spk_gpio != -1) {
		gpio_set_value(spk_gpio, !spk_level);
		gpio_free(spk_gpio);
	}

	if(excodec){
		kfree(excodec);
		ak7755_data = NULL;
	}

	return AUDIO_SUCCESS;
}

static const struct i2c_device_id ak7755_i2c_id[] = {
	{ "ak7755", 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, ak7755_i2c_id);

static struct i2c_driver ak7755_i2c_driver = {
	.driver = {
		.name = "ak7755",
		.owner = THIS_MODULE,
	},
	.probe = ak7755_i2c_probe,
	.remove = ak7755_i2c_remove,
	.id_table = ak7755_i2c_id,
};

static int __init init_ak7755(void)
{
	int ret = 0;

	ret = i2c_add_driver(&ak7755_i2c_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "Failed to register external codec I2C driver: %d\n", ret);
	}
	return ret;
}

static void __exit exit_ak7755(void)
{
	i2c_del_driver(&ak7755_i2c_driver);
}

module_init(init_ak7755);
module_exit(exit_ak7755);

MODULE_DESCRIPTION("AK7755 external codec driver");
MODULE_LICENSE("GPL v2");
