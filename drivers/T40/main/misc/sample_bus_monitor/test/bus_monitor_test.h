#ifndef __MONITOR_TEST_H_
#define __MONITOR_TEST_H_

enum {
    /* AHB chn5 */
    OTG_MASTER  = 0x0,
    UHC_MASTER  = 0x1,
    AES_MASTER  = 0x2,
    SFC_MASTER  = 0x3,
    ETH_MASTER  = 0x4,
    CPU_MASTER  = 0x5,
    HASH_MASTER = 0x6,
    MSC0_MASTER = 0x7,
    MSCX_MASTER = 0x8,
    PDMA_MASTER = 0x9,
    MCU_MASTER  = 0xa,
};

/*
    chn0   tiziano
    chn1   radix
    chn2   ipu, msc0, msc1, i2d, dbox
    chn3   bscaler, lcd, vo
    chn4   el150
    chn6   cpu
*/
enum {
    /* AXI chn2 */
    IPU_ID  = 0x0,
    MSC0_ID = 0x1,
    MSC1_ID = 0x2,
    I2D_ID  = 0x3,
    DBOX_ID = 0x4,
};

enum{
    /* AXI chn3 */
    BSCALER_ID = 0x0,
    LCD_ID     = 0x1,
    VO_ID      = 0x2,
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

#endif
