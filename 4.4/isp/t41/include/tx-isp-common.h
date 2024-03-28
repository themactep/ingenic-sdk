#ifndef __TX_ISP_COMMON_H__
#define __TX_ISP_COMMON_H__
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>

#include "tx-isp-videodev.h"
#include "txx-funcs.h"
#include "tx-isp-device.h"

/* #define TX_ISP_LDC_ENABLE */

/* FGPA flag : V9=1 V10=2 */
/* #define CONFIG_FPGA_ISP_TEST 2 */

/* #define MULTICAMERA */

#define ISP_SUCCESS 0
/* #define SENSORNUM 3 */
/* #define CSINUM 2 */
/* #define VINNUM 3 */
/* #define VICNUM 3 */
/* #define ISPCORENUM 3 */
/* #define TIZIANONUM 2 */
#define SENSORNUM 1
#define CSINUM 1
#define VINNUM 1
#define VICNUM 1
#define ISPCORENUM 1
#define TIZIANONUM 1

/* T31 */
#define TX_ISP_INPUT_PORT_MAX_WIDTH		4096
#define TX_ISP_INPUT_PORT_MAX_HEIGHT		4096
#define TX_ISP_FR_CHANNEL_MAX_WIDTH		2624
#define TX_ISP_FR_CHANNEL_MAX_HEIGHT		2048
#define TX_ISP_DS1_CHANNEL_MAX_WIDTH		1920
#define TX_ISP_DS1_CHANNEL_MAX_HEIGHT		1080

/*****************************************************
 			sensor attributes
*****************************************************/
#define SENSOR_R_BLACK_LEVEL	0
#define SENSOR_GR_BLACK_LEVEL	1
#define SENSOR_GB_BLACK_LEVEL	2
#define SENSOR_B_BLACK_LEVEL	3


struct tx_isp_sensor_win_setting {
	int width;
	int height;
	int fps;
	enum input_mbus_fmt mbus_code;
	enum tisp_colorspace colorspace;
	void *regs;	/* Regs to tweak; the default fps is fast */
};


/***************************************************
 *  Provide extensions to v4l2 for ISP driver.
 ****************************************************/
#define V4L2_PIX_FMT_RGB310   v4l2_fourcc('R', 'G', 'B', 'A') /* 32  RGB-10-10-10  */
#define V4L2_MBUS_FMT_RGB888_3X8_LE (V4L2_MBUS_FMT_Y8_1X8 - 0x10)

/*
 *------ definition sensor associated structure -----
 */

/* define control bus */
enum tx_sensor_control_bus_type{
	TX_SENSOR_CONTROL_INTERFACE_I2C = 1,
	TX_SENSOR_CONTROL_INTERFACE_SPI,
};
struct tx_isp_i2c_board_info {
	char type[I2C_NAME_SIZE];
	int addr;
//	struct i2c_board_info board_info;
	int i2c_adapter_id;
};

struct tx_isp_spi_board_info {
	char modalias[SPI_NAME_SIZE];
	int bus_num;
//	struct spi_board_info board_info;
};

/* define data bus */
enum tx_sensor_data_bus_type{
	TX_SENSOR_DATA_INTERFACE_MIPI = 1,
	TX_SENSOR_DATA_INTERFACE_DVP,
	TX_SENSOR_DATA_INTERFACE_BT601,
	TX_SENSOR_DATA_INTERFACE_BT656,
	TX_SENSOR_DATA_INTERFACE_BT1120,
	TX_SENSOR_DATA_INTERFACE_LVDS,
};

enum tx_sensor_data_type {
	TX_SENSOR_DATA_TYPE_LINEAR = 0,
	TX_SENSOR_DATA_TYPE_WDR_FS,
	TX_SENSOR_DATA_TYPE_WDR_DOL,
	TX_SENSOR_DATA_TYPE_WDR_NATIVE,
};

enum tx_sensor_frm_mode {
	TX_SENSOR_DEFAULT_FRAME_MODE = 0,
	TX_SENSOR_WDR_2_FRAME_MODE,
	TX_SENSOR_WDR_3_FRAME_MODE,
	TX_SENSOR_WDR_4_FRAME_MODE,
};

enum tx_sensor_mode {
	TX_SENSOR_DEFAULT_MODE = 0,
	TX_SENSOR_NOT_VC_MODE,
	TX_SENSOR_VC_MODE,
};

enum tx_sensor_csi_fmt {
	TX_SENSOR_RAW8 = 0,
	TX_SENSOR_RAW10,
	TX_SENSOR_RAW12,
	TX_SENSOR_YUV422 = 7,
};

