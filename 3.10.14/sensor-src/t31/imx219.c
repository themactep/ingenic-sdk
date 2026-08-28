// SPDX-License-Identifier: GPL-2.0+
/*
 * imx219.c
 * IMX219 sensor driver for the Ingenic T31 tx-isp framework.
 *
 * Ported from:
 *  - T31 framework glue: sensor-src/t31/sc2336.c (proven on 28 real boards)
 *  - 16-bit register read/write + split exposure/gain style: this session's
 *    own sensor-src/t31/ov5647.c port (same conventions apply directly)
 *  - All IMX219-specific register data (chip ID, mode timing, exposure/gain
 *    encoding): mainline Linux drivers/media/i2c/imx219.c (v5.15, static
 *    per-mode register-table era) -- register values are Raspberry Pi's own
 *    firmware-derived dump for the 1920x1080@30fps mode.
 *
 * I2C address 0x10 and chip ID 0x0219 confirmed live on real hardware
 * (this exact board/sensor) before writing this driver.
 *
 * Four modes ported from mainline's supported_modes[], selected at load
 * time via the sensor_resolution module param (this SDK's convention --
 * see jxf35.c/gc2053.c for the same pattern):
 *   - 1920x1080@30fps (cropped)    -- default, sensor_resolution=1080
 *   - 1632x1232@30fps (2x2 binned, full FOV) -- sensor_resolution=1232
 *   - 3280x2464@15fps (full sensor)          -- sensor_resolution=2464
 *   - 640x480@30fps                          -- sensor_resolution=480
 * Pixel rate (182.4MHz) and HTS (0x0d78) are identical across all four
 * modes -- only VTS and the digital crop/output-size registers change.
 *
 * All four confirmed live on real T31 hardware (I2C chip-ID read,
 * MIPI stream-on, real RTSP video). 1080p/1632x1232/640x480 also
 * confirmed through the full encoder pipeline (H.264 + JPEG channels
 * create and deliver real frames). 3280x2464 programs the sensor
 * correctly but T31's own IMP encoder rejects it outright
 * ("invalid resolution(3280x2464) to encode", both H.264 and JPEG)
 * -- it exceeds this SoC's real ISP output-channel ceiling
 * (TX_ISP_FR_CHANNEL_MAX_WIDTH=2624, MAX_HEIGHT=2048 in
 * tx-isp-common.h). That's a T31 silicon/ISP limit, not a driver
 * bug -- kept for mainline fidelity and other SoC tiers.
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
#define SENSOR_NAME "imx219"
#define SENSOR_VERSION "H20260828c"
#define SENSOR_CHIP_ID 0x0219
#define SENSOR_CHIP_ID_H (0x02)
#define SENSOR_CHIP_ID_L (0x19)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x10

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 3280
#define SENSOR_MAX_HEIGHT 2464

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
/* IMX219's pixel rate is fixed at 182.4MHz for all modes (mainline:
 * IMX219_PIXEL_RATE), regardless of resolution/binning. */
#define SENSOR_SUPPORT_30FPS_SCLK (182400000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

/* Mode select, named/valued to match this SDK's sensor_resolution
 * convention (see jxf35.c/gc2053.c): value is the mode's height, since
 * width alone isn't unique here (mainline defines no 5th mode). */
enum {
    SENSOR_RES_1080 = 1080,
    SENSOR_RES_1232 = 1232,
    SENSOR_RES_2464 = 2464,
    SENSOR_RES_480 = 480,
};

static int sensor_resolution = SENSOR_RES_1080;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor Resolution setting interface: 1080=1920x1080@30fps (default, cropped), 1232=1632x1232@30fps (2x2 binned, full FOV), 2464=3280x2464@15fps (full sensor), 480=640x480@30fps");

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

/* IMX219 analog gain: 8-bit code across 0x0157, Sony's documented formula
 * gain_x = 256 / (256 - code), code range 0-232 (max ~10.67x).
 * gain_q16 = log2(gain_x) * 65536. */
