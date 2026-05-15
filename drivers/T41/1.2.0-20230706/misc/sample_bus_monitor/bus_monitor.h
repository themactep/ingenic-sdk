#ifndef __MONITOR__H__
#define __MONITOR__H__

#define MONITOR_VERSION "20220805a"

#define MONITOR_BASE	0x130A0000

/* AXI channel 0,1,2,3,4,6 */

#define MONITOR_TOP_REST(n)                            (n * 0x0400 + 0x3FC)   //W
#define MONITOR_CHKID_FUN_EN(n)	                     (n * 0x0400 + 0x000)   //RW
#define MONITOR_CHKID_FUN_CLR(n)                         (n * 0x0400 + 0x004)   //W
#define MONITOR_CHKID_FUN_MODE(n)	                 (n * 0x0400 + 0x008)   //RW
#define MONITOR_CHKID_INT_EN(n)	                     (n * 0x0400 + 0x010)   //RW
#define MONITOR_CHKID_INT_CLR(n)	                 (n * 0x0400 + 0x014)   //W
#define MONITOR_CHKID_INT_STATE(n)	                     (n * 0x0400 + 0x018)   //R
#define MONITOR_AWID_VALUE(n)	                     (n * 0x0400 + 0x020)   //RW
#define MONITOR_ARID_VALUE(n)	                     (n * 0x0400 + 0x024)   //RW
#define MONITOR_WID_VALUE(n)	                     (n * 0x0400 + 0x028)   //RW
#define MONITOR_RID_VALUE(n)	                     (n * 0x0400 + 0x02C)   //RW
#define MONITOR_BID_VALUE(n)	                     (n * 0x0400 + 0x030)   //RW
#define MONITOR_AWID_ZONE0_NUM(n)	                 (n * 0x0400 + 0x040)   //R
#define MONITOR_AWID_ZONE1_NUM(n)	                 (n * 0x0400 + 0x044)   //R
#define MONITOR_AWID_ZONE2_NUM(n)	                 (n * 0x0400 + 0x048)   //R
#define MONITOR_ARID_ZONE0_NUM(n)	                 (n * 0x0400 + 0x060)   //R
#define MONITOR_ARID_ZONE1_NUM(n)	                 (n * 0x0400 + 0x064)   //R
#define MONITOR_ARID_ZONE2_NUM(n)	                 (n * 0x0400 + 0x068)   //R
#define MONITOR_WID_ZONE0_NUM(n)	                 (n * 0x0400 + 0x080)   //R
#define MONITOR_WID_ZONE1_NUM(n)	                 (n * 0x0400 + 0x084)   //R
#define MONITOR_WID_ZONE2_NUM(n)	                 (n * 0x0400 + 0x088)   //R
#define MONITOR_RID_ZONE0_NUM(n)	                 (n * 0x0400 + 0x0A0)   //R
#define MONITOR_RID_ZONE1_NUM(n)	                 (n * 0x0400 + 0x0A4)   //R
#define MONITOR_RID_ZONE2_NUM(n)	                 (n * 0x0400 + 0x0A8)   //R
#define MONITOR_BID_ZONE0_NUM(n)	                 (n * 0x0400 + 0x0C0)   //R
#define MONITOR_BID_ZONE1_NUM(n)	                 (n * 0x0400 + 0x0C4)   //R
#define MONITOR_BID_ZONE2_NUM(n)	                 (n * 0x0400 + 0x0C8)   //R
#define MONITOR_CHKADDR_FUN_EN(n)	                 (n * 0x0400 + 0x100)   //RW
#define MONITOR_CHKADDR_FUN_CLR(n)	                 (n * 0x0400 + 0x104)   //W
#define MONITOR_CHKADDR_FUN_MODE(n)	                 (n * 0x0400 + 0x108)   //RW
#define MONITOR_CHKADDR_INT_EN(n)	                 (n * 0x0400 + 0x110)   //RW
#define MONITOR_CHKADDR_INT_CLR(n)	                 (n * 0x0400 + 0x114)   //W
#define MONITOR_CHKADDR_INT_STATE(n)                 (n * 0x0400 + 0x118)   //R
#define MONITOR_AWADDR_VALUE0(n)	                 (n * 0x0400 + 0x120)   //RW
#define MONITOR_AWADDR_VALUE1(n)	                 (n * 0x0400 + 0x124)   //RW
#define MONITOR_ARADDR_VALUE0(n)	                 (n * 0x0400 + 0x140)   //RW
#define MONITOR_ARADDR_VALUE1(n)	                 (n * 0x0400 + 0x144)   //RW
#define MONITOR_AWAADDR_ZONE0_NUM(n)                 (n * 0x0400 + 0x160)   //R
#define MONITOR_AWAADDR_ZONE1_NUM(n)                 (n * 0x0400 + 0x164)   //R
#define MONITOR_AWAADDR_ZONE2_NUM(n)                 (n * 0x0400 + 0x168)   //R
#define MONITOR_ARAADDR_ZONE0_NUM(n)                 (n * 0x0400 + 0x180)   //R
#define MONITOR_ARAADDR_ZONE1_NUM(n)                 (n * 0x0400 + 0x184)   //R
#define MONITOR_ARAADDR_ZONE2_NUM(n)                 (n * 0x0400 + 0x188)   //R
#define MONITOR_CHKVALS_FUN_EN(n)                       (n * 0x0400 + 0x200)   //RW
#define MONITOR_CHKVALS_FUN_CLR(n)                      (n * 0x0400 + 0x204)   //W
#define MONITOR_CHKVALS_FUN_MODE(n)                     (n * 0x0400 + 0x208)   //RW
#define MONITOR_CHKVALS_FUN_STATE(n)                    (n * 0x0400 + 0x20C)   //R
#define MONITOR_CHKVALS_INT_EN(n)                       (n * 0x0400 + 0x210)   //RW
#define MONITOR_CHKVALS_INT_CLR(n)                      (n * 0x0400 + 0x214)   //W
#define MONITOR_CHKVALS_INT_STATE(n)                    (n * 0x0400 + 0x218)   //R
#define MONITOR_AWVAL_CLK_NUM(n)                        (n * 0x0400 + 0x220)   //R    AWVAL_SEQ_NUM
#define MONITOR_AWVAL_VLD_NUM(n)                        (n * 0x0400 + 0x224)   //R    AWVAL_RDY_NUM
#define MONITOR_AWVAL_RDY_NUM(n)                        (n * 0x0400 + 0x228)   //R
#define MONITOR_AWVAL_HIT_NUM(n)                        (n * 0x0400 + 0x22C)   //R
#define MONITOR_ARVAL_CLK_NUM(n)                        (n * 0x0400 + 0x230)   //R    AWVAL_SEQ_NUM
#define MONITOR_ARVAL_VLD_NUM(n)                        (n * 0x0400 + 0x234)   //R    AWVAL_RDY_NUM
#define MONITOR_ARVAL_RDY_NUM(n)                        (n * 0x0400 + 0x238)   //R
#define MONITOR_ARVAL_HIT_NUM(n)                        (n * 0x0400 + 0x23C)   //R
#define MONITOR_WVAL_CLK_NUM(n)                         (n * 0x0400 + 0x240)   //R
#define MONITOR_WVAL_VLD_NUM(n)                         (n * 0x0400 + 0x244)   //R
#define MONITOR_WVAL_RDY_NUM(n)                         (n * 0x0400 + 0x248)   //R
#define MONITOR_WVAL_HIT_NUM(n)                         (n * 0x0400 + 0x24C)   //R
#define MONITOR_RVAL_CLK_NUM(n)                         (n * 0x0400 + 0x250)   //R
#define MONITOR_RVAL_VLD_NUM(n)                         (n * 0x0400 + 0x254)   //R
#define MONITOR_RVAL_RDY_NUM(n)                         (n * 0x0400 + 0x258)   //R
#define MONITOR_RVAL_HIT_NUM(n)                         (n * 0x0400 + 0x25C)   //R
#define MONITOR_BVAL_CLK_NUM(n)                         (n * 0x0400 + 0x260)   //R
#define MONITOR_BVAL_VLD_NUM(n)                         (n * 0x0400 + 0x264)   //R
#define MONITOR_BVAL_RDY_NUM(n)                         (n * 0x0400 + 0x268)   //R
#define MONITOR_BVAL_HIT_NUM(n)                         (n * 0x0400 + 0x26C)   //R
#define MONITOR_CHKVALM_FUN_EN(n)                       (n * 0x0400 + 0x280)   //RW
#define MONITOR_CHKVALM_FUN_CLR(n)                      (n * 0x0400 + 0x284)   //W
#define MONITOR_CHKVALM_INT_EN(n)                       (n * 0x0400 + 0x2A0)   //RW
#define MONITOR_CHKVALM_INT_CLR(n)                      (n * 0x0400 + 0x2A4)   //W
#define MONITOR_CHKVALM_INT_STATE(n)                    (n * 0x0400 + 0x2A8)   //R
#define MONITOR_AWVAL_THRES(n)                          (n * 0x0400 + 0x2C0)   //RW
#define MONITOR_ARVAL_THRES(n)                          (n * 0x0400 + 0x2C4)   //RW
#define MONITOR_WVAL_THRES(n)                           (n * 0x0400 + 0x2C8)   //RW
#define MONITOR_RVAL_THRES(n)                           (n * 0x0400 + 0x2CC)   //RW
#define MONITOR_BVAL_THRES(n)                           (n * 0x0400 + 0x2D0)   //RW
#define MONITOR_AWVAL_MAX_NUM(n)                        (n * 0x0400 + 0x2E0)   //R
#define MONITOR_ARVAL_MAX_NUM(n)                        (n * 0x0400 + 0x2E4)   //R
#define MONITOR_WVAL_MAX_NUM(n)                         (n * 0x0400 + 0x2E8)   //R
#define MONITOR_RVAL_MAX_NUM(n)                         (n * 0x0400 + 0x2EC)   //R
#define MONITOR_BVAL_MAX_NUM(n)                         (n * 0x0400 + 0x2F0)   //R
#define MONITOR_CHKLAST_FUN_EN(n)                       (n * 0x0400 + 0x300)   //RW
#define MONITOR_CHKLAST_FUN_CLR(n)                      (n * 0x0400 + 0x304)   //W
#define MONITOR_CHKLAST_FUN_MODE(n)                     (n * 0x0400 + 0x308)   //RW
#define MONITOR_CHKLAST_INT_EN(n)                       (n * 0x0400 + 0x310)   //RW
#define MONITOR_CHKLAST_INT_CLR(n)                      (n * 0x0400 + 0x314)   //W
#define MONITOR_CHKLAST_INT_STATE(n)                    (n * 0x0400 + 0x318)   //R
#define MONITOR_WLAST_ID(n)                             (n * 0x0400 + 0x320)   //RW  -----2022/10/19
#define MONITOR_RLAST_ID(n)                             (n * 0x0400 + 0x324)   //RW  -----
#define MONITOR_WLAST_THRES(n)                          (n * 0x0400 + 0x330)   //RW
#define MONITOR_RLAST_THRES(n)                          (n * 0x0400 + 0x334)   //RW
#define MONITOR_WLEN_ZONE0_NUM(n)                       (n * 0x0400 + 0x340)   //R
#define MONITOR_WLEN_ZONE1_NUM(n)                       (n * 0x0400 + 0x344)   //R
#define MONITOR_WLEN_ZONE2_NUM(n)                       (n * 0x0400 + 0x348)   //R
#define MONITOR_RLEN_ZONE0_NUM(n)                       (n * 0x0400 + 0x360)   //R
#define MONITOR_RLEN_ZONE1_NUM(n)                       (n * 0x0400 + 0x364)   //R
#define MONITOR_RLEN_ZONE2_NUM(n)                       (n * 0x0400 + 0x368)   //R
#define MONITOR_AWVAL_CLK_ID_NUM(n)                     (n * 0x0400 + 0x370)   //R
#define MONITOR_AWVAL_VLD_ID_NUM(n)                     (n * 0x0400 + 0x374)   //R
#define MONITOR_AWVAL_RDY_ID_NUM(n)                     (n * 0x0400 + 0x378)   //R
#define MONITOR_AWVAL_HIT_ID_NUM(n)                     (n * 0x0400 + 0x37C)   //R
#define MONITOR_ARVAL_CLK_ID_NUM(n)                     (n * 0x0400 + 0x380)   //R
#define MONITOR_ARVAL_VLD_ID_NUM(n)                     (n * 0x0400 + 0x384)   //R
#define MONITOR_ARVAL_RDY_ID_NUM(n)                     (n * 0x0400 + 0x388)   //R
#define MONITOR_ARVAL_HIT_ID_NUM(n)                     (n * 0x0400 + 0x38C)   //R
#define MONITOR_WVAL_CLK_ID_NUM(n)                      (n * 0x0400 + 0x390)   //R
#define MONITOR_WVAL_VLD_ID_NUM(n)                      (n * 0x0400 + 0x394)   //R
#define MONITOR_WVAL_RDY_ID_NUM(n)                      (n * 0x0400 + 0x398)   //R
#define MONITOR_WVAL_HIT_ID_NUM(n)                      (n * 0x0400 + 0x39C)   //R
#define MONITOR_RVAL_CLK_ID_NUM(n)                      (n * 0x0400 + 0x3A0)   //R
#define MONITOR_RVAL_VLD_ID_NUM(n)                      (n * 0x0400 + 0x3A4)   //R
#define MONITOR_RVAL_RDY_ID_NUM(n)                      (n * 0x0400 + 0x3A8)   //R
#define MONITOR_RVAL_HIT_ID_NUM(n)                      (n * 0x0400 + 0x3AC)   //R
#define MONITOR_BVAL_CLK_ID_NUM(n)                      (n * 0x0400 + 0x3B0)   //R
#define MONITOR_BVAL_VLD_ID_NUM(n)                      (n * 0x0400 + 0x3B4)   //R
#define MONITOR_BVAL_RDY_ID_NUM(n)                      (n * 0x0400 + 0x3B8)   //R
#define MONITOR_BVAL_HIT_ID_NUM(n)                      (n * 0x0400 + 0x3BC)   //R