typedef enum {
	DVP_PA_LOW_10BIT,
	DVP_PA_HIGH_10BIT,
	DVP_PA_12BIT,
	DVP_PA_LOW_8BIT,
	DVP_PA_HIGH_8BIT,
} sensor_dvp_gpio_mode;

typedef enum {
	SENSOR_DVP_HREF_MODE,
	SENSOR_DVP_HSYNC_MODE,
	SENSOR_DVP_SONY_MODE,
} sensor_dvp_timing_mode;

typedef enum {
	SENSOR_MIPI_OTHER_MODE,
	SENSOR_MIPI_SONY_MODE,
}sensor_mipi_mode;

typedef enum {
	SENSOR_BT_8BIT,
	SENSOR_BT_16BIT,
}sensor_bt_mode;

typedef enum {
	ISP_CLK_960P_MODE = 60000000,
	ISP_CLK_1080P_MODE = 90000000,
	ISP_CLK_3M_MODE = 100000000,
} isp_clk_mode;

typedef enum {
	TX_SENSOR_MAX_FPS_120 = 120,
	TX_SENSOR_MAX_FPS_110 = 110,
	TX_SENSOR_MAX_FPS_100 = 100,
	TX_SENSOR_MAX_FPS_90 = 90,
	TX_SENSOR_MAX_FPS_80 = 80,
	TX_SENSOR_MAX_FPS_70 = 70,
	TX_SENSOR_MAX_FPS_60 = 60,
	TX_SENSOR_MAX_FPS_50 = 50,
	TX_SENSOR_MAX_FPS_45 = 45,
	TX_SENSOR_MAX_FPS_40 = 40,
	TX_SENSOR_MAX_FPS_30 = 30,
	TX_SENSOR_MAX_FPS_25 = 25,
	TX_SENSOR_MAX_FPS_20 = 20,
	TX_SENSOR_MAX_FPS_15 = 15,
	TX_SENSOR_MAX_FPS_12 = 12,
	TX_SENSOR_MAX_FPS_10 = 10,
	TX_SENSOR_MAX_FPS_5 = 5,
} sensor_max_fps_mode;

typedef enum {
	TX_SENSOR_RES_30 = 30,
	TX_SENSOR_RES_100 = 100,
	TX_SENSOR_RES_130 = 130,
	TX_SENSOR_RES_200 = 200,
	TX_SENSOR_RES_300 = 300,
	TX_SENSOR_RES_400 = 400,
	TX_SENSOR_RES_500 = 500,
	TX_SENSOR_RES_800 = 800,
} sensor_resolution_type;

enum tx_isp_dvp_polarity {
	DVP_POLARITY_DEFAULT,
	DVP_POLARITY_HIGH,
	DVP_POLARITY_LOW,
};

typedef struct {
	unsigned short vblanking;
	unsigned short hblanking;
} sensor_dvp_blanking;

enum  mipi_sensor_data_type_value {
	YUV422_8BIT = 0x1e,
	RAW8 = 0x2a,
	RAW10 = 0x2b,
	RAW12 = 0x2c,
};

typedef struct {
	unsigned char hsync_polar;
	unsigned char vsync_polar;
	unsigned char pclk_polar;
} sensor_dvp_polar;

struct vic_mipi_sensor_ctl {
	unsigned int hcrop_diff_en;
	unsigned int mipi_vcomp_en;
	unsigned int mipi_hcomp_en;
	unsigned short mipi_crop_start0x;
	unsigned short mipi_crop_start0y;
	unsigned short mipi_crop_start1x;
	unsigned short mipi_crop_start1y;
	unsigned short mipi_crop_start2x;
	unsigned short mipi_crop_start2y;
	unsigned short mipi_crop_start3x;
	unsigned short mipi_crop_start3y;
	unsigned int line_sync_mode;
	unsigned int work_start_flag;
	unsigned int data_type_en;
	enum  mipi_sensor_data_type_value data_type_value;//0x1E 2A 2B 2C
	unsigned int del_start;
	unsigned int sensor_fid_mode;
	enum tx_sensor_frm_mode sensor_frame_mode;
	enum tx_sensor_mode sensor_mode;
	enum tx_sensor_csi_fmt sensor_csi_fmt;
};

struct mipi_crop_ch {
	unsigned int all_image_width;
	unsigned int hcrop_del_start_pixel;
};

