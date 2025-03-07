#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/timeb.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
#include "bs_src.h"
#define SHIFT 26//matrix Fixed bit perspective
#define SHIFT_AFFINE 16//matrix Fixed bit affine
#include "mxu_common.hpp"
#define CLIP(val, min, max) ((val) < (min) ? (min) : (val) > (max) ? (max) : (val))
#define BS_CSCA         (20) // Color Space Conversion Accuracy
#define INTER_BIT 5
#define INTER_BITS 5
#define MAT_ACC 15
const int INTER_REMAP_COEF_SCALE = 1<<MAT_ACC;

using namespace std;

const int INTER_RESIZE_COEF_BITS=11;
const int INTER_RESIZE_COEF_SCALE=1 << INTER_RESIZE_COEF_BITS;

static inline void bilinear_bound_check(short x, short y, uint16_t w, uint16_t h,
		bool *out_bound)
{
	int16_t x0 = x;
	int16_t x1 = x + 1;
	int16_t y0 = y;
	int16_t y1 = y + 1;
	uint16_t last_x = w - 1;
	uint16_t last_y = h - 1;
	out_bound[0] = ((x0 < 0) || (x0 > last_x) ||
			(y0 < 0) || (y0 > last_y)) ? true : false;
	out_bound[1] = ((x1 < 0) || (x1 > last_x) ||
			(y0 < 0) || (y0 > last_y)) ? true : false;
	out_bound[2] = ((x0 < 0) || (x0 > last_x) ||
			(y1 < 0) || (y1 > last_y)) ? true : false;
	out_bound[3] = ((x1 < 0) || (x1 > last_x) ||
			(y1 < 0) || (y1 > last_y)) ? true : false;
}

static inline void bilinear_bound_check_uv(short x0, short x1,short y0,short y1,int w,int h,
		bool *out_bound)
{
	x0 = x0&0xfffe;
	x1 = x1&0xfffe;
	y0 = y0 / 2;
	y1 = y1 / 2;
	uint16_t last_x = w - 1;
	uint16_t last_y = (h/2) - 1;
	out_bound[0] = ((x0 < 0) || (x0 > last_x) ||
			(y0 < 0) || (y0 > last_y)) ? true : false;
	out_bound[1] = ((x1 < 0) || (x1 > last_x) ||
			(y0 < 0) || (y0 > last_y)) ? true : false;
	out_bound[2] = ((x0 < 0) || (x0 > last_x) ||
			(y1 < 0) || (y1 > last_y)) ? true : false;
	out_bound[3] = ((x1 < 0) || (x1 > last_x) ||
			(y1 < 0) || (y1 > last_y)) ? true : false;
}