/* AHB monitor unit */
#define MONITOR_AHB_TOP_RESET(n)                        (n * 0x0400 + 0x380)   //W
#define MONITOR_AHB_MST_WR_00_NUM(n)                    (n * 0x0400 + 0x040)   //R
#define MONITOR_AHB_MST_WR_01_NUM(n)                    (n * 0x0400 + 0x044)   //R
#define MONITOR_AHB_MST_WR_02_NUM(n)                    (n * 0x0400 + 0x048)   //R
#define MONITOR_AHB_MST_WR_03_NUM(n)                    (n * 0x0400 + 0x04C)   //R
#define MONITOR_AHB_MST_WR_04_NUM(n)                    (n * 0x0400 + 0x050)   //R
#define MONITOR_AHB_MST_WR_05_NUM(n)                    (n * 0x0400 + 0x054)   //R
#define MONITOR_AHB_MST_WR_06_NUM(n)                    (n * 0x0400 + 0x058)   //R
#define MONITOR_AHB_MST_WR_07_NUM(n)                    (n * 0x0400 + 0x05C)   //R
#define MONITOR_AHB_MST_WR_08_NUM(n)                    (n * 0x0400 + 0x060)   //R
#define MONITOR_AHB_MST_WR_09_NUM(n)                    (n * 0x0400 + 0x064)   //R
#define MONITOR_AHB_MST_WR_10_NUM(n)                    (n * 0x0400 + 0x068)   //R
#define MONITOR_AHB_MST_WR_11_NUM(n)                    (n * 0x0400 + 0x06C)   //R
#define MONITOR_AHB_MST_WR_12_NUM(n)                    (n * 0x0400 + 0x070)   //R
#define MONITOR_AHB_MST_WR_13_NUM(n)                    (n * 0x0400 + 0x074)   //R
#define MONITOR_AHB_MST_WR_14_NUM(n)                    (n * 0x0400 + 0x078)   //R
#define MONITOR_AHB_MST_WR_15_NUM(n)                    (n * 0x0400 + 0x07C)   //R
#define MONITOR_AHB_MST_RD_00_NUM(n)                    (n * 0x0400 + 0x080)   //R
#define MONITOR_AHB_MST_RD_01_NUM(n)                    (n * 0x0400 + 0x084)   //R
#define MONITOR_AHB_MST_RD_02_NUM(n)                    (n * 0x0400 + 0x088)   //R
#define MONITOR_AHB_MST_RD_03_NUM(n)                    (n * 0x0400 + 0x08C)   //R
#define MONITOR_AHB_MST_RD_04_NUM(n)                    (n * 0x0400 + 0x090)   //R
#define MONITOR_AHB_MST_RD_05_NUM(n)                    (n * 0x0400 + 0x094)   //R
#define MONITOR_AHB_MST_RD_06_NUM(n)                    (n * 0x0400 + 0x098)   //R
#define MONITOR_AHB_MST_RD_07_NUM(n)                    (n * 0x0400 + 0x09C)   //R
#define MONITOR_AHB_MST_RD_08_NUM(n)                    (n * 0x0400 + 0x0A0)   //R
#define MONITOR_AHB_MST_RD_09_NUM(n)                    (n * 0x0400 + 0x0A4)   //R
#define MONITOR_AHB_MST_RD_10_NUM(n)                    (n * 0x0400 + 0x0A8)   //R
#define MONITOR_AHB_MST_RD_11_NUM(n)                    (n * 0x0400 + 0x0AC)   //R
#define MONITOR_AHB_MST_RD_12_NUM(n)                    (n * 0x0400 + 0x0B0)   //R
#define MONITOR_AHB_MST_RD_13_NUM(n)                    (n * 0x0400 + 0x0B4)   //R
#define MONITOR_AHB_MST_RD_14_NUM(n)                    (n * 0x0400 + 0x0B8)   //R
#define MONITOR_AHB_MST_RD_15_NUM(n)                    (n * 0x0400 + 0x0BC)   //R


