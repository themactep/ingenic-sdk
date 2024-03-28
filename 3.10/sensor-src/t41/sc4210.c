// SPDX-License-Identifier: GPL-2.0+
/*
 * sc4210.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0            2560*1440     15        mipi_2lane           linear
 *   1            2560*1440     30        mipi_2lane           linear
 *   2            1280*720       30        mipi_2lane           linear
 */

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
#include <sensor-info.h>
#include <txx-funcs.h>

#define SENSOR_NAME "sc4210"
#define SENSOR_CHIP_ID_H (0x42)
#define SENSOR_CHIP_ID_L (0x10)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_BINNING_SCLK (144000000)
#define SENSOR_SUPPORT_25FPS_SCLK (122400000)
#define SENSOR_SUPPORT_15FPS_SCLK (67478400)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230522a"

static int reset_gpio = GPIO_PA(18);
static int pwdn_gpio = -1;

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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
	{0x2340, 95643},
	{0x2341, 97110},
	{0x2342, 98555},
	{0x2343, 99978},
	{0x2344, 101379},
	{0x2345, 102760},
	{0x2346, 104122},
	{0x2347, 105464},
	{0x2348, 106787},
	{0x2349, 108092},
	{0x234a, 109379},
	{0x234b, 110649},
	{0x234c, 111902},
	{0x234d, 113139},
	{0x234e, 114360},
	{0x234f, 115565},
	{0x2350, 116755},
	{0x2351, 117903},
	{0x2352, 119064},
	{0x2353, 120211},
	{0x2354, 121344},
	{0x2355, 122464},
	{0x2356, 123571},
	{0x2357, 124665},
	{0x2358, 125746},
	{0x2359, 126815},
	{0x235a, 127872},
	{0x235b, 128918},
	{0x235c, 129952},
	{0x235d, 130975},
	{0x235e, 131987},
	{0x235f, 132988},
	{0x2360, 133979},
	{0x2361, 134959},
	{0x2362, 135930},
	{0x2363, 136890},
	{0x2364, 137841},
	{0x2365, 138783},
	{0x2366, 139715},
	{0x2367, 140638},
	{0x2368, 141552},
	{0x2369, 142457},
	{0x236a, 143354},
	{0x236b, 144242},
	{0x236c, 145123},
	{0x236d, 145995},
	{0x236e, 146859},
	{0x236f, 147715},
	{0x2370, 148563},
	{0x2371, 149385},
	{0x2372, 150218},
	{0x2373, 151045},
	{0x2374, 151864},
	{0x2375, 152676},
	{0x2376, 153482},
	{0x2377, 154280},
	{0x2378, 155072},
	{0x2379, 155857},
	{0x237a, 156636},
	{0x237b, 157408},
	{0x237c, 158174},
	{0x237d, 158934},
	{0x237e, 159688},
	{0x237f, 160436},
	{0x2740, 161178},
	{0x2741, 162645},
	{0x2742, 164090},
	{0x2743, 165513},
	{0x2744, 166914},
	{0x2745, 168295},
	{0x2746, 169657},
	{0x2747, 170999},
	{0x2748, 172322},
	{0x2749, 173612},
	{0x274a, 174899},
	{0x274b, 176169},
	{0x274c, 177423},
	{0x274d, 178660},
	{0x274e, 179880},
	{0x274f, 181086},
	{0x2750, 182276},
	{0x2751, 183451},
	{0x2752, 184612},
	{0x2753, 185759},
	{0x2754, 186892},
	{0x2755, 188012},
	{0x2756, 189118},
	{0x2757, 190212},
	{0x2758, 191293},
	{0x2759, 192350},
	{0x275a, 193407},
	{0x275b, 194453},
	{0x275c, 195487},
	{0x275d, 196510},
	{0x275e, 197522},
	{0x275f, 198523},
	{0x2760, 199514},
	{0x2761, 200494},
	{0x2762, 201465},
	{0x2763, 202425},
	{0x2764, 203376},
	{0x2765, 204318},
	{0x2766, 205250},
	{0x2767, 206173},
	{0x2768, 207087},
	{0x2769, 207982},
	{0x276a, 208879},
	{0x276b, 209767},
	{0x276c, 210647},
	{0x276d, 211519},
	{0x276e, 212384},
	{0x276f, 213240},
	{0x2770, 214088},
	{0x2771, 214929},
	{0x2772, 215763},
	{0x2773, 216589},
	{0x2774, 217409},
	{0x2775, 218221},
	{0x2776, 219026},
	{0x2777, 219824},
	{0x2778, 220616},
	{0x2779, 221392},
	{0x277a, 222171},
	{0x277b, 222943},
	{0x277c, 223709},
	{0x277d, 224469},
	{0x277e, 225223},
	{0x277f, 225971},
	{0x2f40, 226713},
	{0x2f41, 228180},
	{0x2f42, 229625},
	{0x2f43, 231048},
	{0x2f44, 232449},
	{0x2f45, 233823},
	{0x2f46, 235184},
	{0x2f47, 236526},
	{0x2f48, 237849},
	{0x2f49, 239154},
	{0x2f4a, 240442},
	{0x2f4b, 241712},
	{0x2f4c, 242965},
	{0x2f4d, 244195},
	{0x2f4e, 245415},
	{0x2f4f, 246621},
	{0x2f50, 247811},
	{0x2f51, 248986},
	{0x2f52, 250147},
	{0x2f53, 251294},
	{0x2f54, 252427},
	{0x2f55, 253540},
	{0x2f56, 254647},
	{0x2f57, 255741},
	{0x2f58, 256822},
	{0x2f59, 257891},
	{0x2f5a, 258948},
	{0x2f5b, 259994},
	{0x2f5c, 261028},
	{0x2f5d, 262045},
	{0x2f5e, 263057},
	{0x2f5f, 264058},
	{0x2f60, 265049},
	{0x2f61, 266029},
	{0x2f62, 267000},
	{0x2f63, 267960},
	{0x2f64, 268911},
	{0x2f65, 269847},
	{0x2f66, 270779},
	{0x2f67, 271702},
	{0x2f68, 272617},
	{0x2f69, 273522},
	{0x2f6a, 274419},
	{0x2f6b, 275307},
	{0x2f6c, 276187},
	{0x2f6d, 277054},
	{0x2f6e, 277919},
	{0x2f6f, 278775},
	{0x2f70, 279623},
	{0x2f71, 280464},
	{0x2f72, 281298},
	{0x2f73, 282124},
	{0x2f74, 282944},
	{0x2f75, 283751},
	{0x2f76, 284556},
	{0x2f77, 285355},
	{0x2f78, 286146},
	{0x2f79, 286932},
	{0x2f7a, 287710},
	{0x2f7b, 288483},
	{0x2f7c, 289249},
	{0x2f7d, 290004},
	{0x2f7e, 290758},
	{0x2f7f, 291506},
	{0x3f40, 292248},
	{0x3f41, 293715},
	{0x3f42, 295160},
	{0x3f43, 296578},
	{0x3f44, 297980},
	{0x3f45, 299361},
	{0x3f46, 300723},
	{0x3f47, 302061},
	{0x3f48, 303384},
	{0x3f49, 304689},
	{0x3f4a, 305977},
	{0x3f4b, 307243},
	{0x3f4c, 308496},
	{0x3f4d, 309733},
	{0x3f4e, 310954},
	{0x3f4f, 312156},
	{0x3f50, 313346},
	{0x3f51, 314521},
	{0x3f52, 315682},
	{0x3f53, 316826},
	{0x3f54, 317959},
	{0x3f55, 319079},
	{0x3f56, 320185},
	{0x3f57, 321276},
	{0x3f58, 322357},
	{0x3f59, 323426},
	{0x3f5a, 324483},
	{0x3f5b, 325526},
	{0x3f5c, 326560},
	{0x3f5d, 327583},
	{0x3f5e, 328595},
	{0x3f5f, 329593},
	{0x3f60, 330584},
	{0x3f61, 331564},
	{0x3f62, 332535},
	{0x3f63, 333493},
	{0x3f64, 334443},
	{0x3f65, 335385},
	{0x3f66, 336317},
	{0x3f67, 337237},
	{0x3f68, 338152},
	{0x3f69, 339057},
	{0x3f6a, 339954},
	{0x3f6b, 340840},
	{0x3f6c, 341720},
	{0x3f6d, 342592},
	{0x3f6e, 343456},
	{0x3f6f, 344310},
	{0x3f70, 345158},
	{0x3f71, 345999},
	{0x3f72, 346833},
	{0x3f73, 347657},
	{0x3f74, 348476},
	{0x3f75, 349288},
	{0x3f76, 350094},
	{0x3f77, 350890},
	{0x3f78, 351681},
	{0x3f79, 352467},
	{0x3f7a, 353245},
	{0x3f7b, 354015},
	{0x3f7c, 354782},
	{0x3f7d, 355542},
	{0x3f7e, 356295},
	{0x3f7f, 357041},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus mipi_binning = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 480,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x4210,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 800,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 357041,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1562 - 4,
	.integration_time_limit = 1562 - 4,
	.total_width = 2700,
	.total_height = 1500,
	.max_integration_time = 1562 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1280_720_25fps_binning_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3001, 0x07},
	{0x3002, 0xc0},
	{0x300a, 0x2c},
	{0x300f, 0x00},
	{0x3018, 0x33},
	{0x3019, 0x0c},
	{0x301f, 0x64},
	{0x3031, 0x0c},
	{0x3038, 0x22},
	{0x3207, 0xa7},
	{0x3208, 0x05},
	{0x3209, 0x00},
	{0x320a, 0x02},
	{0x320b, 0xd0},
	{0x320c, 0x06},//1600
	{0x320d, 0x40},
	{0x320e, 0x07},//1500
	{0x320f, 0x08},
	{0x3211, 0x04},
	{0x3213, 0x02},
	{0x3215, 0x31},
	{0x3220, 0x14},
	{0x3225, 0x01},
	{0x3227, 0x03},
	{0x3000, 0x00},
	{0x3229, 0x08},
	{0x3231, 0x01},
	{0x3241, 0x02},
	{0x3243, 0x03},
	{0x3249, 0x17},
	{0x3251, 0x08},
	{0x3253, 0x10},
	{0x325e, 0x00},
	{0x325f, 0x00},
	{0x3273, 0x01},
	{0x3301, 0x15},
	{0x3302, 0x18},
	{0x3304, 0x20},
	{0x3305, 0x00},
	{0x3306, 0x64},
	{0x3308, 0x10},
	{0x3309, 0x50},
	{0x330a, 0x00},
	{0x330b, 0xe8},
	{0x330e, 0x20},
	{0x3312, 0x02},
	{0x3314, 0x84},
	{0x331e, 0x19},
	{0x331f, 0x49},
	{0x3320, 0x05},
	{0x3338, 0x10},
	{0x334c, 0x10},
	{0x335d, 0x20},
	{0x335e, 0x02},
	{0x335f, 0x04},
	{0x3364, 0x1e},
	{0x3366, 0x92},
	{0x3367, 0x08},
	{0x3368, 0x05},
	{0x3369, 0xdc},
	{0x336a, 0x0b},
	{0x336b, 0xb8},
	{0x336c, 0xc2},
	{0x337a, 0x08},
	{0x337b, 0x10},
	{0x337c, 0x06},
	{0x337d, 0x0a},
	{0x337e, 0x40},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x15},
	{0x3394, 0x24},
	{0x3395, 0x24},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x15},
	{0x339a, 0x15},
	{0x339b, 0x24},
	{0x339c, 0x24},
	{0x33a2, 0x08},
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
	{0x3622, 0xff},
	{0x3624, 0x07},
	{0x3625, 0x0a},
	{0x3630, 0xc4},
	{0x3631, 0x80},
	{0x3632, 0x88},
	{0x3633, 0x42},
	{0x3634, 0x64},
	{0x3635, 0x20},
	{0x3636, 0x20},
	{0x3638, 0x28},
	{0x363b, 0x03},
	{0x363c, 0x06},
	{0x363d, 0x06},
	{0x366e, 0x04},
	{0x3670, 0x4a},
	{0x3671, 0xff},
	{0x3672, 0x9f},
	{0x3673, 0x9f},
	{0x3674, 0xc4},
	{0x3675, 0xc4},
	{0x3676, 0xb8},
	{0x367a, 0x40},
	{0x367b, 0x48},
	{0x367c, 0x40},
	{0x367d, 0x48},
	{0x3690, 0x43},
	{0x3691, 0x55},
	{0x3692, 0x66},
	{0x3699, 0x8c},
	{0x369a, 0x96},
	{0x369b, 0x9f},
	{0x369c, 0x40},
	{0x369d, 0x48},
	{0x36a2, 0x40},
	{0x36a3, 0x48},
	{0x36cc, 0x2c},
	{0x36cd, 0x30},
	{0x36ce, 0x30},
	{0x36d0, 0x20},
	{0x36d1, 0x40},
	{0x36d2, 0x40},
	{0x36ea, 0x37},
	{0x36eb, 0x06},
	{0x36ec, 0x13},
	{0x36ed, 0x1c},
	{0x36fa, 0x3b},
	{0x36fb, 0x09},
	{0x36fc, 0x10},
	{0x36fd, 0x3c},
	{0x3817, 0x20},
	{0x3905, 0xd8},
	{0x3908, 0x11},
	{0x391b, 0x80},
	{0x391c, 0x0f},
	{0x391d, 0x01},
	{0x3933, 0x1f},
	{0x3934, 0xff},
	{0x3935, 0x80},
	{0x3936, 0x1f},
	{0x393e, 0x01},
	{0x3940, 0x60},
	{0x3942, 0x04},
	{0x3943, 0xd0},
	{0x3980, 0x00},
	{0x3981, 0x30},
	{0x3982, 0x00},
	{0x3983, 0x2c},
	{0x3984, 0x00},
	{0x3985, 0x15},
	{0x3986, 0x00},
	{0x3987, 0x10},
	{0x3988, 0x00},
	{0x3989, 0x30},
	{0x398a, 0x00},
	{0x398b, 0x28},
	{0x398c, 0x00},
	{0x398d, 0x30},
	{0x398e, 0x00},
	{0x398f, 0x70},
	{0x3990, 0x0a},
	{0x3991, 0x00},
	{0x3992, 0x00},
	{0x3993, 0x60},
	{0x3994, 0x00},
	{0x3995, 0x30},
	{0x3996, 0x00},
	{0x3997, 0x10},
	{0x3998, 0x00},
	{0x3999, 0x1c},
	{0x399a, 0x00},
	{0x399b, 0x48},
	{0x399c, 0x00},
	{0x399d, 0x90},
	{0x399e, 0x00},
	{0x399f, 0xc0},
	{0x39a0, 0x14},
	{0x39a1, 0x28},
	{0x39a2, 0x48},
	{0x39a3, 0x70},
	{0x39a4, 0x18},
	{0x39a5, 0x04},
	{0x39a6, 0x08},
	{0x39a7, 0x04},
	{0x39a8, 0x01},
	{0x39a9, 0x14},
	{0x39aa, 0x28},
	{0x39ab, 0x50},
	{0x39ac, 0x30},
	{0x39ad, 0x20},
	{0x39ae, 0x10},
	{0x39af, 0x08},
	{0x39b9, 0x00},
	{0x39ba, 0x00},
	{0x39bb, 0x00},
	{0x39bc, 0x00},
	{0x39bd, 0x58},
	{0x39be, 0xc0},
	{0x39bf, 0x00},
	{0x39c0, 0x00},
	{0x39c5, 0x41},
	{0x3e00, 0x00},
	{0x3e01, 0xbb},
	{0x3e02, 0x40},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e0e, 0x6a},
	{0x3e26, 0x40},
	{0x4401, 0x0b},
	{0x4407, 0xb0},
	{0x4418, 0x16},
	{0x4501, 0xa4},
	{0x4509, 0x08},
	{0x4603, 0x00},
	{0x4800, 0x24},
	{0x4819, 0x40},
	{0x4829, 0x01},
	{0x4837, 0x25},
	{0x5000, 0x4e},
	{0x550f, 0x20},
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
	{0x5900, 0x01},
	{0x5901, 0x04},
	{0x36e9, 0x43},
	{0x36f9, 0x24},
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2560_1440_15fps_mipi[] = {
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
	{SENSOR_REG_DELAY, 0x10},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2560_1440_25fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3001, 0x07},
	{0x3002, 0xc0},
	{0x300a, 0x2c},
	{0x300f, 0x00},
	{0x3018, 0x33},
	{0x3019, 0x0c},
	{0x301f, 0x20},
	{0x3031, 0x0a},
	{0x3038, 0x22},
	{0x320c, 0x05},
	{0x320d, 0x50},
	{0x320e, 0x07},
	{0x320f, 0x08},
	{0x3220, 0x10},
	{0x3225, 0x01},
	{0x3227, 0x03},
	{0x3229, 0x08},
	{0x3231, 0x01},
	{0x3241, 0x02},
	{0x3243, 0x03},
	{0x3249, 0x17},
	{0x3251, 0x08},
	{0x3253, 0x08},
	{0x325e, 0x00},
	{0x325f, 0x00},
	{0x3273, 0x01},
	{0x3301, 0x0c},
	{0x3302, 0x18},
	{0x3000, 0x00},
	{0x3304, 0x20},
	{0x3305, 0x00},
	{0x3306, 0x74},
	{0x3308, 0x10},
	{0x3309, 0x40},
	{0x330a, 0x00},
	{0x330b, 0xe8},
	{0x330e, 0x18},
	{0x3312, 0x02},
	{0x3314, 0x84},
	{0x331e, 0x19},
	{0x331f, 0x39},
	{0x3320, 0x05},
	{0x3338, 0x10},
	{0x334c, 0x10},
	{0x335d, 0x20},
	{0x335e, 0x02},
	{0x335f, 0x04},
	{0x3364, 0x1e},
	{0x3366, 0x92},
	{0x3367, 0x08},
	{0x3368, 0x05},
	{0x3369, 0xdc},
	{0x336a, 0x0b},
	{0x336b, 0xb8},
	{0x336c, 0xc2},
	{0x337a, 0x08},
	{0x337b, 0x10},
	{0x337c, 0x06},
	{0x337d, 0x0a},
	{0x337e, 0x40},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x0c},
	{0x3394, 0x28},
	{0x3395, 0x28},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x0a},
	{0x339a, 0x18},
	{0x339b, 0x28},
	{0x339c, 0x28},
	{0x33a2, 0x08},
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
	{0x3622, 0xff},
	{0x3624, 0x07},
	{0x3625, 0x0a},
	{0x3630, 0xc4},
	{0x3631, 0x80},
	{0x3632, 0x88},
	{0x3633, 0x22},
	{0x3634, 0x64},
	{0x3635, 0x20},
	{0x3636, 0x20},
	{0x3638, 0x28},
	{0x363b, 0x03},
	{0x363c, 0x06},
	{0x363d, 0x06},
	{0x366e, 0x04},
	{0x3670, 0x48},
	{0x3671, 0xff},
	{0x3672, 0x1f},
	{0x3673, 0x1f},
	{0x367a, 0x40},
	{0x367b, 0x40},
	{0x3690, 0x42},
	{0x3691, 0x44},
	{0x3692, 0x44},
	{0x3699, 0x80},
	{0x369a, 0x9f},
	{0x369b, 0x9f},
	{0x369c, 0x40},
	{0x369d, 0x40},
	{0x36a2, 0x40},
	{0x36a3, 0x40},
	{0x36cc, 0x2c},
	{0x36cd, 0x30},
	{0x36ce, 0x30},
	{0x36d0, 0x20},
	{0x36d1, 0x40},
	{0x36d2, 0x40},
	{0x36ea, 0x3b},
	{0x36eb, 0x06},
	{0x36ec, 0x03},
	{0x36ed, 0x24},
	{0x36fa, 0x2f},
	{0x36fb, 0x14},
	{0x36fc, 0x00},
	{0x36fd, 0x04},
	{0x3817, 0x20},
	{0x3905, 0xd8},
	{0x3908, 0x11},
	{0x391b, 0x80},
	{0x391c, 0x0f},
	{0x391d, 0x21},
	{0x3933, 0x24},
	{0x3934, 0xb0},
	{0x3935, 0x80},
	{0x3936, 0x1f},
	{0x3940, 0x68},
	{0x3942, 0x04},
	{0x3943, 0xc0},
	{0x3980, 0x00},
	{0x3981, 0x50},
	{0x3982, 0x00},
	{0x3983, 0x40},
	{0x3984, 0x00},
	{0x3985, 0x20},
	{0x3986, 0x00},
	{0x3987, 0x10},
	{0x3988, 0x00},
	{0x3989, 0x20},
	{0x398a, 0x00},
	{0x398b, 0x30},
	{0x398c, 0x00},
	{0x398d, 0x50},
	{0x398e, 0x00},
	{0x398f, 0x60},
	{0x3990, 0x00},
	{0x3991, 0x70},
	{0x3992, 0x00},
	{0x3993, 0x36},
	{0x3994, 0x00},
	{0x3995, 0x20},
	{0x3996, 0x00},
	{0x3997, 0x14},
	{0x3998, 0x00},
	{0x3999, 0x20},
	{0x399a, 0x00},
	{0x399b, 0x50},
	{0x399c, 0x00},
	{0x399d, 0x90},
	{0x399e, 0x00},
	{0x399f, 0xf0},
	{0x39a0, 0x08},
	{0x39a1, 0x10},
	{0x39a2, 0x20},
	{0x39a3, 0x40},
	{0x39a4, 0x20},
	{0x39a5, 0x10},
	{0x39a6, 0x08},
	{0x39a7, 0x04},
	{0x39a8, 0x18},
	{0x39a9, 0x30},
	{0x39aa, 0x40},
	{0x39ab, 0x60},
	{0x39ac, 0x38},
	{0x39ad, 0x20},
	{0x39ae, 0x10},
	{0x39af, 0x08},
	{0x39b9, 0x00},
	{0x39ba, 0xa0},
	{0x39bb, 0x80},
	{0x39bc, 0x00},
	{0x39bd, 0x44},
	{0x39be, 0x00},
	{0x39bf, 0x00},
	{0x39c0, 0x00},
	{0x39c5, 0x41},
	{0x3e00, 0x00},
	{0x3e01, 0xbb},
	{0x3e02, 0x40},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e0e, 0x6a},
	{0x3e26, 0x40},
	{0x4401, 0x0b},
	{0x4407, 0xb0},
	{0x4418, 0x0b},
	{0x4501, 0xb4},
	{0x4509, 0x10},
	{0x4603, 0x00},
	{0x4837, 0x1b},
	{0x5000, 0x0e},
	{0x550f, 0x20},
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
	{0x36e9, 0x24},
	{0x36f9, 0x23},
	{0x0100, 0x01},