int warpAffine(Mat_c src,Mat_c dst,double *M)
{
	int AB_BITS = 10>INTER_BIT ? 10:INTER_BIT;
	int AB_SCALE = 1<<AB_BITS;
	int INTER_TAB_SIZE = 1<<INTER_BIT;
	int round_data = AB_SCALE/INTER_TAB_SIZE/2;
	int HALF_ONE = 1<<(MAT_ACC - 1);
	//convert matrix
	double D = M[0]*M[4] - M[1]*M[3];
	D = D != 0 ? 1./D : 0;
	double A11 = M[4]*D, A22=M[0]*D;
	M[0] = A11; M[1] *= -D;
	M[3] *= -D; M[4] = A22;
	double b1 = -M[0]*M[2] - M[1]*M[5];
	double b2 = -M[3]*M[2] - M[4]*M[5];
	M[2] = b1; M[5] = b2;
	bool out_bound[4];
	int zero_point = 0;
	const int BLOCK_SZ = 64;
	int bh0 = std::min(BLOCK_SZ/2, dst.rows);
	int bw0 = std::min(BLOCK_SZ*BLOCK_SZ/bh0, dst.cols);
	bh0 = std::min(BLOCK_SZ*BLOCK_SZ/bw0, dst.rows);
	uint8_t *src_ptr = src.base;
	uint8_t *dst_ptr = dst.base;
	for(int y=0;y<dst.rows;y=y+bh0)
	{
		for(int x=0;x<dst.cols;x=x+bw0)
		{
			int bw = std::min( bw0, dst.cols - x);
			int bh = std::min( bh0, dst.rows - y);
			for(int y1=0;y1<bh;y1++)
			{
				int X0 = saturate_cast<int>((M[1]*(y+y1) + M[2])*AB_SCALE + round_data);
				int Y0 = saturate_cast<int>((M[4]*(y+y1) + M[5])*AB_SCALE + round_data);
				for(int x1=0;x1<bw;x1++)
				{
					int adelta = saturate_cast<int>(M[0]*(x+x1)*AB_SCALE);
					int bdelta = saturate_cast<int>(M[3]*(x+x1)*AB_SCALE);
					int X = (X0 + adelta) >> (AB_BITS - INTER_BITS);
					int Y = (Y0 + bdelta) >> (AB_BITS - INTER_BITS);
					short src_x = saturate_cast<short>(X>>INTER_BIT);
					short src_y = saturate_cast<short>(Y>>INTER_BIT);
					short beta = X&(INTER_TAB_SIZE-1);
					short alpha = Y&(INTER_TAB_SIZE-1);
					bilinear_bound_check(src_x,src_y,src.cols,src.rows,out_bound);

					for(int c=0;c<dst.channels;c++)
					{
						int index0 = src_y*src.cols*src.channels + src_x*src.channels + c;
						int index1 = src_y*src.cols*src.channels + (src_x + 1)*src.channels + c;
						int index2 = (src_y+1)*src.cols*src.channels + src_x*src.channels + c;
						int index3 = (src_y+1)*src.cols*src.channels + (src_x+1)*src.channels + c;
						uint8_t p00 = out_bound[0] ? zero_point : src_ptr[index0];
						uint8_t p01 = out_bound[1] ? zero_point : src_ptr[index1];
						uint8_t p10 = out_bound[2] ? zero_point : src_ptr[index2];
						uint8_t p11 = out_bound[3] ? zero_point : src_ptr[index3];

						int w0 = ((INTER_TAB_SIZE - alpha)*(INTER_TAB_SIZE - beta))<<INTER_BIT;
						int w1 = ((INTER_TAB_SIZE - alpha)*beta)<<INTER_BIT;
						int w2 = (alpha*(INTER_TAB_SIZE - beta))<<INTER_BIT;
						int w3 = (alpha*beta)<<INTER_BIT;
						int out_index = (y+y1)*dst.cols*dst.channels + (x+x1)*dst.channels + c;
						dst_ptr[out_index] = (p00*w0 + p01*w1 + p10*w2 + p11*w3 + HALF_ONE)>>MAT_ACC;
					}
				}
			}
		}
	}
	return 0;
}
int warpPerspective(Mat_c src,Mat_c dst,double *M)
{
	int AB_BITS = 10>INTER_BIT ? 10:INTER_BIT;
	int AB_SCALE = 1<<AB_BITS;
	int INTER_TAB_SIZE_JZ = 1<<INTER_BIT;
	//int round_data = AB_SCALE/INTER_TAB_SIZE_JZ/2;
	int HALF_ONE = 1<<(MAT_ACC - 1);
	double t0 = M[0]*(M[4]*M[8]-M[5]*M[7]);
	double t1 = M[1]*(M[3]*M[8]-M[5]*M[6]);
	double t2 = M[2]*(M[3]*M[7]-M[4]*M[6]);
	double t3 = t0-t1+t2;
	double def = 0;
	double dst_m[9]={0};
	if(t3!=0.)
	{
		def = 1.0/t3;
		dst_m[0] = (M[4]*M[8] - M[5]*M[7])*def;
		dst_m[1] = (M[2]*M[7] - M[1]*M[8])*def;
		dst_m[2] = (M[1]*M[5] - M[2]*M[4])*def;

		dst_m[3] = (M[5]*M[6] - M[3]*M[8])*def;
		dst_m[4] = (M[0]*M[8] - M[2]*M[6])*def;
		dst_m[5] = (M[2]*M[3] - M[0]*M[5])*def;

		dst_m[6] = (M[3]*M[7] - M[4]*M[6])*def;
		dst_m[7] = (M[1]*M[6] - M[0]*M[7])*def;
		dst_m[8] = (M[0]*M[4] - M[1]*M[3])*def;
	}
	bool out_bound[4];
	int zero_point = 0;
	const int BLOCK_SZ_AFFINE = 32;
	int bh0 = std::min(BLOCK_SZ_AFFINE/2, src.rows);
	int bw0 = std::min(BLOCK_SZ_AFFINE*BLOCK_SZ_AFFINE/bh0, src.cols);
	bh0 = std::min(BLOCK_SZ_AFFINE*BLOCK_SZ_AFFINE/bw0, src.rows);
	uint8_t *src_ptr = src.base;
	uint8_t *dst_ptr = dst.base;
	for(int y =0;y<dst.rows;y+=bh0)
	{
		for(int x=0;x<dst.cols;x+=bw0)
		{
			int bw = std::min( bw0, src.cols - x);
			int bh = std::min( bh0, src.rows - y); // height
			for(int y1=0;y1<bh;y1++)
			{
				double X0 = dst_m[0]*x + dst_m[1]*(y+y1) + dst_m[2];
				double Y0 = dst_m[3]*x + dst_m[4]*(y+y1) + dst_m[5];
				double W0 = dst_m[6]*x + dst_m[7]*(y+y1) + dst_m[8];
				for(int x1=0;x1<bw;x1++)
				{
					double W = W0 + dst_m[6]*x1;
					W = W ? INTER_TAB_SIZE_JZ/W : 0;
					double fX = std::max((double)INT_MIN, std::min((double)INT_MAX, (X0 + dst_m[0]*x1)*W));
					double fY = std::max((double)INT_MIN, std::min((double)INT_MAX, (Y0 + dst_m[3]*x1)*W));
					int X = saturate_cast<int>(fX);
					int Y = saturate_cast<int>(fY);
					short src_x = saturate_cast<short>(X >> INTER_BITS);
					short src_y = saturate_cast<short>(Y >> INTER_BITS);
					short beta = X&(INTER_TAB_SIZE_JZ-1);
					short alpha = Y&(INTER_TAB_SIZE_JZ-1);
					bilinear_bound_check(src_x,src_y,src.cols,src.rows,out_bound);
					for(int c=0;c<dst.channels;c++)
					{
						int index0 = src_y*src.cols*src.channels + src_x*src.channels + c;
						int index1 = src_y*src.cols*src.channels + (src_x + 1)*src.channels + c;
						int index2 = (src_y+1)*src.cols*src.channels + src_x*src.channels + c;
						int index3 = (src_y+1)*src.cols*src.channels + (src_x+1)*src.channels + c;
						uint8_t p00 = out_bound[0] ? zero_point : src_ptr[index0];
						uint8_t p01 = out_bound[1] ? zero_point : src_ptr[index1];
						uint8_t p10 = out_bound[2] ? zero_point : src_ptr[index2];
						uint8_t p11 = out_bound[3] ? zero_point : src_ptr[index3];

						int w0 = ((INTER_TAB_SIZE_JZ - alpha)*(INTER_TAB_SIZE_JZ - beta))<<INTER_BIT;
						int w1 = ((INTER_TAB_SIZE_JZ - alpha)*beta)<<INTER_BIT;
						int w2 = (alpha*(INTER_TAB_SIZE_JZ - beta))<<INTER_BIT;
						int w3 = (alpha*beta)<<INTER_BIT;
						int out_index = (y+y1)*dst.cols*dst.channels + (x+x1)*dst.channels + c;
						dst_ptr[out_index] = (p00*w0 + p01*w1 + p10*w2 + p11*w3 + HALF_ONE)>>MAT_ACC;
					}
				}
			}
		}
	}
	return 0;
}

int compare_bgra(uint8_t *dst_ptr,uint8_t *dst_opencv_ptr,int rows,int cols,int channels)
{
	for(int i=0;i<rows;i++)
	{
		for(int j=0;j<cols;j++)
		{
			for(int c=0;c<channels;c++)
			{
				int index = i*cols*channels + j*channels + c;
				uint8_t val0 = dst_ptr[index];
				uint8_t val1 = dst_opencv_ptr[index];
				// if(i==0 && j==34) {
					// printf("dst_ptr = %p, dst_opencv_ptr = %p\n",&dst_ptr[index],  &dst_opencv_ptr[index]);
					// printf("%d %d\n",val0,val1);
				// }
				if(abs(val0-val1)>0)
				{
					printf("ERROR!!   (h:%d,w:%d,c:%d) (h:%d,w:%d)  => dst(%d) != dst_opencv(%d)\n",i,j,c,rows,cols,val0,val1);
					for(c = 0;c<4;c++)
					{
						index = i*cols*channels + j*channels + c;
						uint8_t val0 = dst_ptr[index];
						uint8_t val1 = dst_opencv_ptr[index];
						printf("%d %d\n",val0,val1);
					}
					exit(-1);
				}
			}
		}
	}
}