struct again_lut sensor_again_lut[] = {
    {0x00, 0},
    {0x01, 370},
    {0x02, 742},
    {0x03, 1115},
    {0x04, 1489},
    {0x05, 1865},
    {0x06, 2242},
    {0x07, 2621},
    {0x08, 3002},
    {0x09, 3384},
    {0x0a, 3767},
    {0x0b, 4152},
    {0x0c, 4539},
    {0x0d, 4927},
    {0x0e, 5317},
    {0x0f, 5709},
    {0x10, 6102},
    {0x11, 6497},
    {0x12, 6893},
    {0x13, 7291},
    {0x14, 7691},
    {0x15, 8093},
    {0x16, 8496},
    {0x17, 8901},
    {0x18, 9307},
    {0x19, 9716},
    {0x1a, 10126},
    {0x1b, 10538},
    {0x1c, 10952},
    {0x1d, 11367},
    {0x1e, 11785},
    {0x1f, 12204},
    {0x20, 12625},
    {0x21, 13048},
    {0x22, 13473},
    {0x23, 13900},
    {0x24, 14329},
    {0x25, 14760},
    {0x26, 15192},
    {0x27, 15627},
    {0x28, 16064},
    {0x29, 16502},
    {0x2a, 16943},
    {0x2b, 17386},
    {0x2c, 17831},
    {0x2d, 18278},
    {0x2e, 18727},
    {0x2f, 19179},
    {0x30, 19632},
    {0x31, 20088},
    {0x32, 20546},
    {0x33, 21006},
    {0x34, 21468},
    {0x35, 21933},
    {0x36, 22399},
    {0x37, 22869},
    {0x38, 23340},
    {0x39, 23814},
    {0x3a, 24290},
    {0x3b, 24769},
    {0x3c, 25250},
    {0x3d, 25734},
    {0x3e, 26220},
    {0x3f, 26709},
    {0x40, 27200},
    {0x41, 27694},
    {0x42, 28190},
    {0x43, 28689},
    {0x44, 29190},
    {0x45, 29695},
    {0x46, 30202},
    {0x47, 30711},
    {0x48, 31224},
    {0x49, 31739},
    {0x4a, 32257},
    {0x4b, 32778},
    {0x4c, 33302},
    {0x4d, 33829},
    {0x4e, 34358},
    {0x4f, 34891},
    {0x50, 35427},
    {0x51, 35965},
    {0x52, 36507},
    {0x53, 37052},
    {0x54, 37600},
    {0x55, 38152},
    {0x56, 38706},
    {0x57, 39264},
    {0x58, 39825},
    {0x59, 40390},
    {0x5a, 40957},
    {0x5b, 41529},
    {0x5c, 42103},
    {0x5d, 42682},
    {0x5e, 43264},
    {0x5f, 43849},
    {0x60, 44438},
    {0x61, 45031},
    {0x62, 45627},
    {0x63, 46228},
    {0x64, 46832},
    {0x65, 47440},
    {0x66, 48052},
    {0x67, 48668},
    {0x68, 49288},
    {0x69, 49912},
    {0x6a, 50540},
    {0x6b, 51173},
    {0x6c, 51809},
    {0x6d, 52450},
    {0x6e, 53096},
    {0x6f, 53745},
    {0x70, 54400},
    {0x71, 55059},
    {0x72, 55722},
    {0x73, 56390},
    {0x74, 57063},
    {0x75, 57741},
    {0x76, 58424},
    {0x77, 59111},
    {0x78, 59804},
    {0x79, 60502},
    {0x7a, 61205},
    {0x7b, 61913},
    {0x7c, 62627},
    {0x7d, 63346},
    {0x7e, 64070},
    {0x7f, 64800},
    {0x80, 65536},
    {0x81, 66278},
    {0x82, 67025},
    {0x83, 67778},
    {0x84, 68538},
    {0x85, 69303},
    {0x86, 70075},
    {0x87, 70853},
    {0x88, 71638},
    {0x89, 72429},
    {0x8a, 73227},
    {0x8b, 74032},
    {0x8c, 74843},
    {0x8d, 75662},
    {0x8e, 76488},
    {0x8f, 77321},
    {0x90, 78161},
    {0x91, 79009},
    {0x92, 79865},
    {0x93, 80728},
    {0x94, 81600},
    {0x95, 82479},
    {0x96, 83367},
    {0x97, 84263},
    {0x98, 85168},
    {0x99, 86082},
    {0x9a, 87004},
    {0x9b, 87935},
    {0x9c, 88876},
    {0x9d, 89826},
    {0x9e, 90786},
    {0x9f, 91756},
    {0xa0, 92736},
    {0xa1, 93726},
    {0xa2, 94726},
    {0xa3, 95738},
    {0xa4, 96760},
    {0xa5, 97793},
    {0xa6, 98838},
    {0xa7, 99894},
    {0xa8, 100963},
    {0xa9, 102043},
    {0xaa, 103136},
    {0xab, 104242},
    {0xac, 105361},
    {0xad, 106493},
    {0xae, 107639},
    {0xaf, 108800},
    {0xb0, 109974},
    {0xb1, 111163},
    {0xb2, 112368},
    {0xb3, 113588},
    {0xb4, 114824},
    {0xb5, 116076},
    {0xb6, 117345},
    {0xb7, 118632},
    {0xb8, 119936},
    {0xb9, 121258},
    {0xba, 122599},
    {0xbb, 123960},
    {0xbc, 125340},
    {0xbd, 126741},
    {0xbe, 128163},
    {0xbf, 129606},
    {0xc0, 131072},
    {0xc1, 132561},
    {0xc2, 134074},
    {0xc3, 135611},
    {0xc4, 137174},
    {0xc5, 138763},
    {0xc6, 140379},
    {0xc7, 142024},
    {0xc8, 143697},
    {0xc9, 145401},
    {0xca, 147136},
    {0xcb, 148903},
    {0xcc, 150704},
    {0xcd, 152540},
    {0xce, 154412},
    {0xcf, 156322},
    {0xd0, 158272},
    {0xd1, 160262},
    {0xd2, 162296},
    {0xd3, 164374},
    {0xd4, 166499},
    {0xd5, 168672},
    {0xd6, 170897},
    {0xd7, 173175},
    {0xd8, 175510},
    {0xd9, 177904},
    {0xda, 180360},
    {0xdb, 182881},
    {0xdc, 185472},
    {0xdd, 188135},
    {0xde, 190876},
    {0xdf, 193699},
    {0xe0, 196608},
    {0xe1, 199610},
    {0xe2, 202710},
    {0xe3, 205915},
    {0xe4, 209233},
    {0xe5, 212672},
    {0xe6, 216240},
    {0xe7, 219948},
    {0xe8, 223808},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
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
        /* 2-lane MIPI (standard Raspberry Pi Camera v2 wiring; IMX219
         * register 0x0114 is explicitly set to 2-lane mode in the init
         * table below). clk (Mbps/lane) = pixel_rate * bpp / lanes / 1e6
         * = 182,400,000 * 10 / 2 / 1e6 = 912, cross-checked against
         * link_freq * 2 / 1e6 = 456,000,000 * 2 / 1e6 = 912. */
        .clk = 912,
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
    },
    .data_type = TX_SENSOR_DATA_TYPE_LINEAR,
    .max_again = 223808,
    .max_dgain = 0,
    .min_integration_time = 4,
    .min_integration_time_native = 4,
    .max_integration_time_native = 0x6e3 - 4,
    .integration_time_limit = 0x6e3 - 4,
    .total_width = 0x0d78,
    .total_height = 0x6e3,
    .max_integration_time = 0x6e3 - 4,
    .one_line_expr_in_us = 19,
    .integration_time_apply_delay = 2,
    .again_apply_delay = 2,
    .dgain_apply_delay = 0,
    .sensor_ctrl.alloc_again = sensor_alloc_again,
    .sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