//      {SENSOR_REG_DELAY, 0x10},
	{SENSOR_REG_END, 0x00},
};
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2560,
		.height = 1440,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_15fps_mipi,
	},
	{
		.width = 2560,
		.height = 1440,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_25fps_mipi,
	},
	{
		.width = 1280,
		.height = 720,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1280_720_25fps_binning_mipi,
	},
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
		unsigned char *value) {
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
		 unsigned char value) {
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

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value) {
	int ret = -1;
	int alpha = 0;
	static unsigned int oldalpha = 0;
	static int oldAGain = -1;
	unsigned char v1 = 0;
	unsigned char v2 = 0;
	unsigned char v3 = 0;
	unsigned char v4 = 0;
	unsigned int blc = 0;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	unsigned int gainoffset = 0;
	unsigned int currAgain = oldAGain;
	unsigned int nextAgain = again;

	ret = sensor_write(sd, 0x3e00, (unsigned char) ((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char) ((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char) ((it & 0x0f) << 4));
	ret += sensor_write(sd, 0x3e09, (unsigned char) (again & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char) (((again >> 8) & 0xff)));


	if (oldAGain == -1) {
		oldAGain = again;
	}

	ret += sensor_read(sd, 0x3974, &v1);
	ret += sensor_read(sd, 0x3975, &v2);

	blc = (v1 << 8) | v2;
	if (blc > 0x2000) {
		alpha = ((blc >> 3) - 1024) * 16;
	} else {
		alpha = 0;
	}
	if ((alpha / 10) > 0xfff) {
		alpha = 0xfff;
	}

	gainoffset = (currAgain >= nextAgain) ? (currAgain - nextAgain) : (nextAgain - currAgain);
	if (gainoffset < 0x20) {
		oldalpha = (alpha / 10 + oldalpha) / 2;
	} else {
		oldalpha = (alpha / 10) * nextAgain / currAgain;
	}
	if (oldalpha > 0xfff) {
		oldalpha = 0xfff;
	}
	v3 = ((oldalpha >> 8) & 0x0f) | 0x10;
	v4 = oldalpha & 0xff;
	if (blc > 0x5200) {
		ret += sensor_write(sd, 0x3800, 0x01);
		ret += sensor_write(sd, 0x39c6, (unsigned char) (v3 & 0xff));
		ret += sensor_write(sd, 0x39c7, (unsigned char) (v4 & 0xff));
		ret += sensor_write(sd, 0x4418, 0x16);
		ret += sensor_write(sd, 0x4501, 0xa4);
		ret += sensor_write(sd, 0x4509, 0x08);
		ret += sensor_write(sd, 0x3800, 0x11);
		ret += sensor_write(sd, 0x3800, 0x41);
	} else if (blc < 0x5100) {
		ret += sensor_write(sd, 0x3800, 0x01);
		ret += sensor_write(sd, 0x39c6, (unsigned char) (v3 & 0xff));
		ret += sensor_write(sd, 0x39c7, (unsigned char) (v3 & 0xff));
		ret += sensor_write(sd, 0x4418, 0x0b);
		ret += sensor_write(sd, 0x4501, 0xb4);
		ret += sensor_write(sd, 0x4509, 0x10);
		ret += sensor_write(sd, 0x3800, 0x11);
		ret += sensor_write(sd, 0x3800, 0x41);
	} else {
		ret += sensor_write(sd, 0x3800, 0x01);
		ret += sensor_write(sd, 0x39c6, v3);
		ret += sensor_write(sd, 0x39c7, v4);
		ret += sensor_write(sd, 0x3800, 0x11);
		ret += sensor_write(sd, 0x3800, 0x41);
	}
	oldAGain = again;

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

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		sensor_set_attr(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		sensor->priv = wsize;
	}

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("%s stream on\n", SENSOR_NAME);
		}

	} else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch (sensor->info.default_boot) {
		case 0:
			sclk = SENSOR_SUPPORT_15FPS_SCLK;
			max_fps = 15;
			break;
		case 1:
			sclk = SENSOR_SUPPORT_25FPS_SCLK;
			max_fps = 30;
			break;
		case 2:
			sclk = SENSOR_SUPPORT_BINNING_SCLK;
			max_fps = 30;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return ret;
	}

	ret = sensor_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}
	hts = ((hts << 8) + tmp) << 1;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	// ret = sensor_write(sd,0x3812,0x00);
	ret += sensor_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char) (vts >> 8));
	// ret += sensor_write(sd,0x3812,0x30);
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	unsigned char val;

	ret += sensor_read(sd, 0x3221, &val);
	if (enable & 0x02) {
		val |= 0x60;
	} else {
		val &= 0x9f;
	}
	ret = sensor_write(sd, 0x3221, val);
	if (ret < 0)
		return -1;

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

