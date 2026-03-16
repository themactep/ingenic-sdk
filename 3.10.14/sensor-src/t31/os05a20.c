// SPDX-License-Identifier: GPL-2.0+
/*
 * os05a20.c
 * Copyright (C) 2023 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sensor_max_fps   resolution      fps       interface              mode
 *  25               2592*1944       25        mipi_2lane            linear
 *  30               1920*1080       30        mipi_2lane            linear
 *  60               1920*1080       60        mipi_2lane            linear
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
#define SENSOR_NAME "os05a20"
#define SENSOR_VERSION "H20190817a"
#define SENSOR_CHIP_ID 0x530541
#define SENSOR_CHIP_ID_H (0x53)
#define SENSOR_CHIP_ID_M (0x05)
#define SENSOR_CHIP_ID_L (0x41)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x36

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 2592
#define SENSOR_MAX_HEIGHT 1944

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_SCLK_FPS_25 (214579200) /* 2112 * 2032 * 25 * 2 = 214579200 */
#define SENSOR_SUPPORT_SCLK_FPS_30 (214579200)  /* 669 * 1345 *30 * 2 = 214579200 */
#define SENSOR_SUPPORT_SCLK_FPS_60 (214579200)  /* 669 * 1345 *60 * 2 = 214579200 */
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
	{0x80, 0},
	{0x88, 5731},
	{0x90, 11136},
	{0x98, 16248},
	{0xa0, 21097},
	{0xa8, 25710},
	{0xb0, 30109},
	{0xb8, 34312},
	{0xc0, 38336},
	{0xc8, 42195},
	{0xd0, 45904},
	{0xd8, 49472},
	{0xe0, 52910},
	{0xe8, 56228},
	{0xf0, 59433},
	{0xf8, 62534},
	{0x100, 65536},
	{0x110, 71267},
	{0x120, 76672},
	{0x130, 81784},
	{0x140, 86633},
	{0x150, 91246},
	{0x160, 95645},
	{0x170, 99848},
	{0x180, 103872},
	{0x190, 107731},
	{0x1a0, 111440},
	{0x1b0, 115008},
	{0x1c0, 118446},
	{0x1d0, 121764},
	{0x1e0, 124969},
	{0x1f0, 128070},
	{0x200, 131072},
	{0x220, 136803},
	{0x240, 142208},
	{0x260, 147320},
	{0x280, 152169},
	{0x2a0, 156782},
	{0x2c0, 161181},
	{0x2e0, 165384},
	{0x300, 169408},
	{0x320, 173267},
	{0x340, 176976},
	{0x360, 180544},
	{0x380, 183982},
	{0x3a0, 187300},
	{0x3c0, 190505},
	{0x3e0, 193606},
	{0x400, 196608},
	{0x440, 202339},
	{0x480, 207744},
	{0x4c0, 212856},
	{0x500, 217705},
	{0x540, 222318},
	{0x580, 226717},
	{0x5c0, 230920},
	{0x600, 234944},
	{0x640, 238803},
	{0x680, 242512},
	{0x6c0, 246080},
	{0x700, 249518},
	{0x740, 252836},
	{0x780, 256041},
	{0x7c0, 259142},
	{0x7ff, 262144},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;

	*sensor_it = expo;

	return it;
}

unsigned int sensor_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;

	*sensor_it = expo;

	return it;
}

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
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


unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi_25fps={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 600,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2592,
	.image_theight = 1944,
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_30fps={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1504,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
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
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_60fps={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1088,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
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
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
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
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 262144,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};


static struct regval_list sensor_init_regs_2592_1944_25fps[] = {
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x36},
	{0x0306, 0x00},
	{0x0307, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x04},
	{0x032a, 0x00},
	{0x031e, 0x0a},
	{0x0325, 0x48},
	{0x0328, 0x07},
	{0x300d, 0x11},
	{0x300e, 0x11},
	{0x300f, 0x11},
	{0x3026, 0x00},
	{0x3027, 0x00},
	{0x3010, 0x01},
	{0x3012, 0x21},
	{0x3016, 0xf0},
	{0x3018, 0xf0},
	{0x3028, 0xf0},
	{0x301e, 0x98},
	{0x3010, 0x01},
	{0x3011, 0x9c},
	{0x3031, 0xa9},
	{0x3103, 0x48},
	{0x3104, 0x01},
	{0x3106, 0x10},
	{0x3400, 0x04},
	{0x3025, 0x03},
	{0x3425, 0x01},
	{0x3428, 0x01},
	{0x3406, 0x08},
	{0x3408, 0x03},
	{0x3501, 0x08},
	{0x3502, 0x20},
	{0x3505, 0x83},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3600, 0x00},
	{0x3626, 0xff},
	{0x3605, 0x50},
	{0x3609, 0xdb},
	{0x3610, 0x69},
	{0x360c, 0x01},
	{0x3628, 0xa4},
	{0x3629, 0x6a},
	{0x362d, 0x10},
	{0x3660, 0xd3},
	{0x3661, 0x06},
	{0x3662, 0x00},
	{0x3663, 0x28},
	{0x3664, 0x0d},
	{0x366a, 0x38},
	{0x366b, 0xa0},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x3680, 0x00},
	{0x36c0, 0x00},
	{0x3621, 0x81},
	{0x3634, 0x31},
	{0x3620, 0x00},
	{0x3622, 0x00},
	{0x362a, 0xd0},
	{0x362e, 0x8c},
	{0x362f, 0x98},
	{0x3630, 0xb0},
	{0x3631, 0xd7},
	{0x3701, 0x0f},
	{0x3737, 0x02},
	{0x3740, 0x18},
	{0x3741, 0x04},
	{0x373c, 0x0f},
	{0x373b, 0x02},
	{0x3705, 0x00},
	{0x3706, 0xa0},
	{0x370a, 0x01},
	{0x370b, 0xc8},
	{0x3709, 0x4a},
	{0x3714, 0x21},
	{0x371c, 0x00},
	{0x371d, 0x08},
	{0x375e, 0x0e},
	{0x3760, 0x13},
	{0x3776, 0x10},
	{0x3781, 0x02},
	{0x3782, 0x04},
	{0x3783, 0x02},
	{0x3784, 0x08},
	{0x3785, 0x08},
	{0x3788, 0x01},
	{0x3789, 0x01},
	{0x3797, 0x04},
	{0x3798, 0x01},
	{0x3799, 0x00},
	{0x3761, 0x02},
	{0x3762, 0x0d},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0e},
	{0x3805, 0xff},
	{0x3806, 0x08},
	{0x3807, 0x6f},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
	{0x380b, 0x98},
	{0x380c, 0x07},
	{0x380d, 0xf0},
	{0x380e, 0x08},
	{0x380f, 0x40},
	{0x3811, 0x10},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381c, 0x00},
	{0x3820, 0x00},
	{0x3821, 0x04},
	{0x3822, 0x54},
	{0x3823, 0x18},
	{0x3826, 0x00},
	{0x3827, 0x01},
	{0x3833, 0x00},
	{0x3832, 0x02},
	{0x383c, 0x48},
	{0x383d, 0xff},
	{0x3843, 0x20},
	{0x382d, 0x08},
	{0x3d85, 0x0b},
	{0x3d84, 0x40},
	{0x3d8c, 0x63},
	{0x3d8d, 0x00},
	{0x4000, 0x78},
	{0x4001, 0x2b},
	{0x4004, 0x01},
	{0x4005, 0x00},
	{0x4028, 0x2f},
	{0x400a, 0x01},
	{0x4010, 0x12},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x401a, 0x58},
	{0x4050, 0x00},
	{0x4051, 0x01},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x430b, 0xff},
	{0x430c, 0xff},
	{0x430d, 0x00},
	{0x430e, 0x00},
	{0x4501, 0x18},
	{0x4502, 0x00},
	{0x4600, 0x00},
	{0x4601, 0x10},
	{0x4603, 0x01},
	{0x4643, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x480e, 0x00},
	{0x4813, 0x00},
	{0x4815, 0x2b},
	{0x486e, 0x36},
	{0x486f, 0x84},
	{0x4860, 0x00},
	{0x4861, 0xa0},
	{0x484b, 0x05},
	{0x4850, 0x00},
	{0x4851, 0xaa},
	{0x4852, 0xff},
	{0x4853, 0x8a},
	{0x4854, 0x08},
	{0x4855, 0x30},
	{0x4800, 0x60},
	{0x4837, 0x12},
	{0x484a, 0x3f},
	{0x5000, 0xc9},
	{0x5001, 0x43},
	{0x5002, 0x00},
	{0x5211, 0x03},
	{0x5291, 0x03},
	{0x520d, 0x0f},
	{0x520e, 0xfd},
	{0x520f, 0xa5},
	{0x5210, 0xa5},
	{0x528d, 0x0f},
	{0x528e, 0xfd},
	{0x528f, 0xa5},
	{0x5290, 0xa5},
	{0x5004, 0x40},
	{0x5005, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x5182, 0x0f},
	{0x5183, 0xff},
	{0x580b, 0x03},
	{0x4d00, 0x03},
	{0x4d01, 0xe9},
	{0x4d02, 0xba},
	{0x4d03, 0x66},
	{0x4d04, 0x46},
	{0x4d05, 0xa5},
	{0x3603, 0x3c},
	{0x3703, 0x26},
	{0x3709, 0x49},
	{0x3708, 0x2d},
	{0x3719, 0x1c},
	{0x371a, 0x06},
	{0x4000, 0x79},
	{0x4603, 0x00},
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};