/* 1920x1080@30fps mode regs (mainline mode_1920_1080_regs verbatim, plus
 * explicit VTS write per the OV5647-port lesson), terminated with
 * stream-on (0x0100=0x01) matching this SDK's own convention of baking
 * initial stream-on into the mode-set table. */
static struct regval_list sensor_init_regs_1920_1080_mipi[] = {
    {0x0100, 0x00},
    {0x30eb, 0x05},
    {0x30eb, 0x0c},
    {0x300a, 0xff},
    {0x300b, 0xff},
    {0x30eb, 0x05},
    {0x30eb, 0x09},
    {0x0114, 0x01},
    {0x0128, 0x00},
    {0x012a, 0x18},
    {0x012b, 0x00},
    {0x0162, 0x0d},
    {0x0163, 0x78},
    {0x0160, 0x06},
    {0x0161, 0xe3},
    {0x0164, 0x02},
    {0x0165, 0xa8},
    {0x0166, 0x0a},
    {0x0167, 0x27},
    {0x0168, 0x02},
    {0x0169, 0xb4},
    {0x016a, 0x06},
    {0x016b, 0xeb},
    {0x016c, 0x07},
    {0x016d, 0x80},
    {0x016e, 0x04},
    {0x016f, 0x38},
    {0x0170, 0x01},
    {0x0171, 0x01},
    {0x0174, 0x00},
    {0x0175, 0x00},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x03},
    {0x0305, 0x03},
    {0x0306, 0x00},
    {0x0307, 0x39},
    {0x030b, 0x01},
    {0x030c, 0x00},
    {0x030d, 0x72},
    {0x0624, 0x07},
    {0x0625, 0x80},
    {0x0626, 0x04},
    {0x0627, 0x38},
    {0x455e, 0x00},
    {0x471e, 0x4b},
    {0x4767, 0x0f},
    {0x4750, 0x14},
    {0x4540, 0x00},
    {0x47b4, 0x14},
    {0x4713, 0x30},
    {0x478b, 0x10},
    {0x478f, 0x10},
    {0x4793, 0x10},
    {0x4797, 0x0e},
    {0x479b, 0x0e},
    {0x0100, 0x01},

    {SENSOR_REG_END, 0x00},
};

