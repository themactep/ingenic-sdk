#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/fs.h>

#include "include/fast_start_common.h"
#include "include/tx-isp-frame-channel.h"
#include "include/tx-isp-device.h"
#include "include/tx-isp-tuning.h"

#define IR_STATUS_ON "on"
#define IR_STATUS_OFF "off"

enum IR_STATUS{
    IR_STATUS_DAY = 0,  //白天模式
    IR_STATUS_NIGHT = 1,//晚上模式
    IR_STATUS_LED = 2,  //白光灯模式
    IR_STATUS_IGNORE=3,
};
extern int fast_wdr_mode;
static char *g_ir_status[] = {"day","night","led","ignore"};

// 声明全局变量
extern unsigned long rmem_base; /* rmem 内存基地址 */
extern unsigned long high_framerate_kernel_mode_en;

int riscv_is_run = 1;
int riscv_is_pass = 1;
extern int tx_isp_riscv_is_run();

uint32_t g_riscv_isp_ev = 0;
uint32_t g_riscv_sensor_again = 0;
uint32_t g_riscv_sensor_inttime = 0;
uint32_t g_riscv_isp_gain = 0;
uint32_t g_riscv_luma = 0;
static awb_start_t g_awb_start;
static int g_is_night = IR_STATUS_DAY;
struct isp_buf_info g_ncu_buf;
struct isp_buf_info g_wdr_buf;

#define GPIO_IRCUT_N -1//GPIO_PB(28)
#define GPIO_IRCUT_P -1//GPIO_PB(18)

static int ircut_gpio_init(void)
{
	int ret = 0;

	if(GPIO_IRCUT_N > 0) {
		ret = gpio_request_one(GPIO_IRCUT_N, GPIOF_OUT_INIT_LOW | GPIOF_EXPORT, "IR_N");
		if(ret < 0) {
			printk("gpio_request_one GPIO_IRCUT_N failed\n");
			return -1;
		}
		gpio_direction_output(GPIO_IRCUT_N, 0);
	}

	if(GPIO_IRCUT_P > 0) {
		ret = gpio_request_one(GPIO_IRCUT_P, GPIOF_OUT_INIT_LOW | GPIOF_EXPORT, "IR_P");
		if(ret < 0) {
			printk("gpio_request_one GPIO_IRCUT_P failed\n");
			return -1;
		}

		gpio_direction_output(GPIO_IRCUT_P, 0);
	}

	return 0;
}

static ssize_t ir_fops_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned int count = (unsigned int)size;
    unsigned int len = (strlen(g_ir_status[g_is_night] + *ppos) < size) ? (strlen(g_ir_status[g_is_night] + *ppos)) : size;
    ssize_t ret;

    ret = copy_to_user(buf, (void *)(g_ir_status[g_is_night] + *ppos), len);
    if(ret != 0) {
        return ret;
    }

    *ppos += len;

    return len;
}

static const struct file_operations ir_status_proc_fops ={
    .read = ir_fops_read,
};


