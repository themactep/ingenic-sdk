/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : bscaler_api.h
 * Authors    : ywhan
 * Create Time: 2021-05-20:15:50:09
 * Description:
 *
 */

#ifndef __BSCALER_API_H__
#define __BSCALER_API_H__
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int                 x;
    int                 y;
    int                 w;
    int                 h;
} box_info_s;

typedef struct {
    box_info_s          box;//source box
    float               matrix[9];//fixme
    uint32_t            wrap;
    uint32_t            zero_point;
} box_affine_info_s;

typedef struct {
    box_info_s          box;//source box
    float               matrix[9];
    uint32_t            wrap;
    uint32_t            zero_point;
} box_perspective_info_s;

typedef struct {
    box_info_s          box;//source box
    uint32_t            wrap;
    uint32_t            zero_point;
} box_resize_info_s;

typedef enum {
    BS_DATA_NV12        = 0,  //000,0000
    BS_DATA_BGRA        = 1,  //000,0001
    BS_DATA_GBRA        = 3,  //000,0011
    BS_DATA_RBGA        = 5,  //000,0101
    BS_DATA_BRGA        = 7,  //000,0111
    BS_DATA_GRBA        = 9,  //000,1001
    BS_DATA_RGBA        = 11, //000,1011
    BS_DATA_ABGR        = 17, //001,0001
    BS_DATA_AGBR        = 19, //001,0011
    BS_DATA_ARBG        = 21, //001,0101
    BS_DATA_ABRG        = 23, //001,0111
    BS_DATA_AGRB        = 25, //001,1001
    BS_DATA_ARGB        = 27, //001,1011
    BS_DATA_FMU2        = 33, //010,0000
    BS_DATA_FMU4        = 65, //100,0000
    BS_DATA_FMU8        = 97, //110,0000
    BS_DATA_FMU8_H	= 98,
    BS_DATA_VBGR        = 101, //110,0000//fixme
    BS_DATA_VRGB        = 102, //110,0000//fixme
    BS_DATA_Y		= 111,
    BS_DATA_UV		= 112,
} bs_data_format_e;

typedef enum {
    BS_DATA_NMEM        = 0,//data in nmem
    BS_DATA_ORAM        = 1,//data in oram
    BS_DATA_RMEM        = 2,//data in rmem
} bs_data_locate_e;

typedef struct {
    uint32_t		base;
    uint32_t		base1;
    bs_data_format_e    format;
    int                 chn;
    int                 width;
    int                 height;
    int                 line_stride;
    bs_data_locate_e    locate;
} data_info_s;

typedef struct {
    uint8_t             kw; // kernel size width
    uint8_t             kh; // kernel size height
    uint8_t             k_stride_x; // kernel stride in x
    uint8_t             k_stride_y; // kernel stride in y
    uint8_t             pad_top;
    uint8_t             pad_bottom;
    uint8_t             pad_left;
    uint8_t             pad_right;
    uint32_t            zero_point; // 3 chain
    uint32_t            task_len;
    uint32_t            plane_stride;
} virchn_info_s;//delete me

typedef struct {
    uint32_t            zero_point; // 3 chain
    uint32_t            task_len;
    uint32_t            plane_stride;
} task_info_s;

int bs_covert_cfg(data_info_s *src, const data_info_s *dst,
                  const uint32_t *coef, const uint32_t *offset,
                  const task_info_s *task_info);

int bs_covert_step_start(const task_info_s *task_info, uint32_t dst_ptr, const bs_data_locate_e locate);

int bs_covert_step_wait();
int bs_covert_step_exit();

int bs_perspective_start(data_info_s *src,
                         const int box_num, const data_info_s *dst,
                         const box_affine_info_s *boxes,
                         const uint32_t *coef, const uint32_t *offset);
int bs_perspective_wait();

int bs_affine_start(data_info_s *src,
                    const int box_num, const data_info_s *dst,
                    const box_affine_info_s *boxes,
                    const uint32_t *coef, const uint32_t *offset);
int bs_affine_wait();

int bs_resize_start(data_info_s *src,
                    const int box_num, data_info_s *dst,
                    const box_resize_info_s *boxes,
                    const uint32_t *coef, const uint32_t *offset);

int bs_resize_wait();

void bs_version();

#ifdef __cplusplus
}
#endif
#endif /* __BSCALER_API_H__ */