#define MONITOR_CTRL	            (0x00)   /* WR */
#define MONITOR_SLOT_CYC_NUM        (0x04)   /* WR */
#define MONITOR_RD_WR_ID	        (0x08)   /* WR */
#define MONITOR_BUF_LIMIT_CYC	    (0x0C)   /* WR */
#define MONITOR_RD_CYC_CNT	        (0x10)   /* R */
#define MONITOR_RD_CMD_CNT	        (0x14)   /* R */
#define MONITOR_RD_ARVALID_CNT	    (0x18)   /* R */
#define MONITOR_RD_RVALID_CNT	    (0x1C)   /* R */
#define MONITOR_RD_DATA_CNT	        (0x20)   /* R */
#define MONITOR_RD_LAST_CNT	        (0x24)   /* R */
#define MONITOR_RD_DA_TO_LAST_CNT	(0x28)   /* R */
#define MONITOR_WR_CYC_CNT	        (0x2C)   /* R */
#define MONITOR_WR_CMD_CNT	        (0x30)   /* R */
#define MONITOR_WR_AWVALID_CNT	    (0x34)   /* R */
#define MONITOR_WR_WVALID_CNT	    (0x38)   /* R */
#define MONITOR_WR_DATA_CNT	        (0x3C)   /* R */
#define MONITOR_WR_LAST_CNT	        (0x40)   /* R */
#define MONITOR_WR_BRESP_CNT	    (0x44)   /* R */
#define MONITOR_WR_DA_TO_LAST_CNT	(0x48)   /* R */
#define MONITOR_WATCH_ADDRL	        (0x50)   /* WR */
#define MONITOR_WATCH_ADDRH	        (0x54)   /* WR */
#define MONITOR_WATCH_MATCH_WRID	(0x60)   /* R */
#define MONITOR_WATCH_MATCH_WADDR	(0x64)   /* R */
#define MONITOR_WATCH_MATCH_ARADDR	(0x68)   /* R */
#define MONITOR_INT_STA	            (0x70)   /* R */
#define MONITOR_INT_MASK	        (0x74)   /* WR */
#define MONITOR_INT_CLR	            (0x78)   /* WR */