/* 1632x1232@30fps mode regs -- 2x2 binned readout of the full
 * 3280x2464 array, trimmed from mainline's native 1640-wide binned
 * output to 1632 (a multiple of 16). T31's IMP encoder rejects
 * 1640x1232 outright ("invalid resolution(1640x1232) to encode") --
 * confirmed on real hardware, reproducibly, independent of codec --
 * because 1640 isn't macroblock-aligned (1640 = 16*102 + 8). Trims
 * 8px off each side of the analog readout (a multiple of 4, so the
 * binner's 2x2 Bayer grouping stays aligned): x_addr_start=8,
 * x_addr_end=3271 (was 0/3279), giving a binned output width of
 * 1632 instead of 1640. Same HTS (0x0d78) and VTS (0x06e3) as the
 * 1080p mode; only the x crop/output-size registers change. */
static struct regval_list sensor_init_regs_1632_1232_mipi[] = {
    {0x0100, 0x00},
    {0x30eb, 0x0c},
    {0x30eb, 0x05},
    {0x300a, 0xff},
    {0x300b, 0xff},
    {0x30eb, 0x05},
    {0x30eb, 0x09},
    {0x0114, 0x01},
    {0x0128, 0x00},
    {0x012a, 0x18},
    {0x012b, 0x00},
    {0x0164, 0x00},
    {0x0165, 0x08},
    {0x0166, 0x0c},
    {0x0167, 0xc7},
    {0x0168, 0x00},
    {0x0169, 0x00},
    {0x016a, 0x09},
    {0x016b, 0x9f},
    {0x016c, 0x06},
    {0x016d, 0x60},
    {0x016e, 0x04},
    {0x016f, 0xd0},
    {0x0170, 0x01},
    {0x0171, 0x01},
    {0x0174, 0x01},
    {0x0175, 0x01},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x03},
    {0x0305, 0x03},
    {0x0306, 0x00},
    {0x0307, 0x39},
    {0x030b, 0x01},
    {0x030c, 0x00},
    {0x030d, 0x72},
    {0x0624, 0x06},
    {0x0625, 0x60},
    {0x0626, 0x04},
    {0x0627, 0xd0},
    {0x455e, 0x00},
    {0x471e, 0x4b},
    {0x4767, 0x0f},
    {0x4750, 0x14},
    {0x4540, 0x00},
    {0x47b4, 0x14},
    {0x4713, 0x30},
    {0x478b, 0x10},
    {0x478f, 0x10},
    {0x4793, 0x10},
    {0x4797, 0x0e},
    {0x479b, 0x0e},
    {0x0162, 0x0d},
    {0x0163, 0x78},
    {0x0160, 0x06},
    {0x0161, 0xe3},
    {0x0100, 0x01},

    {SENSOR_REG_END, 0x00},
};

