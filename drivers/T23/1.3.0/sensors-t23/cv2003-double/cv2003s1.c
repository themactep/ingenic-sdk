/*
 * cv2003s1.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @fsync FSync hardware connection:The VFSYNC of the two cameras is connected, and the HSYNC is connected
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       15        mipi_2lane           linear
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

#define CV2003S1_CHIP_ID_H	(0x20)
#define CV2003S1_CHIP_ID_L	(0x03)
#define cv2003s1_REG_END		0xffff
#define cv2003s1_REG_DELAY	0xfffe
#define cv2003s1_SUPPORT_30FPS_SCLK (2124 * 1130 * 30)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20240219a"

static int reset_gpio = -1;
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int fsync_mode = 3;
module_param(fsync_mode, int, S_IRUGO);
MODULE_PARM_DESC(fsync_mode, "Sensor Indicates the frame synchronization mode");

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

struct again_lut cv2003s1_again_lut[] = {
	{0x00, 0},
	{0x01, 377},
	{0x02, 753},
	{0x03, 1127},
	{0x04, 1500},
	{0x05, 1872},
	{0x06, 2242},
	{0x07, 2610},
	{0x08, 2978},
	{0x09, 3343},
	{0x0a, 3799},
	{0x0b, 4161},
	{0x0c, 4522},
	{0x0f, 4882},
	{0x0d, 5330},
	{0x0e, 5687},
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
	{0x1b, 10546},
	{0x1c, 10967},
	{0x1d, 11388},
	{0x1e, 11806},
	{0x1f, 12222},
	{0x20, 12637},
	{0x21, 13049},
	{0x22, 13460},
	{0x23, 13869},
	{0x24, 14358},
	{0x25, 14763},
	{0x26, 15167},
	{0x27, 15649},
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
	{0x33, 21022},
	{0x34, 21475},
	{0x35, 21926},
	{0x36, 22375},
	{0x37, 22896},
	{0x38, 23340},
	{0x39, 23782},
	{0x3a, 24295},
	{0x3b, 24733},
	{0x3c, 25241},
	{0x3d, 25746},
	{0x3e, 26249},
	{0x3f, 26678},
	{0x40, 27176},
	{0x41, 27671},
	{0x42, 28164},
	{0x43, 28654},
	{0x44, 29211},
	{0x45, 29695},
	{0x46, 30178},
	{0x47, 30726},
	{0x48, 31203},
	{0x49, 31745},
	{0x4a, 32284},
	{0x4b, 32753},
	{0x4c, 33287},
	{0x4d, 33817},
	{0x4e, 34345},
	{0x4f, 34869},
	{0x50, 35456},
	{0x51, 35974},
	{0x52, 36490},
	{0x53, 37066},
	{0x54, 37576},
	{0x55, 38146},
	{0x56, 38713},
	{0x57, 39276},
	{0x58, 39836},
	{0x59, 40393},
	{0x5a, 40947},
	{0x5b, 41558},
	{0x5c, 42104},
	{0x5d, 42708},
	{0x5e, 43248},
	{0x5f, 43845},
	{0x60, 44438},
	{0x61, 45027},
	{0x62, 45612},
	{0x63, 46252},
	{0x64, 46830},
	{0x65, 47462},
	{0x66, 48032},
	{0x67, 48656},
	{0x68, 49276},
	{0x69, 49891},
	{0x6a, 50558},
	{0x6b, 51165},
	{0x6c, 51824},
	{0x6d, 52423},
	{0x6e, 53072},
	{0x6f, 53771},
	{0x70, 54411},
	{0x71, 55047},
	{0x72, 55731},
	{0x73, 56411},
	{0x74, 57085},
	{0x75, 57755},
	{0x76, 58420},
	{0x77, 59130},
	{0x78, 59786},
	{0x79, 60487},
	{0x7a, 61182},
	{0x7b, 61922},
	{0x7c, 62607},
	{0x7d, 63335},
	{0x7e, 64059},
	{0x7f, 64776},
	{0x80, 65536},
	{0x81, 66289},
	{0x82, 67036},
	{0x83, 67778},
	{0x84, 68559},
	{0x85, 69289},
	{0x86, 70058},
	{0x87, 70866},
	{0x88, 71623},
	{0x89, 72417},
	{0x8a, 73205},
	{0x8b, 74030},
	{0x8c, 74847},
	{0x8d, 75658},
	{0x8e, 76503},
	{0x8f, 77300},
	{0x90, 78173},
	{0x91, 78996},
	{0x92, 79853},
	{0x93, 80743},
	{0x94, 81584},
	{0x95, 82498},
	{0x96, 83363},
	{0x97, 84259},
	{0x98, 85185},
	{0x99, 86064},
	{0x9a, 87011},
	{0x9b, 87948},
	{0x9c, 88876},
	{0x9d, 89831},
	{0x9e, 90777},
	{0x9f, 91749},
	{0xa0, 92747},
	{0xa1, 93735},
	{0xa2, 94712},
	{0xa3, 95748},
	{0xa4, 96773},
	{0xa5, 97786},
	{0xa6, 98823},
	{0xa7, 99881},
	{0xa8, 100959},
	{0xa9, 102058},
	{0xaa, 103144},
	{0xab, 104249},
	{0xac, 105372},
	{0xad, 106483},
	{0xae, 107640},
	{0xaf, 108784},
	{0xb0, 109974},
	{0xb1, 111177},
	{0xb2, 112366},
	{0xb3, 113597},
	{0xb4, 114812},
	{0xb5, 116066},
	{0xb6, 117332},
	{0xb7, 118635},
	{0xb8, 119947},
	{0xb9, 121267},
	{0xba, 122595},
	{0xbb, 123956},
	{0xbc, 125347},
	{0xbd, 126743},
	{0xbe, 128167},
	{0xbf, 129595},
	{0xc0, 131072},
	{0xc1, 132549},
	{0xc2, 134073},
	{0xc3, 135617},
	{0xc4, 137181},
	{0xc5, 138763},
	{0xc6, 140383},
	{0xc7, 142018},
	{0xc8, 143688},
	{0xc9, 145410},
	{0xca, 147140},
	{0xcb, 148899},
	{0xcc, 150702},
	{0xcd, 152547},
	{0xce, 154412},
	{0xcf, 156313},
	{0xd0, 158265},
	{0xd1, 160265},
	{0xd2, 162292},
	{0xd3, 164375},
	{0xd4, 166495},
	{0xd5, 168664},
	{0xd6, 170893},
	{0xd7, 173176},
	{0xd8, 175510},
	{0xd9, 177902},
	{0xda, 180362},
	{0xdb, 182882},
	{0xdc, 185470},
	{0xdd, 188131},
	{0xde, 190870},
	{0xdf, 193703},
	{0xe0, 196608},
	{0xe1, 199609},
	{0xe2, 202706},
	{0xe3, 205919},
	{0xe4, 209234},
	{0xe5, 212666},
	{0xe6, 216238},
	{0xe7, 219948},
	{0xe8, 223810},
	{0xe9, 227828},
	{0xea, 232031},
	{0xeb, 236429},
	{0xec, 241046},
	{0xed, 245898},
	{0xee, 251006},
	{0xef, 256413},
	{0xf0, 262144},
	{0xf1, 268247},
	{0xf2, 274770},
	{0xf3, 281774},
	{0xf4, 289342},
	{0xf5, 297571},
	{0xf6, 306582},
	{0xf7, 316542},
	{0xf8, 327680},
};

struct tx_isp_sensor_attribute cv2003s1_attr;

unsigned int cv2003s1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = cv2003s1_again_lut;
	while(lut->gain <= cv2003s1_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == cv2003s1_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int cv2003s1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute cv2003s1_attr={
	.name = "cv2003s1",
	.chip_id = 0x2003,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 960,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1920,
		.image_theight = 1080,
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
	.max_again = 327680,
	.max_dgain = 0,
	.min_integration_time = 6,
	.min_integration_time_native = 6,
	.max_integration_time_native = 0X1429-2,
	.integration_time_limit = 0X1429-2,
	.total_width = 2200,
	.total_height = 0x1429,
	.max_integration_time = 0X1429-2,
	.one_line_expr_in_us = 29,
	.integration_time_apply_delay = 6,
	.again = 0,
	.integration_time = 0x0,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = cv2003s1_alloc_again,
	.sensor_ctrl.alloc_dgain = cv2003s1_alloc_dgain,
        .fsync_attr = {
                .mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE,
                .call_times = 1,
                .sdelay = 500,

        }
};

static struct regval_list cv2003s1_init_regs_1920_1080_30fps_mipi[] = {
    {0x3020,0x60},
    {0x3021,0x09},
    {0x3022,0x00},
    {0x3024,0x70},
    {0x3025,0x02},
    {0x3029,0x00},
    {0x302A,0x00},
    {0x3300,0x01},
    {0x3401,0x01},
    {0x3440,0x03},
    {0x3442,0x00},
    {0x3806,0x01},
    {0x3908,0x4B},
    {0x3909,0x00},
    {0x3158,0x01},
    {0x3159,0x01},
    {0x315A,0x01},
    {0x315B,0x01},
    {0x3148,0x64},
    {0x3670,0x00},
    {0x3679,0x02},
    {0x35b3,0x15},
    {0x320E,0x02},
    {0x3804,0x10},
    {0x35a1,0x06},
    {0x35a8,0x06},
    {0x35a9,0x06},
    {0x35aa,0x06},
    {0x35ab,0x06},
    {0x35ac,0x06},
    {0x35ad,0x06},
    {0x35ae,0x07},
    {0x35af,0x07},
    {0x333B,0x01},
    {0x3339,0x00},
    {0x3031,0x00},
    {0x3118,0x01},
    {0x3119,0x06},
    {0x3400,0x11},
    {0x3330,0x00},
    {0x301C,0x04},
    {0x3020,0x29},
    {0x3021,0x14},
    {0x3024,0x6C},
    {0x3025,0x02},
    {0x3038,0x04},
    {0x3039,0x00},
    {0x303A,0x80},
    {0x303B,0x07},
    {0x303C,0x04},
    {0x303D,0x00},
    {0x303E,0x38},
    {0x303F,0x04},
    {0x3908,0x50},
    {0x390A,0x02},
  /** {0x3001,0x01},
    {0x307a,0x02},
    {0x306d,0x0f},
    {0x3078,0x04},
    {0x3000,0x00},
*/
        {cv2003s1_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting cv2003s1_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= cv2003s1_init_regs_1920_1080_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &cv2003s1_win_sizes[0];

static struct regval_list cv2003s1_stream_on_mipi[] = {
	{0x0100, 0x01},
	{cv2003s1_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list cv2003s1_stream_off_mipi[] = {
	{0x0100, 0x00},
	{cv2003s1_REG_END, 0x00},	/* END MARKER */
};

int cv2003s1_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int cv2003s1_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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
static int cv2003s1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != cv2003s1_REG_END) {
		if (vals->reg_num == cv2003s1_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv2003s1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int cv2003s1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != cv2003s1_REG_END) {
		if (vals->reg_num == cv2003s1_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv2003s1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int cv2003s1_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int cv2003s1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = cv2003s1_read(sd, 0x3011, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != CV2003S1_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = cv2003s1_read(sd, 0x3138, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != CV2003S1_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int cv2003s1_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	uint32_t it = (value & 0xffff);
    int expo=0;
	int again = (value & 0xffff0000) >> 16;
    expo=sensor->video.attr->total_height-it;
    ret += cv2003s1_write(sd, 0x3141, 0x01);
	ret += cv2003s1_write(sd, 0x304a, (unsigned char)((expo >> 16) & 0x0f));
	ret += cv2003s1_write(sd, 0x3049, (unsigned char)((expo >> 8) & 0xff));
	ret += cv2003s1_write(sd, 0x3048, (unsigned char)((expo & 0x0ff)));
	//ret += cv2003s1_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));
	ret += cv2003s1_write(sd, 0x3154, (unsigned char)(again & 0xff));
	if (ret < 0)
		return ret;
	return 0;
}

#if 0
static int cv2003s1_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value = ((value >> 1) << 1);
	ret += cv2003s1_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += cv2003s1_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int cv2003s1_set_analog_gain(struct tx_isp_subdev *sd, int value)
{

	int ret = 0;

	ret += cv2003s1_write(sd, 0x3e07, (unsigned char)(value & 0xff));
	ret += cv2003s1_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int cv2003s1_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv2003s1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv2003s1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv2003s1_init(struct tx_isp_subdev *sd, int enable)
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

	ret = cv2003s1_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int cv2003s1_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = cv2003s1_write_array(sd, cv2003s1_stream_on_mipi);
		ISP_WARNING("cv2003s1 stream on\n");

	} else {
		ret = cv2003s1_write_array(sd, cv2003s1_stream_off_mipi);
		ISP_WARNING("cv2003s1 stream off\n");
	}

	return ret;
}

static int cv2003s1_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int max_fps = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;


	sclk = 0x1429*0x26c*15;
	max_fps = 15;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(0x%x) no in range\n", fps);
		return -1;
	}

	ret = cv2003s1_read(sd, 0x3025, &tmp);
	hts = tmp;
	ret += cv2003s1_read(sd, 0x3024, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: cv2003s1 read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp);
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += cv2003s1_write(sd, 0x3020, (unsigned char)(vts & 0xff));
	ret += cv2003s1_write(sd, 0x3021, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: cv2003s1_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts-2;
	sensor->video.attr->integration_time_limit = vts-2;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts-2;
    ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int cv2003s1_set_mode(struct tx_isp_subdev *sd, int value)
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

static int cv2003s1_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"cv2003s1_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(35);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(35);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"cv2003s1_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = cv2003s1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an cv2003s1 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("cv2003s1 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "cv2003s1", sizeof("cv2003s1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}


static int cv2003s1_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = cv2003s1_read(sd, 0x3028, &val);
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
	cv2003s1_write(sd, 0x3028, val);
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int cv2003s1_fsync(struct tx_isp_subdev *sd, struct tx_isp_sensor_fsync *fsync)
{
        //int ret = 0;
        //uint32_t val;
        //uint32_t vts;

        if (fsync->place != TX_ISP_SENSOR_FSYNC_PLACE_STREAMON_AFTER)
                return 0;
        switch (fsync->call_index) {
        case 0:
                switch (fsync_mode) {
                case 3:
                        cv2003s1_write(sd, 0x3001, 0x01);
                        cv2003s1_write(sd, 0x307a, 0x02);
                        cv2003s1_write(sd, 0x306d, 0x0f);
                        cv2003s1_write(sd, 0x3078, 0x00);
                        cv2003s1_write(sd, 0x3000, 0x00);
                break;
                }
                break;
        }

        return 0;
}

static int cv2003s1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = cv2003s1_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
//		if(arg)
//			ret = cv2003s1_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if(arg)
//			ret = cv2003s1_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = cv2003s1_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = cv2003s1_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = cv2003s1_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = cv2003s1_write_array(sd, cv2003s1_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = cv2003s1_write_array(sd, cv2003s1_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = cv2003s1_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = cv2003s1_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = cv2003s1_set_logic(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int cv2003s1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = cv2003s1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int cv2003s1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	cv2003s1_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops cv2003s1_core_ops = {
	.g_chip_ident = cv2003s1_g_chip_ident,
	.reset = cv2003s1_reset,
	.init = cv2003s1_init,
	/*.ioctl = cv2003s1_ops_ioctl,*/
	.g_register = cv2003s1_g_register,
	.s_register = cv2003s1_s_register,
};

static struct tx_isp_subdev_video_ops cv2003s1_video_ops = {
	.s_stream = cv2003s1_s_stream,
};

static struct tx_isp_subdev_sensor_ops	cv2003s1_sensor_ops = {
	.ioctl	= cv2003s1_sensor_ops_ioctl,
        .fsync = cv2003s1_fsync,
};

static struct tx_isp_subdev_ops cv2003s1_ops = {
	.core = &cv2003s1_core_ops,
	.video = &cv2003s1_video_ops,
	.sensor = &cv2003s1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "cv2003s1",
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


static int cv2003s1_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

    cv2003s1_attr.fsync_attr.mode = fsync_mode;
    cv2003s1_attr.max_integration_time_native = cv2003s1_attr.total_height - 7;
    cv2003s1_attr.integration_time_limit = cv2003s1_attr.total_height - 7;
    cv2003s1_attr.max_integration_time = cv2003s1_attr.total_height - 7;
    sensor_mclk_config(sensor, 24000000);
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &cv2003s1_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &cv2003s1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->cv2003s1\n");

	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int cv2003s1_remove(struct i2c_client *client)
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

static const struct i2c_device_id cv2003s1_id[] = {
	{ "cv2003s1", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cv2003s1_id);

static struct i2c_driver cv2003s1_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "cv2003s1",
	},
	.probe		= cv2003s1_probe,
	.remove		= cv2003s1_remove,
	.id_table	= cv2003s1_id,
};

static __init int init_cv2003s1(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init cv2003s1 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&cv2003s1_driver);
}

static __exit void exit_cv2003s1(void)
{
	private_i2c_del_driver(&cv2003s1_driver);
}

module_init(init_cv2003s1);
module_exit(exit_cv2003s1);

MODULE_DESCRIPTION("A low-level driver for SmartSens cv2003s1 sensors");
MODULE_LICENSE("GPL");