void yuv2rgb(uint8_t y, uint8_t u, uint8_t v,
		const uint32_t *coef, const uint8_t *offset,
		const uint8_t nv2bgr_alpha,
		bs_data_format_e order, uint8_t *rgba)
{
	int16_t y_m = y - offset[0];
	int16_t u_m = u - offset[1];
	int16_t v_m = v - offset[1];
	/*if(y_m<0)
	  y_m = 0;*/
	int64_t b32 = ((int64_t)coef[0] * y_m + (int64_t)coef[1] * u_m +
			(int64_t)coef[2] * v_m + (1 << (BS_CSCA - 1)));
	int64_t g32 = ((int64_t)coef[3] * y_m - (int64_t)coef[4] * u_m -
			(int64_t)coef[5] * v_m + (1 << (BS_CSCA - 1)));
	int64_t r32 = ((int64_t)coef[6] * y_m + (int64_t)coef[7] * u_m +
			(int64_t)coef[8] * v_m + (1 << (BS_CSCA - 1)));

	uint8_t a = nv2bgr_alpha;//fixme
	uint8_t b = CLIP(b32, 0, (1 << (BS_CSCA + 8)) - 1) >> BS_CSCA;//must do clip
	uint8_t g = CLIP(g32, 0, (1 << (BS_CSCA + 8)) - 1) >> BS_CSCA;//must do clip
	uint8_t r = CLIP(r32, 0, (1 << (BS_CSCA + 8)) - 1) >> BS_CSCA;//must do clip
#if 0
	printf("y:%d u:%d v:%d\n",y,u,v);
	printf("y_m:%d u_m:%d v_m:%d\n",y_m,u_m,v_m);
	printf("b:  y_w:%ld v_w:%ld u_w:%ld\n",coef[0],coef[2],coef[1]);
	printf("g:  y_w:%ld v_w:%ld u_w:%ld\n",coef[3],coef[5],coef[4]);
	printf("r:  y_w:%ld v_w:%ld u_w:%ld\n",coef[6],coef[8],coef[7]);
	printf("b:%d g:%d r:%d\n",b,g,r);
#endif
	switch (order) {
	case BS_DATA_BGRA://BGRA
		rgba[0] = b;
		rgba[1] = g;
		rgba[2] = r;
		rgba[3] = a;
		break;
	case BS_DATA_GBRA:
		rgba[0] = g;
		rgba[1] = b;
		rgba[2] = r;
		rgba[3] = a;
		break;
	case BS_DATA_RBGA:
		rgba[0] = r;
		rgba[1] = b;
		rgba[2] = g;
		rgba[3] = a;
		break;
	case BS_DATA_BRGA:
		rgba[0] = b;
		rgba[1] = r;
		rgba[2] = g;
		rgba[3] = a;
		break;
	case BS_DATA_GRBA:
		rgba[0] = g;
		rgba[1] = r;
		rgba[2] = b;
		rgba[3] = a;
		break;
	case BS_DATA_RGBA:
		rgba[0] = r;
		rgba[1] = g;
		rgba[2] = b;
		rgba[3] = a;
		break;
	default:
		rgba[0] = b;
		rgba[1] = g;
		rgba[2] = r;
		rgba[3] = a;
		fprintf(stderr, "[Error][%s]: not support this order(%d) (bgra = %d)\n", __func__, order,BS_DATA_BGRA);
		fprintf(stderr, "y:%d u:%d v:%d -> r:%d g:%d b:%d\n", y,u,v,r,g,b);
		break;
	}
}

/*convert*/
int nv12_bgra(uint8_t* src_y, uint8_t* src_uv, int in_height, int in_width,
		int iline_size,uint8_t* dst, int oline_size, bs_data_format_e order){
	if (!src_y || !src_uv || !dst) {
		printf("%s: %d null ptr\n", __func__, __LINE__);
		return -1;
	}
	uint8_t pix_y, pix_u, pix_v;
	uint32_t coef[9] = {1220542, 2116026, 0, 1220542, 409993, 852492, 1220542, 0, 1673527};
	uint8_t offset[2] = {16, 128};
	uint8_t nv2bgr_alpha = 0;

	int rgba;
	int i_col, i_row;
	for (i_row = 0; i_row < in_height; i_row++) {
		int y_row = i_row;
		int uv_row = y_row / 2;
		int *dst_hptr = (int *)(dst + i_row * oline_size);

		for (i_col = 0; i_col < in_width; i_col++) {
			int uv_col = (i_col / 2);
			pix_y = src_y[y_row * iline_size + i_col];
			pix_u = src_uv[uv_row * iline_size + uv_col * 2];
			pix_v = src_uv[uv_row * iline_size + uv_col * 2 + 1];
			yuv2rgb(pix_y, pix_u, pix_v, coef, offset, nv2bgr_alpha, order, (uint8_t *)(&rgba));
			dst_hptr[i_col] = rgba;
		}
	}
	return 0;
}

