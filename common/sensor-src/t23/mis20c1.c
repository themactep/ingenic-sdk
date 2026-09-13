// SPDX-License-Identifier: GPL-2.0+
/*
 * mis20c1.c - ImageDesign MIS20C1 sensor driver
 *
 * Register programming and runtime behavior recovered from the vendor
 * sensor_mis20c1_t23.ko (version H20240321a).
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

#define SENSOR_NAME                 "mis20c1"
#define SENSOR_VERSION              "H20240321a"
#define SENSOR_CHIP_ID_H            0x20
#define SENSOR_CHIP_ID_L            0xc1
#define SENSOR_CHIP_ID              0x20c1
#define SENSOR_I2C_ADDRESS          0x30

#define SENSOR_REG_END              0xffff
#define SENSOR_REG_DELAY            0xfffe

#define SENSOR_MAX_WIDTH            1920
#define SENSOR_MAX_HEIGHT           1080
#define SENSOR_SUPPORT_30FPS_PCLK   74385000U
#define SENSOR_OUTPUT_MAX_FPS       30
#define SENSOR_OUTPUT_MIN_FPS       5

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

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
	.actual_fps = SENSOR_OUTPUT_MAX_FPS,
	.chip_i2c_addr = SENSOR_I2C_ADDRESS,
	.width = SENSOR_MAX_WIDTH,
	.height = SENSOR_MAX_HEIGHT,
};

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

static struct again_lut mis20c1_again_lut[] = {
/* Exact vendor gain-code to ISP log2-gain mapping. */
	{0x0, 0},
	{0x10, 1465},
	{0x20, 2998},
	{0x30, 4506},
	{0x40, 6077},
	{0x50, 7623},
	{0x60, 9228},
	{0x70, 10888},
	{0x80, 12601},
	{0x90, 14283},
	{0xa0, 16014},
	{0xb0, 17789},
	{0xc0, 19607},
	{0xd0, 21465},
	{0xe0, 23287},
	{0xf0, 25216},
	{0x100, 27175},
	{0x108, 28140},
	{0x110, 29164},
	{0x118, 30175},
	{0x120, 31177},
	{0x128, 32233},
	{0x130, 33277},
	{0x138, 34311},
	{0x140, 35396},
	{0x148, 36470},
	{0x150, 37593},
	{0x158, 38703},
	{0x160, 39801},
	{0x168, 40945},
	{0x170, 42076},
	{0x178, 43252},
	{0x180, 44413},
	{0x188, 45618},
	{0x190, 46808},
	{0x198, 48037},
	{0x1a0, 49252},
	{0x1a8, 50505},
	{0x1b0, 51794},
	{0x1b8, 53068},
	{0x1c0, 54375},
	{0x1c8, 55716},
	{0x1d0, 57039},
	{0x1d8, 58392},
	{0x1e0, 59776},
	{0x1e8, 61189},
	{0x1f0, 62581},
	{0x1f8, 64046},
	{0x200, 65536},
	{0x204, 66271},
	{0x208, 67001},
	{0x20c, 67769},
	{0x210, 68534},
	{0x214, 69290},
	{0x218, 70042},
	{0x21c, 70831},
	{0x220, 71613},
	{0x224, 72390},
	{0x228, 73201},
	{0x22c, 74007},
	{0x230, 74805},
	{0x234, 75639},
	{0x238, 76466},
	{0x23c, 77284},
	{0x240, 78137},
	{0x244, 78982},
	{0x248, 79858},
	{0x24c, 80688},
	{0x250, 81588},
	{0x254, 82441},
	{0x258, 83364},
	{0x25c, 84239},
	{0x260, 85143},
	{0x264, 86076},
	{0x268, 87001},
	{0x26c, 87916},
	{0x270, 88859},
	{0x274, 89792},
	{0x278, 90752},
	{0x27c, 91737},
	{0x280, 92711},
	{0x284, 93710},
	{0x288, 94700},
	{0x28c, 95711},
	{0x290, 96745},
	{0x294, 97769},
	{0x298, 98813},
	{0x29c, 99879},
	{0x2a0, 100932},
	{0x2a4, 102037},
	{0x2a8, 103129},
	{0x2ac, 104239},
	{0x2b0, 105337},
	{0x2b4, 106481},
	{0x2b8, 107612},
	{0x2bc, 108788},
	{0x2c0, 109949},
	{0x2c4, 111154},
	{0x2c8, 112344},
	{0x2cc, 113573},
	{0x2d0, 114815},
	{0x2d4, 116067},
	{0x2d8, 117330},
	{0x2dc, 118630},
	{0x2e0, 119911},
	{0x2e4, 121252},
	{0x2e8, 122575},
	{0x2ec, 123954},
	{0x2f0, 125337},
	{0x2f4, 126725},
	{0x2f8, 128141},
	{0x2fc, 129582},
	{0x300, 131072},
	{0x302, 131807},
	{0x304, 132559},
	{0x306, 133305},
	{0x308, 134070},
	{0x30a, 134826},
	{0x30c, 135599},
	{0x30e, 136367},
	{0x310, 137171},
	{0x312, 137947},
	{0x314, 138760},
	{0x316, 139564},
	{0x318, 140363},
	{0x31a, 141196},
	{0x31c, 142022},
	{0x31e, 142840},
	{0x320, 143693},
	{0x322, 144537},
	{0x324, 145394},
	{0x326, 146243},
	{0x328, 147124},
	{0x32a, 147996},
	{0x32c, 148900},
	{0x32e, 149793},
	{0x330, 150698},
	{0x332, 151612},
	{0x334, 152537},
	{0x336, 153452},
	{0x338, 154395},
	{0x33a, 155346},
	{0x33c, 156306},
	{0x33e, 157290},
	{0x340, 158265},
	{0x342, 159246},
	{0x344, 160252},
	{0x346, 161264},
	{0x348, 162281},
	{0x34a, 163321},
	{0x34c, 164366},
	{0x34e, 165415},
	{0x350, 166484},
	{0x352, 167573},
	{0x354, 168665},
	{0x356, 169775},
	{0x358, 170888},
	{0x35a, 172017},
	{0x35c, 173162},
	{0x35e, 174324},
	{0x360, 175500},
	{0x362, 176690},
	{0x364, 177894},
	{0x366, 179109},
	{0x368, 180351},
	{0x36a, 181603},
	{0x36c, 182866},
	{0x36e, 184166},
	{0x370, 185460},
	{0x372, 186788},
	{0x374, 188123},
	{0x376, 189490},
	{0x378, 190873},
	{0x37a, 192273},
	{0x37c, 193688},
	{0x37e, 195129},
	{0x380, 196608},
	{0x382, 198095},
	{0x384, 199606},
	{0x386, 201135},
	{0x388, 202707},
	{0x38a, 204296},
	{0x38c, 205909},
	{0x38e, 207558},
	{0x390, 209229},
	{0x392, 210930},
	{0x394, 212670},
	{0x396, 214436},
	{0x398, 216234},
	{0x39a, 218073},
	{0x39c, 219940},
	{0x39e, 221850},
	{0x3a0, 223801},
	{0x3a2, 225796},
	{0x3a4, 227826},
	{0x3a6, 229902},
	{0x3a8, 232028},
	{0x3aa, 234201},
	{0x3ac, 236432},
	{0x3ae, 238706},
	{0x3b0, 241043},
	{0x3b2, 243436},
	{0x3b4, 245894},
	{0x3b6, 248409},
	{0x3b8, 251003},
	{0x3ba, 253666},
	{0x3bc, 256409},
	{0x3be, 259230},
	{0x3c0, 262144},
	{0x3c1, 263631},
	{0x3c2, 265142},
	{0x3c3, 266678},
	{0x3c4, 268243},
	{0x3c5, 269832},
	{0x3c6, 271445},
	{0x3c7, 273094},
	{0x3c8, 274765},
	{0x3c9, 276471},
	{0x3ca, 278206},
	{0x3cb, 279972},
	{0x3cc, 281770},
	{0x3cd, 283609},
	{0x3ce, 285481},
	{0x3cf, 287391},
	{0x3d0, 289341},
	{0x3d1, 291332},
	{0x3d2, 293366},
	{0x3d3, 295442},
	{0x3d4, 297567},
	{0x3d5, 299741},
	{0x3d6, 301968},
	{0x3d7, 304245},
	{0x3d8, 306579},
	{0x3d9, 308972},
	{0x3da, 311430},
	{0x3db, 313949},
	{0x3dc, 316542},
	{0x3dd, 319205},
	{0x3de, 321945},
	{0x3df, 324769},
	{0x3e0, 327680},
	{0x3e1, 330682},
	{0x3e2, 333782},
	{0x3e3, 336987},
	{0x3e4, 340305},
	{0x3e5, 343744},
	{0x3e6, 347312},
	{0x3e7, 351020},
	{0x3e8, 354880},
	{0x3e9, 358904},
	{0x3ea, 363107},
	{0x3eb, 367505},
	{0x3ec, 372118},
	{0x3ed, 376968},
	{0x3ee, 382080},
	{0x3ef, 387484},
	{0x3f0, 393216},
	{0x3f1, 399318},
	{0x3f2, 405841},
	{0x3f3, 412848},
	{0x3f4, 420416},
	{0x3f5, 428643},
	{0x3f6, 437654},
	{0x3f7, 447616},
	{0x3f8, 458752},
	{0x3f9, 471377},
	{0x3fa, 485952},
	{0x3fb, 503190},
	{0x3fc, 524288},
	{0x3fd, 551488},
	{0x3fe, 589824},
	{0x3ff, 655360},
};

