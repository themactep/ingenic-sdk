// SPDX-License-Identifier: GPL-2.0+
/*
 * imx662.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       30        mipi_2lane           linear
 *   1          960*540           30        mipi_2lane           linear
 *   2          1280*720         30        mipi_2lane           linear
 * NOTE: SENSOR_CHIP_ID_H/_L are 0x00/0x00 placeholders and sensor_detect()
 *       only checks that the two raw bytes match them. The real IMX662 id
 *       (SENSOR_ATTR chip_id below = 0xb201) and the correct id registers
 *       are NOT verified against a datasheet.
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

#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_CHIP_ID ((SENSOR_CHIP_ID_H << 8) | SENSOR_CHIP_ID_L)
#define SENSOR_CHIP_ID_H (0x00)
#define SENSOR_CHIP_ID_L (0x00)
#define SENSOR_I2C_ADDRESS 0x1a
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_NAME "imx662"
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_REG_END 0xffff
#define SENSOR_SUPPORT_SCLK (74250000)
#define SENSOR_VERSION "H20230928a"

#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x8c
#define LOG2_GAIN_SHIFT 16

static int reset_gpio = GPIO_PC(27);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int wdr_bufsize = 230400; // cache lines corrponding on VPB1
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

static int rhs1 = 101;

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
	.chip_i2c_addr = SENSOR_I2C_ADDRESS,
	.width = SENSOR_MAX_WIDTH,
	.height = SENSOR_MAX_HEIGHT,
};

struct regval_list {
	uint16_t reg_num;
	uint16_t value;
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	uint16_t again = 0;
	uint32_t hcg = 166528;	   // 5.82x
	uint32_t hcg_thr = 196608; // 20x 196608;//8x

	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		hcg = 0;
	} else {
		if (isp_gain >= hcg_thr) {
			isp_gain = isp_gain - hcg;
			*sensor_again = 0;
			*sensor_again |= 1 << 12;
		} else {
			*sensor_again = 0;
			hcg = 0;
		}
	}
	again = (isp_gain * 20) >> LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again > AGAIN_MAX_DB + DGAIN_MAX_DB)
		again = AGAIN_MAX_DB + DGAIN_MAX_DB;

	*sensor_again += again;
	isp_gain = (((int32_t)again) << LOG2_GAIN_SHIFT) / 20 + hcg;

	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus mipi_linear = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 891,
	/* .clk = 1000, */
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 1920,
	.image_theight = 1080,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 21,
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

struct tx_isp_mipi_bus mipi_binning = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 891,
	/* .clk = 1000, */
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 968,
	.image_theight = 551,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 8,
	.mipi_sc.mipi_crop_start0y = 11,
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

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0xb201,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi =
		{
			.mode = SENSOR_MIPI_SONY_MODE,
			.clk = 445,
			.lans = 2,
			.settle_time_apative_en = 0,
			.image_twidth = 1920,
			.image_theight = 1080,
			.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
			.mipi_sc.hcrop_diff_en = 0,
			.mipi_sc.mipi_vcomp_en = 0,
			.mipi_sc.mipi_hcomp_en = 0,
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
			.mipi_sc.data_type_en = 1,
			.mipi_sc.data_type_value = RAW12,
			.mipi_sc.del_start = 1,
			.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
			.mipi_sc.sensor_fid_mode = 0,
			.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
		},

	.max_again =
		655360, // 30db->31.6x->327675, 72db->3981x->786432, 60.2db->1024x->657544,  54.2db->512x->591849 1024x->655360
	.max_again_short = 655360, // 786432,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 2496,
	.min_integration_time_short = 1,
	.max_integration_time_short = 98,
	.integration_time_limit = 2496,
	.total_width = 990,   // hmax
	.total_height = 2500, // 9c4 vmax
	.max_integration_time = 2496,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.wdr_cache = 0,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

#if 0
static struct regval_list sensor_init_regs_1920_1080_30fps_mipi_2dol_lcg[] = {
        {SENSOR_REG_END, 0x00},

};
#endif

