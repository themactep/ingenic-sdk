// SPDX-License-Identifier: GPL-2.0+
/*
 * AR1337.c
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
#include <soc/gpio.h>
#include <tx-isp-common.h>
#include <sensor-common.h>
#include <sensor-info.h>
#include <txx-funcs.h>

#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_CHIP_ID ((SENSOR_CHIP_ID_H << 8) | SENSOR_CHIP_ID_L)
#define SENSOR_CHIP_ID_H (0x02)
#define SENSOR_CHIP_ID_L (0x53)
#define SENSOR_I2C_ADDRESS 0x36
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_NAME "ar1337"
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_REG_END 0xffff
#define SENSOR_SUPPORT_25FPS_SCLK (441567840)
#define SENSOR_VERSION "H20211008a"


static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int test_gpio = GPIO_PA(15);
module_param(test_gpio, int, S_IRUGO);
MODULE_PARM_DESC(test_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x2010, 0},
	{0x2018, 38336},
	{0x2020, 65536},
	{0x2024, 86633},
	{0x2028, 103872},
	{0x202c, 118446},
	{0x2030, 131072},
	{0x2032, 142208},
	{0x2034, 152169},
	{0x2036, 161181},
	{0x2038, 169408},
	{0x203a, 176976},
	{0x203c, 183982},
	{0x203e, 190505},
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
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi =
		{
			.mode = SENSOR_MIPI_OTHER_MODE,
			.clk = 510,
			.lans = 2,
			.settle_time_apative_en = 0,
			.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
			.mipi_sc.hcrop_diff_en = 0,
			.mipi_sc.mipi_vcomp_en = 1,
			.mipi_sc.mipi_hcomp_en = 1,
			.mipi_sc.line_sync_mode = 0,
			.mipi_sc.work_start_flag = 0,
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
			.mipi_sc.data_type_en = 0,
			.mipi_sc.data_type_value = RAW10,
			.mipi_sc.del_start = 0,
			.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
			.mipi_sc.sensor_fid_mode = 0,
			.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
		},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 190505,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1645 - 4,
	.integration_time_limit = 1645 - 4,
	.total_width = 4652,
	.total_height = 1645,
	.max_integration_time = 1645 - 4,
	.one_line_expr_in_us = 28,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
	{SENSOR_REG_DELAY, 100},
	{0x0103, 0x01},
	{SENSOR_REG_DELAY, 100},
	{SENSOR_REG_DELAY, 100},
	{0x3042, 0x500c}, // DARK_CONTROL2
	{0x3044, 0x0580}, // DARK_CONTROL
	{0x32ba, 0x0000}, // PDAF_SC_VISUAL_TAG
	{0x30ee, 0x5133}, // DARK_CONTROL3
	{0x30d2, 0x0000}, // CRM_CONTROL
	{0x3c50, 0x0001}, // BPC_SETUP
	{0x31e0, 0x0001}, // PIX_DEF_ID
	{0x3180, 0x9434}, // FINEDIGCORR_CONTROL
	{0x3f2c, 0x2028}, // GTH_THRES_RTN
	{0x3c58, 0x03ff}, // BPC_BLOOM_TH
	{0x3c5a, 0x00fd}, // BPC_TAG_MODE
	{0x3c5c, 0x000a}, // BPC_QR_SETUP
	{0x3c5e, 0x000a}, // BPC_GP_NUM_OF
	{0x3c60, 0x0500}, // BPC_GP_SIGMA_SETUP
	{0x3c62, 0xeca0}, // BPC_PC_MASK
	{0x3c64, 0x000f}, // BPC_PC_SIGMA
	{0x3c66, 0x002a}, // BPC_NOISE_LIFT
	{0x3c68, 0x0000}, // BPC_BOISE_CEOF
	{0x3c6a, 0x0006}, // BPC_NOISE_FLOOR0
	{0x3c6c, 0x000a}, // BPC_NOISE_FLOOR1
	{0x3c6e, 0x000e}, // BPC_NOISE_FLOOR2
	{0x3c70, 0x0012}, // BPC_NOISE_FLOOR3
	{0x3c72, 0x2010}, // BPC_NOISE_GAIN_TH10
	{0x3c74, 0x0040}, // BPC_NOISE_GAIN_TH2
	{0x3c78, 0x0000}, // BPC_NOISE_ADD_GP
	{0x3c7a, 0x0210}, // BPC_DEFECT
	{0x3c7c, 0x000a}, // BPC_SF_SIGMA
	{0x3c7e, 0x20ff}, // BPC_DF_SIGMA10
	{0x3c80, 0x0810}, // BPC_DF_SIGMA32
	{0x3c82, 0x20ff}, // BPC_DC_SIGMA10
	{0x3c84, 0x0810}, // BPC_DC_SIGMA32
	{0x3c86, 0x4210}, // BPC_DC_F
	{0x3c88, 0x0000}, // BPC_DEN_EN10
	{0x3c8a, 0x0000}, // BPC_DEN_EN32
	{0x3c8c, 0x0010}, // BPC_DEN_SIGMA
	{0x3c8e, 0x03f5}, // BPC_DEN_DIS_TH
	{0x3c90, 0x0000}, // BPC_TRI_ABYP_SIGMA
	{0x3c94, 0x47e0}, // BPC_PDAF_REC_SETUP1
	{0x3c96, 0x6000}, // BPC_PDAF_TAG_GRAD_EN
	{0x3c98, 0x0000}, // BPC_PDAF_REC_TOL_ABS0
	{0x3c9a, 0x0000}, // BPC_PDAF_REC_TOL_ABS1
	{0x3c9c, 0x0000}, // BPC_PDAF_REC_TOL_ABS2
	{0x3c9e, 0x0000}, // BPC_PDAF_REC_TOL_ABS3
	{0x3ca0, 0x0000}, // BPC_PDAF_REC_TOL_ABS4
	{0x3ca2, 0x0000}, // BPC_PDAF_REC_TOL_ABS5
	{0x3ca4, 0x0000}, // BPC_PDAF_REC_TOL_ABS6
	{0x3ca6, 0x0000}, // BPC_PDAF_REC_TOL_ABS7
	{0x3cc0, 0x0000}, // BPC_PDAF_REC_TOL_DARK
	{0x3cc2, 0x03e8}, // BPC_BM_T0
	{0x3cc4, 0x07d0}, // BPC_BM_T1
	{0x3cc6, 0x0bb8}, // BPC_BM_T2
	{0x3222, 0x00c0}, // PDAF_ROW_CONTROL
	{0x322e, 0x0000}, // PDAF_PEDESTAL
	{0x3230, 0x0ff0}, // PDAF_SAT_TH
	{0x32bc, 0x0019}, // PDAF_DC_TH_ABS
	{0x32be, 0x0013}, // PDAF_DC_TH_REL
	{0x32c0, 0x0010}, // PDAF_DC_DIFF_FACTOR
	{0x32ba, 0x0044}, // PDAF recon improvement in binning
	{0x3d00, 0x0446},
	{0x3d02, 0x4c66},
	{0x3d04, 0xffff},
	{0x3d06, 0xffff},
	{0x3d08, 0x5e40},
	{0x3d0a, 0x1106},
	{0x3d0c, 0x8041},
	{0x3d0e, 0x4f83},
	{0x3d10, 0x4200},
	{0x3d12, 0xc055},
	{0x3d14, 0x805b},
	{0x3d16, 0x8360},
	{0x3d18, 0x845a},
	{0x3d1a, 0x8d00},
	{0x3d1c, 0xc083},
	{0x3d1e, 0x4292},
	{0x3d20, 0x5a52},
	{0x3d22, 0x8453},
	{0x3d24, 0x6410},
	{0x3d26, 0x3080},
	{0x3d28, 0x1c00},
	{0x3d2a, 0xce57},
	{0x3d2c, 0x568b},
	{0x3d2e, 0x5150},
	{0x3d30, 0x804d},
	{0x3d32, 0x4382},
	{0x3d34, 0x5280},
	{0x3d36, 0x5858},
	{0x3d38, 0xa443},
	{0x3d3a, 0x9a45},
	{0x3d3c, 0x9245},
	{0x3d3e, 0xa752},
	{0x3d40, 0x9451},
	{0x3d42, 0xe651},
	{0x3d44, 0x8610},
	{0x3d46, 0xc49c},
	{0x3d48, 0x4f86},
	{0x3d4a, 0x5959},
	{0x3d4c, 0xe642},
	{0x3d4e, 0x9361},
	{0x3d50, 0x8262},
	{0x3d52, 0x8342},
	{0x3d54, 0x8141},
	{0x3d56, 0x64ff},
	{0x3d58, 0xffb7},
	{0x3d5a, 0x4081},
	{0x3d5c, 0x4080},
	{0x3d5e, 0x4180},
	{0x3d60, 0x4280},
	{0x3d62, 0x438d},
	{0x3d64, 0x44ba},
	{0x3d66, 0x4488},
	{0x3d68, 0x4380},
	{0x3d6a, 0x4241},
	{0x3d6c, 0x8140},
	{0x3d6e, 0x8240},
	{0x3d70, 0x8041},
	{0x3d72, 0x8042},
	{0x3d74, 0x8043},
	{0x3d76, 0x8d44},
	{0x3d78, 0xba44},
	{0x3d7a, 0x875e},
	{0x3d7c, 0x4354},
	{0x3d7e, 0x4241},
	{0x3d80, 0x8140},
	{0x3d82, 0x815b},
	{0x3d84, 0x8160},
	{0x3d86, 0x2600},
	{0x3d88, 0x5580},
	{0x3d8a, 0x7000},
	{0x3d8c, 0x8040},
	{0x3d8e, 0x4c81},
	{0x3d90, 0x45c3},
	{0x3d92, 0x4581},
	{0x3d94, 0x4c40},
	{0x3d96, 0x8070},
	{0x3d98, 0x8040},
	{0x3d9a, 0x4c85},
	{0x3d9c, 0x6ca8},
	{0x3d9e, 0x6c8c},
	{0x3da0, 0x000e},
	{0x3da2, 0xbe44},
	{0x3da4, 0x8844},
	{0x3da6, 0xbc78},
	{0x3da8, 0x0900},
	{0x3daa, 0x8904},
	{0x3dac, 0x8080},
	{0x3dae, 0x0240},
	{0x3db0, 0x8609},
	{0x3db2, 0x008e},
	{0x3db4, 0x0900},
	{0x3db6, 0x8002},
	{0x3db8, 0x4080},
	{0x3dba, 0x0480},
	{0x3dbc, 0x887c},
	{0x3dbe, 0xaa86},
	{0x3dc0, 0x0900},
	{0x3dc2, 0x877a},
	{0x3dc4, 0x000e},
	{0x3dc6, 0xc379},
	{0x3dc8, 0x4c40},
	{0x3dca, 0xbf70},
	{0x3dcc, 0x5e40},
	{0x3dce, 0x114e},
	{0x3dd0, 0x5d41},
	{0x3dd2, 0x5383},
	{0x3dd4, 0x4200},
	{0x3dd6, 0xc055},
	{0x3dd8, 0xa400},
	{0x3dda, 0xc083},
	{0x3ddc, 0x4288},
	{0x3dde, 0x6083},
	{0x3de0, 0x5b80},
	{0x3de2, 0x5a64},
	{0x3de4, 0x1030},
	{0x3de6, 0x801c},
	{0x3de8, 0x00a5},
	{0x3dea, 0x5697},
	{0x3dec, 0x57a5},
	{0x3dee, 0x5180},
	{0x3df0, 0x505a},
	{0x3df2, 0x814d},
	{0x3df4, 0x8358},
	{0x3df6, 0x8058},
	{0x3df8, 0xa943},
	{0x3dfa, 0x8345},
	{0x3dfc, 0xb045},
	{0x3dfe, 0x8343},
	{0x3e00, 0xa351},
	{0x3e02, 0xe251},
	{0x3e04, 0x8c59},
	{0x3e06, 0x8059},
	{0x3e08, 0x8a5f},
	{0x3e0a, 0xec7c},
	{0x3e0c, 0xcc84},
	{0x3e0e, 0x6182},
	{0x3e10, 0x6283},
	{0x3e12, 0x4283},
	{0x3e14, 0x10cc},
	{0x3e16, 0x6496},
	{0x3e18, 0x4281},
	{0x3e1a, 0x41bb},
	{0x3e1c, 0x4082},
	{0x3e1e, 0x407e},
	{0x3e20, 0xcc41},
	{0x3e22, 0x8042},
	{0x3e24, 0x8043},
	{0x3e26, 0x8300},
	{0x3e28, 0xc088},
	{0x3e2a, 0x44ba},
	{0x3e2c, 0x4488},
	{0x3e2e, 0x00c8},
	{0x3e30, 0x8042},
	{0x3e32, 0x4181},
	{0x3e34, 0x4082},
	{0x3e36, 0x4080},
	{0x3e38, 0x4180},
	{0x3e3a, 0x4280},
	{0x3e3c, 0x4383},
	{0x3e3e, 0x00c0},
	{0x3e40, 0x8844},
	{0x3e42, 0xba44},
	{0x3e44, 0x8800},
	{0x3e46, 0xc880},
	{0x3e48, 0x4241},
	{0x3e4a, 0x8240},
	{0x3e4c, 0x8140},
	{0x3e4e, 0x8041},
	{0x3e50, 0x8042},
	{0x3e52, 0x8043},
	{0x3e54, 0x8300},
	{0x3e56, 0xc088},
	{0x3e58, 0x44ba},
	{0x3e5a, 0x4488},
	{0x3e5c, 0x00c8},
	{0x3e5e, 0x8042},
	{0x3e60, 0x4181},
	{0x3e62, 0x4082},
	{0x3e64, 0x4080},
	{0x3e66, 0x4180},
	{0x3e68, 0x4280},
	{0x3e6a, 0x4383},
	{0x3e6c, 0x00c0},
	{0x3e6e, 0x8844},
	{0x3e70, 0xba44},
	{0x3e72, 0x8800},
	{0x3e74, 0xc880},
	{0x3e76, 0x4241},
	{0x3e78, 0x8140},
	{0x3e7a, 0x9f5e},
	{0x3e7c, 0x8a54},
	{0x3e7e, 0x8620},
	{0x3e80, 0x2881},
	{0x3e82, 0x6026},
	{0x3e84, 0x8055},
	{0x3e86, 0x8070},
	{0x3e88, 0x0000},
	{0x3e8a, 0x0000},
	{0x3e8c, 0x0000},
	{0x3e8e, 0x0000},
	{0x3e90, 0x0000},
	{0x3e92, 0x0000},
	{0x3e94, 0x0000},
	{0x3e96, 0x0000},
	{0x3e98, 0x0000},
	{0x3e9a, 0x0000},
	{0x3e9c, 0x0000},
	{0x3e9e, 0x0000},
	{0x3ea0, 0x0000},
	{0x3ea2, 0x0000},
	{0x3ea4, 0x0000},
	{0x3ea6, 0x0000},
	{0x3ea8, 0x0000},
	{0x3eaa, 0x0000},
	{0x3eac, 0x0000},
	{0x3eae, 0x0000},
	{0x3eb0, 0x0000},
	{0x3eb2, 0x0000},
	{0x3eb4, 0x0000},
	{0x3eb6, 0x004d}, // DAC_LD_0_1
	{0x3eba, 0x1dab}, // DAC_LD_4_5
	{0x3ebc, 0xaa06}, // DAC_LD_6_7
	{0x3ec0, 0x1300}, // DAC_LD_10_11
	{0x3ec2, 0x7000}, // DAC_LD_12_13
	{0x3ec4, 0x1c08}, // DAC_LD_14_15
	{0x3ec6, 0xe244}, // DAC_LD_16_17
	{0x3ec8, 0x0f0f}, // DAC_LD_18_19
	{0x3eca, 0x0f4a}, // DAC_LD_20_21
	{0x3ecc, 0x0706}, // DAC_LD_22_23
	{0x3ece, 0x443b}, // DAC_LD_24_25
	{0x3ed0, 0x12f0}, // DAC_LD_26_27
	{0x3ed2, 0x0039}, // DAC_LD_28_29
	{0x3ed4, 0x862f}, // DAC_LD_30_31
	{0x3ed6, 0x4880}, // DAC_LD_32_33
	{0x3ed8, 0x0423}, // DAC_LD_34_35
	{0x3eda, 0xf882}, // DAC_LD_36_37
	{0x3edc, 0x8282}, // DAC_LD_38_39
	{0x3ede, 0x8205}, // DAC_LD_40_41
	{0x316a, 0x8200}, // DAC_RSTLO
	{0x316c, 0x8200}, // DAC_TXLO
	{0x316e, 0x8200}, // DAC_ECL
	{0x3ef0, 0x5165}, // DAC_LD_ECL
	{0x3ef2, 0x0101}, // DAC_LD_FSC
	{0x3ef6, 0x030a}, // DAC_LD_RSTD
	{0x3efa, 0x0f0f}, // DAC_LD_TXLO
	{0x3efc, 0x070f}, // DAC_LD_TXLO1
	{0x3efe, 0x0f0f}, // DAC_LD_TXLO2
	{0x31b0, 0x0060}, // FRAME_PREAMBLE
	{0x31b2, 0x002e}, // LINE_PREAMBLE
	{0x31b4, 0x33d4}, // MIPI_TIMING_0
	{0x31b6, 0x244b}, // MIPI_TIMING_1
	{0x31b8, 0x2413}, // MIPI_TIMING_2
	{0x31ba, 0x2070}, // MIPI_TIMING_3
	{0x31bc, 0x870b}, // MIPI_TIMING_4
	{0x0300, 0x0005}, // VT_PIX_CLK_DIV
	{0x0302, 0x0001}, // VT_SYS_CLK_DIV
	{0x0304, 0x0101}, // PRE_PLL_CLK_DIV
	{0x0306, 0x2e2e}, // PLL_MULTIPLIER
	{0x0308, 0x000a}, // OP_PIX_CLK_DIV
	{0x030a, 0x0001}, // OP_SYS_CLK_DIV
	{0x0112, 0x0a0a}, // CCP_DATA_FORMAT
	{0x3016, 0x0101}, // ROW_SPEED
	{0x31ae, 0x0202}, //
	{0x0344, 0x00c0}, // X_ADDR_START
	{0x0348, 0x0fbf}, // X_ADDR_END {0x0346, 0x01e8}, // Y_ADDR_START
	{0x034a, 0x0a55}, // Y_ADDR_END {0x034c, 0x0780}, // X_OUTPUT_SIZE
	{0x034e, 0x0438}, // Y_OUTPUT_SIZE
	{0x3040, 0x0043}, // READ_MODE
	{0x3172, 0x0206}, // ANALOG_CONTROL2
	{0x317a, 0x516e}, // ANALOG_CONTROL6
	{0x3f3c, 0x0003}, // ANALOG_CONTROL9
	{0x0400, 0x01},	  // Scaling Enabling: 0= disable, = x-dir
	{0x0404, 0x20},	  // Scale_M = 32
	{0x32c8, 0x030c}, // PDAF_SEQ_START
	{0x32ca, 0x08a6}, // PDAF_ODP_LLENGTH
	{0x0342, 0x22f4}, // LINE_LENGTH_PCK
	{0x0340, 0x066d}, // FRAME_LENGTH_LINES
	{0x0202, 0x066c}, // COARSE_INTEGRATION_TIME
	{0x30ec, 0xfb08}, // CTX_RD_DATA
	{0x31d6, 0x336b}, // MIPI_JPEG_PN9_DATA_TYPE
	{0x32c2, 0x03fc}, // pdaf_dma_start=PDAF_ZONE_PER_LINE*(PDAF_NUMBER_OF_CC + 1)*4 = 1020 = 0x03fc
	{0x32c4, 0x0f30}, // PDAF_DMA_SIZE
	{0x32c6, 0x0a00}, // PDAF_DMA_Y
	{0x32c8, 0x0342}, // PDAF_SEQ_START
	{0x32d0, 0x0001}, // PE_PARAM_ADDR
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x034e, 0x0442}, // Y_OUTPUT_SIZE
	{0x32d0, 0x6000}, // PE_PARAM_ADDR
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x00b0}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2000}, // PE_PARAM_VALUE
	{0x32d4, 0x189e}, // PE_PARAM_VALUE
	{0x32d4, 0xa948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x189f}, // PE_PARAM_VALUE
	{0x32d4, 0x0a48}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0e}, // PE_PARAM_VALUE
	{0x32d4, 0x188a}, // PE_PARAM_VALUE
	{0x32d4, 0x0a48}, // PE_PARAM_VALUE
	{0x32d4, 0x0806}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2828}, // PE_PARAM_VALUE
	{0x32d4, 0xffff}, // PE_PARAM_VALUE
	{0x32d4, 0x7fff}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x009e}, // PE_PARAM_VALUE
	{0x32d4, 0xa948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x189e}, // PE_PARAM_VALUE
	{0x32d4, 0xa948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x189f}, // PE_PARAM_VALUE
	{0x32d4, 0x0a48}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0e}, // PE_PARAM_VALUE
	{0x32d4, 0x188a}, // PE_PARAM_VALUE
	{0x32d4, 0x0a48}, // PE_PARAM_VALUE
	{0x32d4, 0x0806}, // PE_PARAM_VALUE
	{0x32d4, 0x188a}, // PE_PARAM_VALUE
	{0x32d4, 0x0a48}, // PE_PARAM_VALUE
	{0x32d4, 0x0806}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3800}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d18}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d98}, // PE_PARAM_VALUE
	{0x32d4, 0x401e}, // PE_PARAM_VALUE
	{0x32d4, 0x0002}, // PE_PARAM_VALUE
	{0x32d4, 0x2818}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x11b1}, // PE_PARAM_VALUE
	{0x32d4, 0x001f}, // PE_PARAM_VALUE
	{0x32d4, 0x16ac}, // PE_PARAM_VALUE
	{0x32d4, 0x1010}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb6ac}, // PE_PARAM_VALUE
	{0x32d4, 0x11b1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x0b44}, // PE_PARAM_VALUE
	{0x32d4, 0x000c}, // PE_PARAM_VALUE
	{0x32d4, 0x000c}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b4}, // PE_PARAM_VALUE
	{0x32d4, 0x010d}, // PE_PARAM_VALUE
	{0x32d4, 0x020c}, // PE_PARAM_VALUE
	{0x32d4, 0x09b4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a4}, // PE_PARAM_VALUE
	{0x32d4, 0x000c}, // PE_PARAM_VALUE
	{0x32d4, 0xb224}, // PE_PARAM_VALUE
	{0x32d4, 0x034d}, // PE_PARAM_VALUE
	{0x32d4, 0x021c}, // PE_PARAM_VALUE
	{0x32d4, 0xba24}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0d}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x09bc}, // PE_PARAM_VALUE
	{0x32d4, 0x0150}, // PE_PARAM_VALUE
	{0x32d4, 0x081a}, // PE_PARAM_VALUE
	{0x32d4, 0x09b4}, // PE_PARAM_VALUE
	{0x32d4, 0x01ae}, // PE_PARAM_VALUE
	{0x32d4, 0x001a}, // PE_PARAM_VALUE
	{0x32d4, 0x16b4}, // PE_PARAM_VALUE
	{0x32d4, 0x01b2}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d08}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3800}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2828}, // PE_PARAM_VALUE
	{0x32d4, 0x019e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x11b1}, // PE_PARAM_VALUE
	{0x32d4, 0x001f}, // PE_PARAM_VALUE
	{0x32d4, 0x0082}, // PE_PARAM_VALUE
	{0x32d4, 0x1010}, // PE_PARAM_VALUE
	{0x32d4, 0x018a}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0xa082}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3800}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2238}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2438}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2638}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2820}, // PE_PARAM_VALUE
	{0x32d4, 0x00be}, // PE_PARAM_VALUE
	{0x32d4, 0x0924}, // PE_PARAM_VALUE
	{0x32d4, 0x1010}, // PE_PARAM_VALUE
	{0x32d4, 0x003f}, // PE_PARAM_VALUE
	{0x32d4, 0x00a4}, // PE_PARAM_VALUE
	{0x32d4, 0x1010}, // PE_PARAM_VALUE
	{0x32d4, 0x00ae}, // PE_PARAM_VALUE
	{0x32d4, 0xa130}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x002c}, // PE_PARAM_VALUE
	{0x32d4, 0xa0b0}, // PE_PARAM_VALUE
	{0x32d4, 0x01af}, // PE_PARAM_VALUE
	{0x32d4, 0x002e}, // PE_PARAM_VALUE
	{0x32d4, 0xa130}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0xc000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2830}, // PE_PARAM_VALUE
	{0x32d4, 0x020d}, // PE_PARAM_VALUE
	{0x32d4, 0xa1c0}, // PE_PARAM_VALUE
	{0x32d4, 0x01af}, // PE_PARAM_VALUE
	{0x32d4, 0x032e}, // PE_PARAM_VALUE
	{0x32d4, 0xa1b0}, // PE_PARAM_VALUE
	{0x32d4, 0x120f}, // PE_PARAM_VALUE
	{0x32d4, 0x00ae}, // PE_PARAM_VALUE
	{0x32d4, 0xa6b0}, // PE_PARAM_VALUE
	{0x32d4, 0x120f}, // PE_PARAM_VALUE
	{0x32d4, 0x003c}, // PE_PARAM_VALUE
	{0x32d4, 0x00b0}, // PE_PARAM_VALUE
	{0x32d4, 0x041a}, // PE_PARAM_VALUE
	{0x32d4, 0x006e}, // PE_PARAM_VALUE
	{0x32d4, 0xa1b0}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3800}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d98}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d18}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2828}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb36a}, // PE_PARAM_VALUE
	{0x32d4, 0x03a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x0f6a}, // PE_PARAM_VALUE
	{0x32d4, 0x0200}, // PE_PARAM_VALUE
	{0x32d4, 0x0c1e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x11b1}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0010}, // PE_PARAM_VALUE
	{0x32d4, 0x2230}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0010}, // PE_PARAM_VALUE
	{0x32d4, 0x2438}, // PE_PARAM_VALUE
	{0x32d4, 0x019e}, // PE_PARAM_VALUE
	{0x32d4, 0x0924}, // PE_PARAM_VALUE
	{0x32d4, 0x1010}, // PE_PARAM_VALUE
	{0x32d4, 0x819e}, // PE_PARAM_VALUE
	{0x32d4, 0xa495}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x819f}, // PE_PARAM_VALUE
	{0x32d4, 0xa495}, // PE_PARAM_VALUE
	{0x32d4, 0x1c1d}, // PE_PARAM_VALUE
	{0x32d4, 0x819e}, // PE_PARAM_VALUE
	{0x32d4, 0x0495}, // PE_PARAM_VALUE
	{0x32d4, 0x1c1c}, // PE_PARAM_VALUE
	{0x32d4, 0x819e}, // PE_PARAM_VALUE
	{0x32d4, 0x0495}, // PE_PARAM_VALUE
	{0x32d4, 0x1c1c}, // PE_PARAM_VALUE
	{0x32d4, 0x81de}, // PE_PARAM_VALUE
	{0x32d4, 0x0495}, // PE_PARAM_VALUE
	{0x32d4, 0x1dbc}, // PE_PARAM_VALUE
	{0x32d4, 0x81fe}, // PE_PARAM_VALUE
	{0x32d4, 0xa495}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xbb34}, // PE_PARAM_VALUE
	{0x32d4, 0x03af}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xbb54}, // PE_PARAM_VALUE
	{0x32d4, 0x020f}, // PE_PARAM_VALUE
	{0x32d4, 0xfff0}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3040}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d00}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3800}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4007}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2818}, // PE_PARAM_VALUE
	{0x32d4, 0x003e}, // PE_PARAM_VALUE
	{0x32d4, 0xad8a}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x003f}, // PE_PARAM_VALUE
	{0x32d4, 0xad8a}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x002e}, // PE_PARAM_VALUE
	{0x32d4, 0xad8a}, // PE_PARAM_VALUE
	{0x32d4, 0x160f}, // PE_PARAM_VALUE
	{0x32d4, 0x002e}, // PE_PARAM_VALUE
	{0x32d4, 0xad8a}, // PE_PARAM_VALUE
	{0x32d4, 0x160f}, // PE_PARAM_VALUE
	{0x32d4, 0x203e}, // PE_PARAM_VALUE
	{0x32d4, 0xad8a}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x2820}, // PE_PARAM_VALUE
	{0x32d4, 0x00ae}, // PE_PARAM_VALUE
	{0x32d4, 0xae0a}, // PE_PARAM_VALUE
	{0x32d4, 0x1811}, // PE_PARAM_VALUE
	{0x32d4, 0x002f}, // PE_PARAM_VALUE
	{0x32d4, 0xae1a}, // PE_PARAM_VALUE
	{0x32d4, 0x1411}, // PE_PARAM_VALUE
	{0x32d4, 0x003c}, // PE_PARAM_VALUE
	{0x32d4, 0x0f0a}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0x202a}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0a}, // PE_PARAM_VALUE
	{0x32d4, 0x1818}, // PE_PARAM_VALUE
	{0x32d4, 0x002a}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0c}, // PE_PARAM_VALUE
	{0x32d4, 0x1402}, // PE_PARAM_VALUE
	{0x32d4, 0x002a}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0c}, // PE_PARAM_VALUE
	{0x32d4, 0x15be}, // PE_PARAM_VALUE
	{0x32d4, 0x00aa}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0c}, // PE_PARAM_VALUE
	{0x32d4, 0x15be}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xae1a}, // PE_PARAM_VALUE
	{0x32d4, 0x11b1}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3800}, // PE_PARAM_VALUE
	{0x32d4, 0x003e}, // PE_PARAM_VALUE
	{0x32d4, 0xa93c}, // PE_PARAM_VALUE
	{0x32d4, 0x11b1}, // PE_PARAM_VALUE
	{0x32d4, 0x7052}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaba4}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x1a0f}, // PE_PARAM_VALUE
	{0x32d4, 0xffbe}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xffa7}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa120}, // PE_PARAM_VALUE
	{0x32d4, 0x01af}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x02a0}, // PE_PARAM_VALUE
	{0x32d4, 0x01a2}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa2a0}, // PE_PARAM_VALUE
	{0x32d4, 0x00e3}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x011e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xff9c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa120}, // PE_PARAM_VALUE
	{0x32d4, 0x01af}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x02a0}, // PE_PARAM_VALUE
	{0x32d4, 0x01a2}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa2a0}, // PE_PARAM_VALUE
	{0x32d4, 0x00e3}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x011e}, // PE_PARAM_VALUE
	{0x32d4, 0xa920}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa922}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x0924}, // PE_PARAM_VALUE
	{0x32d4, 0x0fba}, // PE_PARAM_VALUE
	{0x32d4, 0x0006}, // PE_PARAM_VALUE
	{0x32d4, 0xa024}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x0200}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa124}, // PE_PARAM_VALUE
	{0x32d4, 0x0baf}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa124}, // PE_PARAM_VALUE
	{0x32d4, 0x0a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x805e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x011e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xff88}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x704d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x011e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xff83}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x3052}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x009e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2dd0}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3800}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x8000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2820}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x019e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x04c8}, // PE_PARAM_VALUE
	{0x32d4, 0x1010}, // PE_PARAM_VALUE
	{0x32d4, 0x019e}, // PE_PARAM_VALUE
	{0x32d4, 0xa948}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x801f}, // PE_PARAM_VALUE
	{0x32d4, 0xa4c8}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0018}, // PE_PARAM_VALUE
	{0x32d4, 0x0946}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0x818a}, // PE_PARAM_VALUE
	{0x32d4, 0x0936}, // PE_PARAM_VALUE
	{0x32d4, 0x0200}, // PE_PARAM_VALUE
	{0x32d4, 0x4008}, // PE_PARAM_VALUE
	{0x32d4, 0x04c8}, // PE_PARAM_VALUE
	{0x32d4, 0x0fba}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3800}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0002}, // PE_PARAM_VALUE
	{0x32d4, 0x2820}, // PE_PARAM_VALUE
	{0x32d4, 0x0003}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x03fc}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x011e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0x0925}, // PE_PARAM_VALUE
	{0x32d4, 0x1010}, // PE_PARAM_VALUE
	{0x32d4, 0x0011}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x011e}, // PE_PARAM_VALUE
	{0x32d4, 0x0924}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x8006}, // PE_PARAM_VALUE
	{0x32d4, 0x0db7}, // PE_PARAM_VALUE
	{0x32d4, 0x0014}, // PE_PARAM_VALUE
	{0x32d4, 0x0008}, // PE_PARAM_VALUE
	{0x32d4, 0x0dc8}, // PE_PARAM_VALUE
	{0x32d4, 0x0e12}, // PE_PARAM_VALUE
	{0x32d4, 0x0040}, // PE_PARAM_VALUE
	{0x32d4, 0x0dc8}, // PE_PARAM_VALUE
	{0x32d4, 0x0fb2}, // PE_PARAM_VALUE
	{0x32d4, 0x0010}, // PE_PARAM_VALUE
	{0x32d4, 0x1248}, // PE_PARAM_VALUE
	{0x32d4, 0x0e14}, // PE_PARAM_VALUE
	{0x32d4, 0x0050}, // PE_PARAM_VALUE
	{0x32d4, 0x1248}, // PE_PARAM_VALUE
	{0x32d4, 0x0db4}, // PE_PARAM_VALUE
	{0x32d4, 0x0010}, // PE_PARAM_VALUE
	{0x32d4, 0x1248}, // PE_PARAM_VALUE
	{0x32d4, 0x0c14}, // PE_PARAM_VALUE
	{0x32d4, 0x0050}, // PE_PARAM_VALUE
	{0x32d4, 0x1248}, // PE_PARAM_VALUE
	{0x32d4, 0x0db4}, // PE_PARAM_VALUE
	{0x32d4, 0x0016}, // PE_PARAM_VALUE
	{0x32d4, 0xb248}, // PE_PARAM_VALUE
	{0x32d4, 0x0c0f}, // PE_PARAM_VALUE
	{0x32d4, 0x0056}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0daf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3800}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x2000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x30f4}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x7000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaaba}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaea4}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa24}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa24}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa24}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x7001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7009}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2428}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2228}, // PE_PARAM_VALUE
	{0x32d4, 0xffca}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2620}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xadb8}, // PE_PARAM_VALUE
	{0x32d4, 0x0fa1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x0e01}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x700b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7009}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x0a38}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x700a}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700a}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x7002}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x004e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x0201}, // PE_PARAM_VALUE
	{0x32d4, 0xffcf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3040}, // PE_PARAM_VALUE
	{0x32d4, 0x7800}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xfffe}, // PE_PARAM_VALUE
	{0x32d4, 0xffff}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x700c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a4}, // PE_PARAM_VALUE
	{0x32d4, 0x0daf}, // PE_PARAM_VALUE
	{0x32d4, 0x001c}, // PE_PARAM_VALUE
	{0x32d4, 0xada4}, // PE_PARAM_VALUE
	{0x32d4, 0x0c0f}, // PE_PARAM_VALUE
	{0x32d4, 0x085e}, // PE_PARAM_VALUE
	{0x32d4, 0xada4}, // PE_PARAM_VALUE
	{0x32d4, 0x0daf}, // PE_PARAM_VALUE
	{0x32d4, 0x0003}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x0040}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0xff30}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x0010}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0xff2e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x0010}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0xff2c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x2010}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0xff2a}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x2010}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x2000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x7006}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaaba}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaea4}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa24}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa24}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x700d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa93a}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa93a}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa93a}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa93a}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x3019}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x3026}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x7017}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0fa1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xada4}, // PE_PARAM_VALUE
	{0x32d4, 0x0e01}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xada4}, // PE_PARAM_VALUE
	{0x32d4, 0x0e01}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xada4}, // PE_PARAM_VALUE
	{0x32d4, 0x0fa1}, // PE_PARAM_VALUE
	{0x32d4, 0x7004}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7009}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2420}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xff3e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2220}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x704c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x7017}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7018}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2628}, // PE_PARAM_VALUE
	{0x32d4, 0x704d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa0b0}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x1a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2630}, // PE_PARAM_VALUE
	{0x32d4, 0x704e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa0b0}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x1a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x09a8}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x7017}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7009}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7004}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x0a38}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x700a}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700a}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x7005}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x004e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x0201}, // PE_PARAM_VALUE
	{0x32d4, 0xffb0}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3040}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7005}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b6}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b6}, // PE_PARAM_VALUE
	{0x32d4, 0x020f}, // PE_PARAM_VALUE
	{0x32d4, 0x7051}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x3019}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x3026}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x3033}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0x303f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xadb6}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb236}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xb6b6}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa936}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa936}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa936}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x0800}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0x704f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2228}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2420}, // PE_PARAM_VALUE
	{0x32d4, 0x009e}, // PE_PARAM_VALUE
	{0x32d4, 0xb248}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x009e}, // PE_PARAM_VALUE
	{0x32d4, 0x10c8}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0e}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xb0c8}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x010a}, // PE_PARAM_VALUE
	{0x32d4, 0xb242}, // PE_PARAM_VALUE
	{0x32d4, 0x040f}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0x12c2}, // PE_PARAM_VALUE
	{0x32d4, 0x05a0}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0x12ca}, // PE_PARAM_VALUE
	{0x32d4, 0x05a4}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0x12ca}, // PE_PARAM_VALUE
	{0x32d4, 0x0a0a}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0xb2ca}, // PE_PARAM_VALUE
	{0x32d4, 0x17b7}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0x12ca}, // PE_PARAM_VALUE
	{0x32d4, 0x1616}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0x12ca}, // PE_PARAM_VALUE
	{0x32d4, 0x1616}, // PE_PARAM_VALUE
	{0x32d4, 0x000a}, // PE_PARAM_VALUE
	{0x32d4, 0x124a}, // PE_PARAM_VALUE
	{0x32d4, 0x1616}, // PE_PARAM_VALUE
	{0x32d4, 0x006a}, // PE_PARAM_VALUE
	{0x32d4, 0xb24a}, // PE_PARAM_VALUE
	{0x32d4, 0x0017}, // PE_PARAM_VALUE
	{0x32d4, 0x004a}, // PE_PARAM_VALUE
	{0x32d4, 0xb24a}, // PE_PARAM_VALUE
	{0x32d4, 0x04b7}, // PE_PARAM_VALUE
	{0x32d4, 0x802a}, // PE_PARAM_VALUE
	{0x32d4, 0xb24a}, // PE_PARAM_VALUE
	{0x32d4, 0x05b7}, // PE_PARAM_VALUE
	{0x32d4, 0x7004}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xfe77}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x7050}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2638}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9c8}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x03a2}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x89a8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a0}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x01af}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x704f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa0b0}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x1a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x002a}, // PE_PARAM_VALUE
	{0x32d4, 0xb24a}, // PE_PARAM_VALUE
	{0x32d4, 0x01b7}, // PE_PARAM_VALUE
	{0x32d4, 0x002a}, // PE_PARAM_VALUE
	{0x32d4, 0xb24a}, // PE_PARAM_VALUE
	{0x32d4, 0x0017}, // PE_PARAM_VALUE
	{0x32d4, 0x002a}, // PE_PARAM_VALUE
	{0x32d4, 0xb24a}, // PE_PARAM_VALUE
	{0x32d4, 0x04b7}, // PE_PARAM_VALUE
	{0x32d4, 0x802a}, // PE_PARAM_VALUE
	{0x32d4, 0xb24a}, // PE_PARAM_VALUE
	{0x32d4, 0x05b7}, // PE_PARAM_VALUE
	{0x32d4, 0x7004}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xfe5c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x704f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2638}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9c8}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x03a2}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x89a8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a0}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x01af}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x704d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x011e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x811e}, // PE_PARAM_VALUE
	{0x32d4, 0xb949}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x609e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x189e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d18}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xba28}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xbaa8}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xbaa8}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xbaa8}, // PE_PARAM_VALUE
	{0x32d4, 0x01af}, // PE_PARAM_VALUE
	{0x32d4, 0x7051}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7051}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x03a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x0201}, // PE_PARAM_VALUE
	{0x32d4, 0xffa3}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3040}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x03a1}, // PE_PARAM_VALUE
	{0x32d4, 0x700d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7005}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xfe2f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb224}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb224}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xb224}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xfe26}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb224}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb224}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xb224}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2b8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2b8}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2b8}, // PE_PARAM_VALUE
	{0x32d4, 0x1201}, // PE_PARAM_VALUE
	{0x32d4, 0x700f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d40}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x7005}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7004}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7006}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x0a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x12a4}, // PE_PARAM_VALUE
	{0x32d4, 0x0a0e}, // PE_PARAM_VALUE
	{0x32d4, 0x0118}, // PE_PARAM_VALUE
	{0x32d4, 0x12a4}, // PE_PARAM_VALUE
	{0x32d4, 0x0ba0}, // PE_PARAM_VALUE
	{0x32d4, 0x0018}, // PE_PARAM_VALUE
	{0x32d4, 0x0524}, // PE_PARAM_VALUE
	{0x32d4, 0x0bb2}, // PE_PARAM_VALUE
	{0x32d4, 0x4018}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x0013}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d80}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2400}, // PE_PARAM_VALUE
	{0x32d4, 0xfdfd}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x7011}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x045e}, // PE_PARAM_VALUE
	{0x32d4, 0xadb8}, // PE_PARAM_VALUE
	{0x32d4, 0x13a1}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xadb8}, // PE_PARAM_VALUE
	{0x32d4, 0x13a1}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d00}, // PE_PARAM_VALUE
	{0x32d4, 0x2000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xab24}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xab24}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0xfdf5}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2400}, // PE_PARAM_VALUE
	{0x32d4, 0x105e}, // PE_PARAM_VALUE
	{0x32d4, 0xadb8}, // PE_PARAM_VALUE
	{0x32d4, 0x13a1}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xadb8}, // PE_PARAM_VALUE
	{0x32d4, 0x13a1}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d80}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2ca}, // PE_PARAM_VALUE
	{0x32d4, 0x03a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xbb4a}, // PE_PARAM_VALUE
	{0x32d4, 0x0201}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xbb6c}, // PE_PARAM_VALUE
	{0x32d4, 0x021a}, // PE_PARAM_VALUE
	{0x32d4, 0x003a}, // PE_PARAM_VALUE
	{0x32d4, 0x12ec}, // PE_PARAM_VALUE
	{0x32d4, 0x03b6}, // PE_PARAM_VALUE
	{0x32d4, 0x1076}, // PE_PARAM_VALUE
	{0x32d4, 0xb26c}, // PE_PARAM_VALUE
	{0x32d4, 0x1a1a}, // PE_PARAM_VALUE
	{0x32d4, 0x003a}, // PE_PARAM_VALUE
	{0x32d4, 0x12ca}, // PE_PARAM_VALUE
	{0x32d4, 0x03b2}, // PE_PARAM_VALUE
	{0x32d4, 0x047e}, // PE_PARAM_VALUE
	{0x32d4, 0xb26c}, // PE_PARAM_VALUE
	{0x32d4, 0x03a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xbe5e}, // PE_PARAM_VALUE
	{0x32d4, 0x05a5}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x1e5e}, // PE_PARAM_VALUE
	{0x32d4, 0x0404}, // PE_PARAM_VALUE
	{0x32d4, 0x0002}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a4}, // PE_PARAM_VALUE
	{0x32d4, 0x060f}, // PE_PARAM_VALUE
	{0x32d4, 0xff00}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0xfdef}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x201e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9a4}, // PE_PARAM_VALUE
	{0x32d4, 0x05af}, // PE_PARAM_VALUE
	{0x32d4, 0x00ff}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x09af}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x080f}, // PE_PARAM_VALUE
	{0x32d4, 0x7010}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xb2a4}, // PE_PARAM_VALUE
	{0x32d4, 0x09af}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x3019}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x3026}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x3033}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2ac0}, // PE_PARAM_VALUE
	{0x32d4, 0x303f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xadb6}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb236}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xb6b6}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa936}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa936}, // PE_PARAM_VALUE
	{0x32d4, 0x0e0f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa936}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7005}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b6}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b6}, // PE_PARAM_VALUE
	{0x32d4, 0x020f}, // PE_PARAM_VALUE
	{0x32d4, 0x7051}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x2000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x3018}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x7003}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaaa4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaea4}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa24}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2428}, // PE_PARAM_VALUE
	{0x32d4, 0x009e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa8a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x201e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x704d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2430}, // PE_PARAM_VALUE
	{0x32d4, 0x009e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa8a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x081e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x7010}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa0b0}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x1a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d00}, // PE_PARAM_VALUE
	{0x32d4, 0x700f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7004}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa0b0}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x1a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2400}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2600}, // PE_PARAM_VALUE
	{0x32d4, 0xfde8}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x704c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2428}, // PE_PARAM_VALUE
	{0x32d4, 0x009e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa8a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x201e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x704e}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2430}, // PE_PARAM_VALUE
	{0x32d4, 0x009e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa8a4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x081e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x7010}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa0b0}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x1a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d00}, // PE_PARAM_VALUE
	{0x32d4, 0x700f}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7004}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7009}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa0b0}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x1a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2400}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2600}, // PE_PARAM_VALUE
	{0x32d4, 0xfdc4}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x704d}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x011e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x811e}, // PE_PARAM_VALUE
	{0x32d4, 0xb949}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x609e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x189e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x704b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2d18}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb948}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xb9a8}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xba28}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xbaa8}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xbaa8}, // PE_PARAM_VALUE
	{0x32d4, 0x000f}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xbaa8}, // PE_PARAM_VALUE
	{0x32d4, 0x01af}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7009}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7004}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x0a38}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x7051}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7051}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x03a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x0201}, // PE_PARAM_VALUE
	{0x32d4, 0xff92}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3040}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x03a1}, // PE_PARAM_VALUE
	{0x32d4, 0x2000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x3018}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x7000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x061e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaaa4}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaea4}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa24}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa24}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa24}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7009}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x0006}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a80}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x0a38}, // PE_PARAM_VALUE
	{0x32d4, 0x0002}, // PE_PARAM_VALUE
	{0x32d4, 0x0c1e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a3}, // PE_PARAM_VALUE
	{0x32d4, 0x7001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa0b0}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x1a0f}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa1a8}, // PE_PARAM_VALUE
	{0x32d4, 0x120f}, // PE_PARAM_VALUE
	{0x32d4, 0x200e}, // PE_PARAM_VALUE
	{0x32d4, 0xa1a8}, // PE_PARAM_VALUE
	{0x32d4, 0x03af}, // PE_PARAM_VALUE
	{0x32d4, 0x700b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0xfd66}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x4000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2620}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7009}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x601e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x7008}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0x0a38}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x007e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x700b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x0014}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x005e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x700a}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x010e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x801e}, // PE_PARAM_VALUE
	{0x32d4, 0xa925}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x700a}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a40}, // PE_PARAM_VALUE
	{0x32d4, 0x7002}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2418}, // PE_PARAM_VALUE
	{0x32d4, 0x008e}, // PE_PARAM_VALUE
	{0x32d4, 0xa000}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x181e}, // PE_PARAM_VALUE
	{0x32d4, 0xa924}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xaa38}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x004e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x0201}, // PE_PARAM_VALUE
	{0x32d4, 0xffc9}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3040}, // PE_PARAM_VALUE
	{0x32d4, 0x001e}, // PE_PARAM_VALUE
	{0x32d4, 0xa9b8}, // PE_PARAM_VALUE
	{0x32d4, 0x01a1}, // PE_PARAM_VALUE
	{0x32d4, 0x00e7}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2a00}, // PE_PARAM_VALUE
	{0x32d4, 0x43fc}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2618}, // PE_PARAM_VALUE
	{0x32d4, 0xfda3}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x3600}, // PE_PARAM_VALUE
	{0x32d4, 0x7018}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2218}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x2000}, // PE_PARAM_VALUE
	{0x32d4, 0x000e}, // PE_PARAM_VALUE
	{0x32d4, 0xa030}, // PE_PARAM_VALUE
	{0x32d4, 0x0faf}, // PE_PARAM_VALUE
	{0x32d0, 0x3000}, // PE_PARAM_ADDR
	{0x32d4, 0x005c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0066}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x000b}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0050}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0068}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x000c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x001c}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x32d4, 0x0001}, // PE_PARAM_VALUE
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x3220, 0x0c13}, // PDAF_CONTROL
	{0x30ec, 0xfb08}, // CTX_RD_DATA
	{0x31d6, 0x336b}, // MIPI_JPEG_PN9_DATA_TYPE
	{0x32c2, 0x03fc}, // PDAF_DMA_START
	{0x32c4, 0x0f30}, // PDAF_DMA_SIZE
	{0x32c6, 0x0f00}, // PDAF_DMA_Y
	{0x32c8, 0x0a00}, // PDAF_SEQ_START
	{0x32d0, 0x0001}, // PE_PARAM_ADDR
	{0x32d4, 0x0000}, // PE_PARAM_VALUE
	{0x034e, 0x0c3f}, // Y_OUTPUT_SIZE
	{0x32c4, 0x03c0}, // PDAF_DMA_SIZE
	{0x32ca, 0x08a6}, // PDAF_ODP_LLENGTH
	{0x32c8, 0x030c}, // PDAF_SEQ_START
	{0x301a, 0x021c}, // SENSOR_REGISTER
	{0x31d6, 0x332b}, // MIPI_JPEG_PN9_DATA_TYPE
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* [0] 1920*1080 @25fps */
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_mipi[] = {
	// {0x301a, 0x021c},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	// {0x301a, 0x0218},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
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
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, uint16_t value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[4] = {(reg >> 8) & 0xff, reg & 0xff, (value >> 8) & 0xff, value & 0xff};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 4,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
	int ret;
	unsigned char val;
	vals->value &= 0;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
			vals->value = vals->value | (val << 8);

			ret = sensor_read(sd, vals->reg_num + 1, &val);
			if (ret < 0)
				return ret;
			vals->value = vals->value | val;
		}
		vals++;
	}
	return 0;
}

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
#if 0
			if (vals->reg_num == 0x34c || vals->reg_num == 0x34e) {
				ISP_INFO(" write reg_num = 0x%x \nv[0] = 0x%x ====== v[1] = 0x%x \n", vals->reg_num, v[0], v[1]);
				sensor_read(sd, vals->reg_num, &z[0]);
				sensor_read(sd, vals->reg_num+1, &z[1]);
				ISP_INFO(" read reg_num = 0x%x  \nz[0] = 0x%x ====== z[1] = 0x%x \n",vals->reg_num, z[0], z[1]);
			}
