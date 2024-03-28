#include <linux/debugfs.h>
#include <linux/module.h>
#include <tx-isp-debug.h>
#include <linux/vmalloc.h>

/* -------------------debugfs interface------------------- */
static int print_level = ISP_WARNING_LEVEL;
module_param(print_level, int, S_IRUGO);
MODULE_PARM_DESC(print_level, "isp print level");

static int isp_clk = 100000000;
module_param(isp_clk, int, S_IRUGO);
MODULE_PARM_DESC(isp_clk, "isp clock freq");

extern int isp_ch0_pre_dequeue_time;
module_param(isp_ch0_pre_dequeue_time, int, S_IRUGO);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_time, "isp pre dequeue time, unit ms");

extern int isp_ch0_pre_dequeue_interrupt_process;
module_param(isp_ch0_pre_dequeue_interrupt_process, int, S_IRUGO);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_interrupt_process, "isp pre dequeue interrupt process");

extern int isp_ch0_pre_dequeue_valid_lines;
module_param(isp_ch0_pre_dequeue_valid_lines, int, S_IRUGO);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_valid_lines, "isp pre dequeue valid lines");

extern int isp_ch1_dequeue_delay_time;
module_param(isp_ch1_dequeue_delay_time, int, S_IRUGO);
MODULE_PARM_DESC(isp_ch1_dequeue_delay_time, "isp pre dequeue time, unit ms");

extern int isp_day_night_switch_drop_frame_num;
module_param(isp_day_night_switch_drop_frame_num, int, S_IRUGO);
MODULE_PARM_DESC(isp_day_night_switch_drop_frame_num, "isp day night switch drop frame number");

extern int isp_memopt;
module_param(isp_memopt, int, S_IRUGO);
MODULE_PARM_DESC(isp_memopt, "isp memory optimize");


void private_dma_sync_single_for_device(struct device *dev,
							      dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	dma_sync_single_for_device(dev, addr, size, dir);
	return;
}
