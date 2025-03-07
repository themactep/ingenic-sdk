/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : aip_p_mdl.c
 * Authors    : cbzhang@abel.ic.jz.com
 * Create Time: 2020-11-19:18:05:19
 * Description:
 *
 */
#include "aip_p_mdl.h"
#define WEIGHT_PRECISION_W 5
#define WEIGHT_PRECISION (1<<WEIGHT_PRECISION_W)

int aip_p_debug_dx = -1;
int aip_p_debug_dy = -1;
int aip_p_debug_dc = -1;

#define AIP_T_CSCA     (20)
#define CLIP(val, min, max)     ((val) < (min) ? (min) : (val) > (max) ? (max) : (val))

void aip_p_debug_point(int dx, int dy, int dc)
{
	aip_p_debug_dx = dx;
	aip_p_debug_dy = dy;
	aip_p_debug_dc = dc;
}

void aip_t_nv2bgr_core(uint8_t y,
		uint8_t u,
		uint8_t v,
		const int32_t *coef,
		const uint8_t *offset,
		const uint8_t nv2bgr_alpha,
		int order,
		uint8_t *rgba)
{
	int16_t y_m = y - offset[0];
	int16_t u_m = u - offset[1];
	int16_t v_m = v - offset[1];
	int64_t b32 = ((int64_t)coef[0] * y_m + (int64_t)coef[1] * u_m +
			(int64_t)coef[2] * v_m + (1 << (AIP_T_CSCA - 1)));
	int64_t g32 = ((int64_t)coef[3] * y_m + (int64_t)coef[4] * u_m +
			(int64_t)coef[5] * v_m + (1 << (AIP_T_CSCA - 1)));
	int64_t r32 = ((int64_t)coef[6] * y_m + (int64_t)coef[7] * u_m +
			(int64_t)coef[8] * v_m + (1 << (AIP_T_CSCA - 1)));
	uint8_t a = nv2bgr_alpha;//fixme
	uint8_t b = CLIP(b32, 0, (1 << (AIP_T_CSCA + 8)) - 1) >> AIP_T_CSCA;//must do clip
	uint8_t g = CLIP(g32, 0, (1 << (AIP_T_CSCA + 8)) - 1) >> AIP_T_CSCA;//must do clip
	uint8_t r = CLIP(r32, 0, (1 << (AIP_T_CSCA + 8)) - 1) >> AIP_T_CSCA;//must do clip
	switch (order) {
	case 0://BGRA
		rgba[0] = b;
		rgba[1] = g;
		rgba[2] = r;
		rgba[3] = a;
		break;
	case 1:
		rgba[0] = g;
		rgba[1] = b;
		rgba[2] = r;
		rgba[3] = a;
		break;
	case 2:
		rgba[0] = r;
		rgba[1] = b;
		rgba[2] = g;
		rgba[3] = a;
		break;
	case 3:
		rgba[0] = b;
		rgba[1] = r;
		rgba[2] = g;
		rgba[3] = a;
		break;
	case 4:
		rgba[0] = g;
		rgba[1] = r;
		rgba[2] = b;
		rgba[3] = a;
		break;
	case 5:
		rgba[0] = r;
		rgba[1] = g;
		rgba[2] = b;
		rgba[3] = a;
		break;
	case 8:
		rgba[0] = a;
		rgba[1] = b;
		rgba[2] = g;
		rgba[3] = r;
		break;
	case 9:
		rgba[0] = a;
		rgba[1] = g;
		rgba[2] = b;
		rgba[3] = r;
		break;
	case 10:
		rgba[0] = a;
		rgba[1] = r;
		rgba[2] = b;
		rgba[3] = g;
		break;
	case 11:
		rgba[0] = a;
		rgba[1] = b;
		rgba[2] = r;
		rgba[3] = g;
		break;
	case 12:
		rgba[0] = a;
		rgba[1] = g;
		rgba[2] = r;
		rgba[3] = b;
		break;
	case 13:
		rgba[0] = a;
		rgba[1] = r;
		rgba[2] = g;
		rgba[3] = b;
		break;
	default:
		fprintf(stderr, "[Error][%s]: not support this order(%d)\n", __func__, order);
	}
}
void aip_t_mdl(uint8_t *src_y,
		uint8_t* src_c,
		uint32_t src_s,
		uint8_t *dst,
		uint32_t dst_s,
		int width,
		int height,
		uint32_t nv2bgr_order,
		int32_t *nv2bgr_coef,
		uint8_t *nv2bgr_ofst,
		uint8_t nv2bgr_alpha)
{
	uint8_t *rgb_buffer = dst;
	int x, y;
	for (y = 0; y < height; y += 2) {
		for (x = 0; x < width; x += 2) {
			uint8_t pix_y0 = src_y[src_s * y + x];
			uint8_t pix_y1 = src_y[src_s * y + x + 1];
			uint8_t pix_y2 = src_y[src_s * (y + 1) + x];
			uint8_t pix_y3 = src_y[src_s * (y + 1) + (x + 1)];
			uint8_t pix_u  = src_c[src_s * y/2 + x];
			uint8_t pix_v  = src_c[src_s * y/2 + x + 1];
			uint8_t *dst_ptr = rgb_buffer + dst_s * y + x * 4;
			aip_t_nv2bgr_core(pix_y0, pix_u, pix_v, nv2bgr_coef, nv2bgr_ofst, nv2bgr_alpha,
					nv2bgr_order, dst_ptr);
			aip_t_nv2bgr_core(pix_y1, pix_u, pix_v, nv2bgr_coef, nv2bgr_ofst, nv2bgr_alpha,
					nv2bgr_order, dst_ptr + 4);
			aip_t_nv2bgr_core(pix_y2, pix_u, pix_v, nv2bgr_coef, nv2bgr_ofst, nv2bgr_alpha,
					nv2bgr_order, dst_ptr + dst_s);
			aip_t_nv2bgr_core(pix_y3, pix_u, pix_v, nv2bgr_coef, nv2bgr_ofst, nv2bgr_alpha,
					nv2bgr_order, dst_ptr + dst_s + 4);
			//printf("%d, %d, %d, %d, %d, %d -- %d, %d, %d, %d\n", pix_y0, pix_y1, pix_y2, pix_y3, pix_u, pix_v, dst_ptr[0], dst_ptr[1], dst_ptr[2], dst_ptr[3]);
		}
	}
}

