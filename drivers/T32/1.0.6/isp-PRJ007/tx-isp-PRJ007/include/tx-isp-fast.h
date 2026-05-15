#ifndef __TX_ISP_FAST_H__
#define __TX_ISP_FAST_H__


struct tx_isp_sensor_fast_attr {
	char name[16];
	int i2c_addr;
	int width;
	int height;
	int wdr;
	int (* init_sensor)(void);
	void (* exit_sensor)(void);
};

/*
 * [in]channel: channle
 * [in]frame_num: Snap frame index
 * [in]alloc_size: Frame size
 * [in]width: Frame width
 * [in]height: Frame height
 * [in]src_kpaddr: Frame qbuf addr
 * [out]kpaddr: Frame phyaddr
 * [out]kvaddr: Frame viraddr
 *
 * */
struct fast_frame_info{
	int vinum;
	int channel;
	int frame_num;
	unsigned int alloc_size;
	unsigned int width;
	unsigned int height;
	unsigned int kpaddr;
	void *kvaddr;
	void *src_kpaddr;
};
struct fast_frame_list{
	struct fast_frame_info info;
	struct list_head flist;
	bool busy;
};

#endif /* __TX_ISP_FAST_H__  */
