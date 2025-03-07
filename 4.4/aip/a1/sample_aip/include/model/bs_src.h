#ifndef __BSCALER_A1_H__
#define __BSCALER_A1_H__

#include <stdio.h>
#include <stdlib.h>
#include "bscaler_api.h"
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif
typedef u_int32_t uint32_t;
typedef u_int16_t uint16_t;
typedef u_int8_t uint8_t;
using namespace std;

struct Mat_c{
	int rows;
	int cols;
	int channels;
	uint8_t *base;
};

int warpAffine(Mat_c src,Mat_c dst,double *M);
int warpPerspective(Mat_c src,Mat_c dst,double *M);
int compare_bgra(uint8_t *dst_ptr,uint8_t *dst_opencv_ptr,int rows,int cols,int channels);
int compare_nv12(uint8_t *dst_ptr,uint8_t *dst_opencv_ptr,int rows,int cols);
int nv12_bgra(uint8_t* src_y, uint8_t* src_uv, int in_height,
		int in_width,int iline_size,uint8_t* dst,
		int oline_size, bs_data_format_e order);
void yuv2rgb(uint8_t y, uint8_t u, uint8_t v,const uint32_t *coef,
		const uint8_t *offset,const uint8_t nv2bgr_alpha,
		bs_data_format_e order, uint8_t *rgba);
int resize_nv12_nv12(uint8_t *src_ptr,int src_w,int src_h,uint8_t *dst_ptr,int dst_w,int dst_h);
int opencv_resize(uint8_t *src_base,int src_w,int src_h,int src_chn,uint8_t *dst_base,int dst_w,int dst_h,int dst_chn);/*bgr->bgr*/

int iwarpPerspective(uint8_t *src,uint8_t *dst,int src_w,int src_h,int src_chn,int dst_w,int dst_h,int dst_chn,double *M);
int iwarpAffine(uint8_t *src,uint8_t *dst,int src_w,int src_h,int src_chn,int dst_w,int dst_h,int dst_chn,double *M);
int iwarpAffine_nv12_nv12(uint8_t *src,uint8_t *dst,int src_w,int src_h,int dst_w,int dst_h,double *M);
#ifdef __cplusplus
}
#endif
#endif /* __A1_H__ */
