#ifndef __TX_ISP_DEVICE_H__
#define __TX_ISP_DEVICE_H__

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "txx-isp.h"

enum tx_isp_subdev_id {
	TX_ISP_CORE_SUBDEV_ID,
	TX_ISP_MAX_SUBDEV_ID,
};

#define TX_ISP_ENTITY_ENUM_MAX_DEPTH	16
struct tx_isp_module;
struct tx_isp_irq_device;
struct tx_isp_subdev;
struct tx_isp_subdev_pad;
struct tx_isp_subdev_link;

struct tx_isp_module {
	struct tx_isp_descriptor desc;
	struct device *dev;
	const char *name;
	struct miscdevice miscdev;
	/*struct list_head list;*/

	/* the interface */
	struct file_operations *ops;

	/* the interface */
	struct file_operations *debug_ops;

	/* the list header of sub-modules */
	struct tx_isp_module *submods[TX_ISP_ENTITY_ENUM_MAX_DEPTH];
	/* the module's parent */
	void *parent;
	int (*notify)(struct tx_isp_module *module, unsigned int notification, void *data);
};

/* Description of the connection between modules */
struct link_pad_description {
	char *name; 		// the module name
	unsigned char type;	// the pad type
	unsigned char index;	// the index in array of some pad type
};

struct tx_isp_link_config {
	struct link_pad_description src;
	struct link_pad_description dst;
	unsigned int flag;
};

struct tx_isp_link_configs {
	struct tx_isp_link_config *config;
	unsigned int length;
};

/* The description of module entity */
struct tx_isp_subdev_link {
	struct tx_isp_subdev_pad *source;	/* Source pad */
	struct tx_isp_subdev_pad *sink;		/* Sink pad  */
	struct tx_isp_subdev_link *reverse;	/* Link in the reverse direction */
	unsigned int flag;			/* Link flag (TX_ISP_LINKTYPE_*) */
	unsigned int state;			/* Link state (TX_ISP_MODULE_*) */
};

struct tx_isp_subdev_pad {
	struct tx_isp_subdev *sd;		/* Subdev this pad belongs to */
	unsigned char index;			/* Pad index in the entity pads array */
	unsigned char type;			/* Pad type (TX_ISP_PADTYPE_*) */
	unsigned char links_type;		/* Pad link type (TX_ISP_PADLINK_*) */
	unsigned char state;			/* Pad state (TX_ISP_PADSTATE_*) */
	struct tx_isp_subdev_link link;		/* The active link */
	int (*event)(struct tx_isp_subdev_pad *, unsigned int event, void *data);
	void *priv;
};

struct tx_isp_dbg_register {
	char name[32];
	int sensor_id;
	int type;
	unsigned int size;
	unsigned long long reg;
	unsigned long long val;
};

struct tx_isp_dbg_register_list {
	char name[32];
	int sensor_id;
	int type;
	unsigned int size;
	unsigned long long reg;
	unsigned long long val;
};

struct tx_isp_chip_ident {
	char name[32];
	char *revision;
	unsigned int ident;
};
struct tx_isp_frame_sync {
	int enable;
	int vinum;
};

typedef struct _ldc_opt_ {
	uint32_t width;
	uint32_t height;
	uint32_t y_str;
	uint32_t uv_str;
	uint32_t k1_x;
	uint32_t k2_x;
	uint32_t k1_y;
	uint32_t k2_y;
	uint32_t r2_rep;
	uint32_t len_cofe;
	int16_t y_shift_lut[256];
	int16_t uv_shift_lut[256];
	uint32_t reserve[32];
} tx_isp_ldc_opt;

typedef struct {
	uint32_t mode;
	uint32_t enable;
	uint32_t priority;
	tx_isp_ldc_opt params;
} ldc_init_channel_attr;

typedef enum{
	IMPISP_OPS_MODE_DISABLE = 0,	/**< 不使能该模块功能 */
	IMPISP_OPS_MODE_ENABLE,		/**< 使能该模块功能 */
	IMPISP_OPS_MODE_BUTT,		/**< 用于判断参数的有效性，参数大小必须小于这个值 */
};

