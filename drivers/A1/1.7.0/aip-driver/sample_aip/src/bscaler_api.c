#include <string.h>
#include "bscaler_api.h"
#include "jz_aip.h"
#include "aie_mmap.h"


int bs_affine_start(data_info_s *src,
		const int box_num, const data_info_s *dst,
		const box_affine_info_s *boxes,
		const uint32_t *coef, const uint32_t *offset)
{
	int i,k;
	int ret = 0;
	uint8_t mode = 0;
	uint32_t ofst = 0;
	uint32_t alpha = 0 ,cfg = 0;
	AIP_DDRORORAM src_locate, dst_locate;
	AIP_RGB_FORMAT dst_format;
	uint32_t src_base_y = 0, src_base_uv = 0;
	uint32_t dst_base = 0;
	uint32_t offset_y = 0, offset_uv = 0;
	float matrix[9];
	int64_t ax,bx,cx,ay,by,cy,az,bz,cz;
	int x, y, h, w;

	struct aip_p_chainnode *node = NULL;
	node = (struct aip_p_chainnode *)malloc(sizeof(struct aip_p_chainnode) * box_num);
	if(!node)
		return -1;
	memset(node, 0x00, sizeof(struct aip_p_chainnode) * box_num);

	for(i = 0; i < box_num; i++) {
		x = boxes[i].box.x;
		y = boxes[i].box.y;
		w = boxes[i].box.w;
		h = boxes[i].box.h;

		src_base_y 	= src->base;
		dst_base 	= dst[i].base;
		if(BS_DATA_BGRA <= src->format && src->format <= BS_DATA_ARGB) {
			offset_y 	= src->line_stride * y + x * 4;
			mode 		= 3;
		} else if(src->format == BS_DATA_NV12) {
			if(src->base1 == NULL)
				src->base1 = src->base + src->width * src->height;
			else
				src->base1 = src->base1;
			src_base_uv 	= src->base1;
			offset_y	= src->line_stride * y + x;
			offset_uv 	= src->line_stride * y / 2  + x;
			mode 		= 2;
		} else if(src->format == BS_DATA_UV) {
			offset_y 	= src->line_stride * y + x * 2;
			mode 		= 1;
		} else if(src->format == BS_DATA_Y) {
			offset_y 	= src->line_stride * y + x;
			mode 		= 0;
		}

		node[i].mode 		= mode;
		node[i].src_base_y 	= src_base_y + offset_y;
		node[i].src_base_uv 	= src_base_uv + offset_uv;
		node[i].src_stride 	= src->line_stride;
		node[i].dst_base 	= dst_base;
		node[i].dst_stride 	= dst[i].line_stride;
		node[i].dst_width 	= dst[i].width;
		node[i].dst_height 	= dst[i].height;
		node[i].src_width 	= w;//src->width;
		node[i].src_height 	= h;//src->height;
		node[i].dummy_val	= boxes[i].zero_point;

		for(k = 0; k < 9; k++) {
			matrix[k] = boxes[i].matrix[k];
		}
		double D = matrix[0]*matrix[4] - matrix[1]*matrix[3];
		D = D != 0 ? 1./D : 0;
		double A11 = matrix[4]*D, A22=matrix[0]*D;
		matrix[0] = A11;
		matrix[1] *= -D;
		matrix[3] *= -D;
		matrix[4] = A22;
		double b1 = -matrix[0]*matrix[2] - matrix[1]*matrix[5];
		double b2 = -matrix[3]*matrix[2] - matrix[4]*matrix[5];
		matrix[2] = b1;
		matrix[5] = b2;

		ax = (int64_t)round(matrix[0] * 65536);
		bx = (int64_t)round(matrix[1] * 65536);
		cx = (int64_t)round(matrix[2] * 65536);
		ay = (int64_t)round(matrix[3] * 65536);
		by = (int64_t)round(matrix[4] * 65536);
		cy = (int64_t)round(matrix[5] * 65536);
		az = (int64_t)round(matrix[6] * 65536);
		bz = (int64_t)round(matrix[7] * 65536);
		cz = (int64_t)round(matrix[8] * 65536);
		/* printf("A1:*******************\n"); */
		/* printf("a1:%lld %lld %lld\n",ax,bx,cx); */
		/* printf("a1:%lld %lld %lld\n",ay,by,cy); */
		/* printf("a1:%lld %lld %lld\n",az,bz,cz); */

		node[i].coef_parm0 = (ax & 0xffffffff);
		node[i].coef_parm1 = ((bx & 0xffff) << 16) | ((ax & 0xffff00000000) >> 32);
		node[i].coef_parm2 = (bx & 0xffffffff0000) >> 16;
		node[i].coef_parm3 = (cx & 0xffffffff);
		node[i].coef_parm4 = (cx & 0x7ffffff00000000) >> 32;

		node[i].coef_parm5 = (ay & 0xffffffff);
		node[i].coef_parm6 = ((by & 0xffff) << 16) | ((ay & 0xffff00000000) >> 32);
		node[i].coef_parm7 = (by & 0xffffffff0000) >> 16;
		node[i].coef_parm8 = (cy & 0xffffffff);
		node[i].coef_parm9 = (cy & 0x7ffffff00000000) >> 32;

		node[i].coef_parm10 = (az & 0xffffffff);
		node[i].coef_parm11 = ((bz & 0xffff) << 16) | ((az & 0xffff00000000) >> 32);
		node[i].coef_parm12 = (bz & 0xffffffff0000) >> 16;
		node[i].coef_parm13 = (cz & 0xffffffff);
		node[i].coef_parm14 = (cz & 0x7ffffff00000000) >> 32;

		switch (dst[i].format) {
			case BS_DATA_Y:    dst_format = AIP_NV2BGRA; break;
			case BS_DATA_UV:   dst_format = AIP_NV2BGRA; break;
			case BS_DATA_NV12: dst_format = AIP_NV2BGRA; break;
			case BS_DATA_BGRA: dst_format = AIP_NV2BGRA; break;
			case BS_DATA_GBRA: dst_format = AIP_NV2GBRA; break;
			case BS_DATA_RBGA: dst_format = AIP_NV2RBGA; break;
			case BS_DATA_BRGA: dst_format = AIP_NV2BRGA; break;
			case BS_DATA_GRBA: dst_format = AIP_NV2GRBA; break;
			case BS_DATA_RGBA: dst_format = AIP_NV2RGBA; break;
			case BS_DATA_ABGR: dst_format = AIP_NV2ABGR; break;
			case BS_DATA_AGBR: dst_format = AIP_NV2AGBR; break;
			case BS_DATA_ARBG: dst_format = AIP_NV2ARBG; break;
			case BS_DATA_ABRG: dst_format = AIP_NV2ABRG; break;
			case BS_DATA_AGRB: dst_format = AIP_NV2AGRB; break;
			case BS_DATA_ARGB: dst_format = AIP_NV2ARGB; break;
			default :
							   printf("src->format type  error\n");
							   break;
		}
		switch (src->locate) {
			case BS_DATA_NMEM: src_locate = AIP_DATA_DDR; break;
			case BS_DATA_ORAM: src_locate = AIP_DATA_ORAM; break;
			case BS_DATA_RMEM: src_locate = AIP_DATA_DDR; break;
			default :
							   printf("src->locate type error\n");
							   break;
		}

		switch (dst[i].locate) {
			case BS_DATA_NMEM: dst_locate = AIP_DATA_DDR; break;
			case BS_DATA_ORAM: dst_locate = AIP_DATA_ORAM; break;
			case BS_DATA_RMEM: dst_locate = AIP_DATA_DDR; break;
			default :
							   printf("dst->locate type error\n");
							   break;
		}
		node[i].offset = dst_format<<24 | alpha<<16 | offset[1]<<8 | offset[0];
	}

	cfg = dst_locate<<1 | src_locate;
	ret = aip_p_init(node, box_num, coef, cfg);
	if(ret)
		printf("aip_p_init error\n");

	aip_p_start(box_num);
	free(node);

	return 0;
}