/* AHB channel 5 */
#define AHB_MON_CTRL                (0x500)  /* WR */
#define AHB_MON_WATCH_ADDRL         (0x504)  /* WR */
#define AHB_MON_WATCH_ADDRH         (0x508)  /* WR */
#define AHB_MON_WATCH_MATCH_MAS     (0x510)  /* R */
#define AHB_MON_WATCH_MATCH_WADDR   (0x514)  /* R */
#define AHB_MON_WATCH_MATCH_RADDR   (0x518)  /* R */
#define AHB_MON_INT_STA             (0x570)  /* R */
#define AHB_MON_INT_MASK            (0x574)  /* WR */
#define AHB_MON_INT_CLR             (0x578)  /* WR */




#define u32 unsigned int

struct monitor_reg_struct {
	char *name;
	unsigned int addr;
};

struct monitor_param
{
	unsigned int 		monitor_id;
	unsigned int 		monitor_start;
	unsigned int 		monitor_end;
	unsigned int		monitor_mode;
	unsigned int		monitor_chn;
	unsigned int		monitor_fun_mode;

};

struct jz_monitor {

	int irq;
	char name[16];

	struct clk *clk;
	struct clk *ahb0_gate;
	void __iomem *iomem;
	struct device *dev;
	struct resource *res;
	struct miscdevice misc_dev;