typedef struct {
	ldc_init_channel_attr cattr[3];
} ldc_init_attr;

struct tx_isp_initarg;
struct tx_isp_subdev_core_ops {
	int (*g_chip_ident)(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip);
	int (*init)(struct tx_isp_subdev *sd, struct tx_isp_initarg *init);		// clk's, power's and init ops.
	int (*reset)(struct tx_isp_subdev *sd, struct tx_isp_initarg *init);
	int (*fsync)(struct tx_isp_subdev *sd, struct tx_isp_frame_sync *value);
	int (*g_register)(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg);
	int (*g_register_list)(struct tx_isp_subdev *sd, struct tx_isp_dbg_register_list *reg);
	int (*g_register_all)(struct tx_isp_subdev *sd, struct tx_isp_dbg_register_list *reg);
	int (*s_register)(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg);
	int (*ioctl)(struct tx_isp_subdev *sd, unsigned int cmd, void *arg);
	irqreturn_t (*interrupt_service_routine)(struct tx_isp_subdev *sd, u32 status, bool *handled);
	irqreturn_t (*interrupt_service_thread)(struct tx_isp_subdev *sd, void *data);
};

struct tx_isp_subdev_video_ops {
	int (*s_stream)(struct tx_isp_subdev *sd, struct tx_isp_initarg *init);
	int (*link_stream)(struct tx_isp_subdev *sd, struct tx_isp_initarg *init);
	int (*link_setup)(const struct tx_isp_subdev_pad *local,
			  const struct tx_isp_subdev_pad *remote, u32 flags);
};

struct tx_isp_subdev_sensor_ops {
	int (*release_all_sensor)(struct tx_isp_subdev *sd);
	int (*sync_sensor_attr)(struct tx_isp_subdev *sd, void *arg);
	int (*ioctl)(struct tx_isp_subdev *sd, unsigned int cmd, void *arg);
};

struct tx_isp_subdev_pad_ops {
	int (*g_fmt)(struct tx_isp_subdev *sd, struct tisp_format *f);
	int (*s_fmt)(struct tx_isp_subdev *sd, struct tisp_format *f);
	int (*streamon)(struct tx_isp_subdev *sd, void *data);
	int (*streamoff)(struct tx_isp_subdev *sd, void *data);
};

struct tx_isp_irq_device {
	spinlock_t slock[3];
	/*struct mutex mlock;*/
	int irq[3];
	int irq_enabled[3];
	int irq_num;
	void (*enable_irq)(struct tx_isp_irq_device *irq_dev, int vinum);
	void (*disable_irq)(struct tx_isp_irq_device *irq_dev, int vinum);
};

enum tx_isp_module_state {
	TX_ISP_MODULE_UNDEFINE = 0,
	TX_ISP_MODULE_SLAKE,
	TX_ISP_MODULE_ACTIVATE,
	TX_ISP_MODULE_DEINIT = TX_ISP_MODULE_ACTIVATE,
	TX_ISP_MODULE_INIT,
	TX_ISP_MODULE_RUNNING,
};

/*
 * Internal ops. Never call this from drivers, only the tx isp device can call
 * these ops.
 *
 * activate_module: called when this subdev is enabled. When called the module
 * could be operated;
 *
 * slake_module: called when this subdev is disabled. When called the
 *	module couldn't be operated.
 *
 */
struct tx_isp_subdev_internal_ops {
	int (*activate_module)(struct tx_isp_subdev *sd);
	int (*slake_module)(struct tx_isp_subdev *sd);
#ifdef CONFIG_PM
	int (*suspend_module)(struct tx_isp_subdev *sd);
	int (*resume_module)(struct tx_isp_subdev *sd);
#endif
};

struct tx_isp_subdev_ops {
	struct tx_isp_subdev_core_ops		*core;
	struct tx_isp_subdev_video_ops		*video;
	struct tx_isp_subdev_pad_ops		*pad;
	struct tx_isp_subdev_sensor_ops		*sensor;
	struct tx_isp_subdev_internal_ops	*internal;
};