/* 3280x2464@15fps mode regs (mainline mode_3280x2464_regs verbatim) --
 * full 8MP sensor array, no binning/cropping. VTS is larger here
 * (0x0dc6) since this mode runs at 15fps instead of 30fps. */
static struct regval_list sensor_init_regs_3280_2464_mipi[] = {
    {0x0100, 0x00},
    {0x30eb, 0x0c},
    {0x30eb, 0x05},
    {0x300a, 0xff},
    {0x300b, 0xff},
    {0x30eb, 0x05},
    {0x30eb, 0x09},
    {0x0114, 0x01},
    {0x0128, 0x00},
    {0x012a, 0x18},
    {0x012b, 0x00},
    {0x0164, 0x00},
    {0x0165, 0x00},
    {0x0166, 0x0c},
    {0x0167, 0xcf},
    {0x0168, 0x00},
    {0x0169, 0x00},
    {0x016a, 0x09},
    {0x016b, 0x9f},
    {0x016c, 0x0c},
    {0x016d, 0xd0},
    {0x016e, 0x09},
    {0x016f, 0xa0},
    {0x0170, 0x01},
    {0x0171, 0x01},
    {0x0174, 0x00},
    {0x0175, 0x00},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x03},
    {0x0305, 0x03},
    {0x0306, 0x00},
    {0x0307, 0x39},
    {0x030b, 0x01},
    {0x030c, 0x00},
    {0x030d, 0x72},
    {0x0624, 0x0c},
    {0x0625, 0xd0},
    {0x0626, 0x09},
    {0x0627, 0xa0},
    {0x455e, 0x00},
    {0x471e, 0x4b},
    {0x4767, 0x0f},
    {0x4750, 0x14},
    {0x4540, 0x00},
    {0x47b4, 0x14},
    {0x4713, 0x30},
    {0x478b, 0x10},
    {0x478f, 0x10},
    {0x4793, 0x10},
    {0x4797, 0x0e},
    {0x479b, 0x0e},
    {0x0162, 0x0d},
    {0x0163, 0x78},
    {0x0160, 0x0d},
    {0x0161, 0xc6},
    {0x0100, 0x01},

    {SENSOR_REG_END, 0x00},
};

/* 640x480@30fps mode regs (mainline mode_640_480_regs verbatim). Same
 * HTS/VTS as the 1080p mode; only the crop/output-size registers
 * differ (crops to the center 1280x960 of the binned array, then
 * further reduces to 640x480). */