int resize_nv12_nv12(uint8_t *src_ptr,int src_w,int src_h,uint8_t *dst_ptr,int dst_w,int dst_h)
{
	uint8_t *src_base = src_ptr;
	uint8_t *src_cbase = src_ptr + src_w * src_h;/*src_uv_ptr*/
	uint8_t *dst_base = dst_ptr;
	uint8_t *dst_cbase = dst_ptr + dst_w * dst_h;/*dst_uv_ptr*/
	double inv_scale_x = (float)src_w/dst_w;
	double inv_scale_y = (float)src_h/dst_h;
	int src_stride = src_w;
	int dst_stride = dst_w;
	int trans_x = (0.5 * inv_scale_x - 0.5) * 65536;
	int trans_y = (0.5 * inv_scale_y - 0.5) * 65536;
	int bpp = 2;/*U and V*/
	int scale_x = inv_scale_x * 65536;
	int scale_y = inv_scale_y * 65536;
	int src_fx = trans_x;
	int src_fy = trans_y;
	int dst_x ,dst_y;
	for (dst_y = 0; dst_y < dst_h; dst_y++) {
		src_fx = trans_x;
		for (dst_x = 0; dst_x < dst_w; dst_x++) {
			int src_fx_round = src_fx + 16;
			int src_fy_round = src_fy + 16;
			int src_x = (src_fx_round) >> 16;
			int src_y = (src_fy_round) >> 16;
			int w_x1 = (src_fx_round & 0xFFFF) >> 5;
			int w_y1 = (src_fy_round & 0xFFFF) >> 5;
			int w_x0 = 2048 - w_x1;
			int w_y0 = 2048 - w_y1;
			int src_x0 = src_x;
			int src_x1 = src_x + 1;
			int src_y0 = src_y;
			int src_y1 = src_y + 1;

			if (src_x < 0) {
				src_x0 = 0;
				src_x1 = 0;
			}
			if (src_y < 0) {
				src_y0 = 0;
				src_y1 = 0;
			}

			if (src_x >= src_w - 1) {
				src_x0 = src_w - 1;
				src_x1 = src_w - 1;
			}
			if (src_y >= src_h - 1) {
				src_y0 = src_h - 1;
				src_y1 = src_h - 1;
			}

			int d0_idx = 0;
			int d1_idx = 0;
			int d2_idx = 0;
			int d3_idx = 0;
			uint8_t d0 = 0;
			uint8_t d1 = 0;
			uint8_t d2 = 0;
			uint8_t d3 = 0;
			/*y_ptr*/
			{
				d0_idx = src_y0 * src_stride + src_x0 ;
				d1_idx = src_y0 * src_stride + src_x1 ;
				d2_idx = src_y1 * src_stride + src_x0 ;
				d3_idx = src_y1 * src_stride + src_x1 ;
				d0 = src_ptr[d0_idx];
				d1 = src_ptr[d1_idx];
				d2 = src_ptr[d2_idx];
				d3 = src_ptr[d3_idx];

				uint32_t v0 = (d0 * w_x0 + d1 * w_x1) >> 4;
				uint32_t v1 = (d2 * w_x0 + d3 * w_x1) >> 4;
				uint32_t res = (((v0 * w_y0) >> 16) +
						((v1 * w_y1) >> 16) + 2) >> 2;

				int dst_idx = dst_y * dst_stride + dst_x;
				dst_ptr[dst_idx] = (uint8_t)(res & 0xFF);
			}
			/*uv_ptr*/
			for (int k = 0; k < bpp; k++) {
				d0_idx = (src_y0 / 2) * src_stride + (src_x0 & 0xfffe) + k;
				d1_idx = (src_y0 / 2) * src_stride + (src_x1 & 0xfffe) + k;
				d2_idx = (src_y1 / 2) * src_stride + (src_x0 & 0xfffe) + k;
				d3_idx = (src_y1 / 2) * src_stride + (src_x1 & 0xfffe) + k;
				d0 = src_cbase[d0_idx];
				d1 = src_cbase[d1_idx];
				d2 = src_cbase[d2_idx];
				d3 = src_cbase[d3_idx];
				uint32_t v0 = (d0 * w_x0 + d1 * w_x1) >> 4;
				uint32_t v1 = (d2 * w_x0 + d3 * w_x1) >> 4;
				uint32_t res = (((v0 * w_y0) >> 16) +
						((v1 * w_y1) >> 16) + 2) >> 2;
				if ((dst_x%2 == 0) && (dst_y%2 == 0)) {
					int dst_idx = (dst_y/2) * dst_stride + (dst_x & 0xfffe) + k;
					dst_cbase[dst_idx] = (uint8_t)(res & 0xFF);
				}
			}
			src_fx += scale_x;
		}
		src_fy += scale_y;
	}

}
#if 1

int opencv_resize(uint8_t *src_base,int src_w,int src_h,int src_chn,uint8_t *dst_base,int dst_w,int dst_h,int dst_chn){
	double inv_scale_x = (double)src_w/(double)dst_w;
	double inv_scale_y = (double)src_h/(double)dst_h;
	int trans_x = (int)((inv_scale_x * 0.5 - 0.5)*65536);
	int trans_y = (int)((inv_scale_y * 0.5 - 0.5)*65536);
	int scale_x = inv_scale_x * 65536;
	int scale_y = inv_scale_y * 65536;
	int32_t src_fx = trans_x; // src float x
	int32_t src_fy = trans_y; // src float y
	int src_stride = src_w*src_chn;
	int dst_stride = dst_w*dst_chn;
	int dst_y=0;
	for(;dst_y<dst_h;dst_y++)
	{
		src_fx = trans_x;
		int dst_x=0;
		for(;dst_x<dst_w;dst_x++)
		{
			int src_fx_round = src_fx + 16;
			int src_fy_round = src_fy + 16;
			int src_x = (src_fx_round) >> 16;
			int src_y = (src_fy_round) >> 16;
			int w_x1 = (src_fx_round & 0xFFFF) >> 5;
			int w_y1 = (src_fy_round & 0xFFFF) >> 5;
			int w_x0 = 2048 - w_x1;
			int w_y0 = 2048 - w_y1;
			int src_x0 = src_x;
			int src_x1 = src_x + 1;
			int src_y0 = src_y;
			int src_y1 = src_y + 1;
			if (src_x < 0) {
				src_x0 = 0;
				src_x1 = 0;
			}
			if (src_y < 0) {
				src_y0 = 0;
				src_y1 = 0;
			}

			if (src_x >= src_w - 1) {
				src_x0 = src_w - 1;
				src_x1 = src_w - 1;
			}
			if (src_y >= src_h - 1) {
				src_y0 = src_h - 1;
				src_y1 = src_h - 1;
			}
			int bpp=4;
			int k = 0;
			for (; k < bpp; k++) {
				int d0_idx = 0;
				int d1_idx = 0;
				int d2_idx = 0;
				int d3_idx = 0;
				uint8_t d0 = 0;
				uint8_t d1 = 0;
				uint8_t d2 = 0;
				uint8_t d3 = 0;
				{
					d0_idx = src_y0 * src_stride + src_x0 * bpp + k;
					d1_idx = src_y0 * src_stride + src_x1 * bpp + k;
					d2_idx = src_y1 * src_stride + src_x0 * bpp + k;
					d3_idx = src_y1 * src_stride + src_x1 * bpp + k;
					d0 = src_base[d0_idx];
					d1 = src_base[d1_idx];
					d2 = src_base[d2_idx];
					d3 = src_base[d3_idx];
				}

				{ //8bits
					uint32_t v0 = (d0 * w_x0 + d1 * w_x1) >> 4;
					uint32_t v1 = (d2 * w_x0 + d3 * w_x1) >> 4;
					uint32_t res = (((v0 * w_y0) >> 16) +
							((v1 * w_y1) >> 16) + 2) >> 2;
					int dst_idx = dst_y * dst_stride + dst_x * bpp + k;
					dst_base[dst_idx] = (uint8_t)(res & 0xFF);
				}
			}
			src_fx += scale_x;
		}
		src_fy += scale_y;
	}
}