/* void aip_t_nv2bgr_core(uint8_t y, */
		/* uint8_t u, */
		/* uint8_t v, */
		/* const int32_t *coef, */
		/* const uint8_t *offset, */
		/* const uint8_t nv2bgr_alpha, */
		/* int order, */
		/* uint8_t *rgba); */

int aip_p_dump(FILE *fp, char *s, char *p, char size, int h, int w, int stride){
	int x, y;
	fprintf(fp, "{//%0dx%0d; %s\n", h, w, s);
	if (size== 1) { //char
		unsigned char *pp= (unsigned char *)p;
		for (y= 0; y< h; y++) {
			for (x= 0; x< w; x++) {
				fprintf(fp, "0x%0x,", pp[y*stride+ x]);
			}
			fprintf(fp, "//y= 0x%0x\n", y);
		}
	} else if (size== 2) { //short
		unsigned short *pp= (unsigned short *)p;
		for (y= 0; y< h; y++) {
			for (x= 0; x< w; x++) {
				fprintf(fp, "0x%0x,", pp[y*stride+ x]);
			}
			fprintf(fp, "//y= 0x%0x\n", y);
		}
	} else if (size== 4) { //int
		unsigned int *pp= (unsigned int *)p;
		for (y= 0; y< h; y++) {
			for (x= 0; x< w; x++) {
				fprintf(fp, "0x%0x,", pp[y*stride+ x]);
			}
			fprintf(fp, "//y= 0x%0x\n", y);
		}
	}
	fprintf(fp, "}\n");
	return 0;
}

int64_t aip_p_clip(int64_t val, int64_t min_val, int64_t max_val) {
	if (val > max_val)
		return max_val;
	else if (val < min_val)
		return min_val;
	else
		return val;
}

uint8_t aip_p_bilinear_interplotion(uint8_t src0, uint8_t src1,
		uint8_t src2, uint8_t src3,
		uint16_t sx_wgt, uint16_t sy_wgt) {
	uint32_t tmp_x0_unshift = src0*(WEIGHT_PRECISION - sx_wgt) + src1*sx_wgt; //19bits
	uint32_t tmp_x1_unshift = src2*(WEIGHT_PRECISION - sx_wgt) + src3*sx_wgt; //19bits
	uint32_t tmp_x0 = tmp_x0_unshift;
	uint32_t tmp_x1 = tmp_x1_unshift;
	uint32_t tmp_y  = tmp_x0*(WEIGHT_PRECISION - sy_wgt) + tmp_x1*sy_wgt; //32bits
	uint8_t dst = (tmp_y + (WEIGHT_PRECISION*WEIGHT_PRECISION)/2)/(WEIGHT_PRECISION*WEIGHT_PRECISION);
	return dst;
}