#endif
		}
		vals++;
	}
#if 0
	sensor_read(sd, 0x31ae, &z[0]);
	sensor_read(sd, 0x31af, &z[1]);
	ISP_INFO(" read reg_num = 0x31AE  \nz[0] = 0x%x ====== z[1] = 0x%x \n", z[0], z[1]);
#endif
	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, int val) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	int ret;
	char v[2] = {0};
#if 0
	struct regval_list id_array = {
		.reg_num = 0x3000,
		.value = 0
	};

	ret = sensor_read_array(sd, &id_array);
	if (ret < 0)
		return ret;
	v[0] = (id_array.value >> 8) && 0xff;
	v[1] = id_array.value && 0xff;

	if (v[0] != SENSOR_CHIP_ID_H)
		return -ENODEV;
	if (v[1] != SENSOR_CHIP_ID_L)
		return -ENODEV;
	ISP_INFO("v[0] = 0x%x ---------- v[1] = 0x%x\n",v[0], v[1]);
#else
	ret = sensor_read(sd, 0x3000, &v[0]);
	ISP_INFO("ret = %d &&&&&& v[0] = %d\n", ret, v[0]);
	ret = sensor_read(sd, 0x3001, &v[1]);
	ISP_INFO("ret = %d &&&&&& v[0] = %d\n", ret, v[1]);
	if (ret < 0)
		return ret;

	if (v[0] != SENSOR_CHIP_ID_H)
		return -ENODEV;

	if (v[1] != SENSOR_CHIP_ID_L)
		return -ENODEV;
	ISP_INFO("v[0] = %d --------- v[1] = %d\n", v[0], v[1]);
