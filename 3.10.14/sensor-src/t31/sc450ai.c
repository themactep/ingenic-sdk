// SPDX-License-Identifier: GPL-2.0+
/*
 * sc450ai.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <tx-isp-common.h>
#include <sensor-common.h>
#include <sensor-info.h>

// ============================================================================
// SENSOR IDENTIFICATION
// ============================================================================
#define SENSOR_NAME "sc450ai"
#define SENSOR_VERSION "H20220228a"
#define SENSOR_CHIP_ID 0xbd2f
#define SENSOR_CHIP_ID_H (0xbd)
#define SENSOR_CHIP_ID_L (0x2f)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x30

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 2592
#define SENSOR_MAX_HEIGHT 1520

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_PCLK (144000000) /* mipiclk*lane_num/bit = pclk */
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
	.actual_fps = 0,
	.chip_i2c_addr = SENSOR_I2C_ADDRESS,
	.width = SENSOR_MAX_WIDTH,
	.height = SENSOR_MAX_HEIGHT,
};

struct regval_list {
    uint16_t reg_num;
    unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
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
	{0x352, 23414},
	{0x353, 24587},
	{0x354, 25746},
	{0x355, 26820},
	{0x356, 27953},
	{0x357, 29002},
	{0x358, 30109},
	{0x359, 31203},
	{0x35a, 32217},
	{0x35b, 33287},
	{0x35c, 34345},
	{0x35d, 35326},
	{0x35e, 36361},
	{0x35f, 37322},
	{0x360, 38336},
	{0x361, 39339},
	{0x362, 40270},
	{0x363, 41253},
	{0x364, 42226},
	{0x365, 43129},
	{0x366, 44082},
	{0x367, 44968},
	{0x368, 45904},
	{0x369, 46830},
	{0x36a, 47690},
	{0x36b, 48599},
	{0x36c, 49500},
	{0x36d, 50336},
	{0x36e, 51220},
	{0x36f, 52042},
	{0x370, 52910},
	{0x371, 53771},
	{0x372, 54571},
	{0x373, 55416},
	{0x374, 56254},
	{0x375, 57033},
	{0x376, 57857},
	{0x377, 58623},
	{0x378, 59433},
	{0x379, 60237},
	{0x37a, 60984},
	{0x37b, 61774},
	{0x37c, 62558},
	{0x37d, 63287},
	{0x37e, 64059},
	{0x37f, 64776},
	{0x740, 65536},
	{0x741, 66990},
	{0x742, 68468},
	{0x743, 69878},
	{0x744, 71267},
	{0x745, 72637},
	{0x746, 74030},
	{0x747, 75360},
	{0x748, 76672},
	{0x749, 77965},
	{0x74a, 79283},
	{0x74b, 80541},
	{0x74c, 81784},
	{0x74d, 83010},
	{0x74e, 84259},
	{0x74f, 85454},
	{0x750, 86633},
	{0x751, 87799},
	{0x752, 88986},
	{0x753, 90123},
	{0x754, 91246},
	{0x755, 92356},
	{0x756, 93489},
	{0x757, 94573},
	{0x758, 95645},
	{0x759, 96705},
	{0x75a, 97786},
	{0x75b, 98823},
	{0x75c, 99848},
	{0x75d, 100862},
	{0x75e, 101897},
	{0x75f, 102890},
	{0x760, 103872},
	{0x761, 104844},
	{0x762, 105837},
	{0x763, 106789},
	{0x764, 107731},
	{0x765, 108665},
	{0x766, 109618},
	{0x767, 110533},
	{0x768, 111440},
	{0x769, 112337},
	{0x76a, 113255},
	{0x76b, 114135},
	{0x76c, 115008},
	{0x76d, 115872},
	{0x76e, 116756},
	{0x76f, 117605},
	{0x770, 118446},
	{0x771, 119280},
	{0x772, 120133},
	{0x773, 120952},
	{0x774, 121764},
	{0x775, 122569},
	{0x776, 123393},
	{0x777, 124185},
	{0x778, 124969},
	{0x779, 125748},
	{0x2340, 126545},
	{0x2341, 127996},
	{0x2342, 129450},
	{0x2343, 130859},
	{0x2344, 132269},
	{0x2345, 133636},
	{0x2346, 135007},
	{0x2347, 136335},
	{0x2348, 137667},
	{0x2349, 138981},
	{0x234a, 140255},
	{0x234b, 141533},
	{0x234c, 142773},
	{0x234d, 144018},
	{0x234e, 145227},
	{0x234f, 146440},
	{0x2350, 147638},
	{0x2351, 148801},
	{0x2352, 149969},
	{0x2353, 151104},
	{0x2354, 152245},
	{0x2355, 153353},
	{0x2356, 154467},
	{0x2357, 155568},
	{0x2358, 156638},
	{0x2359, 157714},
	{0x235a, 158761},
	{0x235b, 159813},
	{0x235c, 160836},
	{0x235d, 161866},
	{0x235e, 162884},
	{0x235f, 163875},
	{0x2360, 164873},
	{0x2361, 165843},
	{0x2362, 166820},
	{0x2363, 167770},
	{0x2364, 168728},
	{0x2365, 169675},
	{0x2366, 170598},
	{0x2367, 171527},
	{0x2368, 172432},
	{0x2369, 173343},
	{0x236a, 174231},
	{0x236b, 175125},
	{0x236c, 176011},
	{0x236d, 176874},
	{0x236e, 177743},
	{0x236f, 178591},
	{0x2370, 179445},
	{0x2371, 180277},
	{0x2372, 181116},
	{0x2373, 181948},
	{0x2374, 182759},
	{0x2375, 183576},
	{0x2376, 184373},
	{0x2377, 185177},
	{0x2378, 185961},
	{0x2379, 186751},
	{0x237a, 187535},
	{0x237b, 188299},
	{0x237c, 189070},
	{0x237d, 189822},
	{0x237e, 190581},
	{0x237f, 191321},
	{0x2740, 192068},
	{0x2741, 193532},
	{0x2742, 194974},
	{0x2743, 196395},
	{0x2744, 197805},
	{0x2745, 199184},
	{0x2746, 200543},
	{0x2747, 201882},
	{0x2748, 203203},
	{0x2749, 204506},
	{0x274a, 205791},
	{0x274b, 207069},
	{0x274c, 208320},
	{0x274d, 209554},
	{0x274e, 210773},
	{0x274f, 211976},
	{0x2750, 213164},
	{0x2751, 214337},
	{0x2752, 215505},
	{0x2753, 216650},
	{0x2754, 217781},
	{0x2755, 218899},
	{0x2756, 220003},
	{0x2757, 221095},
	{0x2758, 222174},
	{0x2759, 223250},
	{0x275a, 224305},
	{0x275b, 225349},
	{0x275c, 226381},
	{0x275d, 227402},
	{0x275e, 228412},
	{0x275f, 229411},
	{0x2760, 230409},
	{0x2761, 231387},
	{0x2762, 232356},
	{0x2763, 233314},
	{0x2764, 234264},
	{0x2765, 235203},
	{0x2766, 236134},
	{0x2767, 237055},
	{0x2768, 237975},
	{0x2769, 238879},
	{0x276a, 239774},
	{0x276b, 240661},
	{0x276c, 241539},
	{0x276d, 242410},
	{0x276e, 243272},
	{0x276f, 244134},
	{0x2770, 244981},
	{0x2771, 245820},
	{0x2772, 246652},
	{0x2773, 247477},
	{0x2774, 248295},
	{0x2775, 249105},
	{0x2776, 249916},
	{0x2777, 250713},
	{0x2778, 251503},
	{0x2779, 252287},
	{0x277a, 253064},
	{0x277b, 253835},
	{0x277c, 254600},
	{0x277d, 255365},
	{0x277e, 256117},
	{0x277f, 256864},
	{0x2f40, 257604},
	{0x2f41, 259068},
	{0x2f42, 260516},
	{0x2f43, 261936},
	{0x2f44, 263336},
	{0x2f45, 264714},
	{0x2f46, 266079},
	{0x2f47, 267418},
	{0x2f48, 268739},
	{0x2f49, 270047},
	{0x2f4a, 271332},
	{0x2f4b, 272600},
	{0x2f4c, 273851},
	{0x2f4d, 275090},
	{0x2f4e, 276309},
	{0x2f4f, 277512},
	{0x2f50, 278705},
	{0x2f51, 279878},
	{0x2f52, 281037},
	{0x2f53, 282181},
	{0x2f54, 283317},
	{0x2f55, 284435},
	{0x2f56, 285539},
	{0x2f57, 286631},
	{0x2f58, 287715},
	{0x2f59, 288782},
	{0x2f5a, 289837},
	{0x2f5b, 290885},
	{0x2f5c, 291917},
	{0x2f5d, 292938},
	{0x2f5e, 293948},
	{0x2f5f, 294952},
	{0x2f60, 295940},
	{0x2f61, 296919},
	{0x2f62, 297892},
	{0x2f63, 298850},
	{0x2f64, 299800},
	{0x2f65, 300739},
	{0x2f66, 301674},
	{0x2f67, 302595},
	{0x2f68, 303507},
	{0x2f69, 304415},
	{0x2f6a, 305310},
	{0x2f6b, 306197},
	{0x2f6c, 307075},
    {0x2f6d, 307949},
    {0x2f6e, 308812},
    {0x2f6f, 309666},
    {0x2f70, 310517},
    {0x2f71, 311356},
    {0x2f72, 312188},
    {0x2f73, 313013},
    {0x2f74, 313834},
    {0x2f75, 314645},
    {0x2f76, 315449},
    {0x2f77, 316246},
    {0x2f78, 317039},
    {0x2f79, 317823},
    {0x2f7a, 318600},
    {0x2f7b, 319374},
    {0x2f7c, 320139},
    {0x2f7d, 320897},
    {0x2f7e, 321650},
    {0x2f7f, 322400},
    {0x3f40, 323140},
    {0x3f41, 324608},
    {0x3f42, 326049},
    {0x3f43, 327472},
    {0x3f44, 328872},
    {0x3f45, 330253},
    {0x3f46, 331612},
    {0x3f47, 332954},
    {0x3f48, 334278},
    {0x3f49, 335580},
    {0x3f4a, 336868},
    {0x3f4b, 338136},
    {0x3f4c, 339389},
    {0x3f4d, 340624},
    {0x3f4e, 341845},
    {0x3f4f, 343048},
    {0x3f50, 344238},
    {0x3f51, 345414},
    {0x3f52, 346573},
    {0x3f53, 347720},
    {0x3f54, 348851},
    {0x3f55, 349971},
    {0x3f56, 351075},
    {0x3f57, 352169},
    {0x3f58, 353251},
    {0x3f59, 354318},
    {0x3f5a, 355375},
    {0x3f5b, 356419},
    {0x3f5c, 357453},
    {0x3f5d, 358474},
    {0x3f5e, 359486},
    {0x3f5f, 360485},
    {0x3f60, 361476},
    {0x3f61, 362457},
    {0x3f62, 363426},
    {0x3f63, 364386},
    {0x3f64, 365336},
    {0x3f65, 366277},
    {0x3f66, 367208},
    {0x3f67, 368131},
    {0x3f68, 369045},
    {0x3f69, 369949},
    {0x3f6a, 370846},
    {0x3f6b, 371733},
    {0x3f6c, 372613},
    {0x3f6d, 373483},
    {0x3f6e, 374348},
    {0x3f6f, 375202},
    {0x3f70, 376051},
    {0x3f71, 376892},
    {0x3f72, 377724},
    {0x3f73, 378551},
    {0x3f74, 379369},
    {0x3f75, 380181},
    {0x3f76, 380985},
    {0x3f77, 381783},
    {0x3f78, 382575},
    {0x3f79, 383359},
    {0x3f7a, 384138},
    {0x3f7b, 384909},
    {0x3f7c, 385675},
    {0x3f7d, 386433},
    {0x3f7e, 387188},
    {0x3f7f, 387934},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi={
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 720,
    .lans = 2,
    .settle_time_apative_en = 0,
    .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
    .mipi_sc.hcrop_diff_en = 0,
    .mipi_sc.mipi_vcomp_en = 0,
    .mipi_sc.mipi_hcomp_en = 0,
    .mipi_sc.line_sync_mode = 0,
    .mipi_sc.work_start_flag = 0,
    .image_twidth = 2592,
    .image_theight = 1520,
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
};

struct tx_isp_mipi_bus sensor_mipi ={
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 360,
    .lans = 4,
    .settle_time_apative_en = 0,
    .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
    .mipi_sc.hcrop_diff_en = 0,
    .mipi_sc.mipi_vcomp_en = 0,
    .mipi_sc.mipi_hcomp_en = 0,
    .mipi_sc.line_sync_mode = 0,
    .mipi_sc.work_start_flag = 0,
    .image_twidth = 2696,
    .image_theight = 1528,
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
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 387934,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 3736,
	.integration_time_limit = 3736,
	.total_width = 750,
	.total_height = 3736,
	.max_integration_time = 3736,	/* 2*(320e,320f)-8 */
	.one_line_expr_in_us = 21,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_2592_1520_25fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x3a},
	{0x3019, 0x0c},
	{0x301c, 0x78},
	{0x301f, 0x0b},
	{0x302e, 0x00},
	{0x3200, 0x00},
	{0x3201, 0x30},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3204, 0x0a},
	{0x3205, 0x5f},
	{0x3206, 0x05},
	{0x3207, 0xff},
	{0x3208, 0x0a},
	{0x3209, 0x20},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x02},/*hts*/
	{0x320d, 0xee},//
	{0x320e, 0x07},/*vts*/
	{0x320f, 0x50},//
	{0x3210, 0x00},
	{0x3211, 0x08},
	{0x3212, 0x00},
	{0x3213, 0x08},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3220, 0x00},
	{0x3223, 0xc0},
	{0x3253, 0x10},
	{0x325f, 0x44},
	{0x3274, 0x09},
	{0x3280, 0x01},
	{0x3301, 0x07},
	{0x3306, 0x20},
	{0x3308, 0x08},
	{0x330b, 0x58},
	{0x330e, 0x18},
	{0x3315, 0x00},
	{0x335d, 0x60},
	{0x3364, 0x56},
	{0x338f, 0x80},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x07},
	{0x3394, 0x10},
	{0x3395, 0x18},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x10},
	{0x339a, 0x13},
	{0x339b, 0x15},
	{0x339c, 0x18},
	{0x33af, 0x18},
	{0x360f, 0x13},
	{0x3621, 0xec},
	{0x3622, 0x00},
	{0x3625, 0x0b},
	{0x3627, 0x20},
	{0x3630, 0x90},
	{0x3633, 0x56},
	{0x3637, 0x1d},
	{0x3638, 0x12},
	{0x363c, 0x0f},
	{0x363d, 0x0f},
	{0x363e, 0x08},
	{0x3670, 0x4a},
	{0x3671, 0xe0},
	{0x3672, 0xe0},
	{0x3673, 0xe1},
	{0x3674, 0xc0},
	{0x3675, 0x87},
	{0x3676, 0x8c},
	{0x367a, 0x48},
	{0x367b, 0x58},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x22},
	{0x3691, 0x33},
	{0x3692, 0x44},
	{0x3699, 0x03},
	{0x369a, 0x07},
	{0x369b, 0x0f},
	{0x369c, 0x40},
	{0x369d, 0x78},
	{0x36a2, 0x08},
	{0x36a3, 0x58},
	{0x36b0, 0x53},
	{0x36b1, 0x73},
	{0x36b2, 0x34},
	{0x36b3, 0x40},
	{0x36b4, 0x78},
	{0x36b7, 0xa0},
	{0x36b8, 0xa0},
	{0x36b9, 0x20},
	{0x36bd, 0x40},
	{0x36be, 0x48},
	{0x36d0, 0x20},
	{0x36e0, 0x08},
	{0x36e1, 0x08},
	{0x36e2, 0x12},
	{0x36e3, 0x48},
	{0x36e4, 0x78},
	{0x36ea, 0x0f},
	{0x36eb, 0x05},
	{0x36ec, 0x43},
	{0x36ed, 0x14},
	{0x36fa, 0xcd},
	{0x36fb, 0xb4},
	{0x36fc, 0x00},
	{0x36fd, 0x04},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391e, 0xf1},
	{0x3933, 0x81},
	{0x3934, 0x03},
	{0x3937, 0x7c},
	{0x3939, 0x00},
	{0x393a, 0x0f},
	{0x3e01, 0xe9},
	{0x3e02, 0x80},
	{0x3e03, 0x0b},
	{0x3e08, 0x03},
	{0x3e1b, 0x2a},
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4837, 0x16},
	{0x5000, 0x0e},
	{0x5001, 0x44},
	{0x5799, 0x06},
	{0x59e0, 0xfe},
	{0x59e1, 0x40},
	{0x59e2, 0x3f},
	{0x59e3, 0x38},
	{0x59e4, 0x30},
	{0x59e5, 0x3f},
	{0x59e6, 0x38},
	{0x59e7, 0x30},
	{0x59e8, 0x3f},
	{0x59e9, 0x3c},
	{0x59ea, 0x38},
	{0x59eb, 0x3f},
	{0x59ec, 0x3c},
	{0x59ed, 0x38},
	{0x59ee, 0xfe},
	{0x59ef, 0x40},
	{0x59f4, 0x3f},
	{0x59f5, 0x38},
	{0x59f6, 0x30},
	{0x59f7, 0x3f},
	{0x59f8, 0x38},
	{0x59f9, 0x30},
	{0x59fa, 0x3f},
	{0x59fb, 0x3c},
	{0x59fc, 0x38},
	{0x59fd, 0x3f},
	{0x59fe, 0x3c},
	{0x59ff, 0x38},
	{0x36e9, 0x20},
	{0x36f9, 0x53},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},

};