static struct regval_list sensor_init_regs_1920_1080_30fps[] = {
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x5e},
	{0x0306, 0x00},
	{0x0307, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x04},
	{0x032a, 0x00},
	{0x031e, 0x09},
	{0x0325, 0x48},
	{0x0328, 0x07},
	{0x300d, 0x11},
	{0x300e, 0x11},
	{0x300f, 0x11},
	{0x3026, 0x00},
	{0x3027, 0x00},
	{0x3010, 0x01},
	{0x3012, 0x21},
	{0x3016, 0xf0},
	{0x3018, 0xf0},
	{0x3028, 0xf0},
	{0x301e, 0x98},
	{0x3010, 0x01},
	{0x3011, 0x04},
	{0x3031, 0xa9},
	{0x3103, 0x48},
	{0x3104, 0x01},
	{0x3106, 0x10},
	{0x3400, 0x04},
	{0x3025, 0x03},
	{0x3425, 0x01},
	{0x3428, 0x01},
	{0x3406, 0x08},
	{0x3408, 0x03},
	{0x3501, 0x05},
	{0x3502, 0x21},
	{0x3505, 0x83},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3600, 0x00},
	{0x3626, 0xff},
	{0x3605, 0x50},
	{0x3609, 0xb5},
	{0x3610, 0x69},
	{0x360c, 0x01},
	{0x3628, 0xa4},
	{0x3629, 0x6a},
	{0x362d, 0x10},
	{0x3660, 0x43},
	{0x3661, 0x06},
	{0x3662, 0x00},
	{0x3663, 0x28},
	{0x3664, 0x0d},
	{0x366a, 0x38},
	{0x366b, 0xa0},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x3680, 0x00},
	{0x36c0, 0x00},
	{0x3621, 0x81},
	{0x3634, 0x31},
	{0x3620, 0x00},
	{0x3622, 0x00},
	{0x362a, 0xd0},
	{0x362e, 0x8c},
	{0x362f, 0x98},
	{0x3630, 0xb0},
	{0x3631, 0xd7},
	{0x3701, 0x0f},
	{0x3737, 0x02},
	{0x3740, 0x18},
	{0x3741, 0x04},
	{0x373c, 0x0f},
	{0x373b, 0x02},
	{0x3705, 0x00},
	{0x3706, 0x50},
	{0x370a, 0x00},
	{0x370b, 0xe4},
	{0x3709, 0x4a},
	{0x3714, 0x21},
	{0x371c, 0x00},
	{0x371d, 0x08},
	{0x375e, 0x0e},
	{0x3760, 0x13},
	{0x3776, 0x10},
	{0x3781, 0x02},
	{0x3782, 0x04},
	{0x3783, 0x02},
	{0x3784, 0x08},
	{0x3785, 0x08},
	{0x3788, 0x01},
	{0x3789, 0x01},
	{0x3797, 0x04},
	{0x3798, 0x01},
	{0x3799, 0x00},
	{0x3761, 0x02},
	{0x3762, 0x0d},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0e},
	{0x3805, 0xff},
	{0x3806, 0x08},
	{0x3807, 0x6f},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x02},//0x29d
	{0x380d, 0x9d},//
	{0x380e, 0x05},//0x541
	{0x380f, 0x41},//
	{0x3811, 0x10},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381c, 0x00},
	{0x3820, 0x00},
	{0x3821, 0x04},
	{0x3822, 0x54},
	{0x3823, 0x18},
	{0x3826, 0x00},
	{0x3827, 0x01},
	{0x3833, 0x00},
	{0x3832, 0x02},
	{0x383c, 0x48},
	{0x383d, 0xff},
	{0x3843, 0x20},
	{0x382d, 0x08},
	{0x3d85, 0x0b},
	{0x3d84, 0x40},
	{0x3d8c, 0x63},
	{0x3d8d, 0x00},
	{0x4000, 0x78},
	{0x4001, 0x2b},
	{0x4004, 0x00},
	{0x4005, 0x40},
	{0x4028, 0x2f},
	{0x400a, 0x01},
	{0x4010, 0x12},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x401a, 0x58},
	{0x4050, 0x00},
	{0x4051, 0x01},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x430b, 0xff},
	{0x430c, 0xff},
	{0x430d, 0x00},
	{0x430e, 0x00},
	{0x4501, 0x18},
	{0x4502, 0x00},
	{0x4643, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x480e, 0x00},
	{0x4813, 0x00},
	{0x4815, 0x2b},
	{0x486e, 0x36},
	{0x486f, 0x84},
	{0x4860, 0x00},
	{0x4861, 0xa0},
	{0x484b, 0x05},
	{0x4850, 0x00},
	{0x4851, 0xaa},
	{0x4852, 0xff},
	{0x4853, 0x8a},
	{0x4854, 0x08},
	{0x4855, 0x30},
	{0x4800, 0x60},
	{0x4837, 0x0a},
	{0x484a, 0x3f},
	{0x5000, 0xc9},
	{0x5001, 0x43},
	{0x5002, 0x00},
	{0x5211, 0x03},
	{0x5291, 0x03},
	{0x520d, 0x0f},
	{0x520e, 0xfd},
	{0x520f, 0xa5},
	{0x5210, 0xa5},
	{0x528d, 0x0f},
	{0x528e, 0xfd},
	{0x528f, 0xa5},
	{0x5290, 0xa5},
	{0x5004, 0x40},
	{0x5005, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x5182, 0x0f},
	{0x5183, 0xff},
	{0x580b, 0x03},
	{0x4d00, 0x03},
	{0x4d01, 0xe9},
	{0x4d02, 0xba},
	{0x4d03, 0x66},
	{0x4d04, 0x46},
	{0x4d05, 0xa5},
	{0x3603, 0x3c},
	{0x3703, 0x26},
	{0x3709, 0x49},
	{0x3708, 0x2d},
	{0x3719, 0x1c},
	{0x371a, 0x06},
	{0x4000, 0x79},
	{0x380c, 0x05},
	{0x380d, 0x3a},
	{0x380e, 0x0a},
	{0x380f, 0x82},
	{0x3501, 0x0a},
	{0x3502, 0x7a},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};


