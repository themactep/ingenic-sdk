/*
 * bf314a.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * resolution      fps       interface          mode
 * 1280*720       30        mipi_2lane        linear
 *
 */
#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>

#include <tx-isp-common.h>
#include <sensor-common.h>
#include <txx-funcs.h>

#define BF314A_CHIP_ID_H	(0x31)
#define BF314A_CHIP_ID_L	(0x4a)
#define BF314A_REG_END		0xff
#define BF314A_REG_DELAY	0xfffe
#define BF314A_SUPPORT_30FPS_SCLK (36000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20240801a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut bf314a_again_lut[] = {
	//cnt_gain = 161 cnt_reg = 161
	{0x000f, 0},
	{0x0010, 5732},
	{0x0011, 11136},
	{0x0012, 16248},
	{0x0013, 21098},
	{0x0014, 25711},
	{0x0015, 30109},
	{0x0016, 34312},
	{0x0017, 38336},
	{0x0018, 42196},
	{0x0019, 45904},
	{0x001a, 49472},
	{0x001b, 52911},
	{0x001c, 56229},
	{0x001d, 59434},
	{0x001e, 62534},
	{0x001f, 65536},
	{0x0020, 68445},
	{0x0021, 71268},
	{0x0022, 74009},
	{0x0023, 76672},
	{0x0024, 79263},
	{0x0025, 81784},
	{0x0026, 84240},
	{0x0027, 86634},
	{0x0028, 88969},
	{0x0029, 91247},
	{0x002a, 93472},
	{0x002b, 95645},
	{0x002c, 97770},
	{0x002d, 99848},
	{0x002e, 101882},
	{0x002f, 103872},
	{0x0030, 105822},
	{0x0031, 107732},
	{0x0032, 109604},
	{0x0033, 111440},
	{0x0034, 113241},
	{0x0035, 115008},
	{0x0036, 116743},
	{0x0037, 118447},
	{0x0038, 120120},
	{0x0039, 121765},
	{0x003a, 123381},
	{0x003b, 124970},
	{0x003c, 126533},
	{0x003d, 128070},
	{0x003e, 129583},
	{0x003f, 131072},
	{0x0040, 132538},
	{0x0041, 133981},
	{0x0042, 135403},
	{0x0043, 136804},
	{0x0044, 138184},
	{0x0045, 139545},
	{0x0046, 140886},
	{0x0047, 142208},
	{0x0048, 143512},
	{0x0049, 144799},
	{0x004a, 146068},
	{0x004b, 147320},
	{0x004c, 148556},
	{0x004d, 149776},
	{0x004e, 150981},
	{0x004f, 152170},
	{0x0050, 153344},
	{0x0051, 154505},
	{0x0052, 155651},
	{0x0053, 156783},
	{0x0054, 157902},
	{0x0055, 159008},
	{0x0056, 160101},
	{0x0057, 161181},
	{0x0058, 162250},
	{0x0059, 163306},
	{0x005a, 164351},
	{0x005b, 165384},
	{0x005c, 166406},
	{0x005d, 167418},
	{0x005e, 168418},
	{0x005f, 169408},
	{0x0060, 170388},
	{0x0061, 171358},
	{0x0062, 172318},
	{0x0063, 173268},
	{0x0064, 174209},
	{0x0065, 175140},
	{0x0066, 176062},
	{0x0067, 176976},
	{0x0068, 177881},
	{0x0069, 178777},
	{0x006a, 179665},
	{0x006b, 180544},
	{0x006c, 181416},
	{0x006d, 182279},
	{0x006e, 183135},
	{0x006f, 183983},
	{0x0070, 184823},
	{0x0071, 185656},
	{0x0072, 186482},
	{0x0073, 187301},
	{0x0074, 188112},
	{0x0075, 188917},
	{0x0076, 189715},
	{0x0077, 190506},
	{0x0078, 191291},
	{0x0079, 192069},
	{0x007a, 192841},
	{0x007b, 193606},
	{0x007c, 194366},
	{0x007d, 195119},
	{0x007e, 195866},
	{0x007f, 196608},
	{0x0080, 197344},
	{0x0081, 198074},
	{0x0082, 198798},
	{0x0083, 199517},
	{0x0084, 200231},
	{0x0085, 200939},
	{0x0086, 201642},
	{0x0087, 202340},
	{0x0088, 203033},
	{0x0089, 203720},
	{0x008a, 204403},
	{0x008b, 205081},
	{0x008c, 205754},
	{0x008d, 206422},
	{0x008e, 207085},
	{0x008f, 207744},
	{0x0090, 208399},
	{0x0091, 209048},
	{0x0092, 209694},
	{0x0093, 210335},
	{0x0094, 210971},
	{0x0095, 211604},
	{0x0096, 212232},
	{0x0097, 212856},
	{0x0098, 213476},
	{0x0099, 214092},
	{0x009a, 214704},
	{0x009b, 215312},
	{0x009c, 215916},
	{0x009d, 216517},
	{0x009e, 217113},
	{0x009f, 217706},
	{0x00a0, 218295},
	{0x00a1, 218880},
	{0x00a2, 219462},
	{0x00a3, 220041},
	{0x00a4, 220615},
	{0x00a5, 221187},
	{0x00a6, 221754},
	{0x00a7, 222319},
	{0x00a8, 222880},
	{0x00a9, 223438},
	{0x00aa, 223992},
	{0x00ab, 224544},
	{0x00ac, 225092},
	{0x00ad, 225637},
	{0x00ae, 226179},
	{0x00af, 226717},
	{0x00b0, 227253},
	{0x00b1, 227786},
	{0x00b2, 228315},
	{0x00b3, 228842},
	{0x00b4, 229366},
	{0x00b5, 229887},
	{0x00b6, 230405},
	{0x00b7, 230920},
	{0x00b8, 231433},
	{0x00b9, 231942},
	{0x00ba, 232449},
	{0x00bb, 232954},
	{0x00bc, 233455},
	{0x00bd, 233954},
	{0x00be, 234450},
	{0x00bf, 234944},
	{0x00c0, 235435},
	{0x00c1, 235924},
	{0x00c2, 236410},
	{0x00c3, 236894},
	{0x00c4, 237375},
	{0x00c5, 237854},
	{0x00c6, 238330},
	{0x00c7, 238804},
	{0x00c8, 239275},
	{0x00c9, 239745},
	{0x00ca, 240211},
	{0x00cb, 240676},
	{0x00cc, 241138},
	{0x00cd, 241598},
	{0x00ce, 242056},
	{0x00cf, 242512},
	{0x00d0, 242965},
	{0x00d1, 243417},
	{0x00d2, 243866},
	{0x00d3, 244313},
	{0x00d4, 244758},
	{0x00d5, 245201},
	{0x00d6, 245642},
	{0x00d7, 246080},
	{0x00d8, 246517},
	{0x00d9, 246952},
	{0x00da, 247384},
	{0x00db, 247815},
	{0x00dc, 248244},
	{0x00dd, 248671},
	{0x00de, 249096},
	{0x00df, 249519},
	{0x00e0, 249940},
	{0x00e1, 250359},
	{0x00e2, 250777},
	{0x00e3, 251192},
	{0x00e4, 251606},
	{0x00e5, 252018},
	{0x00e6, 252428},
	{0x00e7, 252837},
	{0x00e8, 253243},
	{0x00e9, 253648},
	{0x00ea, 254051},
	{0x00eb, 254453},
	{0x00ec, 254853},
	{0x00ed, 255251},
	{0x00ee, 255647},
	{0x00ef, 256042},
	{0x00f0, 256435},
	{0x00f1, 256827},
	{0x00f2, 257217},
	{0x00f3, 257605},
	{0x00f4, 257992},
	{0x00f5, 258377},
	{0x00f6, 258760},
	{0x00f7, 259142},
	{0x00f8, 259523},
	{0x00f9, 259902},
	{0x00fa, 260279},
	{0x00fb, 260655},
	{0x00fc, 261029},
	{0x00fd, 261402},
	{0x00fe, 261774},
	{0x00ff, 262144},
};