static unsigned int mis20c1_alloc_again(unsigned int isp_gain,
		unsigned char shift, unsigned int *sensor_again);
static unsigned int mis20c1_alloc_dgain(unsigned int isp_gain,
		unsigned char shift, unsigned int *sensor_dgain);

struct tx_isp_sensor_attribute mis20c1_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS |
		V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 372,
		.lans = 2,
		.image_twidth = SENSOR_MAX_WIDTH,
		.image_theight = SENSOR_MAX_HEIGHT,
		.mipi_sc = {
			.data_type_value = RAW10,
			.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
			.sensor_mode = TX_SENSOR_DEFAULT_MODE,
			.sensor_csi_fmt = TX_SENSOR_RAW10,
		},
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 327680,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1124,
	.integration_time_limit = 1124,
	.total_width = 2204,
	.total_height = 1125,
	.max_integration_time = 1124,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.one_line_expr_in_us = 29,
	.sensor_ctrl.alloc_again = mis20c1_alloc_again,
	.sensor_ctrl.alloc_dgain = mis20c1_alloc_dgain,
};

static unsigned int mis20c1_alloc_again(unsigned int isp_gain,
		unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = mis20c1_again_lut;

	while (lut->gain <= mis20c1_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		}
		if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		if (lut->gain == mis20c1_attr.max_again &&
		    isp_gain >= lut->gain) {
			*sensor_again = lut->value;
			return lut->gain;
		}
		lut++;
	}

	return isp_gain;
}

