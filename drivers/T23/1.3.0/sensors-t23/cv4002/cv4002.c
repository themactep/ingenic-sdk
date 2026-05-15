/*
 * cv4002.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @fsync FSync hardware connection:The VFSYNC of the two cameras is connected, and the HSYNC is connected.
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          2304*1296       25        mipi_2lane            linear
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

#define CV4002_CHIP_ID_H	(0x02)
#define CV4002_CHIP_ID_L	(0x40)
#define cv4002_REG_END		0xffff
#define cv4002_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20241113b"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

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

struct again_lut cv4002_again_lut[] = {
	{0x0, 0},
	{0x1, 377},
	{0x2, 753},
	{0x3, 1127},
	{0x4, 1500},
	{0x5, 1872},
	{0x6, 2242},
	{0x7, 2610},
	{0x8, 2978},
	{0x9, 3343},
	{0xa, 3799},
	{0xb, 4161},
	{0xc, 4522},
	{0xd, 4882},
	{0xe, 5330},
	{0xf, 5687},
	{0x10, 6131},
	{0x11, 6485},
	{0x12, 6925},
	{0x13, 7276},
	{0x14, 7713},
	{0x15, 8061},
	{0x16, 8494},
	{0x17, 8925},
	{0x18, 9268},
	{0x19, 9696},
	{0x1a, 10122},
	{0x1b, 10545},
	{0x1c, 10967},
	{0x1d, 11387},
	{0x1e, 11805},
	{0x1f, 12222},
	{0x20, 12636},
	{0x21, 13049},
	{0x22, 13460},
	{0x23, 13869},
	{0x24, 14358},
	{0x25, 14763},
	{0x26, 15166},
	{0x27, 15648},
	{0x28, 16048},
	{0x29, 16526},
	{0x2a, 16922},
	{0x2b, 17395},
	{0x2c, 17866},
	{0x2d, 18256},
	{0x2e, 18723},
	{0x2f, 19187},
	{0x30, 19649},
	{0x31, 20109},
	{0x32, 20566},
	{0x33, 21021},
	{0x34, 21474},
	{0x35, 21925},
	{0x36, 22374},
	{0x37, 22895},
	{0x38, 23339},
	{0x39, 23782},
	{0x3a, 24295},
	{0x3b, 24733},
	{0x3c, 25241},
	{0x3d, 25746},
	{0x3e, 26249},
	{0x3f, 26678},
	{0x40, 27175},
	{0x41, 27671},
	{0x42, 28163},
	{0x43, 28653},
	{0x44, 29210},
	{0x45, 29695},
	{0x46, 30177},
	{0x47, 30725},
	{0x48, 31202},
	{0x49, 31744},
	{0x4a, 32284},
	{0x4b, 32753},
	{0x4c, 33286},
	{0x4d, 33817},
	{0x4e, 34344},
	{0x4f, 34869},
	{0x50, 35455},
	{0x51, 35974},
	{0x52, 36489},
	{0x53, 37066},
	{0x54, 37576},
	{0x55, 38146},
	{0x56, 38712},
	{0x57, 39276},
	{0x58, 39836},
	{0x59, 40393},
	{0x5a, 40946},
	{0x5b, 41557},
	{0x5c, 42104},
	{0x5d, 42708},
	{0x5e, 43248},
	{0x5f, 43844},
	{0x60, 44437},
	{0x61, 45026},
	{0x62, 45611},
	{0x63, 46251},
	{0x64, 46829},
	{0x65, 47461},
	{0x66, 48031},
	{0x67, 48655},
	{0x68, 49275},
	{0x69, 49890},
	{0x6a, 50557},
	{0x6b, 51165},
	{0x6c, 51823},
	{0x6d, 52422},
	{0x6e, 53071},
	{0x6f, 53770},
	{0x70, 54410},
	{0x71, 55046},
	{0x72, 55730},
	{0x73, 56410},
	{0x74, 57084},
	{0x75, 57754},
	{0x76, 58419},
	{0x77, 59130},
	{0x78, 59785},
	{0x79, 60486},
	{0x7a, 61181},
	{0x7b, 61921},
	{0x7c, 62606},
	{0x7d, 63335},
	{0x7e, 64058},
	{0x7f, 64775},
	{0x80, 65535},
	{0x81, 66288},
	{0x82, 67035},
	{0x83, 67777},
	{0x84, 68558},
	{0x85, 69288},
	{0x86, 70057},
	{0x87, 70865},
	{0x88, 71622},
	{0x89, 72416},
	{0x8a, 73204},
	{0x8b, 74029},
	{0x8c, 74846},
	{0x8d, 75657},
	{0x8e, 76502},
	{0x8f, 77299},
	{0x90, 78171},
	{0x91, 78995},
	{0x92, 79852},
	{0x93, 80742},
	{0x94, 81583},
	{0x95, 82496},
	{0x96, 83362},
	{0x97, 84258},
	{0x98, 85184},
	{0x99, 86063},
	{0x9a, 87009},
	{0x9b, 87947},
	{0x9c, 88874},
	{0x9d, 89830},
	{0x9e, 90776},
	{0x9f, 91748},
	{0xa0, 92746},
	{0xa1, 93733},
	{0xa2, 94710},
	{0xa3, 95746},
	{0xa4, 96771},
	{0xa5, 97785},
	{0xa6, 98821},
	{0xa7, 99879},
	{0xa8, 100958},
	{0xa9, 102056},
	{0xaa, 103142},
	{0xab, 104247},
	{0xac, 105371},
	{0xad, 106481},
	{0xae, 107639},
	{0xaf, 108783},
	{0xb0, 109972},
	{0xb1, 111176},
	{0xb2, 112364},
	{0xb3, 113595},
	{0xb4, 114810},
	{0xb5, 116065},
	{0xb6, 117330},
	{0xb7, 118633},
	{0xb8, 119945},
	{0xb9, 121265},
	{0xba, 122593},
	{0xbb, 123954},
	{0xbc, 125345},
	{0xbd, 126741},
	{0xbe, 128165},
	{0xbf, 129593},
	{0xc0, 131070},
	{0xc1, 132547},
	{0xc2, 134071},
	{0xc3, 135615},
	{0xc4, 137179},
	{0xc5, 138761},
	{0xc6, 140381},
	{0xc7, 142016},
	{0xc8, 143686},
	{0xc9, 145407},
	{0xca, 147138},
	{0xcb, 148897},
	{0xcc, 150700},
	{0xcd, 152544},
	{0xce, 154409},
	{0xcf, 156311},
	{0xd0, 158263},
	{0xd1, 160263},
	{0xd2, 162289},
	{0xd3, 164373},
	{0xd4, 166493},
	{0xd5, 168661},
	{0xd6, 170890},
	{0xd7, 173174},
	{0xd8, 175507},
	{0xd9, 177899},
	{0xda, 180359},
	{0xdb, 182879},
	{0xdc, 185467},
	{0xdd, 188128},
	{0xde, 190867},
	{0xdf, 193700},
	{0xe0, 196605},
	{0xe1, 199606},
	{0xe2, 202703},
	{0xe3, 205916},
	{0xe4, 209231},
	{0xe5, 212663},
	{0xe6, 216235},
	{0xe7, 219944},
	{0xe8, 223807},
	{0xe9, 227824},
	{0xea, 232028},
	{0xeb, 236425},
	{0xec, 241042},
	{0xed, 245894},
	{0xee, 251002},
	{0xef, 256409},
	{0xf0, 262140},
	{0xf1, 268243},
	{0xf2, 274766},
	{0xf3, 281770},
	{0xf4, 289338},
	{0xf5, 297567},
	{0xf6, 306577},
	{0xf7, 316537},
	{0xf8, 327675},
};

struct tx_isp_sensor_attribute cv4002_attr;

unsigned int cv4002_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = cv4002_again_lut;
	while(lut->gain <= cv4002_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == cv4002_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int cv4002_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute cv4002_attr={
	.name = "cv4002",
	.chip_id = 0x4002,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x35,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 717,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 2304,
		.image_theight = 1296,
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
	.max_again = 327675,
	.max_dgain = 0,
	.min_integration_time = 6,
	.min_integration_time_native = 6,
	.max_integration_time_native = 4091 - 2,
	.integration_time_limit = 4091 - 2,
	.total_width = 880,
	.total_height = 4091,
	.max_integration_time = 4091 - 2,
	.integration_time_apply_delay = 2,
	.again = 0,
	.integration_time = 0x0a,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = cv4002_alloc_again,
	.sensor_ctrl.alloc_dgain = cv4002_alloc_dgain,
};

static struct regval_list cv4002_init_regs_2304_1296_25fps_mipi[] = {
	{0x3024, 0x70},
	{0x3025, 0x03},
	{0x3029, 0x00},
	{0x302A, 0x00},
	{0x3300, 0x00},
	{0x3401, 0x01},
	{0x3418, 0x97},
	{0x3419, 0x00},
	{0x341A, 0x4F},
	{0x341B, 0x00},
	{0x341C, 0x4F},
	{0x341D, 0x00},
	{0x341E, 0x4F},
	{0x341F, 0x01},
	{0x3420, 0x4F},
	{0x3421, 0x00},
	{0x3422, 0x97},
	{0x3423, 0x00},
	{0x3424, 0x4F},
	{0x3425, 0x00},
	{0x3426, 0x7F},
	{0x3427, 0x00},
	{0x3428, 0x3F},
	{0x3429, 0x00},
	{0x3440, 0x01},
	{0x3442, 0x00},
	{0x3806, 0x02},
	{0x3486, 0x2B},
	{0x3031, 0x00},
	{0x3118, 0x01},
	{0x3119, 0x06},
	{0x3330, 0x00},
	{0x332D, 0x40},
	{0x3048, 0x0A},
	{0x3049, 0x00},
	{0x3148, 0x64},
	{0x3670, 0x00},
	{0x3679, 0x02},
	{0x320e, 0x03},
	{0x35ab, 0x08},
	{0x35b3, 0x10},
	{0x3804, 0x15},
	{0x397a, 0x1d},
	{0x3852, 0x00},
	{0x3230, 0x00},
	{0x3231, 0x00},
	{0x3232, 0x01},
	{0x3120, 0x03},
	{0x3121, 0x01},
	{0x3109, 0x04},
	{0x313A, 0x04},
	{0x3124, 0x6C},
	{0x3125, 0x0B},
	{0x3126, 0x10},
	{0x3127, 0x00},
	{0x327C, 0x68},
	{0x327D, 0x0B},
	{0x327E, 0x64},
	{0x327F, 0x0B},
	{0x3284, 0x6A},
	{0x3285, 0x0B},
	{0x3286, 0x66},
	{0x3287, 0x0B},
	{0x3282, 0x60},
	{0x3283, 0x0B},
	{0x328A, 0x62},
	{0x328B, 0x0B},
	{0x3B55, 0x01},
	{0x3B56, 0x01},
	{0x3021, 0x0F},
	{0x3020, 0xFB},
	{0x3022, 0x00},
	{0x3909, 0x00},
	{0x3908, 0x3C},
	{0x3668, 0x00},
	{0x365A, 0x01},
	{0x366c, 0x00},
	{0x3678, 0x00},
	{0x3806, 0x01},
	{0x3141, 0x01},
	{0x301C, 0x04},
	{0x303C, 0x48},
	{0x303D, 0x00},
	{0x303E, 0x20},
	{0x303F, 0x05},
	{0x3030, 0x01},
	{0x3038, 0x88},
	{0x3039, 0x00},
	{0x303A, 0x00},
	{0x303B, 0x09},
	{0x3034, 0x08},
	{0x3035, 0x00},
	{0x3036, 0x10},
	{0x3037, 0x05},
	{0x3000, 0x00},
	{cv4002_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting cv4002_win_sizes[] = {
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= cv4002_init_regs_2304_1296_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &cv4002_win_sizes[0];

static struct regval_list cv4002_stream_on_mipi[] = {
	{0x3000, 0x00},
	{cv4002_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list cv4002_stream_off_mipi[] = {
	{0x3000, 0x01},
	{cv4002_REG_END, 0x00},	/* END MARKER */
};

