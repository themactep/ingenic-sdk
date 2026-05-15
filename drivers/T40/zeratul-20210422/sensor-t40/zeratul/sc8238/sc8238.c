/*
 * sc8238.c
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

#define SC8238_CHIP_ID_H	(0x82)
#define SC8238_CHIP_ID_L	(0x35)
#define SC8238_REG_END		0xffff
#define SC8238_REG_DELAY	0xfffe
#define SC8238_SUPPORT_SCLK_8M_FPS_15 (120000000)
#define SENSOR_OUTPUT_MAX_FPS 15
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define SENSOR_VERSION	"H20201229a"

#define DUAL_CAMERA_MODE 0
#define MAIN_SENSOR 1
#define SECOND_SENSOR 0

#define SENSOR_WITHOUT_INIT 0

static int reset_gpio = GPIO_PC(27);
static int pwdn_gpio = -1;
static int sensor_max_fps = 15;
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
struct again_lut sc8238_again_lut[] = {
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

struct tx_isp_sensor_attribute sc8238_attr;

unsigned int sc8238_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc8238_again_lut;
	while(lut->gain <= sc8238_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc8238_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc8238_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc8238_attr={
	.name = "sc8238",
	.chip_id = 0x8235,
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
	.max_again = 260651,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_native = 3,
	.max_integration_time_native = 2160,
	.integration_time_limit = 2662,
	.total_width = 0x820 * 2, //2080
	.total_height = 2160,
	.max_integration_time = 2160,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc8238_alloc_again,
	.sensor_ctrl.alloc_dgain = sc8238_alloc_dgain,
};

static struct regval_list sc8238_init_regs_3840_2160_30fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x72},
	{0x3019, 0x00},
	{0x301f, 0x5c},
	{0x3031, 0x0a},
	{0x3037, 0x20},
	{0x3038, 0x44},
	{0x3200, 0x00},
	{0x3201, 0x0c},
	{0x3202, 0x00},
	{0x3203, 0x0c},
	{0x3204, 0x0f},
	{0x3205, 0x13},
	{0x3206, 0x08},
	{0x3207, 0x83},
	{0x3208, 0x0f},
	{0x3209, 0x00},
	{0x320a, 0x08},
	{0x320b, 0x70},
	{0x320c, 0x08},
	{0x320d, 0x20},
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3241, 0x00},
	{0x3243, 0x03},
	{0x3248, 0x04},
	{0x3271, 0x1c},
	{0x3273, 0x1f},
	{0x3301, 0x1c},
	{0x3306, 0xa8},
	{0x3308, 0x20},
	{0x3309, 0x68},
	{0x330b, 0x48},
	{0x330d, 0x28},
	{0x330e, 0x58},
	{0x3314, 0x94},
	{0x331f, 0x59},
	{0x3332, 0x24},
	{0x334c, 0x10},
	{0x3350, 0x24},
	{0x3358, 0x24},
	{0x335c, 0x24},
	{0x335d, 0x60},
	{0x3364, 0x16},
	{0x3366, 0x92},
	{0x3367, 0x08},
	{0x3368, 0x07},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x336c, 0xc2},
	{0x337f, 0x33},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x1c},
	{0x3394, 0x28},
	{0x3395, 0x60},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x1c},
	{0x339a, 0x1c},
	{0x339b, 0x28},
	{0x339c, 0x60},
	{0x339e, 0x24},
	{0x33aa, 0x24},
	{0x33af, 0x48},
	{0x33e1, 0x08},
	{0x33e2, 0x18},
	{0x33e3, 0x10},
	{0x33e4, 0x0c},
	{0x33e5, 0x10},
	{0x33e6, 0x06},
	{0x33e7, 0x02},
	{0x33e8, 0x18},
	{0x33e9, 0x10},
	{0x33ea, 0x0c},
	{0x33eb, 0x10},
	{0x33ec, 0x04},
	{0x33ed, 0x02},
	{0x33ee, 0xa0},
	{0x33ef, 0x08},
	{0x33f4, 0x18},
	{0x33f5, 0x10},
	{0x33f6, 0x0c},
	{0x33f7, 0x10},
	{0x33f8, 0x06},
	{0x33f9, 0x02},
	{0x33fa, 0x18},
	{0x33fb, 0x10},
	{0x33fc, 0x0c},
	{0x33fd, 0x10},
	{0x33fe, 0x04},
	{0x33ff, 0x02},
	{0x360f, 0x01},
	{0x3622, 0xf7},
	{0x3624, 0x45},
	{0x3628, 0x83},
	{0x3630, 0x80},
	{0x3631, 0x80},
	{0x3632, 0xa8},
	{0x3633, 0x53},
	{0x3635, 0x02},
	{0x3637, 0x52},
	{0x3638, 0x0a},
	{0x363a, 0x88},
	{0x363b, 0x06},
	{0x363d, 0x01},
	{0x363e, 0x00},
	{0x3641, 0x00},
	{0x3670, 0x4a},
	{0x3671, 0xf7},
	{0x3672, 0xf7},
	{0x3673, 0x17},
	{0x3674, 0x80},
	{0x3675, 0x85},
	{0x3676, 0xa5},
	{0x367a, 0x48},
	{0x367b, 0x78},
	{0x367c, 0x48},
	{0x367d, 0x78},
	{0x3690, 0x53},
	{0x3691, 0x63},
	{0x3692, 0x54},
	{0x3699, 0x88},
	{0x369a, 0x9f},
	{0x369b, 0x9f},
	{0x369c, 0x48},
	{0x369d, 0x78},
	{0x36a2, 0x48},
	{0x36a3, 0x78},
	{0x36bb, 0x48},
	{0x36bc, 0x78},
	{0x36c9, 0x05},
	{0x36ca, 0x05},
	{0x36cb, 0x05},
	{0x36cc, 0x00},
	{0x36cd, 0x10},
	{0x36ce, 0x1a},
	{0x36d0, 0x30},
	{0x36d1, 0x48},
	{0x36d2, 0x78},
	{0x36ea, 0x73},
	{0x36eb, 0x04},
	{0x36ec, 0x05},
	{0x36ed, 0x14},
	{0x36fa, 0x73},
	{0x36fb, 0x11},
	{0x36fc, 0x00},
	{0x36fd, 0x07},
	{0x3901, 0x00},
	{0x3902, 0xc5},
	{0x3904, 0x18},
	{0x3905, 0xd8},
	{0x394c, 0x0f},
	{0x394d, 0x20},
	{0x394e, 0x08},
	{0x394f, 0x90},
	{0x3980, 0x71},
	{0x3981, 0x70},
	{0x3982, 0x00},
	{0x3983, 0x00},
	{0x3984, 0x20},
	{0x3987, 0x0b},
	{0x3990, 0x03},
	{0x3991, 0xfd},
	{0x3992, 0x03},
	{0x3993, 0xfc},
	{0x3994, 0x00},
	{0x3995, 0x00},
	{0x3996, 0x00},
	{0x3997, 0x05},
	{0x3998, 0x00},
	{0x3999, 0x09},
	{0x399a, 0x00},
	{0x399b, 0x12},
	{0x399c, 0x00},
	{0x399d, 0x12},
	{0x399e, 0x00},
	{0x399f, 0x18},
	{0x39a0, 0x00},
	{0x39a1, 0x14},
	{0x39a2, 0x03},
	{0x39a3, 0xe3},
	{0x39a4, 0x03},
	{0x39a5, 0xf2},
	{0x39a6, 0x03},
	{0x39a7, 0xf6},
	{0x39a8, 0x03},
	{0x39a9, 0xfa},
	{0x39aa, 0x03},
	{0x39ab, 0xff},
	{0x39ac, 0x00},
	{0x39ad, 0x06},
	{0x39ae, 0x00},
	{0x39af, 0x09},
	{0x39b0, 0x00},
	{0x39b1, 0x12},
	{0x39b2, 0x00},
	{0x39b3, 0x22},
	{0x39b4, 0x0c},
	{0x39b5, 0x1c},
	{0x39b6, 0x38},
	{0x39b7, 0x5b},
	{0x39b8, 0x50},
	{0x39b9, 0x38},
	{0x39ba, 0x20},
	{0x39bb, 0x10},
	{0x39bc, 0x0c},
	{0x39bd, 0x16},
	{0x39be, 0x21},
	{0x39bf, 0x36},
	{0x39c0, 0x3b},
	{0x39c1, 0x2a},
	{0x39c2, 0x16},
	{0x39c3, 0x0c},
	{0x39c5, 0x30},
	{0x39c6, 0x07},
	{0x39c7, 0xf8},
	{0x39c9, 0x07},
	{0x39ca, 0xf8},
	{0x39cc, 0x00},
	{0x39cd, 0x1b},
	{0x39ce, 0x00},
	{0x39cf, 0x00},
	{0x39d0, 0x1b},
	{0x39d1, 0x00},
	{0x39e2, 0x15},
	{0x39e3, 0x87},
	{0x39e4, 0x12},
	{0x39e5, 0xb7},
	{0x39e6, 0x00},
	{0x39e7, 0x8c},
	{0x39e8, 0x01},
	{0x39e9, 0x31},
	{0x39ea, 0x01},
	{0x39eb, 0xd7},
	{0x39ec, 0x08},
	{0x39ed, 0x00},
	{0x3e00, 0x01},
	{0x3e01, 0x18},
	{0x3e02, 0xa0},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e0e, 0x09},
	{0x3e14, 0x31},
	{0x3e16, 0x00},
	{0x3e17, 0xac},
	{0x3e18, 0x00},
	{0x3e19, 0xac},
	{0x3e1b, 0x3a},
	{0x3e1e, 0x76},
	{0x3e25, 0x23},
	{0x3e26, 0x40},
	{0x4501, 0xa4},
	{0x4509, 0x10},
	{0x4837, 0x1c},
	{0x5799, 0x06},
	{0x57aa, 0x2f},
	{0x57ab, 0xff},
	{0x36e9, 0x51},
	{0x36f9, 0x35},
	{0x0100, 0x01},
	{SC8238_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc8238_init_regs_3840_2160_15fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x72},
	{0x3019, 0x00},
	{0x301f, 0x50},
	{0x3031, 0x0a},
	{0x3037, 0x20},
	{0x3038, 0x44},
	{0x3200, 0x00},
	{0x3201, 0x08},
	{0x3202, 0x00},
	{0x3203, 0x08},
	{0x3204, 0x0f},
	{0x3205, 0x17},
	{0x3206, 0x08},
	{0x3207, 0x87},
	{0x3208, 0x0f},
	{0x3209, 0x00},
	{0x320a, 0x08},
	{0x320b, 0x70},
	{0x320c, 0x08},
	{0x320d, 0x20},
	{0x3210, 0x00},
	{0x3211, 0x08},
	{0x3212, 0x00},
	{0x3213, 0x08},
	{0x3241, 0x00},
	{0x3243, 0x03},
	{0x3248, 0x04},
	{0x3271, 0x1c},
	{0x3273, 0x1f},
	{0x3301, 0x0c},
	{0x3306, 0x7c},
	{0x3308, 0x20},
	{0x3309, 0x68},
	{0x330b, 0x48},
	{0x330d, 0x28},
	{0x330e, 0x58},
	{0x3314, 0x94},
	{0x331f, 0x59},
	{0x3332, 0x24},
	{0x334c, 0x10},
	{0x3350, 0x24},
	{0x3358, 0x24},
	{0x335c, 0x24},
	{0x335d, 0x60},
	{0x335e, 0x02},
	{0x335f, 0x04},
	{0x3364, 0x16},
	{0x3366, 0x92},
	{0x3367, 0x08},
	{0x3368, 0x07},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x336c, 0xc2},
	{0x336d, 0x03},
	{0x337c, 0x06},
	{0x337d, 0x0a},
	{0x337f, 0x33},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x1c},
	{0x3394, 0x28},
	{0x3395, 0x60},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x0c},
	{0x339a, 0x1c},
	{0x339b, 0x28},
	{0x339c, 0x60},
	{0x339e, 0x24},
	{0x33a2, 0x08},
	{0x33aa, 0x24},
	{0x33af, 0x48},
	{0x33e1, 0x08},
	{0x33e2, 0x18},
	{0x33e3, 0x10},
	{0x33e4, 0x0c},
	{0x33e5, 0x10},
	{0x33e6, 0x06},
	{0x33e7, 0x02},
	{0x33e8, 0x18},
	{0x33e9, 0x10},
	{0x33ea, 0x0c},
	{0x33eb, 0x10},
	{0x33ec, 0x04},
	{0x33ed, 0x02},
	{0x33ee, 0xa0},
	{0x33ef, 0x08},
	{0x33f4, 0x18},
	{0x33f5, 0x10},
	{0x33f6, 0x0c},
	{0x33f7, 0x10},
	{0x33f8, 0x06},
	{0x33f9, 0x02},
	{0x33fa, 0x18},
	{0x33fb, 0x10},
	{0x33fc, 0x0c},
	{0x33fd, 0x10},
	{0x33fe, 0x04},
	{0x33ff, 0x02},
	{0x360f, 0x01},
	{0x3622, 0xf7},
	{0x3624, 0x45},
	{0x3628, 0x83},
	{0x3630, 0x80},
	{0x3631, 0x80},
	{0x3632, 0x98},
	{0x3633, 0x53},
	{0x3635, 0x02},
	{0x3637, 0x52},
	{0x3638, 0x0a},
	{0x363a, 0x88},
	{0x363b, 0x06},
	{0x363d, 0x01},
	{0x363e, 0x00},
	{0x3641, 0x00},
	{0x3670, 0x4a},
	{0x3671, 0xf7},
	{0x3672, 0xf7},
	{0x3673, 0x17},
	{0x3674, 0x80},
	{0x3675, 0x85},
	{0x3676, 0xb5},
	{0x367a, 0x48},
	{0x367b, 0x78},
	{0x367c, 0x48},
	{0x367d, 0x78},
	{0x3690, 0x33},
	{0x3691, 0x33},
	{0x3692, 0x53},
	{0x3699, 0x88},
	{0x369a, 0x9f},
	{0x369b, 0x9f},
	{0x369c, 0x48},
	{0x369d, 0x78},
	{0x36a2, 0x48},
	{0x36a3, 0x78},
	{0x36bb, 0x48},
	{0x36bc, 0x78},
	{0x36c9, 0x05},
	{0x36ca, 0x05},
	{0x36cb, 0x05},
	{0x36cc, 0x00},
	{0x36cd, 0x10},
	{0x36ce, 0x1a},
	{0x36d0, 0x30},
	{0x36d1, 0x48},
	{0x36d2, 0x78},
	{0x36ea, 0x33},
	{0x36eb, 0x04},
	{0x36ec, 0x15},
	{0x36ed, 0x34},
	{0x36fa, 0x33},
	{0x36fb, 0x13},
	{0x36fc, 0x10},
	{0x36fd, 0x37},
	{0x3901, 0x00},
	{0x3902, 0xc5},
	{0x3904, 0x18},
	{0x3905, 0xd8},
	{0x394c, 0x0f},
	{0x394d, 0x20},
	{0x394e, 0x08},
	{0x394f, 0x90},
	{0x3980, 0x71},
	{0x3981, 0x70},
	{0x3982, 0x00},
	{0x3983, 0x00},
	{0x3984, 0x20},
	{0x3987, 0x0b},
	{0x3990, 0x03},
	{0x3991, 0xfd},
	{0x3992, 0x03},
	{0x3993, 0xfc},
	{0x3994, 0x00},
	{0x3995, 0x00},
	{0x3996, 0x00},
	{0x3997, 0x05},
	{0x3998, 0x00},
	{0x3999, 0x09},
	{0x399a, 0x00},
	{0x399b, 0x12},
	{0x399c, 0x00},
	{0x399d, 0x12},
	{0x399e, 0x00},
	{0x399f, 0x18},
	{0x39a0, 0x00},
	{0x39a1, 0x14},
	{0x39a2, 0x03},
	{0x39a3, 0xe3},
	{0x39a4, 0x03},
	{0x39a5, 0xf2},
	{0x39a6, 0x03},
	{0x39a7, 0xf6},
	{0x39a8, 0x03},
	{0x39a9, 0xfa},
	{0x39aa, 0x03},
	{0x39ab, 0xff},
	{0x39ac, 0x00},
	{0x39ad, 0x06},
	{0x39ae, 0x00},
	{0x39af, 0x09},
	{0x39b0, 0x00},
	{0x39b1, 0x12},
	{0x39b2, 0x00},
	{0x39b3, 0x22},
	{0x39b4, 0x0c},
	{0x39b5, 0x1c},
	{0x39b6, 0x38},
	{0x39b7, 0x5b},
	{0x39b8, 0x50},
	{0x39b9, 0x38},
	{0x39ba, 0x20},
	{0x39bb, 0x10},
	{0x39bc, 0x0c},
	{0x39bd, 0x16},
	{0x39be, 0x21},
	{0x39bf, 0x36},
	{0x39c0, 0x3b},
	{0x39c1, 0x2a},
	{0x39c2, 0x16},
	{0x39c3, 0x0c},
	{0x39c5, 0x30},
	{0x39c6, 0x07},
	{0x39c7, 0xf8},
	{0x39c9, 0x07},
	{0x39ca, 0xf8},
	{0x39cc, 0x00},
	{0x39cd, 0x1b},
	{0x39ce, 0x00},
	{0x39cf, 0x00},
	{0x39d0, 0x1b},
	{0x39d1, 0x00},
	{0x39e2, 0x15},
	{0x39e3, 0x87},
	{0x39e4, 0x12},
	{0x39e5, 0xb7},
	{0x39e6, 0x00},
	{0x39e7, 0x8c},
	{0x39e8, 0x01},
	{0x39e9, 0x31},
	{0x39ea, 0x01},
	{0x39eb, 0xd7},
	{0x39ec, 0x08},
	{0x39ed, 0x00},
	{0x3e00, 0x01},
	{0x3e01, 0x18},
	{0x3e02, 0xa0},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e0e, 0x09},
	{0x3e14, 0x31},
	{0x3e16, 0x00},
	{0x3e17, 0xac},
	{0x3e18, 0x00},
	{0x3e19, 0xac},
	{0x3e1b, 0x3a},
	{0x3e1e, 0x76},
	{0x3e25, 0x23},
	{0x3e26, 0x40},
	{0x4501, 0xa4},
	{0x4509, 0x10},
	{0x4837, 0x39},
	{0x5799, 0x06},
	{0x57aa, 0x2f},
	{0x57ab, 0xff},
	{0x36e9, 0x51},
	{0x36f9, 0x39},
	{0x0100, 0x01},
	{SC8238_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc8238_win_sizes[] = {
	/* 3840*2160 */
	{
		.width		= 3840,
		.height		= 2160,
		.fps		= 15 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc8238_init_regs_3840_2160_15fps_mipi,
	},
	{
		.width		= 3840,
		.height		= 2160,
		.fps		= 30 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc8238_init_regs_3840_2160_30fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sc8238_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc8238_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC8238_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc8238_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC8238_REG_END, 0x00},	/* END MARKER */
};

