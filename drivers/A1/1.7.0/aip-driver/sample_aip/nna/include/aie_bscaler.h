/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : aie_bscaler.h
 * Authors    : yzhai@aram.ic.jz.com
 * Create Time: 2019-11-14:15:39:28
 * Description:
 *
 */

#ifndef __AIE_BSCALER_H__
#define __AIE_BSCALER_H__

#include "stdint.h"
#include "aie_mmap.h"

#define AIE_BSCALER_FRMT_CTRL           (32*4)
#define AIE_BSCALER_FRMT_TASK           (33*4)
#define AIE_BSCALER_FRMT_SRC_Y          (34*4)
#define AIE_BSCALER_FRMT_SRC_C          (35*4)
#define AIE_BSCALER_FRMT_SIZE           (36*4)
#define AIE_BSCALER_FRMT_SRC_STRD       (37*4)
#define AIE_BSCALER_FRMT_DST            (38*4)
#define AIE_BSCALER_FRMT_DST_STRD       (39*4)
#define AIE_BSCALER_PARAM0              (40*4)
#define AIE_BSCALER_PARAM1              (41*4)
#define AIE_BSCALER_PARAM2              (42*4)
#define AIE_BSCALER_PARAM3              (43*4)
#define AIE_BSCALER_PARAM4              (44*4)
#define AIE_BSCALER_FRMT_BUS            (60*4)

/* color space conversion: NV12 -> BGRa */
inline void __aie_bscaler_init_csc(uint32_t src_y, uint32_t src_c,
                   uint32_t src_stride, uint32_t dst_stride,
                   uint16_t h, uint16_t w) {
    const uint16_t C00 = 0x4ad, C01 = 0x669, C02 = 0x0;
    const uint16_t C10 = 0x4ad, C11 = 0x344, C12 = 0x193;
    const uint16_t C20 = 0x4ad, C21 = 0x0, C22 = 0x81a;
    const uint8_t OFST_Y = 16, OFST_C = 128;

    uint32_t bscaler_io_vbase = __aie_get_bscaler_io_vbase();

    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_PARAM0) = (C00 | C01<<16);
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_PARAM1) = (C02 | C10<<16);
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_PARAM2) = (C11 | C12<<16);
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_PARAM3) = (C20 | C21<<16);
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_PARAM4) = (C22 | OFST_Y<<16 | OFST_C<<24);

    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_FRMT_SRC_Y) = __aie_get_ddr_paddr(src_y);
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_FRMT_SRC_C) = __aie_get_ddr_paddr(src_c);
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_FRMT_SIZE) = (w | h<<16);
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_FRMT_SRC_STRD) = src_stride;
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_FRMT_DST_STRD) = dst_stride;
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_FRMT_CTRL) = 0x1;
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_FRMT_BUS) = 0x0;
}

inline void __aie_bscaler_run_csc_rolling(uint32_t dst, uint16_t len) {
    uint32_t bscaler_io_vbase = __aie_get_bscaler_io_vbase();
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_FRMT_DST) = dst;
    *(volatile int *)(bscaler_io_vbase+AIE_BSCALER_FRMT_TASK) = (len<<16 | 0x1);
}

inline void __aie_bscaler_wait_csc() {
}

#endif /* __AIE_BSCALER_H__ */