int bs_affine_wait()
{
	int ret = 0;
	ret = aip_p_wait();
	if(ret)
		printf("aip_p_wait error\n");
	aip_p_exit();

	return ret;
}

int bs_perspective_start(data_info_s *src,
		const int box_num, const data_info_s *dst,
		const box_affine_info_s *boxes,
		const uint32_t *coef, const uint32_t *offset)
{
	int i,k;
	int ret = 0;
	uint8_t mode = 0 ;
	uint32_t ofst = 0;
	uint32_t alpha  = 0 ,cfg = 0;
	AIP_DDRORORAM src_locate, dst_locate;
	AIP_RGB_FORMAT dst_format;
	uint32_t src_base_y = 0, src_base_uv = 0;
	uint32_t dst_base = 0;
	uint32_t offset_y = 0, offset_uv = 0;
	float matrix[9];
	int64_t ax,bx,cx,ay,by,cy,az,bz,cz;
	int x, y, h, w;

	struct aip_p_chainnode *node = NULL;
	node = (struct aip_p_chainnode *)malloc(sizeof(struct aip_p_chainnode) * box_num);
	memset(node, 0x00, sizeof(struct aip_p_chainnode) * box_num);
	if(!node)
		return -1;

	for(i = 0; i < box_num; i++) {
		x = boxes[i].box.x;
		y = boxes[i].box.y;
		w = boxes[i].box.w;
		h = boxes[i].box.h;

		src_base_y 	= src->base;
		dst_base 	= dst[i].base;
		if(BS_DATA_BGRA <= src->format && src->format <= BS_DATA_ARGB) {
			offset_y 	= src->line_stride * y + x * 4;
			mode 		= 3;
		} else if(src->format == BS_DATA_NV12) {
			if(src->base1 == NULL)
				src->base1 = src->base + src->width * src->height;
			else
				src->base1 = src->base1;
			src_base_uv 	= src->base1;
			offset_y	= src->line_stride * y + x;
			offset_uv 	= src->line_stride * y / 2  + x;
			mode 		= 2;
		} else if(src->format == BS_DATA_UV) {
			offset_y 	= src->line_stride * y + x * 2;
			mode 		= 1;
		} else if(src->format == BS_DATA_Y) {
			offset_y 	= src->line_stride * y + x;
			mode 		= 0;
		}

		node[i].mode 		= mode;
		node[i].src_base_y 	= src_base_y + offset_y;
		node[i].src_base_uv 	= src_base_uv + offset_uv;
		node[i].src_stride 	= src->line_stride;
		node[i].dst_base 	= dst_base;
		node[i].dst_stride 	= dst->line_stride;
		node[i].dst_width 	= dst[i].width;
		node[i].dst_height 	= dst[i].height;
		node[i].src_width 	= w;//src->width;
		node[i].src_height 	= h;//src->height;
		node[i].dummy_val	= boxes[i].zero_point;

		for(k = 0; k < 9; k++) {
			matrix[k] = boxes[i].matrix[k];
		}
		//transformation of matrix
		double t0 = matrix[0]*(matrix[4]*matrix[8]-matrix[5]*matrix[7]);
		double t1 = matrix[1]*(matrix[3]*matrix[8]-matrix[5]*matrix[6]);
		double t2 = matrix[2]*(matrix[3]*matrix[7]-matrix[4]*matrix[6]);
		double t3 = t0-t1+t2;
		double dst_m[9]={0};
		if(t3 != 0.0)
		{
			float def = 1.0 / t3;
			dst_m[0] = (matrix[4]*matrix[8]-matrix[5]*matrix[7])*def;
			dst_m[1] = (matrix[2]*matrix[7]-matrix[1]*matrix[8])*def;
			dst_m[2] = (matrix[1]*matrix[5]-matrix[2]*matrix[4])*def;

			dst_m[3] = (matrix[5]*matrix[6]-matrix[3]*matrix[8])*def;
			dst_m[4] = (matrix[0]*matrix[8]-matrix[2]*matrix[6])*def;
			dst_m[5] = (matrix[2]*matrix[3]-matrix[0]*matrix[5])*def;

			dst_m[6] = (matrix[3]*matrix[7]-matrix[4]*matrix[6])*def;
			dst_m[7] = (matrix[1]*matrix[6]-matrix[0]*matrix[7])*def;
			dst_m[8] = (matrix[0]*matrix[4]-matrix[1]*matrix[3])*def;
		}

		ax = (int64_t)round(dst_m[0] * 65536 * 1024); //26bit fixed poind
		bx = (int64_t)round(dst_m[1] * 65536 * 1024);
		cx = (int64_t)round(dst_m[2] * 65536 * 1024);
		ay = (int64_t)round(dst_m[3] * 65536 * 1024);
		by = (int64_t)round(dst_m[4] * 65536 * 1024);
		cy = (int64_t)round(dst_m[5] * 65536 * 1024);
		az = (int64_t)round(dst_m[6] * 65536 * 1024);
		bz = (int64_t)round(dst_m[7] * 65536 * 1024);
		cz = (int64_t)round(dst_m[8] * 65536 * 1024);

		node[i].coef_parm0 = (ax & 0xffffffff);
		node[i].coef_parm1 = ((bx & 0xffff) << 16) | ((ax & 0xffff00000000) >> 32);
		node[i].coef_parm2 = (bx & 0xffffffff0000) >> 16;
		node[i].coef_parm3 = (cx & 0xffffffff);
		node[i].coef_parm4 = (cx & 0x7ffffff00000000) >> 32;

		node[i].coef_parm5 = (ay & 0xffffffff);
		node[i].coef_parm6 = ((by & 0xffff) << 16) | ((ay & 0xffff00000000) >> 32);
		node[i].coef_parm7 = (by & 0xffffffff0000) >> 16;
		node[i].coef_parm8 = (cy & 0xffffffff);
		node[i].coef_parm9 = (cy & 0x7ffffff00000000) >> 32;

		node[i].coef_parm10 = (az & 0xffffffff);
		node[i].coef_parm11 = ((bz & 0xffff) << 16) | ((az & 0xffff00000000) >> 32);
		node[i].coef_parm12 = (bz & 0xffffffff0000) >> 16;
		node[i].coef_parm13 = (cz & 0xffffffff);
		node[i].coef_parm14 = (cz & 0x7ffffff00000000) >> 32;

		switch (dst[i].format) {
			case BS_DATA_Y:    dst_format = AIP_NV2BGRA; break;
			case BS_DATA_UV:   dst_format = AIP_NV2BGRA; break;
			case BS_DATA_NV12: dst_format = AIP_NV2BGRA; break;
			case BS_DATA_BGRA: dst_format = AIP_NV2BGRA; break;
			case BS_DATA_GBRA: dst_format = AIP_NV2GBRA; break;
			case BS_DATA_RBGA: dst_format = AIP_NV2RBGA; break;
			case BS_DATA_BRGA: dst_format = AIP_NV2BRGA; break;
			case BS_DATA_GRBA: dst_format = AIP_NV2GRBA; break;
			case BS_DATA_RGBA: dst_format = AIP_NV2RGBA; break;
			case BS_DATA_ABGR: dst_format = AIP_NV2ABGR; break;
			case BS_DATA_AGBR: dst_format = AIP_NV2AGBR; break;
			case BS_DATA_ARBG: dst_format = AIP_NV2ARBG; break;
			case BS_DATA_ABRG: dst_format = AIP_NV2ABRG; break;
			case BS_DATA_AGRB: dst_format = AIP_NV2AGRB; break;
			case BS_DATA_ARGB: dst_format = AIP_NV2ARGB; break;
			default :
							   printf("src->format type  error\n");
							   break;
		}
		switch (src->locate) {
			case BS_DATA_NMEM: src_locate = AIP_DATA_DDR; break;
			case BS_DATA_ORAM: src_locate = AIP_DATA_ORAM; break;
			case BS_DATA_RMEM: src_locate = AIP_DATA_DDR; break;
			default :
							   printf("src->locate type error\n");
							   break;
		}

		switch (dst[i].locate) {
			case BS_DATA_NMEM: dst_locate = AIP_DATA_DDR; break;
			case BS_DATA_ORAM: dst_locate = AIP_DATA_ORAM; break;
			case BS_DATA_RMEM: dst_locate = AIP_DATA_DDR; break;
			default :
							   printf("dst->locate type error\n");
							   break;
		}
		node[i].offset = dst_format<<24 | alpha<<16 | offset[1]<<8 | offset[0];
	}

	cfg = dst_locate<<1 | src_locate;
	ret = aip_p_init(node, box_num, coef, cfg);
	if(ret)
		printf("aip_p_init error\n");

	aip_p_start(box_num);
	free(node);

	return 0;
}