static unsigned int mis20c1_alloc_dgain(unsigned int isp_gain,
		unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

static struct regval_list mis20c1_init_regs_1920_1080_30fps_mipi[] = {
/* Exact vendor 1920x1080, 30 fps, 2-lane RAW10 register program. */
	{0x3006, 0x02},
	{0xfffe, 0x01},
	{0x3038, 0x01},
	{0x3106, 0x65},
	{0x3105, 0x04},
	{0x3108, 0x9c},
	{0x3107, 0x08},
	{0x310a, 0x02},
	{0x3109, 0x00},
	{0x310c, 0x39},
	{0x310b, 0x04},
	{0x310e, 0x04},
	{0x310d, 0x00},
	{0x3110, 0x83},
	{0x310f, 0x07},
	{0x3039, 0x00},
	{0x2102, 0x00},
	{0x3300, 0x7c},
	{0x3301, 0x01},
	{0x3303, 0x07},
	{0x330d, 0x00},
	{0x3302, 0x02},
	{0x330b, 0x01},
	{0x330c, 0x03},
	{0x3312, 0x07},
	{0x330a, 0x08},
	{0x3314, 0x1b},
	{0x3313, 0x00},
	{0x4001, 0x4b},
	{0x4000, 0x00},
	{0x4003, 0x2b},
	{0x4002, 0x07},
	{0x4005, 0xff},
	{0x4004, 0x1f},
	{0x4007, 0xff},
	{0x4006, 0x1f},
	{0x4009, 0x48},
	{0x4008, 0x02},
	{0x400b, 0xd9},
	{0x400a, 0x03},
	{0x400d, 0xff},
	{0x400c, 0x1f},
	{0x400f, 0xff},
	{0x400e, 0x1f},
	{0x4011, 0x01},
	{0x4010, 0x00},
	{0x4013, 0x08},
	{0x4012, 0x00},
	{0x4015, 0x00},
	{0x4014, 0x00},
	{0x4017, 0x31},
	{0x4016, 0x01},
	{0x4019, 0x4b},
	{0x4018, 0x00},
	{0x401b, 0x57},
	{0x401a, 0x02},
	{0x401d, 0x00},
	{0x401c, 0x00},
	{0x401f, 0x2b},
	{0x401e, 0x07},
	{0x4021, 0x03},
	{0x4020, 0x00},
	{0x4023, 0x2b},
	{0x4022, 0x07},
	{0x4025, 0x01},
	{0x4024, 0x00},
	{0x4027, 0x2b},
	{0x4026, 0x07},
	{0x4029, 0x08},
	{0x4028, 0x00},
	{0x402b, 0x23},
	{0x402a, 0x07},
	{0x402d, 0x00},
	{0x402c, 0x00},
	{0x402f, 0x4f},
	{0x402e, 0x02},
	{0x4031, 0x52},
	{0x4030, 0x00},
	{0x4033, 0xe7},
	{0x4032, 0x00},
	{0x4035, 0x52},
	{0x4034, 0x00},
	{0x4037, 0xdf},
	{0x4036, 0x00},
	{0x4039, 0x08},
	{0x4038, 0x00},
	{0x403b, 0x04},
	{0x403a, 0x01},
	{0x403d, 0x04},
	{0x403c, 0x01},
	{0x403f, 0x13},
	{0x403e, 0x01},
	{0x4041, 0x48},
	{0x4040, 0x02},
	{0x4043, 0x57},
	{0x4042, 0x02},
	{0x4045, 0xff},
	{0x4044, 0x1f},
	{0x4047, 0xff},
	{0x4046, 0x1f},
	{0x4049, 0xff},
	{0x4048, 0x1f},
	{0x404b, 0xff},
	{0x404a, 0x1f},
	{0x404d, 0x13},
	{0x404c, 0x01},
	{0x404f, 0x22},
	{0x404e, 0x01},
	{0x4051, 0xf7},
	{0x4050, 0x03},
	{0x4053, 0x05},
	{0x4052, 0x04},
	{0x4055, 0xff},
	{0x4054, 0x1f},
	{0x4057, 0xff},
	{0x4056, 0x1f},
	{0x4059, 0xff},
	{0x4058, 0x1f},
	{0x405b, 0xff},
	{0x405a, 0x1f},
	{0x405d, 0x40},
	{0x405c, 0x01},
	{0x405f, 0x40},
	{0x405e, 0x02},
	{0x4061, 0x23},
	{0x4060, 0x04},
	{0x4063, 0x23},
	{0x4062, 0x07},
	{0x4065, 0xff},
	{0x4064, 0x1f},
	{0x4067, 0xff},
	{0x4066, 0x1f},
	{0x4069, 0xff},
	{0x4068, 0x1f},
	{0x406b, 0xff},
	{0x406a, 0x1f},
	{0x406d, 0x31},
	{0x406c, 0x01},
	{0x406f, 0x40},
	{0x406e, 0x02},
	{0x4071, 0x14},
	{0x4070, 0x04},
	{0x4073, 0x23},
	{0x4072, 0x07},
	{0x4075, 0xff},
	{0x4074, 0x1f},
	{0x4077, 0xff},
	{0x4076, 0x1f},
	{0x4079, 0xff},
	{0x4078, 0x1f},
	{0x407b, 0xff},
	{0x407a, 0x1f},
	{0x407d, 0x00},
	{0x407c, 0x00},
	{0x407f, 0x68},
	{0x407e, 0x00},
	{0x4081, 0x31},
	{0x4080, 0x01},
	{0x4083, 0x48},
	{0x4082, 0x02},
	{0x4085, 0x14},
	{0x4084, 0x04},
	{0x4087, 0x2b},
	{0x4086, 0x07},
	{0x4089, 0x52},
	{0x4088, 0x00},
	{0x408b, 0x40},
	{0x408a, 0x01},
	{0x408d, 0x48},
	{0x408c, 0x02},
	{0x408f, 0x23},
	{0x408e, 0x04},
	{0x4091, 0xff},
	{0x4090, 0x1f},
	{0x4093, 0xff},
	{0x4092, 0x1f},
	{0x4095, 0xff},
	{0x4094, 0x1f},
	{0x4097, 0xff},
	{0x4096, 0x1f},
	{0x4099, 0x00},
	{0x4098, 0x00},
	{0x409b, 0x95},
	{0x409a, 0x00},
	{0x409d, 0x00},
	{0x409c, 0x00},
	{0x409f, 0x95},
	{0x409e, 0x00},
	{0x40a1, 0x00},
	{0x40a0, 0x00},
	{0x40a3, 0x95},
	{0x40a2, 0x00},
	{0x40a5, 0x95},
	{0x40a4, 0x00},
	{0x40a7, 0x9c},
	{0x40a6, 0x00},
	{0x40a9, 0x00},
	{0x40a8, 0x00},
	{0x40ab, 0xc0},
	{0x40aa, 0x01},
	{0x40ad, 0x8b},
	{0x40ac, 0x02},
	{0x40af, 0xf7},
	{0x40ae, 0x03},
	{0x40b1, 0x61},
	{0x40b0, 0x00},
	{0x40b3, 0x92},
	{0x40b2, 0x02},
	{0x40b5, 0x4f},
	{0x40b4, 0x02},
	{0x40b7, 0xe8},
	{0x40b6, 0x03},
	{0x40b9, 0x4f},
	{0x40b8, 0x02},
	{0x40bb, 0xe8},
	{0x40ba, 0x03},
	{0x40bd, 0xff},
	{0x40bc, 0x1f},
	{0x40bf, 0xff},
	{0x40be, 0x1f},
	{0x40c1, 0xff},
	{0x40c0, 0x1f},
	{0x40c3, 0xff},
	{0x40c2, 0x1f},
	{0x40c5, 0x00},
	{0x40c4, 0x00},
	{0x40c7, 0x5d},
	{0x40c6, 0x08},
	{0x40c9, 0xb8},
	{0x40c8, 0x07},
	{0x40cb, 0x5d},
	{0x40ca, 0x08},
	{0x40cd, 0xb8},
	{0x40cc, 0x07},
	{0x40cf, 0x5d},
	{0x40ce, 0x08},
	{0x40d1, 0x00},
	{0x40d0, 0x00},
	{0x40d3, 0x03},
	{0x40d2, 0x00},
	{0x40e1, 0xc0},
	{0x40e0, 0x01},
	{0x40e3, 0x54},
	{0x40e2, 0x02},
	{0x40e5, 0xa3},
	{0x40e4, 0x04},
	{0x40e7, 0x37},
	{0x40e6, 0x07},
	{0x40e9, 0xff},
	{0x40e8, 0x1f},
	{0x40eb, 0xff},
	{0x40ea, 0x1f},
	{0x40ed, 0xff},
	{0x40ec, 0x1f},
	{0x40ef, 0xff},
	{0x40ee, 0x1f},
	{0x40d5, 0x7b},
	{0x40d4, 0x08},
	{0x40dd, 0x00},
	{0x40df, 0x00},
	{0x4102, 0x0c},
	{0x4103, 0x1f},
	{0x40a8, 0x00},
	{0x40a9, 0x00},
	{0x40aa, 0x01},
	{0x40ab, 0xc0},
	{0x3a0a, 0x0b},
	{0x3a0b, 0x0f},
	{0x3111, 0x01},
	{0x3a04, 0x3d},
	{0x3116, 0x00},
	{0x4102, 0x0c},
	{0x4103, 0x1f},
	{0x3a0b, 0x0d},
	{0x3707, 0x00},
	{0x3708, 0x40},
	{0x3a07, 0x27},
	{0x3a0d, 0x01},
	{0x3a10, 0x0c},
	{0x4103, 0x07},
	{0x3b00, 0x07},
	{0x3b01, 0xff},
	{0x3b03, 0xff},
	{0x4100, 0x09},
	{0x4101, 0x09},
	{0x3a03, 0x3e},
	{0x3a05, 0x26},
	{0x3a06, 0x01},
	{0x3a08, 0x09},
	{0x3a0b, 0x05},
	{0x3a0c, 0x08},
	{0x3a12, 0xe0},
	{0x3a16, 0x85},
	{0x3a17, 0x20},
	{0x3a19, 0x19},
	{0x3311, 0x01},
	{0x3310, 0x00},
	{0x3306, 0x04},
	{0x3a1e, 0x40},
	{0x4100, 0x00},
	{0x3a1b, 0x1a},
	{0x410b, 0x1b},
	{0x3a04, 0x3d},
	{0x4102, 0x0f},
	{0x3a1f, 0x48},
	{0x4100, 0x09},
	{0x3a08, 0x08},
	{0x4101, 0x23},
	{0x3a0a, 0x0a},
	{0x3400, 0x01},
	{0x3a0b, 0x0e},
	{0x3a07, 0x25},
	{0x302e, 0x01},
	{0x3a04, 0x2d},
	{0x3800, 0x02},
	{0x3029, 0x01},
	{0x302e, 0x01},
	{0x3008, 0x01},
	{0x3116, 0x00},
	{0x3006, 0x00},
	{0x3022, 0x01},
	{0xffff, 0x00},
};

/* The vendor driver intentionally leaves stream transitions to init. */
static struct regval_list mis20c1_stream_on[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list mis20c1_stream_off[] = {
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting mis20c1_win_sizes[] = {
	{
		.width = SENSOR_MAX_WIDTH,
		.height = SENSOR_MAX_HEIGHT,
		.fps = (SENSOR_OUTPUT_MAX_FPS << 16) | 1,
		.mbus_code = V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = mis20c1_init_regs_1920_1080_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &mis20c1_win_sizes[0];

static int mis20c1_read(struct tx_isp_subdev *sd, u16 reg, u8 *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	u8 buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		},
	};
	int ret;

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int mis20c1_write(struct tx_isp_subdev *sd, u16 reg, u8 value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	u8 buf[3] = {reg >> 8, reg & 0xff, value};
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

static int mis20c1_write_array(struct tx_isp_subdev *sd,
		struct regval_list *values)
{
	int ret;

	while (values->reg_num != SENSOR_REG_END) {
		if (values->reg_num == SENSOR_REG_DELAY) {
			private_msleep(values->value);
		} else {
			ret = mis20c1_write(sd, values->reg_num, values->value);
			if (ret < 0)
				return ret;
		}
		values++;
	}

	return 0;
}

static int mis20c1_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int mis20c1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	u8 value;
	int ret;

	ret = mis20c1_read(sd, 0x3000, &value);
	if (ret < 0)
		return ret;
	if (value != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = value;

	ret = mis20c1_read(sd, 0x3001, &value);
	if (ret < 0)
		return ret;
	if (value != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | value;

	return 0;
}

static int mis20c1_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret;

	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, SENSOR_NAME "_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request fail %d\n", reset_gpio);
		}
	}

	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, SENSOR_NAME "_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request fail %d\n", pwdn_gpio);
		}
	}

	ret = mis20c1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not %s\n",
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

