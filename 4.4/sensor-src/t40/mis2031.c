// SPDX-License-Identifier: GPL-2.0+
/*
 * mis2031.c
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

#define SENSOR_NAME "mis2031"
#define SENSOR_CHIP_ID_H (0x20)
#define SENSOR_CHIP_ID_L (0x09)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (74250000)
#define SENSOR_SUPPORT_SCLK_90fps (222750000)
#define SENSOR_SUPPORT_SCLK_120fps (216000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20220415a"

static int reset_gpio = GPIO_PC(28);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int wdr_bufsize = 4073600;//1451520;
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0, 0},   // 1.000000
	{0x10, 1465},   // 1.015873
	{0x20, 2998},   // 1.032258
	{0x30, 4506},   // 1.049180
	{0x40, 6077},   // 1.066667
	{0x50, 7623},   // 1.084746
	{0x60, 9228},   // 1.103448
	{0x70, 10888},   // 1.122807
	{0x80, 12601},   // 1.142857
	{0x90, 14283},   // 1.163636
	{0xa0, 16014},   // 1.185185
	{0xb0, 17789},   // 1.207547
	{0xc0, 19607},   // 1.230769
	{0xd0, 21465},   // 1.254902
	{0xe0, 23287},   // 1.280000
	{0xf0, 25216},   // 1.306122
	{0x100, 27175},   // 1.333333
	{0x108, 28140},   // 1.347368
	{0x110, 29164},   // 1.361702
	{0x118, 30175},   // 1.376344
	{0x120, 31177},   // 1.391304
	{0x128, 32233},   // 1.406593
	{0x130, 33277},   // 1.422222
	{0x138, 34311},   // 1.438202
	{0x140, 35396},   // 1.454545
	{0x148, 36470},   // 1.471264
	{0x150, 37593},   // 1.488372
	{0x158, 38703},   // 1.505882
	{0x160, 39801},   // 1.523810
	{0x168, 40945},   // 1.542169
	{0x170, 42076},   // 1.560976
	{0x178, 43252},   // 1.580247
	{0x180, 44413},   // 1.600000
	{0x188, 45618},   // 1.620253
	{0x190, 46808},   // 1.641026
	{0x198, 48037},   // 1.662338
	{0x1a0, 49252},   // 1.684211
	{0x1a8, 50505},   // 1.706667
	{0x1b0, 51794},   // 1.729730
	{0x1b8, 53068},   // 1.753425
	{0x1c0, 54375},   // 1.777778
	{0x1c8, 55716},   // 1.802817
	{0x1d0, 57039},   // 1.828571
	{0x1d8, 58392},   // 1.855072
	{0x1e0, 59776},   // 1.882353
	{0x1e8, 61189},   // 1.910448
	{0x1f0, 62581},   // 1.939394
	{0x1f8, 64046},   // 1.969231
	{0x200, 65536},   // 2.000000
	{0x204, 66271},   // 2.015748
	{0x208, 67001},   // 2.031746
	{0x20c, 67769},   // 2.048000
	{0x210, 68534},   // 2.064516
	{0x214, 69290},   // 2.081301
	{0x218, 70042},   // 2.098361
	{0x21c, 70831},   // 2.115702
	{0x220, 71613},   // 2.133333
	{0x224, 72390},   // 2.151261
	{0x228, 73201},   // 2.169492
	{0x22c, 74007},   // 2.188034
	{0x230, 74805},   // 2.206897
	{0x234, 75639},   // 2.226087
	{0x238, 76466},   // 2.245614
	{0x23c, 77284},   // 2.265487
	{0x240, 78137},   // 2.285714
	{0x244, 78982},   // 2.306306
	{0x248, 79858},   // 2.327273
	{0x24c, 80688},   // 2.348624
	{0x250, 81588},   // 2.370370
	{0x254, 82441},   // 2.392523
	{0x258, 83364},   // 2.415094
	{0x25c, 84239},   // 2.438095
	{0x260, 85143},   // 2.461539
	{0x264, 86076},   // 2.485437
	{0x268, 87001},   // 2.509804
	{0x26c, 87916},   // 2.534653
	{0x270, 88859},   // 2.560000
	{0x274, 89792},   // 2.585859
	{0x278, 90752},   // 2.612245
	{0x27c, 91737},   // 2.639175
	{0x280, 92711},   // 2.666667
	{0x284, 93710},   // 2.694737
	{0x288, 94700},   // 2.723404
	{0x28c, 95711},   // 2.752688
	{0x290, 96745},   // 2.782609
	{0x294, 97769},   // 2.813187
	{0x298, 98813},   // 2.844445
	{0x29c, 99879},   // 2.876405
	{0x2a0, 100932},   // 2.909091
	{0x2a4, 102037},   // 2.942529
	{0x2a8, 103129},   // 2.976744
	{0x2ac, 104239},   // 3.011765
	{0x2b0, 105337},   // 3.047619
	{0x2b4, 106481},   // 3.084337
	{0x2b8, 107612},   // 3.121951
	{0x2bc, 108788},   // 3.160494
	{0x2c0, 109949},   // 3.200000
	{0x2c4, 111154},   // 3.240506
	{0x2c8, 112344},   // 3.282051
	{0x2cc, 113573},   // 3.324675
	{0x2d0, 114815},   // 3.368421
	{0x2d4, 116067},   // 3.413333
	{0x2d8, 117330},   // 3.459460
	{0x2dc, 118630},   // 3.506849
	{0x2e0, 119911},   // 3.555556
	{0x2e4, 121252},   // 3.605634
	{0x2e8, 122575},   // 3.657143
	{0x2ec, 123954},   // 3.710145
	{0x2f0, 125337},   // 3.764706
	{0x2f4, 126725},   // 3.820895
	{0x2f8, 128141},   // 3.878788
	{0x2fc, 129582},   // 3.938462
	{0x300, 131072},   // 4.000000
	{0x302, 131807},   // 4.031496
	{0x304, 132559},   // 4.063492
	{0x306, 133305},   // 4.096000
	{0x308, 134070},   // 4.129032
	{0x30a, 134826},   // 4.162601
	{0x30c, 135599},   // 4.196721
	{0x30e, 136367},   // 4.231405
	{0x310, 137171},   // 4.266667
	{0x312, 137947},   // 4.302521
	{0x314, 138760},   // 4.338983
	{0x316, 139564},   // 4.376069
	{0x318, 140363},   // 4.413793
	{0x31a, 141196},   // 4.452174
	{0x31c, 142022},   // 4.491228
	{0x31e, 142840},   // 4.530973
	{0x320, 143693},   // 4.571429
	{0x322, 144537},   // 4.612613
	{0x324, 145394},   // 4.654545
	{0x326, 146243},   // 4.697248
	{0x328, 147124},   // 4.740741
	{0x32a, 147996},   // 4.785047
	{0x32c, 148900},   // 4.830189
	{0x32e, 149793},   // 4.876191
	{0x330, 150698},   // 4.923077
	{0x332, 151612},   // 4.970874
	{0x334, 152537},   // 5.019608
	{0x336, 153452},   // 5.069307
	{0x338, 154395},   // 5.120000
	{0x33a, 155346},   // 5.171717
	{0x33c, 156306},   // 5.224490
	{0x33e, 157290},   // 5.278350
	{0x340, 158265},   // 5.333333
	{0x342, 159246},   // 5.389474
	{0x344, 160252},   // 5.446808
	{0x346, 161264},   // 5.505376
	{0x348, 162281},   // 5.565217
	{0x34a, 163321},   // 5.626374
	{0x34c, 164366},   // 5.688889
	{0x34e, 165415},   // 5.752809
	{0x350, 166484},   // 5.818182
	{0x352, 167573},   // 5.885057
	{0x354, 168665},   // 5.953488
	{0x356, 169775},   // 6.023530
	{0x358, 170888},   // 6.095238
	{0x35a, 172017},   // 6.168674
	{0x35c, 173162},   // 6.243902
	{0x35e, 174324},   // 6.320988
	{0x360, 175500},   // 6.400000
	{0x362, 176690},   // 6.481013
	{0x364, 177894},   // 6.564103
	{0x366, 179109},   // 6.649351
	{0x368, 180351},   // 6.736842
	{0x36a, 181603},   // 6.826667
	{0x36c, 182866},   // 6.918919
	{0x36e, 184166},   // 7.013699
	{0x370, 185460},   // 7.111111
	{0x372, 186788},   // 7.211267
	{0x374, 188123},   // 7.314286
	{0x376, 189490},   // 7.420290
	{0x378, 190873},   // 7.529412
	{0x37a, 192273},   // 7.641791
	{0x37c, 193688},   // 7.757576
	{0x37e, 195129},   // 7.876923
	{0x380, 196608},   // 8.000000
	{0x382, 198095},   // 8.126985
	{0x384, 199606},   // 8.258064
	{0x386, 201135},   // 8.393442
	{0x388, 202707},   // 8.533334
	{0x38a, 204296},   // 8.677966
	{0x38c, 205909},   // 8.827586
	{0x38e, 207558},   // 8.982456
	{0x390, 209229},   // 9.142858
	{0x392, 210930},   // 9.309091
	{0x394, 212670},   // 9.481482
	{0x396, 214436},   // 9.660378
	{0x398, 216234},   // 9.846154
	{0x39a, 218073},   // 10.039216
	{0x39c, 219940},   // 10.240000
	{0x39e, 221850},   // 10.448979
	{0x3a0, 223801},   // 10.666667
	{0x3a2, 225796},   // 10.893617
	{0x3a4, 227826},   // 11.130435
	{0x3a6, 229902},   // 11.377778
	{0x3a8, 232028},   // 11.636364
	{0x3aa, 234201},   // 11.906977
	{0x3ac, 236432},   // 12.190476
	{0x3ae, 238706},   // 12.487804
	{0x3b0, 241043},   // 12.800000
	{0x3b2, 243436},   // 13.128205
	{0x3b4, 245894},   // 13.473684
	{0x3b6, 248409},   // 13.837838
	{0x3b8, 251003},   // 14.222222
	{0x3ba, 253666},   // 14.628572
	{0x3bc, 256409},   // 15.058824
	{0x3be, 259230},   // 15.515152
	{0x3c0, 262144},   // 16.000000
	{0x3c1, 263631},   // 16.253969
	{0x3c2, 265142},   // 16.516129
	{0x3c3, 266678},   // 16.786884
	{0x3c4, 268243},   // 17.066668
	{0x3c5, 269832},   // 17.355932
	{0x3c6, 271445},   // 17.655172
	{0x3c7, 273094},   // 17.964912
	{0x3c8, 274765},   // 18.285715
	{0x3c9, 276471},   // 18.618181
	{0x3ca, 278206},   // 18.962963
	{0x3cb, 279972},   // 19.320755
	{0x3cc, 281770},   // 19.692308
	{0x3cd, 283609},   // 20.078432
	{0x3ce, 285481},   // 20.480000
	{0x3cf, 287391},   // 20.897959
	{0x3d0, 289341},   // 21.333334
	{0x3d1, 291332},   // 21.787233
	{0x3d2, 293366},   // 22.260870
	{0x3d3, 295442},   // 22.755556
	{0x3d4, 297567},   // 23.272728
	{0x3d5, 299741},   // 23.813953
	{0x3d6, 301968},   // 24.380953
	{0x3d7, 304245},   // 24.975609
	{0x3d8, 306579},   // 25.600000
	{0x3d9, 308972},   // 26.256411
	{0x3da, 311430},   // 26.947369
	{0x3db, 313949},   // 27.675676
	{0x3dc, 316542},   // 28.444445
	{0x3dd, 319205},   // 29.257143
	{0x3de, 321945},   // 30.117647
	{0x3df, 324769},   // 31.030304
	{0x3e0, 327680},   // 32.000000
};


struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
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

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
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

struct tx_isp_mipi_bus sensor_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 200,
	.lans = 2,
	.settle_time_apative_en = 0,
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

struct tx_isp_mipi_bus sensor_mipi_linear_720p = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 540,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 1280,
	.image_theight = 720,
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

struct tx_isp_mipi_bus sensor_mipi_dol = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.settle_time_apative_en = 0,
	.clk = 372,
	.lans = 2,
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
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_dvp_bus sensor_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x2009,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 372,
		.lans = 2,
		.settle_time_apative_en = 0,
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
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 327680,
	.max_again_short = 327680,
	//.min_integration_time = 1,
	//.min_integration_time_short = 1,
	//.max_integration_time_short = 0x82,
	//.min_integration_time_native = 1,
	//.max_integration_time_native = 2110,
	//.integration_time_limit = 2110,
	//.total_width = 0x898,
	//.total_height = 0x465 * 2,
	//.max_integration_time = 2110,
	.integration_time_apply_delay = 2,
	.expo_fs = 1,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_1920_1080_30fps_dvp[] = {

	{SENSOR_REG_DELAY, 10},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
#if 0
	//raw12
	{0x330c, 0x01},
	{0x3020, 0x01},
	{0x3021, 0x02},
	{0x3201, 0x65},
	{0x3200, 0x04},
	{0x3203, 0x98},
	{0x3202, 0x08},
	{0x3205, 0x08},
	{0x3204, 0x00},
	{0x3207, 0x43},
	{0x3206, 0x04},
	{0x3209, 0x08},
	{0x3208, 0x00},
	{0x320b, 0x87},
	{0x320a, 0x07},
	{0x3007, 0x00},
	{0x300a, 0x00},
	{0x330c, 0x01},
	{0x3300, 0x84},
	{0x3301, 0x01},
	{0x3302, 0x02},
	{0x3303, 0x05},
	{0x3309, 0x00},
	{0x3307, 0x02},
	{0x330b, 0x28},
	{0x3014, 0x00},
	{0x330f, 0x00},
	{0x310f, 0x00},
	{0x3986, 0x00},
	{0x3986, 0x00},
	{0x3900, 0x00},
	{0x3902, 0x21},
	{0x3901, 0x00},
	{0x3904, 0xf8},
	{0x3903, 0x11},
	{0x3906, 0xff},
	{0x3905, 0x1f},
	{0x3908, 0xff},
	{0x3907, 0x1f},
	{0x390a, 0x3c},
	{0x3909, 0x05},
	{0x390c, 0x30},
	{0x390b, 0x06},
	{0x390e, 0xe4},
	{0x390d, 0x11},
	{0x3910, 0xff},
	{0x390f, 0x1f},
	{0x3911, 0x01},
	{0x3917, 0x00},
	{0x3916, 0x00},
	{0x3919, 0x57},
	{0x3918, 0x01},
	{0x3913, 0x11},
	{0x3912, 0x00},
	{0x3915, 0x4c},
	{0x3914, 0x05},
	{0x391b, 0x00},
	{0x391a, 0x00},
	{0x391d, 0xf5},
	{0x391c, 0x11},
	{0x391f, 0xff},
	{0x391e, 0x1f},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0x00},
	{0x3922, 0x00},
	{0x3925, 0x41},
	{0x3924, 0x05},
	{0x394c, 0x00},
	{0x394e, 0x73},
	{0x394d, 0x00},
	{0x3950, 0x83},
	{0x394f, 0x00},
	{0x3952, 0x62},
	{0x3951, 0x00},
	{0x3954, 0x5b},
	{0x3953, 0x05},
	{0x3927, 0x00},
	{0x3926, 0x00},
	{0x3929, 0xe5},
	{0x3928, 0x00},
	{0x392b, 0xd9},
	{0x392a, 0x01},
	{0x392d, 0x1b},
	{0x392c, 0x05},
	{0x392f, 0xa2},
	{0x392e, 0x06},
	{0x3931, 0xe4},
	{0x3930, 0x11},
	{0x3933, 0xe4},
	{0x3932, 0x11},
	{0x3935, 0xe4},
	{0x3934, 0x11},
	{0x3937, 0xe4},
	{0x3936, 0x11},
	{0x3939, 0xe4},
	{0x3938, 0x11},
	{0x393b, 0xe4},
	{0x393a, 0x11},
	{0x3991, 0xc9},
	{0x3990, 0x01},
	{0x3993, 0x20},
	{0x3992, 0x05},
	{0x3995, 0x92},
	{0x3994, 0x06},
	{0x3997, 0xe9},
	{0x3996, 0x11},
	{0x393d, 0xa3},
	{0x393c, 0x00},
	{0x393f, 0xd9},
	{0x393e, 0x01},
	{0x3941, 0xef},
	{0x3940, 0x05},
	{0x3943, 0xa2},
	{0x3942, 0x06},
	{0x3945, 0x00},
	{0x3944, 0x00},
	{0x3947, 0x26},
	{0x3946, 0x01},
	{0x3949, 0x26},
	{0x3948, 0x01},
	{0x394b, 0xe9},
	{0x394a, 0x11},
	{0x395a, 0x00},
	{0x3959, 0x00},
	{0x395c, 0x09},
	{0x395b, 0x00},
	{0x395e, 0x19},
	{0x395d, 0x05},
	{0x3960, 0x82},
	{0x395f, 0x06},
	{0x3956, 0x09},
	{0x3955, 0x00},
	{0x3958, 0xe9},
	{0x3957, 0x11},
	{0x3962, 0x00},
	{0x3961, 0x00},
	{0x3964, 0x83},
	{0x3963, 0x00},
	{0x3966, 0x00},
	{0x3965, 0x00},
	{0x3968, 0x83},
	{0x3967, 0x00},
	{0x3989, 0x00},
	{0x3988, 0x00},
	{0x398b, 0xa3},
	{0x398a, 0x00},
	{0x398d, 0x00},
	{0x398c, 0x00},
	{0x398f, 0x83},
	{0x398e, 0x00},
	{0x396a, 0x46},
	{0x3969, 0x12},
	{0x396d, 0x60},
	{0x396c, 0x00},
	{0x396f, 0x60},
	{0x396e, 0x00},
	{0x3971, 0x60},
	{0x3970, 0x00},
	{0x3973, 0x60},
	{0x3972, 0x00},
	{0x3975, 0x60},
	{0x3974, 0x00},
	{0x3977, 0x60},
	{0x3976, 0x00},
	{0x3979, 0xa0},
	{0x3978, 0x01},
	{0x397b, 0xa0},
	{0x397a, 0x01},
	{0x397d, 0xa0},
	{0x397c, 0x01},
	{0x397f, 0xa0},
	{0x397e, 0x01},
	{0x3981, 0xa0},
	{0x3980, 0x01},
	{0x3983, 0xa0},
	{0x3982, 0x01},
	{0x3985, 0xa0},
	{0x3984, 0x05},
	{0x3c42, 0x03},
	{0x3012, 0x2c},
	{0x3205, 0x08},
	{0x3204, 0x00},
	{0x310f, 0x00},
	{0x3600, 0x63},
	{0x364e, 0x63},
	{0x369c, 0x63},

	{0x301f, 0x01},
	{0x3C20, 0x2C},
	{0x3C21, 0x6C},
	{0x3C22, 0xAC},
	{0x3C23, 0xEC},
	{0x3C24, 0x07},
	{0x3C25, 0x80},
	{0x3C26, 0x04},
	{0x3C27, 0x38},
	{0x3C28, 0x07},
	{0x3C29, 0x80},
	{0x3C2A, 0x04},
	{0x3C2B, 0x38},
	{0x3C2C, 0x07},
	{0x3C2D, 0x80},
	{0x3C2E, 0x04},
	{0x3C2F, 0x38},
	{0x3C30, 0x07},
	{0x3C31, 0x80},
	{0x3C32, 0x04},
	{0x3C33, 0x38},
	{0x300b, 0x00},
	{0x3c42, 0x03},
	{0x3f05, 0xf0},
	{0x3f04, 0xf0},
	{0x3f03, 0xf0},
	{0x3f02, 0xf0},
	{0x3f01, 0xf0},
	{0x3c70, 0x02},
	{0x3021, 0x01},
	{0x3a31, 0x39},
	{0x300b, 0x01},
	{0x330a, 0x01},
	{0x3006, 0x02},
	{0x3006, 0x00},
	{SENSOR_REG_DELAY, 50},
	{0x330a, 0x00},
	{0x3a07, 0x56},
	{0x3a35, 0x07},/
	{0x3707, 0x90},
	{0x300c, 0x01},
	{0x210b, 0x00},
	{0x3021, 0x00},
#else
	//raw10
	{0x300b, 0x01},
	{0x3006, 0x02},
	{SENSOR_REG_DELAY, 50},
	{0x330c, 0x01},
	{0x3020, 0x01},
	{0x3021, 0x02},
	{0x3201, 0x46},
	{0x3200, 0x05},/*25fps vts*/
	{0x3203, 0x98},
	{0x3202, 0x08},
	{0x3205, 0x04},
	{0x3204, 0x00},
	{0x3207, 0x43},
	{0x3206, 0x04},
	{0x3209, 0x08},
	{0x3208, 0x00},
	{0x320b, 0x87},
	{0x320a, 0x07},
	{0x3007, 0x00},
	{0x3007, 0x00},
	{0x300a, 0x01},
	{0x330c, 0x01},
	{0x3300, 0x37},
	{0x3301, 0x01},
	{0x3302, 0x01},
	{0x3303, 0x06},
	{0x3309, 0x01},
	{0x3307, 0x01},
	{0x330b, 0x0a},
	{0x3014, 0x00},
	{0x330f, 0x00},
	{0x310f, 0x00},
	{0x3986, 0x02},
	{0x3986, 0x02},
	{0x3900, 0x00},
	{0x3902, 0x09},
	{0x3901, 0x00},
	{0x3904, 0x3a},
	{0x3903, 0x04},
	{0x3906, 0xff},
	{0x3905, 0x1f},
	{0x3908, 0xff},
	{0x3907, 0x1f},
	{0x390a, 0x6b},
	{0x3909, 0x01},
	{0x390c, 0x7b},
	{0x390b, 0x01},
	{0x390e, 0x31},
	{0x390d, 0x04},
	{0x3910, 0xff},
	{0x390f, 0x1f},
	{0x3911, 0x01},
	{0x3917, 0x00},
	{0x3916, 0x00},
	{0x3919, 0x63},
	{0x3918, 0x01},
	{0x3913, 0x09},
	{0x3912, 0x00},
	{0x3915, 0x73},
	{0x3914, 0x01},
	{0x391b, 0x00},
	{0x391a, 0x00},
	{0x391d, 0x39},
	{0x391c, 0x04},
	{0x391f, 0xff},
	{0x391e, 0x1f},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0x00},
	{0x3922, 0x00},
	{0x3925, 0x6d},
	{0x3924, 0x01},
	{0x394c, 0x00},
	{0x394e, 0x3a},
	{0x394d, 0x00},
	{0x3950, 0x42},
	{0x394f, 0x00},
	{0x3952, 0x32},
	{0x3951, 0x00},
	{0x3954, 0x82},
	{0x3953, 0x01},
	{0x3927, 0x00},
	{0x3926, 0x00},
	{0x3929, 0x63},
	{0x3928, 0x00},
	{0x392b, 0xcf},
	{0x392a, 0x00},
	{0x392d, 0x63},
	{0x392c, 0x01},
	{0x392f, 0xbd},
	{0x392e, 0x01},
	{0x3931, 0x31},
	{0x3930, 0x04},
	{0x3933, 0x31},
	{0x3932, 0x04},
	{0x3935, 0x31},
	{0x3934, 0x04},
	{0x3937, 0x31},
	{0x3936, 0x04},
	{0x3939, 0x31},
	{0x3938, 0x04},
	{0x393b, 0x31},
	{0x393a, 0x04},
	{0x3991, 0xc6},
	{0x3990, 0x00},
	{0x3993, 0x65},
	{0x3992, 0x01},
	{0x3995, 0x94},
	{0x3994, 0x01},
	{0x3997, 0x33},
	{0x3996, 0x04},
	{0x393d, 0x3a},
	{0x393c, 0x00},
	{0x393f, 0xcf},
	{0x393e, 0x00},
	{0x3941, 0x73},
	{0x3940, 0x01},
	{0x3943, 0x9d},
	{0x3942, 0x01},
	{0x3945, 0x00},
	{0x3944, 0x00},
	{0x3947, 0x74},
	{0x3946, 0x00},
	{0x3949, 0x74},
	{0x3948, 0x00},
	{0x394b, 0x33},
	{0x394a, 0x04},
	{0x395a, 0x00},
	{0x3959, 0x00},
	{0x395c, 0x05},
	{0x395b, 0x00},
	{0x395e, 0x61},
	{0x395d, 0x01},
	{0x3960, 0x8c},
	{0x395f, 0x01},
	{0x3956, 0x05},
	{0x3955, 0x00},
	{0x3958, 0x33},
	{0x3957, 0x04},
	{0x3962, 0x00},
	{0x3961, 0x00},
	{0x3964, 0x42},
	{0x3963, 0x00},
	{0x3966, 0x00},
	{0x3965, 0x00},
	{0x3968, 0x3a},
	{0x3967, 0x00},
	{0x3989, 0x00},
	{0x3988, 0x00},
	{0x398b, 0x53},
	{0x398a, 0x00},
	{0x398d, 0x00},
	{0x398c, 0x00},
	{0x398f, 0x42},
	{0x398e, 0x00},
	{0x396a, 0x49},
	{0x3969, 0x04},
	{0x396d, 0x60},
	{0x396c, 0x00},
	{0x396f, 0x60},
	{0x396e, 0x00},
	{0x3971, 0x60},
	{0x3970, 0x00},
	{0x3973, 0x60},
	{0x3972, 0x00},
	{0x3975, 0x60},
	{0x3974, 0x00},
	{0x3977, 0x60},
	{0x3976, 0x00},
	{0x3979, 0xa0},
	{0x3978, 0x01},
	{0x397b, 0xa0},
	{0x397a, 0x01},
	{0x397d, 0xa0},
	{0x397c, 0x01},
	{0x397f, 0xa0},
	{0x397e, 0x01},
	{0x3981, 0xa0},
	{0x3980, 0x01},
	{0x3983, 0xa0},
	{0x3982, 0x01},
	{0x3985, 0xa0},
	{0x3984, 0x05},
	{0x3c42, 0x03},
	{0x3012, 0x2b},
	{0x3205, 0x08},
	{0x3204, 0x00},
	{0x310f, 0x00},
	{0x3600, 0x63},
	{0x364e, 0x63},
	{0x369c, 0x63},
	{0x3707, 0x90},
	{0x210b, 0x00},
	{0x3021, 0x00},
	{0x3a03, 0x3a},
	{0x3a05, 0x7a},
	{0x3a07, 0x56},
	{0x3a0a, 0x3a},
	{0x3a2a, 0x14},
	{0x3a35, 0x07},
	{0x3006, 0x00},