int bs_perspective_wait()
{
	int ret = 0;
	ret = aip_p_wait();
	if(ret)
		printf("aip_p_wait error\n");
	aip_p_exit();
	return ret;
}

int bs_covert_cfg(data_info_s *src, const data_info_s *dst,
		const uint32_t *coef, const uint32_t *offset,
		const task_info_s *task_info)
{
	int ret = 0;
	uint32_t ofst;
	uint32_t alpha, cfg;
	uint32_t src_base_y, src_base_uv;
	uint32_t dst_base;
	AIP_DDRORORAM src_locate, dst_locate;
	AIP_RGB_FORMAT dst_format;

	struct aip_t_chainnode *node = NULL;
	node = (struct aip_t_chainnode *)malloc(sizeof(struct aip_t_chainnode));
	memset(node, 0x00, sizeof(struct aip_t_chainnode));
	if(!node)
		return -1;

	if(src->base1 == NULL)
		src->base1 = src->base + src->width * src->height;
	else
		src->base1 = src->base1;
	src_base_y 	= src->base;
	src_base_uv 	= src->base1;
	dst_base 	= dst->base;

	node->src_base_y 	= src_base_y;
	node->src_base_uv 	= src_base_uv;
	node->dst_base 		= dst_base;
	node->src_width 	= src->width;
	node->src_height 	= src->height;
	node->src_stride 	= src->line_stride;
	node->dst_stride 	= dst->line_stride;

	switch (dst->format) {
		case BS_DATA_BGRA: dst_format = AIP_NV2BGRA; break;
		case BS_DATA_GBRA: dst_format = AIP_NV2GBRA; break;
		case BS_DATA_RBGA: dst_format = AIP_NV2RBGA; break;
		case BS_DATA_BRGA: dst_format = AIP_NV2BRGA; break;
		case BS_DATA_GRBA: dst_format = AIP_NV2GRBA; break;
		case BS_DATA_RGBA: dst_format = AIP_NV2RGBA; break;
		case BS_DATA_ABGR: dst_format = AIP_NV2ABGR; break;
		case BS_DATA_AGBR: dst_format = AIP_NV2AGBR; break;
		case BS_DATA_ARBG: dst_format = AIP_NV2ARBG; break;
		case BS_DATA_ABRG: dst_format = AIP_NV2ABRG; break;
		case BS_DATA_AGRB: dst_format = AIP_NV2AGRB; break;
		case BS_DATA_ARGB: dst_format = AIP_NV2ARGB; break;
		default :
						   printf("src->format type  error\n");
						   break;
	}
	switch (src->locate) {
		case BS_DATA_NMEM: src_locate = AIP_DATA_DDR; break;
		case BS_DATA_ORAM: src_locate = AIP_DATA_ORAM; break;
		case BS_DATA_RMEM: src_locate = AIP_DATA_DDR; break;
		default :
						   printf("src->locate type error\n");
						   break;
	}

	switch (dst->locate) {
		case BS_DATA_NMEM: dst_locate = AIP_DATA_DDR; break;
		case BS_DATA_ORAM: dst_locate = AIP_DATA_ORAM; break;
		case BS_DATA_RMEM: dst_locate = AIP_DATA_DDR; break;
		default :
						   printf("dst->locate type error\n");
						   break;
	}

	alpha = 0;;
	ofst= dst_format<<24 | alpha<<16 | offset[1]<<8 | offset[0];

	cfg = dst_locate<<1 | src_locate;
	ret = aip_t_init(node, 1, coef, ofst, cfg);
	if(ret)
		printf("aip_t_init error\n");
	free(node);

	return ret;
}