static int mis20c1_set_expo(struct tx_isp_subdev *sd, int value)
{
	unsigned int expo = value & 0xffff;
	unsigned int again = (value >> 16) & 0xffff;
	unsigned int vts;
	unsigned int margin;
	unsigned int statistic;
	u8 tmp = 0;
	int ret = 0;

	ret += mis20c1_write(sd, 0x3100, (expo >> 8) & 0xff);
	ret += mis20c1_write(sd, 0x3101, expo & 0xff);
	ret += mis20c1_write(sd, 0x3102, (again >> 8) & 0x03);
	ret += mis20c1_write(sd, 0x3103, again & 0xff);
	ret += mis20c1_write(sd, 0x3008, 0x01);

	ret += mis20c1_read(sd, 0x3105, &tmp);
	vts = tmp << 8;
	ret += mis20c1_read(sd, 0x3106, &tmp);
	vts |= tmp;

	margin = vts - expo;
	if (margin < 4) {
		ret += mis20c1_write(sd, 0x3114, 0x00);
		ret += mis20c1_write(sd, 0x3115, 0x01);
	} else {
		margin -= 4;
		ret += mis20c1_write(sd, 0x3114, (margin >> 8) & 0xff);
		ret += mis20c1_write(sd, 0x3115, margin & 0xff);
	}
	ret += mis20c1_write(sd, 0x3113, 0x03);

	if (expo & 0xfffe) {
		ret += mis20c1_write(sd, 0x3709, 0x00);
		ret += mis20c1_write(sd, 0x370a, 0x80);
		ret += mis20c1_write(sd, 0x3008, 0x01);
	} else {
		margin = vts - 4;
		ret += mis20c1_write(sd, 0x3709, 0x00);
		ret += mis20c1_write(sd, 0x370a, 0x97);
		ret += mis20c1_write(sd, 0x3008, 0x01);
		ret += mis20c1_write(sd, 0x3114, (margin >> 8) & 0xff);
		ret += mis20c1_write(sd, 0x3115, margin & 0xff);
		ret += mis20c1_write(sd, 0x3113, 0x03);
	}

	ret += mis20c1_read(sd, 0x3402, &tmp);
	statistic = tmp << 8;
	ret += mis20c1_read(sd, 0x3403, &tmp);
	statistic |= tmp;

	ret += mis20c1_write(sd, 0x3800,
		(statistic >= 0x0b01 && again >= 0x0380) ? 0x02 : 0x00);
	ret += mis20c1_write(sd, 0x3a1b, again < 0x0300 ? 0x1a : 0x16);

	if (statistic < 0x0b01) {
		ret += mis20c1_write(sd, 0x3a0f, 0x0f);
		ret += mis20c1_write(sd, 0x3606, 0x10);
		ret += mis20c1_write(sd, 0x3607, 0x3a);
	} else {
		ret += mis20c1_write(sd, 0x3a0f, 0x09);
		ret += mis20c1_write(sd, 0x3606, 0x40);
		ret += mis20c1_write(sd, 0x3607, 0x40);
	}
	ret += mis20c1_write(sd, 0x3029, 0x01);

	return ret;
}

