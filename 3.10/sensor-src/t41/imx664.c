// SPDX-License-Identifier: GPL-2.0+
/*
 * imx664.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot	resolution      fps       interface	     mode
 *   0	  2688*1520       30	mipi_2lane	   linear
 *   1	  2688*1520       20	mipi_2lane	   dol
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

#define SENSOR_NAME "imx664"
#define SENSOR_CHIP_ID 0x2823
#define SENSOR_CHIP_ID_H (0x02)
#define SENSOR_CHIP_ID_L (0x98)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (148500000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20231103a"
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16

static int reset_gpio = -1;
static int pwdn_gpio = -1;

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;

static int wdr_bufsize = 5376000;


struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	uint16_t again=0;
	uint32_t hcg = 166528;//5.82x
	uint32_t hcg_thr = 196608;//20x 196608;//8x

	if (isp_gain >= hcg_thr) {
		isp_gain = isp_gain - hcg;
		*sensor_again = 0;
		*sensor_again |= 1 << 12;
	} else {
		*sensor_again = 0;
		hcg = 0;
	}
	again = (isp_gain * 20) >> LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again > AGAIN_MAX_DB + DGAIN_MAX_DB) again = AGAIN_MAX_DB + DGAIN_MAX_DB;

	*sensor_again += again;
	isp_gain =  (((int32_t) again) << LOG2_GAIN_SHIFT) / 20 + hcg;

	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	uint16_t again = 0;
	uint32_t hcg = 166528;//5.82x
	uint32_t hcg_thr = 196608;//20x 196608;//8x

	if (isp_gain >= hcg_thr) {
		isp_gain = isp_gain - hcg;
		*sensor_again = 0;
		*sensor_again |= 1 << 12;
	} else {
		*sensor_again = 0;
		hcg = 0;
	}
	again = (isp_gain * 20) >> LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again > AGAIN_MAX_DB + DGAIN_MAX_DB) again = AGAIN_MAX_DB + DGAIN_MAX_DB;

	*sensor_again += again;
	isp_gain = (((int32_t) again) << LOG2_GAIN_SHIFT) / 20 + hcg;

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi_linear = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 1188,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 2704,
	.image_theight = 1560,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 20,
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
	.mipi_sc.del_start = 1,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_dol = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 1782,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 2704,
	.image_theight = 1520,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 1,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x37,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 655360,
	.max_again_short = 655360,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.min_integration_time_short = 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.wdr_cache = 0,
};

static struct regval_list sensor_init_regs_2688_1520_30fps_mipi[] = {
	{0x3000, 0x01},  // STANDBY
	{0x3001, 0x00},  // REGHOLD
	{0x3002, 0x01},  // XMSTA
	{0x3014, 0x01},  // INCK_SEL[3:0]
	{0x3015, 0x04},  // DATARATE_SEL[3:0]
	{0x3018, 0x00},  // WINMODE[3:0]
	{0x301A, 0x00},  // WDMODE[7:0]
	{0x301B, 0x00},  // ADDMODE[1:0]
	{0x301C, 0x00},  // THIN_V_EN[7:0]
	{0x301E, 0x01},  // VCMODE[7:0]
	{0x3020, 0x00},  // HREVERSE
	{0x3021, 0x00},  // VREVERSE
	{0x3022, 0x02},  // ADBIT[1:0]
	{0x3023, 0x01},  // MDBIT
	{0x3028, 0x72},  // VMAX[19:0] 0x672 = 1650
	{0x3029, 0x06},  // VMAX[19:0]
	{0x302A, 0x00},  // VMAX[19:0]
	{0x302C, 0xDC},  // HMAX[15:0] 0x5dc = 1500
	{0x302D, 0x05},  // HMAX[15:0]
	{0x3030, 0x00},  // FDG_SEL0[1:0]
	{0x3031, 0x00},  // FDG_SEL1[1:0]
	{0x3032, 0x00},  // FDG_SEL2[1:0]
	{0x303C, 0x00},  // PIX_HST[12:0]
	{0x303D, 0x00},  // PIX_HST[12:0]
	{0x303E, 0x90},  // PIX_HWIDTH[12:0]
	{0x303F, 0x0A},  // PIX_HWIDTH[12:0]
	{0x3040, 0x01},  // LANEMODE[2:0]
	{0x3044, 0x00},  // PIX_VST[11:0]
	{0x3045, 0x00},  // PIX_VST[11:0]
	{0x3046, 0x04},  // PIX_VWIDTH[11:0]
	{0x3047, 0x06},  // PIX_VWIDTH[11:0]
	{0x304C, 0x00},  // GAIN_HG0[10:0]
	{0x304D, 0x00},  // GAIN_HG0[10:0]
	{0x3050, 0x08},  // SHR0[19:0]
	{0x3051, 0x00},  // SHR0[19:0]
	{0x3052, 0x00},  // SHR0[19:0]
	{0x3054, 0x1A},  // SHR1[19:0]
	{0x3055, 0x00},  // SHR1[19:0]
	{0x3056, 0x00},  // SHR1[19:0]
	{0x3058, 0x4C},  // SHR2[19:0]
	{0x3059, 0x00},  // SHR2[19:0]
	{0x305A, 0x00},  // SHR2[19:0]
	{0x3060, 0x32},  // RHS1[19:0]
	{0x3061, 0x00},  // RHS1[19:0]
	{0x3062, 0x00},  // RHS1[19:0]
	{0x3064, 0x6A},  // RHS2[19:0]
	{0x3065, 0x00},  // RHS2[19:0]
	{0x3066, 0x00},  // RHS2[19:0]
	{0x3070, 0x00},  // GAIN_0[10:0]
	{0x3071, 0x00},  // GAIN_0[10:0]
	{0x3072, 0x00},  // GAIN_1[10:0]
	{0x3073, 0x00},  // GAIN_1[10:0]
	{0x3074, 0x00},  // GAIN_2[10:0]
	{0x3075, 0x00},  // GAIN_2[10:0]
	{0x30A4, 0xAA},  // XVSOUTSEL[1:0]
	{0x30A6, 0x00},  // XVS_DRV[1:0]
	{0x30CC, 0x00},
	{0x30CD, 0x00},
	{0x30DC, 0x32},  // BLKLEVEL[9:0]
	{0x30DD, 0x40},  // BLKLEVEL[9:0]
	{0x3148, 0x00},
	{0x3400, 0x01},  // GAIN_PGC_FIDMD
	{0x3412, 0x01},
	{0x3460, 0x21},
	{0x3492, 0x08},
	{0x3930, 0x01},
	{0x3B1D, 0x17},
	{0x3C00, 0x0C},
	{0x3C01, 0x0C},
	{0x3C04, 0x02},
	{0x3C08, 0x1E},
	{0x3C09, 0x1D},
	{0x3C0A, 0x1D},
	{0x3C0B, 0x1D},
	{0x3C0C, 0x1D},
	{0x3C0D, 0x1D},
	{0x3C0E, 0x1C},
	{0x3C0F, 0x1B},
	{0x3C30, 0x73},
	{0x3C34, 0x03},
	{0x3C3C, 0x20},
	{0x3C44, 0x06},
	{0x3CB8, 0x00},
	{0x3CBA, 0xFF},
	{0x3CBB, 0x03},
	{0x3CBC, 0xFF},
	{0x3CBD, 0x03},
	{0x3CC2, 0xFF},
	{0x3CC8, 0xFF},
	{0x3CC9, 0x03},
	{0x3CCA, 0x00},
	{0x3CCE, 0xFF},
	{0x3CCF, 0x03},
	{0x3CD0, 0xFF},
	{0x3E00, 0x31},
	{0x3E02, 0x04},
	{0x3E03, 0x00},
	{0x3E20, 0x04},
	{0x3E21, 0x00},
	{0x3E22, 0x31},
	{0x3E24, 0xB0},
	{0x3E2C, 0x38},
	{0x4470, 0xFF},
	{0x4471, 0x00},
	{0x4478, 0x00},
	{0x4479, 0xFF},
	{0x4490, 0x05},
	{0x4494, 0x19},
	{0x4495, 0x00},
	{0x4496, 0x55},
	{0x4497, 0x01},
	{0x4498, 0xA1},
	{0x449A, 0xA0},
	{0x449C, 0x9B},
	{0x449E, 0x96},
	{0x44A0, 0x91},
	{0x44A2, 0x87},
	{0x44A4, 0x7D},
	{0x44A6, 0x78},
	{0x44B8, 0xA1},
	{0x44BA, 0xA0},
	{0x44BC, 0x9B},
	{0x44BE, 0x96},
	{0x44C0, 0x91},
	{0x44C2, 0x87},
	{0x44C4, 0x7D},
	{0x44C6, 0x78},
	{0x44C8, 0xB6},
	{0x44C9, 0x01},
	{0x44CA, 0xB6},
	{0x44CB, 0x01},
	{0x44CC, 0xB1},
	{0x44CD, 0x01},
	{0x44CE, 0xB1},
	{0x44CF, 0x01},
	{0x44D0, 0xA7},
	{0x44D1, 0x01},
	{0x44D2, 0xA2},
	{0x44D3, 0x01},
	{0x44D4, 0xA2},
	{0x44D5, 0x01},
	{0x44D6, 0x93},
	{0x44D7, 0x01},
	{0x44E8, 0xB6},
	{0x44E9, 0x01},
	{0x44EA, 0xB6},
	{0x44EB, 0x01},
	{0x44EC, 0xB1},
	{0x44ED, 0x01},
	{0x44EE, 0xB1},
	{0x44EF, 0x01},
	{0x44F0, 0xA7},
	{0x44F1, 0x01},
	{0x44F2, 0xA2},
	{0x44F3, 0x01},
	{0x44F4, 0xA2},
	{0x44F5, 0x01},
	{0x44F6, 0x93},
	{0x44F7, 0x01},
	{0x4534, 0x22},
	{0x4538, 0x22},
	{0x4539, 0x22},
	{0x453A, 0x22},
	{0x453B, 0x22},
	{0x453C, 0x22},
	{0x453D, 0x22},
	{0x453E, 0x22},
	{0x453F, 0x22},
	{0x4540, 0x22},
	{0x4544, 0x22},
	{0x4545, 0x22},
	{0x4546, 0x22},
	{0x4547, 0x22},
	{0x4548, 0x22},
	{0x4549, 0x22},
	{0x454A, 0x22},
	{0x454B, 0x22},
	{0x454C, 0x22},
	{0x4550, 0x22},
	{0x4551, 0x22},
	{0x4552, 0x22},
	{0x4553, 0x22},
	{0x4554, 0x22},
	{0x4555, 0x22},
	{0x4556, 0x22},
	{0x4557, 0x22},
	{0x4558, 0x22},
	{0x455C, 0x22},
	{0x455D, 0x22},
	{0x455E, 0x22},
	{0x455F, 0x22},
	{0x4560, 0x22},
	{0x4561, 0x22},
	{0x4562, 0x22},
	{0x4563, 0x22},
	{0x4564, 0x22},
	{0x4E30, 0x01},
	{0x4E31, 0x00},
	{0x4E32, 0x03},
	{0x4E33, 0x02},
	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0x18},
	{0x3002, 0x00},
	{0x30A5, 0x00},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sensor_init_regs_2688_1520_20fps_mipi_dol[] = {
	{0x3000, 0x01},  //STANDBY
	{0x3001, 0x00},  //REGHOLD
	{0x3002, 0x01},  //XMSTA
	{0x3014, 0x01},  //INCK_SEL[3:0]
	{0x3015, 0x02},  //DATARATE_SEL[3:0]
	{0x3018, 0x00},  //WINMODE[3:0]
	{0x301A, 0x01},  //WDMODE[7:0]
	{0x301B, 0x00},  //ADDMODE[1:0]
	{0x301C, 0x01},  //THIN_V_EN[7:0]
	{0x301E, 0x01},  //VCMODE[7:0]
	{0x3020, 0x00},  //HREVERSE
	{0x3021, 0x00},  //VREVERSE
	{0x3022, 0x00},  //ADBIT[1:0]
	{0x3023, 0x00},  //MDBIT
	{0x3028, 0xAA},  //VMAX[19:0] 0x9aa = 2474
	{0x3029, 0x09},  //VMAX[19:0]
	{0x302A, 0x00},  //VMAX[19:0]
	{0x302C, 0xEE},  //HMAX[15:0] 0x2ee = 750
	{0x302D, 0x02},  //HMAX[15:0]
	{0x3030, 0x00},  //FDG_SEL0[1:0]
	{0x3031, 0x00},  //FDG_SEL1[1:0]
	{0x3032, 0x00},  //FDG_SEL2[1:0]
	{0x303C, 0x00},  //PIX_HST[12:0]
	{0x303D, 0x00},  //PIX_HST[12:0]
	{0x303E, 0x90},  //PIX_HWIDTH[12:0]
	{0x303F, 0x0A},  //PIX_HWIDTH[12:0]
	{0x3040, 0x01},  //LANEMODE[2:0]
	{0x3044, 0x00},  //PIX_VST[11:0]
	{0x3045, 0x00},  //PIX_VST[11:0]
	{0x3046, 0x04},  //PIX_VWIDTH[11:0]
	{0x3047, 0x06},  //PIX_VWIDTH[11:0]
	{0x304C, 0x00},  //GAIN_HG0[10:0]
	{0x304D, 0x00},  //GAIN_HG0[10:0]
	{0x3050, 0x74},  //SHR0[19:0]
	{0x3051, 0x0F},  //SHR0[19:0]
	{0x3052, 0x00},  //SHR0[19:0]
	{0x3054, 0x0A},  //SHR1[19:0]
	{0x3055, 0x00},  //SHR1[19:0]
	{0x3056, 0x00},  //SHR1[19:0]
	{0x3058, 0x4C},  //SHR2[19:0]
	{0x3059, 0x00},  //SHR2[19:0]
	{0x305A, 0x00},  //SHR2[19:0]
	{0x3060, 0x36},  //RHS1[19:0] 0x6E = 110 -> 0x136 = 310
	{0x3061, 0x01},  //RHS1[19:0]
	{0x3062, 0x00},  //RHS1[19:0]
	{0x3064, 0x6A},  //RHS2[19:0]
	{0x3065, 0x00},  //RHS2[19:0]
	{0x3066, 0x00},  //RHS2[19:0]
	{0x3070, 0x00},  //GAIN_0[10:0]
	{0x3071, 0x00},  //GAIN_0[10:0]
	{0x3072, 0x00},  //GAIN_1[10:0]
	{0x3073, 0x00},  //GAIN_1[10:0]
	{0x3074, 0x00},  //GAIN_2[10:0]
	{0x3075, 0x00},  //GAIN_2[10:0]
	{0x30A4, 0xAA},  //XVSOUTSEL[1:0]
	{0x30A6, 0x00},  //XVS_DRV[1:0]
	{0x30CC, 0x00},
	{0x30CD, 0x00},
	{0x30DC, 0x32},  //BLKLEVEL[9:0]
	{0x30DD, 0x40},  //BLKLEVEL[9:0]
	{0x310C, 0x01},
	{0x3130, 0x01},
	{0x3148, 0x00},
	{0x315E, 0x10},
	{0x3400, 0x00},  //GAIN_PGC_FIDMD 0x01 -> 0x00
	{0x3412, 0x01},
	{0x3460, 0x21},
	{0x3492, 0x08},
	{0x3890, 0x08},  //HFR_EN[3:0]
	{0x3891, 0x00},  //HFR_EN[3:0]
	{0x3893, 0x00},
	{0x3930, 0x01},
	{0x3B1D, 0x17},
	{0x3C00, 0x0A},
	{0x3C01, 0x0A},
	{0x3C04, 0x02},
	{0x3C08, 0x1F},
	{0x3C09, 0x1F},
	{0x3C0A, 0x1F},
	{0x3C0B, 0x1F},
	{0x3C0C, 0x1F},
	{0x3C0D, 0x1F},
	{0x3C0E, 0x1F},
	{0x3C0F, 0x1F},
	{0x3C30, 0x73},
	{0x3C34, 0x03},
	{0x3C3C, 0x20},
	{0x3C44, 0x06},
	{0x3CB8, 0x00},
	{0x3CBA, 0xFF},
	{0x3CBB, 0x03},
	{0x3CBC, 0xFF},
	{0x3CBD, 0x03},
	{0x3CC2, 0xFF},
	{0x3CC8, 0xFF},
	{0x3CC9, 0x03},
	{0x3CCA, 0x00},
	{0x3CCE, 0xFF},
	{0x3CCF, 0x03},
	{0x3CD0, 0xFF},
	{0x3E00, 0x31},
	{0x3E02, 0x04},
	{0x3E03, 0x00},
	{0x3E20, 0x04},
	{0x3E21, 0x00},
	{0x3E22, 0x31},
	{0x3E24, 0xB0},
	{0x3E2C, 0x38},
	{0x4470, 0xFF},
	{0x4471, 0x00},
	{0x4478, 0x00},
	{0x4479, 0xFF},
	{0x4490, 0x05},
	{0x4494, 0x19},
	{0x4495, 0x00},
	{0x4496, 0x55},
	{0x4497, 0x01},
	{0x4498, 0xA1},
	{0x449A, 0xA0},
	{0x449C, 0x9B},
	{0x449E, 0x96},
	{0x44A0, 0x91},
	{0x44A2, 0x87},
	{0x44A4, 0x7D},
	{0x44A6, 0x78},
	{0x44B8, 0xA1},
	{0x44BA, 0xA0},
	{0x44BC, 0x9B},
	{0x44BE, 0x96},
	{0x44C0, 0x91},
	{0x44C2, 0x87},
	{0x44C4, 0x7D},
	{0x44C6, 0x78},
	{0x44C8, 0xB6},
	{0x44C9, 0x01},
	{0x44CA, 0xB6},
	{0x44CB, 0x01},
	{0x44CC, 0xB1},
	{0x44CD, 0x01},
	{0x44CE, 0xB1},
	{0x44CF, 0x01},
	{0x44D0, 0xA7},
	{0x44D1, 0x01},
	{0x44D2, 0xA2},
	{0x44D3, 0x01},
	{0x44D4, 0xA2},
	{0x44D5, 0x01},
	{0x44D6, 0x93},
	{0x44D7, 0x01},
	{0x44E8, 0xB6},
	{0x44E9, 0x01},
	{0x44EA, 0xB6},
	{0x44EB, 0x01},
	{0x44EC, 0xB1},
	{0x44ED, 0x01},
	{0x44EE, 0xB1},
	{0x44EF, 0x01},
	{0x44F0, 0xA7},
	{0x44F1, 0x01},
	{0x44F2, 0xA2},
	{0x44F3, 0x01},
	{0x44F4, 0xA2},
	{0x44F5, 0x01},
	{0x44F6, 0x93},
	{0x44F7, 0x01},
	{0x4534, 0x22},
	{0x4538, 0x22},
	{0x4539, 0x22},
	{0x453A, 0x22},
	{0x453B, 0x22},
	{0x453C, 0x22},
	{0x453D, 0x22},
	{0x453E, 0x22},
	{0x453F, 0x22},
	{0x4540, 0x22},
	{0x4544, 0x22},
	{0x4545, 0x22},
	{0x4546, 0x22},
	{0x4547, 0x22},
	{0x4548, 0x22},
	{0x4549, 0x22},
	{0x454A, 0x22},
	{0x454B, 0x22},
	{0x454C, 0x22},
	{0x4550, 0x22},
	{0x4551, 0x22},
	{0x4552, 0x22},
	{0x4553, 0x22},
	{0x4554, 0x22},
	{0x4555, 0x22},
	{0x4556, 0x22},
	{0x4557, 0x22},
	{0x4558, 0x22},
	{0x455C, 0x22},
	{0x455D, 0x22},
	{0x455E, 0x22},
	{0x455F, 0x22},
	{0x4560, 0x22},
	{0x4561, 0x22},
	{0x4562, 0x22},
	{0x4563, 0x22},
	{0x4564, 0x22},
	{0x4E30, 0x01},
	{0x4E31, 0x00},
	{0x4E32, 0x03},
	{0x4E33, 0x02},
	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0x18},
	{0x3002, 0x00},
	{0x30A5, 0x00},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution]. */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2688,
		.height = 1520,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_30fps_mipi,
	},
	{
		.width = 2688,
		.height = 1520,
		.fps = 20 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_20fps_mipi_dol,
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

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
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
#if 1
	unsigned char v;
	int ret;

	printk("[%s,%d]\n",__func__,__LINE__);
	ret = sensor_write(sd, 0x3000, 0x00);
	private_msleep(40);
	ret = sensor_read(sd, 0x4D1D, &v);
	//pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
	       return ret;
	if (v != SENSOR_CHIP_ID_H)
	       return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x4D1C, &v);
	//pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
	       return ret;
	if (v != SENSOR_CHIP_ID_L)
	       return -ENODEV;
	*ident = (*ident << 8) | v;
	ret = sensor_write(sd, 0x3000, 0x01);