static int frame_channel_fast_start(unsigned int addr)
{
	int ret = 0;
	struct tx_isp_frame_channel *chan=NULL;
	struct frame_image_format format;
	struct v4l2_requestbuffers req;
	int count = 0;
	int buf_i = 0;
	enum v4l2_buf_type type;
	printk("TTFF %s %d W:%d H:%d N:%d\n", __func__, __LINE__, (int)frame_channel_width, (int)frame_channel_height, (int)frame_channel_nrvbs);

	// 如果内核开启高帧率小图，则关闭之前的通道
	if(high_framerate_kernel_mode_en) {
		ret = frame_channel_release(NULL, NULL);
		if(ret != 0) {
			printk(KERN_ERR "frame_channel_release failed\n");
		}
	}

	// 打开新的通道
	ret = frame_channel_open(NULL, NULL);
	if(ret != 0) {
		printk(KERN_ERR "frame_channel_open failed\n");
	}

	chan = IS_ERR_OR_NULL(g_f0_mdev) ? NULL : miscdev_to_frame_chan(g_f0_mdev);
	if(IS_ERR_OR_NULL(chan)){
		printk(KERN_ERR "chan is null\n");
	}

	// 设置通道格式，分辨率，裁剪缩放等

	memset(&format, 0x0, sizeof(struct frame_image_format));

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.pix.field = V4L2_FIELD_ANY;
	if(frame_channel_width) {
		format.pix.width = frame_channel_width;
		format.crop_width = frame_channel_width;
		format.scaler_out_width = frame_channel_width;
	} else {
		format.pix.width = FRAME_CHANNEL_DEF_WIDTH;
		format.crop_width = FRAME_CHANNEL_DEF_WIDTH;
		format.scaler_out_width = FRAME_CHANNEL_DEF_WIDTH;
	}

	if(frame_channel_height) {
		format.pix.height = frame_channel_height;
		format.crop_height = frame_channel_height;
		format.scaler_out_height = frame_channel_height;
	} else {
		format.pix.height = FRAME_CHANNEL_DEF_HEIGHT;
		format.crop_height = FRAME_CHANNEL_DEF_HEIGHT;
		format.scaler_out_height = FRAME_CHANNEL_DEF_HEIGHT;
	}
	format.pix.pixelformat = V4L2_PIX_FMT_NV12;
	format.crop_enable = 0;
	format.crop_top = 0;
	format.crop_left = 0;
	format.scaler_enable = 1;
	format.pix.colorspace = V4L2_COLORSPACE_SRGB;
	format.rate_bits = 0;
	format.rate_mask = 1;

	ret = frame_channel_vidioc_set_fmt(chan, (unsigned long)&format, K_MODE);
	if(ret != 0) {
		printk(KERN_ERR "frame_channel_vidioc_set_fmt failed\n");
	}

	// 设置通道buffer

	memset(&req, 0, sizeof(struct v4l2_requestbuffers));
	if(frame_channel_nrvbs) {
		req.count = frame_channel_nrvbs; /*nrVBs*/
	} else {
		req.count = TX_ISP_NRVBS; /*nrVBs*/
	}
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	ret = frame_channel_reqbufs(chan, (unsigned long)&req, K_MODE);
	if(ret != 0) {
		printk(KERN_ERR "frame_channel_reqbufs failed\n");
	}


	if(frame_channel_nrvbs) {
		count = frame_channel_nrvbs; /*nrVBs*/
	} else {
		count = TX_ISP_NRVBS; /*nrVBs*/
	}
	ret = frame_channel_set_channel_banks(chan, (unsigned long)&count, K_MODE);
	if(ret != 0) {
		printk(KERN_ERR "frame_channel_set_channel_banks failed\n");
	}


	for(buf_i = 0; buf_i < count; buf_i++) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(struct v4l2_buffer));

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;
		buf.index = buf_i;
		buf.m.userptr = addr + buf_i * (frame_channel_width * ((frame_channel_height + 0xF) & ~0xF) * 3 / 2);
		buf.length = (frame_channel_width * ((frame_channel_height + 0xF) & ~0xF) * 3 / 2);

		ret = frame_channel_vb2_qbuf(chan, (unsigned long)&buf, K_MODE);
		if(ret != 0) {
			printk(KERN_ERR "frame_channel_vb2_qbuf failed\n");
		}
	}


	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	// 启动出流
	ret = frame_channel_vb2_streamon(chan, (unsigned long)&type, K_MODE);
	if(ret != 0) {
		printk(KERN_ERR "frame_channel_vb2_streamon failed\n");
	}

	return 0;
}


/**
 * 高帧率小图RISCV模式切换函数
 * 读取 小图AE、AWB信息
 * 停掉小核
 */
