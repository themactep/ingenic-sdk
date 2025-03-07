/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : aip_f_mdl.h
 * Authors    : hjiang@abel.ic.jz.com
 * Create Time: 2020-11-19:17:33:10
 * Description:
 *
 */

#ifndef __AIP_F_MDL_H__
#define __AIP_F_MDL_H__
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "bscaler_api.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
	AIP_F_Y                     = 0,
	AIP_F_old_C                 = 1,
	AIP_F_BGRA                  = 2,
	AIP_F_FMU2_CHN32            = 3,
	AIP_F_FMU4_CHN32            = 4,
	AIP_F_FMU8_CHN32            = 5,
	AIP_F_FMU8_CHN16            = 6,
	AIP_F_new_C                 = 7,
} aip_f_mode_e;

typedef struct {
	int32_t                     scale_x;
	int32_t                     scale_y;
	int32_t                     trans_x;
	int32_t                     trans_y;
	uint8_t                     *src_base;
	uint8_t                     *src_cbase;
	uint8_t                     *dst_base;
	uint8_t                     *dst_cbase;
	int16_t                     src_w;
	int16_t                     src_h;
	int16_t                     dst_w;
	int16_t                     dst_h;
	int16_t                     src_stride;
	int16_t                     dst_stride;
	uint8_t                     bitmode;

} aip_f_cfg_s;

void aip_f_debug_point(int dx, int dy, int dc);

void aip_f_mdl(aip_f_cfg_s *cfg);

void aip_f_model(const data_info_s *src,
                    const int box_num, const data_info_s *dst,
                    const box_resize_info_s *boxes);

#ifdef __cplusplus
}
#endif
#endif