static struct regval_list sensor_init_regs_2696_1528_30fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301c, 0x78},
	{0x301f, 0x09},
	{0x302e, 0x00},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3204, 0x0a},
	{0x3205, 0x8f},
	{0x3206, 0x05},
	{0x3207, 0xff},
	{0x3208, 0x0a},
	{0x3209, 0x88},
	{0x320a, 0x05},
	{0x320b, 0xf8},
	{0x320c, 0x02},/*hts*/
	{0x320d, 0xee},//
	{0x320e, 0x06},/*vts*/
	{0x320f, 0x18},//
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3220, 0x00},
	{0x3223, 0xc0},
	{0x3253, 0x10},
	{0x325f, 0x44},
	{0x3274, 0x09},
	{0x3280, 0x01},
	{0x3301, 0x07},
	{0x3306, 0x20},
	{0x3308, 0x08},
	{0x330b, 0x58},
	{0x330e, 0x18},
	{0x3315, 0x00},
	{0x335d, 0x60},
	{0x3364, 0x56},
	{0x338f, 0x80},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x07},
	{0x3394, 0x10},
	{0x3395, 0x18},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x10},
	{0x339a, 0x13},
	{0x339b, 0x15},
	{0x339c, 0x18},
	{0x33af, 0x18},
	{0x360f, 0x13},
	{0x3621, 0xec},
	{0x3622, 0x00},
	{0x3625, 0x0b},
	{0x3627, 0x20},
	{0x3630, 0x90},
	{0x3633, 0x56},
	{0x3637, 0x1d},
	{0x3638, 0x12},
	{0x363c, 0x0f},
	{0x363d, 0x0f},
	{0x363e, 0x08},
	{0x3670, 0x4a},
	{0x3671, 0xe0},
	{0x3672, 0xe0},
	{0x3673, 0xe1},
	{0x3674, 0xc0},
	{0x3675, 0x87},
	{0x3676, 0x8c},
	{0x367a, 0x48},
	{0x367b, 0x58},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x22},
	{0x3691, 0x33},
	{0x3692, 0x44},
	{0x3699, 0x03},
	{0x369a, 0x07},
	{0x369b, 0x0f},
	{0x369c, 0x40},
	{0x369d, 0x78},
	{0x36a2, 0x08},
	{0x36a3, 0x58},
	{0x36b0, 0x53},
	{0x36b1, 0x73},
	{0x36b2, 0x34},
	{0x36b3, 0x40},
	{0x36b4, 0x78},
	{0x36b7, 0xa0},
	{0x36b8, 0xa0},
	{0x36b9, 0x20},
	{0x36bd, 0x40},
	{0x36be, 0x48},
	{0x36d0, 0x20},
	{0x36e0, 0x08},
	{0x36e1, 0x08},
	{0x36e2, 0x12},
	{0x36e3, 0x48},
	{0x36e4, 0x78},
	{0x36ea, 0x06},
	{0x36eb, 0x05},
	{0x36ec, 0x53},
	{0x36ed, 0x24},
	{0x36fa, 0xcd},
	{0x36fb, 0xb4},
	{0x36fc, 0x00},
	{0x36fd, 0x04},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391e, 0xf1},
	{0x3933, 0x81},
	{0x3934, 0x03},
	{0x3937, 0x7c},
	{0x3939, 0x00},
	{0x393a, 0x0f},
	{0x3e01, 0xc2},
	{0x3e02, 0x60},
	{0x3e03, 0x0b},
	{0x3e08, 0x03},
	{0x3e1b, 0x2a},
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4837, 0x2c},
	{0x5000, 0x0e},
	{0x5001, 0x44},
	{0x5799, 0x06},
	{0x59e0, 0xfe},
	{0x59e1, 0x40},
	{0x59e2, 0x3f},
	{0x59e3, 0x38},
	{0x59e4, 0x30},
	{0x59e5, 0x3f},
	{0x59e6, 0x38},
	{0x59e7, 0x30},
	{0x59e8, 0x3f},
	{0x59e9, 0x3c},
	{0x59ea, 0x38},
	{0x59eb, 0x3f},
	{0x59ec, 0x3c},
	{0x59ed, 0x38},
	{0x59ee, 0xfe},
	{0x59ef, 0x40},
	{0x59f4, 0x3f},
	{0x59f5, 0x38},
	{0x59f6, 0x30},
	{0x59f7, 0x3f},
	{0x59f8, 0x38},
	{0x59f9, 0x30},
	{0x59fa, 0x3f},
	{0x59fb, 0x3c},
	{0x59fc, 0x38},
	{0x59fd, 0x3f},
	{0x59fe, 0x3c},
	{0x59ff, 0x38},
	{0x36e9, 0x24},
	{0x36f9, 0x53},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 2592*1520 */
	{
		.width = 2592,
		.height = 1520,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2592_1520_25fps_mipi,
	},
	{
		.width = 2696,
		.height = 1528,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2696_1528_30fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	ret += sensor_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret = sensor_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
	ret = sensor_write_array(sd, wsize->regs);
	if (ret)
		return ret;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		ISP_WARNING("%s stream on\n", SENSOR_NAME);
	} else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