static int mis20c1_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret;

	if (!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	ret = mis20c1_write_array(sd, wsize->regs);
	if (ret)
		return ret;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR,
		&sensor->video);
	sensor->priv = wsize;

	return ret;
}

static int mis20c1_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret;

	if (data_interface != TX_SENSOR_DATA_INTERFACE_MIPI) {
		ISP_ERROR("unsupported data interface %d\n", data_interface);
		return -EINVAL;
	}

	ret = mis20c1_write_array(sd,
		enable ? mis20c1_stream_on : mis20c1_stream_off);
	pr_debug("%s stream %s\n", SENSOR_NAME, enable ? "on" : "off");

	return ret;
}

static int mis20c1_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int numerator = fps >> 16;
	unsigned int denominator = fps & 0xffff;
	unsigned int newformat;
	unsigned int hts;
	unsigned int vts;
	u8 tmp;
	int ret;

	if (!numerator || !denominator)
		return -EINVAL;

	newformat = (numerator / denominator) << 8;
	newformat += ((numerator % denominator) << 8) / denominator;
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) ||
	    newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -ERANGE;
	}

	ret = mis20c1_read(sd, 0x3107, &tmp);
	hts = tmp << 8;
	ret += mis20c1_read(sd, 0x3108, &tmp);
	hts |= tmp;
	if (ret < 0 || !hts) {
		ISP_ERROR("err: %s read hts failed\n", SENSOR_NAME);
		return ret < 0 ? ret : -EINVAL;
	}

	vts = SENSOR_SUPPORT_30FPS_PCLK * denominator / hts / numerator;
	ret = mis20c1_write(sd, 0x3106, vts & 0xff);
	ret += mis20c1_write(sd, 0x3105, (vts >> 8) & 0xff);
	if (ret < 0) {
		ISP_ERROR("err: %s write vts failed\n", SENSOR_NAME);
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 1;
	sensor->video.attr->integration_time_limit = vts - 1;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 1;
	sensor_info.actual_fps = numerator / denominator;

	return tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR,
		&sensor->video);
}

