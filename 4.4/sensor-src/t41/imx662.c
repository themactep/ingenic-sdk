// SPDX-License-Identifier: GPL-2.0+
/*
 * imx662.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot	resolution      fps       interface	      mode
 *   0	  1920*1080       30	mipi_2lane	   linear
 *   1	  960*540	   30	mipi_2lane	   linear
 *   2	  1280*720	 30	mipi_2lane	   linear
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

#define SENSOR_NAME "imx662"
#define SENSOR_CHIP_ID_H (0x00)
#define SENSOR_CHIP_ID_L (0x00)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (74250000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230518"
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

static int wdr_bufsize = 230400;//cache lines corrponding on VPB1
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

static int rhs1 = 101;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	uint16_t again = 0;
	uint32_t hcg = 166528;//5.82x
	uint32_t hcg_thr = 196608;//20x 196608;//8x

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
	if (again > AGAIN_MAX_DB + DGAIN_MAX_DB) again = AGAIN_MAX_DB + DGAIN_MAX_DB;

	*sensor_again += again;
	isp_gain= (((int32_t) again) << LOG2_GAIN_SHIFT) / 20 + hcg;

	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus mipi_2dol_lcg = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 891,
	/* .clk = 1200, */
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 1952,
	.image_theight = 1109,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 1,
	.mipi_sc.mipi_crop_start0x = 16,
	.mipi_sc.mipi_crop_start0y = 12,
	.mipi_sc.mipi_crop_start1x = 16,
	.mipi_sc.mipi_crop_start1y = 62,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 1,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_NOT_VC_MODE,
};


struct tx_isp_mipi_bus mipi_crop_720p = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 891,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 1920,
	.image_theight = 1080,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 320,
	.mipi_sc.mipi_crop_start0y = 201,
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

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0xb201,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
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

	.max_again = 655360,//30db->31.6x->327675, 72db->3981x->786432, 60.2db->1024x->657544,  54.2db->512x->591849 1024x->655360
	.max_again_short = 655360,//786432,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 2496,
	.min_integration_time_short = 1,
	.max_integration_time_short = 98,
	.integration_time_limit = 2496,
	.total_width = 990,//hmax
	.total_height = 2500,//9c4 vmax
	.max_integration_time = 2496,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.wdr_cache = 0,
	//      void priv; /* point to struct tx_isp_sensor_board_info */
};

#if 0
static struct regval_list sensor_init_regs_1920_1080_30fps_mipi_2dol_lcg[] = {
	{SENSOR_REG_END, 0x00},
};
#endif

