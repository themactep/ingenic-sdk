// SPDX-License-Identifier: GPL-2.0+
/*
 * imx386.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          3840*2160       15        mipi_2lane            linear
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

#define SENSOR_NAME "imx386"
#define SENSOR_CHIP_ID_H (0x28)
#define SENSOR_CHIP_ID_L (0x23)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION "H20220920a"

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int reset_gpio = GPIO_PC(28);
static int pwdn_gpio = -1;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0, 0},
	{0x1, 0},
	{0x2, 0},
	{0x3, 0},
	{0x4, 0},
	{0x5, 0},
	{0x6, 940},
	{0x7, 940},
	{0x8, 940},
	{0x9, 940},
	{0x10, 940},
	{0x11, 1872},
	{0x12, 1872},
	{0x13, 1872},
	{0x14, 1872},
	{0x15, 2794},
	{0x16, 2794},
	{0x17, 2794},
	{0x18, 2794},
	{0x19, 2794},
	{0x20, 3708},
	{0x21, 3708},
	{0x22, 3708},
	{0x23, 3708},
	{0x24, 3708},
	{0x25, 4612},
	{0x26, 4612},
	{0x27, 4612},
	{0x28, 4612},
	{0x29, 5509},
	{0x30, 5509},
	{0x31, 5509},
	{0x32, 5509},
	{0x33, 5509},
	{0x34, 6396},
	{0x35, 6396},
	{0x36, 6396},
	{0x37, 6396},
	{0x38, 7276},
	{0x39, 7276},
	{0x40, 7276},
	{0x41, 7276},
	{0x42, 7276},
	{0x43, 8147},
	{0x44, 8147},
	{0x45, 8147},
	{0x46, 8147},
	{0x47, 9011},
	{0x48, 9011},
	{0x49, 9011},
	{0x50, 9011},
	{0x51, 9866},
	{0x52, 9866},
	{0x53, 9866},
	{0x54, 9866},
	{0x55, 10714},
	{0x56, 10714},
	{0x57, 10714},
	{0x58, 10714},
	{0x59, 11555},
	{0x60, 11555},
	{0x61, 11555},
	{0x62, 11555},
	{0x63, 12388},
	{0x64, 12388},
	{0x65, 12388},
	{0x66, 12388},
	{0x67, 13214},
	{0x68, 13214},
	{0x69, 13214},
	{0x70, 13214},
	{0x71, 14032},
	{0x72, 14032},
	{0x73, 14032},
	{0x74, 14032},
	{0x75, 14844},
	{0x76, 14844},
	{0x77, 14844},
	{0x78, 14844},
	{0x79, 15648},
	{0x80, 15648},
	{0x81, 15648},
	{0x82, 16446},
	{0x83, 16446},
	{0x84, 16446},
	{0x85, 16446},
	{0x86, 17237},
	{0x87, 17237},
	{0x88, 17237},
	{0x89, 18022},
	{0x90, 18022},
	{0x91, 18022},
	{0x92, 18022},
	{0x93, 18800},
	{0x94, 18800},
	{0x95, 18800},
	{0x96, 19572},
	{0x97, 19572},
	{0x98, 19572},
	{0x99, 19572},
	{0x100, 20338},
	{0x101, 20338},
	{0x102, 20338},
	{0x103, 21097},
	{0x104, 21097},
	{0x105, 21097},
	{0x106, 21850},
	{0x107, 21850},
	{0x108, 21850},
	{0x109, 22598},
	{0x110, 22598},
	{0x111, 22598},
	{0x112, 23339},
	{0x113, 23339},
	{0x114, 23339},
	{0x115, 23339},
	{0x116, 24075},
	{0x117, 24075},
	{0x118, 24075},
	{0x119, 24805},
	{0x120, 24805},
	{0x121, 24805},
	{0x122, 25530},
	{0x123, 25530},
	{0x124, 25530},
	{0x125, 26249},
	{0x126, 26249},
	{0x127, 26249},
	{0x128, 26962},
	{0x129, 26962},
	{0x130, 27671},
	{0x131, 27671},
	{0x132, 27671},
	{0x133, 28373},
	{0x134, 28373},
	{0x135, 28373},
	{0x136, 29071},
	{0x137, 29071},
	{0x138, 29071},
	{0x139, 29764},
	{0x140, 29764},
	{0x141, 30452},
	{0x142, 30452},
	{0x143, 30452},
	{0x144, 31134},
	{0x145, 31134},
	{0x146, 31134},
	{0x147, 31812},
	{0x148, 31812},
	{0x149, 32485},
	{0x150, 32485},
	{0x151, 32485},
	{0x152, 33153},
	{0x153, 33153},
	{0x154, 33817},
	{0x155, 33817},
	{0x156, 33817},
	{0x157, 34475},
	{0x158, 34475},
	{0x159, 35130},
	{0x160, 35130},
	{0x161, 35130},
	{0x162, 35780},
	{0x163, 35780},
	{0x164, 36425},
	{0x165, 36425},
	{0x166, 36425},
	{0x167, 37066},
	{0x168, 37066},
	{0x169, 37703},
	{0x170, 37703},
	{0x171, 38335},
	{0x172, 38335},
	{0x173, 38963},
	{0x174, 38963},
	{0x175, 38963},
	{0x176, 39587},
	{0x177, 39587},
	{0x178, 40207},
	{0x179, 40207},
	{0x180, 40823},
	{0x181, 40823},
	{0x182, 41435},
	{0x183, 41435},
	{0x184, 42043},
	{0x185, 42043},
	{0x186, 42647},
	{0x187, 42647},
	{0x188, 43248},
	{0x189, 43248},
	{0x190, 43844},
	{0x191, 43844},
	{0x192, 44437},
	{0x193, 44437},
	{0x194, 45026},
	{0x195, 45026},
	{0x196, 45611},
	{0x197, 45611},
	{0x198, 46193},
	{0x199, 46193},
	{0x200, 46772},
	{0x201, 46772},
	{0x202, 47346},
	{0x203, 47346},
	{0x204, 47918},
	{0x205, 47918},
	{0x206, 48485},
	{0x207, 48485},
	{0x208, 49050},
	{0x209, 49050},
	{0x210, 49611},
	{0x211, 50169},
	{0x212, 50169},
	{0x213, 50723},
	{0x214, 50723},
	{0x215, 51275},
	{0x216, 51275},
	{0x217, 51823},
	{0x218, 52368},
	{0x219, 52368},
	{0x220, 52910},
	{0x221, 52910},
	{0x222, 53448},
	{0x223, 53984},
	{0x224, 53984},
	{0x225, 54517},
	{0x226, 55046},
	{0x227, 55046},
	{0x228, 55573},
	{0x229, 55573},
	{0x230, 56097},
	{0x231, 56618},
	{0x232, 56618},
	{0x233, 57136},
	{0x234, 57651},
	{0x235, 57651},
	{0x236, 58163},
	{0x237, 58673},
	{0x238, 58673},
	{0x239, 59180},
	{0x240, 59684},
	{0x241, 59684},
	{0x242, 60186},
	{0x243, 60685},
	{0x244, 61181},
	{0x245, 61181},
	{0x246, 61675},
	{0x247, 62166},
	{0x248, 62166},
	{0x249, 62655},
	{0x250, 63141},
	{0x251, 63624},
	{0x252, 63624},
	{0x253, 64106},
	{0x254, 64584},
	{0x255, 65061},
	{0x256, 65535},
	{0x257, 65535},
	{0x258, 66006},
	{0x259, 66475},
	{0x260, 66942},
	{0x261, 66942},
	{0x262, 67407},
	{0x263, 67869},
	{0x264, 68329},
	{0x265, 68787},
	{0x266, 69243},
	{0x267, 69243},
	{0x268, 69696},
	{0x269, 70147},
	{0x270, 70597},
	{0x271, 71044},
	{0x272, 71489},
	{0x273, 71931},
	{0x274, 72372},
	{0x275, 72811},
	{0x276, 72811},
	{0x277, 73248},
	{0x278, 73682},
	{0x279, 74115},
	{0x280, 74546},
	{0x281, 74975},
	{0x282, 75401},
	{0x283, 75826},
	{0x284, 76249},
	{0x285, 76671},
	{0x286, 77090},
	{0x287, 77507},
	{0x288, 77923},
	{0x289, 78337},
	{0x290, 78749},
	{0x291, 79159},
	{0x292, 79567},
	{0x293, 79974},
	{0x294, 80379},
	{0x295, 80782},
	{0x296, 81583},
	{0x297, 81981},
	{0x298, 82378},
	{0x299, 82772},
	{0x300, 83166},
	{0x301, 83557},
	{0x302, 83947},
	{0x303, 84335},
	{0x304, 85107},
	{0x305, 85491},
	{0x306, 85873},
	{0x307, 86253},
	{0x308, 86632},
	{0x309, 87385},
	{0x310, 87760},
	{0x311, 88133},
	{0x312, 88874},
	{0x313, 89243},
	{0x314, 89610},
	{0x315, 89976},
	{0x316, 90703},
	{0x317, 91065},
	{0x318, 91425},
	{0x319, 92141},
	{0x320, 92497},
	{0x321, 93206},
	{0x322, 93558},
	{0x323, 93908},
	{0x324, 94606},
	{0x325, 94953},
	{0x326, 95643},
	{0x327, 95987},
	{0x328, 96669},
	{0x329, 97009},
	{0x330, 97684},
	{0x331, 98020},
	{0x332, 98688},
	{0x333, 99352},
	{0x334, 99682},
	{0x335, 100338},
	{0x336, 100665},
	{0x337, 101315},
	{0x338, 101960},
	{0x339, 102281},
	{0x340, 102920},
	{0x341, 103554},
	{0x342, 104185},
	{0x343, 104498},
	{0x344, 105122},
	{0x345, 105742},
	{0x346, 106358},
	{0x347, 106970},
	{0x348, 107578},
	{0x349, 108182},
	{0x350, 108783},
	{0x351, 109379},
	{0x352, 109972},
	{0x353, 110561},
	{0x354, 111146},
	{0x355, 111728},
	{0x356, 112307},
	{0x357, 112881},
	{0x358, 113453},
	{0x359, 114020},
	{0x360, 114585},
	{0x361, 115425},
	{0x362, 115981},
	{0x363, 116534},
	{0x364, 117084},
	{0x365, 117903},
	{0x366, 118445},
	{0x367, 119251},
	{0x368, 119786},
	{0x369, 120581},
	{0x370, 121108},
	{0x371, 121893},
	{0x372, 122412},
	{0x373, 123186},
	{0x374, 123954},
	{0x375, 124462},
	{0x376, 125219},
	{0x377, 125971},
	{0x378, 126716},
	{0x379, 127210},
	{0x380, 127946},
	{0x381, 128676},
	{0x382, 129400},
	{0x383, 130119},
	{0x384, 131070},
	{0x385, 131776},
	{0x386, 132477},
	{0x387, 133173},
	{0x388, 133864},
	{0x389, 134778},
	{0x390, 135457},
	{0x391, 136355},
	{0x392, 137024},
	{0x393, 137907},
	{0x394, 138565},
	{0x395, 139434},
	{0x396, 140295},
	{0x397, 141149},
	{0x398, 141995},
	{0x399, 142834},
	{0x400, 143665},
	{0x401, 144489},
	{0x402, 145306},
	{0x403, 146116},
	{0x404, 147118},
	{0x405, 147913},
	{0x406, 148897},
	{0x407, 149676},
	{0x408, 150642},
	{0x409, 151598},
	{0x410, 152356},
	{0x411, 153295},
	{0x412, 154409},
	{0x413, 155328},
	{0x414, 156238},
	{0x415, 157140},
	{0x416, 158210},
	{0x417, 159093},
	{0x418, 160141},
	{0x419, 161178},
	{0x420, 162204},
	{0x421, 163219},
	{0x422, 164223},
	{0x423, 165381},
	{0x424, 166363},
	{0x425, 167495},
	{0x426, 168614},
	{0x427, 169720},
	{0x428, 170813},
	{0x429, 171893},
	{0x430, 173113},
	{0x431, 174318},
	{0x432, 175507},
	{0x433, 176681},
	{0x434, 177842},
	{0x435, 178988},
	{0x436, 180260},
	{0x437, 181516},
	{0x438, 182756},
	{0x439, 184114},
	{0x440, 185454},
	{0x441, 186774},
	{0x442, 188077},
	{0x443, 189489},
	{0x444, 190754},
	{0x445, 192251},
	{0x446, 193603},
	{0x447, 195055},
	{0x448, 196605},
};
struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
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


struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x386,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_SONY_MODE,
		.clk = 800,
		.lans = 4,
		.settle_time_apative_en = 0,
		.image_twidth = 3840,
		.image_theight = 2160,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 1,
		.mipi_sc.mipi_crop_start0x = 0,
		.mipi_sc.mipi_crop_start0y = 0,
		.mipi_sc.mipi_crop_start1x = 0,
		.mipi_sc.mipi_crop_start1y = 0,
		.mipi_sc.mipi_crop_start2x = 0,
		.mipi_sc.mipi_crop_start2y = 0,
		.mipi_sc.mipi_crop_start3x = 0,
		.mipi_sc.mipi_crop_start3y = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},

	.max_again = 196605,
	.max_again_short = 196605,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 0xae9 - 4,
	.min_integration_time_short = 1,
	.max_integration_time_short = 10,
	.integration_time_limit = 0xae9 - 4,
	.total_width = 0x12a3,
	.total_height = 0xae9,
	.max_integration_time = 0xae9 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};


static struct regval_list sensor_init_regs_3840_2160_15fps_mipi[] = {
	{0x0100, 0x00},
	{0x0101, 0x00},
	{0x0136, 0x18},
	{0x3A7D, 0x00},
	{0x3A7E, 0x01},
	{0x0137, 0x00},
	{0x3A7F, 0x05},
	{0x3100, 0x00},
	{0x3101, 0x40},
	{0x3102, 0x00},
	{0x3103, 0x10},
	{0x3104, 0x01},
	{0x3105, 0xE8},
	{0x3106, 0x01},
	{0x3107, 0xF0},
	{0x3150, 0x04},
	{0x3151, 0x03},
	{0x3152, 0x02},
	{0x3153, 0x01},
	{0x5A86, 0x00},
	{0x5A87, 0x82},
	{0x5D1A, 0x00},
	{0x5D95, 0x02},
	{0x5E1B, 0x00},
	{0x5F5A, 0x00},
	{0x5F5B, 0x04},
	{0x682C, 0x31},
	{0x6831, 0x31},
	{0x6835, 0x0E},
	{0x6836, 0x31},
	{0x6838, 0x30},
	{0x683A, 0x06},
	{0x683B, 0x33},
	{0x683D, 0x30},
	{0x6842, 0x31},
	{0x6844, 0x31},
	{0x6847, 0x31},
	{0x6849, 0x31},
	{0x684D, 0x0E},
	{0x684E, 0x32},
	{0x6850, 0x31},
	{0x6852, 0x06},
	{0x6853, 0x33},
	{0x6855, 0x31},
	{0x685A, 0x32},
	{0x685C, 0x33},
	{0x685F, 0x31},
	{0x6861, 0x33},
	{0x6865, 0x0D},
	{0x6866, 0x33},
	{0x6868, 0x31},
	{0x686B, 0x34},
	{0x686D, 0x31},
	{0x6872, 0x32},
	{0x6877, 0x33},
	{0x7FF0, 0x01},
	{0x7FF4, 0x08},
	{0x7FF5, 0x3C},
	{0x7FFA, 0x01},
	{0x7FFD, 0x00},
	{0x831E, 0x00},
	{0x831F, 0x00},
	{0x9301, 0xBD},
	{0x9B94, 0x03},
	{0x9B95, 0x00},
	{0x9B96, 0x08},
	{0x9B97, 0x00},
	{0x9B98, 0x0A},
	{0x9B99, 0x00},
	{0x9BA7, 0x18},
	{0x9BA8, 0x18},
	{0x9D04, 0x08},
	{0x9D50, 0x8C},
	{0x9D51, 0x64},
	{0x9D52, 0x50},
	{0x9E31, 0x04},
	{0x9E32, 0x04},
	{0x9E33, 0x04},
	{0x9E34, 0x04},
	{0xA200, 0x00},
	{0xA201, 0x0A},
	{0xA202, 0x00},
	{0xA203, 0x0A},
	{0xA204, 0x00},
	{0xA205, 0x0A},
	{0xA206, 0x01},
	{0xA207, 0xC0},
	{0xA208, 0x00},
	{0xA209, 0xC0},
	{0xA20C, 0x00},
	{0xA20D, 0x0A},
	{0xA20E, 0x00},
	{0xA20F, 0x0A},
	{0xA210, 0x00},
	{0xA211, 0x0A},
	{0xA212, 0x01},
	{0xA213, 0xC0},
	{0xA214, 0x00},
	{0xA215, 0xC0},
	{0xA300, 0x00},
	{0xA301, 0x0A},
	{0xA302, 0x00},
	{0xA303, 0x0A},
	{0xA304, 0x00},
	{0xA305, 0x0A},
	{0xA306, 0x01},
	{0xA307, 0xC0},
	{0xA308, 0x00},
	{0xA309, 0xC0},
	{0xA30C, 0x00},
	{0xA30D, 0x0A},
	{0xA30E, 0x00},
	{0xA30F, 0x0A},
	{0xA310, 0x00},
	{0xA311, 0x0A},
	{0xA312, 0x01},
	{0xA313, 0xC0},
	{0xA314, 0x00},
	{0xA315, 0xC0},
	{0xBC19, 0x01},
	{0xBC1C, 0x0A},
	{0x3035, 0x01},
	{0x3051, 0x00},
	{0x7F47, 0x00},
	{0x7F78, 0x00},
	{0x7F89, 0x00},
	{0x7F93, 0x00},
	{0x7FB4, 0x00},
	{0x7FCC, 0x01},
	{0x9D02, 0x00},
	{0x9D44, 0x8C},
	{0x9D62, 0x8C},
	{0x9D63, 0x50},
	{0x9D64, 0x1B},
	{0x9E0D, 0x00},
	{0x9E0E, 0x00},
	{0x9E15, 0x0A},
	{0x9F02, 0x00},
	{0x9F03, 0x23},
	{0x9F4E, 0x00},
	{0x9F4F, 0x42},
	{0x9F54, 0x00},
	{0x9F55, 0x5A},
	{0x9F6E, 0x00},
	{0x9F6F, 0x10},
	{0x9F72, 0x00},
	{0x9F73, 0xC8},
	{0x9F74, 0x00},
	{0x9F75, 0x32},
	{0x9FD3, 0x00},
	{0x9FD4, 0x00},
	{0x9FD5, 0x00},
	{0x9FD6, 0x3C},
	{0x9FD7, 0x3C},
	{0x9FD8, 0x3C},
	{0x9FD9, 0x00},
	{0x9FDA, 0x00},
	{0x9FDB, 0x00},
	{0x9FDC, 0xFF},
	{0x9FDD, 0xFF},
	{0x9FDE, 0xFF},
	{0xA002, 0x00},
	{0xA003, 0x14},
	{0xA04E, 0x00},
	{0xA04F, 0x2D},
	{0xA054, 0x00},
	{0xA055, 0x40},
	{0xA06E, 0x00},
	{0xA06F, 0x10},
	{0xA072, 0x00},
	{0xA073, 0xC8},
	{0xA074, 0x00},
	{0xA075, 0x32},
	{0xA0CA, 0x04},
	{0xA0CB, 0x04},
	{0xA0CC, 0x04},
	{0xA0D3, 0x0A},
	{0xA0D4, 0x0A},
	{0xA0D5, 0x0A},
	{0xA0D6, 0x00},
	{0xA0D7, 0x00},
	{0xA0D8, 0x00},
	{0xA0D9, 0x18},
	{0xA0DA, 0x18},
	{0xA0DB, 0x18},
	{0xA0DC, 0x00},
	{0xA0DD, 0x00},
	{0xA0DE, 0x00},
	{0xBCB2, 0x01},
	{0x4041, 0x00},
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0301, 0x04},
	{0x0303, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x36},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030D, 0x0C},
	{0x030E, 0x03},
	{0x030F, 0x20},
	//{0x0310, 0x01},
	//{0x0342, 0x10},
	//{0x0343, 0xC8},
	//{0x0340, 0x08},//0x0c
	//{0x0341, 0xd0},//0x1e

	{0x0342, 0x12},
	{0x0343, 0xA3},
	{0x0340, 0x0a},
	{0x0341, 0xE9},

	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x0e},
	{0x0349, 0xfF},
	{0x034A, 0x08},
	{0x034B, 0x6f},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x300D, 0x00},
	{0x302E, 0x01},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x040C, 0x0F},
	{0x040D, 0x00},
	{0x040E, 0x08},
	{0x040F, 0x70},
	{0x034C, 0x0F},
	{0x034D, 0xC0},
	{0x034E, 0x0b},
	{0x034F, 0xc8},
	{0x0114, 0x01},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x0902, 0x00},
	{0x3030, 0x00},
	{0x3031, 0x01},
	{0x3032, 0x00},
	{0x3047, 0x01},
	{0x3049, 0x01},
	{0x30E6, 0x00},
	{0x30E7, 0x00},
	{0x4E25, 0x80},
	{0x663A, 0x02},
	{0x9311, 0x00},
	{0xA0CD, 0x19},
	{0xA0CE, 0x19},
	{0xA0CF, 0x19},
	{0x0202, 0x0C},
	{0x0203, 0x14},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};


static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 3840,
		.height = 2160,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_15fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
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

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x0016, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x0017, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shr0 = 0;
	unsigned short vmax = 0;
	int it = (value & 0xffff);
	vmax = sensor_attr.total_height;
	shr0 = it;
	ret = sensor_write(sd, 0x0104, 0x01);
	ret = sensor_write(sd, 0x0350, 0x01);
	ret = sensor_write(sd, 0x0203, (unsigned char) (shr0 & 0xff));
	ret += sensor_write(sd, 0x0202, (unsigned char) ((shr0 >> 8) & 0xff));
	ret = sensor_write(sd, 0x0104, 0x00);
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	int again = (value & 0xffff0000) >> 16;
	ret = sensor_write(sd, 0x0104, 0x01);

	ret += sensor_write(sd, 0x0205, (unsigned char) (again & 0xff));
	ret += sensor_write(sd, 0x0204, (unsigned char) (((again >> 8) & 0xff)));
	ret = sensor_write(sd, 0x0104, 0x00);
	if (ret < 0)
		return ret;
	return 0;
}

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

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING) {

			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}
	} else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char tmp;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch (sensor->info.default_boot) {
		case 0:
			sclk = 199881045;
			max_fps = TX_SENSOR_MAX_FPS_15;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x0342, &val);
	hts = val << 8;
	ret += sensor_read(sd, 0x0343, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}
	hts = (hts << 8) + tmp;//////
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x3001, 0x01);
	ret += sensor_write(sd, 0x0340, (unsigned char) ((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x0341, (unsigned char) (vts & 0xff));
	ret += sensor_write(sd, 0x3001, 0x00);
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 9;
	sensor->video.attr->integration_time_limit = vts - 9;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 9;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

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

#if 0
static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sensor_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		sensor_write(sd, 0x3221, val & 0x99);
		break;
	case 1:
		sensor_write(sd, 0x3221, val | 0x06);
		break;
	case 2:
		sensor_write(sd, 0x3221, val | 0x60);
		break;
	case 3:
		sensor_write(sd, 0x3221, val | 0x66);
		break;
	}

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret;

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			sensor_attr.mipi.clk = 800;
			sensor_attr.total_width = 0x12a3;
			sensor_attr.total_height = 0xae9;
			sensor_attr.max_integration_time = 0xae9 - 4;
			sensor_attr.integration_time_limit = 0xae9 - 4;
			sensor_attr.max_integration_time_native = 0xae9 - 4;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x424;
			break;
		default:
			ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch (info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 0;
			break;
		case TISP_SENSOR_VI_DVP:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
			break;
		default:
			ISP_ERROR("Have no this Interface Source!!!\n");
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
	switch (info->default_boot) {
		case 0:
		case 1:
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
			private_clk_set_rate(sensor->mclk, 2400000);
			private_clk_prepare_enable(sensor->mclk);
			break;
	}

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot,
		    wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	return 0;

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
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 0);
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

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_EXPO:
			//	if (arg)
			//		ret = sensor_set_expo(sd, sensor_val->value);
			//	break;
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
				ret = sensor_set_integration_time(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = sensor_set_analog_gain(sd, sensor_val->value);
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
			//	if (arg)
			//		ret = sensor_set_vflip(sd, sensor_val->value);
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
	sensor->video.shvflip = 1;
	sensor->video.attr = &sensor_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

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
