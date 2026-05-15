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


#endif /* __TX_ISP_FAST_H__  */