#else
int opencv_resize(uint8_t *src_base,int src_w,int src_h,int src_chn,uint8_t *dst_base,int dst_w,int dst_h,int dst_chn)
{
	double inv_scale_x = (double)src_w/(double)dst_w;
	double inv_scale_y = (double)src_h/(double)dst_h;
	//double scale_x = 1./inv_scale_x, scale_y = 1./inv_scale_y;
	double scale_x = inv_scale_x, scale_y = inv_scale_y;
	float fx, fy;
	int sx, sy,sy1, dx, dy;
	short alpha0,alpha1,beta0,beta1;
	int zero_point = 0;
	bool out_bound[4];
	dy=0;
	for(;dy<dst_h;dy++)
	{
		fy = (float)((dy+0.5)*scale_y - 0.5);
		sy = mxuFloor(fy);
		fy -= sy;
		sy1 = sy + 1;
		if(sy < 0)
		{
			sy = 0;
			sy1 = 0;
		}
		if(sy >= src_h - 1)
		{
			sy = src_h - 1;
			sy1 = src_h - 1;
		}
		//sy =  sy >= 0 ? (sy < dst_h ? sy : dst_h-1) : 0;
		//sy1 =  sy1 >= 0 ? (sy1 < dst_h ? sy1 : dst_h-1) : 0;
		beta1 = saturate_cast<short>(fy * INTER_RESIZE_COEF_SCALE);
		beta0 = saturate_cast<short>((1 - fy) * INTER_RESIZE_COEF_SCALE);
		dx=0;
		for(;dx<dst_w;dx++)
		{
			fx = (float)((dx+0.5)*scale_x - 0.5);
			sx = mxuFloor(fx);
			fx -= sx;
			if( sx < 0 )
				fx = 0, sx = 0;
			if( sx >= src_w - 1)
				fx = 0, sx = src_w-1;
			alpha1 = saturate_cast<short>(fx * INTER_RESIZE_COEF_SCALE);
			alpha0 = saturate_cast<short>((1 - fx) * INTER_RESIZE_COEF_SCALE);
			bilinear_bound_check(sx,sy,src_w,src_h,out_bound);
			for(int c=0;c<dst_chn;c++)
			{

				int index0 = sy*src_w*src_chn + sx*src_chn + c;
				int index1 = sy*src_w*src_chn + (sx+1)*src_chn + c;
				int index2 = sy1*src_w*src_chn + sx*src_chn + c;
				int index3 = sy1*src_w*src_chn + (sx+1)*src_chn + c;
				uint8_t p00 = out_bound[0] ? zero_point : src_base[index0];
				uint8_t p01 = out_bound[1] ? zero_point : src_base[index1];
				uint8_t p10 = out_bound[0] ? zero_point : src_base[index2];
				uint8_t p11 = out_bound[1] ? zero_point : src_base[index3];

				int val0 = p00*alpha0 + p01*alpha1;
				int val1 = p10*alpha0 + p11*alpha1;
				uint8_t val = ((((beta0 *(val0>>4))>>16) + ((beta1 *(val1>>4))>>16) + 2)>>2);
				//if(dy==0&&dx==0&&c==0)
				if(0)
				{
					printf("sx:%d sy:%d\n",sx,sy);
					printf("p00:%d p01:%d p10:%d p11:%d\n",p00,p01,p10,p11);
					printf("alpha0:%d alpha1:%d beta0:%d beta1:%d\n",alpha0,alpha1,beta0,beta1);
					printf("val0:%d val1:%d val:%d\n",val0,val1,val);
				}
				dst_base[dy*dst_w*dst_chn + dx*dst_chn + c] = val;
			}
		}
	}
}
#endif

int compare_nv12(uint8_t *dst_ptr,uint8_t *dst_opencv_ptr,int rows,int cols)
{
	uint8_t *dst_ptr_y = dst_ptr;
	uint8_t *dst_ptr_uv = dst_ptr + rows*cols;
	uint8_t *dst_opencv_y = dst_opencv_ptr;
	uint8_t *dst_opencv_uv = dst_opencv_ptr + rows*cols;
	for(int i=0;i<rows;i++)
	{
		for(int j=0;j<cols;j++)
		{
			int index_y = i*cols + j;
			uint8_t val0_y = dst_ptr_y[index_y];
			uint8_t val1_y = dst_opencv_y[index_y];

			int index_uv = (i/2)*cols + j;
			uint8_t val0_uv = dst_ptr_uv[index_uv];
			uint8_t val1_uv = dst_opencv_uv[index_uv];
			if((abs(val0_y-val1_y)>0)||(abs(val0_uv - val1_uv)>0))
			{
				printf("ERROR!! (h:%d,w:%d)  => dst != dst_opencv\n",rows,cols);
				printf("i:%d j:%d\n",i,j);
				printf("y:%d %d\n",val0_y,val1_y);
				printf("u:%d %d\n",dst_ptr_uv[(i/2)*cols + (j&0xffffe)],dst_opencv_uv[(i/2)*cols + (j&0xffffe)]);
				printf("v:%d %d\n",dst_ptr_uv[(i/2)*cols + (j&0xffffe) + 1],dst_opencv_uv[(i/2)*cols + (j&0xffffe) + 1]);
				exit(-1);
			}
		}
	}
}

static inline void matrix_double_to_s32(double *src, int64_t *dst,int num,int shift)
{
	const int AB_BITS = shift;//MAX(10, shift);
	const int AB_SCALE = 1 << AB_BITS;
	for (int i = 0; i < num; i++) {
		dst[i] = round(src[i] * AB_SCALE);
	}
}