//	unsigned int sclk = 0;
//	unsigned int hts = 0;
	unsigned int vts = 0;
//	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}
/*
	sclk = SENSOR_SUPPORT_PCLK;
	ret += sensor_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x320d, &tmp);
	hts = ((hts << 8) + tmp);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
*/
	vts = 46800 / ((fps & 0xffff0000) >> 16);	/* VTS25 = 1872  eg: VTS15=VTS25 * (25/15) */
	ret = sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

	sensor->video.fps = fps;


	sensor_update_actual_fps((fps >> 16) & 0xffff);
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;

		sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio request fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio request fail %d\n",pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if (arg)
		//	ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if (arg)
		//	ret = sensor_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sensor_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sensor_core_ops = {
	.g_chip_ident = sensor_g_chip_ident,
	.reset = sensor_reset,
	.init = sensor_init,
	.g_register = sensor_g_register,
	.s_register = sensor_s_register,
};

static struct tx_isp_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
};

static struct tx_isp_subdev_sensor_ops sensor_sensor_ops = {
	.ioctl = sensor_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.sensor = &sensor_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = SENSOR_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
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

	switch(sensor_max_fps) {
		case TX_SENSOR_MAX_FPS_25:
			wsize = &sensor_win_sizes[0];
			sensor_info.max_fps = 25;
			memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
        	break;
		case TX_SENSOR_MAX_FPS_30:
			wsize = &sensor_win_sizes[1];
			sensor_info.max_fps = 30;
			memcpy(&(sensor_attr.mipi), &sensor_mipi1, sizeof(sensor_mipi1));
        	break;
		default:
			ISP_ERROR("do not support max framerate %d in mipi mode\n",sensor_max_fps);
	}

	sd = &sensor->sd;
	video = &sensor->video;
	sensor_attr.expo_fs = 1;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void)
{
	int ret = 0;
	sensor_common_init(&sensor_info);

	ret = private_driver_get_interface();
	if (ret) {
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}

	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
	sensor_common_exit();
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