struct vic_mipi_vcrop_del{
	unsigned int del0;
	unsigned int del1;
	unsigned int del2;
	unsigned int del3;
};

struct tx_isp_mipi_bus{
	sensor_mipi_mode mode;
	unsigned int clk;
	unsigned int clk_nocontinue;
	unsigned int data;
	unsigned char index;
	unsigned char lans;
	unsigned int def2wdr;
	unsigned int image_twidth;
	unsigned int image_theight;
	unsigned int front_invlid_pix;
	unsigned int after_invlid_pix;
	unsigned int settle_time_apative_en;
	struct vic_mipi_sensor_ctl mipi_sc;
	struct vic_mipi_vcrop_del mipi_vcropdel;
};

struct tx_isp_dvp_bus{
	sensor_dvp_gpio_mode gpio;
	sensor_dvp_timing_mode mode;
	sensor_dvp_blanking blanking;
	sensor_dvp_polar polar;
	unsigned int dvp_hcomp_en;
};

struct tx_isp_bt1120_bus{
	sensor_bt_mode gpio;
};
struct tx_isp_bt656_bus{
	sensor_bt_mode gpio;
};
struct tx_isp_bt601_bus{
	sensor_bt_mode gpio;
};

struct tx_isp_lvds_bus{

};

/* define sensor attribute */

#define TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS 	1
#define TX_ISP_SENSOR_FULL_RES_MAX_FPS 		2

typedef enum {
	TISP_SENSOR_VI_MIPI_CSI0 = 0,
	TISP_SENSOR_VI_MIPI_CSI1 = 1,
	TISP_SENSOR_VI_DVP = 2,
	TISP_SENSOR_VI_BUTT = 3,
} vin_type_t;

typedef enum {
	TISP_SENSOR_MCLK0 = 0,
	TISP_SENSOR_MCLK1 = 1,
	TISP_SENSOR_MCLK2 = 2,
	TISP_SENSOR_MCLK_BUTT = 3,
} vin_mclk_t;

struct tx_isp_sensor_register_info {
	char name[32];
	enum tx_sensor_control_bus_type cbus_type;
	union {
		struct tx_isp_i2c_board_info i2c;
		struct tx_isp_spi_board_info spi;
	};
	int rst_gpio;
	int pwdn_gpio;
	int power_gpio;
	unsigned short sensor_id;
	vin_type_t video_interface;
	vin_mclk_t mclk;
	int default_boot;
};

typedef struct tx_isp_sensor_ctrl{
	/* isp_gain mean that the value is output of ISP-FW,it is not a gain multiplier unit.
	 *  gain_mutiplier = (2^(isp_gain/(2^LOG_GAIN_SHIFT))).
	 *  the fuction will convert gain_mutiplier to sensor_Xgain.
	 *  the return value is isp_gain of sensor_Xgain's inverse conversion.
	 */
	unsigned int (*alloc_again)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again);
	unsigned int (*alloc_again_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again);
	unsigned int (*alloc_dgain)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain);
	unsigned int (*alloc_dgain_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain);
	unsigned int (*alloc_integration_time)(unsigned int it, unsigned char shift, unsigned int *sensor_it);
	unsigned int (*alloc_integration_time_short)(unsigned int it_short, unsigned char shift, unsigned int *sensor_it_short);
	unsigned int (*get_first_integration_time)(unsigned int it, unsigned char shift, unsigned int *sensor_it);
	unsigned int (*set_first_integration_time)(unsigned int it, unsigned char shift, unsigned int *sensor_it);
	unsigned int (*get_first_again)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_isp_gain);
	unsigned int (*set_first_again)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_isp_gain);
	unsigned int (*get_first_dgain)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_isp_gain);
	unsigned int (*set_first_dgain)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_isp_gain);
	unsigned int (*get_first_integration_time_short)(unsigned int it_short, unsigned char shift, unsigned int *sensor_it_short);
	unsigned int (*set_first_integration_time_short)(unsigned int it_short, unsigned char shift, unsigned int *sensor_it_short);
	unsigned int (*get_first_again_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_isp_gain);
	unsigned int (*set_first_again_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_isp_gain);
	unsigned int (*get_first_dgain_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_isp_gain);
	unsigned int (*set_first_dgain_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_isp_gain);
} TX_ISP_SENSOR_CTRL;

enum tx_sensor_frame_sync_mode {
	TX_SENSOR_FSYNC_CLOSE,
	TX_SENSOR_FSYNC_SLAVE_MODE,	/**< salve mode, PWM contrl */
	TX_SENSOR_FSYNC_MSLAVE_MODE,	/**< master-salve mode, the master controls the slave */
};

