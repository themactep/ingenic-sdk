#ifndef __ROTATE_H__
#define __ROTATE_H__


#include <log.h>
#include <riscv.h>
#include <string.h>


#define DIV_ROUND_UP( left_size,seg_size) ((left_size-1)/seg_size +1)


typedef enum {
    NV12       		  = 0,
    ARGB1555   	      = 1,
    ARGB8888   	      = 2,
    TYPE_END   		  = 3,
} stream_type_e;

typedef enum {
	ROTATE0			  = 0,
	ROTATE90		  = 1,
	ROTATE180		  = 2,
	ROTATE270		  = 3,
	ROTATEEND	  	  = 4,
} angle_e;



struct jz_rotate_info{
	int 			index;
	stream_type_e 	type;
	angle_e         angle;
	int 			width;
	int 			height;
	unsigned int 	src;
	unsigned int 	des;
	int   			box_width;
	int   			boxh_height;
};


int get_rotate_info( struct jz_rotate_info *rotate_info);


/*
*get_box_info( int width,int height,int index,int *x,int *y,int *w,int *h,int statd_w,int statd_h)
*获取box的坐标和宽高
* @width  	:画布宽
* @height 	:画布高
* @index  	:第几个box
* @*x     	:获取box的坐标x
* @*y     	:获取box的坐标y
* @*w     	:获取box的宽
* @*h     	:获取box的高
* @statd_w	:box宽
* @*statd_h :box高
*/
int get_box_info( int width,int height,int index,int *x,int *y,int *w,int *h,int statd_w,int statd_h);

/*nv12格式 顺时针旋转90度 按照box分割旋转*/
int nv12rotate90(void);

/*nv12格式 顺时针旋转180度 按照box分割旋转*/
int nv12rotate180(void);

/*nv12格式 顺时针旋转270度 按照box分割旋转*/
int nv12rotate270(void);

/*ARGB1555格式 顺时针旋转90度 按照box分割旋转*/
int argb1555rotate90(void);

/*ARGB1555格式 顺时针旋转180度 按照box分割旋转*/
int argb1555rotate180(void);

/*ARGB1555格式 顺时针旋转270度 按照box分割旋转*/
int argb1555rotate270(void);

/*ARGB1555格式 顺时针旋转90度 按照box分割旋转*/
int argb8888rotate90(void);

/*ARGB1555格式 顺时针旋转180度 按照box分割旋转*/
int argb8888rotate180(void);

/*ARGB1555格式 顺时针旋转270度 按照box分割旋转*/
int argb8888rotate270(void);


/*
*rotateRGB90(unsigned char *src, unsigned char *dst, int width, int height, int bpp)
*RGB格式 顺时针旋转90度 整体旋转
* @bpp	:
* 		argb1555 2
* 		rgb888   3
* 		argb8888 4
*
* @*src	 :源
* @*dst	 :目的
* @width :源的宽
* @height:源的高
*/
int rotateRGB90(unsigned char *src, unsigned char *dst, int width, int height, int bpp);

/*
*rotateRGB180(unsigned char *src, unsigned char *dst, int width, int height, int bpp)
*RGB格式 顺时针旋转180度 整体旋转
* @bpp	:
* 		argb1555 2
* 		rgb888   3
* 		argb8888 4
*
* @*src	 :源
* @*dst	 :目的
* @width :源的宽
* @height:源的高
*/
int rotateRGB180(unsigned char *src, unsigned char *dst, int width, int height, int bpp);

/*
*rotateRGB270(unsigned char *src, unsigned char *dst, int width, int height, int bpp)
*RGB格式 顺时针旋转270度 整体旋转
* @bpp	:
* 		argb1555 2
* 		rgb888   3
* 		argb8888 4
*
* @*src	 :源
* @*dst	 :目的
* @width :源的宽
* @height:源的高
*/
int rotateRGB270(unsigned char *src, unsigned char *dst, int width, int height, int bpp);


int rotate360(struct jz_rotate_info rotate_info);

int argb1555rotateinline180(void);
int argb8888rotateinline180(void);
int nv12rotateinline180(void);




#endif
