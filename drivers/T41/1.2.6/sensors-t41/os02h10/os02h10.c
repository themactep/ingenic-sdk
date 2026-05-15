/*
 * os02h10.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080        30       mipi_2lane           linear
 *   1          1920*1088        30       mipi_2lane           linear
 */
#define DEBUG
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


#define OS02H10_CHIP_ID_H	(0x53)
#define OS02H10_CHIP_ID_M0	(0x02)
#define OS02H10_CHIP_ID_M1	(0x48)
#define OS02H10_CHIP_ID_L	(0x10)
#define OS02H10_REG_END 0xff
#define OS02H10_REG_DELAY 0xfe
#define OS02H10_SUPPORT_30FPS_SCLK 4000*2250*15
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20240223a"

uint8_t dismode;
static int rst_gpio = GPIO_PA(18);
//static int pwdn_gpio = -1;

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list
{
	unsigned char reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut
{
	unsigned int value;
	unsigned int gain;
};

struct again_lut os02h10_again_lut[] = {
	{0x0010, 0},
	{0x0011, 5731},
	{0x0012, 11136},
	{0x0013, 16248},
	{0x0014, 21097},
	{0x0015, 25710},
	{0x0016, 30109},
	{0x0017, 34312},
	{0x0018, 38336},
	{0x0019, 42195},
	{0x001a, 45904},
	{0x001b, 49472},
	{0x001c, 52910},
	{0x001d, 56228},
	{0x001e, 59433},
	{0x001f, 62534},
	{0x0021, 65536},
	{0x0023, 71267},
	{0x0025, 76672},
	{0x0027, 81784},
	{0x0029, 86633},
	{0x002b, 91246},
	{0x002d, 95645},
	{0x002f, 99848},
	{0x0031, 103872},
	{0x0033, 107731},
	{0x0035, 111440},
	{0x00cf, 115008},
	{0x00cd, 118446},
	{0x003b, 121764},
	{0x003d, 124969},
	{0x003f, 128070},
	{0x0043, 131072},
	{0x0047, 136803},
	{0x004b, 142208},
	{0x004f, 147320},
	{0x0053, 152169},
	{0x0057, 156782},
	{0x005b, 161181},
	{0x005f, 165384},
	{0x0063, 169408},
	{0x0067, 173267},
	{0x006b, 176976},
	{0x006f, 180544},
	{0x0073, 183982},
	{0x0077, 187300},
	{0x007b, 190505},
	{0x007f, 193606},
	{0x0087, 196608},
	{0x008f, 202339},
	{0x0097, 207744},
	{0x009f, 212856},
	{0x00a7, 217705},
	{0x00af, 222318},
	{0x00b7, 226717},
	{0x00bf, 230920},
	{0x00c7, 234944},
	{0x00cf, 238803},
	{0x00d7, 242512},
	{0x00df, 246080},
	{0x00e7, 249518},
	{0x00ef, 252836},
	{0x00f7, 256041},
	{0x00ff, 259142},

};

struct tx_isp_sensor_attribute os02h10_attr;

unsigned int os02h10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = os02h10_again_lut;

	while (lut->gain <= os02h10_attr.max_again)
	{
		if (isp_gain == 0)
		{
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if (isp_gain < lut->gain)
		{
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else
		{
			if ((lut->gain == os02h10_attr.max_again) && (isp_gain >= lut->gain))
			{
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}
	return isp_gain;
}

unsigned int os02h10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus mipi_1088 = {
        .mode = SENSOR_MIPI_SONY_MODE,
        .clk = 1008,
        /* .clk = 1200, */
        .lans = 2,
        .settle_time_apative_en = 0,
        .image_twidth = 1920,
        .image_theight = 1088,
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
        .mipi_sc.data_type_en = 0,
        .mipi_sc.data_type_value = RAW12,
        .mipi_sc.del_start = 0,
        .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
        .mipi_sc.sensor_fid_mode = 0,
        .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute os02h10_attr = {
	.name = "os02h10",
	.chip_id = 0x53024810,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x3c,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 1008,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12, // RAW12
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
		.mipi_sc.data_type_value = 0,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 2250 - 4,
	.integration_time_limit = 2250 - 4,
	.total_width = 1244,
	.total_height = 2250,
	.max_integration_time = 2250 - 4,
	.one_line_expr_in_us = 15,
	.integration_time_apply_delay = 1,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = os02h10_alloc_again,
	.sensor_ctrl.alloc_dgain = os02h10_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};



static struct regval_list os02h10_init_regs_1920_1080_30fps_mipi[] = {
    //{0xfd ,0x00},
    //{0x20 ,0x00},
    //{0x20, 0x00},//
    {0x53, 0xfe},//
    {0x54, 0x7f},//
    {0x61, 0xa8},//  ;mpll_divp_8lsb
    {0x63, 0x05},//  ;mpll_prediv
    {0x64, 0x00},//  ;mpll_divout
    {0x65, 0x00},//  ;mpll_divsys
    {0x66, 0x03},//  ;mpll_divbit
    {0x67, 0x00},//  ;mpll_predivp
    {0x8d, 0x00},//  ;mipi_rst.mipi_pd
    {0x8e, 0x07},//  ;mipi_hsize_4msb
    {0x8f, 0x80},//  ;mipi_hsize_8lsb
    {0x90, 0x04},//  ;mipi_vsize_4msb
    {0x91, 0x38},//  ;mipi_vsize_8lsb
    {0x94, 0x77},//
    {0x95, 0x88},//
    {0x98, 0xa9},//
    {0x97, 0x2c},//  ;data id for long frame raw12
    {0x99, 0x2c},//  ;data id for emb, raw12
    {0x9a, 0x2c},//  ;data id for short frame raw12
    {0xa1, 0x05},//  ;
    {0xb6, 0x40},//  ;mipi 2lane
    {0x23, 0x00},//  ;pll_predivp
    {0x24, 0x05},//  ;pll_prediv
    {0x26, 0x4d},//  ;pll_divp
    {0x29, 0x00},//  ;pll_divdac
    {0x2a, 0x03},//  ;pll_divpump
    {0x31, 0x90},//  ;clk_mode
    {0x32, 0x04},//  ;clk_mode2
    {0x33, 0x00},//  ;ao_mode_en
    {0x38, 0x00},//  ;lvds_en
    {0x55, 0x00},//  ;pclk_in_sel
    {0xf5, 0x01},//  ;osc_cal_en
    {0xfd, 0x01},//
    {0x03, 0x00},//  ;exp1 msb
    {0x04, 0x04},//  ;exp1 lsb
    {0x05, 0x04},// ;00
    {0x06, 0x65},// ;00
    {0x09, 0x00},//
    {0x0a, 0xd0},//
    {0x24, 0xff},//  ;again
    {0x3e, 0x3c},//  ;scg_en=1,col_gain
    {0x3f, 0x01},//  ;mirror
    {0x01, 0x01},//
    {0x11, 0x40},//  ;[6]db_shutter
    {0x14, 0x65},//
    {0x15, 0x00},//
    {0x16, 0x95},//
    {0x18, 0xf3},//
    {0x19, 0x47},//
    {0x1a, 0x03},//
    {0x1b, 0x50},//
    {0x1c, 0xf0},//  ;icomp1/icomp2
    {0x1d, 0x3d},//
    {0x1e, 0x00},//  ;col_comp1_cap_sel
    {0x1f, 0x27},//  ;ipix
    {0x21, 0x74},//  ;ramp bias sel
    {0x22, 0x90},//  ;vrhv
    {0x25, 0x00},//
    {0x26, 0x77},//  ;vrnv1/vrnv2
    {0x27, 0xf9},//  ;vcap
    {0x2a, 0x4e},//  ;ramp_cur_code(adc_range)
    {0x2e, 0x28},//
    {0x34, 0xf6},//  ;vbl up/dn
    {0x35, 0x22},//  ;
    {0x36, 0xa2},//  ;
    {0x50, 0x00},//  ;p0
    {0x51, 0x08},//  ;p1
    {0x52, 0x07},//  ;P2
    {0x53, 0x00},//  ;p3
    {0x55, 0x35},//
    {0x56, 0x01},//
    {0x57, 0x0a},//
    {0x59, 0x01},//
    {0x5b, 0x13},//
    {0x5d, 0x08},//
    {0x5e, 0x00},//  ;p12,rowsg
    {0x5f, 0x1d},//  ;p13,rowsel
    {0x60, 0x1e},//  ;p14,scg
    {0x68, 0x05},//
    {0x69, 0x04},//  ;p24
    {0x6a, 0x00},//
    {0x70, 0x64},//  ;p31,rst_d0
    {0x71, 0x15},//  ;p32,rst_d1
    {0x72, 0x42},//  ;p33,sig_d0
    {0x7b, 0x40},//  ;p43
    {0x80, 0x08},//
    {0x86, 0x2d},//
    {0x8a, 0xf9},//
    {0x95, 0x25},//
    {0x97, 0x10},//
    {0x99, 0x30},//
    {0x9f, 0x10},//  ;p90,rowsel
    {0xa8, 0x10},//  ;p99,rowsg
    {0xad, 0x00},//  ;p106,restg
    {0xb0, 0x63},//  ;sc1_1
    {0xb1, 0x63},//  ;sc1_2
    {0xb2, 0x63},//  ;sc1_3
    {0xb3, 0x63},//  ;sc1_4
    {0xb4, 0x61},//  ;sc2_1
    {0xb5, 0x61},//  ;sc2_2
    {0xb6, 0x61},//  ;sc2_3
    {0xb7, 0x61},//  ;sc2_4
    {0xb8, 0x77},//  ;vbl_1/2_scg0
    {0xb9, 0x66},//  ;vbl_3/4_scg0
    {0xba, 0x07},//  ;vofs_1
    {0xbb, 0x07},//  ;vofs_2
    {0xbc, 0x07},//  ;vofs_3
    {0xbd, 0x07},//  ;vofs_4
    {0x29, 0x0a},//  ;ramp_psrr_sw_gain
    {0x2b, 0x03},//  ;ramp_psrr_bias_sel
    {0x30, 0x04},//  ;ramp_psnc_cap2_ctrl
    {0x92, 0x05},//
    {0xc4, 0x00},//  ;
    {0xc5, 0x00},//  ;
    {0xc6, 0x13},//  ;
    {0xc7, 0x6d},//  ;
    {0xc9, 0x00},//  ;
    {0xd0, 0x77},//  ;vbl_1/2_scg1
    {0xd1, 0x66},//  ;vbl_3/4_scg1
    {0xd5, 0x12},//  ;col_en1_num_8lsb
    {0xd7, 0x40},//  ;col_en2_num_8lsb
    {0xdc, 0x01},//  ;ulp pwd en
    {0xdd, 0x01},//  ;ulp start
    {0xde, 0x04},//  ;ulp end
    {0xfd, 0x02},//
    {0x68, 0x02},//  ;[1]high/low 8 bit
    {0x6d, 0x5d},//  ;blc en
    {0x6e, 0xdc},//  ;auto trigger
    {0x78, 0x70},//  ;blc k
    {0x79, 0x70},//
    {0x7a, 0x70},//
    {0x7b, 0x70},//
    {0x80, 0x60},//
    {0x81, 0x60},//
    {0x82, 0x12},//
    {0x83, 0x60},//
    {0x84, 0x60},//
    {0x9f, 0x12},//
    {0xa1, 0x04},//  ;dig vstart
    {0xa2, 0x04},//  ;dig vsize
    {0xa3, 0x38},//
    {0xa5, 0x04},//  ;dig hstart
    {0xa6, 0x07},//  ;dig hsize
    {0xa7, 0x80},//
    {0x72, 0x40},//  ;offset
    {0x73, 0x40},//
    {0x74, 0x40},//
    {0x75, 0x40},//
    {0xfd, 0x06},//
    {0x00, 0x04},// ;r_tmp_slope_h[7:0]
    {0x01, 0x7c},// ;r_tmp_slope_l[7:0]
    {0x02, 0xbf},// ;r_tmp_offset_3[7:0]
    {0x03, 0x86},// ;r_tmp_offset_2[7:0]
    {0x04, 0xbd},// ;r_tmp_offset_1[7:0]
    {0x05, 0x02},// ;r_tmp_offset_0[7:0]
    {0xfd, 0x02},//
    {0x52, 0xff},//
    {0xfd, 0x01},//
    {0xfd, 0x00},//

    {0xb1, 0x02},// ;[1]mipi_en
    {0xfd, 0x01},//
    {OS02H10_REG_DELAY, 0x20}, /* END MARKER */
	{OS02H10_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list os02h10_init_regs_1920_1088_30fps_mipi[] = {
    //{0xfd ,0x00},//
    //{0x20 ,0x00},//
    //{OS02H10_REG_DELAY, 0x05}, /* END MARKER */
    {0x53 ,0xfe},//
    {0x54 ,0x7f},//
    {0x61 ,0xa8},//  ;mpll_divp_8lsb
    {0x63 ,0x05},//  ;mpll_prediv
    {0x64 ,0x00},//  ;mpll_divout
    {0x65 ,0x00},//  ;mpll_divsys
    {0x66 ,0x03},//  ;mpll_divbit
    {0x67 ,0x00},//  ;mpll_predivp
    {0x8d ,0x00},//  ;mipi_rst.mipi_pd
    {0x8e ,0x07},//  ;mipi_hsize_4msb
    {0x8f ,0x80},//  ;mipi_hsize_8lsb
    {0x90 ,0x04},//  ;mipi_vsize_4msb
    {0x91 ,0x38},//  ;mipi_vsize_8lsb
    {0x94 ,0x77},//
    {0x95 ,0x88},//
    {0x98 ,0xa9},//
    {0x97 ,0x2c},//  ;data id for long frame raw12
    {0x99 ,0x2c},//  ;data id for emb, raw12
    {0x9a ,0x2c},//  ;data id for short frame raw12
    {0xa1 ,0x05},//  ;
    {0xb6 ,0x40},//  ;mipi 2lane
    {0x23 ,0x00},//  ;pll_predivp
    {0x24 ,0x05},//  ;pll_prediv
    {0x26 ,0x4d},//  ;pll_divp
    {0x29 ,0x00},//  ;pll_divdac
    {0x2a ,0x03},//  ;pll_divpump
    {0x31 ,0x90},//  ;clk_mode
    {0x32 ,0x04},//  ;clk_mode2
    {0x33 ,0x00},//  ;ao_mode_en
    {0x38 ,0x00},//  ;lvds_en
    {0x55 ,0x00},//  ;pclk_in_sel
    {0xf5 ,0x01},//  ;osc_cal_en
    {0xfd ,0x01},//
    {0x03 ,0x00},//  ;exp1 msb
    {0x04 ,0x04},//  ;exp1 lsb
    {0x05 ,0x04},// ;00
    {0x06 ,0x65},// ;00
    {0x09 ,0x00},//
    {0x0a ,0xd0},//
    {0x24 ,0xff},//  ;again
    {0x3e ,0x3c},//  ;scg_en=1,col_gain
    {0x3f ,0x01},//  ;mirror
    {0x01 ,0x01},//
    {0x11 ,0x40},//  ;[6]db_shutter
    {0x14 ,0x65},//
    {0x15 ,0x00},//
    {0x16 ,0x95},//
    {0x18 ,0xf3},//
    {0x19 ,0x47},//
    {0x1a ,0x03},//
    {0x1b ,0x50},//
    {0x1c ,0xf0},//  ;icomp1/icomp2
    {0x1d ,0x3d},//
    {0x1e ,0x00},//  ;col_comp1_cap_sel
    {0x1f ,0x27},//  ;ipix
    {0x21 ,0x74},//  ;ramp bias sel
    {0x22 ,0x90},//  ;vrhv
    {0x25 ,0x00},//
    {0x26 ,0x77},//  ;vrnv1/vrnv2
    {0x27 ,0xf9},//  ;vcap
    {0x2a ,0x4e},//  ;ramp_cur_code(adc_range)
    {0x2e ,0x28},//
    {0x34 ,0xf6},//  ;vbl up/dn
    {0x35 ,0x22},//  ;
    {0x36 ,0xa2},//  ;
    {0x50 ,0x00},//  ;p0
    {0x51 ,0x08},//  ;p1
    {0x52 ,0x07},//  ;P2
    {0x53 ,0x00},//  ;p3
    {0x55 ,0x35},//
    {0x56 ,0x01},//
    {0x57 ,0x0a},//
    {0x59 ,0x01},//
    {0x5b ,0x13},//
    {0x5d ,0x08},//
    {0x5e ,0x00},//  ;p12,rowsg
    {0x5f ,0x1d},//  ;p13,rowsel
    {0x60 ,0x1e},//  ;p14,scg
    {0x68 ,0x05},//
    {0x69 ,0x04},//  ;p24
    {0x6a ,0x00},//
    {0x70 ,0x64},//  ;p31,rst_d0
    {0x71 ,0x15},//  ;p32,rst_d1
    {0x72 ,0x42},//  ;p33,sig_d0
    {0x7b ,0x40},//  ;p43
    {0x80 ,0x08},//
    {0x86 ,0x2d},//
    {0x8a ,0xf9},//
    {0x95 ,0x25},//
    {0x97 ,0x10},//
    {0x99 ,0x30},//
    {0x9f ,0x10},//  ;p90,rowsel
    {0xa8 ,0x10},//  ;p99,rowsg
    {0xad ,0x00},//  ;p106,restg
    {0xb0 ,0x63},//  ;sc1_1
    {0xb1 ,0x63},//  ;sc1_2
    {0xb2 ,0x63},//  ;sc1_3
    {0xb3 ,0x63},//  ;sc1_4
    {0xb4 ,0x61},//  ;sc2_1
    {0xb5 ,0x61},//  ;sc2_2
    {0xb6 ,0x61},//  ;sc2_3
    {0xb7 ,0x61},//  ;sc2_4
    {0xb8 ,0x77},//  ;vbl_1/2_scg0
    {0xb9 ,0x66},//  ;vbl_3/4_scg0
    {0xba ,0x07},//  ;vofs_1
    {0xbb ,0x07},//  ;vofs_2
    {0xbc ,0x07},//  ;vofs_3
    {0xbd ,0x07},//  ;vofs_4
    {0x29 ,0x0a},//  ;ramp_psrr_sw_gain
    {0x2b ,0x03},//  ;ramp_psrr_bias_sel
    {0x30 ,0x04},//  ;ramp_psnc_cap2_ctrl
    {0x92 ,0x05},//
    {0xc4 ,0x00},//  ;
    {0xc5 ,0x00},//  ;
    {0xc6 ,0x13},//  ;
    {0xc7 ,0x6d},//  ;
    {0xc9 ,0x00},//  ;
    {0xd0 ,0x77},//  ;vbl_1/2_scg1
    {0xd1 ,0x66},//  ;vbl_3/4_scg1
    {0xd5 ,0x12},//  ;col_en1_num_8lsb
    {0xd7 ,0x40},//  ;col_en2_num_8lsb
    {0xdc ,0x01},//  ;ulp pwd en
    {0xdd ,0x01},//  ;ulp start
    {0xde ,0x04},//  ;ulp end
    {0xfd ,0x02},//
    {0x68 ,0x02},//  ;[1]high/low 8 bit
    {0x6d ,0x5d},//  ;blc en
    {0x6e ,0xdc},//  ;auto trigger
    {0x78 ,0x70},//  ;blc k
    {0x79 ,0x70},//
    {0x7a ,0x70},//
    {0x7b ,0x70},//
    {0x80 ,0x60},//
    {0x81 ,0x60},//
    {0x82 ,0x12},//
    {0x83 ,0x60},//
    {0x84 ,0x60},//
    {0x9f ,0x12},//
    {0xa1 ,0x00},//  ;04  ;dig vstart
    {0xa2 ,0x04},//  ;dig vsize
    {0xa3 ,0x40},//  ;38
    {0xa5 ,0x04},//  ;dig hstart
    {0xa6 ,0x07},//  ;dig hsize
    {0xa7 ,0x80},//
    {0x72 ,0x40},//  ;offset
    {0x73 ,0x40},//
    {0x74 ,0x40},//
    {0x75 ,0x40},//
    {0xfd ,0x06},//
    {0x00 ,0x04},// ;r_tmp_slope_h[7:0]
    {0x01 ,0x7c},// ;r_tmp_slope_l[7:0]
    {0x02 ,0xbf},// ;r_tmp_offset_3[7:0]
    {0x03 ,0x86},// ;r_tmp_offset_2[7:0]
    {0x04 ,0xbd},// ;r_tmp_offset_1[7:0]
    {0x05 ,0x02},// ;r_tmp_offset_0[7:0]
    {0xfd ,0x02},//
    {0x52 ,0xff},//
    {0xfd ,0x01},//
    {0xfd ,0x00},//
    {0xb1 ,0x02},// ;[1]mipi_en
    {0xfd ,0x01},//
    {OS02H10_REG_DELAY, 0x20}, /* END MARKER */
	{OS02H10_REG_END, 0x00}, /* END MARKER */
};

/*
 * the order of the os02h10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os02h10_win_sizes[] = {
	/* [0] 2560*1440@30fps linear */
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = os02h10_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width = 1920,
		.height = 1088,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = os02h10_init_regs_1920_1088_30fps_mipi,
	},


};
struct tx_isp_sensor_win_setting *wsize = &os02h10_win_sizes[0];
/*
 * the part of driver was fixed.
 */
static struct regval_list os02h10_stream_on_mipi[] = {
	{OS02H10_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list os02h10_stream_off_mipi[] = {
	{OS02H10_REG_END, 0x00}, /* END MARKER */
};
int os02h10_read(struct tx_isp_subdev *sd, unsigned char reg,
			   unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
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
int os02h10_write(struct tx_isp_subdev *sd, unsigned char reg,
				unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}
/*
static int os02h10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OS02H10_REG_END) {
		if (vals->reg_num == OS02H10_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = os02h10_read(sd, vals->reg_num, &val);
			 printk("{0x%x, 0x%x}\n", vals->reg_num, val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
*/
static int os02h10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{

	int ret;
	while (vals->reg_num != OS02H10_REG_END)
	{
		if (vals->reg_num == OS02H10_REG_DELAY)
		{
			private_msleep(vals->value);
		}
		else
		{
			ret = os02h10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int os02h10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int os02h10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = os02h10_read(sd, 0x02, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS02H10_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = os02h10_read(sd, 0x03, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS02H10_CHIP_ID_M0)
		return -ENODEV;
	*ident = (*ident << 8) | v;

    ret = os02h10_read(sd, 0x04, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS02H10_CHIP_ID_M1)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = os02h10_read(sd, 0x05, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS02H10_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 16) | v;
	return 0;
}

static int os02h10_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int expo = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

    ret += os02h10_write(sd, 0xfd, 0x01);


	// set integration_time
    ret += os02h10_write(sd, 0x01, 0x01);
	ret += os02h10_write(sd, 0x04, (unsigned char)(expo & 0xff));
	ret += os02h10_write(sd, 0x03, (unsigned char)((expo >> 8) & 0xff));
    ret += os02h10_write(sd, 0x01, 0x01);
    // set sensor again
    ret = os02h10_write(sd, 0x24, (unsigned char)(again & 0xff));
    ret += os02h10_write(sd, 0x01, 0x01);

	if (ret < 0)
		ISP_ERROR("err: os02h10_write err\n");

	return ret;
}
/*
static int os02h10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret = os02h10_write(sd,  0x01, (unsigned char)(expo & 0xff));
	ret += os02h10_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int os02h10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret += os02h10_write(sd, 0x00, (unsigned char)(value & 0x7f));
	if (ret < 0)
				ISP_ERROR("%s %d, sensor reg write err!!\n",__func__,__LINE__);
	return ret;

	return 0;
}
*/
static int os02h10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os02h10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
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
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;
}

static int os02h10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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

static int os02h10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable)
	{
		if (sensor->video.state == TX_ISP_MODULE_INIT)
		{
			ret = os02h10_write_array(sd, wsize->regs);
			ret = os02h10_read(sd, 0x27, &dismode);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING)
		{

			ret = os02h10_write_array(sd, os02h10_stream_on_mipi);
			ISP_WARNING("os02h10 stream on\n");
		}
	}
	else
	{
		ret = os02h10_write_array(sd, os02h10_stream_off_mipi);
		ISP_WARNING("os02h10 stream off\n");
	}

	return ret;
}

static int os02h10_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
    unsigned int vb = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; // the format is 24.8
	int ret = 0;

	sclk = 30*2250*1244;
	max_fps = 30;
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
	{
		ISP_WARNING("warn: fps(%d) no in range\n", fps);
		return -1;
	}

    ret = os02h10_write(sd, 0xfd, 0x01);
	ret += os02h10_read(sd, 0xda, &val);
	hts = val;
	ret += os02h10_read(sd, 0xdb, &val);
	hts = (hts << 8) + val; /* frame width = hts*8 */


	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
    vb=vts-1125;
	ret = os02h10_write(sd, 0x06, (unsigned char)(vb & 0xff));
	ret += os02h10_write(sd, 0x05, (unsigned char)(vb >> 8));
    ret += os02h10_write(sd, 0x01, 0x01);

	if (0 != ret)
	{
		ISP_ERROR("err: os02h10_write err\n");
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

static int os02h10_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize)
	{
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int os02h10_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0x20;
    ret = os02h10_write(sd, 0xfd, 0x00);
    ret = os02h10_write(sd, 0xb1, 0x00);
    ret = os02h10_write(sd, 0xfd, 0x01);
	/* 2'b01: mirror; 2'b10:flip*/
	enable &= 0x03;
	switch (enable)
	{
	case 0: /*normal*/
		val = 0x00;
        sensor->video.mbus.code =TISP_VI_FMT_SGBRG12_1X12;
		break;
	case 1: /*mirror*/
		val = 0x01;
        sensor->video.mbus.code =TISP_VI_FMT_SBGGR12_1X12;
		break;
	case 2: /*filp*/
        val = 0x02;
         sensor->video.mbus.code =TISP_VI_FMT_SRGGB12_1X12;
		break;
	case 3: /*mirror & filp*/
        val = 0x03;
        sensor->video.mbus.code =TISP_VI_FMT_SGRBG12_1X12;
		break;
	default:
		break;
	}

	ret = os02h10_write(sd, 0x3f, val);
    ret = os02h10_write(sd, 0x01, 0x01);
	if (0 != ret)
	{
		ISP_ERROR("%s:%d, os02h10_write err!!\n", __func__, __LINE__);
		return ret;
	}

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
    sensor->video.mbus_change = 1;
    ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	ret = os02h10_write(sd, 0xfd, 0x00);
    ret = os02h10_write(sd, 0xb1, 0x02);
    return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;

	switch (info->default_boot)
	{
	case 0:
		wsize = &os02h10_win_sizes[0];
		os02h10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		os02h10_attr.max_integration_time_native = 2250 - 4;
		os02h10_attr.integration_time_limit = 2250 - 4;
		os02h10_attr.total_width = 1244*2;
		os02h10_attr.total_height = 2250;
		os02h10_attr.max_integration_time = 2250 - 4;
		os02h10_attr.again = 0;
		os02h10_attr.max_again = 259142,
		os02h10_attr.integration_time = 0x1;
		break;
    case 1:
		wsize = &os02h10_win_sizes[1];
        memcpy((void*)(&(os02h10_attr.mipi)),(void*)(&mipi_1088),sizeof(mipi_1088));
		os02h10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		os02h10_attr.max_integration_time_native = 2250 - 4;
		os02h10_attr.integration_time_limit = 2250 - 4;
		os02h10_attr.total_width = 1244*2;
		os02h10_attr.total_height = 2250;
		os02h10_attr.max_integration_time = 2250 - 4;
		os02h10_attr.again = 0;
		os02h10_attr.max_again = 259142,
		os02h10_attr.integration_time = 0x1;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
		break;
	}

	switch (info->video_interface)
	{
	case TISP_SENSOR_VI_MIPI_CSI0:
		os02h10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os02h10_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		os02h10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os02h10_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		os02h10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

	switch (info->mclk)
	{
	case TISP_SENSOR_MCLK0:
	case TISP_SENSOR_MCLK1:
	case TISP_SENSOR_MCLK2:
		sclka = private_devm_clk_get(&client->dev, "mux_cim");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim");
		set_sensor_mclk_function(0);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk))
	{
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;

err_get_mclk:
	return -1;
}

static int os02h10_g_chip_ident(struct tx_isp_subdev *sd,
							  struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	sensor_attr_check(sd);
	info->rst_gpio = rst_gpio;
	if (1)
	{
		ret = private_gpio_request(info->rst_gpio, "os02h10_reset");
		if (!ret)
		{
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(5);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1)
	{
		ret = private_gpio_request(info->pwdn_gpio, "os02h10_pwdn");
		if (!ret)
		{
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = os02h10_detect(sd, &ident);
	if (ret)
	{
		ISP_ERROR("chip found @ 0x%x (%s) is not an os02h10 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("os02h10 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
	if (chip)
	{
		memcpy(chip->name, "os02h10", sizeof("os02h10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int os02h10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd))
	{
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd)
	{
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = os02h10_set_expo(sd, sensor_val->value);
		break;

	/*case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = os02h10_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = os02h10_set_analog_gain(sd, sensor_val->value);
		break;
*/
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = os02h10_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = os02h10_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = os02h10_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = os02h10_write_array(sd, os02h10_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = os02h10_write_array(sd, os02h10_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = os02h10_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = os02h10_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int os02h10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = os02h10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int os02h10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	os02h10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops os02h10_core_ops = {
	.g_chip_ident = os02h10_g_chip_ident,
	.reset = os02h10_reset,
	.init = os02h10_init,
	/*.ioctl = os02h10_ops_ioctl,*/
	.g_register = os02h10_g_register,
	.s_register = os02h10_s_register,
};

static struct tx_isp_subdev_video_ops os02h10_video_ops = {
	.s_stream = os02h10_s_stream,
};

static struct tx_isp_subdev_sensor_ops os02h10_sensor_ops = {
	.ioctl = os02h10_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops os02h10_ops = {
	.core = &os02h10_core_ops,
	.video = &os02h10_video_ops,
	.sensor = &os02h10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "os02h10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int os02h10_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
	{
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	os02h10_attr.expo_fs = 0;
	sensor->video.shvflip = 1;
	sensor->video.attr = &os02h10_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &os02h10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->os02h10\n");

	return 0;
}

static int os02h10_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	if (info->rst_gpio != -1)
		private_gpio_free(info->rst_gpio);
	if (info->pwdn_gpio != -1)
		private_gpio_free(info->pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id os02h10_id[] = {
	{"os02h10", 0},
	{}};
MODULE_DEVICE_TABLE(i2c, os02h10_id);

static struct i2c_driver os02h10_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "os02h10",
	},
	.probe = os02h10_probe,
	.remove = os02h10_remove,
	.id_table = os02h10_id,
};

static __init int init_os02h10(void)
{
	/* ret = private_driver_get_interface(); */
	/* if(ret){ */
	/* 	ISP_ERROR("Failed to init os02h10 dirver.\n"); */
	/* 	return -1; */
	/* } */
	return private_i2c_add_driver(&os02h10_driver);
}

static __exit void exit_os02h10(void)
{
	private_i2c_del_driver(&os02h10_driver);
}

module_init(init_os02h10);
module_exit(exit_os02h10);

MODULE_DESCRIPTION("A low-level driver for SOI os02h10 sensors");
MODULE_LICENSE("GPL");