#define tx_isp_call_module_notify(ent, args...)				\
	(!(ent) ? -ENOENT : (((ent)->notify) ?				\
			     (ent)->notify(((ent)->parent), ##args) : -ENOIOCTLCMD))

#define tx_isp_call_subdev_notify(ent, args...)				\
	(!(ent) ? -ENOENT : (((ent)->module.notify) ?			\
			     ((ent)->module.notify(&((ent)->module), ##args)): -ENOIOCTLCMD))

#define tx_isp_call_subdev_event(ent, args...)				\
	(!(ent) ? -ENOENT : (((ent)->event) ?				\
			     (ent)->event((ent), ##args) : -ENOIOCTLCMD))

#define tx_isp_subdev_call(sd, o, f, args...)				\
	(!(sd) ? -ENODEV : (((sd)->ops->o && (sd)->ops->o->f) ?		\
			    (sd)->ops->o->f((sd) , ##args) : -ENOIOCTLCMD))

struct tx_isp_subdev {
	struct tx_isp_module module;
	struct tx_isp_irq_device irqdev;
	struct tx_isp_chip_ident chip;

	/* basic members */
	struct resource *res[3];
	void __iomem *base[3];
	struct clk **clks;
	unsigned int clk_num;
	struct tx_isp_subdev_ops *ops;

	/* expanded members */
	unsigned short num_outpads;			/* Number of sink pads */
	unsigned short num_inpads;			/* Number of source pads */

	struct tx_isp_subdev_pad *outpads;		/* OutPads array (num_pads elements) */
	struct tx_isp_subdev_pad *inpads;		/* InPads array (num_pads elements) */

	void *dev_priv;
	void *host_priv;
};

#define TX_ISP_PLATFORM_MAX_NUM 16

struct tx_isp_platform {
	struct platform_device *dev;
	struct platform_driver *drv;
};

struct tx_isp_device {
	struct tx_isp_module module;
	unsigned int pdev_num;
	struct tx_isp_platform pdevs[TX_ISP_PLATFORM_MAX_NUM];

	char *version;
	struct mutex mlock;
	spinlock_t slock;
	int refcnt;
	int ldc_open;

	int active_link[5];
	/* debug parameters */
	struct proc_dir_entry *proc;
};

#define miscdev_to_module(mdev) (container_of(mdev, struct tx_isp_module, miscdev))
#define module_to_subdev(mod) (container_of(mod, struct tx_isp_subdev, module))
#define irqdev_to_subdev(dev) (container_of(dev, struct tx_isp_subdev, irqdev))
#define module_to_ispdev(mod) (container_of(mod, struct tx_isp_device, module))


#define tx_isp_sd_readl(sd, reg)		\
	tx_isp_readl(((sd)->base[0]), reg)
#define tx_isp_sd_writel(sd, reg, value)		\
	tx_isp_writel(((sd)->base[0]), reg, value)

int private_reset_tx_isp_module(enum tx_isp_subdev_id id);
int tx_isp_reg_set(struct tx_isp_subdev *sd, unsigned int reg, int start, int end, int val);

int tx_isp_subdev_init(struct platform_device *pdev, struct tx_isp_subdev *sd, struct tx_isp_subdev_ops *ops);
void tx_isp_subdev_deinit(struct tx_isp_subdev *sd);
int tx_isp_send_event_to_remote(struct tx_isp_subdev_pad *pad, unsigned int cmd, void *data);

static inline void tx_isp_set_module_nodeops(struct tx_isp_module *module, struct file_operations *ops)
{
	module->ops = ops;
}

static inline void tx_isp_set_module_debugops(struct tx_isp_module *module, struct file_operations *ops)
{
	module->debug_ops = ops;
}

static inline void tx_isp_set_subdev_nodeops(struct tx_isp_subdev *sd, struct file_operations *ops)
{
	tx_isp_set_module_nodeops(&sd->module, ops);
}

static inline void tx_isp_set_subdev_debugops(struct tx_isp_subdev *sd, struct file_operations *ops)
{
	tx_isp_set_module_debugops(&sd->module, ops);
}

static inline void tx_isp_set_subdevdata(struct tx_isp_subdev *sd, void *data)
{
	sd->dev_priv = data;
}

static inline void *tx_isp_get_subdevdata(struct tx_isp_subdev *sd)
{
	return sd->dev_priv;
}

static inline void tx_isp_set_subdev_hostdata(struct tx_isp_subdev *sd, void *data)
{
	sd->host_priv = data;
}

static inline void *tx_isp_get_subdev_hostdata(struct tx_isp_subdev *sd)
{
	return sd->host_priv;
}

extern char ncu_buf_len[255];
extern char wdr_buf_len[255];
extern struct tx_isp_module *g_isp_module;
extern int high_frame_mode_changed;

typedef struct tisp_awb_start{
	unsigned int rgain;
	unsigned int bgain;
} awb_start_t;

int tx_isp_resize(void);
int tx_isp_sensor_register_sensor(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_sensor_set_input(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_sensor_get_input(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_sensor_enum_input(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_get_mdns_buf(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_video_s_stream(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_set_mdns_buf(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_dualsensor_set_buf(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_dualsensor_get_buf(struct tx_isp_module *module, unsigned long arg, int 	kmode);
int tx_isp_dualsensor_mode(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_wdr_set_buf(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_wdr_get_buf(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_wdr_open(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_wdr_enable(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_wdr_disable(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_video_link_setup(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_video_link_stream(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_start_night_mode(struct tx_isp_module *module, unsigned long arg, int kmode);
int tx_isp_set_ldc_enable(struct tx_isp_module *module, unsigned long arg, int kmode);
/**
 * @fn int isp_core_tuning_switch_day_or_night(int day, int vinum);
 *
 * 设置ISP的白天夜视模式
 *
 * @param[in] day  1：白天模式  0：夜晚模式
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int isp_core_tuning_switch_day_or_night(int day, int vinum);

/**
 * @fn int tx_isp_get_ev_start(int vinum, int *value);
 *
 * 获取ISP的EV信息，返回 riscv 收敛后的最终结果
 *
 * @param[out] value riscv收敛后的ev参数结果
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数可在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_get_ev_start(int vinum, int *value);

/**
 * @fn int tx_isp_set_ev_start(int vinum, int enable, int value);
 *
 * 设置ISP的起始EV信息
 *
 * @param[in] value 起始ev
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数建议在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_set_ev_start(int vinum, int enable, int value);

/**
 * @fn int tx_isp_get_ae_luma(uint32_t *luma);
 *
 * 获取图片亮度信息，返回 riscv 收敛后的最终结果
 *
 * @param[out] luma riscv收敛后的亮度结果
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数可在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_get_ae_luma(uint32_t *luma);

/**
 * @fn int tx_isp_get_sensor_again(uint32_t *value);
 *
 * 获取sensor的again信息，返回 riscv 收敛后的最终结果
 *
 * @param[out] value riscv收敛后的最终结果
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数可在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_get_sensor_again(uint32_t *value);

/**
 * @fn int tx_isp_get_sensor_again(uint32_t *value);
 *
 * 设置 ISP 起始的sensor again参数
 *
 * @param[in] value  sensor again 参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数建议在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_set_sensor_again(uint32_t value);

/**
 * @fn int tx_isp_get_sensor_inttime(uint32_t *value);
 *
 * 获取sensor的曝光信息，返回 riscv 收敛后的最终结果
 *
 * @param[out] value riscv收敛后的最终结果
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数可在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_get_sensor_inttime(uint32_t *value);

/**
 * @fn int tx_isp_set_sensor_inttime(uint32_t value);
 *
 * 设置 ISP 起始的sensor 曝光参数
 *
 * @param[in] value  sensor inttime 参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数建议在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_set_sensor_inttime(uint32_t value);

/**
 * @fn int tx_isp_get_isp_gain(uint32_t *value);
 *
 * 获取ISP的dgain信息，返回 riscv 收敛后的最终结果
 *
 * @param[out] value riscv收敛后的最终结果
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数可在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_get_isp_gain(uint32_t *value);

/**
 * @fn int tx_isp_set_isp_gain(uint32_t value);
 *
 * 设置 ISP 起始dgain 参数
 *
 * @param[in] value  isp dgain 参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数建议在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_set_isp_gain(uint32_t value);

/**
 * @fn int tx_isp_get_awb_start(awb_start_t *awb_start);
 *
 * 获取AWB信息，返回 riscv 收敛后的最终结果
 *
 * @param[out] awb_start riscv收敛后的最终结果
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 该函数可在tx_isp_riscv_hf_prepare函数中调用
 */
int tx_isp_get_awb_start(awb_start_t *awb_start);

/**
 * @fn int tx_isp_set_awb_start(awb_start_t awb_start);
 *
 * 设置AWB信息
 *
 * @param[in] awb_start awb 结构体信息
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_set_awb_start(awb_start_t awb_start);


/**
 * @fn int tx_isp_enable_debug(int vinum, int en, int count);
 *
 * 是否开启debug模式
 *
 * @param [in] vinum 0:主摄  1：副摄
 * @param [in] en 0:关闭debug  1：开启debug
 * @param [in] count :需要打印的帧数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 开启debug模式后，每帧的AE、AWB信息将会通过dmesg打印出来，方便调试
 *
 * @attention 该函数可在任意位置调用
 */
int tx_isp_enable_debug(int vinum, int en, int count);

/**
 * @fn int tx_isp_discard_frame(uint32_t frame);
 *
 * 内核丢帧接口
 *
 * @param [frame] :需要丢弃的帧格式
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 最大可控帧数为16帧，支持任意丢帧，frame 填充方式，对应 bit 为 1 则不丢弃，对应bit为0，则丢弃。如 丢弃前面两帧 frame = 0xfffffffc, 如果想丢弃 第一帧和第三帧，frame = 0xfffffffa
 *
 * @attention 该函数建议在tx_isp_riscv_hf_prepare函数中调用或在stream on之前调用即可
 */
int tx_isp_discard_frame(uint32_t frame);
int sec_tx_isp_discard_frame(uint32_t frame);

/**
 * @fn int tx_isp_get_riscv_night_mode(void);
 *
 * 获取小核白天夜视模式
 *
 * @retval 0 白天 1 夜视
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention
 */
int tx_isp_get_riscv_night_mode(void);

/**
 * @fn int tx_isp_disable_sensor_expo(void);
 *
 * 关闭sensor曝光参数设置，ISP统计结果不会通过I2C设置给Sensor
 *
 * @retval 非0 失败，返回错误码
 *
 * @remark
 *
 * @attention
 */
int tx_isp_disable_sensor_expo(void);

/**
 * @fn int tx_isp_enable_sensor_expo(void);
 *
 * 开启sensor曝光参数设置，ISP统计结果通过I2C设置给Sensor
 *
 * @retval 非0 失败，返回错误码
 *
 * @remark 此类接口不调用默认值是开启状态
 *
 * @attention
 */
int tx_isp_enable_sensor_expo(void);
/**
 * @fn int tx_isp_get_riscv_framecount(void);
 *
 * 获取小核riscv跑的帧数
 *
 * @retval
 *
 * @remark 返回值无意义，结果通过出参value传出
 *
 * @attention
 */
int tx_isp_get_riscv_framecount(uint32_t *value); /* For Zeratul */
/**
 * @fn int tx_isp_riscv_is_run(void);
 *
 * 判断小核riscv是否运行
 *
 * @retval 非0表示没有运行，0表示运行
 *
 * @remark
 *
 * @attention
 */
int tx_isp_riscv_is_run(void); /* For Zeratul */
/**
 * @fn int tx_isp_stop_riscv(void);
 *
 * 停止riscv
 *
 * @retval 无意义
 *
 * @remark 调用一次即可，无需判断返回值
 *
 * @attention
 */
int tx_isp_stop_riscv(void);
/**
 * @fn int tx_isp_enable_sensor_expo(void);
 *
 * 判断riscv是否停止
 *
 * @retval 1 已停止，0未停止
 *
 * @remark
 *
 * @attention
 */
int tx_isp_riscv_is_stop(void);

#endif/*__TX_ISP_DEVICE_H__*/