void aip_p_mdl(AIP_P_CFG_S *s) {
	for (uint32_t dy = 0; dy < s->dh; dy++) {
		for (uint32_t dx = 0; dx < s->dw; dx++) {
			if (s->debug_en)
				printf("[0x%0x][0x%0x]----------------------\n", dy, dx);
			int64_t sx = ((int64_t)s->ax)*dx + ((int64_t)s->bx)*dy + s->cx; //48bits
			int64_t sy = ((int64_t)s->ay)*dx + ((int64_t)s->by)*dy + s->cy;
			int64_t sz = ((int64_t)s->az)*dx + ((int64_t)s->bz)*dy + s->cz;
			//high 48bit: pos; low 5 bit: weight
			int64_t sx_quo;
			int64_t sy_quo;
			if(sz) {
				sx_quo = (sx*WEIGHT_PRECISION)/sz;
				sy_quo = (sy*WEIGHT_PRECISION)/sz;
			} else {
				sx_quo = 0;
				sy_quo = 0;
			}
			uint16_t sx_wgt_t = ((uint64_t)sx_quo)%WEIGHT_PRECISION;
			uint16_t sy_wgt_t = ((uint64_t)sy_quo)%WEIGHT_PRECISION;
			uint16_t sx_wgt = sx_wgt_t;
			uint16_t sy_wgt = sy_wgt_t;
			int16_t sx_pos = aip_p_clip(sx_quo>>WEIGHT_PRECISION_W, -2, s->sw);
			int16_t sy_pos = aip_p_clip(sy_quo>>WEIGHT_PRECISION_W, -2, s->sh);
			/* if (s->debug_en)  */
				if (dx == aip_p_debug_dx && dy == aip_p_debug_dy) {
					printf("sx = 0x%llx(%lld), sy = 0x%llx(%lld), sz = 0x%llx(%lld)\n", sx, sx, sy, sy, sz, sz);
					printf("sx_quo = 0x%llx, sy_quo = 0x%llx,sx_pos = 0x%0x, sy_pos = 0x%0x, sx_wgt = 0x%0x, sy_wgt = 0x%0x\n", sx_quo, sy_quo, sx_pos, sy_pos, sx_wgt, sy_wgt);
				}
			uint8_t src[4][4]; //[pixel][channel]
			for(uint8_t i = 0; i < 4; i++) {
				for(uint8_t j = 0; j < 4; j++) {
					src[i][j] = 0;
				}
			}
			for (uint8_t py = 0; py < 2; py++) {
				for (uint8_t px = 0; px < 2; px++) {
					uint8_t pi = py*2 + px;
					int16_t x = sx_pos + px;
					int16_t y = sy_pos + py;
					bool is_dummy = ((x < 0) || (x >= s->sw) ||
							(y < 0) || (y >= s->sh));
					uint8_t cn_y = ((s->bw == 0) ? 1 :
							(s->bw == 1) ? 2 :
							(s->bw == 2) ? 1 : //Y
							(s->bw == 3) ? 4 : 0); //channel number
					for (uint8_t ci = 0;  ci < cn_y; ci++) { //channel index
						uint8_t channel_dummy_y = (s->dummy_val >> (ci*8)) & 0xff;
						uint32_t si_y = y*s->src_stride + x*cn_y + ci;
						src[pi][ci] = is_dummy ? channel_dummy_y : s->src_ybase[si_y];
						//DEBUG
						/* if (s->debug_en)  */
							if (dx == aip_p_debug_dx && dy == aip_p_debug_dy && ci == aip_p_debug_dc) {
								printf("src_y[%0d][%0d] = 0x%0x -- (src_base)(0x%0x)\n",
										pi, ci, src[pi][ci], &s->src_ybase[si_y]);
							}
					}
					if (s->bw == 2) { //nv12
						for (uint8_t ci = 2;  ci < 4; ci++) { //channel index
							uint32_t si_c = (y/2)*s->src_stride + (x/2)*2 + (ci-2);
							uint8_t channel_dummy_c = (s->dummy_val >> ci*8) & 0xff;
							src[pi][ci] = is_dummy ? channel_dummy_c : s->src_cbase[si_c];
							if (dx == aip_p_debug_dx && dy == aip_p_debug_dy)
								printf("src_c[%0d][%0d] = 0x%0x\n", pi, ci, src[pi][ci]);
						}
					}
				}
			}
			if (s->bw == 2) {

				int i;
				if (dx == aip_p_debug_dx && dy == aip_p_debug_dy) {
					for(i=0;i<9;i++)
						printf("param[%d] = 0x%x\n",i,s->param[i]);
					printf("alpha = %d\n",s->alpha);
				}
				/* if (s->debug_en) aip_p_dump(stderr, "nv12, src:", (char *)src, 4, 2, 2, 2); */
				aip_t_nv2bgr_core(src[0][0], //y,
						src[0][2], //uint8_t u,
						src[0][3], //uint8_t v,
						s->param, //const uint32_t *coef,
						s->ofst, //const uint8_t *offset,
						s->alpha, //const uint8_t nv2bgr_alpha,
						s->order, //int order,
						src[0]);
				aip_t_nv2bgr_core(src[1][0], //y,
						src[1][2], //uint8_t u,
						src[1][3], //uint8_t v,
						s->param, //const uint32_t *coef,
						s->ofst, //const uint8_t *offset,
						s->alpha, //const uint8_t nv2bgr_alpha,
						s->order, //int order,
						src[1]);
				aip_t_nv2bgr_core(src[2][0], //y,
						src[2][2], //uint8_t u,
						src[2][3], //uint8_t v,
						s->param, //const uint32_t *coef,
						s->ofst, //const uint8_t *offset,
						s->alpha, //const uint8_t nv2bgr_alpha,
						s->order, //int order,
						src[2]);
				aip_t_nv2bgr_core(src[3][0], //y,
						src[3][2], //uint8_t u,
						src[3][3], //uint8_t v,
						s->param, //const uint32_t *coef,
						s->ofst, //const uint8_t *offset,
						s->alpha, //const uint8_t nv2bgr_alpha,
						s->order, //int order,
						src[3]);
			}
			/* if (s->debug_en) aip_p_dump(stderr, "src:", (char *)src, 4, 2, 2, 2); */
			uint8_t cn = ((s->bw == 0) ? 1 :
					(s->bw == 1) ? 2 :
					(s->bw == 2) ? 4 : //Y
					(s->bw == 3) ? 4 : 0); //channel number
			for (uint8_t ci = 0; ci < cn; ci++) {
				uint32_t di = dy*s->dst_stride + dx*cn + ci;
				s->dst_base[di] = aip_p_bilinear_interplotion(src[0][ci],
						src[1][ci],
						src[2][ci],
						src[3][ci],
						sx_wgt,
						sy_wgt);
				if (dx == aip_p_debug_dx && dy == aip_p_debug_dy)
					printf("(0x%0x, 0x%0x, 0x%0x, 0x%0x)(0x%0x, 0x%0x) -> 0x%0x  (%p) (di =%d dst_stride = %d)\n",
							src[0][ci], src[1][ci], src[2][ci], src[3][ci], sx_wgt, sy_wgt,
							s->dst_base[di], &s->dst_base[di], di, s->dst_stride);
			}
		}
	}

}