#endif
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi_dol[] = {
	{0x300b, 0x01},
	{0x3006, 0x02},
	{SENSOR_REG_DELAY, 150},
	{0x330c, 0x01},
	{0x3b00, 0x03},
	{0x3b01, 0xff},
	{0x3a00, 0x80},
	{0x3a05, 0x78},
	{0x3a35, 0x07},
	{0x3300, 0x6e},
	{0x3301, 0x01},
	{0x3302, 0x01},
	{0x3303, 0x05},
	{0x3309, 0x01},
	{0x3307, 0x01},
	{0x330b, 0x0a},
	{0x3021, 0x01},
	{0x3201, 0x46},
	{0x3200, 0x05},
	{0x3203, 0x98},
	{0x3202, 0x08},
	{0x3205, 0x00},
	{0x3204, 0x00},
	{0x3207, 0x3b},
	{0x3206, 0x04},
	{0x3209, 0x08},
	{0x3208, 0x00},
	{0x320b, 0x87},
	{0x320a, 0x07},
	{0x3007, 0x00},
	{0x300a, 0x01},
	{0x3014, 0x00},
	{0x330f, 0x00},
	{0x310f, 0x01},
	{0x3986, 0x02},
	{0x3986, 0x02},
	{0x3900, 0x00},
	{0x3902, 0x1b},
	{0x3901, 0x00},
	{0x3904, 0xdd},
	{0x3903, 0x06},
	{0x3906, 0xff},
	{0x3905, 0x1f},
	{0x3908, 0xff},
	{0x3907, 0x1f},
	{0x390a, 0x81},
	{0x3909, 0x02},
	{0x390c, 0xaa},
	{0x390b, 0x03},
	{0x390e, 0xcd},
	{0x390d, 0x06},
	{0x3910, 0xff},
	{0x390f, 0x1f},
	{0x3911, 0x01},
	{0x3917, 0x00},
	{0x3916, 0x00},
	{0x3919, 0x74},
	{0x3918, 0x02},
	{0x3913, 0x0e},
	{0x3912, 0x00},
	{0x3915, 0x8f},
	{0x3914, 0x02},
	{0x391b, 0x00},
	{0x391a, 0x00},
	{0x391d, 0xda},
	{0x391c, 0x06},
	{0x391f, 0xff},
	{0x391e, 0x1f},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0x00},
	{0x3922, 0x00},
	{0x3925, 0x86},
	{0x3924, 0x02},
	{0x394c, 0x00},
	{0x394e, 0x5f},
	{0x394d, 0x00},
	{0x3950, 0x6c},
	{0x394f, 0x00},
	{0x3952, 0x51},
	{0x3951, 0x00},
	{0x3954, 0x9a},
	{0x3953, 0x02},
	{0x3927, 0x00},
	{0x3926, 0x00},
	{0x3929, 0xcb},
	{0x3928, 0x00},
	{0x392b, 0x95},
	{0x392a, 0x01},
	{0x392d, 0x66},
	{0x392c, 0x02},
	{0x392f, 0xfb},
	{0x392e, 0x03},
	{0x3931, 0xcd},
	{0x3930, 0x06},
	{0x3933, 0xcd},
	{0x3932, 0x06},
	{0x3935, 0xcd},
	{0x3934, 0x06},
	{0x3937, 0xcd},
	{0x3936, 0x06},
	{0x3939, 0xcd},
	{0x3938, 0x06},
	{0x393b, 0xcd},
	{0x393a, 0x06},
	{0x3991, 0x88},
	{0x3990, 0x01},
	{0x3993, 0x6b},
	{0x3992, 0x02},
	{0x3995, 0xee},
	{0x3994, 0x03},
	{0x3997, 0xd1},
	{0x3996, 0x06},
	{0x393d, 0x95},
	{0x393c, 0x00},
	{0x393f, 0x95},
	{0x393e, 0x01},
	{0x3941, 0x81},
	{0x3940, 0x02},
	{0x3943, 0xfb},
	{0x3942, 0x03},
	{0x3945, 0x00},
	{0x3944, 0x00},
	{0x3947, 0x01},
	{0x3946, 0x01},
	{0x3949, 0x01},
	{0x3948, 0x01},
	{0x394b, 0xd1},
	{0x394a, 0x06},
	{0x395a, 0x00},
	{0x3959, 0x00},
	{0x395c, 0x07},
	{0x395b, 0x00},
	{0x395e, 0x64},
	{0x395d, 0x02},
	{0x3960, 0xe0},
	{0x395f, 0x03},
	{0x3956, 0x07},
	{0x3955, 0x00},
	{0x3958, 0xd1},
	{0x3957, 0x06},
	{0x3962, 0x00},
	{0x3961, 0x00},
	{0x3964, 0x6c},
	{0x3963, 0x00},
	{0x3966, 0x00},
	{0x3965, 0x00},
	{0x3968, 0x6c},
	{0x3967, 0x00},
	{0x3989, 0x00},
	{0x3988, 0x00},
	{0x398b, 0x87},
	{0x398a, 0x00},
	{0x398d, 0x00},
	{0x398c, 0x00},
	{0x398f, 0x6c},
	{0x398e, 0x00},
	{0x396a, 0x1e},
	{0x3969, 0x07},
	{0x396d, 0x60},
	{0x396c, 0x00},
	{0x396f, 0x60},
	{0x396e, 0x00},
	{0x3971, 0x60},
	{0x3970, 0x00},
	{0x3973, 0x60},
	{0x3972, 0x00},
	{0x3975, 0x60},
	{0x3974, 0x00},
	{0x3977, 0x60},
	{0x3976, 0x00},
	{0x3979, 0xa0},
	{0x3978, 0x01},
	{0x397b, 0xa0},
	{0x397a, 0x01},
	{0x397d, 0xa0},
	{0x397c, 0x01},
	{0x397f, 0xa0},
	{0x397e, 0x01},
	{0x3981, 0xa0},
	{0x3980, 0x01},
	{0x3983, 0xa0},
	{0x3982, 0x01},
	{0x3985, 0xa0},
	{0x3984, 0x05},
	{0x3c42, 0x03},
	{0x3012, 0x2b},
	{0x3600, 0x63},
	{0x364e, 0x63},
	{0x369c, 0x63},
	{0x3a00, 0x00},
	{0x3707, 0x90},
	{0x3709, 0x90},
	{0x370b, 0x90},
	{0x390c, 0x20},
	{0x210b, 0x00},
	{0x3021, 0x00},
	{0x3a03, 0x3a},
	{0x3a05, 0x7a},
	{0x3a07, 0x56},
	{0x3a0a, 0x3a},
	{0x3a2a, 0x14},
	{0x3a35, 0x07},
	{0x3006, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_90fps_mipi_linear_raw10[] = {
	{0x300b, 0x01},
	{0x3006, 0x02},
	{SENSOR_REG_DELAY, 50},
	{0x330c, 0x01},
	{0x3b00, 0x04},
	{0x3b01, 0xff},
	{0x330c, 0x01},
	{0x3300, 0xa5},
	{0x3301, 0x03},
	{0x3302, 0x00},
	{0x3303, 0x00},
	{0x3309, 0x01},
	{0x3307, 0x00},
	{0x330b, 0x0a},
	{0x3201, 0x7d},
	{0x3200, 0x04},
	{0x3203, 0x6a},
	{0x3202, 0x08},
	{0x3205, 0x00},
	{0x3204, 0x00},
	{0x3207, 0x3b},
	{0x3206, 0x04},
	{0x3209, 0x08},
	{0x3208, 0x00},
	{0x320b, 0x87},
	{0x320a, 0x07},
	{0x3007, 0x00},
	{0x3007, 0x00},
	{0x300a, 0x01},
	{0x3014, 0x00},
	{0x330f, 0x00},
	{0x310f, 0x00},
	{0x3986, 0x02},
	{0x3986, 0x02},
	{0x3900, 0x00},
	{0x3902, 0x13},
	{0x3901, 0x00},
	{0x3904, 0x75},
	{0x3903, 0x06},
	{0x3906, 0xff},
	{0x3905, 0x1f},
	{0x3908, 0xff},
	{0x3907, 0x1f},
	{0x390a, 0xb1},
	{0x3909, 0x02},
	{0x390c, 0x0e},
	{0x390b, 0x03},
	{0x390e, 0x5f},
	{0x390d, 0x06},
	{0x3910, 0xff},
	{0x390f, 0x1f},
	{0x3911, 0x01},
	{0x3917, 0x00},
	{0x3916, 0x00},
	{0x3919, 0x9f},
	{0x3918, 0x02},
	{0x3913, 0x13},
	{0x3912, 0x00},
	{0x3915, 0xc4},
	{0x3914, 0x02},
	{0x391b, 0x00},
	{0x391a, 0x00},
	{0x391d, 0x72},
	{0x391c, 0x06},
	{0x391f, 0xff},
	{0x391e, 0x1f},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0x00},
	{0x3922, 0x00},
	{0x3925, 0xb7},
	{0x3924, 0x02},
	{0x394c, 0x00},
	{0x394e, 0x82},
	{0x394d, 0x00},
	{0x3950, 0x94},
	{0x394f, 0x00},
	{0x3952, 0x6f},
	{0x3951, 0x00},
	{0x3954, 0xe7},
	{0x3953, 0x02},
	{0x3927, 0x00},
	{0x3926, 0x00},
	{0x3929, 0xde},
	{0x3928, 0x00},
	{0x392b, 0xcf},
	{0x392a, 0x01},
	{0x392d, 0x9f},
	{0x392c, 0x02},
	{0x392f, 0x8f},
	{0x392e, 0x03},
	{0x3931, 0x5f},
	{0x3930, 0x06},
	{0x3933, 0x5f},
	{0x3932, 0x06},
	{0x3935, 0x5f},
	{0x3934, 0x06},
	{0x3937, 0x5f},
	{0x3936, 0x06},
	{0x3939, 0x5f},
	{0x3938, 0x06},
	{0x393b, 0x5f},
	{0x393a, 0x06},
	{0x3991, 0xbc},
	{0x3990, 0x01},
	{0x3993, 0xa4},
	{0x3992, 0x02},
	{0x3995, 0x7d},
	{0x3994, 0x03},
	{0x3997, 0x65},
	{0x3996, 0x06},
	{0x393d, 0x82},
	{0x393c, 0x00},
	{0x393f, 0xcf},
	{0x393e, 0x01},
	{0x3941, 0xc4},
	{0x3940, 0x02},
	{0x3943, 0x8f},
	{0x3942, 0x03},
	{0x3945, 0x00},
	{0x3944, 0x00},
	{0x3947, 0x03},
	{0x3946, 0x01},
	{0x3949, 0x03},
	{0x3948, 0x01},
	{0x394b, 0x65},
	{0x394a, 0x06},
	{0x395a, 0x00},
	{0x3959, 0x00},
	{0x395c, 0x0a},
	{0x395b, 0x00},
	{0x395e, 0x9d},
	{0x395d, 0x02},
	{0x3960, 0x6a},
	{0x395f, 0x03},
	{0x3956, 0x0a},
	{0x3955, 0x00},
	{0x3958, 0x65},
	{0x3957, 0x06},
	{0x3962, 0x00},
	{0x3961, 0x00},
	{0x3964, 0x94},
	{0x3963, 0x00},
	{0x3966, 0x00},
	{0x3965, 0x00},
	{0x3968, 0x82},
	{0x3967, 0x00},
	{0x3989, 0x00},
	{0x3988, 0x00},
	{0x398b, 0xb9},
	{0x398a, 0x00},
	{0x398d, 0x00},
	{0x398c, 0x00},
	{0x398f, 0x94},
	{0x398e, 0x00},
	{0x396a, 0x97},
	{0x3969, 0x06},
	{0x396d, 0x90},
	{0x396c, 0x00},
	{0x396f, 0x90},
	{0x396e, 0x00},
	{0x3971, 0x90},
	{0x3970, 0x00},
	{0x3973, 0x90},
	{0x3972, 0x00},
	{0x3975, 0x90},
	{0x3974, 0x00},
	{0x3977, 0x90},
	{0x3976, 0x00},
	{0x3979, 0xa0},
	{0x3978, 0x01},
	{0x397b, 0xa0},
	{0x397a, 0x01},
	{0x397d, 0xa0},
	{0x397c, 0x01},
	{0x397f, 0xa0},
	{0x397e, 0x01},
	{0x3981, 0xa0},
	{0x3980, 0x01},
	{0x3983, 0xa0},
	{0x3982, 0x01},
	{0x3985, 0xa0},
	{0x3984, 0x05},
	{0x3c42, 0x03},
	{0x3012, 0x2b},
	{0x3600, 0x63},
	{0x364e, 0x63},
	{0x369c, 0x63},
	{0x3a00, 0x00},
	{0x210b, 0x00},
	{0x3021, 0x00},
	{0x3a03, 0x3a},
	{0x3a05, 0x7a},
	{0x3a07, 0x56},
	{0x3a0a, 0x3a},
	{0x3a2a, 0x54},
	{0x3a35, 0x07},
	{0x3006, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1280_720_120fps_mipi_linear_raw10[] = {
	{0x300b,0x01},
	{0x3006,0x02},
	{SENSOR_REG_DELAY,0x50},
	{0x330c,0x01},
	{0x3b00,0x04},
	{0x3b01,0xff},
	{0x330c,0x01},
	{0x3a00,0x80},
	{0x3300,0xa0},
	{0x3301,0x03},
	{0x3302,0x00},
	{0x3303,0x00},
	{0x3309,0x01},
	{0x3307,0x00},
	{0x330b,0x0a},
	{0x3021,0x02},

	{0x3201,0x20},
	{0x3200,0x03},
	{0x3203,0xca},
	{0x3202,0x08},

	{0x3205,0xb4},
	{0x3204,0x00},
	{0x3207,0x87},
	{0x3206,0x03},

	{0x3209,0x48},
	{0x3208,0x01},
	{0x320b,0x47},
	{0x320a,0x06},

	{0x3007,0x00},
	{0x3007,0x00},
	{0x300a,0x01},
	{0x3014,0x00},
	{0x330f,0x00},
	{0x310f,0x00},
	{0x3986,0x02},
	{0x3986,0x02},
	{0x3900,0x00},
	{0x3902,0x13},
	{0x3901,0x00},
	{0x3904,0x75},
	{0x3903,0x06},
	{0x3906,0xff},
	{0x3905,0x1f},
	{0x3908,0xff},
	{0x3907,0x1f},
	{0x390a,0xb1},
	{0x3909,0x02},
	{0x390c,0x0e},
	{0x390b,0x03},
	{0x390e,0x5f},
	{0x390d,0x06},
	{0x3910,0xff},
	{0x390f,0x1f},
	{0x3911,0x01},
	{0x3917,0x00},
	{0x3916,0x00},
	{0x3919,0x9f},
	{0x3918,0x02},
	{0x3913,0x13},
	{0x3912,0x00},
	{0x3915,0xc4},
	{0x3914,0x02},
	{0x391b,0x00},
	{0x391a,0x00},
	{0x391d,0x72},
	{0x391c,0x06},
	{0x391f,0xff},
	{0x391e,0x1f},
	{0x3921,0xff},
	{0x3920,0x1f},
	{0x3923,0x00},
	{0x3922,0x00},
	{0x3925,0xb7},
	{0x3924,0x02},
	{0x394c,0x00},
	{0x394e,0x82},
	{0x394d,0x00},
	{0x3950,0x94},
	{0x394f,0x00},
	{0x3952,0x6f},
	{0x3951,0x00},
	{0x3954,0xe7},
	{0x3953,0x02},
	{0x3927,0x00},
	{0x3926,0x00},
	{0x3929,0xde},
	{0x3928,0x00},
	{0x392b,0xcf},
	{0x392a,0x01},
	{0x392d,0x9f},
	{0x392c,0x02},
	{0x392f,0x8f},
	{0x392e,0x03},
	{0x3931,0x5f},
	{0x3930,0x06},
	{0x3933,0x5f},
	{0x3932,0x06},
	{0x3935,0x5f},
	{0x3934,0x06},
	{0x3937,0x5f},
	{0x3936,0x06},
	{0x3939,0x5f},
	{0x3938,0x06},
	{0x393b,0x5f},
	{0x393a,0x06},
	{0x3991,0xbc},
	{0x3990,0x01},
	{0x3993,0xa4},
	{0x3992,0x02},
	{0x3995,0x7d},
	{0x3994,0x03},
	{0x3997,0x65},
	{0x3996,0x06},
	{0x393d,0x82},
	{0x393c,0x00},
	{0x393f,0xcf},
	{0x393e,0x01},
	{0x3941,0xc4},
	{0x3940,0x02},
	{0x3943,0x8f},
	{0x3942,0x03},
	{0x3945,0x00},
	{0x3944,0x00},
	{0x3947,0x03},
	{0x3946,0x01},
	{0x3949,0x03},
	{0x3948,0x01},
	{0x394b,0x65},
	{0x394a,0x06},
	{0x395a,0x00},
	{0x3959,0x00},
	{0x395c,0x0a},
	{0x395b,0x00},
	{0x395e,0x9d},
	{0x395d,0x02},
	{0x3960,0x6a},
	{0x395f,0x03},
	{0x3956,0x0a},
	{0x3955,0x00},
	{0x3958,0x65},
	{0x3957,0x06},
	{0x3962,0x00},
	{0x3961,0x00},
	{0x3964,0x94},
	{0x3963,0x00},
	{0x3966,0x00},
	{0x3965,0x00},
	{0x3968,0x82},
	{0x3967,0x00},
	{0x3989,0x00},
	{0x3988,0x00},
	{0x398b,0xb9},
	{0x398a,0x00},
	{0x398d,0x00},
	{0x398c,0x00},
	{0x398f,0x94},
	{0x398e,0x00},
	{0x396a,0x97},
	{0x3969,0x06},
	{0x396d,0x60},
	{0x396c,0x00},
	{0x396f,0x60},
	{0x396e,0x00},
	{0x3971,0x60},
	{0x3970,0x00},
	{0x3973,0x60},
	{0x3972,0x00},
	{0x3975,0x60},
	{0x3974,0x00},
	{0x3977,0x60},
	{0x3976,0x00},
	{0x3979,0xa0},
	{0x3978,0x01},
	{0x397b,0xa0},
	{0x397a,0x01},
	{0x397d,0xa0},
	{0x397c,0x01},
	{0x397f,0xa0},
	{0x397e,0x01},
	{0x3981,0xa0},
	{0x3980,0x01},
	{0x3983,0xa0},
	{0x3982,0x01},
	{0x3985,0xa0},
	{0x3984,0x05},
	{0x3c42,0x03},
	{0x3012,0x2b},
	{0x3600,0x63},
	{0x364e,0x63},
	{0x369c,0x63},
	{0x3a00,0x00},
	{0x210b,0x00},
	{0x3021,0x00},
	{0x3a07,0x56},
	{0x3a05,0x78},
	{0x3a2a,0x14},
	{0x3a35,0x07},
	{0x3a19,0x08},
	{0x3a1a,0x08},
	{0x3006,0x00},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	//[0]
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_dvp,
	},
	//[1]
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
	//[2]
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi_dol,
	},
	//[3]
	{
		.width = 1920,
		.height = 1080,
		.fps = 90 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_90fps_mipi_linear_raw10,
	},
	//[4] 720p
	{
		.width = 1280,
		.height = 720,
		.fps = 120 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1280_720_120fps_mipi_linear_raw10,
	},
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[1];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_dvp[] = {
	{0x3006, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_dvp[] = {
	{0x3006, 0x02},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_on_mipi[] = {
	{0x3006, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x3006, 0x02},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,	unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

/*
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
*/
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

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
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
	unsigned int expo = 0;

	expo = value << 5; /*set low 5 decimal bit to 0*/
	ret = sensor_write(sd, 0x3100, (unsigned char)((expo >> 13) & 0xff));
	ret += sensor_write(sd, 0x3101, (unsigned char)((expo >>  5) & 0xff));
	ret += sensor_write(sd, 0x3102, (unsigned char)((expo >>  0) & 0x1f));
	ret += sensor_write(sd, 0x300c, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = 0;

	expo = value << 5; /*set low 5 decimal bit to 0*/
	ret += sensor_write(sd, 0x3103, (unsigned char)((expo >> 13) & 0xff));
	ret += sensor_write(sd, 0x3104, (unsigned char)((expo >> 5) & 0xff));
	ret += sensor_write(sd, 0x3105, (unsigned char)((expo >> 0) & 0x1f));
	ret += sensor_write(sd, 0x300c, 0x01);


	return 0;
}


static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	/*if (value >0x3e0)
	  {
	  ret =sensor_write(sd, 0x3707, 0x90);
	  ret +=sensor_write(sd, 0x300c, 0x01);
	  ret +=sensor_write(sd, 0x300c, 0x00);
	  }
	  else
	  {
	  ret =sensor_write(sd, 0x3707, 0x80);
	  ret +=sensor_write(sd, 0x300c, 0x01);
	  ret +=sensor_write(sd, 0x300c, 0x00);
	  }*/

	ret = sensor_write(sd, 0x310a, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3109, (unsigned char)(value >> 8 & 0x03));
	ret += sensor_write(sd, 0x300c, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x310c, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x310b, (unsigned char)(value >> 8 & 0x03));
	ret += sensor_write(sd, 0x300c, 0x01);

	return 0;
}
/*
static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}
 */

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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable) {
		return ISP_SUCCESS;
	} else {
		sensor_set_attr(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		sensor->priv = wsize;
	}

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_on_dvp);
			} else if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_on_mipi);
			} else {
				ISP_ERROR("Don't support this Sensor Data interface\n");
			}
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}
	} else {
		if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
		sensor->video.state = TX_ISP_MODULE_INIT;
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps = 0;
	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(info->default_boot) {
		case 0:
		case 1:
		case 2:
			max_fps = SENSOR_OUTPUT_MAX_FPS;
			sclk = SENSOR_SUPPORT_SCLK;
			break;
		case 3:
			max_fps = TX_SENSOR_MAX_FPS_90;
			sclk = SENSOR_SUPPORT_SCLK_90fps;
			break;
		case 4:
			max_fps = TX_SENSOR_MAX_FPS_120;
			sclk = SENSOR_SUPPORT_SCLK_120fps;
			break;
		default:
			ISP_WARNING("default boot select err!!!\n");
			break;
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret = sensor_read(sd, 0x3202, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x3203, &tmp);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}
	hts = (hts << 8) + tmp;

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x3201, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x3200, (unsigned char)(vts >> 8));
	ret += sensor_write(sd, 0x300c, 0x01);
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native =vts - 2;
	sensor->video.attr->integration_time_limit =vts - 2;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time =vts - 2;
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
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