#endif
	return 0;
}

#if 0
static int sensor_set_expo(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	int it = (value & 0xffff);
	int index = (value & 0xffff0000) >> 16;
	struct sensor_gain_lut *gain_lut = sensor_gain_lut;

	/*set integration time*/
	ret += sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	/*set analog gain*/
	ret += sensor_write(sd, 0x3e09, gain_lut[index].again);
	/*set coarse dgain*/
	ret += sensor_write(sd, 0x3e06, gain_lut[index].coarse_dgain);
	/*set fine dgain*/
	ret += sensor_write(sd, 0x3e07, gain_lut[index].fine_dgain);
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	ret = sensor_write(sd, 0x0202, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret;
	ret = sensor_write(sd, 0x305e, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_logic(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, int enable) {
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
	ret = sensor_write_array(sd, wsize->regs);
	if (ret)
		return ret;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_INFO("%s stream on\n", SENSOR_NAME);
	} else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_INFO("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	return 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp[2];
	unsigned int pclk = SENSOR_SUPPORT_25FPS_SCLK;
	unsigned int newformat = 0; // the format is 24.8
	int ret = 0;
	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
		return -1;
	ret += sensor_read(sd, 0x300c, tmp);
	hts = tmp[0];
	if (ret < 0)
		return -1;
	hts = (hts << 8) + tmp[1];
	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x300a, vts);

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 5;
	sensor->video.attr->integration_time_limit = vts - 5;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 5;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;
	if (wsize) {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
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
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			client->addr,
			client->adapter->name,
			SENSOR_NAME);
		return ret;
	}

	ISP_INFO("%s chip found @ 0x%02x (%s)\n", SENSOR_NAME, client->addr, client->adapter->name);
	ISP_INFO("sensor driver version %s\n", SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		/*
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = sensor_set_expo(sd, *(int*)arg);
			break;
*/
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		/*
			if (arg)
				ret = sensor_set_vflip(sd, *(int*)arg);
			break;
*/
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if (arg)
			ret = sensor_set_logic(sd, *(int *)arg);
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
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);
	sensor_attr.expo_fs = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);
	ISP_INFO("probe ok ------->%s\n", SENSOR_NAME);
	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);
	return -1;
}

static int sensor_remove(struct i2c_client *client) {
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
	int ret = 0;
	sensor_common_init(&sensor_info);

	ret = private_driver_get_interface();
	if (ret) {
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
	sensor_common_exit();
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for " SENSOR_NAME " sensor");
MODULE_LICENSE("GPL");
