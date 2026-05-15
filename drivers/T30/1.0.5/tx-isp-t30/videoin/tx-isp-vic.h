#ifndef __TX_ISP_VIC_H__
#define __TX_ISP_VIC_H__

#include <tx-isp-common.h>
#include <tx-vic-regs.h>
#include <tx-lfb-regs.h>
/*#include <linux/seq_file.h>*/
#include <linux/proc_fs.h>
/*#include <jz_proc.h>*/
struct tx_isp_vic_device {
	struct tx_isp_subdev sd;
	struct tx_isp_video_in vin;
	int state;
	void * pdata;
	spinlock_t slock;
	struct mutex mlock;
	int irq_state;

	unsigned int snap_paddr;
	void* snap_vaddr;
	struct completion snap_comp;
	struct mutex snap_mlock;
	unsigned int vic_frd_c;

};

#define tx_isp_vic_readl(port,reg)						\
	__raw_readl((volatile unsigned int *)((port)->sd.base + reg))
#define tx_isp_vic_writel(port,reg, value)					\
	__raw_writel((value), (volatile unsigned int *)((port)->sd.base + reg))
#define sd_to_vic_driver(sd) (container_of(sd, struct tx_isp_vic_device, sd))
void check_vic_error(void);

void isp_lfb_reset(void);
void isp_lfb_ctrl_flb_enable(int on);
void isp_lfb_ctrl_hw_recovery(int on);
void isp_lfb_ctrl_select_fr(void);
void isp_lfb_ctrl_select_ds1(void);
void isp_lfb_ctrl_output_ncu(void);
void isp_lfb_ctrl_output_mscaler(void);
void isp_lfb_ctrl_vsync_wait(int on);
void isp_lfb_ctrl_ncu_to_mscaler(void);
void isp_lfb_ctrl_ncu_to_ddr(void);
void isp_lfb_restart(void);
void isp_lfb_config_default_dma(unsigned int base, unsigned int uv_offset, int lineoffset, int bank_id);
void isp_lfb_config_resolution(unsigned int width, unsigned int height);
volatile unsigned int isp_lfb_read_error_reg(void);

void tx_vic_enable_irq(int enable);
void tx_vic_disable_irq(int enable);
#endif /* __TX_ISP_VIC_H__ */