static int mis20c1_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	return tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR,
		&sensor->video);
}

static int mis20c1_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	u8 reg_3007;
	u8 reg_310a;
	u8 reg_310c;
	u8 reg_310e;
	u8 reg_3110;
	int ret = 0;

	switch (enable) {
	case 1:
		reg_3007 = 0x01;
		reg_310a = 0x02;
		reg_310c = 0x39;
		reg_310e = 0x05;
		reg_3110 = 0x84;
		break;
	case 2:
		reg_3007 = 0x02;
		reg_310a = 0x03;
		reg_310c = 0x3a;
		reg_310e = 0x04;
		reg_3110 = 0x83;
		break;
	case 3:
		reg_3007 = 0x03;
		reg_310a = 0x03;
		reg_310c = 0x3a;
		reg_310e = 0x05;
		reg_3110 = 0x84;
		break;
	case 0:
	default:
		reg_3007 = 0x00;
		reg_310a = 0x02;
		reg_310c = 0x39;
		reg_310e = 0x04;
		reg_3110 = 0x83;
		break;
	}

	ret += mis20c1_write(sd, 0x3006, 0x02);
	private_msleep(30);
	ret += mis20c1_write(sd, 0x3007, reg_3007);
	ret += mis20c1_write(sd, 0x310a, reg_310a);
	ret += mis20c1_write(sd, 0x310c, reg_310c);
	ret += mis20c1_write(sd, 0x310e, reg_310e);
	ret += mis20c1_write(sd, 0x3110, reg_3110);
	ret += mis20c1_write(sd, 0x3006, 0x00);

	*(volatile u32 *)0xb3380000 = 0x5;

	return ret;
}