int iwarpPerspective(uint8_t *src,uint8_t *dst,int src_w,int src_h,int src_chn,int dst_w,int dst_h,int dst_chn,double *M)
{
	int AB_BITS = 10>INTER_BIT ? 10:INTER_BIT;
	int AB_SCALE = 1<<AB_BITS;
	int INTER_TAB_SIZE = 1<<INTER_BIT;
	float scale = 1.0/INTER_TAB_SIZE;
	//int round_data = AB_SCALE/INTER_TAB_SIZE/2;
	int HALF_ONE = 1<<(MAT_ACC - 1);
	double t0 = M[0]*(M[4]*M[8]-M[5]*M[7]);
	double t1 = M[1]*(M[3]*M[8]-M[5]*M[6]);
	double t2 = M[2]*(M[3]*M[7]-M[4]*M[6]);
	double t3 = t0-t1+t2;
	double dst_m[9]={0};
	if(t3 != 0.0)
	{
		double def = 1.0/t3;
		dst_m[0] = (M[4]*M[8]-M[5]*M[7])*def;
		dst_m[1] = (M[2]*M[7]-M[1]*M[8])*def;
		dst_m[2] = (M[1]*M[5]-M[2]*M[4])*def;

		dst_m[3] = (M[5]*M[6]-M[3]*M[8])*def;
		dst_m[4] = (M[0]*M[8]-M[2]*M[6])*def;
		dst_m[5] = (M[2]*M[3]-M[0]*M[5])*def;

		dst_m[6] = (M[3]*M[7]-M[4]*M[6])*def;
		dst_m[7] = (M[1]*M[6]-M[0]*M[7])*def;
		dst_m[8] = (M[0]*M[4]-M[1]*M[3])*def;
	}
	bool out_bound[4];
	int zero_point = 0;
	const int BLOCK_SZ = 32;
	int bh0 = std::min(BLOCK_SZ/2, src_h);
	int bw0 = std::min(BLOCK_SZ*BLOCK_SZ/bh0, src_w);
	bh0 = std::min(BLOCK_SZ*BLOCK_SZ/bw0, src_h);
	int64_t idst_m[9]={0};
	matrix_double_to_s32(dst_m,idst_m,9,SHIFT);
	for(int y =0;y<dst_h;y+=bh0)
	{
		for(int x=0;x<dst_w;x+=bw0)
		{
			int bw = std::min( bw0, dst_w - x);
			int bh = std::min( bh0, dst_h - y); // height
			for(int y1=0;y1<bh;y1++)
			{
				for(int x1=0;x1<bw;x1++)
				{
					int64_t X0 = idst_m[0]*(x+x1) + idst_m[1]*(y+y1) + idst_m[2];
					int64_t Y0 = idst_m[3]*(x+x1) + idst_m[4]*(y+y1) + idst_m[5];
					int64_t W0 = idst_m[6]*(x+x1) + idst_m[7]*(y+y1) + idst_m[8];
					int64_t shift_x = 0;//W0/2;
					int64_t shift_y = 0;//W0/2;
					if(X0<0)
						shift_x = 0;//-W0/2;
					if(Y0<0)
						shift_y = 0;//-W0/2;
					int64_t fX = 0;
					int64_t fY = 0;
					if(W0)
					{
						//fX =(int64_t)(round((X0*INTER_TAB_SIZE)/W0));
						//fY =(int64_t)(round((Y0*INTER_TAB_SIZE)/W0));
						fX =(int64_t)((X0*INTER_TAB_SIZE + shift_x)/W0);
						fY =(int64_t)((Y0*INTER_TAB_SIZE + shift_y)/W0);
					}
					int X = CLIP(fX,INT_MIN,INT_MAX);
					int Y = CLIP(fY,INT_MIN,INT_MAX);
					short src_x = saturate_cast<short>(X >> INTER_BITS);
					short src_y = saturate_cast<short>(Y >> INTER_BITS);

					short beta = X&(INTER_TAB_SIZE-1);
					short alpha = Y&(INTER_TAB_SIZE-1);
					bilinear_bound_check(src_x,src_y,src_w,src_h,out_bound);
					float alpha_f = alpha*scale;
					float beta_f = beta*scale;
					for(int c=0;c<dst_chn;c++)
					{
						int index0 = src_y*src_w*src_chn + src_x*src_chn + c;
						int index1 = src_y*src_w*src_chn + (src_x+1)*src_chn + c;
						int index2 = (src_y+1)*src_w*src_chn + src_x*src_chn + c;
						int index3 = (src_y+1)*src_w*src_chn + (src_x+1)*src_chn + c;
						uint8_t p00 = out_bound[0] ? zero_point : src[index0];
						uint8_t p01 = out_bound[1] ? zero_point : src[index1];
						uint8_t p10 = out_bound[2] ? zero_point : src[index2];
						uint8_t p11 = out_bound[3] ? zero_point : src[index3];
						int w0 = ((1 - alpha_f)*(1 - beta_f))*INTER_REMAP_COEF_SCALE;
						int w1 = ((1 - alpha_f)*(beta_f))*INTER_REMAP_COEF_SCALE;
						int w2 = ((alpha_f)*(1 - beta_f))*INTER_REMAP_COEF_SCALE;
						int w3 = ((alpha_f)*(beta_f))*INTER_REMAP_COEF_SCALE;
						dst[(y+y1)*dst_w*dst_chn + (x+x1)*dst_chn + c] = (p00*w0 + p01*w1 + p10*w2 + p11*w3 + HALF_ONE)>>MAT_ACC;
						if((y+y1)==5&&(x+x1)==6&&c==0)
							//if(0)
						{
							printf("nzf:*******************\n");
							printf("%lf %lf %lf\n",M[0],M[1],M[2]);
							printf("%lf %lf %lf\n",M[3],M[4],M[5]);
							printf("%lf %lf %lf\n",M[6],M[7],M[8]);

							printf("%lld %lld %lld\n",dst_m[0],dst_m[1],dst_m[2]);
							printf("%lld %lld %lld\n",dst_m[3],dst_m[4],dst_m[5]);
							printf("%lld %lld %lld\n",dst_m[6],dst_m[7],dst_m[8]);
							printf("X0:%d Y0:%d W0:%d\n",X0,Y0,W0);
							printf("X:%d Y:%d\n",X,Y);
							printf("src_x:%d src_y:%d\n",src_x,src_y);
							printf("beta:%d alpha:%d\n",beta,alpha);
							printf("p00:%d p01:%d p10:%d p11:%d\n",p00,p01,p10,p11);
							printf("w0:%d w1:%d w2:%d w3:%d\n",w0,w1,w2,w3);
							int tt = (y+y1)*dst_w*dst_chn + (x+x1)*dst_chn + c;
							printf("tt:%d = %d\n",tt,dst[tt]);

						}
					}
				}
			}
		}
	}
	return 0;
}

