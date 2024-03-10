/*
 * sc4238.c
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
#include <txx-funcs.h>

#define SC4238_CHIP_ID_H	(0x42)
#define SC4238_CHIP_ID_L	(0x35)
#define SC4238_REG_END		0xffff
#define SC4238_REG_DELAY	0xfffe
#define SC4238_SUPPORT_25FPS_SCLK (145872000)
#define SC4238_SUPPORT_15FPS_SCLK (67478400)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20190906a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

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

struct again_lut sc4238_again_lut[] = {
	{0x340, 0},
	{0x341, 1500},
	{0x342, 2886},
	{0x343, 4342},
	{0x344, 5776},
	{0x345, 7101},
	{0x346, 8494},
	{0x347, 9781},
	{0x348, 11136},
	{0x349, 12471},
	{0x34a, 13706},
	{0x34b, 15005},
	{0x34c, 16287},
	{0x34d, 17474},
	{0x34e, 18723},
	{0x34f, 19879},
	{0x350, 21097},
	{0x351, 22300},
	{0x352, 23413},
	{0x353, 24587},
	{0x354, 25746},
	{0x355, 26820},
	{0x356, 27952},
	{0x357, 29002},
	{0x358, 30108},
	{0x359, 31202},
	{0x35a, 32216},
	{0x35b, 33286},
	{0x35c, 34344},
	{0x35d, 35325},
	{0x35e, 36361},
	{0x35f, 37321},
	{0x360, 38335},
	{0x361, 39338},
	{0x362, 40269},
	{0x363, 41252},
	{0x364, 42225},
	{0x365, 43128},
	{0x366, 44082},
	{0x367, 44967},
	{0x368, 45903},
	{0x369, 46829},
	{0x36a, 47689},
	{0x36b, 48599},
	{0x36c, 49499},
	{0x36d, 50336},
	{0x36e, 51220},
	{0x36f, 52041},
	{0x370, 52910},
	{0x371, 53770},
	{0x372, 54570},
	{0x373, 55415},
	{0x374, 56253},
	{0x375, 57032},
	{0x376, 57856},
	{0x377, 58622},
	{0x378, 59433},
	{0x379, 60236},
	{0x37a, 60983},
	{0x37b, 61773},
	{0x37c, 62557},
	{0x37d, 63286},
	{0x37e, 64058},
	{0x37f, 64775},
	{0x740, 65535},
	{0x741, 66989},
	{0x742, 68467},
	{0x743, 69877},
	{0x744, 71266},
	{0x745, 72636},
	{0x746, 74029},
	{0x747, 75359},
	{0x748, 76671},
	{0x749, 77964},
	{0x74a, 79281},
	{0x74b, 80540},
	{0x74c, 81782},
	{0x74d, 83009},
	{0x74e, 84258},
	{0x74f, 85452},
	{0x750, 86632},
	{0x751, 87797},
	{0x752, 88985},
	{0x753, 90122},
	{0x754, 91245},
	{0x755, 92355},
	{0x756, 93487},
	{0x757, 94571},
	{0x758, 95643},
	{0x759, 96703},
	{0x75a, 97785},
	{0x75b, 98821},
	{0x75c, 99846},
	{0x75d, 100860},
	{0x75e, 101896},
	{0x75f, 102888},
	{0x760, 103870},
	{0x761, 104842},
	{0x762, 105835},
	{0x763, 106787},
	{0x764, 107730},
	{0x765, 108663},
	{0x766, 109617},
	{0x767, 110532},
	{0x768, 111438},
	{0x769, 112335},
	{0x76a, 113253},
	{0x76b, 114134},
	{0x76c, 115006},
	{0x76d, 115871},
	{0x76e, 116755},
	{0x76f, 117603},
	{0x770, 118445},
	{0x771, 119278},
	{0x772, 120131},
	{0x773, 120950},
	{0x774, 121762},
	{0x775, 122567},
	{0x776, 123391},
	{0x777, 124183},
	{0x778, 124968},
	{0x779, 125746},
	{0x77a, 126543},
	{0x77b, 127308},
	{0x77c, 128068},
	{0x77d, 128821},
	{0x77e, 129593},
	{0x77f, 130334},
	{0xf40, 131070},
	{0xf41, 132547},
	{0xf42, 133979},
	{0xf43, 135412},
	{0xf44, 136801},
	{0xf45, 138193},
	{0xf46, 139542},
	{0xf47, 140894},
	{0xf48, 142206},
	{0xf49, 143520},
	{0xf4a, 144796},
	{0xf4b, 146075},
	{0xf4c, 147317},
	{0xf4d, 148563},
	{0xf4e, 149773},
	{0xf4f, 150987},
	{0xf50, 152167},
	{0xf51, 153351},
	{0xf52, 154502},
	{0xf53, 155657},
	{0xf54, 156780},
	{0xf55, 157908},
	{0xf56, 159005},
	{0xf57, 160106},
	{0xf58, 161178},
	{0xf59, 162255},
	{0xf5a, 163303},
	{0xf5b, 164356},
	{0xf5c, 165381},
	{0xf5d, 166411},
	{0xf5e, 167414},
	{0xf5f, 168423},
	{0xf60, 169405},
	{0xf61, 170393},
	{0xf62, 171355},
	{0xf63, 172322},
	{0xf64, 173265},
	{0xf65, 174213},
	{0xf66, 175137},
	{0xf67, 176067},
	{0xf68, 176973},
	{0xf69, 177885},
	{0xf6a, 178774},
	{0xf6b, 179669},
	{0xf6c, 180541},
	{0xf6d, 181419},
	{0xf6e, 182276},
	{0xf6f, 183138},
	{0xf70, 183980},
	{0xf71, 184827},
	{0xf72, 185653},
	{0xf73, 186485},
	{0xf74, 187297},
	{0xf75, 188115},
	{0xf76, 188914},
	{0xf77, 189718},
	{0xf78, 190503},
	{0xf79, 191293},
	{0xf7a, 192065},
	{0xf7b, 192843},
	{0xf7c, 193603},
	{0xf7d, 194368},
	{0xf7e, 195116},
	{0xf7f, 195869},
	{0x1f40, 196605},
	{0x1f41, 198070},
	{0x1f42, 199514},
	{0x1f43, 200936},
	{0x1f44, 202336},
	{0x1f45, 203717},
	{0x1f46, 205077},
	{0x1f47, 206418},
	{0x1f48, 207741},
	{0x1f49, 209045},
	{0x1f4a, 210331},
	{0x1f4b, 211600},
	{0x1f4c, 212852},
	{0x1f4d, 214088},
	{0x1f4e, 215308},
	{0x1f4f, 216513},
	{0x1f50, 217702},
	{0x1f51, 218877},
	{0x1f52, 220037},
	{0x1f53, 221183},
	{0x1f54, 222315},
	{0x1f55, 223434},
	{0x1f56, 224540},
	{0x1f57, 225633},
	{0x1f58, 226713},
	{0x1f59, 227782},
	{0x1f5a, 228838},
	{0x1f5b, 229883},
	{0x1f5c, 230916},
	{0x1f5d, 231938},
	{0x1f5e, 232949},
	{0x1f5f, 233950},
	{0x1f60, 234940},
	{0x1f61, 235920},
	{0x1f62, 236890},
	{0x1f63, 237849},
	{0x1f64, 238800},
	{0x1f65, 239740},
	{0x1f66, 240672},
	{0x1f67, 241594},
	{0x1f68, 242508},
	{0x1f69, 243413},
	{0x1f6a, 244309},
	{0x1f6b, 245197},
	{0x1f6c, 246076},
	{0x1f6d, 246947},
	{0x1f6e, 247811},
	{0x1f6f, 248667},
	{0x1f70, 249515},
	{0x1f71, 250355},
	{0x1f72, 251188},
	{0x1f73, 252014},
	{0x1f74, 252832},
	{0x1f75, 253644},
	{0x1f76, 254449},
	{0x1f77, 255246},
	{0x1f78, 256038},
	{0x1f79, 256822},
	{0x1f7a, 257600},
	{0x1f7b, 258372},
	{0x1f7c, 259138},
	{0x1f7d, 259897},
	{0x1f7e, 260651},
	{0x1f7f, 261398},

};

struct tx_isp_sensor_attribute sc4238_attr;

unsigned int sc4238_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc4238_again_lut;
	while(lut->gain <= sc4238_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc4238_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc4238_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc4238_attr={
	.name = "sc4238",
	.chip_id = 0x4235,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 800,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 2560,
		.image_theight = 1440,
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
	.max_again = 261398,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1562 - 4,
	.integration_time_limit = 1562 - 4,
	.total_width = 2880,
	.total_height = 1562,
	.max_integration_time = 1562 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc4238_alloc_again,
	.sensor_ctrl.alloc_dgain = sc4238_alloc_dgain,
};


static struct regval_list sc4238_init_regs_2560_1440_15fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x33},
	{0x301f, 0x18},
	{0x3031, 0x0a},
	{0x3037, 0x20},
	{0x3038, 0x22},
	{0x3106, 0x81},
	{0x3200, 0x00},
	{0x3201, 0x40},
	{0x3202, 0x00},
	{0x3203, 0x28},
	{0x3204, 0x0a},
	{0x3205, 0x47},
	{0x3206, 0x05},
	{0x3207, 0xcf},
	{0x3208, 0x0a},
	{0x3209, 0x00},
	{0x320a, 0x05},
	{0x320b, 0xa0},
	{0x320c, 0x05},
	{0x320d, 0xa0},
	{0x320e, 0x06},
	{0x320f, 0x1a},
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3253, 0x06},
	{0x3273, 0x01},
	{0x3301, 0x30},
	{0x3304, 0x30},
	{0x3306, 0x70},
	{0x3308, 0x10},
	{0x3309, 0x50},
	{0x330b, 0xe0},
	{0x330e, 0x14},
	{0x3314, 0x94},
	{0x331e, 0x29},
	{0x331f, 0x49},
	{0x3320, 0x09},
	{0x334c, 0x10},
	{0x3352, 0x02},
	{0x3356, 0x1f},
	{0x3363, 0x00},
	{0x3364, 0x1e},
	{0x3366, 0x92},
	{0x336d, 0x01},
	{0x337a, 0x08},
	{0x337b, 0x10},
	{0x337f, 0x2d},
	{0x3390, 0x08},
	{0x3391, 0x08},
	{0x3392, 0x08},
	{0x3393, 0x30},
	{0x3394, 0x30},
	{0x3395, 0x30},
	{0x3399, 0xff},
	{0x33a3, 0x0c},
	{0x33e0, 0xa0},
	{0x33e1, 0x08},
	{0x33e2, 0x00},
	{0x33e3, 0x10},
	{0x33e4, 0x10},
	{0x33e5, 0x00},
	{0x33e6, 0x10},
	{0x33e7, 0x10},
	{0x33e8, 0x00},
	{0x33e9, 0x10},
	{0x33ea, 0x16},
	{0x33eb, 0x00},
	{0x33ec, 0x10},
	{0x33ed, 0x18},
	{0x33ee, 0xa0},
	{0x33ef, 0x08},
	{0x33f4, 0x00},
	{0x33f5, 0x10},
	{0x33f6, 0x10},
	{0x33f7, 0x00},
	{0x33f8, 0x10},
	{0x33f9, 0x10},
	{0x33fa, 0x00},
	{0x33fb, 0x10},
	{0x33fc, 0x16},
	{0x33fd, 0x00},
	{0x33fe, 0x10},
	{0x33ff, 0x18},
	{0x360f, 0x05},
	{0x3622, 0xee},
	{0x3630, 0xa8},
	{0x3631, 0x80},
	{0x3633, 0x43},
	{0x3634, 0x34},
	{0x3635, 0x60},
	{0x3636, 0x20},
	{0x3637, 0x11},
	{0x3638, 0x2a},
	{0x363a, 0x80},
	{0x363b, 0x03},
	{0x3641, 0x00},
	{0x366e, 0x04},
	{0x3670, 0x48},
	{0x3671, 0xee},
	{0x3672, 0x0e},
	{0x3673, 0x0e},
	{0x367a, 0x08},
	{0x367b, 0x08},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x43},
	{0x3699, 0x80},
	{0x369a, 0x9f},
	{0x369b, 0x9f},
	{0x369c, 0x08},
	{0x369d, 0x08},
	{0x36a2, 0x08},
	{0x36a3, 0x08},
	{0x36ea, 0x31},
	{0x36eb, 0x1c},
	{0x36ec, 0x05},
	{0x36ed, 0x24},
	{0x36fa, 0x31},
	{0x36fb, 0x09},
	{0x36fc, 0x10},
	{0x36fd, 0x24},
	{0x3902, 0xc5},
	{0x3905, 0xd8},
	{0x3908, 0x11},
	{0x391b, 0x80},
	{0x391c, 0x0f},
	{0x3933, 0x28},
	{0x3934, 0x20},
	{0x3940, 0x6c},
	{0x3942, 0x08},
	{0x3943, 0x28},
	{0x3980, 0x00},
	{0x3981, 0x00},
	{0x3982, 0x00},
	{0x3983, 0x00},
	{0x3984, 0x00},
	{0x3985, 0x00},
	{0x3986, 0x00},
	{0x3987, 0x00},
	{0x3988, 0x00},
	{0x3989, 0x00},
	{0x398a, 0x00},
	{0x398b, 0x04},
	{0x398c, 0x00},
	{0x398d, 0x04},
	{0x398e, 0x00},
	{0x398f, 0x08},
	{0x3990, 0x00},
	{0x3991, 0x10},
	{0x3992, 0x03},
	{0x3993, 0xd8},
	{0x3994, 0x03},
	{0x3995, 0xe0},
	{0x3996, 0x03},
	{0x3997, 0xf0},
	{0x3998, 0x03},
	{0x3999, 0xf8},
	{0x399a, 0x00},
	{0x399b, 0x00},
	{0x399c, 0x00},
	{0x399d, 0x08},
	{0x399e, 0x00},
	{0x399f, 0x10},
	{0x39a0, 0x00},
	{0x39a1, 0x18},
	{0x39a2, 0x00},
	{0x39a3, 0x28},
	{0x39af, 0x58},
	{0x39b5, 0x30},
	{0x39b6, 0x00},
	{0x39b7, 0x34},
	{0x39b8, 0x00},
	{0x39b9, 0x00},
	{0x39ba, 0x34},
	{0x39bb, 0x00},
	{0x39bc, 0x00},
	{0x39bd, 0x00},
	{0x39be, 0x00},
	{0x39bf, 0x00},
	{0x39c0, 0x00},
	{0x39c1, 0x00},
	{0x39c5, 0x21},
	{0x39db, 0x20},
	{0x39dc, 0x00},
	{0x39de, 0x20},
	{0x39df, 0x00},
	{0x39e0, 0x00},
	{0x39e1, 0x00},
	{0x39e2, 0x00},
	{0x39e3, 0x00},
	{0x3e00, 0x00},
	{0x3e01, 0xc2},
	{0x3e02, 0xa0},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e14, 0xb1},
	{0x3e25, 0x03},
	{0x3e26, 0x40},
	{0x4501, 0xb4},
	{0x4509, 0x20},
	{0x4837, 0x3b},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x06},
	{0x5788, 0x06},
	{0x5789, 0x00},
	{0x578a, 0x06},
	{0x578b, 0x06},
	{0x578c, 0x00},
	{0x5790, 0x10},
	{0x5791, 0x10},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x10},
	{0x5795, 0x00},
	{0x57c4, 0x10},
	{0x57c5, 0x08},
	{0x57c7, 0x06},
	{0x57c8, 0x06},
	{0x57c9, 0x00},
	{0x57ca, 0x06},
	{0x57cb, 0x06},
	{0x57cc, 0x00},
	{0x57d0, 0x10},
	{0x57d1, 0x10},
	{0x57d2, 0x00},
	{0x57d3, 0x10},
	{0x57d4, 0x10},
	{0x57d5, 0x00},
	{0x36e9, 0x51},
	{0x36f9, 0x51},
	{0x0100, 0x00},
	{SC4238_REG_DELAY, 0x10},
	{SC4238_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc4238_init_regs_2560_1440_25fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x33},
	{0x301f, 0x20},
	{0x3031, 0x0a},
	{0x3037, 0x20},
	{0x3038, 0x22},
	{0x3106, 0x81},
	{0x3200, 0x00},
	{0x3201, 0x40},
	{0x3202, 0x00},
	{0x3203, 0x2c},
	{0x3204, 0x0a},
	{0x3205, 0x47},
	{0x3206, 0x05},
	{0x3207, 0xd3},
	{0x3208, 0x0a},
	{0x3209, 0x00},
	{0x320a, 0x05},
	{0x320b, 0xa0},
	{0x320c, 0x05},
	{0x320d, 0xa0},
	{0x320e, 0x07},
	{0x320f, 0xEA},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3253, 0x06},
	{0x3273, 0x01},
	{0x3301, 0x30},
	{0x3304, 0x30},
	{0x3306, 0x70},
	{0x3308, 0x10},
	{0x3309, 0x50},
	{0x330b, 0xe0},
	{0x330e, 0x14},
	{0x3314, 0x94},
	{0x331e, 0x29},
	{0x331f, 0x49},
	{0x3320, 0x09},
	{0x334c, 0x10},
	{0x3352, 0x02},
	{0x3356, 0x1f},
	{0x3363, 0x00},
	{0x3364, 0x1e},
	{0x3366, 0x92},
	{0x336d, 0x01},
	{0x337a, 0x08},
	{0x337b, 0x10},
	{0x337f, 0x2d},
	{0x3390, 0x08},
	{0x3391, 0x08},
	{0x3392, 0x08},
	{0x3393, 0x30},
	{0x3394, 0x30},
	{0x3395, 0x30},
	{0x3399, 0xff},
	{0x33a3, 0x0c},
	{0x33e0, 0xa0},
	{0x33e1, 0x08},
	{0x33e2, 0x00},
	{0x33e3, 0x10},
	{0x33e4, 0x10},
	{0x33e5, 0x00},
	{0x33e6, 0x10},
	{0x33e7, 0x10},
	{0x33e8, 0x00},
	{0x33e9, 0x10},
	{0x33ea, 0x16},
	{0x33eb, 0x00},
	{0x33ec, 0x10},
	{0x33ed, 0x18},
	{0x33ee, 0xa0},
	{0x33ef, 0x08},
	{0x33f4, 0x00},
	{0x33f5, 0x10},
	{0x33f6, 0x10},
	{0x33f7, 0x00},
	{0x33f8, 0x10},
	{0x33f9, 0x10},
	{0x33fa, 0x00},
	{0x33fb, 0x10},
	{0x33fc, 0x16},
	{0x33fd, 0x00},
	{0x33fe, 0x10},
	{0x33ff, 0x18},
	{0x360f, 0x05},
	{0x3622, 0xee},
	{0x3630, 0xa8},
	{0x3631, 0x80},
	{0x3633, 0x43},
	{0x3634, 0x34},
	{0x3635, 0x60},
	{0x3636, 0x20},
	{0x3637, 0x11},
	{0x3638, 0x2a},
	{0x363a, 0x80},
	{0x363b, 0x03},
	{0x3641, 0x00},
	{0x366e, 0x04},
	{0x3670, 0x48},
	{0x3671, 0xee},
	{0x3672, 0x0e},
	{0x3673, 0x0e},
	{0x367a, 0x08},
	{0x367b, 0x08},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x43},
	{0x3699, 0x80},
	{0x369a, 0x9f},
	{0x369b, 0x9f},
	{0x369c, 0x08},
	{0x369d, 0x08},
	{0x36a2, 0x08},
	{0x36a3, 0x08},
	{0x36ea, 0x31},
	{0x36eb, 0x0c},
	{0x36ec, 0x05},
	{0x36ed, 0x24},
	{0x36fa, 0x31},
	{0x36fb, 0x09},
	{0x36fc, 0x00},
	{0x36fd, 0x24},
	{0x3902, 0xc5},
	{0x3905, 0xd8},
	{0x3908, 0x11},
	{0x391b, 0x80},
	{0x391c, 0x0f},
	{0x3933, 0x28},
	{0x3934, 0x20},
	{0x3940, 0x6c},
	{0x3942, 0x08},
	{0x3943, 0x28},
	{0x3980, 0x00},
	{0x3981, 0x00},
	{0x3982, 0x00},
	{0x3983, 0x00},
	{0x3984, 0x00},
	{0x3985, 0x00},
	{0x3986, 0x00},
	{0x3987, 0x00},
	{0x3988, 0x00},
	{0x3989, 0x00},
	{0x398a, 0x00},
	{0x398b, 0x04},
	{0x398c, 0x00},
	{0x398d, 0x04},
	{0x398e, 0x00},
	{0x398f, 0x08},
	{0x3990, 0x00},
	{0x3991, 0x10},
	{0x3992, 0x03},
	{0x3993, 0xd8},
	{0x3994, 0x03},
	{0x3995, 0xe0},
	{0x3996, 0x03},
	{0x3997, 0xf0},
	{0x3998, 0x03},
	{0x3999, 0xf8},
	{0x399a, 0x00},
	{0x399b, 0x00},
	{0x399c, 0x00},
	{0x399d, 0x08},
	{0x399e, 0x00},
	{0x399f, 0x10},
	{0x39a0, 0x00},
	{0x39a1, 0x18},
	{0x39a2, 0x00},
	{0x39a3, 0x28},
	{0x39af, 0x58},
	{0x39b5, 0x30},
	{0x39b6, 0x00},
	{0x39b7, 0x34},
	{0x39b8, 0x00},
	{0x39b9, 0x00},
	{0x39ba, 0x34},
	{0x39bb, 0x00},
	{0x39bc, 0x00},
	{0x39bd, 0x00},
	{0x39be, 0x00},
	{0x39bf, 0x00},
	{0x39c0, 0x00},
	{0x39c1, 0x00},
	{0x39c5, 0x21},
	{0x39db, 0x20},
	{0x39dc, 0x00},
	{0x39de, 0x20},
	{0x39df, 0x00},
	{0x39e0, 0x00},
	{0x39e1, 0x00},
	{0x39e2, 0x00},
	{0x39e3, 0x00},
	{0x3e00, 0x00},
	{0x3e01, 0xc2},
	{0x3e02, 0xa0},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e14, 0xb1},
	{0x3e25, 0x03},
	{0x3e26, 0x40},
	{0x4501, 0xb4},
	{0x4509, 0x20},
	{0x4837, 0x3b},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x06},
	{0x5788, 0x06},
	{0x5789, 0x00},
	{0x578a, 0x06},
	{0x578b, 0x06},
	{0x578c, 0x00},
	{0x5790, 0x10},
	{0x5791, 0x10},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x10},
	{0x5795, 0x00},
	{0x57c4, 0x10},
	{0x57c5, 0x08},
	{0x57c7, 0x06},
	{0x57c8, 0x06},
	{0x57c9, 0x00},
	{0x57ca, 0x06},
	{0x57cb, 0x06},
	{0x57cc, 0x00},
	{0x57d0, 0x10},
	{0x57d1, 0x10},
	{0x57d2, 0x00},
	{0x57d3, 0x10},
	{0x57d4, 0x10},
	{0x57d5, 0x00},
	{0x36e9, 0x51},
	{0x36f9, 0x51},
	{0x0100, 0x01},
	{SC4238_REG_DELAY, 0x10},
	{SC4238_REG_END, 0x00},	/* END MARKER */
};
static struct tx_isp_sensor_win_setting sc4238_win_sizes[] = {
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc4238_init_regs_2560_1440_15fps_mipi,
	},
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc4238_init_regs_2560_1440_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sc4238_win_sizes[0];