struct tx_isp_sensor_attribute bf314a_attr;

unsigned int bf314a_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = bf314a_again_lut;
	while(lut->gain <= bf314a_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == bf314a_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int bf314a_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute bf314a_attr={
	.name = "bf314a",
	.chip_id = 0x314a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x6e,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 360,
		.lans = 1,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1280,
		.image_theight = 720,
		.mipi_sc.mipi_crop_start0x = 0,
		.mipi_sc.mipi_crop_start0y = 0,
		.mipi_sc.mipi_crop_start1x = 0,
		.mipi_sc.mipi_crop_start1y = 0,
		.mipi_sc.mipi_crop_start2x = 0,
		.mipi_sc.mipi_crop_start2y = 0,
		.mipi_sc.mipi_crop_start3x = 0,
		.mipi_sc.mipi_crop_start3y = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 262144,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 750 - 6,
	.integration_time_limit = 750 - 6,
	.total_width = 2000,
	.total_height = 750,
	.max_integration_time = 750 - 6,
	.one_line_expr_in_us = 27,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = bf314a_alloc_again,
	.sensor_ctrl.alloc_dgain = bf314a_alloc_dgain,
};

static struct regval_list bf314a_init_regs_1280_720_30fps_mipi[] = {
	{0xf3,0x00},
	{0xf3,0x00},
	{0x00,0x21},
	{0x7d,0x0f},
	{0xeb,0x05},
	{0x0b,0x20},
	{0x0c,0x03},
	{0x26,0xd0},
	{0x27,0x38},
	{0x2b,0xaa},
	{0xe0,0x0b},
	{0xe1,0xaa},
	{0xe2,0x06},
	{0xe3,0x4a},
	{0x5e,0x32},
	{0x59,0x10},
	{0x5a,0x10},
	{0x5b,0x10},
	{0x5c,0x10},
	{0x6a,0x1f},
	{0x6b,0x02},
	{0x6c,0xd0},
	{0x6f,0x10},
	{BF314A_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting bf314a_win_sizes[] = {
	{
		.width		= 1280,
		.height		= 720,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= bf314a_init_regs_1280_720_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &bf314a_win_sizes[0];

static struct regval_list bf314a_stream_on_mipi[] = {
	{0xf3, 0x00},
	{BF314A_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf314a_stream_off_mipi[] = {
	{0xf3, 0x01},
	{BF314A_REG_END, 0x00},	/* END MARKER */
};

int bf314a_read(struct tx_isp_subdev *sd, unsigned char reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int bf314a_write(struct tx_isp_subdev *sd, unsigned char reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int bf314a_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != BF314A_REG_END) {
		if (vals->reg_num == BF314A_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = bf314a_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int bf314a_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != BF314A_REG_END) {
		if (vals->reg_num == BF314A_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = bf314a_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int bf314a_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int bf314a_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret = 0;
	unsigned char v;

	ret += bf314a_read(sd, 0xfc, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != BF314A_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret += bf314a_read(sd, 0xfd, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != BF314A_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int bf314a_set_expo(struct tx_isp_subdev *sd, int value)
{

	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	//integration time
	ret = bf314a_write(sd, 0x6b, (unsigned char)((it >> 8) & 0xff));
	ret += bf314a_write(sd, 0x6c, (unsigned char)((it & 0xff)));


	//sensor analog gain
	// ret += bf314a_write(sd, 0x6a, (unsigned char)(((again >> 8) & 0xff)));
	//sensor dig fine gain
	ret += bf314a_write(sd, 0x6a, (unsigned char)(again & 0xff));
	
	if (ret < 0)
		return ret;

	return 0;
}

static int bf314a_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int bf314a_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int bf314a_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int bf314a_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	ret = bf314a_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int bf314a_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = bf314a_write_array(sd, bf314a_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("bf314a stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = bf314a_write_array(sd, bf314a_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("bf314a stream off\n");
	}

	return ret;
}

static int bf314a_set_fps(struct tx_isp_subdev *sd, int fps)
{

	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int vts_base = 0;
	unsigned int vb = 0;
	int ret = 0;

	sclk = BF314A_SUPPORT_30FPS_SCLK;
	vts_base = 738;

	printk("-------fps=%d\n",((fps >> 16) / (fps & 0xff)));
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	ret = bf314a_read(sd, 0x0c, &tmp);
	hts = tmp;
	ret += bf314a_read(sd, 0x0b, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: bf314a read err\n");
		return ret;
	}
	hts = ((hts << 8) | tmp) << 1;

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	vb = vts - vts_base;

	printk("-------hts=%d\n",hts);
	printk("-------vts=%d\n",vts);
	
	ret += bf314a_write(sd, 0x06, (unsigned char)(vb & 0xff));
	ret += bf314a_write(sd, 0x07, (unsigned char)(vb >> 8));

	
	if (0 != ret) {
		ISP_ERROR("err: bf314a_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 6;
	sensor->video.attr->integration_time_limit = vts - 6;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 6;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;

}

static int bf314a_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int bf314a_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"bf314a_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"bf314a_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = bf314a_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an bf314a chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("bf314a chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "bf314a", sizeof("bf314a"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int bf314a_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	uint8_t val;

	val = bf314a_read(sd, 0x00, &val);
	switch(enable) {
	case 0:
                val &= 0xf3;
				sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		break;
	case 1:
                val = ((val & 0xF7) | 0x08);//mirror
				sensor->video.mbus.code = V4L2_MBUS_FMT_SGBRG10_1X10;
		break;
	case 2:

                val = ((val & 0xFb) | 0x04);//flip
				sensor->video.mbus.code = V4L2_MBUS_FMT_SGRBG10_1X10;
		break;
	case 3:
                val = ((val &0xff) | 0x0c);
				sensor->video.mbus.code = V4L2_MBUS_FMT_SRGGB10_1X10;
		break;
	}
	bf314a_write(sd, 0x00, val);

	sensor->video.mbus_change = 1;
	tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int bf314a_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
	     	ret = bf314a_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	//	if(arg)
	//		ret = bf314a_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
	//	if(arg)
	//		ret = bf314a_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = bf314a_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = bf314a_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = bf314a_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = bf314a_write_array(sd, bf314a_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = bf314a_write_array(sd, bf314a_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = bf314a_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = bf314a_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = bf314a_set_logic(sd, *(int*)arg);
	default:
		break;
	}

	return ret;
}

static int bf314a_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = bf314a_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int bf314a_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	bf314a_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops bf314a_core_ops = {
	.g_chip_ident = bf314a_g_chip_ident,
	.reset = bf314a_reset,
	.init = bf314a_init,
	/*.ioctl = bf314a_ops_ioctl,*/
	.g_register = bf314a_g_register,
	.s_register = bf314a_s_register,
};

static struct tx_isp_subdev_video_ops bf314a_video_ops = {
	.s_stream = bf314a_s_stream,
};

static struct tx_isp_subdev_sensor_ops	bf314a_sensor_ops = {
	.ioctl	= bf314a_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops bf314a_ops = {
	.core = &bf314a_core_ops,
	.video = &bf314a_video_ops,
	.sensor = &bf314a_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "bf314a",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int bf314a_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

#ifdef CONFIG_KERNEL_4_4_94
		sensor->mclk = clk_get(NULL, "div_cim");
#else
		sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
        if (IS_ERR(sensor->mclk)) {
                ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
                goto err_get_mclk;
        }
        {
                unsigned int arate = 0,mrate = 0;
                unsigned int want_rate = 0;
                struct clk *clka = NULL;
                struct clk *clkm = NULL;

                want_rate=24000000;
                clka = clk_get(NULL, "sclka");
                clkm = clk_get(NULL, "mpll");
                arate = clk_get_rate(clka);
                mrate = clk_get_rate(clkm);
                if((arate%want_rate) && (mrate%want_rate)) {
                        if(want_rate == 37125000){
                                if(arate >= 1400000000) {
                                        arate = 1485000000;
                                } else if((arate >= 1100) || (arate < 1400)) {
                                        arate = 1188000000;
                                } else if(arate <= 1100) {
                                        arate = 891000000;
                                }
                        } else {
                                mrate = arate%want_rate;
                                arate = arate-mrate;
                        }
                        clk_set_rate(clka, arate);
                        clk_set_parent(sensor->mclk, clka);
                } else if(!(arate%want_rate)) {
                        clk_set_parent(sensor->mclk, clka);
                } else if(!(mrate%want_rate)) {
                        clk_set_parent(sensor->mclk, clkm);
                }
                private_clk_set_rate(sensor->mclk, want_rate);
                private_clk_enable(sensor->mclk);
        }

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &bf314a_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &bf314a_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->bf314a\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int bf314a_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id bf314a_id[] = {
	{ "bf314a", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bf314a_id);

static struct i2c_driver bf314a_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "bf314a",
	},
	.probe		= bf314a_probe,
	.remove		= bf314a_remove,
	.id_table	= bf314a_id,
};

static __init int init_bf314a(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init bf314a dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&bf314a_driver);
}

static __exit void exit_bf314a(void)
{
	private_i2c_del_driver(&bf314a_driver);
}

module_init(init_bf314a);
module_exit(exit_bf314a);

MODULE_DESCRIPTION("A low-level driver for SmartSens bf314a sensors");
MODULE_LICENSE("GPL");