static int tx_isp_riscv_hf_prepare(void)
{
	int ret = 0;
	int timeout = 200;
	uint32_t frame_count = 0;
	uint32_t riscv_stable = 0;

	DEBUG_TTFF("tx_isp_stop_riscv start");

	ret = tx_isp_riscv_is_run();
	if (ret < 0) {
		riscv_is_run = 0;
	} else {

//等待riscv固定帧数
#if 1
		timeout = 1000;
		while(timeout--) {
			ret = tx_isp_get_riscv_framecount(&frame_count);
			if(ret < 0) {
				printk("Error: %s tx_isp_get_riscv_framecount failed ret:%d\n", __func__, ret);
				return ret;
			}

			if((frame_count >= 6) && (frame_count != -1))
				break;

			mdelay(1);
		}

		DEBUG_TTFF("wait 8 frame done");
#endif

//等待riscv稳定，不在意帧数
#if 0
		timeout = 1000;
		while(timeout--) {
			ret = tx_isp_get_riscv_stable(4,&riscv_stable);
			if(ret < 0) {
				printk("Error: %s tx_isp_get_riscv_stable failed ret:%d\n", __func__, ret);
				return ret;
			}

			if(riscv_stable == 1) 
				break;

			mdelay(1);
		}

#endif
		ret = tx_isp_stop_riscv();
		if(ret < 0) {
			printk("Error: %s tx_isp_stop_riscv failed ret:%d\n", __func__, ret);
			return ret;
		}

		timeout = 1000;
		while(!tx_isp_riscv_is_stop() && timeout--) {
			mdelay(1);
		}
		DEBUG_TTFF("tx_isp_stop_riscv done");

		if(timeout <= 0) {
			printk("stop riscv timeout\n");
			riscv_is_pass = 0;
		}
	}

	ret = tx_isp_get_sensor_again(&g_riscv_sensor_again);
	if(ret < 0) {
		printk("Error: %s tx_isp_get_sensor_again failed\n", __func__);
		return ret;
	}

	ret = tx_isp_get_sensor_inttime(&g_riscv_sensor_inttime);
	if(ret < 0) {
		printk("Error: %s tx_isp_get_sensor_inttime failed\n", __func__);
		return ret;
	}

	ret = tx_isp_get_isp_gain(&g_riscv_isp_gain);
	if(ret < 0) {
		printk("Error: %s tx_isp_get_isp_gain failed\n", __func__);
		return ret;
	}

	ret = tx_isp_get_ae_luma(&g_riscv_luma);
	if(ret < 0) {
		printk("Error: %s tx_isp_get_ae_luma failed\n", __func__);
		return ret;
	}

	ret = tx_isp_get_ev_start(&g_riscv_isp_ev);
	if(ret < 0) {
		printk("Error: %s g_riscv_isp_ev failed\n", __func__);
		return ret;
	}

	printk("Current sensor again:%d isp gain:%d inttime:%d luma:%d ev:%d\n", g_riscv_sensor_again, g_riscv_isp_gain, g_riscv_sensor_inttime, g_riscv_luma, g_riscv_isp_ev);

	ret = tx_isp_get_awb_start(&g_awb_start);
	if(ret < 0) {
		printk("Error: %s tx_isp_get_awb_start failed ret:%d\n", __func__, ret);
		return ret;
	}

	printk("Current awb rgain:%d bgain:%d\n", g_awb_start.rgain, g_awb_start.bgain);

	ret = tx_isp_get_riscv_framecount(&frame_count);
	if(ret < 0) {
		printk("Error: %s tx_isp_get_riscv_framecount failed ret:%d\n", __func__, ret);
		return ret;
	}

	printk("Riscv frame count:%d\n", frame_count);

	g_is_night = tx_isp_get_riscv_night_mode();
    if(g_is_night > IR_STATUS_IGNORE){
        g_is_night = IR_STATUS_IGNORE;
    }
    printk("Tuning mode is:%d\n", g_is_night);
    //g_riscv_sensor_again = 4096;
	//g_riscv_sensor_inttime = 1242;

	// Set Sensor Attr (again、inttime)
	//tx_isp_set_ev_start(g_riscv_isp_ev);
	//tx_isp_set_sensor_again(g_riscv_sensor_again);
	//tx_isp_set_sensor_inttime(g_riscv_sensor_inttime);
	//tx_isp_set_isp_gain(4096);

	tx_isp_discard_frame(0xfffffffc); /* 丢弃前两帧 */

	// enable debug info per frame
	//tx_isp_enable_debug(1, 50);

	return 0;
}

static int tx_isp_riscv_hf_resize(void)
{
	struct tx_isp_module *module = g_isp_module;
	struct tx_isp_module * submod = NULL;
	struct tx_isp_subdev *sd;
	int ret = 0;
	int mode = 0;
	struct tx_isp_device *ispdev = NULL;
	struct tx_isp_subdev *subdev = NULL;
	struct tx_isp_sensor_register_info sensor_register_info;
	int index = 0;
	int input = 0;
	int link = 0;
	int on = 0;

	if(g_isp_module == NULL){
		printk("Error: %s module is NULL\n", __func__);
		return 0;
	}

	submod = module->submods[0];
	sd = module_to_subdev(submod);
	ispdev = module_to_ispdev(g_isp_module);

	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		submod = module->submods[index];
		if(submod){
			subdev = module_to_subdev(submod);
			ret = tx_isp_subdev_call(subdev, internal, activate_module);
			if(ret && ret != -ENOIOCTLCMD)
				break;
		}
	}
	if (fast_wdr_mode == 1) {
		tx_isp_wdr_enabele(module, 1);
	}

	// Register sensor
	memset(&sensor_register_info, 0, sizeof(struct tx_isp_sensor_register_info));
	strcpy(sensor_register_info.name, get_sensor_name());
	sensor_register_info.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
	strcpy(sensor_register_info.i2c.type, get_sensor_name());
	sensor_register_info.i2c.addr = get_sensor_i2c_addr();

	ret = tx_isp_sensor_register_sensor(module,(unsigned long)&sensor_register_info, K_MODE);
	if(ret < 0) {
		printk("tx_isp_sensor_register_sensor failed\n");
		return ret;
	}

	// Set Input
	ret = tx_isp_sensor_set_input(module, (unsigned long)&input, K_MODE);
	if(ret < 0) {
		printk("tx_isp_sensor_set_input failed\n");
		return ret;
	}

	// Set NCU buf
	ret = tx_isp_get_buf(module, (unsigned long)&g_ncu_buf, K_MODE);
	if(ret < 0) {
		printk("tx_isp_get_buf failed\n");
		return ret;
	}
	g_ncu_buf.paddr = rmem_base;

	printk("NCU: size = %d  paddr = 0x%x\n", g_ncu_buf.size, g_ncu_buf.paddr);
	sprintf(ncu_buf_len,"len=%d", g_ncu_buf.size);
	ret = tx_isp_set_buf(module, (unsigned long)&g_ncu_buf, K_MODE);
	if(ret < 0) {
		printk("tx_isp_set_buf failed\n");
		return ret;
	}

	if (fast_wdr_mode == 1) {
		// Set WDR buf
		ret = tx_isp_wdr_get_buf(module, (unsigned long)&g_wdr_buf, K_MODE);
		if(ret < 0) {
			printk("tx_isp_wdr_get_buf failed\n");
			return ret;
		}
		g_wdr_buf.paddr = rmem_base + ALIGN_SIZE(g_ncu_buf.size);

		printk("WDR: size = %d  paddr = 0x%x\n", g_wdr_buf.size, g_wdr_buf.paddr);
		sprintf(wdr_buf_len,"len=%d", g_wdr_buf.size);
		ret = tx_isp_wdr_set_buf(module, (unsigned long)&g_wdr_buf, K_MODE);
		if(ret < 0) {
			printk("tx_isp_wdr_set_buf failed\n");
			return ret;
		}
	}

	on = 1;