#endif

	return 0;
}

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shs1 = 0;
	int rhs1 = 310;

	shs1 = rhs1 - (value << 2);
	ret = sensor_write(sd, 0x3054, (unsigned char)(shs1 & 0xff));
	ret += sensor_write(sd, 0x3055, (unsigned char)((shs1 >> 8) & 0xff));
	ret += sensor_write(sd, 0x3056, (unsigned char)((shs1 >> 16) & 0x3));

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR)
	{
		vmax = sensor_attr.total_height;
		shs = vmax - value;
		ret = sensor_write(sd, 0x3050, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3051, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3052, (unsigned char)((shs >> 16) & 0x0f));
	}
	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)
	{
		vmax = sensor_attr.total_height;
		shs = 2 * vmax - (value << 2);
		ret = sensor_write(sd, 0x3050, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3051, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3052, (unsigned char)((shs >> 16) & 0x0f));
	}
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	ret += sensor_write(sd, 0x3072, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3073, (unsigned char)((value >> 8) & 0xff));

	if (value & (1 << 12)) {
		ret += sensor_write(sd, 0x3031, 0x1);
	} else {
		ret += sensor_write(sd, 0x3031, 0x0);
	}
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	ret += sensor_write(sd, 0x3070, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3071, (unsigned char)((value >> 8) & 0xff));

	if (value & (1 << 12)) {
		ret += sensor_write(sd, 0x3030, 0x1);
	} else {
		ret += sensor_write(sd, 0x3030, 0x0);
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
	unsigned int sclk = 0;
	unsigned int aclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int rhs1 = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot) {
		case 0:
			sclk = 1500 * 1620 * 30;
			max_fps = TX_SENSOR_MAX_FPS_30;
			break;
		case 1:
			sclk = 750 * 2474 * 20;
			aclk = 310 * 20;
			max_fps = TX_SENSOR_MAX_FPS_20;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x302D, &val);
	hts = val;
	ret += sensor_read(sd, 0x302C, &val);
	hts = (hts << 8) + val;
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x3001, 0x01);
	ret = sensor_write(sd, 0x302A, (unsigned char)((vts & 0xf0000) >> 16));
	ret += sensor_write(sd, 0x3029, (unsigned char)((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x3028, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x3001, 0x00);
	if (0 != ret) {
		ISP_ERROR("err: sensor_write err\n");
		return ret;
	}

	if ((sensor->info.default_boot) == 0) {
	    sensor->video.fps = fps;
	    sensor->video.attr->max_integration_time_native = vts -8;
	    sensor->video.attr->integration_time_limit = vts -8;
	    sensor->video.attr->total_height = vts;
	    sensor->video.attr->max_integration_time = vts -8;
	} else if ((sensor->info.default_boot) == 1) {
	    rhs1 = aclk * (fps & 0xffff) / ((fps & 0xffff0000) >> 16);
	    rhs1 = ((rhs1 >> 2) << 2) + 2;
	    ret += sensor_write(sd, 0x3061, ((rhs1 >> 8) & 0xff));
	    ret += sensor_write(sd, 0x3060, (rhs1 & 0xff));

	    sensor->video.fps = fps;
	    sensor->video.attr->max_integration_time_native = (((vts * 2) - rhs1 - 10) / 4);
	    sensor->video.attr->integration_time_limit = (((vts * 2) - rhs1 - 10) / 4);
	    sensor->video.attr->total_height = vts;
	    sensor->video.attr->max_integration_time = (((vts * 2) - rhs1 - 10) / 4);
	    sensor->video.attr->max_integration_time_short = ((rhs1 - 10) / 4);
	}
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	uint8_t h_val;
	uint8_t v_val;

	ret = sensor_read(sd, 0x3020, &h_val);
	ret = sensor_read(sd, 0x3021, &v_val);
	switch(enable) {
		case 0:
			h_val &= 0xFE;
			v_val &= 0xFE;
			break;
		case 1:
			h_val |= 0x01;
			v_val &= 0xFE;
			break;
		case 2:
			h_val &= 0xFE;
			v_val |= 0x01;
			break;
		case 3:
			h_val |= 0x01;
			v_val |= 0x01;
			break;
	}
	ret = sensor_write(sd, 0x3020, h_val);
	ret = sensor_write(sd, 0x3021, v_val);

	return ret;
}

static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en) {
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;

	ret = sensor_write(sd, 0x3000, 0x1);
	if (wdr_en == 1) {
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_dol),sizeof(sensor_mipi_dol));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		wsize = &sensor_win_sizes[1];
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 655360;
		sensor_attr.max_again_short = 655360;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 1157;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 75;
		sensor_attr.integration_time_limit = 1157;
		sensor_attr.total_width = 750;
		sensor_attr.total_height = 2474;
		sensor_attr.max_integration_time = 1157;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor->video.attr = &sensor_attr;
		info->default_boot = 1;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0) {
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_linear),sizeof(sensor_mipi_linear));
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		wsize = &sensor_win_sizes[0];
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 655360;
		sensor_attr.max_again_short = 655360;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 4;
		sensor_attr.min_integration_time_native = 4;
		sensor_attr.max_integration_time_native = 1650 - 8;
		sensor_attr.integration_time_limit = 1650 - 8;
		sensor_attr.total_width = 1500;
		sensor_attr.total_height = 1650;
		sensor_attr.max_integration_time = 1650 - 8;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor->video.attr = &sensor_attr;
		info->default_boot = 0;
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
			sensor_attr.min_integration_time = 4;
			sensor_attr.min_integration_time_native = 4;
			sensor_attr.max_integration_time_native = 1650 - 8;
			sensor_attr.integration_time_limit = 1650 - 8;
			sensor_attr.total_width = 1500;
			sensor_attr.total_height = 1650;
			sensor_attr.max_integration_time = 1650 - 8;
			sensor_attr.integration_time_apply_delay = 2;
			sensor_attr.again_apply_delay = 2;
			sensor_attr.dgain_apply_delay = 0;
			memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_linear),sizeof(sensor_mipi_linear));
			break;
		case 1:
			wsize = &sensor_win_sizes[1];
			data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.wdr_cache = wdr_bufsize;
			sensor_attr.max_again = 655360;
			sensor_attr.max_again_short = 655360;
			sensor_attr.max_dgain = 0;
			sensor_attr.min_integration_time = 1;
			sensor_attr.min_integration_time_native = 1;
			sensor_attr.max_integration_time_native = 1157;
			sensor_attr.min_integration_time_short = 1;
			sensor_attr.max_integration_time_short = 75;
			sensor_attr.integration_time_limit = 1157;
			sensor_attr.total_width = 750;
			sensor_attr.total_height = 2474;
			sensor_attr.max_integration_time = 1157;
			sensor_attr.integration_time_apply_delay = 2;
			sensor_attr.again_apply_delay = 2;
			sensor_attr.dgain_apply_delay = 0;
			memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_dol),sizeof(sensor_mipi_dol));
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

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);
	if (((rate / 1000) % 37125) != 0) {
	       ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
	       sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
	       if (IS_ERR(sclka)) {
		       pr_err("get sclka failed\n");
	       } else {
		       rate = private_clk_get_rate(sclka);
		       if (((rate / 1000) % 37125) != 0) {
			       private_clk_set_rate(sclka, 891000000);
		       }
	       }
	}
		private_clk_set_rate(sensor->mclk, 37125000);
		private_clk_prepare_enable(sensor->mclk);

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
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
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
	sensor->video.shvflip = 1;
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

static __init int init_sensor(void) {
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for Sony imx664 sensor");
MODULE_LICENSE("GPL");