	struct mutex mutex;
	struct completion done_monitor;
	struct completion done_buf;

    struct monitor_param iparam;
};

enum {
    MONITOR_CHK_ID  = 0x0,
    MONITOR_CHK_ADDR,
    MONITOR_ID_ADDR,
    MONITOR_VALS,
    MONITOR_VALM,
    MONITOR_LAST,
};

enum {
    CHECK_AXI_ID = 0x1,
    CHECK_AXI_ADDR,
    CHECK_AXI_VALS,
    CHECK_AHB_ID,
    CHECK_AHB_ADDR,
    CHECK_AHB_VALS,
};

enum {
    MONITOR_LCDC  = 0x0,
    MONITOR_LDC,
    MONITOR_AIP,
    MONITOR_ISP, //3
    MONITOR_IVDC_W,
    MONITOR_IPU,
    MONITOR_MSC0,
    MONITOR_MSC1,
    MONITOR_I2D, //8
    MONITOR_DRAWBOX,
    MONITOR_JILOO,
    MONITOR_EL,
    MONITOR_IVDC_R,
    MONITOR_CPU, //13
    MONITOR_USB, 
    MONITOR_DMIC, 
    MONITOR_AES, 
    MONITOR_SFC0, 
    MONITOR_GMAC, //18
    MONITOR_AHB_BRI, 
    MONITOR_HASH, 
    MONITOR_SFC1, 
    MONITOR_PWM, 
    MONITOR_PDMA, //23
    MONITOR_CPU_LEP, 
};

