// SPDX-License-Identifier: GPL-2.0+
/*
 * AR1337.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 */

#include <sensor-common.h>
#include <sensor-info.h>
#include <tx-isp-common.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>

// ============================================================================
// SENSOR IDENTIFICATION
// ============================================================================
#define SENSOR_NAME "ar1337"
#define SENSOR_VERSION "H20211008a"
#define SENSOR_CHIP_ID 0x0253
#define SENSOR_CHIP_ID_H (0x02)
#define SENSOR_CHIP_ID_L (0x53)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x36

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_25FPS_SCLK (441567840)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5

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
    { 0x2010, 0 },
    { 0x2018, 38336 },
    { 0x2020, 65536 },
    { 0x2024, 86633 },
    { 0x2028, 103872 },
    { 0x202c, 118446 },
    { 0x2030, 131072 },
    { 0x2032, 142208 },
    { 0x2034, 152169 },
    { 0x2036, 161181 },
    { 0x2038, 169408 },
    { 0x203a, 176976 },
    { 0x203c, 183982 },
    { 0x203e, 190505 },
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain,
                                unsigned char shift,
                                unsigned int *sensor_again)
{
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain,
                                unsigned char shift,
                                unsigned int *sensor_dgain)
{
    return 0;
}

struct tx_isp_sensor_attribute sensor_attr = {
    .name = SENSOR_NAME,
    .chip_id = SENSOR_CHIP_ID,
    .cbus_type = SENSOR_BUS_TYPE,
    .cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
    .cbus_device = SENSOR_I2C_ADDRESS,
    .dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
    .mipi = {
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
    { SENSOR_REG_DELAY, 100 },
    { 0x0103, 0x01 },
    { SENSOR_REG_DELAY, 100 },
    { SENSOR_REG_DELAY, 100 },
    { 0x3042, 0x500C }, // DARK_CONTROL2
    { 0x3044, 0x0580 }, // DARK_CONTROL
    { 0x32BA, 0x0000 }, // PDAF_SC_VISUAL_TAG
    { 0x30EE, 0x5133 }, // DARK_CONTROL3
    { 0x30D2, 0x0000 }, // CRM_CONTROL
    { 0x3C50, 0x0001 }, // BPC_SETUP
    { 0x31E0, 0x0001 }, // PIX_DEF_ID
    { 0x3180, 0x9434 }, // FINEDIGCORR_CONTROL
    { 0x3F2C, 0x2028 }, // GTH_THRES_RTN
    { 0x3C58, 0x03FF }, // BPC_BLOOM_TH
    { 0x3C5A, 0x00FD }, // BPC_TAG_MODE
    { 0x3C5C, 0x000A }, // BPC_QR_SETUP
    { 0x3C5E, 0x000A }, // BPC_GP_NUM_OF
    { 0x3C60, 0x0500 }, // BPC_GP_SIGMA_SETUP
    { 0x3C62, 0xECA0 }, // BPC_PC_MASK
    { 0x3C64, 0x000F }, // BPC_PC_SIGMA
    { 0x3C66, 0x002A }, // BPC_NOISE_LIFT
    { 0x3C68, 0x0000 }, // BPC_BOISE_CEOF
    { 0x3C6A, 0x0006 }, // BPC_NOISE_FLOOR0
    { 0x3C6C, 0x000A }, // BPC_NOISE_FLOOR1
    { 0x3C6E, 0x000E }, // BPC_NOISE_FLOOR2
    { 0x3C70, 0x0012 }, // BPC_NOISE_FLOOR3
    { 0x3C72, 0x2010 }, // BPC_NOISE_GAIN_TH10
    { 0x3C74, 0x0040 }, // BPC_NOISE_GAIN_TH2
    { 0x3C78, 0x0000 }, // BPC_NOISE_ADD_GP
    { 0x3C7A, 0x0210 }, // BPC_DEFECT
    { 0x3C7C, 0x000A }, // BPC_SF_SIGMA
    { 0x3C7E, 0x20FF }, // BPC_DF_SIGMA10
    { 0x3C80, 0x0810 }, // BPC_DF_SIGMA32
    { 0x3C82, 0x20FF }, // BPC_DC_SIGMA10
    { 0x3C84, 0x0810 }, // BPC_DC_SIGMA32
    { 0x3C86, 0x4210 }, // BPC_DC_F
    { 0x3C88, 0x0000 }, // BPC_DEN_EN10
    { 0x3C8A, 0x0000 }, // BPC_DEN_EN32
    { 0x3C8C, 0x0010 }, // BPC_DEN_SIGMA
    { 0x3C8E, 0x03F5 }, // BPC_DEN_DIS_TH
    { 0x3C90, 0x0000 }, // BPC_TRI_ABYP_SIGMA
    { 0x3C94, 0x47E0 }, // BPC_PDAF_REC_SETUP1
    { 0x3C96, 0x6000 }, // BPC_PDAF_TAG_GRAD_EN
    { 0x3C98, 0x0000 }, // BPC_PDAF_REC_TOL_ABS0
    { 0x3C9A, 0x0000 }, // BPC_PDAF_REC_TOL_ABS1
    { 0x3C9C, 0x0000 }, // BPC_PDAF_REC_TOL_ABS2
    { 0x3C9E, 0x0000 }, // BPC_PDAF_REC_TOL_ABS3
    { 0x3CA0, 0x0000 }, // BPC_PDAF_REC_TOL_ABS4
    { 0x3CA2, 0x0000 }, // BPC_PDAF_REC_TOL_ABS5
    { 0x3CA4, 0x0000 }, // BPC_PDAF_REC_TOL_ABS6
    { 0x3CA6, 0x0000 }, // BPC_PDAF_REC_TOL_ABS7
    { 0x3CC0, 0x0000 }, // BPC_PDAF_REC_TOL_DARK
    { 0x3CC2, 0x03E8 }, // BPC_BM_T0
    { 0x3CC4, 0x07D0 }, // BPC_BM_T1
    { 0x3CC6, 0x0BB8 }, // BPC_BM_T2
    { 0x3222, 0x00C0 }, // PDAF_ROW_CONTROL
    { 0x322E, 0x0000 }, // PDAF_PEDESTAL
    { 0x3230, 0x0FF0 }, // PDAF_SAT_TH
    { 0x32BC, 0x0019 }, // PDAF_DC_TH_ABS
    { 0x32BE, 0x0013 }, // PDAF_DC_TH_REL
    { 0x32C0, 0x0010 }, // PDAF_DC_DIFF_FACTOR
    { 0x32BA, 0x0044 }, // PDAF recon improvement in binning
    { 0x3D00, 0x0446 },
    { 0x3D02, 0x4C66 },
    { 0x3D04, 0xFFFF },
    { 0x3D06, 0xFFFF },
    { 0x3D08, 0x5E40 },
    { 0x3D0A, 0x1106 },
    { 0x3D0C, 0x8041 },
    { 0x3D0E, 0x4F83 },
    { 0x3D10, 0x4200 },
    { 0x3D12, 0xC055 },
    { 0x3D14, 0x805B },
    { 0x3D16, 0x8360 },
    { 0x3D18, 0x845A },
    { 0x3D1A, 0x8D00 },
    { 0x3D1C, 0xC083 },
    { 0x3D1E, 0x4292 },
    { 0x3D20, 0x5A52 },
    { 0x3D22, 0x8453 },
    { 0x3D24, 0x6410 },
    { 0x3D26, 0x3080 },
    { 0x3D28, 0x1C00 },
    { 0x3D2A, 0xCE57 },
    { 0x3D2C, 0x568B },
    { 0x3D2E, 0x5150 },
    { 0x3D30, 0x804D },
    { 0x3D32, 0x4382 },
    { 0x3D34, 0x5280 },
    { 0x3D36, 0x5858 },
    { 0x3D38, 0xA443 },
    { 0x3D3A, 0x9A45 },
    { 0x3D3C, 0x9245 },
    { 0x3D3E, 0xA752 },
    { 0x3D40, 0x9451 },
    { 0x3D42, 0xE651 },
    { 0x3D44, 0x8610 },
    { 0x3D46, 0xC49C },
    { 0x3D48, 0x4F86 },
    { 0x3D4A, 0x5959 },
    { 0x3D4C, 0xE642 },
    { 0x3D4E, 0x9361 },
    { 0x3D50, 0x8262 },
    { 0x3D52, 0x8342 },
    { 0x3D54, 0x8141 },
    { 0x3D56, 0x64FF },
    { 0x3D58, 0xFFB7 },
    { 0x3D5A, 0x4081 },
    { 0x3D5C, 0x4080 },
    { 0x3D5E, 0x4180 },
    { 0x3D60, 0x4280 },
    { 0x3D62, 0x438D },
    { 0x3D64, 0x44BA },
    { 0x3D66, 0x4488 },
    { 0x3D68, 0x4380 },
    { 0x3D6A, 0x4241 },
    { 0x3D6C, 0x8140 },
    { 0x3D6E, 0x8240 },
    { 0x3D70, 0x8041 },
    { 0x3D72, 0x8042 },
    { 0x3D74, 0x8043 },
    { 0x3D76, 0x8D44 },
    { 0x3D78, 0xBA44 },
    { 0x3D7A, 0x875E },
    { 0x3D7C, 0x4354 },
    { 0x3D7E, 0x4241 },
    { 0x3D80, 0x8140 },
    { 0x3D82, 0x815B },
    { 0x3D84, 0x8160 },
    { 0x3D86, 0x2600 },
    { 0x3D88, 0x5580 },
    { 0x3D8A, 0x7000 },
    { 0x3D8C, 0x8040 },
    { 0x3D8E, 0x4C81 },
    { 0x3D90, 0x45C3 },
    { 0x3D92, 0x4581 },
    { 0x3D94, 0x4C40 },
    { 0x3D96, 0x8070 },
    { 0x3D98, 0x8040 },
    { 0x3D9A, 0x4C85 },
    { 0x3D9C, 0x6CA8 },
    { 0x3D9E, 0x6C8C },
    { 0x3DA0, 0x000E },
    { 0x3DA2, 0xBE44 },
    { 0x3DA4, 0x8844 },
    { 0x3DA6, 0xBC78 },
    { 0x3DA8, 0x0900 },
    { 0x3DAA, 0x8904 },
    { 0x3DAC, 0x8080 },
    { 0x3DAE, 0x0240 },
    { 0x3DB0, 0x8609 },
    { 0x3DB2, 0x008E },
    { 0x3DB4, 0x0900 },
    { 0x3DB6, 0x8002 },
    { 0x3DB8, 0x4080 },
    { 0x3DBA, 0x0480 },
    { 0x3DBC, 0x887C },
    { 0x3DBE, 0xAA86 },
    { 0x3DC0, 0x0900 },
    { 0x3DC2, 0x877A },
    { 0x3DC4, 0x000E },
    { 0x3DC6, 0xC379 },
    { 0x3DC8, 0x4C40 },
    { 0x3DCA, 0xBF70 },
    { 0x3DCC, 0x5E40 },
    { 0x3DCE, 0x114E },
    { 0x3DD0, 0x5D41 },
    { 0x3DD2, 0x5383 },
    { 0x3DD4, 0x4200 },
    { 0x3DD6, 0xC055 },
    { 0x3DD8, 0xA400 },
    { 0x3DDA, 0xC083 },
    { 0x3DDC, 0x4288 },
    { 0x3DDE, 0x6083 },
    { 0x3DE0, 0x5B80 },
    { 0x3DE2, 0x5A64 },
    { 0x3DE4, 0x1030 },
    { 0x3DE6, 0x801C },
    { 0x3DE8, 0x00A5 },
    { 0x3DEA, 0x5697 },
    { 0x3DEC, 0x57A5 },
    { 0x3DEE, 0x5180 },
    { 0x3DF0, 0x505A },
    { 0x3DF2, 0x814D },
    { 0x3DF4, 0x8358 },
    { 0x3DF6, 0x8058 },
    { 0x3DF8, 0xA943 },
    { 0x3DFA, 0x8345 },
    { 0x3DFC, 0xB045 },
    { 0x3DFE, 0x8343 },
    { 0x3E00, 0xA351 },
    { 0x3E02, 0xE251 },
    { 0x3E04, 0x8C59 },
    { 0x3E06, 0x8059 },
    { 0x3E08, 0x8A5F },
    { 0x3E0A, 0xEC7C },
    { 0x3E0C, 0xCC84 },
    { 0x3E0E, 0x6182 },
    { 0x3E10, 0x6283 },
    { 0x3E12, 0x4283 },
    { 0x3E14, 0x10CC },
    { 0x3E16, 0x6496 },
    { 0x3E18, 0x4281 },
    { 0x3E1A, 0x41BB },
    { 0x3E1C, 0x4082 },
    { 0x3E1E, 0x407E },
    { 0x3E20, 0xCC41 },
    { 0x3E22, 0x8042 },
    { 0x3E24, 0x8043 },
    { 0x3E26, 0x8300 },
    { 0x3E28, 0xC088 },
    { 0x3E2A, 0x44BA },
    { 0x3E2C, 0x4488 },
    { 0x3E2E, 0x00C8 },
    { 0x3E30, 0x8042 },
    { 0x3E32, 0x4181 },
    { 0x3E34, 0x4082 },
    { 0x3E36, 0x4080 },
    { 0x3E38, 0x4180 },
    { 0x3E3A, 0x4280 },
    { 0x3E3C, 0x4383 },
    { 0x3E3E, 0x00C0 },
    { 0x3E40, 0x8844 },
    { 0x3E42, 0xBA44 },
    { 0x3E44, 0x8800 },
    { 0x3E46, 0xC880 },
    { 0x3E48, 0x4241 },
    { 0x3E4A, 0x8240 },
    { 0x3E4C, 0x8140 },
    { 0x3E4E, 0x8041 },
    { 0x3E50, 0x8042 },
    { 0x3E52, 0x8043 },
    { 0x3E54, 0x8300 },
    { 0x3E56, 0xC088 },
    { 0x3E58, 0x44BA },
    { 0x3E5A, 0x4488 },
    { 0x3E5C, 0x00C8 },
    { 0x3E5E, 0x8042 },
    { 0x3E60, 0x4181 },
    { 0x3E62, 0x4082 },
    { 0x3E64, 0x4080 },
    { 0x3E66, 0x4180 },
    { 0x3E68, 0x4280 },
    { 0x3E6A, 0x4383 },
    { 0x3E6C, 0x00C0 },
    { 0x3E6E, 0x8844 },
    { 0x3E70, 0xBA44 },
    { 0x3E72, 0x8800 },
    { 0x3E74, 0xC880 },
    { 0x3E76, 0x4241 },
    { 0x3E78, 0x8140 },
    { 0x3E7A, 0x9F5E },
    { 0x3E7C, 0x8A54 },
    { 0x3E7E, 0x8620 },
    { 0x3E80, 0x2881 },
    { 0x3E82, 0x6026 },
    { 0x3E84, 0x8055 },
    { 0x3E86, 0x8070 },
    { 0x3E88, 0x0000 },
    { 0x3E8A, 0x0000 },
    { 0x3E8C, 0x0000 },
    { 0x3E8E, 0x0000 },
    { 0x3E90, 0x0000 },
    { 0x3E92, 0x0000 },
    { 0x3E94, 0x0000 },
    { 0x3E96, 0x0000 },
    { 0x3E98, 0x0000 },
    { 0x3E9A, 0x0000 },
    { 0x3E9C, 0x0000 },
    { 0x3E9E, 0x0000 },
    { 0x3EA0, 0x0000 },
    { 0x3EA2, 0x0000 },
    { 0x3EA4, 0x0000 },
    { 0x3EA6, 0x0000 },
    { 0x3EA8, 0x0000 },
    { 0x3EAA, 0x0000 },
    { 0x3EAC, 0x0000 },
    { 0x3EAE, 0x0000 },
    { 0x3EB0, 0x0000 },
    { 0x3EB2, 0x0000 },
    { 0x3EB4, 0x0000 },
    { 0x3EB6, 0x004D }, // DAC_LD_0_1
    { 0x3EBA, 0x1DAB }, // DAC_LD_4_5
    { 0x3EBC, 0xAA06 }, // DAC_LD_6_7
    { 0x3EC0, 0x1300 }, // DAC_LD_10_11
    { 0x3EC2, 0x7000 }, // DAC_LD_12_13
    { 0x3EC4, 0x1C08 }, // DAC_LD_14_15
    { 0x3EC6, 0xE244 }, // DAC_LD_16_17
    { 0x3EC8, 0x0F0F }, // DAC_LD_18_19
    { 0x3ECA, 0x0F4A }, // DAC_LD_20_21
    { 0x3ECC, 0x0706 }, // DAC_LD_22_23
    { 0x3ECE, 0x443B }, // DAC_LD_24_25
    { 0x3ED0, 0x12F0 }, // DAC_LD_26_27
    { 0x3ED2, 0x0039 }, // DAC_LD_28_29
    { 0x3ED4, 0x862F }, // DAC_LD_30_31
    { 0x3ED6, 0x4880 }, // DAC_LD_32_33
    { 0x3ED8, 0x0423 }, // DAC_LD_34_35
    { 0x3EDA, 0xF882 }, // DAC_LD_36_37
    { 0x3EDC, 0x8282 }, // DAC_LD_38_39
    { 0x3EDE, 0x8205 }, // DAC_LD_40_41
    { 0x316A, 0x8200 }, // DAC_RSTLO
    { 0x316C, 0x8200 }, // DAC_TXLO
    { 0x316E, 0x8200 }, // DAC_ECL
    { 0x3EF0, 0x5165 }, // DAC_LD_ECL
    { 0x3EF2, 0x0101 }, // DAC_LD_FSC
    { 0x3EF6, 0x030A }, // DAC_LD_RSTD
    { 0x3EFA, 0x0F0F }, // DAC_LD_TXLO
    { 0x3EFC, 0x070F }, // DAC_LD_TXLO1
    { 0x3EFE, 0x0F0F }, // DAC_LD_TXLO2
    { 0x31B0, 0x0060 }, // FRAME_PREAMBLE
    { 0x31B2, 0x002E }, // LINE_PREAMBLE
    { 0x31B4, 0x33D4 }, // MIPI_TIMING_0
    { 0x31B6, 0x244B }, // MIPI_TIMING_1
    { 0x31B8, 0x2413 }, // MIPI_TIMING_2
    { 0x31BA, 0x2070 }, // MIPI_TIMING_3
    { 0x31BC, 0x870B }, // MIPI_TIMING_4
    { 0x0300, 0x0005 }, // VT_PIX_CLK_DIV
    { 0x0302, 0x0001 }, // VT_SYS_CLK_DIV
    { 0x0304, 0x0101 }, // PRE_PLL_CLK_DIV
    { 0x0306, 0x2E2E }, // PLL_MULTIPLIER
    { 0x0308, 0x000A }, // OP_PIX_CLK_DIV
    { 0x030A, 0x0001 }, // OP_SYS_CLK_DIV
    { 0x0112, 0x0A0A }, // CCP_DATA_FORMAT
    { 0x3016, 0x0101 }, // ROW_SPEED
    { 0x31AE, 0x0202 }, //
    { 0x0344, 0x00C0 }, // X_ADDR_START
    { 0x0348, 0x0FBF }, // X_ADDR_END {0x0346, 0x01E8}, // Y_ADDR_START
    { 0x034A, 0x0A55 }, // Y_ADDR_END {0x034C, 0x0780}, // X_OUTPUT_SIZE
    { 0x034E, 0x0438 }, // Y_OUTPUT_SIZE
    { 0x3040, 0x0043 }, // READ_MODE
    { 0x3172, 0x0206 }, // ANALOG_CONTROL2
    { 0x317A, 0x516E }, // ANALOG_CONTROL6
    { 0x3F3C, 0x0003 }, // ANALOG_CONTROL9
    { 0x0400, 0x1 },    //Scaling Enabling: 0= disable, = x-dir
    { 0x0404, 0x20 },   //Scale_M = 32
    { 0x32C8, 0x030C }, // PDAF_SEQ_START
    { 0x32CA, 0x08A6 }, // PDAF_ODP_LLENGTH
    { 0x0342, 0x22F4 }, // LINE_LENGTH_PCK
    { 0x0340, 0x066D }, // FRAME_LENGTH_LINES
    { 0x0202, 0x066C }, // COARSE_INTEGRATION_TIME
    { 0x30EC, 0xFB08 }, // CTX_RD_DATA
    { 0x31D6, 0x336B }, //MIPI_JPEG_PN9_DATA_TYPE
    { 0x32C2, 0x03FC }, //pdaf_dma_start=PDAF_ZONE_PER_LINE*(PDAF_NUMBER_OF_CC + 1)*4 = 1020 = 0x03FC
    { 0x32C4, 0x0F30 }, // PDAF_DMA_SIZE
    { 0x32C6, 0x0A00 }, // PDAF_DMA_Y
    { 0x32C8, 0x0342 }, // PDAF_SEQ_START
    { 0x32D0, 0x0001 }, // PE_PARAM_ADDR
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x034E, 0x0442 }, // Y_OUTPUT_SIZE
    { 0x32D0, 0x6000 }, // PE_PARAM_ADDR
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x00B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x189E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x189F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A48 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0E }, // PE_PARAM_VALUE
    { 0x32D4, 0x188A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A48 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0806 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2828 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFFF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7FFF }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x009E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x189E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x189F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A48 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0E }, // PE_PARAM_VALUE
    { 0x32D4, 0x188A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A48 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0806 }, // PE_PARAM_VALUE
    { 0x32D4, 0x188A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A48 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0806 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D18 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D98 }, // PE_PARAM_VALUE
    { 0x32D4, 0x401E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0002 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2818 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x11B1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001F }, // PE_PARAM_VALUE
    { 0x32D4, 0x16AC }, // PE_PARAM_VALUE
    { 0x32D4, 0x1010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB6AC }, // PE_PARAM_VALUE
    { 0x32D4, 0x11B1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0B44 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000C }, // PE_PARAM_VALUE
    { 0x32D4, 0x000C }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010D }, // PE_PARAM_VALUE
    { 0x32D4, 0x020C }, // PE_PARAM_VALUE
    { 0x32D4, 0x09B4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000C }, // PE_PARAM_VALUE
    { 0x32D4, 0xB224 }, // PE_PARAM_VALUE
    { 0x32D4, 0x034D }, // PE_PARAM_VALUE
    { 0x32D4, 0x021C }, // PE_PARAM_VALUE
    { 0x32D4, 0xBA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0D }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x09BC }, // PE_PARAM_VALUE
    { 0x32D4, 0x0150 }, // PE_PARAM_VALUE
    { 0x32D4, 0x081A }, // PE_PARAM_VALUE
    { 0x32D4, 0x09B4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01AE }, // PE_PARAM_VALUE
    { 0x32D4, 0x001A }, // PE_PARAM_VALUE
    { 0x32D4, 0x16B4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01B2 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D08 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2828 }, // PE_PARAM_VALUE
    { 0x32D4, 0x019E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x11B1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0082 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x018A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0xA082 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2238 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2438 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2638 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2820 }, // PE_PARAM_VALUE
    { 0x32D4, 0x00BE }, // PE_PARAM_VALUE
    { 0x32D4, 0x0924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x003F }, // PE_PARAM_VALUE
    { 0x32D4, 0x00A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x00AE }, // PE_PARAM_VALUE
    { 0x32D4, 0xA130 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x002C }, // PE_PARAM_VALUE
    { 0x32D4, 0xA0B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x002E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA130 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0xC000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2830 }, // PE_PARAM_VALUE
    { 0x32D4, 0x020D }, // PE_PARAM_VALUE
    { 0x32D4, 0xA1C0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x032E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA1B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x120F }, // PE_PARAM_VALUE
    { 0x32D4, 0x00AE }, // PE_PARAM_VALUE
    { 0x32D4, 0xA6B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x120F }, // PE_PARAM_VALUE
    { 0x32D4, 0x003C }, // PE_PARAM_VALUE
    { 0x32D4, 0x00B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x041A }, // PE_PARAM_VALUE
    { 0x32D4, 0x006E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA1B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D98 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D18 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2828 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB36A }, // PE_PARAM_VALUE
    { 0x32D4, 0x03A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0F6A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0200 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0C1E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x11B1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2230 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2438 }, // PE_PARAM_VALUE
    { 0x32D4, 0x019E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x819E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA495 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x819F }, // PE_PARAM_VALUE
    { 0x32D4, 0xA495 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1C1D }, // PE_PARAM_VALUE
    { 0x32D4, 0x819E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0495 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1C1C }, // PE_PARAM_VALUE
    { 0x32D4, 0x819E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0495 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1C1C }, // PE_PARAM_VALUE
    { 0x32D4, 0x81DE }, // PE_PARAM_VALUE
    { 0x32D4, 0x0495 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1DBC }, // PE_PARAM_VALUE
    { 0x32D4, 0x81FE }, // PE_PARAM_VALUE
    { 0x32D4, 0xA495 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBB34 }, // PE_PARAM_VALUE
    { 0x32D4, 0x03AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBB54 }, // PE_PARAM_VALUE
    { 0x32D4, 0x020F }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFF0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3040 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4007 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2818 }, // PE_PARAM_VALUE
    { 0x32D4, 0x003E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAD8A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x003F }, // PE_PARAM_VALUE
    { 0x32D4, 0xAD8A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x002E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAD8A }, // PE_PARAM_VALUE
    { 0x32D4, 0x160F }, // PE_PARAM_VALUE
    { 0x32D4, 0x002E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAD8A }, // PE_PARAM_VALUE
    { 0x32D4, 0x160F }, // PE_PARAM_VALUE
    { 0x32D4, 0x203E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAD8A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2820 }, // PE_PARAM_VALUE
    { 0x32D4, 0x00AE }, // PE_PARAM_VALUE
    { 0x32D4, 0xAE0A }, // PE_PARAM_VALUE
    { 0x32D4, 0x1811 }, // PE_PARAM_VALUE
    { 0x32D4, 0x002F }, // PE_PARAM_VALUE
    { 0x32D4, 0xAE1A }, // PE_PARAM_VALUE
    { 0x32D4, 0x1411 }, // PE_PARAM_VALUE
    { 0x32D4, 0x003C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0F0A }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0x202A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0A }, // PE_PARAM_VALUE
    { 0x32D4, 0x1818 }, // PE_PARAM_VALUE
    { 0x32D4, 0x002A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0C }, // PE_PARAM_VALUE
    { 0x32D4, 0x1402 }, // PE_PARAM_VALUE
    { 0x32D4, 0x002A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0C }, // PE_PARAM_VALUE
    { 0x32D4, 0x15BE }, // PE_PARAM_VALUE
    { 0x32D4, 0x00AA }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0C }, // PE_PARAM_VALUE
    { 0x32D4, 0x15BE }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAE1A }, // PE_PARAM_VALUE
    { 0x32D4, 0x11B1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x003E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA93C }, // PE_PARAM_VALUE
    { 0x32D4, 0x11B1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7052 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xABA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFBE }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFA7 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA120 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x02A0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A2 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA2A0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x00E3 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x011E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF9C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA120 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x02A0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A2 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA2A0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x00E3 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x011E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA920 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA922 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FBA }, // PE_PARAM_VALUE
    { 0x32D4, 0x0006 }, // PE_PARAM_VALUE
    { 0x32D4, 0xA024 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0200 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA124 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0BAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA124 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x805E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x011E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF88 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x011E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF83 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x3052 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x009E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2DD0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x8000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2820 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x019E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x04C8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x019E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x801F }, // PE_PARAM_VALUE
    { 0x32D4, 0xA4C8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0018 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0946 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0x818A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0936 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0200 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x04C8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FBA }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0002 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2820 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0003 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x03FC }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x011E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0011 }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x011E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x8006 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0DB7 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0014 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0DC8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E12 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0040 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0DC8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FB2 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1248 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E14 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0050 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1248 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0DB4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1248 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0C14 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0050 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1248 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0DB4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0016 }, // PE_PARAM_VALUE
    { 0x32D4, 0xB248 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0C0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0056 }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0DAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x30F4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAABA }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAEA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7009 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2428 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2228 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFCA }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2620 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADB8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FA1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E01 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7009 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7002 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x004E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0201 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFCF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3040 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFFE }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFFF }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0DAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001C }, // PE_PARAM_VALUE
    { 0x32D4, 0xADA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0C0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x085E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0DAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0003 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0040 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF30 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF2E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF2C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF2A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7006 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAABA }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAEA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA93A }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA93A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA93A }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA93A }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3019 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3026 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7017 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FA1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E01 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E01 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FA1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7004 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7009 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2420 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF3E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2220 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7017 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7018 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2628 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA0B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2630 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA0B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x09A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7017 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7009 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7004 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7005 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x004E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0201 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFB0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3040 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7005 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B6 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B6 }, // PE_PARAM_VALUE
    { 0x32D4, 0x020F }, // PE_PARAM_VALUE
    { 0x32D4, 0x7051 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3019 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3026 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3033 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x303F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADB6 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB236 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB6B6 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA936 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA936 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA936 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0800 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2228 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2420 }, // PE_PARAM_VALUE
    { 0x32D4, 0x009E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB248 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x009E }, // PE_PARAM_VALUE
    { 0x32D4, 0x10C8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0E }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB0C8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x010A }, // PE_PARAM_VALUE
    { 0x32D4, 0xB242 }, // PE_PARAM_VALUE
    { 0x32D4, 0x040F }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0x12C2 }, // PE_PARAM_VALUE
    { 0x32D4, 0x05A0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0x12CA }, // PE_PARAM_VALUE
    { 0x32D4, 0x05A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0x12CA }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A0A }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2CA }, // PE_PARAM_VALUE
    { 0x32D4, 0x17B7 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0x12CA }, // PE_PARAM_VALUE
    { 0x32D4, 0x1616 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0x12CA }, // PE_PARAM_VALUE
    { 0x32D4, 0x1616 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000A }, // PE_PARAM_VALUE
    { 0x32D4, 0x124A }, // PE_PARAM_VALUE
    { 0x32D4, 0x1616 }, // PE_PARAM_VALUE
    { 0x32D4, 0x006A }, // PE_PARAM_VALUE
    { 0x32D4, 0xB24A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0017 }, // PE_PARAM_VALUE
    { 0x32D4, 0x004A }, // PE_PARAM_VALUE
    { 0x32D4, 0xB24A }, // PE_PARAM_VALUE
    { 0x32D4, 0x04B7 }, // PE_PARAM_VALUE
    { 0x32D4, 0x802A }, // PE_PARAM_VALUE
    { 0x32D4, 0xB24A }, // PE_PARAM_VALUE
    { 0x32D4, 0x05B7 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7004 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFE77 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7050 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2638 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9C8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x03A2 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x89A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA0B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x002A }, // PE_PARAM_VALUE
    { 0x32D4, 0xB24A }, // PE_PARAM_VALUE
    { 0x32D4, 0x01B7 }, // PE_PARAM_VALUE
    { 0x32D4, 0x002A }, // PE_PARAM_VALUE
    { 0x32D4, 0xB24A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0017 }, // PE_PARAM_VALUE
    { 0x32D4, 0x002A }, // PE_PARAM_VALUE
    { 0x32D4, 0xB24A }, // PE_PARAM_VALUE
    { 0x32D4, 0x04B7 }, // PE_PARAM_VALUE
    { 0x32D4, 0x802A }, // PE_PARAM_VALUE
    { 0x32D4, 0xB24A }, // PE_PARAM_VALUE
    { 0x32D4, 0x05B7 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7004 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFE5C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2638 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9C8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x03A2 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x89A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x011E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x811E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB949 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x609E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x189E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D18 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBA28 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBAA8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBAA8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBAA8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7051 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7051 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x03A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0201 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFA3 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3040 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x03A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7005 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFE2F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB224 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB224 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB224 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFE26 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB224 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB224 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB224 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1201 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7005 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7004 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7006 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x12A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A0E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0118 }, // PE_PARAM_VALUE
    { 0x32D4, 0x12A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0BA0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0018 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0524 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0BB2 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4018 }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0013 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2400 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFDFD }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7011 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x045E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADB8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x13A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADB8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x13A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAB24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAB24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFDF5 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2400 }, // PE_PARAM_VALUE
    { 0x32D4, 0x105E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADB8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x13A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADB8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x13A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2CA }, // PE_PARAM_VALUE
    { 0x32D4, 0x03A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBB4A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0201 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBB6C }, // PE_PARAM_VALUE
    { 0x32D4, 0x021A }, // PE_PARAM_VALUE
    { 0x32D4, 0x003A }, // PE_PARAM_VALUE
    { 0x32D4, 0x12EC }, // PE_PARAM_VALUE
    { 0x32D4, 0x03B6 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1076 }, // PE_PARAM_VALUE
    { 0x32D4, 0xB26C }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A1A }, // PE_PARAM_VALUE
    { 0x32D4, 0x003A }, // PE_PARAM_VALUE
    { 0x32D4, 0x12CA }, // PE_PARAM_VALUE
    { 0x32D4, 0x03B2 }, // PE_PARAM_VALUE
    { 0x32D4, 0x047E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB26C }, // PE_PARAM_VALUE
    { 0x32D4, 0x03A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBE5E }, // PE_PARAM_VALUE
    { 0x32D4, 0x05A5 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x1E5E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0404 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0002 }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x060F }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFDEF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x201E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x05AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x00FF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x09AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x080F }, // PE_PARAM_VALUE
    { 0x32D4, 0x7010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB2A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x09AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3019 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3026 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3033 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2AC0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x303F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xADB6 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB236 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB6B6 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA936 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA936 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0E0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA936 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7005 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B6 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B6 }, // PE_PARAM_VALUE
    { 0x32D4, 0x020F }, // PE_PARAM_VALUE
    { 0x32D4, 0x7051 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3018 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7003 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAAA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAEA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2428 }, // PE_PARAM_VALUE
    { 0x32D4, 0x009E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA8A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x201E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2430 }, // PE_PARAM_VALUE
    { 0x32D4, 0x009E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA8A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x081E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA0B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7004 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA0B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2400 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2600 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFDE8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2428 }, // PE_PARAM_VALUE
    { 0x32D4, 0x009E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA8A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x201E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2430 }, // PE_PARAM_VALUE
    { 0x32D4, 0x009E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA8A4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x081E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7010 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA0B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700F }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7004 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7009 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA0B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2400 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2600 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFDC4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x704D }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x011E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x811E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB949 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x609E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x189E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x704B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2D18 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB948 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xB9A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBA28 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBAA8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBAA8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000F }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xBAA8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7009 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7004 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7051 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7051 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x03A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0201 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFF92 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3040 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x03A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3018 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x061E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAAA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAEA4 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA24 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7009 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x0006 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A80 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0002 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0C1E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A3 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA0B0 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x1A0F }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA1A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x120F }, // PE_PARAM_VALUE
    { 0x32D4, 0x200E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA1A8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x03AF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0xFD66 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x4000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2620 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7009 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x601E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x7008 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0x0A38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x007E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0014 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x005E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x700A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x010E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x801E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA925 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x700A }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A40 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7002 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2418 }, // PE_PARAM_VALUE
    { 0x32D4, 0x008E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x181E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA924 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xAA38 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x004E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0201 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFFC9 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3040 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA9B8 }, // PE_PARAM_VALUE
    { 0x32D4, 0x01A1 }, // PE_PARAM_VALUE
    { 0x32D4, 0x00E7 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2A00 }, // PE_PARAM_VALUE
    { 0x32D4, 0x43FC }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2618 }, // PE_PARAM_VALUE
    { 0x32D4, 0xFDA3 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x3600 }, // PE_PARAM_VALUE
    { 0x32D4, 0x7018 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2218 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x2000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000E }, // PE_PARAM_VALUE
    { 0x32D4, 0xA030 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0FAF }, // PE_PARAM_VALUE
    { 0x32D0, 0x3000 }, // PE_PARAM_ADDR
    { 0x32D4, 0x005C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0066 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000B }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0050 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0068 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x000C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x001C }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0001 }, // PE_PARAM_VALUE
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x3220, 0x0C13 }, // PDAF_CONTROL
    { 0x30EC, 0xFB08 }, // CTX_RD_DATA
    { 0x31D6, 0x336B }, // MIPI_JPEG_PN9_DATA_TYPE
    { 0x32C2, 0x03FC }, // PDAF_DMA_START
    { 0x32C4, 0x0F30 }, // PDAF_DMA_SIZE
    { 0x32C6, 0x0F00 }, // PDAF_DMA_Y
    { 0x32C8, 0x0A00 }, // PDAF_SEQ_START
    { 0x32D0, 0x0001 }, // PE_PARAM_ADDR
    { 0x32D4, 0x0000 }, // PE_PARAM_VALUE
    { 0x034E, 0x0C3F }, // Y_OUTPUT_SIZE
    { 0x32C4, 0x03C0 }, // PDAF_DMA_SIZE
    { 0x32CA, 0x08A6 }, // PDAF_ODP_LLENGTH
    { 0x32C8, 0x030C }, // PDAF_SEQ_START
    { 0x301A, 0x021C }, // SENSOR_REGISTER
    { 0x31D6, 0x332B }, // MIPI_JPEG_PN9_DATA_TYPE
    { SENSOR_REG_END, 0x00 },
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
    //	{0x301A, 0x021c},
    { SENSOR_REG_END, 0x00 },
};