int bs_covert_step_start(const task_info_s *task_info, uint32_t dst_ptr, const bs_data_locate_e locate)
{
	int ret = 0;
	uint32_t task_len = task_info->task_len;
	uint32_t skip_dst_base = dst_ptr;

	aip_t_start(task_len, skip_dst_base);

	return ret;
}
int bs_covert_step_wait()
{
	int ret = 0;
	ret = aip_t_wait();
	if(ret)
		printf("aip_t_wait error\n");

	return ret;
}

int bs_covert_step_exit()
{

	aip_t_exit();

	return 0;
}

int bs_resize_start(data_info_s *src,
		const int box_num,data_info_s *dst,
		const box_resize_info_s *boxes,
		const uint32_t *coef, const uint32_t *offset)
{
	int i;
	int ret = 0;
	int x, y, h, w;
	float scale_x_f, scale_y_f;
	uint32_t src_base_y = 0, src_base_uv = 0;
	uint32_t dst_base_y = 0, dst_base_uv = 0;
	uint32_t offset_y = 0, offset_uv = 0;
	uint32_t cfg;
	uint8_t *dst_base_rd;
	struct aip_f_chainnode *node = NULL;
	AIP_DDRORORAM src_locate, dst_locate;
	AIP_F_DATA_FORMAT src_format;

	node = (struct aip_f_chainnode *)malloc(sizeof(struct aip_f_chainnode) * (box_num + 1));
	memset(node, 0x00, sizeof(struct aip_f_chainnode) * (box_num + 1));
	if(!node)
		return -1;

	for(i = 0; i < box_num; i++){
		x = boxes[i].box.x;
		y = boxes[i].box.y;
		w = boxes[i].box.w;
		h = boxes[i].box.h;
		scale_x_f = (float)w / (float)dst[i].width ;
		scale_y_f = (float)h / (float)dst[i].height ;

		src_base_y = src->base;
		dst_base_y = dst[i].base;
		if(BS_DATA_BGRA <= src->format && src->format <= BS_DATA_ARGB) {
			offset_y 	= src->line_stride * y + x * 4;
		} else if(src->format == BS_DATA_NV12) {
			if(src->base1 == NULL)
				src->base1 = src->base + src->width * src->height;
			else
				src->base1 = src->base1;
			src_base_uv	= src->base1;
			dst_base_uv	= dst[i].base1;
			offset_y	= src->line_stride * y + x;
			offset_uv 	= src->line_stride * y / 2 + x;
		} else if(src->format == BS_DATA_Y || src->format == BS_DATA_UV) {
			offset_y 	= src->line_stride * y + x;
		} else if(src->format == BS_DATA_FMU2) {
			offset_y 	= src->line_stride * y + 8 * x;
		} else if(src->format == BS_DATA_FMU4) {
			offset_y 	= src->line_stride * y + 16 * x;
		} else if(src->format == BS_DATA_FMU8) {
			offset_y 	= src->line_stride * y + 32 * x;
		} else if(src->format == BS_DATA_FMU8_H) {
			offset_y 	= src->line_stride * y + 16 * x;
		}

		node[i].scale_x 	= (uint32_t)(scale_x_f * 65536);
		node[i].scale_y 	= (uint32_t)(scale_y_f * 65536);
		node[i].trans_x 	= (int32_t)((scale_x_f * 0.5 - 0.5) * 65536);
		node[i].trans_y 	= (int32_t)((scale_y_f * 0.5 - 0.5) * 65536);
		node[i].src_base_y 	= src_base_y + offset_y;
		node[i].src_base_uv 	= src_base_uv + offset_uv;
		node[i].dst_base_y 	= dst_base_y;
		node[i].dst_base_uv 	= dst_base_uv;
		node[i].src_width 	= w;//src->width;
		node[i].src_height 	= h;//src->height;
		node[i].dst_width 	= dst[i].width;
		node[i].dst_height	= dst[i].height;
		node[i].dst_stride 	= dst[i].line_stride;
		node[i].src_stride 	= src->line_stride;
	}
#if 1
	int size_rd = 32;
	if(dst[i - 1].locate == BS_DATA_NMEM || dst[i-1].locate == BS_DATA_RMEM)
	{
		dst_base_rd = (uint8_t *)ddr_memalign(32,  size_rd);
		__aie_flushcache_dir((void *)dst_base_rd, size_rd, NNA_DMA_FROM_DEVICE);

		dst[i].base = __aie_get_ddr_paddr((uint32_t)dst_base_rd);

	}else if(dst[i -1].locate == BS_DATA_ORAM){
		dst_base_rd = (uint8_t *)oram_memalign(32, size_rd);
		__aie_flushcache_dir((void *)dst_base_rd, size_rd, NNA_DMA_FROM_DEVICE);

		dst[i].base = __aie_get_oram_paddr((uint32_t)dst_base_rd);

	}

	node[i].scale_x 	= (uint32_t)(scale_x_f * 65536);
	node[i].scale_y 	= (uint32_t)(scale_y_f * 65536);
	node[i].trans_x 	= (int32_t)((scale_x_f * 0.5 - 0.5) * 65536);
	node[i].trans_y 	= (int32_t)((scale_y_f * 0.5 - 0.5) * 65536);
	node[i].src_base_y 	= src_base_y + offset_y;
	node[i].src_base_uv 	= src_base_uv + offset_uv;
	node[i].dst_base_y 	= dst[i].base;
	node[i].dst_base_uv 	= dst[1].base;
	node[i].src_width 	= 2;//src->width;
	node[i].src_height 	= 2;//src->height;
	node[i].dst_width 	= 2;
	node[i].dst_height	= 2;
	node[i].dst_stride 	= dst[i-1].line_stride;
	node[i].src_stride 	= src->line_stride;
#endif
	switch (src->format) {
		case BS_DATA_Y: 	src_format = AIP_F_TYPE_Y; break;
		case BS_DATA_UV: 	src_format = AIP_F_TYPE_UV; break;
		case BS_DATA_NV12: 	src_format = AIP_F_TYPE_NV12; break;
		case BS_DATA_BGRA ... BS_DATA_ARGB: src_format = AIP_F_TYPE_BGRA; break;
		case BS_DATA_FMU2: 	src_format = AIP_F_TYPE_FEATURE2; break;
		case BS_DATA_FMU4: 	src_format = AIP_F_TYPE_FEATURE4; break;
		case BS_DATA_FMU8: 	src_format = AIP_F_TYPE_FEATURE8; break;
		case BS_DATA_FMU8_H: src_format = AIP_F_TYPE_FEATURE8_H; break;
		default :
							 printf("src->format type  error\n");
							 break;
	}

	switch (src->locate) {
		case BS_DATA_NMEM: src_locate = AIP_DATA_DDR; break;
		case BS_DATA_ORAM: src_locate = AIP_DATA_ORAM; break;
		case BS_DATA_RMEM: src_locate = AIP_DATA_DDR; break;
		default :
						   printf("src->locate type error\n");
						   break;
	}

	switch (dst->locate) {
		case BS_DATA_NMEM: dst_locate = AIP_DATA_DDR; break;
		case BS_DATA_ORAM: dst_locate = AIP_DATA_ORAM; break;
		case BS_DATA_RMEM: dst_locate = AIP_DATA_DDR; break;
		default :
						   printf("dst->locate type error\n");
						   break;
	}
	cfg = src_format<<7 | dst_locate<<1 | src_locate;

	ret = aip_f_init(node, box_num, cfg);
	if(ret)
		printf("aip_f_init error\n ");

	aip_f_start(box_num);
	if(dst[i - 1].locate == BS_DATA_NMEM){
		ddr_free(dst_base_rd);
	}else if(dst[i -1].locate == BS_DATA_ORAM){
		oram_free(dst_base_rd);
	}
	free(node);

	return ret;
}

int bs_resize_wait()
{
	int ret = 0;
	ret = aip_f_wait();
	if(ret)
		printf("aip_f_wait error\n");
	aip_f_exit();

	return ret;
}

void bs_version()
{
	printf("@@@@ JZ AIP Version is 20210818a\n");
}