int sc8238_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc8238_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc8238_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC8238_REG_END) {
		if (vals->reg_num == SC8238_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sc8238_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc8238_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC8238_REG_END) {
		if (vals->reg_num == SC8238_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sc8238_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc8238_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc8238_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc8238_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC8238_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc8238_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC8238_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc8238_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	if (value != expo_val) {
		ret += sc8238_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
		ret += sc8238_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
		ret += sc8238_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
		ret = sc8238_write(sd, 0x3e09, (unsigned char)(again & 0xff));
		ret += sc8238_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));

		ret += sc8238_write(sd,0x3812,0x00);
		if (again < 0x720) {
			ret += sc8238_write(sd,0x3301,0x1c);
			ret += sc8238_write(sd,0x3630,0x30);
			ret += sc8238_write(sd,0x3633,0x23);
			ret += sc8238_write(sd,0x3622,0xf6);
			ret += sc8238_write(sd,0x363a,0x83);
		}else if (again < 0xf20){
			ret += sc8238_write(sd,0x3301,0x26);
			ret += sc8238_write(sd,0x3630,0x23);
			ret += sc8238_write(sd,0x3633,0x33);
			ret += sc8238_write(sd,0x3622,0xf6);
			ret += sc8238_write(sd,0x363a,0x87);
		}else if(again < 0x1f20){
			ret += sc8238_write(sd,0x3301,0x2c);
			ret += sc8238_write(sd,0x3630,0x24);
			ret += sc8238_write(sd,0x3633,0x43);
			ret += sc8238_write(sd,0x3622,0xf6);
			ret += sc8238_write(sd,0x363a,0x9f);
		}else if(again < 0x1f3f){
			ret += sc8238_write(sd,0x3301,0x38);
			ret += sc8238_write(sd,0x3630,0x28);
			ret += sc8238_write(sd,0x3633,0x43);
			ret += sc8238_write(sd,0x3622,0xf6);
			ret += sc8238_write(sd,0x363a,0x9f);
		}else {
			ret += sc8238_write(sd,0x3301,0x44);
			ret += sc8238_write(sd,0x3630,0x19);
			ret += sc8238_write(sd,0x3633,0x55);
			ret += sc8238_write(sd,0x3622,0x16);
			ret += sc8238_write(sd,0x363a,0x9f);
		}
		ret += sc8238_write(sd,0x3812,0x30);
		if (ret < 0)
			return ret;
	}
	expo_val = value;

	return 0;
}