static int mis20c1_set_logic(struct tx_isp_subdev *sd, int value)
{
	u8 state = 0;
	u8 high = 0;
	u8 low = 0;
	unsigned int statistic;
	int ret = 0;

	ret += mis20c1_read(sd, 0x3016, &state);
	ret += mis20c1_read(sd, 0x3636, &high);
	ret += mis20c1_read(sd, 0x3637, &low);
	statistic = (high << 8) | low;
	ret += mis20c1_write(sd, 0x3400,
		(state == 1 && statistic >= 0x0110) ? 0x00 : 0x01);

	return ret;
}

static int mis20c1_sensor_ops_ioctl(struct tx_isp_subdev *sd,
		unsigned int cmd, void *arg)
{
	long ret = 0;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = mis20c1_set_expo(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = mis20c1_set_mode(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = mis20c1_write_array(sd, mis20c1_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = mis20c1_write_array(sd, mis20c1_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = mis20c1_set_fps(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = mis20c1_set_vflip(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if (arg)
			ret = mis20c1_set_logic(sd, *(int *)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int mis20c1_g_register(struct tx_isp_subdev *sd,
		struct tx_isp_dbg_register *reg)
{
	u8 value = 0;
	int len;
	int ret;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
		return -EINVAL;
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	ret = mis20c1_read(sd, reg->reg & 0xffff, &value);
	reg->val = value;
	reg->size = 2;

	return ret;
}

static int mis20c1_s_register(struct tx_isp_subdev *sd,
		const struct tx_isp_dbg_register *reg)
{
	int len;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
		return -EINVAL;
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	return mis20c1_write(sd, reg->reg & 0xffff, reg->val & 0xff);
}

static struct tx_isp_subdev_core_ops mis20c1_core_ops = {
	.g_chip_ident = mis20c1_g_chip_ident,
	.reset = mis20c1_reset,
	.init = mis20c1_init,
	.g_register = mis20c1_g_register,
	.s_register = mis20c1_s_register,
};

static struct tx_isp_subdev_video_ops mis20c1_video_ops = {
	.s_stream = mis20c1_s_stream,
};

static struct tx_isp_subdev_sensor_ops mis20c1_sensor_ops = {
	.ioctl = mis20c1_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops mis20c1_ops = {
	.core = &mis20c1_core_ops,
	.video = &mis20c1_video_ops,
	.sensor = &mis20c1_sensor_ops,
};

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

static int mis20c1_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct tx_isp_sensor *sensor;
	struct tx_isp_subdev *sd;
	int ret;

	if (data_interface != TX_SENSOR_DATA_INTERFACE_MIPI) {
		ISP_ERROR("%s only supports the MIPI interface\n", SENSOR_NAME);
		return -EINVAL;
	}

	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

#ifdef CONFIG_KERNEL_4_4_94
	sensor->mclk = clk_get(NULL, "div_cim");
#else
	sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
	if (IS_ERR(sensor->mclk)) {
		ret = PTR_ERR(sensor->mclk);
		ISP_ERROR("Cannot get %s input clock: %d\n", SENSOR_NAME, ret);
		goto err_free_sensor;
	}

	ret = private_clk_set_rate(sensor->mclk, 24000000);
	if (ret)
		goto err_put_mclk;
	ret = private_clk_enable(sensor->mclk);
	if (ret)
		goto err_put_mclk;

	sd = &sensor->sd;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &mis20c1_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor_info.actual_fps = SENSOR_OUTPUT_MAX_FPS;

	tx_isp_subdev_init(&sensor_platform_device, sd, &mis20c1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);
	return 0;

err_put_mclk:
	private_clk_put(sensor->mclk);
err_free_sensor:
	kfree(sensor);
	return ret;
}

static int mis20c1_remove(struct i2c_client *client)
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

static const struct i2c_device_id mis20c1_id[] = {
	{SENSOR_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mis20c1_id);

static struct i2c_driver mis20c1_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
	},
	.probe = mis20c1_probe,
	.remove = mis20c1_remove,
	.id_table = mis20c1_id,
};

static __init int init_mis20c1(void)
{
	int ret;

	sensor_common_init(&sensor_info);

	ret = private_driver_get_interface();
	if (ret) {
		ISP_ERROR("Failed to init %s driver\n", SENSOR_NAME);
		sensor_common_exit();
		return ret;
	}

	ret = private_i2c_add_driver(&mis20c1_driver);
	if (ret)
		sensor_common_exit();

	return ret;
}

static __exit void exit_mis20c1(void)
{
	private_i2c_del_driver(&mis20c1_driver);
	sensor_common_exit();
}

module_init(init_mis20c1);
module_exit(exit_mis20c1);

MODULE_DESCRIPTION("A low-level driver for ImageDesign mis20c1 sensors");
MODULE_LICENSE("GPL");