#define TX_ISP_GAIN_FIXED_POINT 16
#define LOG2_GAIN_SHIFT 16
struct tx_isp_sensor_attribute{
	const char *name;
	unsigned int chip_id;
	enum tx_sensor_control_bus_type cbus_type;
	unsigned int cbus_mask;
	unsigned int cbus_device;
	enum tx_sensor_data_bus_type dbus_type;
	union {
		struct tx_isp_mipi_bus 		mipi;
		struct tx_isp_dvp_bus 		dvp;
		struct tx_isp_bt1120_bus 	bt1120;
		struct tx_isp_bt656_bus		bt656bus;
		struct tx_isp_bt601_bus		bt601bus;
		struct tx_isp_lvds_bus		lvds;
		char string[64];
	};
	enum tx_sensor_data_type data_type;
	unsigned int max_again;	//the format is .16
	unsigned int max_dgain;	//the format is .16
	unsigned int again;
	unsigned int dgain;
	unsigned short min_integration_time;
	unsigned short min_integration_time_native;
	unsigned short max_integration_time_native;
	unsigned short integration_time_limit;
	unsigned int integration_time;
	unsigned short total_width;
	unsigned short total_height;
	unsigned short max_integration_time;
	unsigned short integration_time_apply_delay;
	unsigned short again_apply_delay;
	unsigned short dgain_apply_delay;
	unsigned short one_line_expr_in_us;
	TX_ISP_SENSOR_CTRL sensor_ctrl;
	unsigned short min_integration_time_short;
	unsigned short max_integration_time_short;
	unsigned int integration_time_short;
	unsigned int max_again_short;	//the format is .16
	unsigned int again_short;
	unsigned int wdr_cache;	//the format is .16
	unsigned int expo;
	unsigned int expo_short;
	unsigned int expo_fs;
	unsigned int sensor_num;
	unsigned int max_dgain_short;	//the format is .16
	unsigned int dgain_short;
	unsigned short again_short_apply_delay;
	unsigned short dgain_short_apply_delay;
	unsigned short integration_time_short_apply_delay;
	enum tx_sensor_frame_sync_mode sensor_fsync_mode;
	void *priv; /* point to struct tx_isp_sensor_board_info */
};

/* define common struct */
enum tx_isp_priv_ioctl_direction {
	TX_ISP_PRIVATE_IOCTL_SET,
	TX_ISP_PRIVATE_IOCTL_GET,
};

