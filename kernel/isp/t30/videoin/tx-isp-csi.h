#ifndef __TX_ISP_CSI_H__
#define __TX_ISP_CSI_H__

#include <tx-isp-common.h>
#include <linux/proc_fs.h>

/* csi host regs, base addr should be defined in board cfg */
#define VERSION				0x00
#define N_LANES				0x04
#define PHY_SHUTDOWNZ		0x08
#define DPHY_RSTZ			0x0C
#define CSI2_RESETN			0x10
#define PHY_STATE			0x14
#define DATA_IDS_1			0x18
#define DATA_IDS_2			0x1C
#define ERR1				0x20
#define ERR2				0x24
#define MASK1				0x28
#define MASK2				0x2C
#define PHY_TST_CTRL0       0x30
#define PHY_TST_CTRL1       0x34

#if (defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21))
#define PHY_CRTL0                0x000
#define PHY_CRTL1                0x080
#define CK_LANE_SETTLE_TIME      0x100
#define CK_LANE_CONFIG           0x128
#define PHY_DT0_LANE_SETTLE_TIME 0x180
#define PHY_DT1_LANE_SETTLE_TIME 0x200
#define PHY_MODEL_SWITCH         0x2CC
#define PHY_LVDS_MODE            0x300
#define PHY_FORCE_MODE           0x34
#endif
typedef enum
{
	ERR_NOT_INIT = 0xFE,
	ERR_ALREADY_INIT = 0xFD,
	ERR_NOT_COMPATIBLE = 0xFC,
	ERR_UNDEFINED = 0xFB,
	ERR_OUT_OF_BOUND = 0xFA,
	SUCCESS = 0
} csi_error_t;

struct tx_isp_csi_device {
	struct tx_isp_subdev sd;
	struct tx_isp_video_in vin;
	int state;
	struct mutex mlock;
	struct resource *phy_res;
	void __iomem *phy_base;
	void * pdata;
	unsigned int lans;
};

#define csi_readl(port,reg)					\
	__raw_readl((unsigned int *)((port)->base + reg))
#define csi_writel(port,reg, value)					\
	__raw_writel((value), (unsigned int *)((port)->base + reg))

#define csi_core_write(csi, addr, value) csi_writel(&((csi)->sd), addr, value)
#define csi_core_read(csi, addr) csi_readl(&((csi)->sd), addr)

#define csi_phy_write(csi, addr, value) __raw_writel((value), (unsigned int *)((csi)->phy_base + addr))
#define csi_phy_read(csi, addr) __raw_readl((unsigned int *)((csi)->phy_base + addr))

#define sd_to_csi_device(sd) (container_of(sd, struct tx_isp_csi_device, sd))
void check_csi_error(void);
#endif /* __TX_ISP_CSI_H__  */