int iwarpAffine(uint8_t *src,uint8_t *dst,int src_w,int src_h,int src_chn,int dst_w,int dst_h,int dst_chn,double *M)
{
	int AB_BITS = 10>INTER_BIT ? 10:INTER_BIT;
	int AB_SCALE = 1<<AB_BITS;
	int INTER_TAB_SIZE = 1<<INTER_BITS;
	int32_t round_data = (AB_SCALE/INTER_TAB_SIZE/2);
	int HALF_ONE = 1<<(MAT_ACC - 1);
	//convert matrix
	double D = M[0]*M[4] - M[1]*M[3];
	D = D != 0 ? 1./D : 0;
	double A11 = M[4]*D, A22=M[0]*D;
	M[0] = A11; M[1] *= -D;
	M[3] *= -D; M[4] = A22;
	double b1 = -M[0]*M[2] - M[1]*M[5];
	double b2 = -M[3]*M[2] - M[4]*M[5];
	M[2] = b1; M[5] = b2;

	bool out_bound[4];
	int zero_point = 0;
	const int BLOCK_SZ = 64;
	int bh0 = std::min(BLOCK_SZ/2, dst_h);
	int bw0 = std::min(BLOCK_SZ*BLOCK_SZ/bh0, dst_w);
	bh0 = std::min(BLOCK_SZ*BLOCK_SZ/bw0, dst_h);
	int64_t iM[6]={0};
	matrix_double_to_s32(M,iM,6,16);//float to int64
	for(int y=0;y<dst_h;y=y+bh0)
	{
		for(int x=0;x<dst_w;x=x+bw0)
		{
			int bw = std::min( bw0, dst_w - x);
			int bh = std::min( bh0, dst_h - y);
			for(int y1=0;y1<bh;y1++)
			{
				int64_t X0_64 = ((iM[1]*(y+y1) + iM[2])>>(SHIFT_AFFINE - AB_BITS));
				int64_t Y0_64 = ((iM[4]*(y+y1) + iM[5])>>(SHIFT_AFFINE - AB_BITS));
				int32_t X0 = CLIP(X0_64,INT_MIN,INT_MAX) + round_data;
				int32_t Y0 = CLIP(Y0_64,INT_MIN,INT_MAX) + round_data;
				for(int x1=0;x1<bw;x1++)
				{
					int64_t adelta_64 = ((iM[0]*(x+x1))>>(SHIFT_AFFINE - AB_BITS));
					int64_t bdelta_64 = ((iM[3]*(x+x1))>>(SHIFT_AFFINE - AB_BITS));
					int32_t adelta = CLIP(adelta_64,INT_MIN,INT_MAX);
					int32_t bdelta = CLIP(bdelta_64,INT_MIN,INT_MAX);
					int32_t X = (X0 + adelta)>>(AB_BITS - INTER_BITS);
					int32_t Y = (Y0 + bdelta)>>(AB_BITS - INTER_BITS);
					short src_x = saturate_cast<short>(X>>INTER_BITS);
					short src_y = saturate_cast<short>(Y>>INTER_BITS);
					short beta = X&(INTER_TAB_SIZE-1);
					short alpha = Y&(INTER_TAB_SIZE-1);
					bilinear_bound_check(src_x,src_y,src_w,src_h,out_bound);

					for(int c=0;c<dst_chn;c++)
					{
						int index0 = src_y*src_w*src_chn + src_x*src_chn + c;
						int index1 = src_y*src_w*src_chn + (src_x+1)*src_chn + c;
						int index2 = (src_y+1)*src_w*src_chn + src_x*src_chn + c;
						int index3 = (src_y+1)*src_w*src_chn + (src_x+1)*src_chn + c;
						uint8_t p00 = out_bound[0] ? zero_point : src[index0];
						uint8_t p01 = out_bound[1] ? zero_point : src[index1];
						uint8_t p10 = out_bound[2] ? zero_point : src[index2];
						uint8_t p11 = out_bound[3] ? zero_point : src[index3];

						int w0 = ((INTER_TAB_SIZE - alpha)*(INTER_TAB_SIZE - beta))<<INTER_BITS;
						int w1 = ((INTER_TAB_SIZE - alpha)*beta)<<INTER_BITS;
						int w2 = (alpha*(INTER_TAB_SIZE - beta))<<INTER_BITS;
						int w3 = (alpha*beta)<<INTER_BITS;
						int tt = (y+y1)*dst_w*dst_chn + (x+x1)*dst_chn + c;
						dst[tt] = (p00*w0 + p01*w1 + p10*w2 + p11*w3 + HALF_ONE)>>MAT_ACC;
						// if((y+y1)==0&&(x+x1)==1&&c==0)
						if(0)
						{
							printf("nzf:*******************\n");
							printf("%lf %lf %lf\n",M[0],M[1],M[2]);
							printf("%lf %lf %lf\n",M[3],M[4],M[5]);

							printf("%lld %lld %lld\n",iM[0],iM[1],iM[2]);
							printf("%lld %lld %lld\n",iM[3],iM[4],iM[5]);
							printf("X0_64:%d Y0_64:%d\n",X0_64,Y0_64);
							printf("X0:%d Y0:%d\n",X0,Y0);
							printf("adelta_64:%d bdealta_64:%d\n",adelta_64,bdelta_64);
							printf("adelta:%d bdealta:%d\n",adelta,bdelta);
							printf("X:%d Y:%d\n",X,Y);
							printf("src_x:%d src_y:%d\n",src_x,src_y);
							printf("beta:%d alpha:%d\n",beta,alpha);
							printf("p00:%d p01:%d p10:%d p11:%d\n",p00,p01,p10,p11);
							printf("w0:%d w1:%d w2:%d w3:%d\n",w0,w1,w2,w3);
							printf("tt:%d = %d\n",tt,dst[tt]);

						}
					}
				}
			}
		}
	}
	return 0;
}