static struct regval_list sc4238_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC4238_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc4238_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC4238_REG_END, 0x00},	/* END MARKER */
};

int sc4238_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc4238_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int sc4238_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC4238_REG_END) {
		if (vals->reg_num == SC4238_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc4238_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
static int sc4238_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC4238_REG_END) {
		if (vals->reg_num == SC4238_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc4238_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc4238_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc4238_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc4238_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC4238_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc4238_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC4238_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc4238_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret  = sc4238_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc4238_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc4238_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sc4238_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc4238_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc4238_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc4238_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc4238_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc4238_init(struct tx_isp_subdev *sd, int enable)
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

	ret = sc4238_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc4238_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sc4238_write_array(sd, sc4238_stream_on_mipi);
		ISP_WARNING("sc4238 stream on\n");

	}
	else {
		ret = sc4238_write_array(sd, sc4238_stream_off_mipi);
		ISP_WARNING("sc4238 stream off\n");
	}

	return ret;
}

static int sc4238_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return ret;
	}
	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		sclk = SC4238_SUPPORT_25FPS_SCLK;
		max_fps = SENSOR_OUTPUT_MAX_FPS;
		break;
	case TX_SENSOR_MAX_FPS_15:
		sclk = SC4238_SUPPORT_15FPS_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}


	ret = sc4238_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc4238_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc4238 read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp) << 1;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sc4238_write(sd,0x3812,0x00);
	ret += sc4238_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc4238_write(sd, 0x320e, (unsigned char)(vts >> 8));
	ret += sc4238_write(sd,0x3812,0x30);
	if (0 != ret) {
		ISP_ERROR("err: sc4238_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc4238_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc4238_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc4238_reset");
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
		ret = private_gpio_request(pwdn_gpio,"sc4238_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc4238_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc4238 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc4238 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc4238", sizeof("sc4238"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc4238_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = sc4238_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = sc4238_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc4238_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc4238_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc4238_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc4238_write_array(sd, sc4238_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc4238_write_array(sd, sc4238_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc4238_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sc4238_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc4238_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc4238_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	sc4238_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc4238_core_ops = {
	.g_chip_ident = sc4238_g_chip_ident,
	.reset = sc4238_reset,
	.init = sc4238_init,
	/*.ioctl = sc4238_ops_ioctl,*/
	.g_register = sc4238_g_register,
	.s_register = sc4238_s_register,
};

static struct tx_isp_subdev_video_ops sc4238_video_ops = {
	.s_stream = sc4238_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc4238_sensor_ops = {
	.ioctl	= sc4238_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc4238_ops = {
	.core = &sc4238_core_ops,
	.video = &sc4238_video_ops,
	.sensor = &sc4238_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc4238",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc4238_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_15:
		wsize = &sc4238_win_sizes[0];
		break;
	case TX_SENSOR_MAX_FPS_25:
		wsize = &sc4238_win_sizes[1];
		sc4238_attr.max_integration_time_native = 2026 - 4;
		sc4238_attr.integration_time_limit = 2026 - 4;
		sc4238_attr.total_width = 2880;
		sc4238_attr.total_height = 2026;
		sc4238_attr.max_integration_time = 2026 - 4;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sc4238_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc4238_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc4238\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc4238_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc4238_id[] = {
	{ "sc4238", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc4238_id);

static struct i2c_driver sc4238_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc4238",
	},
	.probe		= sc4238_probe,
	.remove		= sc4238_remove,
	.id_table	= sc4238_id,
};

static __init int init_sc4238(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc4238 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc4238_driver);
}

static __exit void exit_sc4238(void)
{
	private_i2c_del_driver(&sc4238_driver);
}

module_init(init_sc4238);
module_exit(exit_sc4238);

MODULE_DESCRIPTION("A low-level driver for Smartsens sc4238 sensors");
MODULE_LICENSE("GPL");
