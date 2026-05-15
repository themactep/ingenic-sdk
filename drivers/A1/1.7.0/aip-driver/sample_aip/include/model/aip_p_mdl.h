/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : aip_p_mdl.h
 * Authors    : hjiang@abel.ic.jz.com
 * Create Time: 2020-11-19:17:33:10
 * Description:
 *
 */

#ifndef __AIP_P_MDL_H__
#define __AIP_P_MDL_H__
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include "bscaler_api.h"

typedef struct AIP_P_CFG_S {
    bool                        debug_en;
    uint32_t                    bw; //0: 1byte; 1: 2bytes; 2: nv12; 3:4byte;
    uint8_t                     *src_ybase;
    uint8_t                     *src_cbase;
    uint32_t                    src_stride;
    uint8_t                     *dst_base;
    uint32_t                    dst_stride;
    uint32_t                    dw; //dst width
    uint32_t                    dh; //dst height
    uint32_t                    sw; //src width
    uint32_t                    sh; //src height
    uint32_t                    dummy_val; //NOTE: nv12: [7:0],Y; [23:16],U; [31:24],v;
    int64_t                     ax;
    int64_t                     bx;
    int64_t                     cx;
    int64_t                     ay;
    int64_t                     by;
    int64_t                     cy;
    int64_t                     az;
    int64_t                     bz;
    int64_t                     cz;
    int32_t                     param[9];
    uint8_t                     ofst[2];
    uint8_t                     alpha;
    uint32_t                    order;

} AIP_P_CFG_S;

void aip_p_debug_point(int dx, int dy, int dc);
int aip_p_dump(FILE *fp, char *s, char *p, char size, int h, int w, int stride);
int64_t aip_p_clip(int64_t val, int64_t min_val, int64_t max_val);
uint8_t aip_p_bilinear_interplotion(uint8_t src0, uint8_t src1,
		uint8_t src2, uint8_t src3,
		uint16_t sx_wgt, uint16_t sy_wgt);
void aip_p_mdl(AIP_P_CFG_S *s);

void aip_p_model_affine(const data_info_s *src,
		const int box_num, const data_info_s *dst,
		const box_affine_info_s *boxes,
		const uint32_t *coef, const uint32_t *offset);
void aip_p_model_perspective(const data_info_s *src,
		const int box_num, const data_info_s *dst,
		const box_affine_info_s *boxes,
		const uint32_t *coef, const uint32_t *offset);

#ifdef __cplusplus
}
#endif
#endif