void aip_p_model_affine(const data_info_s *src,
		const int box_num, const data_info_s *dst,
		const box_affine_info_s *boxes,
		const uint32_t *coef, const uint32_t *offset)
{
	int i,k;
	uint8_t bw = 0;
	uint32_t order = 0;
	uint32_t src_base_y = 0, src_base_uv = 0;
	uint32_t dst_base = 0;
	uint32_t offset_y = 0, offset_uv = 0;
	float matrix[9];
	int64_t ax,bx,cx,ay,by,cy,az,bz,cz;
	int x, y, h, w;
	AIP_P_CFG_S *node = NULL;

	node = (AIP_P_CFG_S *)malloc(sizeof(AIP_P_CFG_S) * box_num);
	if(!node)
		return;
	memset(node, 0x00, sizeof(AIP_P_CFG_S) * box_num);

	for(i = 0; i < box_num; i++) {
		x = boxes[i].box.x;
		y = boxes[i].box.y;
		w = boxes[i].box.w;
		h = boxes[i].box.h;

		src_base_y 	= src->base;
		dst_base 	= dst[i].base;
		if(BS_DATA_BGRA <= src->format && src->format <= BS_DATA_ARGB) {
			offset_y 	= src->line_stride * y + x * 4;
			bw 		= 3;
		} else if(src->format == BS_DATA_NV12) {
			src_base_uv 	= src->base1;
			offset_y	= src->line_stride * y + x;
			offset_uv 	= src->line_stride * y / 2  + x;
			bw 		= 2;
		} else if(src->format == BS_DATA_UV) {
			offset_y 	= src->line_stride * y + x * 2;
			bw 		= 1;
		} else if(src->format == BS_DATA_Y) {
			offset_y 	= src->line_stride * y + x;
			bw 		= 0;
		}
		node[i].bw 		= bw;
		node[i].src_ybase 	= (uint8_t *)(src_base_y + offset_y);
		node[i].src_cbase 	= (uint8_t *)(src_base_uv + offset_uv);
		node[i].src_stride 	= src->line_stride;
		node[i].dst_base 	= (uint8_t *)dst_base;
		node[i].dst_stride 	= dst[i].line_stride;
		node[i].sw		= w;
		node[i].sh		= h;
		node[i].dw		= dst[i].width;
		node[i].dh		= dst[i].height;
		node[i].dummy_val	= boxes[i].zero_point;

		for(k = 0; k < 9; k++) {
			matrix[k] = boxes[i].matrix[k];
		}
		//transformation of matrix
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
		/* printf("mode:*******************\n"); */
		/* printf("ml:%lld %lld %lld\n",ax,bx,cx); */
		/* printf("ml:%lld %lld %lld\n",ay,by,cy); */
		/* printf("ml:%lld %lld %lld\n",az,bz,cz); */

		node[i].ax 		= ax;
		node[i].bx 		= bx;
		node[i].cx 		= cx;
		node[i].ay 		= ay;
		node[i].by 		= by;
		node[i].cy 		= cy;
		node[i].az 		= az;
		node[i].bz 		= bz;
		node[i].cz 		= cz;
		node[i].param[0] 	= coef[0];
		node[i].param[1] 	= coef[1];
		node[i].param[2] 	= coef[2];
		node[i].param[3] 	= coef[3];
		node[i].param[4] 	= -coef[4];
		node[i].param[5] 	= -coef[5];
		node[i].param[6] 	= coef[6];
		node[i].param[7] 	= coef[7];
		node[i].param[8] 	= coef[8];
		node[i].ofst[0] 	= offset[0];
		node[i].ofst[1] 	= offset[1];
		node[i].alpha 		= 0;

		switch (dst[i].format) {
		case BS_DATA_Y:    order = 0; break;
		case BS_DATA_UV:   order = 0; break;
		case BS_DATA_NV12: order = 0; break;
		case BS_DATA_BGRA: order = 0; break;
		case BS_DATA_GBRA: order = 1; break;
		case BS_DATA_RBGA: order = 2; break;
		case BS_DATA_BRGA: order = 3; break;
		case BS_DATA_GRBA: order = 4; break;
		case BS_DATA_RGBA: order = 5; break;
		case BS_DATA_ABGR: order = 8; break;
		case BS_DATA_AGBR: order = 9; break;
		case BS_DATA_ARBG: order = 10; break;
		case BS_DATA_ABRG: order = 11; break;
		case BS_DATA_AGRB: order = 12; break;
		case BS_DATA_ARGB: order = 13; break;
		default :
			printf("src->format type  error\n");
			break;
		}
		node[i].order 		= order;
	}
	for(i = 0; i < box_num; i++){
		aip_p_mdl(&node[i]);
	}

	free(node);

}