static struct regval_list sensor_stream_off_mipi[] = {
    //	{0x301A, 0x0218},
    { SENSOR_REG_END, 0x00 },
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    unsigned char buf[2] = { reg >> 8, reg & 0xff };
    struct i2c_msg msg[2] = {
                [0] ={
                        .addr = client->addr,
                        .flags = 0,
                        .len = 2,
                        .buf = buf,
                },
                [1] ={
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, uint16_t value)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    uint8_t buf[4] = { (reg >> 8) & 0xff, reg & 0xff, (value >> 8) & 0xff, value & 0xff };
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

static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
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
#if 0
            if (vals->reg_num == 0x34c || vals->reg_num == 0x34e) {
                printk(" write reg_num = 0x%x \nv[0] = 0x%x ====== v[1] = 0x%x \n", vals->reg_num, v[0], v[1]);
                sensor_read(sd, vals->reg_num, &z[0]);
                sensor_read(sd, vals->reg_num+1, &z[1]);
                printk(" read reg_num = 0x%x  \nz[0] = 0x%x ====== z[1] = 0x%x \n",vals->reg_num, z[0], z[1]);
            }
#endif
        }
        vals++;
    }
#if 0
        sensor_read(sd, 0x31AE, &z[0]);
        sensor_read(sd, 0x31AF, &z[1]);
        printk(" read reg_num = 0x31AE  \nz[0] = 0x%x ====== z[1] = 0x%x \n", z[0], z[1]);