#define JZMONITOR_IOC_MAGIC  'M'
#define IOCTL_MONITOR_START			_IO(JZMONITOR_IOC_MAGIC, 106)
#define IOCTL_MONITOR_RES_PBUFF		_IO(JZMONITOR_IOC_MAGIC, 114)
#define IOCTL_MONITOR_GET_PBUFF		_IO(JZMONITOR_IOC_MAGIC, 115)
#define IOCTL_MONITOR_DUMP_REG		_IO(JZMONITOR_IOC_MAGIC, 116)
#define IOCTL_MONITOR_BUF_UNLOCK	_IO(JZMONITOR_IOC_MAGIC, 117)
#define IOCTL_MONITOR_BUF_FLUSH_CACHE	_IO(JZMONITOR_IOC_MAGIC, 118)
#define IOCTL_MONITOR_FUNC_CLEAR      _IO(JZMONITOR_IOC_MAGIC, 119)

#define CHN_NUM_LCDC        (1)
#define CHN_NUM_LDC         (2)
#define CHN_NUM_AIP         (0)
#define CHN_NUM_ISP         (0)
#define CHN_NUM_IVDC_W      (1)
#define CHN_NUM_IPU         (0)
#define CHN_NUM_MSC0        (1)
#define CHN_NUM_MSC1        (2)
#define CHN_NUM_I2D         (3)
#define CHN_NUM_DRAWBOX     (4)
#define CHN_NUM_JILOO       (0)
#define CHN_NUM_EL          (1)
#define CHN_NUM_IVDC_R      (2)
#define CHN_NUM_CPU         (0)


