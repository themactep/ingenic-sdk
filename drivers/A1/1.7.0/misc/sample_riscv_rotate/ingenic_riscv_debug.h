#ifndef __INGENIC_RISCV_DEBUG_H__
#define __INGENIC_RISCV_DEBUG_H__

#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/crypto.h>
#include <linux/interrupt.h>
#include <crypto/scatterwalk.h>
#include <crypto/aes.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <soc/base.h>
#ifdef CONFIG_SOC_A1
#include <dt-bindings/interrupt-controller/a1-irq.h>
#include <dt-bindings/dma/ingenic-pdma.h>
#else
#include <soc/irq.h>
#endif
#include <linux/kthread.h>
#include <jz_proc.h>
#include <linux/wait.h>

//#include < linux/irq.h >
//#include < linux/interrupt.h >


#define ROTATE_INFO_LEVEL		(0x0)
#define ROTATE_WARNING_LEVEL	(0x1)
#define ROTATE_ERROR_LEVEL		(0x2)

#define ROTATE_PRINT(level, format, ...) 		\
	rotate_printk(level, format, ##__VA_ARGS__)
#define ROTATE_INFO(...) 		ROTATE_PRINT(ROTATE_INFO_LEVEL, __VA_ARGS__)
#define ROTATE_WARNING(...) 	ROTATE_PRINT(ROTATE_WARNING_LEVEL, __VA_ARGS__)
#define ROTATE_ERROR(...) 		ROTATE_PRINT(ROTATE_ERROR_LEVEL, __VA_ARGS__)

int rotate_printk(unsigned int level, unsigned char *fmt, ...);


typedef enum {
    NV12       		  = 0,
    ARGB1555   	      = 1,
    ARGB8888   	      = 2,
    TYPE_END   		  = 3,
} stream_type_e;


typedef enum {
    RISCV_READY       = 0,
    RISCV_BUSY        = 1,
    RISCV_END         = 2,
} riscv_stat_e;


typedef enum {
	ROTATE0			  = 0,
	ROTATE90		  = 1,
	ROTATE180		  = 2,
	ROTATE270		  = 3,
	ROTATEEND	  	  = 4,
} angle_e;

struct jz_rotate_info{
	int 			index;
	stream_type_e 	type;
	angle_e         angle;
	int 			width;
	int 			height;
	unsigned int 	src;
	unsigned int 	des;
	int   			box_width;
	int   			boxh_height;
};


struct jz_rotate{
	char name[16];
	int irq;
	struct resource *res;
	struct miscdevice misc_dev;       /* miscdevice */
	struct device *dev;
	struct proc_dir_entry 	*proc_root;
	struct proc_dir_entry 	*proc_ipu_info;
	struct jz_rotate_info 	rotate_data;
	wait_queue_head_t btn_wq; //分配等待队列头
	struct completion done;
};





#define JZ_ROTATE_MAGIC			'R'
#define IOCTL_ROTATE_SET_SCALER_TABLE	_IOW(JZ_ROTATE_MAGIC, 4, struct jz_rotate_info)





#endif

