#ifndef __DWU_TEST_H_
#define __DWU_TEST_H_


#define CPU_ID     (0x60)
#define EL150_ID   (0x40)
#define BSCALER_ID (0x30)
#define LCD_ID     (0x31)
#define VO_ID      (0x32)
#define IPU_ID     (0x20)
#define MSC0_ID    (0x21)
#define MSC1_ID    (0x22)
#define I2D_ID     (0x23)
#define DBOX_ID    (0x24)
#define RADIX_ID   (0x10)
#define ISP_ID     (0x00)
#define OTG_ID     (0x50)
#define AES_ID     (0x52)
#define SFC_ID     (0x53)
#define ETH_ID     (0x54)
#define CPU_ID_B   (0x55)
#define HASH_ID    (0x56)
#define MSC0_ID_B  (0x57)
#define MSCX_ID_B  (0x58)
#define PDMA_ID    (0x59)
#define MCU_ID     (0x5a)


struct dwu_param
{
	unsigned int 		dwu_id0;
	unsigned int 		dwu_id1;
	unsigned int 		dwu_id2;
	unsigned int 		dwu_id3;
	unsigned int 		dwu_start_addr;
	unsigned int 		dwu_end_addr;
	unsigned int 		dwu_err_addr;
	unsigned int		dwu_dev_num;

};


#define JZDWU_IOC_MAGIC  'U'
#define IOCTL_DWU_START			_IO(JZDWU_IOC_MAGIC, 106)
#define IOCTL_DWU_RES_PBUFF		_IO(JZDWU_IOC_MAGIC, 114)
#define IOCTL_DWU_GET_PBUFF		_IO(JZDWU_IOC_MAGIC, 115)
#define IOCTL_DWU_BUF_LOCK		_IO(JZDWU_IOC_MAGIC, 116)
#define IOCTL_DWU_BUF_UNLOCK	_IO(JZDWU_IOC_MAGIC, 117)
#define IOCTL_DWU_BUF_FLUSH_CACHE	_IO(JZDWU_IOC_MAGIC, 118)

#endif