int iwarpAffine_nv12_nv12(uint8_t *src,uint8_t *dst,int src_w,int src_h,int dst_w,int dst_h,double *M)
{
	int AB_BITS = 10>INTER_BIT ? 10:INTER_BIT;
	int AB_SCALE = 1<<AB_BITS;
	int INTER_TAB_SIZE = 1<<INTER_BITS;
	int32_t round_data = (AB_SCALE/INTER_TAB_SIZE/2);
	int HALF_ONE = 1<<(MAT_ACC - 1);
	//convert matrix
	double D = M[0]*M[4] - M[1]*M[3];
	D = D != 0 ? 1./D : 0;
	double A11 = M[4]*D, A22=M[0]*D;
	M[0] = A11; M[1] *= -D;
	M[3] *= -D; M[4] = A22;
	double b1 = -M[0]*M[2] - M[1]*M[5];
	double b2 = -M[3]*M[2] - M[4]*M[5];
	M[2] = b1; M[5] = b2;
	bool out_bound[4];
	int zero_point = 0;
	const int BLOCK_SZ = 64;
	int bh0 = std::min(BLOCK_SZ/2, dst_h);
	int bw0 = std::min(BLOCK_SZ*BLOCK_SZ/bh0, dst_w);
	bh0 = std::min(BLOCK_SZ*BLOCK_SZ/bw0, dst_h);
	int64_t iM[6]={0};
	matrix_double_to_s32(M,iM,6,16);//float to int64
	uint8_t *src_uv = src + src_h*src_w;
	uint8_t *dst_uv = dst + dst_h*dst_w;

	for(int y=0;y<dst_h;y=y+bh0)
	{
		for(int x=0;x<dst_w;x=x+bw0)
		{
			int bw = std::min( bw0, dst_w - x);
			int bh = std::min( bh0, dst_h - y);
			for(int y1=0;y1<bh;y1++)
			{
				int64_t X0_64 = ((iM[1]*(y+y1) + iM[2])>>(SHIFT_AFFINE - AB_BITS));
				int64_t Y0_64 = ((iM[4]*(y+y1) + iM[5])>>(SHIFT_AFFINE - AB_BITS));
				int32_t X0 = CLIP(X0_64,INT_MIN,INT_MAX) + round_data;
				int32_t Y0 = CLIP(Y0_64,INT_MIN,INT_MAX) + round_data;
				for(int x1=0;x1<bw;x1++)
				{
					int64_t adelta_64 = ((iM[0]*(x+x1))>>(SHIFT_AFFINE - AB_BITS));
					int64_t bdelta_64 = ((iM[3]*(x+x1))>>(SHIFT_AFFINE - AB_BITS));
					int32_t adelta = CLIP(adelta_64,INT_MIN,INT_MAX);
					int32_t bdelta = CLIP(bdelta_64,INT_MIN,INT_MAX);
					int32_t X = (X0 + adelta)>>(AB_BITS - INTER_BITS);
					int32_t Y = (Y0 + bdelta)>>(AB_BITS - INTER_BITS);
					short src_x = saturate_cast<short>(X>>INTER_BITS);
					short src_y = saturate_cast<short>(Y>>INTER_BITS);
					short beta = X&(INTER_TAB_SIZE-1);
					short alpha = Y&(INTER_TAB_SIZE-1);

					int src_x0 = src_x;
					int src_x1 = src_x + 1;
					int src_y0 = src_y;
					int src_y1 = src_y + 1;
					bilinear_bound_check(src_x,src_y,src_w,src_h,out_bound);
					/*y_ptr*/
					{
						int index0 = src_y0*src_w + src_x0;
						int index1 = src_y0*src_w + src_x1;
						int index2 = src_y1*src_w + src_x0;
						int index3 = src_y1*src_w + src_x1;
						uint8_t p00 = out_bound[0] ? zero_point : src[index0];
						uint8_t p01 = out_bound[1] ? zero_point : src[index1];
						uint8_t p10 = out_bound[2] ? zero_point : src[index2];
						uint8_t p11 = out_bound[3] ? zero_point : src[index3];

						int w0 = ((INTER_TAB_SIZE - alpha)*(INTER_TAB_SIZE - beta))<<INTER_BIT;
						int w1 = ((INTER_TAB_SIZE - alpha)*beta)<<INTER_BIT;
						int w2 = (alpha*(INTER_TAB_SIZE - beta))<<INTER_BIT;
						int w3 = (alpha*beta)<<INTER_BIT;
						int out_index = (y+y1)*dst_w + (x+x1);
						dst[out_index] = (p00*w0 + p01*w1 + p10*w2 + p11*w3 + HALF_ONE)>>MAT_ACC;
					}
					/*uv_ptr*/
					bilinear_bound_check_uv(src_x0,src_x1,src_y0,src_y1,src_w,src_h,out_bound);
					for (int k = 0; k < 2; k++) {
						int index0 = (src_y0 / 2)*src_w + (src_x0 & 0xfffe) + k;
						int index1 = (src_y0 / 2)*src_w + (src_x1 & 0xfffe) + k;
						int index2 = (src_y1 / 2)*src_w + (src_x0 & 0xfffe) + k;
						int index3 = (src_y1 / 2)*src_w + (src_x1 & 0xfffe) + k;
						uint8_t p00 = out_bound[0] ? zero_point : src_uv[index0];
						uint8_t p01 = out_bound[1] ? zero_point : src_uv[index1];
						uint8_t p10 = out_bound[2] ? zero_point : src_uv[index2];
						uint8_t p11 = out_bound[3] ? zero_point : src_uv[index3];

						int w0 = ((INTER_TAB_SIZE - alpha)*(INTER_TAB_SIZE - beta))<<INTER_BIT;
						int w1 = ((INTER_TAB_SIZE - alpha)*beta)<<INTER_BIT;
						int w2 = (alpha*(INTER_TAB_SIZE - beta))<<INTER_BIT;
						int w3 = (alpha*beta)<<INTER_BIT;
						if (((x+x1)%2 == 0) && ((y+y1)%2 == 0)) {
							int dst_idx = ((y+y1)/2) * dst_w + ((x+x1) & 0xfffe) + k;
							uint8_t res = (p00*w0 + p01*w1 + p10*w2 + p11*w3 + HALF_ONE)>>MAT_ACC;
							dst_uv[dst_idx] = res;
						}
					}
				}
			}
		}
	}
	return 0;
}
