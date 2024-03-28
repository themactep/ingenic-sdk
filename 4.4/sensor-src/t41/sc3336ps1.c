// SPDX-License-Identifier: GPL-2.0+
/*
 * sc3336ps1.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface       mode          func       DVDD
 *   0          2304*1296       25        mipi_2lane     linear        master      1.2V
 *   1          2304*1296       25        mipi_2lane     linear         slave      1.2V
 *   2          2304*1296       40        mipi_2lane     linear        master      1.5V
 *   3          2304*1296       40        mipi_2lane     linear         slave      1.5V
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

#define SENSOR_NAME "sc3336ps1"
#define SENSOR_CHIP_ID_H (0x9c)
#define SENSOR_CHIP_ID_L (0x41)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20231010a"

static int reset_gpio = -1;
static int pwdn_gpio = -1;

static int fsync_mode = 3;
module_param(fsync_mode, int, S_IRUGO);
MODULE_PARM_DESC(fsync_mode, "Sensor Indicates the frame synchronization mode");

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
	{0x84, 2886},
	{0x88, 5776},
	{0x8c, 8494},
	{0x90, 11136},
	{0x94, 13706},
	{0x98, 16287},
	{0x9c, 18723},
	{0xa0, 21097},
	{0xa4, 23414},
	{0xa8, 25746},
	{0xac, 27953},
	{0xb0, 30109},
	{0xb4, 32217},
	{0xb8, 34345},
	{0xbc, 36361},
	{0xc0, 38336},
	{0x4080, 39588},
	{0x4084, 42467},
	{0x4088, 45378},
	{0x408c, 48089},
	{0x4090, 50724},
	{0x4094, 53288},
	{0x4098, 55888},
	{0x409c, 58318},
	{0x40a0, 60686},
	{0x40a4, 62996},
	{0x40a8, 65346},
	{0x40ac, 67547},
	{0x40b0, 69697},
	{0x40b4, 71800},
	{0x40b8, 73943},
	{0x40bc, 75955},
	{0x40c0, 77924},
	{0x40c4, 79853},
	{0x40c8, 81823},
	{0x40cc, 83676},
	{0x40d0, 85492},
	{0x40d4, 87274},
	{0x40d8, 89097},
	{0x40dc, 90813},
	{0x40e0, 92499},
	{0x40e4, 94155},
	{0x40e8, 95851},
	{0x40ec, 97450},
	{0x40f0, 99022},
	{0x40f4, 100568},
	{0x40f8, 102154},
	{0x40fc, 103651},
	{0x4880, 105124},
	{0x4884, 108003},
	{0x4888, 110914},
	{0x488c, 113625},
	{0x4890, 116260},
	{0x4894, 118824},
	{0x4898, 121424},
	{0x489c, 123854},
	{0x48a0, 126222},
	{0x48a4, 128532},
	{0x48a8, 130882},
	{0x48ac, 133083},
	{0x48b0, 135233},
	{0x48b4, 137336},
	{0x48b8, 139479},
	{0x48bc, 141491},
	{0x48c0, 143460},
	{0x48c4, 145389},
	{0x48c8, 147359},
	{0x48cc, 149212},
	{0x48d0, 151028},
	{0x48d4, 152810},
	{0x48d8, 154633},
	{0x48dc, 156349},
	{0x48e0, 158035},
	{0x48e4, 159691},
	{0x48e8, 161387},
	{0x48ec, 162986},
	{0x48f0, 164558},
	{0x48f4, 166104},
	{0x48f8, 167690},
	{0x48fc, 169187},
	{0x4980, 170660},
	{0x4984, 173539},
	{0x4988, 176436},
	{0x498c, 179161},
	{0x4990, 181796},
	{0x4994, 184360},
	{0x4998, 186947},
	{0x499c, 189390},
	{0x49a0, 191758},
	{0x49a4, 194068},
	{0x49a8, 196406},
	{0x49ac, 198619},
	{0x49b0, 200769},
	{0x49b4, 202872},
	{0x49b8, 205005},
	{0x49bc, 207027},
	{0x49c0, 208996},
	{0x49c4, 210925},
	{0x49c8, 212886},
	{0x49cc, 214748},
	{0x49d0, 216564},
	{0x49d4, 218346},
	{0x49d8, 220160},
	{0x49dc, 221885},
	{0x49e0, 223571},
	{0x49e4, 225227},
	{0x49e8, 226914},
	{0x49ec, 228522},
	{0x49f0, 230094},
	{0x49f4, 231640},
	{0x49f8, 233218},
	{0x49fc, 234723},
	{0x4b80, 236196},
	{0x4b84, 239083},
	{0x4b88, 241972},
	{0x4b8c, 244690},
	{0x4b90, 247332},
	{0x4b94, 249902},
	{0x4b98, 252483},
	{0x4b9c, 254919},
	{0x4ba0, 257294},
	{0x4ba4, 259610},
	{0x4ba8, 261942},
	{0x4bac, 264149},
	{0x4bb0, 266305},
	{0x4bb4, 268413},
	{0x4bb8, 270541},
	{0x4bbc, 272557},
	{0x4bc0, 274532},
	{0x4bc4, 276466},
	{0x4bc8, 278422},
	{0x4bcc, 280279},
	{0x4bd0, 282100},
	{0x4bd4, 283887},
	{0x4bd8, 285696},
	{0x4bdc, 287417},
	{0x4be0, 289107},
	{0x4be4, 290767},
	{0x4be8, 292450},
	{0x4bec, 294053},
	{0x4bf0, 295630},
	{0x4bf4, 297181},
	{0x4bf8, 298754},
	{0x4bfc, 300255},
	{0x4f80, 301732},
	{0x4f84, 304619},
	{0x4f88, 307508},
	{0x4f8c, 310226},
	{0x4f90, 312868},
	{0x4f94, 315438},
	{0x4f98, 318019},
	{0x4f9c, 320455},
	{0x4fa0, 322830},
	{0x4fa4, 325146},
	{0x4fa8, 327478},
	{0x4fac, 329685},
	{0x4fb0, 331841},
	{0x4fb4, 333949},
	{0x4fb8, 336077},
	{0x4fbc, 338093},
	{0x4fc0, 340068},
	{0x4fc4, 342002},
	{0x4fc8, 343958},
	{0x4fcc, 345815},
	{0x4fd0, 347636},
	{0x4fd4, 349423},
	{0x4fd8, 351232},
	{0x4fdc, 352953},
	{0x4fe0, 354643},
	{0x4fe4, 356303},
	{0x4fe8, 357986},
	{0x4fec, 359589},
	{0x4ff0, 361166},
	{0x4ff4, 362717},
	{0x4ff8, 364290},
	{0x4ffc, 365791},
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

struct tx_isp_mipi_bus sensor_mipi_master = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 495,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
};

struct tx_isp_mipi_bus sensor_mipi_slave = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 495,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
};

struct tx_isp_mipi_bus sensor_mipi_40fps_master = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 743,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
};

struct tx_isp_mipi_bus sensor_mipi_40fps_slave = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 743,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x9c41,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.max_again = 365791,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.fsync_attr = {
		.mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE,
		.call_times = 1,
		.sdelay = 100,
	}
};

static struct regval_list sensor_init_regs_2304_1296_30fps_mipi_master[] = {
	{0x0103, 0x01},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x300a, 0x24},
	{0x301f, 0x0b},
	{0x3032, 0xa0},
	{0x30b8, 0x33},
	{0x320e, 0x06},
	{0x320f, 0x30},
	{0x3253, 0x10},
	{0x325f, 0x20},
	{0x3301, 0x04},
	{0x3306, 0x50},
	{0x3309, 0xf8},
	{0x330a, 0x00},
	{0x330b, 0xd8},
	{0x3314, 0x13},
	{0x331f, 0xe9},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335e, 0x06},
	{0x335f, 0x0a},
	{0x3364, 0x5e},
	{0x337c, 0x02},
	{0x337d, 0x0e},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x04},
	{0x3394, 0x04},
	{0x3395, 0x04},
	{0x3396, 0x08},
	{0x3397, 0x0b},
	{0x3398, 0x1f},
	{0x3399, 0x04},
	{0x339a, 0x0a},
	{0x339b, 0x3a},
	{0x339c, 0x60},
	{0x33a2, 0x04},
	{0x33ac, 0x08},
	{0x33ad, 0x1c},
	{0x33ae, 0x10},
	{0x33af, 0x30},
	{0x33b1, 0x80},
	{0x33b3, 0x48},
	{0x33f9, 0x60},
	{0x33fb, 0x74},
	{0x33fc, 0x4b},
	{0x33fd, 0x5f},
	{0x349f, 0x03},
	{0x34a6, 0x4b},
	{0x34a7, 0x5f},
	{0x34a8, 0x20},
	{0x34a9, 0x18},
	{0x34aa, 0x00},
	{0x34ab, 0xe8},
	{0x34ac, 0x01},
	{0x34ad, 0x00},
	{0x34f8, 0x5f},
	{0x34f9, 0x18},
	{0x3630, 0xc0},
	{0x3631, 0x84},
	{0x3632, 0x64},
	{0x3633, 0x32},
	{0x363b, 0x03},
	{0x363c, 0x08},
	{0x3641, 0x38},
	{0x3670, 0x4e},
	{0x3674, 0xf0},
	{0x3675, 0xc0},
	{0x3676, 0xc0},
	{0x3677, 0x86},
	{0x3678, 0x86},
	{0x3679, 0x86},
	{0x367c, 0x48},
	{0x367d, 0x49},
	{0x367e, 0x4b},
	{0x367f, 0x5f},
	{0x3690, 0x22},
	{0x3691, 0x22},
	{0x3692, 0x33},
	{0x369c, 0x4b},
	{0x369d, 0x4f},
	{0x36b0, 0x87},
	{0x36b1, 0x90},
	{0x36b2, 0xa1},
	{0x36b3, 0xc8},
	{0x36b4, 0x49},
	{0x36b5, 0x4b},
	{0x36b6, 0x4f},
	{0x36ea, 0x0b},
	{0x36eb, 0x0d},
	{0x36ec, 0x1c},
	{0x36ed, 0x26},
	{0x370f, 0x01},
	{0x3722, 0x09},
	{0x3724, 0x41},
	{0x3725, 0xc1},
	{0x3771, 0x09},
	{0x3772, 0x09},
	{0x3773, 0x05},
	{0x377a, 0x48},
	{0x377b, 0x5f},
	{0x37fa, 0x0b},
	{0x37fb, 0x33},
	{0x37fc, 0x11},
	{0x37fd, 0x08},
	{0x3904, 0x04},
	{0x3905, 0x8c},
	{0x391d, 0x04},
	{0x391f, 0x49},
	{0x3921, 0x20},
	{0x3926, 0x21},
	{0x3933, 0x80},
	{0x3934, 0x08},
	{0x3935, 0x00},
	{0x3936, 0x90},
	{0x3937, 0x78},
	{0x3938, 0x77},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x393b, 0x00},
	{0x393c, 0x1c},
	{0x39dc, 0x02},
	{0x3e01, 0x62},
	{0x3e02, 0x80},
	{0x3e09, 0x00},
	{0x440d, 0x10},
	{0x440e, 0x01},
	{0x4509, 0x20},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5785, 0x04},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x04},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x04},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd2},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x59e2, 0x08},
	{0x59e3, 0x03},
	{0x59e4, 0x00},
	{0x59e5, 0x10},
	{0x59e6, 0x06},
	{0x59e7, 0x00},
	{0x59e8, 0x08},
	{0x59e9, 0x02},
	{0x59ea, 0x00},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x28},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x36e9, 0x53},
	{0x37f9, 0x27},
	{0x0100, 0x01},
	{SENSOR_REG_DELAY, 0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sensor_init_regs_2304_1296_30fps_mipi_slave[] = {
	{0x0103, 0x01},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x0c},
	{0x30b8, 0x33},
	{0x320e, 0x06},
	{0x320f, 0x30},
	{0x3222, 0x01},
	{0x3224, 0x83},//0x82 = 1000 0010
	{0x3225, 0x10},
	{0x322e, 0x06},//0x62c
	{0x322f, 0x34},//
	{0x3230, 0x00},//0x04
	{0x3231, 0x04},//
	{0x3253, 0x10},
	{0x325f, 0x20},
	{0x3301, 0x04},
	{0x3306, 0x50},
	{0x3309, 0xf8},
	{0x330a, 0x00},
	{0x330b, 0xd8},
	{0x3314, 0x13},
	{0x331f, 0xe9},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335e, 0x06},
	{0x335f, 0x0a},
	{0x3364, 0x5e},
	{0x337c, 0x02},
	{0x337d, 0x0e},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x04},
	{0x3394, 0x04},
	{0x3395, 0x04},
	{0x3396, 0x08},
	{0x3397, 0x0b},
	{0x3398, 0x1f},
	{0x3399, 0x04},
	{0x339a, 0x0a},
	{0x339b, 0x3a},
	{0x339c, 0x60},
	{0x33a2, 0x04},
	{0x33ac, 0x08},
	{0x33ad, 0x1c},
	{0x33ae, 0x10},
	{0x33af, 0x30},
	{0x33b1, 0x80},
	{0x33b3, 0x48},
	{0x33f9, 0x60},
	{0x33fb, 0x74},
	{0x33fc, 0x4b},
	{0x33fd, 0x5f},
	{0x349f, 0x03},
	{0x34a6, 0x4b},
	{0x34a7, 0x5f},
	{0x34a8, 0x20},
	{0x34a9, 0x18},
	{0x34aa, 0x00},
	{0x34ab, 0xe8},
	{0x34ac, 0x01},
	{0x34ad, 0x00},
	{0x34f8, 0x5f},
	{0x34f9, 0x18},
	{0x3630, 0xc0},
	{0x3631, 0x84},
	{0x3632, 0x64},
	{0x3633, 0x32},
	{0x363b, 0x03},
	{0x363c, 0x08},
	{0x3641, 0x38},
	{0x3670, 0x4e},
	{0x3674, 0xf0},
	{0x3675, 0xc0},
	{0x3676, 0xc0},
	{0x3677, 0x86},
	{0x3678, 0x86},
	{0x3679, 0x86},
	{0x367c, 0x48},
	{0x367d, 0x49},
	{0x367e, 0x4b},
	{0x367f, 0x5f},
	{0x3690, 0x22},
	{0x3691, 0x22},
	{0x3692, 0x33},
	{0x369c, 0x4b},
	{0x369d, 0x4f},
	{0x36b0, 0x87},
	{0x36b1, 0x90},
	{0x36b2, 0xa1},
	{0x36b3, 0xc8},
	{0x36b4, 0x49},
	{0x36b5, 0x4b},
	{0x36b6, 0x4f},
	{0x36ea, 0x0b},
	{0x36eb, 0x0d},
	{0x36ec, 0x1c},
	{0x36ed, 0x26},
	{0x370f, 0x01},
	{0x3722, 0x09},
	{0x3724, 0x41},
	{0x3725, 0xc1},
	{0x3771, 0x09},
	{0x3772, 0x09},
	{0x3773, 0x05},
	{0x377a, 0x48},
	{0x377b, 0x5f},
	{0x37fa, 0x0b},
	{0x37fb, 0x33},
	{0x37fc, 0x11},
	{0x37fd, 0x08},
	{0x3904, 0x04},
	{0x3905, 0x8c},
	{0x391d, 0x04},
	{0x391f, 0x49},
	{0x3921, 0x20},
	{0x3926, 0x21},
	{0x3933, 0x80},
	{0x3934, 0x08},
	{0x3935, 0x00},
	{0x3936, 0x90},
	{0x3937, 0x78},
	{0x3938, 0x77},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x393b, 0x00},
	{0x393c, 0x1c},
	{0x39dc, 0x02},
	{0x3e01, 0x62},
	{0x3e02, 0x80},
	{0x3e09, 0x00},
	{0x440d, 0x10},
	{0x440e, 0x01},
	{0x4509, 0x20},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5785, 0x04},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x04},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x04},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd2},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x59e2, 0x08},
	{0x59e3, 0x03},
	{0x59e4, 0x00},
	{0x59e5, 0x10},
	{0x59e6, 0x06},
	{0x59e7, 0x00},
	{0x59e8, 0x08},
	{0x59e9, 0x02},
	{0x59ea, 0x00},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x28},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x36e9, 0x53},
	{0x37f9, 0x27},
	{0x0100, 0x01},
	{SENSOR_REG_DELAY, 0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};


static struct regval_list sensor_init_regs_2304_1296_40fps_mipi_master[] = {
	{0x0103, 0x01},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x300a, 0x24},
	{0x301f, 0x11},
	{0x3032, 0xa0},
	{0x30b8, 0x33},
	{0x320c, 0x04},
	{0x320d, 0xe2},
	{0x320e, 0x05},//vts = 0x528 = 1320 -> 0x5cd = 1485
	{0x320f, 0xcd},
	{0x3253, 0x10},
	{0x325f, 0x20},
	{0x3301, 0x06},
	{0x3304, 0x70},
	{0x3306, 0x50},
	{0x3309, 0xf8},
	{0x330a, 0x00},
	{0x330b, 0xd8},
	{0x330d, 0x10},
	{0x3314, 0x13},
	{0x331e, 0x61},
	{0x331f, 0xe9},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335e, 0x06},
	{0x335f, 0x0a},
	{0x3364, 0x5e},
	{0x337c, 0x02},
	{0x337d, 0x0e},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x04},
	{0x3394, 0x04},
	{0x3395, 0x04},
	{0x3396, 0x08},
	{0x3397, 0x0b},
	{0x3398, 0x1f},
	{0x3399, 0x06},
	{0x339a, 0x0a},
	{0x339b, 0x3a},
	{0x339c, 0x78},
	{0x33a2, 0x04},
	{0x33ac, 0x08},
	{0x33ad, 0x1c},
	{0x33ae, 0x24},
	{0x33af, 0xa8},
	{0x33b1, 0x80},
	{0x33b3, 0x48},
	{0x33f9, 0x60},
	{0x33fb, 0x78},
	{0x33fc, 0x4b},
	{0x33fd, 0x5f},
	{0x349f, 0x03},
	{0x34a6, 0x4b},
	{0x34a7, 0x5f},
	{0x34a8, 0x20},
	{0x34a9, 0x18},
	{0x34aa, 0x00},
	{0x34ab, 0xe8},
	{0x34ac, 0x01},
	{0x34ad, 0x00},
	{0x34f8, 0x5f},
	{0x34f9, 0x18},
	{0x3630, 0xc0},
	{0x3631, 0x84},
	{0x3632, 0x64},
	{0x3633, 0x32},
	{0x363b, 0x03},
	{0x363c, 0x08},
	{0x3641, 0x08},
	{0x3670, 0x4e},
	{0x3674, 0xf0},
	{0x3675, 0xc0},
	{0x3676, 0xc0},
	{0x3677, 0x86},
	{0x3678, 0x86},
	{0x3679, 0x8d},
	{0x367c, 0x48},
	{0x367d, 0x49},
	{0x367e, 0x4b},
	{0x367f, 0x5f},
	{0x3690, 0x22},
	{0x3691, 0x22},
	{0x3692, 0x33},
	{0x369c, 0x4b},
	{0x369d, 0x4f},
	{0x36b0, 0x87},
	{0x36b1, 0x90},
	{0x36b2, 0xa1},
	{0x36b3, 0xc8},
	{0x36b4, 0x49},
	{0x36b5, 0x4b},
	{0x36b6, 0x4f},
	{0x36ea, 0x4b},
	{0x36eb, 0x0c},
	{0x36ec, 0x0c},
	{0x36ed, 0x26},
	{0x370f, 0x01},
	{0x3722, 0x09},
	{0x3724, 0x41},
	{0x3725, 0xc1},
	{0x3771, 0x09},
	{0x3772, 0x09},
	{0x3773, 0x05},
	{0x377a, 0x48},
	{0x377b, 0x5f},
	{0x37fa, 0x0b},
	{0x37fb, 0x33},
	{0x37fc, 0x11},
	{0x37fd, 0x38},
	{0x3904, 0x04},
	{0x3905, 0x8c},
	{0x391d, 0x04},
	{0x391f, 0x49},
	{0x3921, 0x20},
	{0x3926, 0x21},
	{0x3933, 0x80},
	{0x3934, 0x08},
	{0x3935, 0x00},
	{0x3936, 0x90},
	{0x3937, 0x78},
	{0x3938, 0x77},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x393b, 0x00},
	{0x393c, 0x1c},
	{0x39dc, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x52},
	{0x3e02, 0x00},
	{0x3e09, 0x00},
	{0x440d, 0x10},
	{0x440e, 0x01},
	{0x4509, 0x20},
	{0x4816, 0x51},
	{0x4819, 0x09},
	{0x481b, 0x05},
	{0x481d, 0x14},
	{0x481f, 0x04},
	{0x4821, 0x0a},
	{0x4823, 0x05},
	{0x4825, 0x04},
	{0x4827, 0x05},
	{0x4829, 0x08},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5785, 0x04},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x04},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x04},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd2},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x59e2, 0x08},
	{0x59e3, 0x03},
	{0x59e4, 0x00},
	{0x59e5, 0x10},
	{0x59e6, 0x06},
	{0x59e7, 0x00},
	{0x59e8, 0x08},
	{0x59e9, 0x02},
	{0x59ea, 0x00},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x28},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x36e9, 0x51},
	{0x37f9, 0x53},
	{0x0100, 0x01},
	{SENSOR_REG_DELAY, 0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sensor_init_regs_2304_1296_40fps_mipi_slave[] = {
	{0x0103, 0x01},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x10},
	{0x30b8, 0x33},
	{0x320c, 0x04},
	{0x320d, 0xe2},
//	{0x320e,0x05},
//	{0x320f,0x28},
	{0x320e, 0x05},//vts = 0x528 = 1320 -> 0x5cd = 1485
	{0x320f, 0xcd},
	{0x3222, 0x01},
	{0x3223, 0xc8},
	{0x3224, 0x83},
	{0x3230, 0x00},
	{0x3231, 0x04},
	{0x3253, 0x10},
	{0x325f, 0x20},
	{0x3301, 0x06},
	{0x3304, 0x70},
	{0x3306, 0x50},
	{0x3309, 0xf8},
	{0x330a, 0x00},
	{0x330b, 0xd8},
	{0x330d, 0x10},
	{0x3314, 0x13},
	{0x331e, 0x61},
	{0x331f, 0xe9},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335e, 0x06},
	{0x335f, 0x0a},
	{0x3364, 0x5e},
	{0x337c, 0x02},
	{0x337d, 0x0e},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x04},
	{0x3394, 0x04},
	{0x3395, 0x04},
	{0x3396, 0x08},
	{0x3397, 0x0b},
	{0x3398, 0x1f},
	{0x3399, 0x06},
	{0x339a, 0x0a},
	{0x339b, 0x3a},
	{0x339c, 0x78},
	{0x33a2, 0x04},
	{0x33ac, 0x08},
	{0x33ad, 0x1c},
	{0x33ae, 0x24},
	{0x33af, 0xa8},
	{0x33b1, 0x80},
	{0x33b3, 0x48},
	{0x33f9, 0x60},
	{0x33fb, 0x78},
	{0x33fc, 0x4b},
	{0x33fd, 0x5f},
	{0x349f, 0x03},
	{0x34a6, 0x4b},
	{0x34a7, 0x5f},
	{0x34a8, 0x20},
	{0x34a9, 0x18},
	{0x34aa, 0x00},
	{0x34ab, 0xe8},
	{0x34ac, 0x01},
	{0x34ad, 0x00},
	{0x34f8, 0x5f},
	{0x34f9, 0x18},
	{0x3630, 0xc0},
	{0x3631, 0x84},
	{0x3632, 0x64},
	{0x3633, 0x32},
	{0x363b, 0x03},
	{0x363c, 0x08},
	{0x3641, 0x08},
	{0x3670, 0x4e},
	{0x3674, 0xf0},
	{0x3675, 0xc0},
	{0x3676, 0xc0},
	{0x3677, 0x86},
	{0x3678, 0x86},
	{0x3679, 0x8d},
	{0x367c, 0x48},
	{0x367d, 0x49},
	{0x367e, 0x4b},
	{0x367f, 0x5f},
	{0x3690, 0x22},
	{0x3691, 0x22},
	{0x3692, 0x33},
	{0x369c, 0x4b},
	{0x369d, 0x4f},
	{0x36b0, 0x87},
	{0x36b1, 0x90},
	{0x36b2, 0xa1},
	{0x36b3, 0xc8},
	{0x36b4, 0x49},
	{0x36b5, 0x4b},
	{0x36b6, 0x4f},
	{0x36ea, 0x4b},
	{0x36eb, 0x0c},
	{0x36ec, 0x0c},
	{0x36ed, 0x26},
	{0x370f, 0x01},
	{0x3722, 0x09},
	{0x3724, 0x41},
	{0x3725, 0xc1},
	{0x3771, 0x09},
	{0x3772, 0x09},
	{0x3773, 0x05},
	{0x377a, 0x48},
	{0x377b, 0x5f},
	{0x37fa, 0x0b},
	{0x37fb, 0x33},
	{0x37fc, 0x11},
	{0x37fd, 0x38},
	{0x3904, 0x04},
	{0x3905, 0x8c},
	{0x391d, 0x04},
	{0x391f, 0x49},
	{0x3921, 0x20},
	{0x3926, 0x21},
	{0x3933, 0x80},
	{0x3934, 0x08},
	{0x3935, 0x00},
	{0x3936, 0x90},
	{0x3937, 0x78},
	{0x3938, 0x77},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x393b, 0x00},
	{0x393c, 0x1c},
	{0x39dc, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x52},
	{0x3e02, 0x00},
	{0x3e09, 0x00},
	{0x440d, 0x10},
	{0x440e, 0x01},
	{0x4509, 0x20},
	{0x4816, 0x51},
	{0x4819, 0x09},
	{0x481b, 0x05},
	{0x481d, 0x14},
	{0x481f, 0x04},
	{0x4821, 0x0a},
	{0x4823, 0x05},
	{0x4825, 0x04},
	{0x4827, 0x05},
	{0x4829, 0x08},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5785, 0x04},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x04},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x04},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd2},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x59e2, 0x08},
	{0x59e3, 0x03},
	{0x59e4, 0x00},
	{0x59e5, 0x10},
	{0x59e6, 0x06},
	{0x59e7, 0x00},
	{0x59e8, 0x08},
	{0x59e9, 0x02},
	{0x59ea, 0x00},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x28},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x36e9, 0x51},
	{0x37f9, 0x53},
	{0x0100, 0x01},
	{SENSOR_REG_DELAY, 0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2304,
		.height = 1296,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2304_1296_30fps_mipi_master,
	},
	{
		.width = 2304,
		.height = 1296,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2304_1296_30fps_mipi_slave,
	},
	{
		.width = 2304,
		.height = 1296,
		.fps = 40 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2304_1296_40fps_mipi_master,
	},
	{
		.width = 2304,
		.height = 1296,
		.fps = 40 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2304_1296_40fps_mipi_slave,
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

	ret = sensor_write(sd, 0x440d, 0x10);
	ret += sensor_write(sd, 0x4400, 0x11);
	private_msleep(4);

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
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret += sensor_write(sd, 0x3e00, (unsigned char) ((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char) ((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char) ((it & 0x0f) << 4));
	ret = sensor_write(sd, 0x3e07, (unsigned char) (again & 0xff));
	ret += sensor_write(sd, 0x3e09, (unsigned char) (((again >> 8) & 0xff)));

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
       int ret = 0;
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

       ret += sensor_write(sd, 0x3e07, (unsigned char)(value & 0xff));
       ret += sensor_write(sd, 0x3e09, (unsigned char)((value & 0xff00) >> 8));
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
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch (sensor->info.default_boot) {
		case 0:
			sclk = 99000000; /* 1250 * 1584 * 25 * 2 */
			max_fps = 25;
			break;
		case 1:
			sclk = 99000000; /* 1250 * 1584 * 25 * 2 */
			max_fps = 25;
			break;
		case 2:
			sclk = 148500000; /* 1250 * 1485 * 40 * 2 */
			max_fps = 40;
			break;
		case 3:
			sclk = 148500000; /* 1250 * 1485 * 40 * 2 */
			max_fps = 40;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x320c, &val);
	hts = val;
	ret += sensor_read(sd, 0x320d, &val);
	hts = (((hts << 8) | val) << 1);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char) (vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sensor_read(sd, 0x3221, &val);
	switch (enable) {
		case 0:
			val &= 0x99;
			break;
		case 1:
			val = ((val & 0x9F) | 0x06);
			break;
		case 2:
			val = ((val & 0xF9) | 0x60);
			break;
		case 3:
			val |= 0x66;
			break;
	}
	sensor_write(sd, 0x3221, val);

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

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
			memcpy(&(sensor_attr.mipi), &sensor_mipi_master, sizeof(sensor_mipi_master));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.max_integration_time_native = 1584 - 8;
			sensor_attr.integration_time_limit = 1584 - 8;
			sensor_attr.total_width = 2500;
			sensor_attr.total_height = 1584;
			sensor_attr.max_integration_time = 1584 - 8;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x520;
			break;
		case 1:
			wsize = &sensor_win_sizes[1];
			memcpy(&(sensor_attr.mipi), &sensor_mipi_slave, sizeof(sensor_mipi_slave));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.max_integration_time_native = 1584 - 8;
			sensor_attr.integration_time_limit = 1584 - 8;
			sensor_attr.total_width = 2500;
			sensor_attr.total_height = 1584;
			sensor_attr.max_integration_time = 1584 - 8;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x520;
			break;
		case 2:
			wsize = &sensor_win_sizes[2];
			memcpy(&(sensor_attr.mipi), &sensor_mipi_40fps_master, sizeof(sensor_mipi_40fps_master));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.max_integration_time_native = 1485 - 8;
			sensor_attr.integration_time_limit = 1485 - 8;
			sensor_attr.total_width = 2500;
			sensor_attr.total_height = 1485;
			sensor_attr.max_integration_time = 1485 - 8;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x520;
			break;
		case 3:
			wsize = &sensor_win_sizes[3];
			memcpy(&(sensor_attr.mipi), &sensor_mipi_40fps_slave, sizeof(sensor_mipi_40fps_slave));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.max_integration_time_native = 1485 - 8;
			sensor_attr.integration_time_limit = 1485 - 8;
			sensor_attr.total_width = 2500;
			sensor_attr.total_height = 1485;
			sensor_attr.max_integration_time = 1485 - 8;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x520;
			break;
		default:
			ISP_ERROR("Have no this Setting Source!!!\n");
	}

	sensor_attr.fsync_attr.mode = fsync_mode;
	if (fsync_mode == TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE) {
		sensor_attr.total_height = (sensor_attr.total_height << 1);
		wsize->fps = ((wsize->fps & 0xffff0000) | ((wsize->fps & 0xffff) << 1));
	}
	sensor_attr.max_integration_time_native = sensor_attr.total_height - 8;
	sensor_attr.integration_time_limit = sensor_attr.total_height - 8;
	sensor_attr.max_integration_time = sensor_attr.total_height - 8;

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
		case 2:
		case 3:
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

static int sensor_fsync(struct tx_isp_subdev *sd, struct tx_isp_sensor_fsync *fsync) {
	uint8_t ret;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	printk("=========>> [%s %d]\n", __func__, __LINE__);
	if (fsync->place != TX_ISP_SENSOR_FSYNC_PLACE_STREAMON_AFTER)
		return 0;
	switch (fsync->call_index) {
		case 0:
			switch (fsync_mode) {
				case 2:
					printk("=========>> [%s %d]\n", __func__, __LINE__);
					break;
				case 3:
					if (sensor->info.default_boot == 1) {
						ret += sensor_write(sd, 0x320e, 0x0c);
						ret += sensor_write(sd, 0x320f, 0x60);
					} else if (sensor->info.default_boot == 3) {
						ret += sensor_write(sd, 0x320e, 0x0b);
						ret += sensor_write(sd, 0x320f, 0x9a);
					}
					printk("=========>> [%s %d]\n", __func__, __LINE__);
					break;
			}
			break;
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
			if (arg)
				ret = sensor_set_expo(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			//if (arg)
			//	ret = sensor_set_integration_time(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			//if (arg)
			//	ret = sensor_set_analog_gain(sd, sensor_val->value);
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
	.fsync = sensor_fsync,
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