#endif
    return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, int val)
{
    return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
    int ret;
    char v[2] = { 0 };
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
    printk("v[0] = 0x%x ---------- v[1] = 0x%x\n",v[0], v[1]);
#else
    ret = sensor_read(sd, 0x3000, &v[0]);
    printk("ret = %d &&&&&& v[0] = %d\n", ret, v[0]);
    ret = sensor_read(sd, 0x3001, &v[1]);
    printk("ret = %d &&&&&& v[0] = %d\n", ret, v[1]);
    if (ret < 0)
        return ret;

    if (v[0] != SENSOR_CHIP_ID_H)
        return -ENODEV;

    if (v[1] != SENSOR_CHIP_ID_L)
        return -ENODEV;
    printk("v[0] = %d --------- v[1] = %d\n", v[0], v[1]);
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
    ret += sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
    ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
    ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
    /*set analog gain*/
    ret = sensor_write(sd, 0x3e09, gain_lut[index].again);
    /*set coarse dgain*/
    ret = sensor_write(sd, 0x3e06, gain_lut[index].coarse_dgain);
    /*set fine dgain*/
    ret = sensor_write(sd, 0x3e07, gain_lut[index].fine_dgain);
    if (ret < 0)
            return ret;

    return 0;
}
#endif

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = 0;
    ret = sensor_write(sd, 0x0202, value);
    if (ret < 0)
        return ret;

    return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret;
    ret = sensor_write(sd, 0x305E, value);
    if (ret < 0)
        return ret;

    return 0;
}