static struct regval_list sensor_init_regs_1920_1080_60fps[] = {
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x44},
	{0x0306, 0x00},
	{0x0307, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x04},
	{0x032a, 0x00},
	{0x031e, 0x09},
	{0x0325, 0x48},
	{0x0328, 0x07},
	{0x300d, 0x11},
	{0x300e, 0x11},
	{0x300f, 0x11},
	{0x3026, 0x00},
	{0x3027, 0x00},
	{0x3010, 0x01},
	{0x3012, 0x21},
	{0x3016, 0xf0},
	{0x3018, 0xf0},
	{0x3028, 0xf0},
	{0x301e, 0x98},
	{0x3010, 0x01},
	{0x3011, 0x04},
	{0x3031, 0xa9},
	{0x3103, 0x48},
	{0x3104, 0x01},
	{0x3106, 0x10},
	{0x3400, 0x04},
	{0x3025, 0x03},
	{0x3425, 0x01},
	{0x3428, 0x01},
	{0x3406, 0x08},
	{0x3408, 0x03},
	{0x3501, 0x05},
	{0x3502, 0x21},
	{0x3505, 0x83},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3600, 0x00},
	{0x3626, 0xff},
	{0x3605, 0x50},
	{0x3609, 0xb5},
	{0x3610, 0x69},
	{0x360c, 0x01},
	{0x3628, 0xa4},
	{0x3629, 0x6a},
	{0x362d, 0x10},
	{0x3660, 0x43},
	{0x3661, 0x06},
	{0x3662, 0x00},
	{0x3663, 0x28},
	{0x3664, 0x0d},
	{0x366a, 0x38},
	{0x366b, 0xa0},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x3680, 0x00},
	{0x36c0, 0x00},
	{0x3621, 0x81},
	{0x3634, 0x31},
	{0x3620, 0x00},
	{0x3622, 0x00},
	{0x362a, 0xd0},
	{0x362e, 0x8c},
	{0x362f, 0x98},
	{0x3630, 0xb0},
	{0x3631, 0xd7},
	{0x3701, 0x0f},
	{0x3737, 0x02},
	{0x3740, 0x18},
	{0x3741, 0x04},
	{0x373c, 0x0f},
	{0x373b, 0x02},
	{0x3705, 0x00},
	{0x3706, 0x50},
	{0x370a, 0x00},
	{0x370b, 0xe4},
	{0x3709, 0x4a},
	{0x3714, 0x21},
	{0x371c, 0x00},
	{0x371d, 0x08},
	{0x375e, 0x0e},
	{0x3760, 0x13},
	{0x3776, 0x10},
	{0x3781, 0x02},
	{0x3782, 0x04},
	{0x3783, 0x02},
	{0x3784, 0x08},
	{0x3785, 0x08},
	{0x3788, 0x01},
	{0x3789, 0x01},
	{0x3797, 0x04},
	{0x3798, 0x01},
	{0x3799, 0x00},
	{0x3761, 0x02},
	{0x3762, 0x0d},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0e},
	{0x3805, 0xff},
	{0x3806, 0x08},
	{0x3807, 0x6f},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x02},//0x29d -> 669
	{0x380d, 0x9d},//
	{0x380e, 0x05},//0x541 -> 1345
	{0x380f, 0x41},//
	{0x3811, 0x10},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381c, 0x00},
	{0x3820, 0x00},
	{0x3821, 0x04},
	{0x3822, 0x54},
	{0x3823, 0x18},
	{0x3826, 0x00},
	{0x3827, 0x01},
	{0x3833, 0x00},
	{0x3832, 0x02},
	{0x383c, 0x48},
	{0x383d, 0xff},
	{0x3843, 0x20},
	{0x382d, 0x08},
	{0x3d85, 0x0b},
	{0x3d84, 0x40},
	{0x3d8c, 0x63},
	{0x3d8d, 0x00},
	{0x4000, 0x78},
	{0x4001, 0x2b},
	{0x4004, 0x00},
	{0x4005, 0x40},
	{0x4028, 0x2f},
	{0x400a, 0x01},
	{0x4010, 0x12},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x401a, 0x58},
	{0x4050, 0x00},
	{0x4051, 0x01},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x430b, 0xff},
	{0x430c, 0xff},
	{0x430d, 0x00},
	{0x430e, 0x00},
	{0x4501, 0x18},
	{0x4502, 0x00},
	{0x4643, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x480e, 0x00},
	{0x4813, 0x00},
	{0x4815, 0x2b},
	{0x486e, 0x36},
	{0x486f, 0x84},
	{0x4860, 0x00},
	{0x4861, 0xa0},
	{0x484b, 0x05},
	{0x4850, 0x00},
	{0x4851, 0xaa},
	{0x4852, 0xff},
	{0x4853, 0x8a},
	{0x4854, 0x08},
	{0x4855, 0x30},
	{0x4800, 0x60},
	{0x4837, 0x0e},
	{0x484a, 0x3f},
	{0x5000, 0xc9},
	{0x5001, 0x43},
	{0x5002, 0x00},
	{0x5211, 0x03},
	{0x5291, 0x03},
	{0x520d, 0x0f},
	{0x520e, 0xfd},
	{0x520f, 0xa5},
	{0x5210, 0xa5},
	{0x528d, 0x0f},
	{0x528e, 0xfd},
	{0x528f, 0xa5},
	{0x5290, 0xa5},
	{0x5004, 0x40},
	{0x5005, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x5182, 0x0f},
	{0x5183, 0xff},
	{0x580b, 0x03},
	{0x4d00, 0x03},
	{0x4d01, 0xe9},
	{0x4d02, 0xba},
	{0x4d03, 0x66},
	{0x4d04, 0x46},
	{0x4d05, 0xa5},
	{0x3603, 0x3c},
	{0x3703, 0x26},
	{0x3709, 0x49},
	{0x3708, 0x2d},
	{0x3719, 0x1c},
	{0x371a, 0x06},
	{0x4000, 0x79},
	{0x380c, 0x05},
	{0x380d, 0x3a},
	{0x380e, 0x05},
	{0x380f, 0x41},
	{0x3501, 0x05},
	{0x3502, 0x21},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};