static struct regval_list sensor_init_regs_960_540_30fps_mipi[] = {
//  IMX662-AAQR 2/2-line binning CSI-2_2lane 24MHz AD:10bit Output:12bit 1440Mbps Master Mode LCG Mode 30fps Integration Time 33.298ms
	{0x3000, 0x01},  // STANDBY
	{0x3001, 0x00},  // REGHOLD
	{0x3002, 0x01},  // XMSTA
	{0x3014, 0x04},  // INCK_SEL[3:0]
	{0x3015, 0x03},  // DATARATE_SEL[3:0]
	{0x3018, 0x00},  // WINMODE[3:0]
	{0x301A, 0x00},  // WDMODE
	{0x301B, 0x01},  // ADDMODE[1:0]
	{0x301C, 0x00},  // THIN_V_EN
	{0x301E, 0x01},  // VCMODE
	{0x3020, 0x00},  // HREVERSE
	{0x3021, 0x00},  // VREVERSE
	{0x3022, 0x00},  // ADBIT
	{0x3023, 0x01},  // MDBIT
	{0x3028, 0xA6},  // VMAX[19:0]  //
	{0x3029, 0x0E},  // VMAX[19:0]
	{0x302A, 0x00},  // VMAX[19:0]
	{0x302C, 0x94},  // HMAX[15:0]
	{0x302D, 0x02},  // HMAX[15:0]
	{0x3030, 0x00},  // FDG_SEL0[1:0]
	{0x3031, 0x00},  // FDG_SEL1[1:0]
	{0x3032, 0x00},  // FDG_SEL2[1:0]
	{0x303C, 0x00},  // PIX_HST[12:0]
	{0x303D, 0x00},  // PIX_HST[12:0]
	{0x303E, 0x90},  // PIX_HWIDTH[12:0]
	{0x303F, 0x07},  // PIX_HWIDTH[12:0]
	{0x3040, 0x01},  // LANEMODE[2:0]
	{0x3044, 0x00},  // PIX_VST[11:0]
	{0x3045, 0x00},  // PIX_VST[11:0]
	{0x3046, 0x4C},  // PIX_VWIDTH[11:0]
	{0x3047, 0x04},  // PIX_VWIDTH[11:0]
	{0x3050, 0x04},  // SHR0[19:0]
	{0x3051, 0x00},  // SHR0[19:0]
	{0x3052, 0x00},  // SHR0[19:0]
	{0x3054, 0x0E},  // SHR1[19:0]
	{0x3055, 0x00},  // SHR1[19:0]
	{0x3056, 0x00},  // SHR1[19:0]
	{0x3058, 0x8A},  // SHR2[19:0]
	{0x3059, 0x01},  // SHR2[19:0]
	{0x305A, 0x00},  // SHR2[19:0]
	{0x3060, 0x16},  // RHS1[19:0]
	{0x3061, 0x01},  // RHS1[19:0]
	{0x3062, 0x00},  // RHS1[19:0]
	{0x3064, 0xC4},  // RHS2[19:0]
	{0x3065, 0x0C},  // RHS2[19:0]
	{0x3066, 0x00},  // RHS2[19:0]
	{0x3069, 0x00},  // CHDR_GAIN_EN
	{0x3070, 0x00},  // GAIN[10:0]
	{0x3071, 0x00},  // GAIN[10:0]
	{0x3072, 0x00},  // GAIN_1[10:0]
	{0x3073, 0x00},  // GAIN_1[10:0]
	{0x3074, 0x00},  // GAIN_2[10:0]
	{0x3075, 0x00},  // GAIN_2[10:0]
	{0x3081, 0x00},  // EXP_GAIN
	{0x308C, 0x00},  // CHDR_DGAIN0_HG[15:0]
	{0x308D, 0x01},  // CHDR_DGAIN0_HG[15:0]
	{0x3094, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3095, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3096, 0x00},  // CHDR_AGAIN1[10:0]
	{0x3097, 0x00},  // CHDR_AGAIN1[10:0]
	{0x309C, 0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x309D, 0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x30A4, 0xAA},  // XVSOUTSEL[1:0]
	{0x30A6, 0x00},  // XVS_DRV[1:0]
	{0x30CC, 0x00},  // -
	{0x30CD, 0x00},  // -
	{0x30DC, 0x32},  // BLKLEVEL[11:0]
	{0x30DD, 0x40},  // BLKLEVEL[11:0]
	{0x3400, 0x01},  // GAIN_PGC_FIDMD
	{0x3444, 0xAC},  // -
	{0x3460, 0x21},  // -
	{0x3492, 0x08},  // -
	{0x3A50, 0x62},  // -
	{0x3A51, 0x01},  // -
	{0x3A52, 0x19},  // -
	{0x3B00, 0x39},  // -
	{0x3B23, 0x2D},  // -
	{0x3B45, 0x04},  // -
	{0x3C0A, 0x1F},  // -
	{0x3C0B, 0x1E},  // -
	{0x3C38, 0x21},  // -
	{0x3C40, 0x06},  // -
	{0x3C44, 0x00},  // -
	{0x3CB6, 0xD8},  // -
	{0x3CC4, 0xDA},  // -
	{0x3E24, 0x79},  // -
	{0x3E2C, 0x15},  // -
	{0x3EDC, 0x2D},  // -
	{0x4498, 0x05},  // -
	{0x449C, 0x19},  // -
	{0x449D, 0x00},  // -
	{0x449E, 0x32},  // -
	{0x449F, 0x01},  // -
	{0x44A0, 0x92},  // -
	{0x44A2, 0x91},  // -
	{0x44A4, 0x8C},  // -
	{0x44A6, 0x87},  // -
	{0x44A8, 0x82},  // -
	{0x44AA, 0x78},  // -
	{0x44AC, 0x6E},  // -
	{0x44AE, 0x69},  // -
	{0x44B0, 0x92},  // -
	{0x44B2, 0x91},  // -
	{0x44B4, 0x8C},  // -
	{0x44B6, 0x87},  // -
	{0x44B8, 0x82},  // -
	{0x44BA, 0x78},  // -
	{0x44BC, 0x6E},  // -
	{0x44BE, 0x69},  // -
	{0x44C0, 0x7F},  // -
	{0x44C1, 0x01},  // -
	{0x44C2, 0x7F},  // -
	{0x44C3, 0x01},  // -
	{0x44C4, 0x7A},  // -
	{0x44C5, 0x01},  // -
	{0x44C6, 0x7A},  // -
	{0x44C7, 0x01},  // -
	{0x44C8, 0x70},  // -
	{0x44C9, 0x01},  // -
	{0x44CA, 0x6B},  // -
	{0x44CB, 0x01},  // -
	{0x44CC, 0x6B},  // -
	{0x44CD, 0x01},  // -
	{0x44CE, 0x5C},  // -
	{0x44CF, 0x01},  // -
	{0x44D0, 0x7F},  // -
	{0x44D1, 0x01},  // -
	{0x44D2, 0x7F},  // -
	{0x44D3, 0x01},  // -
	{0x44D4, 0x7A},  // -
	{0x44D5, 0x01},  // -
	{0x44D6, 0x7A},  // -
	{0x44D7, 0x01},  // -
	{0x44D8, 0x70},  // -
	{0x44D9, 0x01},  // -
	{0x44DA, 0x6B},  // -
	{0x44DB, 0x01},  // -
	{0x44DC, 0x6B},  // -
	{0x44DD, 0x01},  // -
	{0x44DE, 0x5C},  // -
	{0x44DF, 0x01},  // -
	{0x4534, 0x1C},  // -
	{0x4535, 0x03},  // -
	{0x4538, 0x1C},  // -
	{0x4539, 0x1C},  // -
	{0x453A, 0x1C},  // -
	{0x453B, 0x1C},  // -
	{0x453C, 0x1C},  // -
	{0x453D, 0x1C},  // -
	{0x453E, 0x1C},  // -
	{0x453F, 0x1C},  // -
	{0x4540, 0x1C},  // -
	{0x4541, 0x03},  // -
	{0x4542, 0x03},  // -
	{0x4543, 0x03},  // -
	{0x4544, 0x03},  // -
	{0x4545, 0x03},  // -
	{0x4546, 0x03},  // -
	{0x4547, 0x03},  // -
	{0x4548, 0x03},  // -
	{0x4549, 0x03},  // -
	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0x18},
	{0x3002, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
//  IMX662-AAQR Window cropping 1920x1084 CSI-2_2lane 24MHz AD:12bit Output:12bit 1440Mbps Master Mode LCG Mode 30fps Integration Time 33.28ms
//  Ver4.0

	{0x3000, 0x01},  // STANDBY
	{0x3001, 0x00},  // REGHOLD
	{0x3002, 0x01},  // XMSTA
	{0x3014, 0x04},  // INCK_SEL[3:0]
	{0x3015, 0x03},  // DATARATE_SEL[3:0]
	{0x3018, 0x04},  // WINMODE[3:0]
	{0x301A, 0x00},  // WDMODE
	{0x301B, 0x00},  // ADDMODE[1:0]
	{0x301C, 0x00},  // THIN_V_EN
	{0x301E, 0x01},  // VCMODE
	{0x3020, 0x00},  // HREVERSE
	{0x3021, 0x00},  // VREVERSE
	{0x3022, 0x01},  // ADBIT
	{0x3023, 0x01},  // MDBIT
	{0x3028, 0xC4},  // VMAX[19:0] //2500
	{0x3029, 0x09},  // VMAX[19:0]
	{0x302A, 0x00},  // VMAX[19:0]
	{0x302C, 0xDE},  // HMAX[15:0] //990
	{0x302D, 0x03},  // HMAX[15:0]
	{0x3030, 0x00},  // FDG_SEL0[1:0]
	{0x3031, 0x00},  // FDG_SEL1[1:0]
	{0x3032, 0x00},  // FDG_SEL2[1:0]
	{0x303C, 0x08},  // PIX_HST[12:0]
	{0x303D, 0x00},  // PIX_HST[12:0]
	{0x303E, 0x80},  // PIX_HWIDTH[12:0]
	{0x303F, 0x07},  // PIX_HWIDTH[12:0]
	{0x3040, 0x01},  // LANEMODE[2:0]
	{0x3044, 0x08},  // PIX_VST[11:0]
	{0x3045, 0x00},  // PIX_VST[11:0]
	{0x3046, 0x3C},  // PIX_VWIDTH[11:0]
	{0x3047, 0x04},  // PIX_VWIDTH[11:0]
	{0x3050, 0x04},  // SHR0[19:0]
	{0x3051, 0x00},  // SHR0[19:0]
	{0x3052, 0x00},  // SHR0[19:0]
	{0x3054, 0x0E},  // SHR1[19:0]
	{0x3055, 0x00},  // SHR1[19:0]
	{0x3056, 0x00},  // SHR1[19:0]
	{0x3058, 0x8A},  // SHR2[19:0]
	{0x3059, 0x01},  // SHR2[19:0]
	{0x305A, 0x00},  // SHR2[19:0]
	{0x3060, 0x16},  // RHS1[19:0]
	{0x3061, 0x01},  // RHS1[19:0]
	{0x3062, 0x00},  // RHS1[19:0]
	{0x3064, 0xC4},  // RHS2[19:0]
	{0x3065, 0x0C},  // RHS2[19:0]
	{0x3066, 0x00},  // RHS2[19:0]
	{0x3069, 0x00},  // CHDR_GAIN_EN
	{0x3070, 0x00},  // GAIN[10:0]
	{0x3071, 0x00},  // GAIN[10:0]
	{0x3072, 0x00},  // GAIN_1[10:0]
	{0x3073, 0x00},  // GAIN_1[10:0]
	{0x3074, 0x00},  // GAIN_2[10:0]
	{0x3075, 0x00},  // GAIN_2[10:0]
	{0x3081, 0x00},  // EXP_GAIN
	{0x308C, 0x00},  // CHDR_DGAIN0_HG[15:0]
	{0x308D, 0x01},  // CHDR_DGAIN0_HG[15:0]
	{0x3094, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3095, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3096, 0x00},  // CHDR_AGAIN1[10:0]
	{0x3097, 0x00},  // CHDR_AGAIN1[10:0]
	{0x309C, 0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x309D, 0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x30A4, 0xAA},  // XVSOUTSEL[1:0]
	{0x30A6, 0x00},  // XVS_DRV[1:0]
	{0x30CC, 0x00},  // -
	{0x30CD, 0x00},  // -
	{0x30DC, 0x32},  // BLKLEVEL[11:0]
	{0x30DD, 0x40},  // BLKLEVEL[11:0]
	{0x3400, 0x01},  // GAIN_PGC_FIDMD
	{0x3444, 0xAC},  // -
	{0x3460, 0x21},  // -
	{0x3492, 0x08},  // -
	{0x3A50, 0xFF},  // -
	{0x3A51, 0x03},  // -
	{0x3A52, 0x00},  // -
	{0x3B00, 0x39},  // -
	{0x3B23, 0x2D},  // -
	{0x3B45, 0x04},  // -
	{0x3C0A, 0x1F},  // -
	{0x3C0B, 0x1E},  // -
	{0x3C38, 0x21},  // -
	{0x3C40, 0x06},  // -
	{0x3C44, 0x00},  // -
	{0x3CB6, 0xD8},  // -
	{0x3CC4, 0xDA},  // -
	{0x3E24, 0x79},  // -
	{0x3E2C, 0x15},  // -
	{0x3EDC, 0x2D},  // -
	{0x4498, 0x05},  // -
	{0x449C, 0x19},  // -
	{0x449D, 0x00},  // -
	{0x449E, 0x32},  // -
	{0x449F, 0x01},  // -
	{0x44A0, 0x92},  // -
	{0x44A2, 0x91},  // -
	{0x44A4, 0x8C},  // -
	{0x44A6, 0x87},  // -
	{0x44A8, 0x82},  // -
	{0x44AA, 0x78},  // -
	{0x44AC, 0x6E},  // -
	{0x44AE, 0x69},  // -
	{0x44B0, 0x92},  // -
	{0x44B2, 0x91},  // -
	{0x44B4, 0x8C},  // -
	{0x44B6, 0x87},  // -
	{0x44B8, 0x82},  // -
	{0x44BA, 0x78},  // -
	{0x44BC, 0x6E},  // -
	{0x44BE, 0x69},  // -
	{0x44C0, 0x7F},  // -
	{0x44C1, 0x01},  // -
	{0x44C2, 0x7F},  // -
	{0x44C3, 0x01},  // -
	{0x44C4, 0x7A},  // -
	{0x44C5, 0x01},  // -
	{0x44C6, 0x7A},  // -
	{0x44C7, 0x01},  // -
	{0x44C8, 0x70},  // -
	{0x44C9, 0x01},  // -
	{0x44CA, 0x6B},  // -
	{0x44CB, 0x01},  // -
	{0x44CC, 0x6B},  // -
	{0x44CD, 0x01},  // -
	{0x44CE, 0x5C},  // -
	{0x44CF, 0x01},  // -
	{0x44D0, 0x7F},  // -
	{0x44D1, 0x01},  // -
	{0x44D2, 0x7F},  // -
	{0x44D3, 0x01},  // -
	{0x44D4, 0x7A},  // -
	{0x44D5, 0x01},  // -
	{0x44D6, 0x7A},  // -
	{0x44D7, 0x01},  // -
	{0x44D8, 0x70},  // -
	{0x44D9, 0x01},  // -
	{0x44DA, 0x6B},  // -
	{0x44DB, 0x01},  // -
	{0x44DC, 0x6B},  // -
	{0x44DD, 0x01},  // -
	{0x44DE, 0x5C},  // -
	{0x44DF, 0x01},  // -
	{0x4534, 0x1C},  // -
	{0x4535, 0x03},  // -
	{0x4538, 0x1C},  // -
	{0x4539, 0x1C},  // -
	{0x453A, 0x1C},  // -
	{0x453B, 0x1C},  // -
	{0x453C, 0x1C},  // -
	{0x453D, 0x1C},  // -
	{0x453E, 0x1C},  // -
	{0x453F, 0x1C},  // -
	{0x4540, 0x1C},  // -
	{0x4541, 0x03},  // -
	{0x4542, 0x03},  // -
	{0x4543, 0x03},  // -
	{0x4544, 0x03},  // -
	{0x4545, 0x03},  // -
	{0x4546, 0x03},  // -
	{0x4547, 0x03},  // -
	{0x4548, 0x03},  // -
	{0x4549, 0x03},  // -

	/* {0x30e0, 0x0},//colorbar */
	/* {0x30e2, 0x1}, */

	{0x3000, 0x00 },
	{SENSOR_REG_DELAY, 0x18},
	{0x3002, 0x00 },

	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution]. */
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
	/* 1280*720 [0]*/
	{
		.width = 1280,
		.height = 720,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
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
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
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
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x4d1d, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
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

	//short frame use shs1
	shs1 = rhs1 - value - 1;
	ret = sensor_write(sd, 0x3020, (unsigned char)(shs1 & 0xff));
	ret += sensor_write(sd, 0x3021, (unsigned char)((shs1 >> 8) & 0xff));
	ret += sensor_write(sd, 0x3022, (unsigned char)((shs1 >> 16) & 0x3));

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		vmax = sensor_attr.total_height;
		shs = vmax - value;
		ret = sensor_write(sd, 0x3050, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3051, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3052, (unsigned char)((shs >> 16) & 0x3));
	} else {
		//long frame use shs2
		vmax = sensor_attr.total_height;
		shs = vmax - value - 1;
		ret = sensor_write(sd, 0x3024, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3025, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3026, (unsigned char)((shs >> 16) & 0x3));
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
		ret += sensor_write(sd, 0x3071, (unsigned char)((value >> 8) & 0x7));

		if (value & (1 << 12)) {
			ret += sensor_write(sd, 0x3030, 0x1);
		} else {
			ret += sensor_write(sd, 0x3030, 0x0);
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
	int ret = 0;
	unsigned int pclk = 0;
	unsigned short hmax = 0;
	unsigned short vmax = 0;
	unsigned short cur_int = 0;
	unsigned short shs = 0;
	unsigned char value = 0;
	unsigned int newformat = 0; //the format is 24.8

	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)
		return 0;
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		pr_debug("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	pclk = SENSOR_SUPPORT_SCLK;

	/*method 2 change vts*/
	ret = sensor_read(sd, 0x302c, &value);
	hmax = value;
	ret += sensor_read(sd, 0x302d, &value);
	hmax = (value << 8) | hmax;

	vmax = pclk * (fps & 0xffff) / hmax / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x3028, vmax & 0xff);
	ret += sensor_write(sd, 0x3029, (vmax >> 8) & 0xff);
	ret += sensor_write(sd, 0x302a, (vmax >> 16) & 0x0f);

	printk("hmax is 0x%x, vmax is 0x%x\n", hmax, vmax);

	/*record current integration time*/
	ret = sensor_read(sd, 0x3050, &value);
	shs = value;
	ret += sensor_read(sd, 0x3051, &value);
	shs = (value << 8) | shs;
	ret += sensor_read(sd, 0x3052, &value);
	shs = ((value & 0x03) << 16) | shs;
	cur_int = sensor->video.attr->total_height - shs -4;

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

static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en) {
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;
	/* struct timeval tv; */

	/* do_gettimeofday(&tv); */
	/* pr_debug("%d:before:time is %d.%d\n", __LINE__,tv.tv_sec,tv.tv_usec); */
	ret = sensor_write(sd, 0x3000, 0x1);
	if (wdr_en == 1) {
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_2dol_lcg),sizeof(mipi_2dol_lcg));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = data_type;
		sensor_attr.wdr_cache = wdr_bufsize;

		sensor_attr.max_again = 655360;//786432,
		sensor_attr.max_again_short = 655360;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 1176;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 98;
		sensor_attr.integration_time_limit = 1176;
		sensor_attr.total_width = 2136;
		sensor_attr.total_height = 2781;
		sensor_attr.max_integration_time = 1176;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;

		sensor->video.attr = &sensor_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0) {
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_linear),sizeof(mipi_linear));
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.data_type = data_type;
		wsize = &sensor_win_sizes[0];

		sensor_attr.data_type = data_type;
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 655360;
		sensor_attr.max_again_short = 655360;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 2496;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 98;
		sensor_attr.integration_time_limit = 2496;
		sensor_attr.total_width = 5280;
		sensor_attr.total_height = 1125;
		sensor_attr.max_integration_time = 2496;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;

		sensor->video.attr = &sensor_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sensor_set_wdr(struct tx_isp_subdev *sd, int wdr_en) {
	int ret = 0;

	private_gpio_direction_output(reset_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);

	ret = sensor_write_array(sd, wsize->regs);
	ret = sensor_write_array(sd, sensor_stream_on_mipi);

	return 0;
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

struct clk *sclka;

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
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
			memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_linear),sizeof(mipi_linear));
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
			memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_binning),sizeof(mipi_binning));
			break;
		case 2:
			wsize = &sensor_win_sizes[2];
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
			memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_crop_720p),sizeof(mipi_crop_720p));
			break;
		case 3:
			wsize = &sensor_win_sizes[2];
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.wdr_cache = wdr_bufsize;
			sensor_attr.max_again = 655360;
			sensor_attr.max_again_short = 655360;
			sensor_attr.max_dgain = 0;
			sensor_attr.min_integration_time = 1;
			sensor_attr.min_integration_time_native = 1;
			sensor_attr.max_integration_time_native = 1176;
			sensor_attr.min_integration_time_short = 1;
			sensor_attr.max_integration_time_short = 98;
			sensor_attr.integration_time_limit = 1176;
			sensor_attr.total_width = 2136;
			sensor_attr.total_height = 2781;
			sensor_attr.max_integration_time = 1176;
			sensor_attr.integration_time_apply_delay = 2;
			sensor_attr.again_apply_delay = 2;
			sensor_attr.dgain_apply_delay = 0;
			memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_2dol_lcg),sizeof(mipi_2dol_lcg));
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

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot, wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

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
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(100);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	/* while (1) */
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

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_initarg *init = arg;

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
		case TX_ISP_EVENT_SENSOR_WDR:
			if (arg)
				ret = sensor_set_wdr(sd, init->enable);
			break;
		case TX_ISP_EVENT_SENSOR_WDR_STOP:
			if (arg)
				ret = sensor_set_wdr_stop(sd, init->enable);
			break;
		default:
			break;
	}

	return 0;
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

static struct tx_isp_subdev_sensor_ops  sensor_sensor_ops = {
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


static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
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

MODULE_DESCRIPTION("A low-level driver for Sony imx662 sensor");
MODULE_LICENSE("GPL");