#define CHN_LCDC(n)        ((n) << 5)
#define CHN_LDC(n)         ((n) << 5)
#define CHN_AIP(n)         ((n) << 4)
#define CHN_ISP(n)         ((n) << 5)
#define CHN_IVDC_W(n)      ((n) << 5)
#define CHN_IPU(n)         ((n) << 5)
#define CHN_MSC0(n)        ((n) << 5)
#define CHN_MSC1(n)        ((n) << 5)
#define CHN_I2D(n)         ((n) << 5)
#define CHN_DRAWBOX(n)     ((n) << 5)
#define CHN_JILOO(n)       ((n) << 6)
#define CHN_EL(n)          ((n) << 6)
#define CHN_IVDC_R(n)      ((n) << 6)
#define CHN_CPU(n)         ((n) << 4)


/* MONITOR_CTRL*/
#define CLK_EN          (1 << 31)
#define SOFT_RESET      (1 << 30)
#define FUNC_EN         (1 << 29)
#define MODE_EN_WATCH   (1 << 18)
#define MODE_EN_MON     (1 << 17)
#define ID_MODE         (1 << 16)
#define NO_ID_MODE      (0 << 16)
#define CNT_CAPTURE     (1 << 2)
#define CNT_CLEAR       (1 << 1)
#define CNT_LOCK        (1 << 0)

/* MONITOR_SLOT_CYC_NUM */
#define SLOT_CYC_NUM    (0)

/* MONITOR_RD_WR_ID */
#define WRITE_ID_EN (1 << 31)
#define WRITE_ID    (16)
#define READ_ID_EN  (1 << 15)
#define READ_ID     (0)

/* MONITOR_BUF_LIMIT_CYC */
#define BUF_LIMIT_CYC       (0)

/* MONITOR_RD_CYC_CNT */
#define RD_CYC_CNT       (0)

/* MONITOR_RD_CMD_CNT */
#define RD_CMD_CNT            (0)

/* MONITOR_RD_ARVALID_CNT */
#define RD_ARVALID_CNT       (0)

/* MONITOR_RD_RVALID_CNT */
#define RD_RVALID_CNT      (0)

/* MONITOR_RD_DATA_CNT */
#define RD_DATA_CNT       (0)

/* MONITOR_RD_LAST_CNT */
#define RD_LAST_CNT       (0)

/* MONITOR_RD_DA_TO_LAST_CNT */
#define RD_DA_TO_LAST_CNT       (0)


/* MONITOR_WR_CYC_CNT */
#define WR_CYC_CNT       (0)

/* MONITOR_WR_CMD_CNT */
#define WR_CMD_CNT            (0)

/* MONITOR_WR_AWVALID_CNT */
#define WR_AWVALID_CNT       (0)

/* MONITOR_WR_WVALID_CNT */
#define WR_WVALID_CNT      (0)

/* MONITOR_WR_DATA_CNT */
#define WR_DATA_CNT       (0)

/* MONITOR_WR_LAST_CNT */
#define WR_LAST_CNT       (0)

/* MONITOR_WR_DA_TO_LAST_CNT */
#define WR_DA_TO_LAST_CNT       (0)

/* MONITOR_WR_BRESP_CNT */
#define WR_BRESP_CNT      (0)

/* MONITOR_WATCH_ADDRL */
#define WATCH_ADDRL      (0)

/* MONITOR_WATCH_ADDRH */
#define WATCH_ADDRLH      (0)

/* MONITOR_WATCH_MATCH_WRID */
#define WATCH_MATCH_WRID       (0)

