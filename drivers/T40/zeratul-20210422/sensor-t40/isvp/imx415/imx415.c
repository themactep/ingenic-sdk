/*
 * imx415.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#define IMX415_CHIP_ID_H	(0x2e)
#define IMX415_CHIP_ID_L	(0x29)
#define IMX415_REG_END		0xffff
#define IMX415_REG_DELAY	0xfffe
#define IMX415_SUPPORT_SCLK_8M_FPS_15 (120000000)
#define SENSOR_OUTPUT_MAX_FPS 15
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION	"H20201229a"

static int reset_gpio = GPIO_PC(27);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static unsigned int expo_val = 0x031f0320;

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
struct again_lut imx415_again_lut[] = {
	{0x320, 0},
	{0x321, 2886},
	{0x322, 5776},
	{0x323, 8494},
	{0x324, 11136},
	{0x325, 13706},
	{0x326, 16287},
	{0x327, 18723},
	{0x328, 21097},
	{0x329, 23413},
	{0x32a, 25746},
	{0x32b, 27952},
	{0x32c, 30108},
	{0x32d, 32216},
	{0x32e, 34344},
	{0x32f, 36361},
	{0x330, 38335},
	{0x331, 40269},
	{0x332, 42225},
	{0x333, 44082},
	{0x334, 45903},
	{0x335, 47689},
	{0x336, 49499},
	{0x337, 51220},
	{0x338, 52910},
	{0x339, 54570},
	{0x33a, 56253},
	{0x33b, 57856},
	{0x33c, 59433},
	{0x33d, 60983},
	{0x33e, 62557},
	{0x33f, 64058},
	{0x720, 65535},
	{0x721, 68467},
	{0x722, 71266},
	{0x723, 74029},
	{0x724, 76671},
	{0x725, 79281},
	{0x726, 81782},
	{0x727, 84258},
	{0x728, 86632},
	{0x729, 88985},
	{0x72a, 91245},
	{0x72b, 93487},
	{0x72c, 95643},
	{0x72d, 97785},
	{0x72e, 99846},
	{0x72f, 101896},
	{0x730, 103870},
	{0x731, 105835},
	{0x732, 107730},
	{0x733, 109617},
	{0x734, 111438},
	{0x735, 113253},
	{0x736, 115006},
	{0x737, 116755},
	{0x738, 118445},
	{0x739, 120131},
	{0x73a, 121762},
	{0x73b, 123391},
	{0x73c, 124968},
	{0x73d, 126543},
	{0x73e, 128068},
	{0x73f, 129593},
	{0xf20, 131070},
	{0xf21, 133979},
	{0xf22, 136801},
	{0xf23, 139542},
	{0xf24, 142206},
	{0xf25, 144796},
	{0xf26, 147317},
	{0xf27, 149773},
	{0xf28, 152167},
	{0xf29, 154502},
	{0xf2a, 156780},
	{0xf2b, 159005},
	{0xf2c, 161178},
	{0xf2d, 163303},
	{0xf2e, 165381},
	{0xf2f, 167414},
	{0xf30, 169405},
	{0xf31, 171355},
	{0xf32, 173265},
	{0xf33, 175137},
	{0xf34, 176973},
	{0xf35, 178774},
	{0xf36, 180541},
	{0xf37, 182276},
	{0xf38, 183980},
	{0xf39, 185653},
	{0xf3a, 187297},
	{0xf3b, 188914},
	{0xf3c, 190503},
	{0xf3d, 192065},
	{0xf3e, 193603},
	{0xf3f, 195116},
	{0x1f20, 196605},
	{0x1f21, 199514},
	{0x1f22, 202336},
	{0x1f23, 205077},
	{0x1f24, 207741},
	{0x1f25, 210331},
	{0x1f26, 212852},
	{0x1f27, 215308},
	{0x1f28, 217702},
	{0x1f29, 220037},
	{0x1f2a, 222315},
	{0x1f2b, 224540},
	{0x1f2c, 226713},
	{0x1f2d, 228838},
	{0x1f2e, 230916},
	{0x1f2f, 232949},
	{0x1f30, 234940},
	{0x1f31, 236890},
	{0x1f32, 238800},
	{0x1f33, 240672},
	{0x1f34, 242508},
	{0x1f35, 244309},
	{0x1f36, 246076},
	{0x1f37, 247811},
	{0x1f38, 249515},
	{0x1f39, 251188},
	{0x1f3a, 252832},
	{0x1f3b, 254449},
	{0x1f3c, 256038},
	{0x1f3d, 257600},
	{0x1f3e, 259138},
	{0x1f3f, 260651}
};

struct tx_isp_sensor_attribute imx415_attr;

unsigned int imx415_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
}

unsigned int imx415_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute imx415_attr={
	.name = "imx415",
	.chip_id = 0x2e29,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 600,
		.lans = 4,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 3840,
		.image_theight = 2160,
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
	.max_again = 589824,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 2160,
	.integration_time_limit = 2662,
	.total_width = 0x820 * 2, //2080
	.total_height = 2160,
	.max_integration_time = 2160,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = imx415_alloc_again,
	.sensor_ctrl.alloc_dgain = imx415_alloc_dgain,
};

static struct regval_list imx415_init_regs_3840_2160_15fps_mipi[] = {
	{0x3000, 0x01},//standy
	{0x3001, 0x01},
	{0x3002, 0x01},//Master stop
	{0x3005, 0x01},//ADBIT 01:12bit
	{0x3007, 0x40},//WINMODE[6:4]
	{0x3009, 0x01},//FRSEL[1:0]
	{0x300a, 0xf0},
	{0x300c, 0x11},//WDMODE[0] WDSEL[5:4]
	{0x3010, 0x61},//FPGC gain for each gain
	{0x30f0, 0x64},//FPGC1 gain for each gain
	{0x3011, 0x02},
	{0x3018, 0x6E},//Vmax FSC=vmax*2
	{0x3019, 0x05},//0x486
	{0x301c, 0x58},//Hmax
	{0x301d, 0x08},
	{0x3020, 0x02},//SHS1 S
	{0x3021, 0x00},
	{0x3024, 0x73},//SHS2 L
	{0x3025, 0x04},
	{0x3028, 0x00},//SHS3
	{0x3029, 0x00},
	{0x302a, 0x00},
	{0x3030, 0x65},//RHS1
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x3034, 0x00},//RHS2
	{0x3035, 0x00},
	{0x3036, 0x00},
	{0x303c, 0x04},//WINPV[7:0]
	{0x303d, 0x00},//WINPV[2:0]
	{0x303e, 0x41},//WINWV[7:0]
	{0x303f, 0x04},//WINWV[2:0]
	{0x3045, 0x05},
	{0x3046, 0x01},//ODBIT[1:0]
	{0x304b, 0x0a},
	{0x305c, 0x18},
	{0x305d, 0x03},
	{0x305e, 0x20},
	{0x305f, 0x01},
	{0x309e, 0x4a},
	{0x309f, 0x4a},
	{0x30d2, 0x19},
	{0x30d7, 0x03},
	{0x3106, 0x11},
	{0x3129, 0x00},
	{0x313b, 0x61},
	{0x315e, 0x1a},
	{0x3164, 0x1a},
	{0x317c, 0x00},
	{0x31ec, 0x0e},
	{0x3204, 0x4a},
	{0x3209, 0xf0},
	{0x320a, 0x22},
	{0x3344, 0x38},
	{0x3405, 0x00},
	{0x3407, 0x01},
	{0x3414, 0x00},//OPB_SIZE_V[5:0]
	{0x3415, 0x00},//NULL0_SIZE_V[5:0]
	{0x3418, 0x7a},//Y_OUT_SIZE[7:0]
	{0x3419, 0x09},//Y_OUT_SIZE[4:0]
	{0x3441, 0x0c},
	{0x3442, 0x0c},
	{0x3443, 0x01},
	{0x3444, 0x20},
	{0x3445, 0x25},
	{0x3446, 0x77},
	{0x3447, 0x00},
	{0x3448, 0x67},
	{0x3449, 0x00},
	{0x344a, 0x47},
	{0x344b, 0x00},
	{0x344c, 0x37},
	{0x344d, 0x00},
	{0x344e, 0x3f},
	{0x344f, 0x00},
	{0x3450, 0xff},
	{0x3451, 0x00},
	{0x3452, 0x3f},
	{0x3453, 0x00},
	{0x3454, 0x37},
	{0x3455, 0x00},
	{0x346a, 0x9c},//EBD
	{0x346b, 0x07},
	{0x3472, 0xa0},
	{0x3473, 0x07},
	{0x347b, 0x23},
	{0x3480, 0x49},
	{0x3001, 0x00},//standy cancel
	{0x3002, 0x00},//Master start
	{0x3000, 0x01},//standy cancel
	{IMX415_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting imx415_win_sizes[] = {
	/* 3840*2160 */
	{
		.width		= 3840,
		.height		= 2160,
		.fps		= 15 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= imx415_init_regs_3840_2160_15fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &imx415_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list imx415_stream_on_mipi[] = {
	{0x0100, 0x01},
	{IMX415_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list imx415_stream_off_mipi[] = {
	{0x0100, 0x00},
	{IMX415_REG_END, 0x00},	/* END MARKER */
};

int imx415_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
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

int imx415_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
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
static int imx415_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != IMX415_REG_END) {
		if (vals->reg_num == IMX415_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx415_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int imx415_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != IMX415_REG_END) {
		if (vals->reg_num == IMX415_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx415_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int imx415_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int imx415_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = imx415_read(sd, 0x3b00, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX415_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = imx415_read(sd, 0x3b06, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX415_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int imx415_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	if (value != expo_val) {
		ret += imx415_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
		ret += imx415_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
		ret += imx415_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
		ret = imx415_write(sd, 0x3e09, (unsigned char)(again & 0xff));
		ret += imx415_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));

		ret += imx415_write(sd,0x3812,0x00);
		if (again < 0x720) {
			ret += imx415_write(sd,0x3301,0x1c);
			ret += imx415_write(sd,0x3630,0x30);
			ret += imx415_write(sd,0x3633,0x23);
			ret += imx415_write(sd,0x3622,0xf6);
			ret += imx415_write(sd,0x363a,0x83);
		}else if (again < 0xf20){
			ret += imx415_write(sd,0x3301,0x26);
			ret += imx415_write(sd,0x3630,0x23);
			ret += imx415_write(sd,0x3633,0x33);
			ret += imx415_write(sd,0x3622,0xf6);
			ret += imx415_write(sd,0x363a,0x87);
		}else if(again < 0x1f20){
			ret += imx415_write(sd,0x3301,0x2c);
			ret += imx415_write(sd,0x3630,0x24);
			ret += imx415_write(sd,0x3633,0x43);
			ret += imx415_write(sd,0x3622,0xf6);
			ret += imx415_write(sd,0x363a,0x9f);
		}else if(again < 0x1f3f){
			ret += imx415_write(sd,0x3301,0x38);
			ret += imx415_write(sd,0x3630,0x28);
			ret += imx415_write(sd,0x3633,0x43);
			ret += imx415_write(sd,0x3622,0xf6);
			ret += imx415_write(sd,0x363a,0x9f);
		}else {
			ret += imx415_write(sd,0x3301,0x44);
			ret += imx415_write(sd,0x3630,0x19);
			ret += imx415_write(sd,0x3633,0x55);
			ret += imx415_write(sd,0x3622,0x16);
			ret += imx415_write(sd,0x363a,0x9f);
		}
		ret += imx415_write(sd,0x3812,0x30);
		if (ret < 0)
			return ret;
	}
	expo_val = value;

	return 0;
}

#if 0
static int imx415_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret = imx415_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += imx415_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += imx415_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int imx415_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += imx415_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += imx415_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int imx415_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx415_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx415_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = imx415_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int imx415_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;

	if (init->enable) {
		ret = imx415_write_array(sd, imx415_stream_on_mipi);
		ISP_WARNING("imx415 stream on\n");

	}
	else {
		ret = imx415_write_array(sd, imx415_stream_off_mipi);
		ISP_WARNING("imx415 stream off\n");
	}

	return ret;
}

static int imx415_set_fps(struct tx_isp_subdev *sd, int fps)
{
	return 0;
}

static int imx415_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int imx415_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"imx415_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(100);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"imx415_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = imx415_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an imx415 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("imx415 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "imx415", sizeof("imx415"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int imx415_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_initarg *init = arg;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = imx415_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
		//	ret = imx415_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
		//	ret = imx415_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = imx415_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = imx415_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = imx415_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = imx415_write_array(sd, imx415_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = imx415_write_array(sd, imx415_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = imx415_set_fps(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int imx415_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = imx415_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int imx415_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	imx415_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops imx415_core_ops = {
	.g_chip_ident = imx415_g_chip_ident,
	.reset = imx415_reset,
	.init = imx415_init,
	.g_register = imx415_g_register,
	.s_register = imx415_s_register,
};

static struct tx_isp_subdev_video_ops imx415_video_ops = {
	.s_stream = imx415_s_stream,
};

static struct tx_isp_subdev_sensor_ops	imx415_sensor_ops = {
	.ioctl	= imx415_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops imx415_ops = {
	.core = &imx415_core_ops,
	.video = &imx415_video_ops,
	.sensor = &imx415_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "imx415",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int imx415_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	int ret;
	unsigned long rate;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sensor->mclk = private_devm_clk_get(&client->dev, "div_cim1");

	set_sensor_mclk_function(1);
	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	imx415_attr.expo_fs = 1;
	sensor->video.attr = &imx415_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx415_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->imx415\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int imx415_remove(struct i2c_client *client)
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

static const struct i2c_device_id imx415_id[] = {
	{ "imx415", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx415_id);

static struct i2c_driver imx415_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "imx415",
	},
	.probe		= imx415_probe,
	.remove		= imx415_remove,
	.id_table	= imx415_id,
};

static __init int init_imx415(void)
{
	return private_i2c_add_driver(&imx415_driver);
}

static __exit void exit_imx415(void)
{
	private_i2c_del_driver(&imx415_driver);
}

module_init(init_imx415);
module_exit(exit_imx415);

MODULE_DESCRIPTION("A low-level driver for Smartsens imx415 sensors");
MODULE_LICENSE("GPL");