static struct regval_list sensor_init_regs_640_480_mipi[] = {
    {0x0100, 0x00},
    {0x30eb, 0x05},
    {0x30eb, 0x0c},
    {0x300a, 0xff},
    {0x300b, 0xff},
    {0x30eb, 0x05},
    {0x30eb, 0x09},
    {0x0114, 0x01},
    {0x0128, 0x00},
    {0x012a, 0x18},
    {0x012b, 0x00},
    {0x0162, 0x0d},
    {0x0163, 0x78},
    {0x0164, 0x03},
    {0x0165, 0xe8},
    {0x0166, 0x08},
    {0x0167, 0xe7},
    {0x0168, 0x02},
    {0x0169, 0xf0},
    {0x016a, 0x06},
    {0x016b, 0xaf},
    {0x016c, 0x02},
    {0x016d, 0x80},
    {0x016e, 0x01},
    {0x016f, 0xe0},
    {0x0170, 0x01},
    {0x0171, 0x01},
    {0x0174, 0x03},
    {0x0175, 0x03},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x03},
    {0x0305, 0x03},
    {0x0306, 0x00},
    {0x0307, 0x39},
    {0x030b, 0x01},
    {0x030c, 0x00},
    {0x030d, 0x72},
    {0x0624, 0x06},
    {0x0625, 0x68},
    {0x0626, 0x04},
    {0x0627, 0xd0},
    {0x455e, 0x00},
    {0x471e, 0x4b},
    {0x4767, 0x0f},
    {0x4750, 0x14},
    {0x4540, 0x00},
    {0x47b4, 0x14},
    {0x4713, 0x30},
    {0x478b, 0x10},
    {0x478f, 0x10},
    {0x4793, 0x10},
    {0x4797, 0x0e},
    {0x479b, 0x0e},
    {0x0160, 0x06},
    {0x0161, 0xe3},
    {0x0100, 0x01},

    {SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
    {
        .width = 1920,
        .height = 1080,
        .fps = 30 << 16 | 1,
        .mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
        .colorspace = V4L2_COLORSPACE_SRGB,
        .regs = sensor_init_regs_1920_1080_mipi,
    },
    {
        .width = 1632,
        .height = 1232,
        .fps = 30 << 16 | 1,
        .mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
        .colorspace = V4L2_COLORSPACE_SRGB,
        .regs = sensor_init_regs_1632_1232_mipi,
    },
    {
        .width = 3280,
        .height = 2464,
        .fps = 15 << 16 | 1,
        .mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
        .colorspace = V4L2_COLORSPACE_SRGB,
        .regs = sensor_init_regs_3280_2464_mipi,
    },
    {
        .width = 640,
        .height = 480,
        .fps = 30 << 16 | 1,
        .mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
        .colorspace = V4L2_COLORSPACE_SRGB,
        .regs = sensor_init_regs_640_480_mipi,
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

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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
    if (ret > 0) {
        ret = 0;
    }

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
    if (ret > 0) {
        ret = 0;
    }

    return ret;
}

static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = sensor_read(sd, vals->reg_num, &val);
            if (ret < 0) {
                return ret;
            }
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
            if (ret < 0) {
                return ret;
            }
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
    int ret;
    unsigned char v;

    ret = sensor_read(sd, 0x0000, &v);
    ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0) {
        return ret;
    }

    if (v != SENSOR_CHIP_ID_H) {
        return -ENODEV;
    }

    *ident = v;

    ret = sensor_read(sd, 0x0001, &v);
    ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0) {
        return ret;
    }

    if (v != SENSOR_CHIP_ID_L) {
        return -ENODEV;
    }

    *ident = (*ident << 8) | v;

    return 0;
}

/* Plain 16-bit line-count exposure across 0x015A(hi)/0x015B(lo) -- unlike
 * OV5647, IMX219 has no sub-line fraction bits, so no <<4 shift needed. */
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = 0;

    ret += sensor_write(sd, 0x015a, (unsigned char)((value >> 8) & 0xff));
    ret += sensor_write(sd, 0x015b, (unsigned char)(value & 0xff));
    if (ret < 0)
        return ret;

    return 0;
}

