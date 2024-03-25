// SPDX-License-Identifier: GPL-2.0+
/*
 * imx415.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          3840*2160       30        mipi_4lane           linear
 *   1          3840*2160       15        mipi_4lane           linear
 *   2          1920*1080       60        mipi_4lane           linear
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
#include <txx-funcs.h>

#define SENSOR_NAME "imx415"
#define SENSOR_CHIP_ID_H (0x28)
#define SENSOR_CHIP_ID_L (0x23)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK_8M (72002970)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define MCLK 24000000
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION "H20230725a"

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int reset_gpio = -1;
static int pwdn_gpio = -1;
char* __attribute__((weak)) sclk_name[4];

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 720,
	.lans = 4,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 3865,
	.image_theight = 2191,
	.mipi_sc.mipi_crop_start0x = 12,
	.mipi_sc.mipi_crop_start0y = 11,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.data_type_en = 1,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sensor_1080P_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1485,
	.lans = 4,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 1944,
	.image_theight = 1099,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 19,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 1,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x2823,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 404346,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 2251,
	.integration_time_limit = 2251,
	.total_width = 1066,
	.total_height = 2251,
	.max_integration_time = 2251,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_3840_2160_30fps_mipi[] = {
	{0x3008, 0x54},	// BCWAIT_TIME[9:0]
	{0x300A, 0x3B},	// CPWAIT_TIME[9:0]
	{0x3024, 0xCB},	// VMAX[19:0]
	{0x3028, 0x2A},	// HMAX[15:0]
	{0x3029, 0x04},	//
	{0x3031, 0x00},	// ADBIT[1:0]
	{0x3032, 0x00},	// MDBIT
	{0x3033, 0x09},	// SYS_MODE[3:0]
	{0x3050, 0xF4},	// SHR0[19:0]
	{0x3051, 0x0E},	//
	{0x3052, 0x00},
	{0x3090, 0x14},	// GAIN_PCG_0[8:0]
	{0x30C1, 0x00},	// XVS_DRV[1:0]
	{0x3116, 0x23},	// INCKSEL2[7:0]
	{0x3118, 0xB4},	// INCKSEL3[10:0]
	{0x311A, 0xFC},	// INCKSEL4[10:0]
	{0x311E, 0x23},	// INCKSEL5[7:0]
	{0x32D4, 0x21},	// -
	{0x32EC, 0xA1},	// -
	{0x3452, 0x7F},	// -
	{0x3453, 0x03},	// -
	{0x358A, 0x04},	// -
	{0x35A1, 0x02},	// -
	{0x36BC, 0x0C},	// -
	{0x36CC, 0x53},	// -
	{0x36CD, 0x00},	// -
	{0x36CE, 0x3C},	// -
	{0x36D0, 0x8C},	// -
	{0x36D1, 0x00},	// -
	{0x36D2, 0x71},	// -
	{0x36D4, 0x3C},	// -
	{0x36D6, 0x53},	// -
	{0x36D7, 0x00},	// -
	{0x36D8, 0x71},	// -
	{0x36DA, 0x8C},	// -
	{0x36DB, 0x00},	// -
	{0x3701, 0x00},	// ADBIT1[7:0]
	{0x3724, 0x02},	// -
	{0x3726, 0x02},	// -
	{0x3732, 0x02},	// -
	{0x3734, 0x03},	// -
	{0x3736, 0x03},	// -
	{0x3742, 0x03},	// -
	{0x3862, 0xE0},	// -
	{0x38CC, 0x30},	// -
	{0x38CD, 0x2F},	// -
	{0x395C, 0x0C},	// -
	{0x3A42, 0xD1},	// -
	{0x3A4C, 0x77},	// -
	{0x3AE0, 0x02},	// -
	{0x3AEC, 0x0C},	// -
	{0x3B00, 0x2E},	// -
	{0x3B06, 0x29},	// -
	{0x3B98, 0x25},	// -
	{0x3B99, 0x21},	// -
	{0x3B9B, 0x13},	// -
	{0x3B9C, 0x13},	// -
	{0x3B9D, 0x13},	// -
	{0x3B9E, 0x13},	// -
	{0x3BA1, 0x00},	// -
	{0x3BA2, 0x06},	// -
	{0x3BA3, 0x0B},	// -
	{0x3BA4, 0x10},	// -
	{0x3BA5, 0x14},	// -
	{0x3BA6, 0x18},	// -
	{0x3BA7, 0x1A},	// -
	{0x3BA8, 0x1A},	// -
	{0x3BA9, 0x1A},	// -
	{0x3BAC, 0xED},	// -
	{0x3BAD, 0x01},	// -
	{0x3BAE, 0xF6},	// -
	{0x3BAF, 0x02},	// -
	{0x3BB0, 0xA2},	// -
	{0x3BB1, 0x03},	// -
	{0x3BB2, 0xE0},	// -
	{0x3BB3, 0x03},	// -
	{0x3BB4, 0xE0},	// -
	{0x3BB5, 0x03},	// -
	{0x3BB6, 0xE0},	// -
	{0x3BB7, 0x03},	// -
	{0x3BB8, 0xE0},	// -
	{0x3BBA, 0xE0},	// -
	{0x3BBC, 0xDA},	// -
	{0x3BBE, 0x88},	// -
	{0x3BC0, 0x44},	// -
	{0x3BC2, 0x7B},	// -
	{0x3BC4, 0xA2},	// -
	{0x3BC8, 0xBD},	// -
	{0x3BCA, 0xBD},	// -
	{0x4004, 0x00},	// TXCLKESC_FREQ[15:0]
	{0x4005, 0x06},	//
	{0x400C, 0x00},	// INCKSEL6
	{0x4018, 0x6F},	// TCLKPOST[15:0]
	{0x401A, 0x2F},	// TCLKPREPARE[15:0]
	{0x401C, 0x2F},	// TCLKTRAIL[15:0]
	{0x401E, 0xBF},	// TCLKZERO[15:0]
	{0x401F, 0x00},	//
	{0x4020, 0x2F},	// THSPREPARE[15:0]
	{0x4022, 0x57},	// THSZERO[15:0]
	{0x4024, 0x2F},	// THSTRAIL[15:0]
	{0x4026, 0x4F},	// THSEXIT[15:0]
	{0x4028, 0x27},	// TLPX[15:0]
	{0x4074, 0x01},	// INCKSEL7 [2:0]
	{0x3000, 0x00},	// STANDBY
	{0x3002, 0x00},	// XMASTER

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_3840_2160_15fps_mipi[] = {
    {0x3008, 0x54}, // BCWAIT_TIME[9:0]
    {0x300A, 0x3B}, // CPWAIT_TIME[9:0]
    {0x3024, 0x97}, // VMAX[19:0]
    {0x3025, 0x11}, //
    {0x3028, 0x2A}, // HMAX[15:0]
    {0x3029, 0x04}, //
    {0x3031, 0x00}, // ADBIT[1:0]
    {0x3032, 0x00}, // MDBIT
    {0x3033, 0x09}, // SYS_MODE[3:0]
    {0x3050, 0xF4}, // SHR0[19:0]
    {0x3051, 0x0E}, //
{0x3052, 0x00},
    {0x3090, 0x14}, // GAIN_PCG_0[8:0]
    {0x30C1, 0x00}, // XVS_DRV[1:0]
    {0x3116, 0x23}, // INCKSEL2[7:0]
    {0x3118, 0xB4}, // INCKSEL3[10:0]
    {0x311A, 0xFC}, // INCKSEL4[10:0]
    {0x311E, 0x23}, // INCKSEL5[7:0]
    {0x32D4, 0x21}, // -
    {0x32EC, 0xA1}, // -
    {0x3452, 0x7F}, // -
    {0x3453, 0x03}, // -
    {0x358A, 0x04}, // -
    {0x35A1, 0x02}, // -
    {0x36BC, 0x0C}, // -
    {0x36CC, 0x53}, // -
    {0x36CD, 0x00}, // -
    {0x36CE, 0x3C}, // -
    {0x36D0, 0x8C}, // -
    {0x36D1, 0x00}, // -
    {0x36D2, 0x71}, // -
    {0x36D4, 0x3C}, // -
    {0x36D6, 0x53}, // -
    {0x36D7, 0x00}, // -
    {0x36D8, 0x71}, // -
    {0x36DA, 0x8C}, // -
    {0x36DB, 0x00}, // -
    {0x3701, 0x00}, // ADBIT1[7:0]
    {0x3724, 0x02}, // -
    {0x3726, 0x02}, // -
    {0x3732, 0x02}, // -
    {0x3734, 0x03}, // -
    {0x3736, 0x03}, // -
    {0x3742, 0x03}, // -
    {0x3862, 0xE0}, // -
    {0x38CC, 0x30}, // -
    {0x38CD, 0x2F}, // -
    {0x395C, 0x0C}, // -
    {0x3A42, 0xD1}, // -
    {0x3A4C, 0x77}, // -
    {0x3AE0, 0x02}, // -
    {0x3AEC, 0x0C}, // -
    {0x3B00, 0x2E}, // -
    {0x3B06, 0x29}, // -
    {0x3B98, 0x25}, // -
    {0x3B99, 0x21}, // -
    {0x3B9B, 0x13}, // -
    {0x3B9C, 0x13}, // -
    {0x3B9D, 0x13}, // -
    {0x3B9E, 0x13}, // -
    {0x3BA1, 0x00}, // -
    {0x3BA2, 0x06}, // -
    {0x3BA3, 0x0B}, // -
    {0x3BA4, 0x10}, // -
    {0x3BA5, 0x14}, // -
    {0x3BA6, 0x18}, // -
    {0x3BA7, 0x1A}, // -
    {0x3BA8, 0x1A}, // -
    {0x3BA9, 0x1A}, // -
    {0x3BAC, 0xED}, // -
    {0x3BAD, 0x01}, // -
    {0x3BAE, 0xF6}, // -
    {0x3BAF, 0x02}, // -
    {0x3BB0, 0xA2}, // -
    {0x3BB1, 0x03}, // -
    {0x3BB2, 0xE0}, // -
    {0x3BB3, 0x03}, // -
    {0x3BB4, 0xE0}, // -
    {0x3BB5, 0x03}, // -
    {0x3BB6, 0xE0}, // -
    {0x3BB7, 0x03}, // -
    {0x3BB8, 0xE0}, // -
    {0x3BBA, 0xE0}, // -
    {0x3BBC, 0xDA}, // -
    {0x3BBE, 0x88}, // -
    {0x3BC0, 0x44}, // -
    {0x3BC2, 0x7B}, // -
    {0x3BC4, 0xA2}, // -
    {0x3BC8, 0xBD}, // -
    {0x3BCA, 0xBD}, // -
    {0x4004, 0x00}, // TXCLKESC_FREQ[15:0]
    {0x4005, 0x06}, //
    {0x400C, 0x00}, // INCKSEL6
    {0x4018, 0x6F}, // TCLKPOST[15:0]
    {0x401A, 0x2F}, // TCLKPREPARE[15:0]
    {0x401C, 0x2F}, // TCLKTRAIL[15:0]
    {0x401E, 0xBF}, // TCLKZERO[15:0]
    {0x401F, 0x00}, //
    {0x4020, 0x2F}, // THSPREPARE[15:0]
    {0x4022, 0x57}, // THSZERO[15:0]
    {0x4024, 0x2F}, // THSTRAIL[15:0]
    {0x4026, 0x4F}, // THSEXIT[15:0]
    {0x4028, 0x27}, // TLPX[15:0]
    {0x4074, 0x01}, // INCKSEL7 [2:0]
    {0x3000, 0x00}, // STANDBY
    {0x3002, 0x00}, // XMASTER

    {SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_60fps_mipi[] = {
	{0x3008,0x7F},  // BCWAIT_TIME[9:0]
	{0x300A,0x5B},  // CPWAIT_TIME[9:0]
	{0x3020,0x01},  // HADD
	{0x3021,0x01},  // VADD
	{0x3022,0x01},  // ADDMODE[1:0]
	{0x3024,0x4C},  // VMAX[19:0] 0xD3E -> 3382 -> 50fps             0xB02 -> 2818 -> 60fps
	{0x3025,0x0B},  //
	{0x3028,0x6D},  // HMAX[15:0] 0x16D -> 365
	{0x3029,0x01},  //
	{0x3031,0x00},  // ADBIT[1:0]
	{0x3033,0x08},  // SYS_MODE[3:0]
	{0x3050,0x08},  // SHR0[19:0]
	{0x30C1,0x00},  // XVS_DRV[1:0]
	{0x30D9,0x02},  // DIG_CLP_VSTART[4:0]
	{0x30DA,0x01},  // DIG_CLP_VNUM[1:0]
	{0x3116,0x24},  // INCKSEL2[7:0]
	{0x3118,0xA0},  // INCKSEL3[10:0]
	{0x311E,0x24},  // INCKSEL5[7:0]
	{0x32D4,0x21},  // -
	{0x32EC,0xA1},  // -
	{0x344C,0x2B},  // -
	{0x344D,0x01},  // -
	{0x344E,0xED},  // -
	{0x344F,0x01},  // -
	{0x3450,0xF6},  // -
	{0x3451,0x02},  // -
	{0x3452,0x7F},  // -
	{0x3453,0x03},  // -
	{0x358A,0x04},  // -
	{0x35A1,0x02},  // -
	{0x35EC,0x27},  // -
	{0x35EE,0x8D},  // -
	{0x35F0,0x8D},  // -
	{0x35F2,0x29},  // -
	{0x36BC,0x0C},  // -
	{0x36CC,0x53},  // -
	{0x36CD,0x00},  // -
	{0x36CE,0x3C},  // -
	{0x36D0,0x8C},  // -
	{0x36D1,0x00},  // -
	{0x36D2,0x71},  // -
	{0x36D4,0x3C},  // -
	{0x36D6,0x53},  // -
	{0x36D7,0x00},  // -
	{0x36D8,0x71},  // -
	{0x36DA,0x8C},  // -
	{0x36DB,0x00},  // -
	{0x3701,0x00},  // ADBIT1[7:0]
	{0x3720,0x00},  // -
	{0x3724,0x02},  // -
	{0x3726,0x02},  // -
	{0x3732,0x02},  // -
	{0x3734,0x03},  // -
	{0x3736,0x03},  // -
	{0x3742,0x03},  // -
	{0x3862,0xE0},  // -
	{0x38CC,0x30},  // -
	{0x38CD,0x2F},  // -
	{0x395C,0x0C},  // -
	{0x39A4,0x07},  // -
	{0x39A8,0x32},  // -
	{0x39AA,0x32},  // -
	{0x39AC,0x32},  // -
	{0x39AE,0x32},  // -
	{0x39B0,0x32},  // -
	{0x39B2,0x2F},  // -
	{0x39B4,0x2D},  // -
	{0x39B6,0x28},  // -
	{0x39B8,0x30},  // -
	{0x39BA,0x30},  // -
	{0x39BC,0x30},  // -
	{0x39BE,0x30},  // -
	{0x39C0,0x30},  // -
	{0x39C2,0x2E},  // -
	{0x39C4,0x2B},  // -
	{0x39C6,0x25},  // -
	{0x3A42,0xD1},  // -
	{0x3A4C,0x77},  // -
	{0x3AE0,0x02},  // -
	{0x3AEC,0x0C},  // -
	{0x3B00,0x2E},  // -
	{0x3B06,0x29},  // -
	{0x3B98,0x25},  // -
	{0x3B99,0x21},  // -
	{0x3B9B,0x13},  // -
	{0x3B9C,0x13},  // -
	{0x3B9D,0x13},  // -
	{0x3B9E,0x13},  // -
	{0x3BA1,0x00},  // -
	{0x3BA2,0x06},  // -
	{0x3BA3,0x0B},  // -
	{0x3BA4,0x10},  // -
	{0x3BA5,0x14},  // -
	{0x3BA6,0x18},  // -
	{0x3BA7,0x1A},  // -
	{0x3BA8,0x1A},  // -
	{0x3BA9,0x1A},  // -
	{0x3BAC,0xED},  // -
	{0x3BAD,0x01},  // -
	{0x3BAE,0xF6},  // -
	{0x3BAF,0x02},  // -
	{0x3BB0,0xA2},  // -
	{0x3BB1,0x03},  // -
	{0x3BB2,0xE0},  // -
	{0x3BB3,0x03},  // -
	{0x3BB4,0xE0},  // -
	{0x3BB5,0x03},  // -
	{0x3BB6,0xE0},  // -
	{0x3BB7,0x03},  // -
	{0x3BB8,0xE0},  // -
	{0x3BBA,0xE0},  // -
	{0x3BBC,0xDA},  // -
	{0x3BBE,0x88},  // -
	{0x3BC0,0x44},  // -
	{0x3BC2,0x7B},  // -
	{0x3BC4,0xA2},  // -
	{0x3BC8,0xBD},  // -
	{0x3BCA,0xBD},  // -
	{0x4004,0x48},  // TXCLKESC_FREQ[15:0]
	{0x4005,0x09},  //
	{0x4018,0xA7},  // TCLKPOST[15:0]
	{0x401A,0x57},  // TCLKPREPARE[15:0]
	{0x401C,0x5F},  // TCLKTRAIL[15:0]
	{0x401E,0x97},  // TCLKZERO[15:0]
	{0x4020,0x5F},  // THSPREPARE[15:0]
	{0x4022,0xAF},  // THSZERO[15:0]
	{0x4024,0x5F},  // THSTRAIL[15:0]
	{0x4026,0x9F},  // THSEXIT[15:0]
	{0x4028,0x4F},  // TLPX[15:0]
	{0x3000,0x00},
	{SENSOR_REG_DELAY,0x18},
	{0x3002,0x00},
	{SENSOR_REG_END, 0x00},
};
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 3840*2160@30fps [0]*/
	{
		.width = 3840,
		.height = 2160,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_30fps_mipi,
	},
	/* 3840*2160@15fps [1]*/
	{
		.width = 3840,
		.height = 2160,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_15fps_mipi,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 60 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_60fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

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

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3b00, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3b06, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;
	vmax = sensor_attr.total_height;
	shs = vmax - value + 8;
	ret = sensor_write(sd, 0x3050, (unsigned char)(shs & 0xff));
	ret += sensor_write(sd, 0x3051, (unsigned char)((shs >> 8) & 0xff));
	ret += sensor_write(sd, 0x3052, (unsigned char)((shs >> 16) & 0x0f));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x3090, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
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

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
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

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
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

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot) {
	case 0:
		wpclk = SENSOR_SUPPORT_SCLK_8M;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	case 1:
		wpclk = SENSOR_SUPPORT_SCLK_8M;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	case 2:
		wpclk = 365 * 2892 * 60;
		max_fps = TX_SENSOR_MAX_FPS_60;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}
	ret += sensor_read(sd, 0x3029, &tmp);
	hts = tmp & 0x0f;
	ret += sensor_read(sd, 0x3028, &tmp);
	if (ret < 0)
		return -1;
	hts = (hts << 8) + tmp;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x3001, 0x01);
	ret = sensor_write(sd, 0x3026, (unsigned char)((vts & 0xf0000) >> 16));
	ret += sensor_write(sd, 0x3025, (unsigned char)((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x3024, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x3001, 0x00);
	if (ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts- 4 ;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;

	/* 2'b01:mirror,2'b10:filp */
	val = sensor_read(sd, 0x3030, &val);
	switch(enable) {
	case 0://normal
		val &= 0xfc;
		break;
	case 1://sensor mirror
		val |= 0x01;
		break;
	case 2://sensor flip
		val |= 0x02;
		break;
	case 3://sensor mirror&flip
		val |= 0x03;
		break;
	}
	ret = sensor_write(sd, 0x3030, val);

	return ret;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *tclk;
	unsigned long rate;
	uint8_t i;
	int ret = 0;

	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.again = 0;
		sensor_attr.integration_time = 0xef4;
		memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
		break;
	case 1:
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.max_integration_time_native = 4503 - 4;
		sensor_attr.integration_time_limit = 4503 - 4;
		sensor_attr.total_width = 1066;
		sensor_attr.total_height = 4503;
		sensor_attr.max_integration_time = 4503 - 4;
		sensor_attr.again = 0;
		sensor_attr.integration_time = 0xef4;
		memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
		break;
	case 2:
		wsize = &sensor_win_sizes[2];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.max_integration_time_native = 2892 - 4;
		sensor_attr.integration_time_limit = 2892 - 4;
		sensor_attr.total_width = 365;
		sensor_attr.total_height = 2892;
		sensor_attr.max_integration_time = 2892 - 4;
		sensor_attr.again = 0;
		sensor_attr.integration_time = 0x2000;
		memcpy(&(sensor_attr.mipi), &sensor_1080P_mipi, sizeof(sensor_1080P_mipi));
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 0;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->mclk) {
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

	switch(info->default_boot) {
	case 0:
	case 1:
		rate = private_clk_get_rate(sensor->mclk);
		if (((rate / 1000) % (MCLK / 1000)) != 0) {
			uint8_t sclk_name_num = sizeof(sclk_name)/sizeof(sclk_name[0]);
			for (i=0; i < sclk_name_num; i++) {
				tclk = private_devm_clk_get(&client->dev, sclk_name[i]);
				ret = clk_set_parent(sclka, clk_get(NULL, sclk_name[i]));
				if (IS_ERR(tclk)) {
					pr_err("get sclka failed\n");
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

		private_clk_set_rate(sensor->mclk, MCLK);
		private_clk_prepare_enable(sensor->mclk);
		break;
	case 2:
		if (((rate / 1000) % 31725) != 0) {
			ret = clk_set_parent(sclka, clk_get(NULL, "epll"));
			sclka = private_devm_clk_get(&client->dev, "epll");
			if (IS_ERR(sclka)) {
				pr_err("get sclka failed\n");
			} else {
				rate = private_clk_get_rate(sclka);
				if (((rate / 1000) % 31725) != 0) {
					private_clk_set_rate(sclka, 891000000);
				}
			}
		}
		private_clk_set_rate(sensor->mclk, 31725000);
		private_clk_prepare_enable(sensor->mclk);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
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
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
		/*	case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
			ret = sensor_set_expo(sd, sensor_val->value);
			break;   */
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
		if (arg)
			ret = sensor_set_vflip(sd, sensor_val->value);
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

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
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
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