#define NOTIFICATION_TYPE_CORE_OPS	(0x1 << 24)
#define NOTIFICATION_TYPE_SENSOR_OPS	(0x2 << 24)
#define NOTIFICATION_TYPE_FS_OPS 	(0x3 << 24)
#define NOTIFICATION_TYPE_TUN_OPS 	(0x4 << 24)
#define NOTIFICATION_TYPE_LDC_OPS 	(0x5 << 24)
#define NOTIFICATION_TYPE_OPS(n)	((n) & (0xff << 24))
enum tx_isp_notification {
	/* the events of subdev */
	TX_ISP_EVENT_SUBDEV_INIT = NOTIFICATION_TYPE_CORE_OPS,
	TX_ISP_EVENT_SYNC_SENSOR_ATTR,
	TX_ISP_EVENT_CREATE_FRAME_CHAN_INODE,
	TX_ISP_EVENT_SYNC_FRAME_FORMAT,
	TX_ISP_EVENT_ADD_INTERNAL_INPUT,
	TX_ISP_EVENT_SET_INTERNAL_INPUT,
	TX_ISP_EVENT_GET_INTERNAL_INPUT,
	TX_ISP_EVENT_SYNC_CORE_INFO,
	TX_ISP_EVENT_S_WDR_MODE,
	/* the events of sensor are defined as follows. */
	TX_ISP_EVENT_SENSOR_REGISTER = NOTIFICATION_TYPE_SENSOR_OPS,
	TX_ISP_EVENT_SENSOR_RELEASE,
	TX_ISP_EVENT_SENSOR_ENUM_INPUT,
	TX_ISP_EVENT_SENSOR_GET_INPUT,
	TX_ISP_EVENT_SENSOR_SET_INPUT,
	TX_ISP_EVENT_SENSOR_INT_TIME,
	TX_ISP_EVENT_SENSOR_INT_TIME_SHORT,
	TX_ISP_EVENT_SENSOR_AGAIN,
	TX_ISP_EVENT_SENSOR_AGAIN_SHORT,
	TX_ISP_EVENT_SENSOR_DGAIN,
	TX_ISP_EVENT_SENSOR_FPS,
	TX_ISP_EVENT_SENSOR_BLACK_LEVEL,
	TX_ISP_EVENT_SENSOR_WDR,
	TX_ISP_EVENT_SENSOR_RESIZE,
	TX_ISP_EVENT_SENSOR_PREPARE_CHANGE,
	TX_ISP_EVENT_SENSOR_FINISH_CHANGE,
	TX_ISP_EVENT_SENSOR_VFLIP,
	TX_ISP_EVENT_SENSOR_S_REGISTER,
	TX_ISP_EVENT_SENSOR_G_REGISTER,
	TX_ISP_EVENT_SENSOR_WDR_STOP,
	TX_ISP_EVENT_SENSOR_WDR_OPEN,
	TX_ISP_EVENT_SENSOR_LOGIC,
	TX_ISP_EVENT_SENSOR_EXPO,
	TX_ISP_EVENT_SENSOR_EXPO_SHORT,
	TX_ISP_EVENT_DUAL_MODE,
	TX_ISP_EVENT_BYPASS_MODE,
	TX_ISP_EVENT_SENSOR_DGAIN_SHORT,
	TX_ISP_EVENT_FRAME_SYNC_MODE,
	/* the events of frame-channel are defined as follows. */
	TX_ISP_EVENT_FRAME_CHAN_BYPASS_ISP = NOTIFICATION_TYPE_FS_OPS,
	TX_ISP_EVENT_FRAME_CHAN_GET_FMT,
	TX_ISP_EVENT_FRAME_CHAN_SET_FMT,
	TX_ISP_EVENT_FRAME_CHAN_STREAM_ON,
	TX_ISP_EVENT_FRAME_CHAN_STREAM_OFF,
	TX_ISP_EVENT_FRAME_CHAN_QUEUE_BUFFER,
	TX_ISP_EVENT_FRAME_CHAN_DQUEUE_BUFFER,
	TX_ISP_EVENT_FRAME_CHAN_REQUEST_BUFFER,
	TX_ISP_EVENT_FRAME_CHAN_FREE_BUFFER,
	TX_ISP_EVENT_FRAME_CHAN_SET_BANKS,
	TX_ISP_EVENT_FRAME_CHAN_CREATE_PRIVATE_PIPO,
	/* the tuning node of isp's core */
	TX_ISP_EVENT_ACTIVATE_MODULE = NOTIFICATION_TYPE_TUN_OPS,
	TX_ISP_EVENT_SLAVE_MODULE,
	TX_ISP_EVENT_CORE_FRAME_DONE,
	TX_ISP_EVENT_CORE_DAY_NIGHT,
};

struct tx_isp_notify_argument{
	int value;
	int ret;
};

struct tx_isp_initarg{
	int enable;
	int vinum;
};

struct tx_isp_sensor_value{
	int vinum;
	int value;
};

struct frame_image_scalercap {
	unsigned short max_width;
	unsigned short max_height;
	unsigned short min_width;
	unsigned short min_height;
};
struct frame_image_scaler {
	unsigned short out_width;
	unsigned short out_height;
};

/* isp image tuning */
struct isp_image_tuning_default_ctrl {
	int vinum;
	enum tx_isp_priv_ioctl_direction dir;
	struct tisp_control control;
};

/**
 * struct frame_image_format
 * @type:	enum v4l2_buf_type; type of the data stream
 */
struct frame_image_format {
	unsigned int type;
	struct tisp_pix_format pix;

	/* crop */
	bool	crop_enable;
	unsigned int crop_top;
	unsigned int crop_left;
	unsigned int crop_width;
	unsigned int crop_height;

	/* scaler */
	bool	scaler_enable;
	unsigned int scaler_out_width;
	unsigned int scaler_out_height;

	unsigned int rate_bits;
	unsigned int rate_mask;

	/* crop front */
	bool	fcrop_enable;
	unsigned int fcrop_top;
	unsigned int fcrop_left;
	unsigned int fcrop_width;
	unsigned int fcrop_height;
	bool mirr_enable;
};

#define ISP_LFB_DEFAULT_BUF_BASE0 0xf0000000
#define ISP_LFB_DEFAULT_BUF_BASE1 0xf8000000
enum tx_isp_module_link_id {
	LINK_ISP_LFB_NCU_MS_FS,
	LINK_ISP_LDC_NCU_MS_FS,
	LINK_ISP_DDR_FS,
};