static struct regval_list sensor_init_regs_960_540_30fps_mipi[] = {
	// IMX662-AAQR 2/2-line binning CSI-2_2lane 24MHz AD:10bit Output:12bit 1440Mbps Master Mode LCG Mode 30fps Integration Time 33.298ms
	{0x3000, 0x01}, // STANDBY
	{0x3001, 0x00}, // REGHOLD
	{0x3002, 0x01}, // XMSTA
	{0x3014, 0x04}, // INCK_SEL[3:0]
	{0x3015, 0x03}, // DATARATE_SEL[3:0]
	{0x3018, 0x00}, // WINMODE[3:0]
	{0x301a, 0x00}, // WDMODE
	{0x301b, 0x01}, // ADDMODE[1:0]
	{0x301c, 0x00}, // THIN_V_EN
	{0x301e, 0x01}, // VCMODE
	{0x3020, 0x00}, // HREVERSE
	{0x3021, 0x00}, // VREVERSE
	{0x3022, 0x00}, // ADBIT
	{0x3023, 0x01}, // MDBIT
	{0x3028, 0xa6}, // VMAX[19:0]  //
	{0x3029, 0x0e}, // VMAX[19:0]
	{0x302a, 0x00}, // VMAX[19:0]
	{0x302c, 0x94}, // HMAX[15:0]
	{0x302d, 0x02}, // HMAX[15:0]
	{0x3030, 0x00}, // FDG_SEL0[1:0]
	{0x3031, 0x00}, // FDG_SEL1[1:0]
	{0x3032, 0x00}, // FDG_SEL2[1:0]
	{0x303c, 0x00}, // PIX_HST[12:0]
	{0x303d, 0x00}, // PIX_HST[12:0]
	{0x303e, 0x90}, // PIX_HWIDTH[12:0]
	{0x303f, 0x07}, // PIX_HWIDTH[12:0]
	{0x3040, 0x01}, // LANEMODE[2:0]
	{0x3044, 0x00}, // PIX_VST[11:0]
	{0x3045, 0x00}, // PIX_VST[11:0]
	{0x3046, 0x4c}, // PIX_VWIDTH[11:0]
	{0x3047, 0x04}, // PIX_VWIDTH[11:0]
	{0x3050, 0x04}, // SHR0[19:0]
	{0x3051, 0x00}, // SHR0[19:0]
	{0x3052, 0x00}, // SHR0[19:0]
	{0x3054, 0x0e}, // SHR1[19:0]
	{0x3055, 0x00}, // SHR1[19:0]
	{0x3056, 0x00}, // SHR1[19:0]
	{0x3058, 0x8a}, // SHR2[19:0]
	{0x3059, 0x01}, // SHR2[19:0]
	{0x305a, 0x00}, // SHR2[19:0]
	{0x3060, 0x16}, // RHS1[19:0]
	{0x3061, 0x01}, // RHS1[19:0]
	{0x3062, 0x00}, // RHS1[19:0]
	{0x3064, 0xc4}, // RHS2[19:0]
	{0x3065, 0x0c}, // RHS2[19:0]
	{0x3066, 0x00}, // RHS2[19:0]
	{0x3069, 0x00}, // CHDR_GAIN_EN
	{0x3070, 0x00}, // GAIN[10:0]
	{0x3071, 0x00}, // GAIN[10:0]
	{0x3072, 0x00}, // GAIN_1[10:0]
	{0x3073, 0x00}, // GAIN_1[10:0]
	{0x3074, 0x00}, // GAIN_2[10:0]
	{0x3075, 0x00}, // GAIN_2[10:0]
	{0x3081, 0x00}, // EXP_GAIN
	{0x308c, 0x00}, // CHDR_DGAIN0_HG[15:0]
	{0x308d, 0x01}, // CHDR_DGAIN0_HG[15:0]
	{0x3094, 0x00}, // CHDR_AGAIN0_LG[10:0]
	{0x3095, 0x00}, // CHDR_AGAIN0_LG[10:0]
	{0x3096, 0x00}, // CHDR_AGAIN1[10:0]
	{0x3097, 0x00}, // CHDR_AGAIN1[10:0]
	{0x309c, 0x00}, // CHDR_AGAIN0_HG[10:0]
	{0x309d, 0x00}, // CHDR_AGAIN0_HG[10:0]
	{0x30a4, 0xaa}, // XVSOUTSEL[1:0]
	{0x30a6, 0x00}, // XVS_DRV[1:0]
	{0x30cc, 0x00}, // -
	{0x30cd, 0x00}, // -
	{0x30dc, 0x32}, // BLKLEVEL[11:0]
	{0x30dd, 0x40}, // BLKLEVEL[11:0]
	{0x3400, 0x01}, // GAIN_PGC_FIDMD
	{0x3444, 0xac}, // -
	{0x3460, 0x21}, // -
	{0x3492, 0x08}, // -
	{0x3a50, 0x62}, // -
	{0x3a51, 0x01}, // -
	{0x3a52, 0x19}, // -
	{0x3b00, 0x39}, // -
	{0x3b23, 0x2d}, // -
	{0x3b45, 0x04}, // -
	{0x3c0a, 0x1f}, // -
	{0x3c0b, 0x1e}, // -
	{0x3c38, 0x21}, // -
	{0x3c40, 0x06}, // -
	{0x3c44, 0x00}, // -
	{0x3cb6, 0xd8}, // -
	{0x3cc4, 0xda}, // -
	{0x3e24, 0x79}, // -
	{0x3e2c, 0x15}, // -
	{0x3edc, 0x2d}, // -
	{0x4498, 0x05}, // -
	{0x449c, 0x19}, // -
	{0x449d, 0x00}, // -
	{0x449e, 0x32}, // -
	{0x449f, 0x01}, // -
	{0x44a0, 0x92}, // -
	{0x44a2, 0x91}, // -
	{0x44a4, 0x8c}, // -
	{0x44a6, 0x87}, // -
	{0x44a8, 0x82}, // -
	{0x44aa, 0x78}, // -
	{0x44ac, 0x6e}, // -
	{0x44ae, 0x69}, // -
	{0x44b0, 0x92}, // -
	{0x44b2, 0x91}, // -
	{0x44b4, 0x8c}, // -
	{0x44b6, 0x87}, // -
	{0x44b8, 0x82}, // -
	{0x44ba, 0x78}, // -
	{0x44bc, 0x6e}, // -
	{0x44be, 0x69}, // -
	{0x44c0, 0x7f}, // -
	{0x44c1, 0x01}, // -
	{0x44c2, 0x7f}, // -
	{0x44c3, 0x01}, // -
	{0x44c4, 0x7a}, // -
	{0x44c5, 0x01}, // -
	{0x44c6, 0x7a}, // -
	{0x44c7, 0x01}, // -
	{0x44c8, 0x70}, // -
	{0x44c9, 0x01}, // -
	{0x44ca, 0x6b}, // -
	{0x44cb, 0x01}, // -
	{0x44cc, 0x6b}, // -
	{0x44cd, 0x01}, // -
	{0x44ce, 0x5c}, // -
	{0x44cf, 0x01}, // -
	{0x44d0, 0x7f}, // -
	{0x44d1, 0x01}, // -
	{0x44d2, 0x7f}, // -
	{0x44d3, 0x01}, // -
	{0x44d4, 0x7a}, // -
	{0x44d5, 0x01}, // -
	{0x44d6, 0x7a}, // -
	{0x44d7, 0x01}, // -
	{0x44d8, 0x70}, // -
	{0x44d9, 0x01}, // -
	{0x44da, 0x6b}, // -
	{0x44db, 0x01}, // -
	{0x44dc, 0x6b}, // -
	{0x44dd, 0x01}, // -
	{0x44de, 0x5c}, // -
	{0x44df, 0x01}, // -
	{0x4534, 0x1c}, // -
	{0x4535, 0x03}, // -
	{0x4538, 0x1c}, // -
	{0x4539, 0x1c}, // -
	{0x453a, 0x1c}, // -
	{0x453b, 0x1c}, // -
	{0x453c, 0x1c}, // -
	{0x453d, 0x1c}, // -
	{0x453e, 0x1c}, // -
	{0x453f, 0x1c}, // -
	{0x4540, 0x1c}, // -
	{0x4541, 0x03}, // -
	{0x4542, 0x03}, // -
	{0x4543, 0x03}, // -
	{0x4544, 0x03}, // -
	{0x4545, 0x03}, // -
	{0x4546, 0x03}, // -
	{0x4547, 0x03}, // -
	{0x4548, 0x03}, // -
	{0x4549, 0x03}, // -
	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0x18},
	{0x3002, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	// IMX662-AAQR Window cropping 1920x1084 CSI-2_2lane 24MHz AD:12bit Output:12bit 1440Mbps Master Mode LCG Mode 30fps Integration Time 33.28ms
	// Ver4.0

	{0x3000, 0x01}, // STANDBY
	{0x3001, 0x00}, // REGHOLD
	{0x3002, 0x01}, // XMSTA
	{0x3014, 0x04}, // INCK_SEL[3:0]
	{0x3015, 0x03}, // DATARATE_SEL[3:0]
	{0x3018, 0x04}, // WINMODE[3:0]
	{0x301a, 0x00}, // WDMODE
	{0x301b, 0x00}, // ADDMODE[1:0]
	{0x301c, 0x00}, // THIN_V_EN
	{0x301e, 0x01}, // VCMODE
	{0x3020, 0x00}, // HREVERSE
	{0x3021, 0x00}, // VREVERSE
	{0x3022, 0x01}, // ADBIT
	{0x3023, 0x01}, // MDBIT
	{0x3028, 0xc4}, // VMAX[19:0] //2500
	{0x3029, 0x09}, // VMAX[19:0]
	{0x302a, 0x00}, // VMAX[19:0]
	{0x302c, 0xde}, // HMAX[15:0] //990
	{0x302d, 0x03}, // HMAX[15:0]
	{0x3030, 0x00}, // FDG_SEL0[1:0]
	{0x3031, 0x00}, // FDG_SEL1[1:0]
	{0x3032, 0x00}, // FDG_SEL2[1:0]
	{0x303c, 0x08}, // PIX_HST[12:0]
	{0x303d, 0x00}, // PIX_HST[12:0]
	{0x303e, 0x80}, // PIX_HWIDTH[12:0]
	{0x303f, 0x07}, // PIX_HWIDTH[12:0]
	{0x3040, 0x01}, // LANEMODE[2:0]
	{0x3044, 0x08}, // PIX_VST[11:0]
	{0x3045, 0x00}, // PIX_VST[11:0]
	{0x3046, 0x3c}, // PIX_VWIDTH[11:0]
	{0x3047, 0x04}, // PIX_VWIDTH[11:0]
	{0x3050, 0x04}, // SHR0[19:0]
	{0x3051, 0x00}, // SHR0[19:0]
	{0x3052, 0x00}, // SHR0[19:0]
	{0x3054, 0x0e}, // SHR1[19:0]
	{0x3055, 0x00}, // SHR1[19:0]
	{0x3056, 0x00}, // SHR1[19:0]
	{0x3058, 0x8a}, // SHR2[19:0]
	{0x3059, 0x01}, // SHR2[19:0]
	{0x305a, 0x00}, // SHR2[19:0]
	{0x3060, 0x16}, // RHS1[19:0]
	{0x3061, 0x01}, // RHS1[19:0]
	{0x3062, 0x00}, // RHS1[19:0]
	{0x3064, 0xc4}, // RHS2[19:0]
	{0x3065, 0x0c}, // RHS2[19:0]
	{0x3066, 0x00}, // RHS2[19:0]
	{0x3069, 0x00}, // CHDR_GAIN_EN
	{0x3070, 0x00}, // GAIN[10:0]
	{0x3071, 0x00}, // GAIN[10:0]
	{0x3072, 0x00}, // GAIN_1[10:0]
	{0x3073, 0x00}, // GAIN_1[10:0]
	{0x3074, 0x00}, // GAIN_2[10:0]
	{0x3075, 0x00}, // GAIN_2[10:0]
	{0x3081, 0x00}, // EXP_GAIN
	{0x308c, 0x00}, // CHDR_DGAIN0_HG[15:0]
	{0x308d, 0x01}, // CHDR_DGAIN0_HG[15:0]
	{0x3094, 0x00}, // CHDR_AGAIN0_LG[10:0]
	{0x3095, 0x00}, // CHDR_AGAIN0_LG[10:0]
	{0x3096, 0x00}, // CHDR_AGAIN1[10:0]
	{0x3097, 0x00}, // CHDR_AGAIN1[10:0]
	{0x309c, 0x00}, // CHDR_AGAIN0_HG[10:0]
	{0x309d, 0x00}, // CHDR_AGAIN0_HG[10:0]
	{0x30a4, 0xaa}, // XVSOUTSEL[1:0]
	{0x30a6, 0x00}, // XVS_DRV[1:0]
	{0x30cc, 0x00}, // -
	{0x30cd, 0x00}, // -
	{0x30dc, 0x32}, // BLKLEVEL[11:0]
	{0x30dd, 0x40}, // BLKLEVEL[11:0]
	{0x3400, 0x01}, // GAIN_PGC_FIDMD
	{0x3444, 0xac}, // -
	{0x3460, 0x21}, // -
	{0x3492, 0x08}, // -
	{0x3a50, 0xff}, // -
	{0x3a51, 0x03}, // -
	{0x3a52, 0x00}, // -
	{0x3b00, 0x39}, // -
	{0x3b23, 0x2d}, // -
	{0x3b45, 0x04}, // -
	{0x3c0a, 0x1f}, // -
	{0x3c0b, 0x1e}, // -
	{0x3c38, 0x21}, // -
	{0x3c40, 0x06}, // -
	{0x3c44, 0x00}, // -
	{0x3cb6, 0xd8}, // -
	{0x3cc4, 0xda}, // -
	{0x3e24, 0x79}, // -
	{0x3e2c, 0x15}, // -
	{0x3edc, 0x2d}, // -
	{0x4498, 0x05}, // -
	{0x449c, 0x19}, // -
	{0x449d, 0x00}, // -
	{0x449e, 0x32}, // -
	{0x449f, 0x01}, // -
	{0x44a0, 0x92}, // -
	{0x44a2, 0x91}, // -
	{0x44a4, 0x8c}, // -
	{0x44a6, 0x87}, // -
	{0x44a8, 0x82}, // -
	{0x44aa, 0x78}, // -
	{0x44ac, 0x6e}, // -
	{0x44ae, 0x69}, // -
	{0x44b0, 0x92}, // -
	{0x44b2, 0x91}, // -
	{0x44b4, 0x8c}, // -
	{0x44b6, 0x87}, // -
	{0x44b8, 0x82}, // -
	{0x44ba, 0x78}, // -
	{0x44bc, 0x6e}, // -
	{0x44be, 0x69}, // -
	{0x44c0, 0x7f}, // -
	{0x44c1, 0x01}, // -
	{0x44c2, 0x7f}, // -
	{0x44c3, 0x01}, // -
	{0x44c4, 0x7a}, // -
	{0x44c5, 0x01}, // -
	{0x44c6, 0x7a}, // -
	{0x44c7, 0x01}, // -
	{0x44c8, 0x70}, // -
	{0x44c9, 0x01}, // -
	{0x44ca, 0x6b}, // -
	{0x44cb, 0x01}, // -
	{0x44cc, 0x6b}, // -
	{0x44cd, 0x01}, // -
	{0x44ce, 0x5c}, // -
	{0x44cf, 0x01}, // -
	{0x44d0, 0x7f}, // -
	{0x44d1, 0x01}, // -
	{0x44d2, 0x7f}, // -
	{0x44d3, 0x01}, // -
	{0x44d4, 0x7a}, // -
	{0x44d5, 0x01}, // -
	{0x44d6, 0x7a}, // -
	{0x44d7, 0x01}, // -
	{0x44d8, 0x70}, // -
	{0x44d9, 0x01}, // -
	{0x44da, 0x6b}, // -
	{0x44db, 0x01}, // -
	{0x44dc, 0x6b}, // -
	{0x44dd, 0x01}, // -
	{0x44de, 0x5c}, // -
	{0x44df, 0x01}, // -
	{0x4534, 0x1c}, // -
	{0x4535, 0x03}, // -
	{0x4538, 0x1c}, // -
	{0x4539, 0x1c}, // -
	{0x453a, 0x1c}, // -
	{0x453b, 0x1c}, // -
	{0x453c, 0x1c}, // -
	{0x453d, 0x1c}, // -
	{0x453e, 0x1c}, // -
	{0x453f, 0x1c}, // -
	{0x4540, 0x1c}, // -
	{0x4541, 0x03}, // -
	{0x4542, 0x03}, // -
	{0x4543, 0x03}, // -
	{0x4544, 0x03}, // -
	{0x4545, 0x03}, // -
	{0x4546, 0x03}, // -
	{0x4547, 0x03}, // -
	{0x4548, 0x03}, // -
	{0x4549, 0x03}, // -

	/* {0x30e0, 0x00},//colorbar */
	/* {0x30e2, 0x01}, */

	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0x18},
	{0x3002, 0x00},

	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1948*1109 [0]*/
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
	/* 960*540 [1]*/
	{
		.width = 960,
		.height = 540,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_960_540_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_mipi[] = {
	{0x3000, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x3000, 0x01},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value) {
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
	struct i2c_msg msg[2] = {[0] =
					 {
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
		}};

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value) {
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
                        private_msleep(vals->value);
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
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x4d1c, &v);
	ISP_INFO("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x4d1d, &v);
	ISP_INFO("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shs1 = 0;

	// short frame use shs1
	shs1 = rhs1 - value - 1;
	ret += sensor_write(sd, 0x3020, (unsigned char)(shs1 & 0xff));
	ret += sensor_write(sd, 0x3021, (unsigned char)((shs1 >> 8) & 0xff));
	ret += sensor_write(sd, 0x3022, (unsigned char)((shs1 >> 16) & 0x03));

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		vmax = sensor_attr.total_height;
		shs = vmax - value;
		ret += sensor_write(sd, 0x3050, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3051, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3052, (unsigned char)((shs >> 16) & 0x03));
	} else {
		// long frame use shs2
		vmax = sensor_attr.total_height;
		shs = vmax - value - 1;
		ret += sensor_write(sd, 0x3024, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3025, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3026, (unsigned char)((shs >> 16) & 0x03));
	}

	if (0 != ret) {
		ISP_ERROR("err: sensor_write err\n");
		return ret;
	}

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	ret = sensor_write(sd, 0x30f2, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		ret += sensor_write(sd, 0x3070, (unsigned char)(value & 0xff));
		ret += sensor_write(sd, 0x3071, (unsigned char)((value >> 8) & 0x07));

		if (value & (1 << 12)) {
			ret += sensor_write(sd, 0x3030, 0x01);
		} else {
			ret += sensor_write(sd, 0x3030, 0x00);
		}

	} else {
		ret += sensor_write(sd, 0x3014, (unsigned char)(value & 0xff));
	}
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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
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
			ISP_INFO("%s stream on\n", SENSOR_NAME);
		}

	} else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		ISP_INFO("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int pclk = 0;
	unsigned short hmax = 0;
	unsigned short vmax = 0;
	unsigned short cur_int = 0;
	unsigned short shs = 0;
	unsigned char value = 0;
	unsigned int newformat = 0; // the format is 24.8

	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)
		return 0;
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	pclk = SENSOR_SUPPORT_SCLK;

	/*method 2 change vts*/
	ret += sensor_read(sd, 0x302c, &value);
	hmax = value;
	ret += sensor_read(sd, 0x302d, &value);
	hmax = (value << 8) | hmax;

	vmax = pclk * (fps & 0xffff) / hmax / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x3028, vmax & 0xff);
	ret += sensor_write(sd, 0x3029, (vmax >> 8) & 0xff);
	ret += sensor_write(sd, 0x302a, (vmax >> 16) & 0x0f);

	ISP_INFO("hmax is 0x%x, vmax is 0x%x\n", hmax, vmax);

	/*record current integration time*/
	ret += sensor_read(sd, 0x3050, &value);
	shs = value;
	ret += sensor_read(sd, 0x3051, &value);
	shs = (value << 8) | shs;
	ret += sensor_read(sd, 0x3052, &value);
	shs = ((value & 0x03) << 16) | shs;
	cur_int = sensor->video.attr->total_height - shs - 4;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vmax - 4;
	sensor->video.attr->integration_time_limit = vmax - 4;
	sensor->video.attr->total_height = vmax;
	sensor->video.attr->max_integration_time = vmax - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	ret = sensor_set_integration_time(sd, cur_int);
	if (ret < 0)
		return -1;

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
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

	return 0;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	// struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch (info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.max_again = 655360;
		sensor_attr.max_again_short = 655360;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 2496;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 98;
		sensor_attr.integration_time_limit = 2496;
		sensor_attr.total_width = 990;
		sensor_attr.total_height = 2500;
		sensor_attr.max_integration_time = 2496;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		memcpy((void *)(&(sensor_attr.mipi)), (void *)(&mipi_linear), sizeof(mipi_linear));
		break;
	case 1:
		wsize = &sensor_win_sizes[1];
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.max_again = 655360;
		sensor_attr.max_again_short = 655360;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 3746;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 98;
		sensor_attr.integration_time_limit = 3746;
		sensor_attr.total_width = 660;
		sensor_attr.total_height = 3750;
		sensor_attr.max_integration_time = 3746;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		memcpy((void *)(&(sensor_attr.mipi)), (void *)(&mipi_binning), sizeof(mipi_binning));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
	case TISP_SENSOR_VI_MIPI_CSI1:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 0;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

	switch (info->mclk) {
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

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	rate = private_clk_get_rate(sensor->mclk);
	/**   if (((rate / 1000) % (MCLK / 1000)) != 0) {
        uint8_t sclk_name_num = sizeof(sclk_name)/sizeof(sclk_name[0]);
        for (i=0; i < sclk_name_num; i++) {
            tclk = private_devm_clk_get(&client->dev, sclk_name[i]);
            ret = clk_set_parent(sclka, clk_get(NULL, sclk_name[i]));
            if (IS_ERR(tclk)) {
                ISP_ERROR("get sclka failed\n");
            } else {
                rate = private_clk_get_rate(tclk);
                if (i == sclk_name_num - 1 && ((rate / 1000) % (MCLK / 1000)) != 0) {
                    if (((MCLK / 1000) % 27000) != 0 || ((MCLK / 1000) % 37125) != 0)
                        private_clk_set_rate(tclk, 891000000);
                    else if (((MCLK / 1000) % 24000) != 0)
                        private_clk_set_rate(tclk, 1200000000);
                } else if (((rate / 1000) % (MCLK / 1000)) == 0) {
                    break;
                }
            }
        }
    }
*/
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;
	sensor_set_attr(sd, wsize);
	// sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(100);
		} else {
			ISP_ERROR("gpio request failed %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request failed %d\n", pwdn_gpio);
		}
	}
	/* while (1) */
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			client->addr,
			client->adapter->name,
			SENSOR_NAME);
		return ret;
	}
	ISP_INFO("%s chip found @ 0x%02x (%s)\n", SENSOR_NAME, client->addr, client->adapter->name);
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
	// struct tx_isp_initarg *init = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if (arg)
			ret = sensor_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if (arg)
			ret = sensor_set_analog_gain_short(sd, sensor_val->value);
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
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, sensor_val->value);
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

static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = SENSOR_NAME,
	.id = -1,
	.dev =
		{
			.dma_mask = &tx_isp_module_dma_mask,
			.coherent_dma_mask = 0xffffffff,
			.platform_data = NULL,
		},
	.num_resources = 0,
};

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	sd = &sensor->sd;
	video = &sensor->video;

	sensor->video.attr = &sensor_attr;
	sensor_attr.expo_fs = 1;
	sensor->dev = &client->dev;
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

	ISP_INFO("probe ok ------->%s\n", SENSOR_NAME);

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
	tx_isp_subdev_deinit(sd);

	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {{SENSOR_NAME, 0}, {}};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver =
		{
			.owner = THIS_MODULE,
			.name = SENSOR_NAME,
		},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void) {
	sensor_common_init(&sensor_info);
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for " SENSOR_NAME " sensor");
MODULE_LICENSE("GPL");
