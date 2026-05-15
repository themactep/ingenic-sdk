#ifndef __MONITOR__H__
#define __MONITOR__H__



#define MONITOR_BASE	0xB30A0000

/* AXI channel 0,1,2,3,4,6 */

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
};

struct monitor_param
{
	unsigned int 		monitor_id;
	unsigned int 		monitor_start;
	unsigned int 		monitor_end;
	unsigned int		monitor_mode;
	unsigned int		monitor_chn;

};


#define JZMONITOR_IOC_MAGIC  'M'
#define IOCTL_MONITOR_START			_IO(JZMONITOR_IOC_MAGIC, 106)
#define IOCTL_MONITOR_RES_PBUFF		_IO(JZMONITOR_IOC_MAGIC, 114)
#define IOCTL_MONITOR_GET_PBUFF		_IO(JZMONITOR_IOC_MAGIC, 115)
#define IOCTL_MONITOR_BUF_LOCK		_IO(JZMONITOR_IOC_MAGIC, 116)
#define IOCTL_MONITOR_BUF_UNLOCK	_IO(JZMONITOR_IOC_MAGIC, 117)
#define IOCTL_MONITOR_BUF_FLUSH_CACHE	_IO(JZMONITOR_IOC_MAGIC, 118)

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