/* MONITOR_WATCH_MATCH_WADDR */
#define WATCH_MATCH_WADDR      (0)

/* MONITOR_WATCH_MATCH_ARADDR */
#define WATCH_MATCH_ARADDR       (0)

/* IRQ_STA */
#define AWR_MATCH            (1 << 6)
#define ARD_MATCH            (1 << 5)
#define TIME_SLOT_REACH      (1 << 4)
#define CAP_WR_CNT_FULL      (1 << 3)
#define CAP_RD_CNT_FULL      (1 << 2)
#define CAP_WR_BURST_ERROR   (1 << 1)
#define CAP_RD_BURST_ERROR   (1 << 0)


/* IRQ_CLEAR */
#define AWR_MATCH_CLR            (1 << 6)
#define ARD_MATCH_CLR            (1 << 5)
#define TIME_SLOT_REACH_CLR      (1 << 4)
#define CAP_WR_CNT_FULL_CLR      (1 << 3)
#define CAP_RD_CNT_FULL_CLR      (1 << 2)
#define CAP_WR_BURST_ERROR_CLR   (1 << 1)
#define CAP_RD_BURST_ERROR_CLR   (1 << 0)


/* IRQ_MASK */
#define AWR_MATCH_MASK              (1 << 6)
#define ARD_MATCH_MASK              (1 << 5)
#define TIME_SLOT_REACH_MASK        (1 << 4)
#define CAP_WR_CNT_FULL_MASK        (1 << 3)
#define CAP_RD_CNT_FULL_MASK        (1 << 2)
#define CAP_WR_BURST_ERROR_MASK     (1 << 1)
#define CAP_RD_BURST_ERROR_MASK     (1 << 0)


/* AHB_MON_CTRL*/
#define AHB_CLK_EN         (1 << 31)
#define AHB_SOFT_RESET     (1 << 30)
#define AHB_FUNC_EN        (1 << 29)
#define AHB_STATE_CLR      (1 << 28)
#define AHB_WATCH_HMASTER  (12)
#define AHB_WATCH_EN       (1 << 1)
#define AHB_ID_MODE        (1 << 0)

/* AHB_MON_WATCH_ADDRL*/
#define AHB_WATCH_ADDRL    (0)

/* AHB_MON_WATCH_ADDRH*/
#define AHB_WATCH_ADDRH    (0)

/* AHB_MON_WATCH_MATCH_MAS */
#define AHB_W_HMASTER_MATCH   (15)
#define AHB_R_HMASTER_MATCH   (0)

/* AHB_MON_WATCH_MATCH_WADDR*/
#define AHB_WATCH_MATCH_WADDR  (0)

/* AHB_MON_WATCH_MATCH_RADDR*/
#define AHB_WATCH_MATCH_RADDR  (0)

/* AHB_MON_INT_STA */
#define AHB_AWR_MATCH          (1 << 1)
#define AHB_ARD_MATCH          (1 << 0)

/* AHB_MON_INT_MASK */
#define AHB_AWR_MATCH_MASK      (1 << 1)
#define AHB_ARD_MATCH_MASK      (1 << 0)

/* AHB_MON_INT_CLR */
#define AHB_AWR_MATCH_CLR      (1 << 1)
#define AHB_ARD_MATCH_CLR      (1 << 0)



static inline unsigned int reg_read(struct jz_monitor *jzmonitor, int offset)
{
	return readl(jzmonitor->iomem + offset);
}

static inline void reg_write(struct jz_monitor *jzmonitor, int offset, unsigned int val)
{
	writel(val, jzmonitor->iomem + offset);
}


#define __start_monitor(reg)			reg_bit_set(monitor, reg, FUNC_EN)
#define __reset_monitor(reg)			reg_bit_set(monitor, reg, SOFT_RESET)

#define __start_monitor_ahb()			reg_bit_set(monitor, AHB_MON_CTRL, AHB_FUNC_EN)
#define __reset_monitor_ahb()			reg_bit_set(monitor, AHB_MON_CTRL, AHB_SOFT_RESET)

#endif
