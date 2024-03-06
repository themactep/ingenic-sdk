#ifndef __SENSOR_FAST_START_COMMON_H__
#define __SENSOR_FAST_START_COMMON_H__

#include <linux/kernel.h>

#define Zeratul_Platform            1
#define TX_ISP_FAST_START           1
#define TX_ISP_NRVBS                2
#define TX_ISP_FRAME_BUFFER_IGNORE_NUM 1    // 顺序丢帧
#define FRAME_CHANNEL_DEF_WIDTH 1920
#define FRAME_CHANNEL_DEF_HEIGHT 1080

#define SENSOR_DAY_FPS 15 // 白天帧率
#define SENSOR_NIGHT_FPS 10 // 夜视帧率

#define SENSOR_HIGH_FRAME_RATE_MODE 1 // Sensor 高帧率小图模式
#define SENSOR_HIGH_FRAME_RATE_MODE_CHANGE_NUM 5 // 小图帧数

#define DEBUG_TTFF(a) printk("TTFF %s:%d %s:%d\n", __func__, __LINE__, (a==NULL? " " : a), *(volatile u32 *)0xb00020B8 * 1024 / 24000 )  //TCU7用于快起计时

#define K_MODE 1
#define U_MODE 0

#define ALIGN_BLOCK_SIZE (1 << 12)
#define ALIGN_ADDR(base) (((base)+((ALIGN_BLOCK_SIZE)-1))&~((ALIGN_BLOCK_SIZE)-1))
#define ALIGN_SIZE(size) (((size)+((ALIGN_BLOCK_SIZE)-1))&~((ALIGN_BLOCK_SIZE)-1))

char * get_sensor_name(void);
int get_sensor_i2c_addr(void);
int get_sensor_width(void);
int get_sensor_height(void);
int get_sensor_wdr_mode(void);

#ifdef CONFIG_BUILT_IN_SENSOR_SETTING
uint8_t *get_sensor_setting(void);
int get_sensor_setting_len(void);
char *get_sensor_setting_date(void);
char *get_sensor_setting_md5(void);
#endif

int init_sensor(void);
void exit_sensor(void);

struct tx_isp_callback_ops {

	/**
	 * 内核期间启动ISP工作
	 * 返回值：0 成功， 1 失败
	 */
	int (*tx_isp_start_device)(struct tx_isp_module *module);

	/**
	 * 内核高帧率小图模式,切换流程
	 * 返回值：0 成功， 1 失败
	 */
	int (*tx_isp_kernel_hf_resize)(void);

	/**
	 * Riscv高帧率小图模式,切换准备
	 * 返回值：0 成功， 1 失败
	 */
	int (*tx_isp_riscv_hf_prepare)(void);

	/**
	 * Riscv高帧率小图模式,切换流程
	 * 返回值：0 成功， 1 失败
	 */
	int (*tx_isp_riscv_hf_resize)(void);

	/**
	 * 帧完成中断回调函数
	 * count: 帧数
	 * 返回值：0 成功， 1 失败
	 * 说明：该函数为中断函数，切勿作复杂处理
	 */
	int (*tx_isp_frame0_done_int_handler)(int count);
};

extern struct tx_isp_callback_ops g_tx_isp_callback_ops;

#endif