/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2592,
		.height = 1944,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2592_1944_25fps,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_60fps,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];


static struct regval_list sensor_stream_on[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
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

static int sensor_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = sensor_read(sd, 0x300c, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 16) | v;
	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret += sensor_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += sensor_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	ret += sensor_write(sd, 0x3500, (unsigned char)((expo >> 16) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x3509, (unsigned char)((value & 0xff)));
	ret += sensor_write(sd, 0x3508, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

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
		ret = sensor_write_array(sd, sensor_stream_on);
		pr_debug("%s stream on\n", SENSOR_NAME);
	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;
	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		sclk = SENSOR_SUPPORT_SCLK_FPS_25;
		max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	case TX_SENSOR_MAX_FPS_30:
		sclk = SENSOR_SUPPORT_SCLK_FPS_30;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	case TX_SENSOR_MAX_FPS_60:
		sclk = SENSOR_SUPPORT_SCLK_FPS_60;
		max_fps = TX_SENSOR_MAX_FPS_60;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x380c, &val);
	hts = val<<8;
	val = 0;
	ret += sensor_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	hts = hts << 1;

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sensor_write(sd, 0x3208, 0x02);
	ret += sensor_write(sd, 0x380f, vts & 0xff);
	ret += sensor_write(sd, 0x380e, (vts >> 8) & 0xff);
	ret += sensor_write(sd, 0x3208, 0x12);
	ret += sensor_write(sd, 0x320d, 0x00);
	ret += sensor_write(sd, 0x3208, 0xe2);
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;

	sensor_update_actual_fps((fps >> 16) & 0xffff);
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
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
			private_msleep(15);
		} else {
			ISP_ERROR("gpio request fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
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
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, *(int*)arg);
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
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_on);
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
	/*.ioctl = sensor_ops_ioctl,*/
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
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		wsize = &sensor_win_sizes[0];
		sensor_info.max_fps = 25;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_25fps),sizeof(sensor_mipi_25fps));
		sensor_attr.max_integration_time_native = 2104; /* 2112 - 8 = 2104 */
		sensor_attr.integration_time_limit = 2104;
		sensor_attr.total_width = 4064; /* 2032 * 2 = 4064 */
		sensor_attr.total_height = 2112;
		sensor_attr.max_integration_time = 2104;
		sensor_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12;
		break;
	case TX_SENSOR_MAX_FPS_30:
		wsize = &sensor_win_sizes[1];
		sensor_info.max_fps = 30;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_30fps),sizeof(sensor_mipi_30fps));
		sensor_attr.max_integration_time_native = 661;  /* 669 - 8 = 691 */
		sensor_attr.integration_time_limit = 661;
		sensor_attr.total_width = 2690;  /* 1345 * 2 = 2690 */
		sensor_attr.total_height = 669;
		sensor_attr.max_integration_time = 661;
		sensor_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10;
		break;
	case TX_SENSOR_MAX_FPS_60:
		wsize = &sensor_win_sizes[2];
		sensor_info.max_fps = 60;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_60fps),sizeof(sensor_mipi_60fps));
		sensor_attr.max_integration_time_native = 661;  /* 669 - 8 = 691 */
		sensor_attr.integration_time_limit = 661;
		sensor_attr.total_width = 2690;  /* 1345 * 2 = 2690 */
		sensor_attr.total_height = 669;
		sensor_attr.max_integration_time = 661;
		sensor_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}
	sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
	sensor_attr.max_again = 262144;
	sensor_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
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