static int sensor_set_logic(struct tx_isp_subdev *sd, int value)
{
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
        if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
            ret = sensor_write_array(sd, sensor_stream_on_mipi);
        } else {
            ISP_ERROR("Don't support this Sensor Data interface\n");
        }
        ISP_WARNING("%s stream on\n", SENSOR_NAME);
    } else {
        if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
            ret = sensor_write_array(sd, sensor_stream_off_mipi);
        } else {
            ISP_ERROR("Don't support this Sensor Data interface\n");
        }
        ISP_WARNING("%s stream off\n", SENSOR_NAME);
    }
    return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
    return 0;
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    unsigned int hts = 0;
    unsigned int vts = 0;
    unsigned char tmp[2];
    unsigned int pclk = SENSOR_SUPPORT_25FPS_SCLK;
    unsigned int newformat = 0; //the format is 24.8
    int ret = 0;
    /* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
    newformat = (((fps >> 16) / (fps & 0xffff)) << 8)
                + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
    if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
        return -1;
    ret += sensor_read(sd, 0x300C, tmp);
    hts = tmp[0];
    if (ret < 0)
        return -1;
    hts = (hts << 8) + tmp[1];
    vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
    ret = sensor_write(sd, 0x300A, vts);

    sensor->video.fps = fps;

    sensor_update_actual_fps((fps >> 16) & 0xffff);
    sensor->video.attr->max_integration_time_native = vts - 5;
    sensor->video.attr->integration_time_limit = vts - 5;
    sensor->video.attr->total_height = vts;
    sensor->video.attr->max_integration_time = vts - 5;
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

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip)
{
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
            ISP_ERROR("gpio request fail %d\n", reset_gpio);
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
            ISP_ERROR("gpio request fail %d\n", pwdn_gpio);
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

    ISP_WARNING("%s chip found @ 0x%02x (%s)\n", SENSOR_NAME, client->addr, client->adapter->name);
    ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
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
    switch (cmd) {
/*
    case TX_ISP_EVENT_SENSOR_EXPO:
        if (arg)
            ret = sensor_set_expo(sd, *(int*)arg);
        break;
*/
    case TX_ISP_EVENT_SENSOR_INT_TIME:
        if (arg)
            ret = sensor_set_integration_time(sd, *(int *) arg);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN:
        if (arg)
            ret = sensor_set_analog_gain(sd, *(int *) arg);
        break;
    case TX_ISP_EVENT_SENSOR_DGAIN:
        if (arg)
            ret = sensor_set_digital_gain(sd, *(int *) arg);
        break;
    case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
        if (arg)
            ret = sensor_get_black_pedestal(sd, *(int *) arg);
        break;
    case TX_ISP_EVENT_SENSOR_RESIZE:
        if (arg)
            ret = sensor_set_mode(sd, *(int *) arg);
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
            ret = sensor_set_fps(sd, *(int *) arg);
        break;
    case TX_ISP_EVENT_SENSOR_VFLIP:
/*
        if (arg)
            ret = sensor_set_vflip(sd, *(int*)arg);
        break;
*/
    case TX_ISP_EVENT_SENSOR_LOGIC:
        if (arg)
            ret = sensor_set_logic(sd, *(int *) arg);
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct tx_isp_subdev *sd;
    struct tx_isp_video_in *video;
    struct tx_isp_sensor *sensor;

    sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
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

    sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
    tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
    tx_isp_set_subdevdata(sd, client);
    tx_isp_set_subdev_hostdata(sd, sensor);
    private_i2c_set_clientdata(client, sd);
    pr_debug("probe ok ------->%s\n", SENSOR_NAME);
    return 0;

err_get_mclk:
    private_clk_disable(sensor->mclk);
    private_clk_put(sensor->mclk);
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

static const struct i2c_device_id sensor_id[] = { { SENSOR_NAME, 0 }, {} };
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

MODULE_DESCRIPTION("A low-level driver for " SENSOR_NAME " sensor");
MODULE_LICENSE("GPL");