void aip_p_model_perspective(const data_info_s *src,
		const int box_num, const data_info_s *dst,
		const box_affine_info_s *boxes,
		const uint32_t *coef, const uint32_t *offset)
{
	int i,k;
	uint8_t bw = 0;
	uint32_t order = 0;
	uint32_t src_base_y = 0, src_base_uv = 0;
	uint32_t dst_base = 0;
	uint32_t offset_y = 0, offset_uv = 0;
	float matrix[9];
	int64_t ax,bx,cx,ay,by,cy,az,bz,cz;
	int x, y, h, w;
	AIP_P_CFG_S *node = NULL;

	node = (AIP_P_CFG_S *)malloc(sizeof(AIP_P_CFG_S) * box_num);
	if(!node)
		return;
	memset(node, 0x00, sizeof(AIP_P_CFG_S) * box_num);

	for(i = 0; i < box_num; i++) {
		x = boxes[i].box.x;
		y = boxes[i].box.y;
		w = boxes[i].box.w;
		h = boxes[i].box.h;

		src_base_y 	= src->base;
		dst_base 	= dst[i].base;
		if(BS_DATA_BGRA <= src->format && src->format <= BS_DATA_ARGB) {
			offset_y 	= src->line_stride * y + x * 4;
			bw 		= 3;
		} else if(src->format == BS_DATA_NV12) {
			src_base_uv 	= src->base1;
			offset_y	= src->line_stride * y + x;
			offset_uv 	= src->line_stride * y / 2  + x;
			bw 		= 2;
		} else if(src->format == BS_DATA_UV) {
			offset_y 	= src->line_stride * y + x * 2;
			bw 		= 1;
		} else if(src->format == BS_DATA_Y) {
			offset_y 	= src->line_stride * y + x;
			bw 		= 0;
		}
		node[i].bw 		= bw;
		node[i].src_ybase 	= (uint8_t *)(src_base_y + offset_y);
		node[i].src_cbase 	= (uint8_t *)(src_base_uv + offset_uv);
		node[i].src_stride 	= src->line_stride;
		node[i].dst_base 	= (uint8_t *)dst_base;
		node[i].dst_stride 	= dst[i].line_stride;
		node[i].sw		= w;
		node[i].sh		= h;
		node[i].dw		= dst[i].width;
		node[i].dh		= dst[i].height;
		node[i].dummy_val	= boxes[i].zero_point;

		for(k = 0; k < 9; k++) {
			matrix[k] = boxes[i].matrix[k];
		}
		//transformation of matrix1
		double t0 = matrix[0]*(matrix[4]*matrix[8]-matrix[5]*matrix[7]);
		double t1 = matrix[1]*(matrix[3]*matrix[8]-matrix[5]*matrix[6]);
		double t2 = matrix[2]*(matrix[3]*matrix[7]-matrix[4]*matrix[6]);
		double t3 = t0-t1+t2;
		double dst_m[9] = {0};
		if(t3 != 0.0)
		{
			float def = 1.0/t3;
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
		ax = (int64_t)round(dst_m[0] * 65536 * 1024);
		bx = (int64_t)round(dst_m[1] * 65536 * 1024);
		cx = (int64_t)round(dst_m[2] * 65536 * 1024);
		ay = (int64_t)round(dst_m[3] * 65536 * 1024);
		by = (int64_t)round(dst_m[4] * 65536 * 1024);
		cy = (int64_t)round(dst_m[5] * 65536 * 1024);
		az = (int64_t)round(dst_m[6] * 65536 * 1024);
		bz = (int64_t)round(dst_m[7] * 65536 * 1024);
		cz = (int64_t)round(dst_m[8] * 65536 * 1024);

		node[i].ax 		= ax;
		node[i].bx 		= bx;
		node[i].cx 		= cx;
		node[i].ay 		= ay;
		node[i].by 		= by;
		node[i].cy 		= cy;
		node[i].az 		= az;
		node[i].bz 		= bz;
		node[i].cz 		= cz;
		node[i].param[0] 	= coef[0];
		node[i].param[1] 	= coef[1];
		node[i].param[2] 	= coef[2];
		node[i].param[3] 	= coef[3];
		node[i].param[4] 	= -coef[4];
		node[i].param[5] 	= -coef[5];
		node[i].param[6] 	= coef[6];
		node[i].param[7] 	= coef[7];
		node[i].param[8] 	= coef[8];
		node[i].ofst[0] 	= offset[0];
		node[i].ofst[1] 	= offset[1];
		node[i].alpha 		= 0;

		switch (dst[i].format) {
		case BS_DATA_Y:    order = 0; break;
		case BS_DATA_UV:   order = 0; break;
		case BS_DATA_NV12: order = 0; break;
		case BS_DATA_BGRA: order = 0; break;
		case BS_DATA_GBRA: order = 1; break;
		case BS_DATA_RBGA: order = 2; break;
		case BS_DATA_BRGA: order = 3; break;
		case BS_DATA_GRBA: order = 4; break;
		case BS_DATA_RGBA: order = 5; break;
		case BS_DATA_ABGR: order = 8; break;
		case BS_DATA_AGBR: order = 9; break;
		case BS_DATA_ARBG: order = 10; break;
		case BS_DATA_ABRG: order = 11; break;
		case BS_DATA_AGRB: order = 12; break;
		case BS_DATA_ARGB: order = 13; break;
		default :
			printf("src->format type  error\n");
			break;
		}
		node[i].order 		= order;
	}
	for(i = 0; i < box_num; i++){
		aip_p_mdl(&node[i]);
	}
	free(node);

}