struct clk *sclk;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
	        sensor_attr.again = 0;
                sensor_attr.integration_time = 0x800;
		memcpy((void*)(&(sensor_attr.dvp)),(void*)(&sensor_dvp),sizeof(sensor_dvp));
		break;
	case 1:
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_linear),sizeof(sensor_mipi_linear));
		sensor_attr.mipi.clk = 200;
		sensor_attr.integration_time_limit = 1348;
		sensor_attr.max_integration_time_native = 1348;
		sensor_attr.total_width = 2200;
		sensor_attr.total_height = 1350;
		sensor_attr.max_integration_time = 1348;
	        sensor_attr.again = 0;
                sensor_attr.integration_time = 0x800;
		break;
	case 2:
		wsize = &sensor_win_sizes[2];
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_dol),sizeof(sensor_mipi_dol));
		sensor_attr.mipi.clk = 372;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.wdr_cache = 1920 * 1080 * 2;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 134;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.integration_time_limit = 1348;
		sensor_attr.max_integration_time_native = 1348;
		sensor_attr.total_width = 2200;
		sensor_attr.total_height = 1350;
		sensor_attr.max_integration_time = 1348;
	        sensor_attr.again = 0;
                sensor_attr.integration_time = 0x800;
		break;
	case 3:
		wsize = &sensor_win_sizes[3];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_linear),sizeof(sensor_mipi_linear));
		sensor_attr.mipi.clk = 560;
		sensor_attr.integration_time_limit = 0x47d - 2;
		sensor_attr.max_integration_time_native = 0x47d - 2;
		sensor_attr.total_width = 0x86a;
		sensor_attr.total_height = 0x47d;
		sensor_attr.max_integration_time = 0x47d - 2;
	        sensor_attr.again = 0;
                sensor_attr.integration_time = 0x800;
		break;
	case 4:
		wsize = &sensor_win_sizes[4];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		memcpy((void*)(&(sensor_attr.mipi)), (void*)(&sensor_mipi_linear_720p), sizeof(sensor_mipi_linear_720p));
		sensor_attr.integration_time_limit = 0x320 - 2;
		sensor_attr.max_integration_time_native = 0x320 - 2;
		sensor_attr.total_width = 0x8ca;
		sensor_attr.total_height = 0x320;
		sensor_attr.max_integration_time = 0x320 - 2;
	        sensor_attr.again = 0;
                sensor_attr.integration_time = 0x800;
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
		break;
	}

	switch(info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 0;
			break;
		case TISP_SENSOR_VI_MIPI_CSI1:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 1;
			break;
		case TISP_SENSOR_VI_DVP:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
			break;
		default:
			ISP_ERROR("Have no this interface!!!\n");
	}

	switch(info->mclk) {
	case TISP_SENSOR_MCLK0:
		sclk = private_devm_clk_get(&client->dev, "mux_cim0");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sclk = private_devm_clk_get(&client->dev, "mux_cim1");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
		set_sensor_mclk_function(1);
		break;
	case TISP_SENSOR_MCLK2:
		sclk = private_devm_clk_get(&client->dev, "mux_cim2");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
		set_sensor_mclk_function(2);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock div_cim%d\n",info->mclk);
		goto err_get_mclk;
	}
	if (((rate / 1000) % 27000) != 0) {
		ret = clk_set_parent(sclk, clk_get(NULL, "epll"));
		if (ret != 0)
			pr_err("%s %d set parent clk to epll err!\n",__func__,__LINE__);
		sclk = private_devm_clk_get(&client->dev, "epll");
		if (IS_ERR(sclk)) {
			pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclk);
			if (((rate / 1000) % 27000) != 0) {
				private_clk_set_rate(sclk, 891000000);
			}
		}
	}
	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(40);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(40);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(40);
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

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;


	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
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
				//ret = sensor_get_black_pedestal(sd, sensor_val->value);
				break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_off_dvp);
			} else if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_off_mipi);

			} else {
				ISP_ERROR("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_on_dvp);
			} else if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_on_mipi);

			} else {
				ISP_ERROR("Don't support this Sensor Data interface\n");
				ret = -1;
			}
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg) {
				ret = sensor_set_fps(sd, sensor_val->value);
			}
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	//private_devm_clk_put(&client->dev, sensor->mclk);
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