struct vicdonecb_wait_attr
{
	int  vic_completion;
	struct completion comp;
};

struct tx_isp_version {
	char cpu[32];
	char version[64];
};

struct isp_buf_info {
	uint32_t vinum;
	uint32_t paddr;
	uint32_t size;
};

enum tx_isp_vidioc_default_command {
	TX_ISP_VIDIOC_DEFAULT_CMD_BYPASS_ISP,
	TX_ISP_VIDIOC_DEFAULT_CMD_SCALER_CAP,
	TX_ISP_VIDIOC_DEFAULT_CMD_SET_SCALER,
};

enum tx_isp_frame_channel_bypass_isp{
	TX_ISP_FRAME_CHANNEL_BYPASS_ISP_DISABLE,
	TX_ISP_FRAME_CHANNEL_BYPASS_ISP_ENABLE,
};

struct tx_isp_internal_input {
	int (*get_fmt)(void *handle, void *data);
	int (*set_fmt)(void *handle, void *data);
	int (*streamon)(void *handle);
	int (*streamoff)(void *handle);
	int (*qbuf)(void *handle, void *data);
	int (*reqbufs)(void *handle, void *data);
	int (*freebufs)(void *handle, void *data);
};

enum tx_isp_internal_channel_type {
	TX_ISP_INTERNAL_CHANNEL_ISP_DMA0,
	TX_ISP_INTERNAL_CHANNEL_ISP_DMA1,
	TX_ISP_INTERNAL_CHANNEL_ISP_DMA2,
	TX_ISP_INTERNAL_CHANNEL_ISP_IR,
	TX_ISP_INTERNAL_CHANNEL_VIC_DMA0,
	TX_ISP_INTERNAL_CHANNEL_VIC_DMA1,
	TX_ISP_INTERNAL_CHANNEL_VIC_BUTT,
};

struct tx_isp_internal_channel_attr {
	enum tx_isp_internal_channel_type type;
	uint8_t vc_index;
};

struct tx_isp_vic_dma {
	unsigned char vc_index;

	struct tx_isp_subdev_pad *pad;

	unsigned char streamon;
};

struct tx_isp_internal_input_attr {
	enum tx_isp_internal_channel_type ch;
	struct tx_isp_vic_dma vdma;
};

struct tx_isp_video_in {
	struct tisp_mbus_framefmt mbus;
	unsigned int mbus_change;
	struct tx_isp_sensor_attribute *attr;
	struct tx_isp_sensor_register_info *info;
	unsigned int vi_max_width;	//the max width of sensor output setting
	unsigned int vi_max_height;	//the max height of sensor output setting
	unsigned int fps;
	unsigned int min_fps;
	unsigned int max_fps;
	int grp_id;
	int vic_id;
	unsigned int shvflip;
	int state;
};

enum tx_isp_notify_statement {
	TX_ISP_NOTIFY_LINK_SETUP = 0x10,
	TX_ISP_NOTIFY_LINK_DESTROY,
};

enum tx_isp_core_info_type {
	TX_ISP_SYNC_CORE_INFO_DM = 0x0,
};

struct tx_isp_core_info {
	enum tx_isp_core_info_type cmd;
	void *data;
};

struct tx_isp_sensor{
	struct tx_isp_subdev sd;
	struct device *dev;
	int index;
	unsigned int type;
	struct list_head list;
	struct tx_isp_sensor_register_info info;
	struct tx_isp_sensor_attribute attr;
	struct tx_isp_video_in video;
	struct clk *mclk;
	void *priv;
};

typedef struct {
	uint8_t vinum;
	enum tx_sensor_data_type mode;
} tx_wdr_open_attr;

#define sd_to_sensor_device(_ep) container_of(_ep, struct tx_isp_sensor, sd)

#define tx_isp_readl(base, reg)			__raw_readl((base) + (reg))
#define tx_isp_writel(base, reg, value)		__raw_writel((value), ((base) + (reg)))
#define tx_isp_readw(base, reg)			__raw_readw((base) + (reg))
#define tx_isp_writew(base, reg, value)		__raw_writew((value), ((base) + (reg)))
#define tx_isp_readb(base, reg)			__raw_readb((base) + (reg))
#define tx_isp_writeb(base, reg, value)		__raw_writeb((value), ((base) + (reg)))

#endif /*__TX_ISP_COMMON_H__*/
