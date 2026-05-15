/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : jz_aip.h
 * Authors    : ywhan
 * Create Time: 2021-05-20:15:50:09
 * Description:
 *
 */

#ifndef __JZ_AIP_H__
#define __JZ_AIP_H__
#include <stdint.h>


#define JZ_AIP_IOBASE		0x13090000
#define JZ_AIP_IOSIZE		0x1000

#define JZ_AIP_T_IOBASE 	0x13090000
#define JZ_AIP_F_IOBASE		0x13090200
#define JZ_AIP_P_IOBASE		0x13090300

#define IOCTL_AIP_IRQ_WAIT_CMP		_IOWR('P', 0, int)
#define IOCTL_AIP_MALLOC		_IOWR('P', 1, int)
#define IOCTL_AIP_FREE			_IOWR('P', 2, int)

#define AIP_F_DEV	"jzaip_f"
#define AIP_T_DEV	"jzaip_t"
#define AIP_P_DEV	"jzaip_p"

#define AIP_P_CTRL			0x00
#define AIP_P_IQR			0x04
#define AIP_P_CFG			0x08
#define AIP_P_TIMEOUT			0x0c
#define AIP_P_MODE			0x10
#define AIP_P_SRC_Y_BASE		0x14
#define AIP_P_SRC_UV_BASE		0x18
#define AIP_P_SRC_STRIDE		0x1c
#define AIP_P_DST_BASE			0x20
#define AIP_P_DST_STRIDE		0x24
#define AIP_P_DST_SIZE			0x28
#define AIP_P_SRC_SIZE			0x2c
#define AIP_P_DUMMY_VAL			0x30
#define AIP_P_COEF0			0x34
#define AIP_P_COEF1			0x38
#define AIP_P_COEF2			0x3c
#define AIP_P_COEF3			0x40
#define AIP_P_COEF4			0x44
#define AIP_P_COEF5			0x48
#define AIP_P_COEF6			0x4c
#define AIP_P_COEF7			0x50
#define AIP_P_COEF8			0x54
#define AIP_P_COEF9			0x58
#define AIP_P_COEF10			0x5c
#define AIP_P_COEF11			0x60
#define AIP_P_COEF12			0x64
#define AIP_P_COEF13			0x68
#define AIP_P_COEF14			0x6c
#define AIP_P_PARAM0			0x70
#define AIP_P_PARAM1			0x74
#define AIP_P_PARAM2			0x78
#define AIP_P_PARAM3			0x7c
#define AIP_P_PARAM4			0x80
#define AIP_P_PARAM5			0x84
#define AIP_P_PARAM6			0x88
#define AIP_P_PARAM7			0x8c
#define AIP_P_PARAM8			0x90
#define AIP_P_OFFSET			0x94
#define AIP_P_CHAIN_BASE		0x98
#define AIP_P_CHAIN_SIZE		0x9c
#define AIP_P_ISUM			0xa0
#define AIP_P_OSUM			0xa4

#define AIP_T_CTRL			0x00
#define AIP_T_IQR			0x04
#define AIP_T_CFG			0x08
#define AIP_T_TIMEOUT			0x0c
#define AIP_T_TASK_LEN			0x10
#define AIP_T_SRC_Y_BASE		0x14
#define AIP_T_SRC_UV_BASE		0x18
#define AIP_T_DST_BASE			0x1c
#define AIP_T_STRIDE			0x20
#define AIP_T_SRC_SIZE			0x24
#define AIP_T_PARAM0			0x28
#define AIP_T_PARAM1			0x2c
#define AIP_T_PARAM2			0x30
#define AIP_T_PARAM3			0x34
#define AIP_T_PARAM4			0x38
#define AIP_T_PARAM5			0x3c
#define AIP_T_PARAM6			0x40
#define AIP_T_PARAM7			0x44
#define AIP_T_PARAM8			0x48
#define AIP_T_OFFSET			0x4c
#define AIP_T_CHAIN_BASE		0x50
#define AIP_T_CHAIN_SIZE		0x54
#define AIP_T_TASL_SKIP_BASE		0x58
#define AIP_T_ISUM			0x5c
#define AIP_T_OSUM			0x60