//	tx_isp_tuning_set_hldc_on_or_off(on); /*设置畸变矫正开关 0:关  1:开*/

//	if(g_riscv_isp_gain > 1024) {
	//	tx_isp_tuning_set_sharpness(50); /* 有效 */
	//	tx_isp_tuning_set_sinter_strength(255); /* 有效 */
//	}

	// set start AWB
	//tx_isp_set_awb_start(g_awb_start); /* 有效 */

	//tx_isp_tuning_set_brightness(255); /* 有效 */
	//tx_isp_tuning_set_contrast(255); /* 有效 */
	//tx_isp_tuning_set_sharpness(50); /* 有效 */
	//tx_isp_tuning_set_saturation(255); /* 有效 */
	//tx_isp_tuning_set_ae_it_max(100); /* SetFPS 之后被刷新 */
	//tx_isp_tuning_set_hv_flip(ISP_CORE_FLIP_HV_MODE); /* 有效 */
	//tx_isp_tuning_set_max_again(1024);/* SetFPS 之后被刷新 */
	//tx_isp_tuning_set_max_dgain(1024); /* 有效 */

	//tx_isp_tuning_set_max_dgain(126); /* 有效 */

	// 切换白天夜视模式
	//isp_core_tuning_switch_day_or_night(0); /* 有效 */

	//tx_isp_tuning_set_sinter_strength(255); /* 有效 */

	//设置ISP抗闪频参数
	tx_isp_tuning_set_flicker(IMPISP_ANTIFLICKER_50HZ); /* 有效 */


	// 切换白天夜视模式
	if(g_is_night == IR_STATUS_NIGHT) {
		isp_core_tuning_switch_day_or_night(0);
	}

	// IRCUT 切回idle状态
	ircut_gpio_init();
	// 创建 ir_status 节点
	proc_create("ir_status", S_IRUGO, NULL, &ir_status_proc_fops);

	// Stream on
	ret = tx_isp_video_s_stream(module, 1);
	if(ret < 0) {
		printk("tx_isp_video_s_stream failed\n");
		return ret;
	}

	ispdev->active_link = -1;
	ret = tx_isp_video_link_setup(module, (unsigned long)&link, K_MODE);
	if(ret < 0) {
		printk("tx_isp_video_link_setup failed\n");
		return ret;
	}

	ret = tx_isp_video_link_stream(module, 1);
	if(ret < 0) {
		printk("tx_isp_video_link_setup failed\n");
		return ret;
	}

	if (fast_wdr_mode == 1) {
		frame_channel_fast_start(g_wdr_buf.paddr + ALIGN_SIZE(g_wdr_buf.size));
	}else{
		frame_channel_fast_start(g_ncu_buf.paddr + ALIGN_SIZE(g_ncu_buf.size));
	}

	return 0;
}

static int tx_isp_frame_done_int_handler(int count)
{
	if(count == 1) {
		tx_isp_tuning_set_fps(15, 1); /* 最终实现在中断下文中实现 */
	}

	return 0;
}

struct tx_isp_callback_ops g_tx_isp_callback_ops = {
	.tx_isp_riscv_hf_prepare = tx_isp_riscv_hf_prepare,
	.tx_isp_riscv_hf_resize = tx_isp_riscv_hf_resize,
	.tx_isp_frame_done_int_handler = tx_isp_frame_done_int_handler,
};