/* 8-bit analog gain code, single register 0x0157. */
static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = 0;

    ret = sensor_write(sd, 0x0157, (unsigned char)(value & 0xff));
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

    if (!enable) {
        return ISP_SUCCESS;
    }

    sensor->video.mbus.width = wsize->width;
    sensor->video.mbus.height = wsize->height;
    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.mbus.field = V4L2_FIELD_NONE;
    sensor->video.mbus.colorspace = wsize->colorspace;
    sensor->video.fps = wsize->fps;

    sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);

    ret = sensor_write_array(sd, wsize->regs);
    if (ret) {
        return ret;
    }

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
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    unsigned int clk = 0;
    unsigned int hts = 0;
    unsigned int vts = 0;
    unsigned char val = 0;
    unsigned int newformat = 0; //the format is 24.8
    int ret = 0;

    newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
    if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
        ISP_ERROR("warn: fps(%d) not in range\n", fps);
        return -1;
    }

    clk = SENSOR_SUPPORT_30FPS_SCLK;
    ret = sensor_read(sd, 0x0162, &val);
    hts = val;
    ret += sensor_read(sd, 0x0163, &val);
    if (0 != ret) {
        ISP_ERROR("err: %s read err\n", SENSOR_NAME);
        return ret;
    }

    hts = ((hts << 8) + val);
    vts = clk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

    ret += sensor_write(sd, 0x0161, (unsigned char)(vts & 0xff));
    ret += sensor_write(sd, 0x0160, (unsigned char)(vts >> 8));
    if (0 != ret) {
        ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
        return ret;
    }

    sensor->video.fps = fps;

    sensor_update_actual_fps((fps >> 16) & 0xffff);
    sensor->video.attr->max_integration_time_native = vts - 4;
    sensor->video.attr->integration_time_limit = vts - 4;
    sensor->video.attr->total_height = vts;
    sensor->video.attr->max_integration_time = vts - 4;
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

    /* IMX219's XCLR (reset) pin is active-low per Sony's standard RESET_N
     * convention -- driving it high releases reset / enables normal
     * operation, confirmed via mainline's own power-on sequence (drive
     * high, wait >=6.2ms, then I2C access is valid). Reuse this board's
     * proven pulse pattern (same reset_gpio=GPIO_PA(18) as imx327/ov5647
     * on this exact board) which ends in the released (high) state. */
    if (pwdn_gpio != -1) {
        ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
        if (!ret) {
            private_gpio_direction_output(pwdn_gpio, 1);
            private_msleep(10);
            private_gpio_direction_output(pwdn_gpio, 0);
            private_msleep(20);
        } else {
            ISP_ERROR("gpio request fail %d\n", pwdn_gpio);
        }
    }
    if (reset_gpio != -1) {
        ret = private_gpio_request(reset_gpio, "sensor_reset");
        if (!ret) {
            private_gpio_direction_output(reset_gpio, 1);
            private_msleep(5);
            private_gpio_direction_output(reset_gpio, 0);
            private_msleep(10);
            private_gpio_direction_output(reset_gpio, 1);
            private_msleep(10);
        } else {
            ISP_ERROR("gpio request fail %d\n", reset_gpio);
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

/* IMX219 uses a single combined orientation register (0x0172, bit0=hflip,
 * bit1=vflip) unlike OV5647's two separate registers. Native pattern is
 * RGGB; flipping shifts the Bayer CFA phase same as any Bayer sensor, so
 * remap mbus_code alongside the register write (modeled after ov2735b.c's
 * approach, adapted for IMX219's single-register flip). */
static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = 0;
    int hflip = enable & 0x1;
    int vflip = (enable >> 1) & 0x1;
    static const u32 codes[4] = {
        V4L2_MBUS_FMT_SRGGB10_1X10, /* no flip (matches mode table default) */
        V4L2_MBUS_FMT_SGRBG10_1X10, /* hflip only */
        V4L2_MBUS_FMT_SGBRG10_1X10, /* vflip only */
        V4L2_MBUS_FMT_SBGGR10_1X10, /* both */
    };

    ret = sensor_write(sd, 0x0172, (unsigned char)(hflip | (vflip << 1)));

    sensor->video.mbus.code = codes[hflip | (vflip << 1)];
    sensor->video.mbus_change = 1;

    if (!ret)
        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
    return ret;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
    long ret = 0;

    if (IS_ERR_OR_NULL(sd)) {
        ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
        return -EINVAL;
    }

    switch (cmd) {
        case TX_ISP_EVENT_SENSOR_INT_TIME:
            if (arg) {
                ret = sensor_set_integration_time(sd, *(int *) arg);
            }
            break;
        case TX_ISP_EVENT_SENSOR_AGAIN:
            if (arg) {
                ret = sensor_set_analog_gain(sd, *(int *) arg);
            }
            break;
        case TX_ISP_EVENT_SENSOR_DGAIN:
            if (arg) {
                ret = sensor_set_digital_gain(sd, *(int *) arg);
            }
            break;
        case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
            if (arg) {
                ret = sensor_get_black_pedestal(sd, *(int *) arg);
            }
            break;
        case TX_ISP_EVENT_SENSOR_RESIZE:
            if (arg) {
                ret = sensor_set_mode(sd, *(int *) arg);
            }
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
            if (arg) {
                ret = sensor_set_fps(sd, *(int *) arg);
            }
            break;
        case TX_ISP_EVENT_SENSOR_VFLIP:
            if (arg) {
                ret = sensor_set_vflip(sd, *(int *) arg);
            }
            break;
        case TX_ISP_EVENT_SENSOR_LOGIC:
            if (arg) {
                ret = sensor_set_logic(sd, *(int *) arg);
            }
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

    if (!private_capable(CAP_SYS_ADMIN)) {
        return -EPERM;
    }

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

    if (!private_capable(CAP_SYS_ADMIN)) {
        return -EPERM;
    }

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

    /* IMX219 requires exactly 24MHz xclk (mainline: IMX219_XCLK_FREQ). */
    private_clk_set_rate(sensor->mclk, 24000000);
    private_clk_enable(sensor->mclk);

    /* Select mode per sensor_resolution (this SDK's convention -- see
     * jxf35.c/gc2053.c). HTS (total_width) and mbus_code are identical
     * across all four modes so only VTS-derived integration limits and
     * the mipi image_twidth/theight need overriding here. */
    switch (sensor_resolution) {
        case SENSOR_RES_1232:
            wsize = &sensor_win_sizes[1];
            sensor_attr.mipi.image_twidth = 1632;
            sensor_attr.mipi.image_theight = 1232;
            break;
        case SENSOR_RES_2464:
            wsize = &sensor_win_sizes[2];
            sensor_attr.mipi.image_twidth = 3280;
            sensor_attr.mipi.image_theight = 2464;
            sensor_attr.total_height = 0x0dc6;
            sensor_attr.max_integration_time_native = 0x0dc6 - 4;
            sensor_attr.integration_time_limit = 0x0dc6 - 4;
            sensor_attr.max_integration_time = 0x0dc6 - 4;
            break;
        case SENSOR_RES_480:
            wsize = &sensor_win_sizes[3];
            sensor_attr.mipi.image_twidth = 640;
            sensor_attr.mipi.image_theight = 480;
            break;
        case SENSOR_RES_1080:
        default:
            wsize = &sensor_win_sizes[0];
            break;
    }

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

    if (reset_gpio != -1) {
        private_gpio_free(reset_gpio);
    }
    if (pwdn_gpio != -1) {
        private_gpio_free(pwdn_gpio);
    }
    private_clk_disable(sensor->mclk);
    private_clk_put(sensor->mclk);
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

static __init int init_sensor(void)
{
    int ret = 0;

    /* sensor_info.width/height (registered here, at module_init, before
     * sensor_probe() runs) is what "auto" (0x0) stream resolution
     * requests resolve against -- it must match whichever mode
     * sensor_resolution actually selects, not the sensor's absolute
     * max capability, or downstream would request a size the hardware
     * isn't actually programmed to output. */
    switch (sensor_resolution) {
        case SENSOR_RES_1232:
            sensor_info.width = 1632;
            sensor_info.height = 1232;
            break;
        case SENSOR_RES_2464:
            sensor_info.width = 3280;
            sensor_info.height = 2464;
            break;
        case SENSOR_RES_480:
            sensor_info.width = 640;
            sensor_info.height = 480;
            break;
        case SENSOR_RES_1080:
        default:
            sensor_info.width = 1920;
            sensor_info.height = 1080;
            break;
    }

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