struct clk *sclka;

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	ISP_INFO("default boot is %d, video interface is %d, mclk is %d, reset is %d, pwdn is %d\n", info->default_boot,
		 info->video_interface, info->mclk, info->rst_gpio, info->pwdn_gpio);
	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			break;
		case 1:
			wsize = &sensor_win_sizes[1];
			/*      sensor_attr.max_integration_time_native = 2026 - 4;
				sensor_attr.integration_time_limit = 2026 - 4;
				sensor_attr.total_width = 2880;
				sensor_attr.total_height = 2026;
				sensor_attr.max_integration_time = 2026 - 4;*/
			break;
		case 2:
			wsize = &sensor_win_sizes[2];
			memcpy((void *) (&(sensor_attr.mipi)), (void *) (&mipi_binning), sizeof(mipi_binning));
			sensor_attr.min_integration_time = 1;
			sensor_attr.min_integration_time_native = 1;
			sensor_attr.max_integration_time_native = 1500 - 1;
			sensor_attr.integration_time_limit = 1500 - 1;
			sensor_attr.total_width = 1600;
			sensor_attr.total_height = 1500;
			sensor_attr.max_integration_time = 1500 - 1;
			break;
		default:
			ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch (info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 0;
			break;
		case TISP_SENSOR_VI_MIPI_CSI1:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 1;
			break;
		case TISP_SENSOR_VI_DVP:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
			break;
		default:
			ISP_ERROR("Have no this interface!!!\n");
	}

	switch (info->mclk) {
		case TISP_SENSOR_MCLK0:
		case TISP_SENSOR_MCLK1:
		case TISP_SENSOR_MCLK2:
			sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
			sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
			set_sensor_mclk_function(0);
			break;
		default:
			ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	if (((rate / 1000) % 24000) != 0) {
		ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
		sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka)) {
			pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclka);
			if (((rate / 1000) % 24000) != 0) {
				private_clk_set_rate(sclka, 1200000000);
			}
		}
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
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
	ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

#if 0
static int tgain = -1;
static int sensor_set_logic(struct tx_isp_subdev *sd, int value)
{
	if (value != tgain) {
		if (value <= 283241) {
			sensor_write(sd, 0x5799, 0x00);
		} else if (value >= 321577) {
			sensor_write(sd, 0x5799, 0x07);
		}
		tgain = value;
	}

	return 0;
}
#endif

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_LOGIC:
			//      if (arg)
			//              ret = sensor_set_logic(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = sensor_set_expo(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			//if (arg)
			//      ret = sensor_set_integration_time(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			//if (arg)
			//      ret = sensor_set_analog_gain(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = sensor_set_digital_gain(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				ret = sensor_get_black_pedestal(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if (arg)
				ret = sensor_set_vflip(sd, sensor_val->value);
			break;
		default:
			break;
	}

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg) {
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

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg) {
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
static u64 tx_isp_module_dma_mask = ~(u64) 0;
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
			const struct i2c_device_id *id) {
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client) {
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
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

static __init int init_sensor(void) {
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