int cv4002_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
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

int cv4002_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int cv4002_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != cv4002_REG_END) {
		if (vals->reg_num == cv4002_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4002_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int cv4002_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != cv4002_REG_END) {
		if (vals->reg_num == cv4002_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv4002_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int cv4002_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int cv4002_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = cv4002_read(sd, 0x3002, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != CV4002_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = cv4002_read(sd, 0x3003, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != CV4002_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int cv4002_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	int shutter = 0;

	shutter = cv4002_attr.total_height - it;
	shutter = ((shutter << 1) >> 1);
	ret += cv4002_write(sd, 0x304a, (unsigned char)((shutter >> 12) & 0xff));
	ret += cv4002_write(sd, 0x3049, (unsigned char)((shutter >> 8) & 0xff));
	ret += cv4002_write(sd, 0x3048, (unsigned char)(shutter & 0xff));

	ret += cv4002_write(sd, 0x3154, (unsigned char)(again & 0xff));

	return ret;
}

#if 0
static int cv4002_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int shutter = 0;

	shutter = cv4002_attr.total_height - value;
	shutter = ((shutter << 1) >> 1);
	ret += cv4002_write(sd, 0x304a, (unsigned char)((shutter >> 12) & 0xff));
	ret += cv4002_write(sd, 0x3049, (unsigned char)((shutter >> 8) & 0xff));
	ret += cv4002_write(sd, 0x3048, (unsigned char)(shutter & 0xff));

	return ret;
}

static int cv4002_set_analog_gain(struct tx_isp_subdev *sd, int value)
{

	int ret = ISP_SUCCESS;

	ret += cv4002_write(sd, 0x3154, (unsigned char)(value & 0xff));

	return ret;
}
#endif

static int cv4002_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv4002_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv4002_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv4002_init(struct tx_isp_subdev *sd, int enable)
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

	ret = cv4002_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int cv4002_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = cv4002_write_array(sd, cv4002_stream_on_mipi);
		ISP_WARNING("cv4002 stream on\n");

	} else {
		ret = cv4002_write_array(sd, cv4002_stream_off_mipi);
		ISP_WARNING("cv4002 stream off\n");
	}

	return ret;
}

static int cv4002_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int max_fps = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	sclk = 880 * 4091 * 25;
	max_fps = 25;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(0x%x) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret = cv4002_read(sd, 0x3025, &val);
	hts = val << 8;
	ret += cv4002_read(sd, 0x3024, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("err: cv4002 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	cv4002_write(sd, 0x3020, (unsigned char) (vts & 0xff));
	cv4002_write(sd, 0x3021, (unsigned char) ((vts >> 8) & 0xff));
	cv4002_write(sd, 0x3022, (unsigned char) ((vts >> 16) & 0x1f));
	if (0 != ret) {
		ISP_ERROR("err: cv4002_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts -2;
	sensor->video.attr->integration_time_limit = vts -2;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts -2;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int cv4002_set_mode(struct tx_isp_subdev *sd, int value)
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

static int cv4002_g_chip_ident(struct tx_isp_subdev *sd,
							   struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"cv4002_reset");
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
		ret = private_gpio_request(pwdn_gpio,"cv4002_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = cv4002_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an cv4002 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("cv4002 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "cv4002", sizeof("cv4002"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}


static int cv4002_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = cv4002_read(sd, 0x3028, &val);
	switch(enable) {
	case 0:
		val &= 0xFC;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SRGGB10_1X10;
		break;
	case 1:
		val = ((val & 0xFD) | 0x01);
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGRBG10_1X10;
		break;
	case 2:
		val = ((val & 0xFE) | 0x02);
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGBRG10_1X10;
		break;
	case 3:
		val |= 0x03;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		break;
	}
	sensor->video.mbus_change = 1;
	cv4002_write(sd, 0x3028, val);
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int cv4002_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = cv4002_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//		if(arg)
		//			ret = cv4002_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//		if(arg)
		//			ret = cv4002_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = cv4002_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = cv4002_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = cv4002_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = cv4002_write_array(sd, cv4002_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = cv4002_write_array(sd, cv4002_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = cv4002_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = cv4002_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = cv4002_set_logic(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int cv4002_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = cv4002_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int cv4002_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	cv4002_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops cv4002_core_ops = {
	.g_chip_ident = cv4002_g_chip_ident,
	.reset = cv4002_reset,
	.init = cv4002_init,
	/*.ioctl = cv4002_ops_ioctl,*/
	.g_register = cv4002_g_register,
	.s_register = cv4002_s_register,
};

static struct tx_isp_subdev_video_ops cv4002_video_ops = {
	.s_stream = cv4002_s_stream,
};

static struct tx_isp_subdev_sensor_ops	cv4002_sensor_ops = {
	.ioctl	= cv4002_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops cv4002_ops = {
	.core = &cv4002_core_ops,
	.video = &cv4002_video_ops,
	.sensor = &cv4002_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "cv4002",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_mclk_config(struct tx_isp_sensor *sensor, unsigned long want_rate)
{
	unsigned long rate = 0;
	struct clk *pll = NULL;
	char *plls[] = {"mpll", "sclka"};
	int psize = sizeof(plls) / sizeof(char *);
	char *ppll = plls[psize - 1];
	int ret = 0, i = 0;

	pll = clk_get_parent(sensor->mclk);
	rate = clk_get_rate(pll);
	if (rate % want_rate) {
		for (i = 0; i < psize; i++) {
			pll = clk_get(NULL, plls[i]);
			rate = clk_get_rate(pll);
			if (!(rate % want_rate)) {
				ret = clk_set_parent(sensor->mclk, pll);
				if (ret) {
					ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
								__func__, __LINE__, plls[i]);
					continue;
				} else {
					break;
				}
			}
		}
		if (i == psize) {
			if (!ret) {
				pll = clk_get(NULL, ppll);
				rate = clk_get_rate(pll);
				if(want_rate == 37125000){
					if((rate >= 1188000000)) {
						rate = 1188000000;
					} else if (rate >= 891000000) {
						rate = 891000000;
					} else {
						ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
								  __func__, __LINE__, ppll);
						ret = -1;
						goto error;
					}
				} else if (want_rate == 24000000 || want_rate == 27000000) {
					rate -= rate % want_rate;
				} else {
					ret = -1;
					goto error;
				}
				ret = private_clk_set_rate(pll, rate);
				if (ret) {
					ISP_WARNING("[%s %d] Failed to set %s !!!\n",
								__func__, __LINE__, ppll);
					goto error;
				} else {
					ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld !!!\n",
								__func__, __LINE__, ppll, rate);
				}
				ret = clk_set_parent(sensor->mclk, pll);
				if (ret) {
					ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
								__func__, __LINE__, ppll);
					goto error;
				}
			} else {
				goto error;
			}
		}
	}
	private_clk_set_rate(sensor->mclk, want_rate);
	private_clk_enable(sensor->mclk);

	rate = clk_get_rate(sensor->mclk);
	if (rate % want_rate) {
		ret = -1;
		goto error;
	}

	return ret;

error:
	ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
			  __func__, __LINE__, want_rate);
	return ret;
}

static int cv4002_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	sensor_mclk_config(sensor, 24000000);
	cv4002_attr.max_integration_time_native = 4091 - 2;
	cv4002_attr.integration_time_limit =4091 - 2;
	cv4002_attr.max_integration_time = 4091 - 2;

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &cv4002_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &cv4002_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->cv4002\n");

	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int cv4002_remove(struct i2c_client *client)
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

static const struct i2c_device_id cv4002_id[] = {
	{ "cv4002", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cv4002_id);

static struct i2c_driver cv4002_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "cv4002",
	},
	.probe		= cv4002_probe,
	.remove		= cv4002_remove,
	.id_table	= cv4002_id,
};

static __init int init_cv4002(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init cv4002 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&cv4002_driver);
}

static __exit void exit_cv4002(void)
{
	private_i2c_del_driver(&cv4002_driver);
}

module_init(init_cv4002);
module_exit(exit_cv4002);

MODULE_DESCRIPTION("A low-level driver for SmartSens cv4002 sensors");
MODULE_LICENSE("GPL");
