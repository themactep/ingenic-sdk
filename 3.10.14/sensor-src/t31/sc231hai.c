// SPDX-License-Identifier: GPL-2.0+
/*
 * sc231hai.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Register map recovered from the vendor sensor_sc231hai_t31.ko (Infiya K1).
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
#define SENSOR_NAME "sc231hai"
#define SENSOR_VERSION "H20220125a"
#define SENSOR_CHIP_ID 0xcb6a
#define SENSOR_CHIP_ID_H (0xcb)
#define SENSOR_CHIP_ID_L (0x6a)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x30

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
/* Vendor value; 396 Mbps x 2 lanes / 10 bit = 79.2 MHz pixel clock. */
#define SENSOR_SUPPORT_30FPS_SCLK (0x4b87f00)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 1

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

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
    {0x20, 0},
    {0x21, 2886},
    {0x22, 5776},
    {0x23, 8494},
    {0x24, 11136},
    {0x25, 13706},
    {0x26, 16288},
    {0x27, 18724},
    {0x28, 21098},
    {0x29, 23414},
    {0x2a, 25747},
    {0x2b, 27953},
    {0x2c, 30109},
    {0x2d, 32217},
    {0x2e, 34345},
    {0x2f, 36362},
    {0x30, 38336},
    {0x31, 40270},
    {0x32, 42226},
    {0x33, 44083},
    {0x34, 45904},
    {0x35, 47691},
    {0x36, 49500},
    {0x37, 51221},
    {0x38, 52911},
    {0x39, 54571},
    {0x3a, 56255},
    {0x3b, 57858},
    {0x3c, 59434},
    {0x3d, 60984},
    {0x3e, 62559},
    {0x3f, 64059},
    {0x120, 65536},
    {0x121, 68468},
    {0x122, 71268},
    {0x123, 74030},
    {0x124, 76672},
    {0x125, 79283},
    {0x126, 81784},
    {0x127, 84260},
    {0x128, 86634},
    {0x129, 88987},
    {0x12a, 91247},
    {0x12b, 93489},
    {0x12c, 95645},
    {0x12d, 97787},
    {0x12e, 99848},
    {0x12f, 101898},
    {0x130, 103872},
    {0x131, 105837},
    {0x132, 107732},
    {0x133, 109619},
    {0x134, 111440},
    {0x135, 113255},
    {0x136, 115008},
    {0x137, 116757},
    {0x138, 118447},
    {0x139, 120134},
    {0x13a, 121765},
    {0x13b, 123394},
    {0x8020, 123701},
    {0x8021, 126620},
    {0x8022, 129427},
    {0x8023, 132176},
    {0x8024, 134848},
    {0x8025, 137425},
    {0x8026, 139954},
    {0x8027, 142397},
    {0x8028, 144799},
    {0x8029, 147141},
    {0x802a, 149407},
    {0x802b, 151639},
    {0x802c, 153819},
    {0x802d, 155933},
    {0x802e, 158017},
    {0x802f, 160040},
    {0x8030, 162037},
    {0x8031, 163993},
    {0x8032, 165893},
    {0x8033, 167771},
    {0x8034, 169613},
    {0x8035, 171404},
    {0x8036, 173177},
    {0x8037, 174902},
    {0x8038, 176612},
    {0x8039, 178291},
    {0x803a, 179926},
    {0x803b, 181547},
    {0x803c, 183142},
    {0x803d, 184696},
    {0x803e, 186238},
    {0x803f, 187743},
    {0x8120, 189237},
    {0x8121, 192143},
    {0x8122, 194975},
    {0x8123, 197712},
    {0x8124, 200373},
    {0x8125, 202961},
    {0x8126, 205490},
    {0x8127, 207944},
    {0x8128, 210335},
    {0x8129, 212667},
    {0x812a, 214953},
    {0x812b, 217175},
    {0x812c, 219346},
    {0x812d, 221469},
    {0x812e, 223553},
    {0x812f, 225585},
    {0x8130, 227573},
    {0x8131, 229520},
    {0x8132, 231437},
    {0x8133, 233307},
    {0x8134, 235141},
    {0x8135, 236940},
    {0x8136, 238713},
    {0x8137, 240446},
    {0x8138, 242148},
    {0x8139, 243819},
    {0x813a, 245469},
    {0x813b, 247083},
    {0x813c, 248671},
    {0x813d, 250232},
    {0x813e, 251774},
    {0x813f, 253286},
    {0x8320, 254773},
    {0x8321, 257685},
    {0x8322, 260505},
    {0x8323, 263248},
    {0x8324, 265909},
    {0x8325, 268502},
    {0x8326, 271021},
    {0x8327, 273480},
    {0x8328, 275871},
    {0x8329, 278208},
    {0x832a, 280484},
    {0x832b, 282711},
    {0x832c, 284882},
    {0x832d, 287009},
    {0x832e, 289085},
    {0x832f, 291121},
    {0x8330, 293109},
    {0x8331, 295061},
    {0x8332, 296969},
    {0x8333, 298843},
    {0x8334, 300677},
    {0x8335, 302480},
    {0x8336, 304245},
    {0x8337, 305982},
    {0x8338, 307684},
    {0x8339, 309359},
    {0x833a, 311002},
    {0x833b, 312620},
    {0x833c, 314207},
    {0x833d, 315771},
    {0x833e, 317307},
    {0x833f, 318822},
    {0x8720, 320309},
    {0x8721, 323218},
    {0x8722, 326041},
    {0x8723, 328782},
    {0x8724, 331445},
    {0x8725, 334036},
    {0x8726, 336557},
    {0x8727, 339013},
    {0x8728, 341407},
    {0x8729, 343741},
    {0x872a, 346020},
    {0x872b, 348245},
    {0x872c, 350418},
    {0x872d, 352543},
    {0x872e, 354621},
    {0x872f, 356654},
    {0x8730, 358645},
    {0x8731, 360594},
    {0x8732, 362505},
    {0x8733, 364377},
    {0x8734, 366213},
    {0x8735, 368014},
    {0x8736, 369781},
    {0x8737, 371516},
    {0x8738, 373220},
    {0x8739, 374893},
    {0x873a, 376538},
    {0x873b, 378154},
    {0x873c, 379743},
    {0x873d, 381306},
    {0x873e, 382843},
    {0x873f, 384356},
    {0x8f20, 385845},
    {0x8f21, 388754},
    {0x8f22, 391577},
    {0x8f23, 394318},
    {0x8f24, 396981},
    {0x8f25, 399572},
    {0x8f26, 402093},
    {0x8f27, 404549},
    {0x8f28, 406943},
    {0x8f29, 409277},
    {0x8f2a, 411556},
    {0x8f2b, 413781},
    {0x8f2c, 415954},
    {0x8f2d, 418079},
    {0x8f2e, 420157},
    {0x8f2f, 422190},
    {0x8f30, 424181},
    {0x8f31, 426130},
    {0x8f32, 428041},
    {0x8f33, 429913},
    {0x8f34, 431749},
    {0x8f35, 433550},
    {0x8f36, 435317},
    {0x8f37, 437052},
    {0x8f38, 438756},
    {0x8f39, 440429},
    {0x8f3a, 442074},
    {0x8f3b, 443690},
    {0x8f3c, 445279},
    {0x8f3d, 446842},
    {0x8f3e, 448379},
    {0x8f3f, 449892},
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
        .clk = 396,
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
    .max_again = 435317,
    .max_dgain = 0,
    .min_integration_time = 3,
    .min_integration_time_native = 3,
    .max_integration_time_native = 0x4b0 - 8,
    .integration_time_limit = 0x4b0 - 8,
    .total_width = 0x898,
    .total_height = 0x4b0,
    .max_integration_time = 0x4b0 - 8,
    .integration_time_apply_delay = 2,
    .again_apply_delay = 2,
    .dgain_apply_delay = 0,
    .sensor_ctrl.alloc_again = sensor_alloc_again,
    .sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
    {0x0103, 0x01},
    {0x36e9, 0x80},
    {0x37f9, 0x80},
    {0x301f, 0x14},
    {0x3058, 0x21},
    {0x3059, 0x53},
    {0x305a, 0x40},
    {0x320e, 0x04},
    {0x320f, 0xb0},
    {0x3210, 0x00},
    {0x3211, 0x04},
    {0x3212, 0x00},
    {0x3213, 0x04},
    {0x3250, 0x00},
    {0x3301, 0x0a},
    {0x3302, 0x20},
    {0x3304, 0x90},
    {0x3305, 0x00},
    {0x3306, 0x68},
    {0x3309, 0xd0},
    {0x330b, 0xd8},
    {0x330d, 0x08},
    {0x331c, 0x04},
    {0x331e, 0x81},
    {0x331f, 0xc1},
    {0x3323, 0x06},
    {0x3333, 0x10},
    {0x3334, 0x40},
    {0x3364, 0x5e},
    {0x336c, 0x8e},
    {0x337f, 0x13},
    {0x338f, 0x80},
    {0x3390, 0x08},
    {0x3391, 0x18},
    {0x3392, 0xb8},
    {0x3393, 0x0e},
    {0x3394, 0x14},
    {0x3395, 0x10},
    {0x3396, 0x88},
    {0x3397, 0x98},
    {0x3398, 0xf8},
    {0x3399, 0x0a},
    {0x339a, 0x0e},
    {0x339b, 0x10},
    {0x339c, 0x3c},
    {0x33ae, 0x80},
    {0x33af, 0xc0},
    {0x33b2, 0x50},
    {0x33b3, 0x14},
    {0x33f8, 0x00},
    {0x33f9, 0x68},
    {0x33fa, 0x00},
    {0x33fb, 0x68},
    {0x33fc, 0x48},
    {0x33fd, 0x78},
    {0x349f, 0x03},
    {0x34a6, 0x40},
    {0x34a7, 0x58},
    {0x34a8, 0x10},
    {0x34a9, 0x10},
    {0x34f8, 0x78},
    {0x34f9, 0x10},
    {0x3619, 0x20},
    {0x361a, 0x90},
    {0x3633, 0x44},
    {0x3637, 0x5c},
    {0x363c, 0xc0},
    {0x363d, 0x02},
    {0x3660, 0x80},
    {0x3661, 0x81},
    {0x3662, 0x8f},
    {0x3663, 0x81},
    {0x3664, 0x81},
    {0x3665, 0x82},
    {0x3666, 0x8f},
    {0x3667, 0x08},
    {0x3668, 0x80},
    {0x3669, 0x88},
    {0x366a, 0x98},
    {0x366b, 0xb8},
    {0x366c, 0xf8},
    {0x3670, 0xb2},
    {0x3671, 0xa2},
    {0x3672, 0x88},
    {0x3680, 0x33},
    {0x3681, 0x33},
    {0x3682, 0x43},
    {0x36c0, 0x80},
    {0x36c1, 0x88},
    {0x36c8, 0x88},
    {0x36c9, 0xb8},
    {0x36ea, 0x0b},
    {0x36eb, 0x0c},
    {0x36ec, 0x5c},
    {0x36ed, 0x04},
    {0x3718, 0x04},
    {0x3722, 0x8b},
    {0x3724, 0xd1},
    {0x3741, 0x08},
    {0x3770, 0x17},
    {0x3771, 0x9b},
    {0x3772, 0x9b},
    {0x37c0, 0x88},
    {0x37c1, 0xb8},
    {0x37fa, 0x0b},
    {0x37fc, 0x10},
    {0x37fd, 0x04},
    {0x3902, 0xc0},
    {0x3903, 0x40},
    {0x3909, 0x00},
    {0x391f, 0x41},
    {0x3926, 0xe0},
    {0x3933, 0x80},
    {0x3934, 0x02},
    {0x3937, 0x6f},
    {0x3e00, 0x00},
    {0x3e01, 0x95},
    {0x3e02, 0x50},
    {0x3e08, 0x00},
    {0x4509, 0x20},
    {0x450d, 0x07},
    {0x4837, 0x33},
    {0x5780, 0x76},
    {0x5784, 0x10},
    {0x5787, 0x0a},
    {0x5788, 0x0a},
    {0x5789, 0x08},
    {0x578a, 0x0a},
    {0x578b, 0x0a},
    {0x578c, 0x08},
    {0x578d, 0x40},
    {0x5792, 0x04},
    {0x5795, 0x04},
    {0x57ac, 0x00},
    {0x57ad, 0x00},
    {0x36e9, 0x27},
    {0x37f9, 0x27},
    {0x0100, 0x01},
    {SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
    {
        .width = 1920,
        .height = 1080,
        .fps = 30 << 16 | 1,
        .mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
        .colorspace = V4L2_COLORSPACE_SRGB,
        .regs = sensor_init_regs_1920_1080_30fps_mipi,
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

    ret = sensor_read(sd, 0x3107, &v);
    ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0) {
        return ret;
    }

    if (v != SENSOR_CHIP_ID_H) {
        return -ENODEV;
    }

    *ident = v;

    ret = sensor_read(sd, 0x3108, &v);
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

static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
    int ret = 0;
    int it = (value & 0xffff);
    int again = (value & 0xffff0000) >> 16;

    /* Integration time is in half-line units in 0x3e00/0x3e01/0x3e02. */
    it *= 2;
    ret += sensor_write(sd, 0x3e00, (unsigned char) ((it >> 12) & 0xf));
    ret += sensor_write(sd, 0x3e01, (unsigned char) ((it >> 4) & 0xff));
    ret += sensor_write(sd, 0x3e02, (unsigned char) ((it & 0x0f) << 4));

    /* Analog gain coarse/fine: 0x3e08 high byte, 0x3e09 low byte. */
    ret += sensor_write(sd, 0x3e08, (unsigned char) ((again >> 8) & 0xff));
    ret += sensor_write(sd, 0x3e09, (unsigned char) (again & 0xff));
    if (ret < 0) {
        return ret;
    }

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
        ret = sensor_write_array(sd, sensor_stream_on_mipi);
        ISP_WARNING("%s stream on\n", SENSOR_NAME);
    } else {
        ret = sensor_write_array(sd, sensor_stream_off_mipi);
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
    ret = sensor_read(sd, 0x320c, &val);
    hts = val;
    ret += sensor_read(sd, 0x320d, &val);
    if (0 != ret) {
        ISP_ERROR("err: %s read err\n", SENSOR_NAME);
        return ret;
    }

    hts = ((hts << 8) + val);
    vts = clk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

    ret += sensor_write(sd, 0x320f, (unsigned char) (vts & 0xff));
    ret += sensor_write(sd, 0x320e, (unsigned char) (vts >> 8));
    if (0 != ret) {
        ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
        return ret;
    }

    sensor->video.fps = fps;

    sensor_update_actual_fps((fps >> 16) & 0xffff);
    sensor->video.attr->max_integration_time_native = vts - 8;
    sensor->video.attr->integration_time_limit = vts - 8;
    sensor->video.attr->total_height = vts;
    sensor->video.attr->max_integration_time = vts - 8;
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
            private_msleep(5);
            private_gpio_direction_output(reset_gpio, 0);
            private_msleep(5);
            private_gpio_direction_output(reset_gpio, 1);
            private_msleep(5);
        } else {
            ISP_ERROR("gpio request fail %d\n", reset_gpio);
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
            ISP_ERROR("gpio request fail %d\n", pwdn_gpio);
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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = -1;
    unsigned char val = 0x0;

    ret += sensor_read(sd, 0x3221, &val);
    if (enable & 0x2) {
        val = (val & 0x99) | 0x60;
    } else {
        val &= 0x99;
    }

    ret += sensor_write(sd, 0x3221, val);
    if (!ret) {
        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
    }
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
        case TX_ISP_EVENT_SENSOR_EXPO:
            if (arg) {
                ret = sensor_set_expo(sd, *(int *) arg);
            }
            break;
        case TX_ISP_EVENT_SENSOR_INT_TIME:
//            if (arg) {
//                ret = sensor_set_integration_time(sd, *(int *) arg);
//            }
            break;
        case TX_ISP_EVENT_SENSOR_AGAIN:
//            if (arg) {
//                ret = sensor_set_analog_gain(sd, *(int *) arg);
//            }
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
            ret = sensor_write_array(sd, sensor_stream_off_mipi);
            break;
        case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
            ret = sensor_write_array(sd, sensor_stream_on_mipi);
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

    private_clk_set_rate(sensor->mclk, 24000000);
    private_clk_enable(sensor->mclk);

    /* Convert sensor-gain into isp-gain, */
    sd = &sensor->sd;
    video = &sensor->video;
    sensor->video.attr = &sensor_attr;
    sensor_attr.expo_fs = 1;
    sensor->video.shvflip = shvflip;
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

    ISP_WARNING("probe ok ------->%s\n", SENSOR_NAME);

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