#define AIP_F_CTRL			0x00
#define AIP_F_IQR			0x04
#define AIP_F_CFG			0x08
#define AIP_F_TIMEOUT			0x0c
#define AIP_F_SCALE_X			0x10
#define AIP_F_SCALE_Y			0x14
#define AIP_F_TRANS_X			0x18
#define AIP_F_TRANS_Y			0x1c
#define AIP_F_SRC_Y_BASE		0x20
#define AIP_F_SRC_UV_BASE		0x24
#define AIP_F_DST_Y_BASE		0x28
#define AIP_F_DST_UV_BASE		0x2c
#define AIP_F_SRC_SIZE			0x30
#define AIP_F_DST_SIZE			0x34
#define AIP_F_STRIDE			0x38
#define AIP_F_CHAIN_BASE		0x3c
#define AIP_F_CHAIN_SIZE		0x40
#define AIP_F_ISUM			0x44
#define AIP_F_OSUM			0x48

typedef enum {
	INDEX_AIP_T = 0,
	INDEX_AIP_F = 1,
	INDEX_AIP_P = 2,
} AIP_TYPE;

typedef enum {
	AIP_F_TYPE_Y = 0,
	AIP_F_TYPE_UV,
	AIP_F_TYPE_BGRA,
	AIP_F_TYPE_FEATURE2,
	AIP_F_TYPE_FEATURE4,
	AIP_F_TYPE_FEATURE8,
	AIP_F_TYPE_FEATURE8_H,
	AIP_F_TYPE_NV12,
} AIP_F_DATA_FORMAT;

typedef enum {
	AIP_NV2BGRA = 0,
	AIP_NV2GBRA,
	AIP_NV2RBGA,
	AIP_NV2BRGA,
	AIP_NV2GRBA,
	AIP_NV2RGBA,
	AIP_NV2ABGR = 8,
	AIP_NV2AGBR,
	AIP_NV2ARBG,
	AIP_NV2ABRG,
	AIP_NV2AGRB,
	AIP_NV2ARGB,
} AIP_RGB_FORMAT;

typedef enum {
	AIP_DATA_DDR = 0,
	AIP_DATA_ORAM,
} AIP_DDRORORAM;

// aip_p
struct aip_p_chainnode {
	uint32_t		mode;
	uint32_t		src_base_y;
	uint32_t		src_base_uv;
	uint32_t		src_stride;
	uint32_t		dst_base;
	uint32_t		dst_stride;
	uint16_t		dst_width;
	uint16_t		dst_height;
	uint16_t		src_width;
	uint16_t		src_height;
	uint32_t		dummy_val;
	int32_t			coef_parm0;
	int32_t			coef_parm1;
	int32_t			coef_parm2;
	int32_t			coef_parm3;
	int32_t			coef_parm4;
	int32_t			coef_parm5;
	int32_t			coef_parm6;
	int32_t			coef_parm7;
	int32_t			coef_parm8;
	int32_t			coef_parm9;
	int32_t			coef_parm10;
	int32_t			coef_parm11;
	int32_t			coef_parm12;
	int32_t			coef_parm13;
	int32_t			coef_parm14;
	uint32_t		offset;
	uint32_t		p_ctrl;
};

//aip_t
struct aip_t_chainnode {
	uint32_t		src_base_y;
	uint32_t		src_base_uv;
	uint32_t		dst_base;
	uint16_t		src_width;
	uint16_t		src_height;
	uint16_t		src_stride;
	uint16_t		dst_stride;
	uint32_t		task_len;
	uint32_t		task_skip_base;
	uint32_t		t_ctrl;
};

//aip_f
struct aip_f_chainnode {
	uint32_t		scale_x;
	uint32_t		scale_y;
	int32_t			trans_x;
	int32_t			trans_y;
	uint32_t		src_base_y;
	uint32_t		src_base_uv;
	uint32_t		dst_base_y;
	uint32_t		dst_base_uv;
	uint16_t		src_width;
	uint16_t		src_height;
	uint16_t		dst_width;
	uint16_t		dst_height;
	uint16_t		dst_stride;
	uint16_t		src_stride;
	uint32_t		f_ctrl;
};


int aip_p_init(struct aip_p_chainnode *chains, int chain_num,
		const uint32_t *nv2rgb_parm, uint32_t cfg);
int aip_p_exit();

int aip_t_init(struct aip_t_chainnode *chains, int chain_num,
		const uint32_t *nv2rgb_parm,  uint32_t offset, uint32_t cfg);
int aip_t_exit();

int aip_f_init(struct aip_f_chainnode *chains, int chain_num, uint32_t cfg);
int aip_f_exit();

int aip_p_start(int chain_num);
int aip_t_start(uint32_t task_len, uint32_t task_skip_base);
int aip_f_start(int chain_num);

int aip_p_wait();
int aip_t_wait();
int aip_f_wait();


#endif