#if 0
static int sc8238_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret = sc8238_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sc8238_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc8238_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc8238_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc8238_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc8238_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sc8238_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc8238_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc8238_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
	sensor->video.state = TX_ISP_MODULE_INIT;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc8238_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_INIT){
#if SENSOR_WITHOUT_INIT
			ret = sc8238_write_array(sd, wsize->regs);
			if (ret)
				return ret;
#endif
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if(sensor->video.state == TX_ISP_MODULE_RUNNING){

			ret = sc8238_write_array(sd, sc8238_stream_on_mipi);
			ISP_WARNING("sc8238 stream on\n");
		}
	}
	else {
		ret = sc8238_write_array(sd, sc8238_stream_off_mipi);
		ISP_WARNING("sc8238 stream off\n");
	}

	return ret;
}

static int sc8238_set_fps(struct tx_isp_subdev *sd, int fps)
{
	return 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	sclk = SC8238_SUPPORT_SCLK_8M_FPS_15;

	ret += sc8238_read(sd, 0x320c, &val);
	hts = val << 8;
	ret += sc8238_read(sd, 0x320d, &val);
	hts = (hts | val) << 1;
	if (0 != ret) {
		ISP_ERROR("err: sc8238 read err\n");
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sc8238_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc8238_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc8238_write err\n");
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

static int sc8238_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned long rate;

	switch(info->default_boot){
	case 0:
		wsize = &sc8238_win_sizes[0];
		sc8238_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		break;
	case 1:
		wsize = &sc8238_win_sizes[1];
		sc8238_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc8238_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc8238_attr.mipi.index = 0;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->mclk){
	case TISP_SENSOR_MCLK0:
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
		set_sensor_mclk_function(1);
		break;
	case TISP_SENSOR_MCLK2:
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
		set_sensor_mclk_function(2);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
#if SENSOR_WITHOUT_INIT
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);
#endif

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	return 0;

err_get_mclk:
	return -1;
}

static int sc8238_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
#if SENSOR_WITHOUT_INIT
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc8238_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc8238_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc8238_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc8238 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#else
	ident = 0x8235;
#endif
	ISP_WARNING("sc8238 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc8238", sizeof("sc8238"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc8238_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc8238_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
		//	ret = sc8238_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
		//	ret = sc8238_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc8238_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc8238_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc8238_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc8238_write_array(sd, sc8238_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc8238_write_array(sd, sc8238_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc8238_set_fps(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int sc8238_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc8238_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc8238_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc8238_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc8238_core_ops = {
	.g_chip_ident = sc8238_g_chip_ident,
	.reset = sc8238_reset,
	.init = sc8238_init,
	.g_register = sc8238_g_register,
	.s_register = sc8238_s_register,
};

static struct tx_isp_subdev_video_ops sc8238_video_ops = {
	.s_stream = sc8238_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc8238_sensor_ops = {
	.ioctl	= sc8238_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc8238_ops = {
	.core = &sc8238_core_ops,
	.video = &sc8238_video_ops,
	.sensor = &sc8238_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc8238",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int sc8238_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
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

       	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sc8238_attr.expo_fs = 1;
	sensor->video.attr = &sc8238_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc8238_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc8238\n");

	return 0;
}

static int sc8238_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sc8238_id[] = {
	{ "sc8238", 0 },
	{ }
};

static struct i2c_driver sc8238_driver = {
	.driver = {
		.owner	= NULL,
		.name	= "sc8238",
	},
	.probe		= sc8238_probe,
	.remove		= sc8238_remove,
	.id_table	= sc8238_id,
};

/*dual mode*/
#if DUAL_CAMERA_MODE
#if MAIN_SENSOR
char * get_sensor_name(void)
{
	return "sc8238";
}

int get_sensor_i2c_addr(void)
{
	return 0x30;
}

int get_sensor_width(void)
{
	return sc8238_win_sizes->width;
}

int get_sensor_height(void)
{
	return sc8238_win_sizes->height;
}

int init_sensor0(void)
{
	return private_i2c_add_driver(&sc8238_driver);
}

void exit_sensor0(void)
{
	private_i2c_del_driver(&sc8238_driver);
}

#elif SECOND_SENSOR

char * get_sensor1_name(void)
{
	return "sc8238";
}

int get_sensor1_i2c_addr(void)
{
	return 0x30;
}

int get_sensor1_width(void)
{
	return sc8238_win_sizes->width;
}

int get_sensor1_height(void)
{
	return sc8238_win_sizes->height;
}


int init_sensor1(void)
{
	return private_i2c_add_driver(&sc8238_driver);
}

void exit_sensor1(void)
{
	return private_i2c_del_driver(&sc8238_driver);
}
#endif
#endif



/*single mode*/
#if !DUAL_CAMERA_MODE
char * get_sensor_name(void)
{
	return "sc8238";
}

int get_sensor_i2c_addr(void)
{
	return 0x30;
}

int get_sensor_width(void)
{
	return sc8238_win_sizes->width;
}

int get_sensor_height(void)
{
	return sc8238_win_sizes->height;
}

int init_sensor0(void)
{
	return private_i2c_add_driver(&sc8238_driver);
}

void exit_sensor0(void)
{
	private_i2c_del_driver(&sc8238_driver);
}

char * get_sensor1_name(void)
{
	return "sc8238";
}

int get_sensor1_i2c_addr(void)
{
	return 0x30;
}

int get_sensor1_width(void)
{
	return sc8238_win_sizes->width;
}

int get_sensor1_height(void)
{
	return sc8238_win_sizes->height;
}


int init_sensor1(void)
{
	return 0;
}

void exit_sensor1(void)
{
	return;
}
#endif
