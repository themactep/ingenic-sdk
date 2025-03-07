/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : aip_f_mdl.c
 * Authors    : cbzhang@abel.ic.jz.com
 * Create Time: 2020-11-19:18:05:30
 * Description:
 *
 */

#include "aip_f_mdl.h"

int aip_f_debug_dx = -1;
int aip_f_debug_dy = 0;
int aip_f_debug_dc = 0;

#define DBG_Y    -1
#define DBG_X    0
#define DBG_BPP  0
#define DEBUG    1  //1 , 0

void aip_f_debug_point(int dx, int dy, int dc)
{
    aip_f_debug_dx = dx;
    aip_f_debug_dy = dy;
    aip_f_debug_dc = dc;
}
void aip_f_matrix_dump(uint8_t *p, uint8_t cn, uint32_t pw, uint32_t ph, uint32_t ps) {

    for (int py = 0; py < ph; py++) {
        printf("[0x%0x]:", py);
        for (int px = 0; px < pw; px++) {
            printf("[0x%0x]{", px);
            for (int ci = 0; ci < cn; ci++) {
		    printf("0x%0x,", p[py*ps + px*cn + ci]);
	    }
	    printf("}, ");
	}
	printf("\n");
    }
}

void aip_f_nv12_Y(aip_f_cfg_s *cfg,
		int dst_x, int dst_y,
		int src_x0, int src_x1, int src_y0, int src_y1,
		int w_x0, int w_x1, int w_y0, int w_y1)
{
	int d0_idx = 0;
	int d1_idx = 0;
	int d2_idx = 0;
	int d3_idx = 0;
	uint8_t d0 = 0;
	uint8_t d1 = 0;
	uint8_t d2 = 0;
	uint8_t d3 = 0;
	d0_idx = src_y0 * cfg->src_stride + src_x0 ;
	d1_idx = src_y0 * cfg->src_stride + src_x1 ;
	d2_idx = src_y1 * cfg->src_stride + src_x0 ;
	d3_idx = src_y1 * cfg->src_stride + src_x1 ;
	d0 = cfg->src_base[d0_idx];
	d1 = cfg->src_base[d1_idx];
	d2 = cfg->src_base[d2_idx];
	d3 = cfg->src_base[d3_idx];

	uint32_t v0 = (d0 * w_x0 + d1 * w_x1) >> 4;
	uint32_t v1 = (d2 * w_x0 + d3 * w_x1) >> 4;
	uint32_t res = (((v0 * w_y0) >> 16) +
			((v1 * w_y1) >> 16) + 2) >> 2;

	int dst_idx = dst_y * cfg->dst_stride + dst_x;
	cfg->dst_base[dst_idx] = (uint8_t)(res & 0xFF);

	//debug
	/* if(dst_y == DBG_Y && dst_x == DBG_X && DEBUG) { */
	/*     printf("Y: {\ndst_y: 0x%0x, dst_x: 0x%0x, src_x0: 0x%0x , src_y0: 0x%0x , w_x1: 0x%0x , w_y1: 0x%0x }\n",dst_y, dst_x, src_x0, src_y0, w_x1, w_y1); */
	/* } */
}

void aip_f_mdl(aip_f_cfg_s *cfg)
{
	int32_t src_fx = cfg->trans_x; // src float x
	int32_t src_fy = cfg->trans_y; // src float y
	int bpp = ((cfg->bitmode == AIP_F_Y) ? 1 :
			(cfg->bitmode == AIP_F_old_C) ? 2 :
			(cfg->bitmode == AIP_F_BGRA) ? 4 :
			(cfg->bitmode == AIP_F_FMU2_CHN32) ? 8 :
			(cfg->bitmode == AIP_F_FMU4_CHN32) ? 16 :
			(cfg->bitmode == AIP_F_FMU8_CHN16) ? 16 :
			(cfg->bitmode == AIP_F_FMU8_CHN32) ? 32 :
			(cfg->bitmode == AIP_F_new_C) ? 2 : 4);

	int dst_x, dst_y;
	for (dst_y = 0; dst_y < cfg->dst_h; dst_y++) {
		src_fx = cfg->trans_x;
		for (dst_x = 0; dst_x < cfg->dst_w; dst_x++) {
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

			if (src_x >= cfg->src_w - 1) {
				src_x0 = cfg->src_w - 1;
				src_x1 = cfg->src_w - 1;
			}
			if (src_y >= cfg->src_h - 1) {
				src_y0 = cfg->src_h - 1;
				src_y1 = cfg->src_h - 1;
			}

			if(cfg->bitmode == AIP_F_new_C) {//NV12_Y
				aip_f_nv12_Y(cfg,
						dst_x, dst_y,
						src_x0, src_x1, src_y0, src_y1,
						w_x0, w_x1,w_y0, w_y1);
			}
			for (int k = 0; k < bpp; k++) {
				int d0_idx = 0;
				int d1_idx = 0;
				int d2_idx = 0;
				int d3_idx = 0;
				uint8_t d0 = 0;
				uint8_t d1 = 0;
				uint8_t d2 = 0;
				uint8_t d3 = 0;
				if (cfg->bitmode == AIP_F_new_C) {//NV12_C
					d0_idx = (src_y0 / 2) * cfg->src_stride + (src_x0 & 0xfffe) + k;
					d1_idx = (src_y0 / 2) * cfg->src_stride + (src_x1 & 0xfffe) + k;
					d2_idx = (src_y1 / 2) * cfg->src_stride + (src_x0 & 0xfffe) + k;
					d3_idx = (src_y1 / 2) * cfg->src_stride + (src_x1 & 0xfffe) + k;
					d0 = cfg->src_cbase[d0_idx];
					d1 = cfg->src_cbase[d1_idx];
					d2 = cfg->src_cbase[d2_idx];
					d3 = cfg->src_cbase[d3_idx];

					if (dst_x==DBG_X && dst_y==DBG_Y) {
					 printf("src_y:%d src_x:%d w_x1:%d w_x0:%d \n", src_y, src_x, w_x1, w_x0);
					    printf("idx| %d %d %d %d\n", d0_idx, d1_idx, d2_idx, d3_idx);
					    printf("data | %d %d %d %d\n", d0, d1, d2, d3);
					}
				}
				else {
					d0_idx = src_y0 * cfg->src_stride + src_x0 * bpp + k;
					d1_idx = src_y0 * cfg->src_stride + src_x1 * bpp + k;
					d2_idx = src_y1 * cfg->src_stride + src_x0 * bpp + k;
					d3_idx = src_y1 * cfg->src_stride + src_x1 * bpp + k;
					d0 = cfg->src_base[d0_idx];
					d1 = cfg->src_base[d1_idx];
					d2 = cfg->src_base[d2_idx];
					d3 = cfg->src_base[d3_idx];
				}
				if(dst_y ==DBG_Y && dst_x == DBG_X && DEBUG && k == DBG_BPP) {
					printf("src_y0:%0d, src_y1:%0d, src_x0: %0d, src_x1: %0d,  bpp: %0d\n", src_y0, src_y1, src_x0, src_x1, bpp);
					printf("d0:0x%0x, (addr: 0x%0x), d0_idx: %0d;\n", d0, &cfg->src_base[d0_idx], d0_idx);
					printf("d1:0x%0x, (addr: 0x%0x), d1_idx: %0d;\n", d1, &cfg->src_base[d1_idx], d1_idx);
					printf("d2:0x%0x, (addr: 0x%0x), d2_idx: %0d;\n", d2, &cfg->src_base[d2_idx], d2_idx);
					printf("d3:0x%0x, (addr: 0x%0x), d3_idx: %0d;\n", d3, &cfg->src_base[d3_idx], d3_idx);
				}
				if ((cfg->bitmode == AIP_F_FMU8_CHN32) ||
						(cfg->bitmode == AIP_F_BGRA) ||
						(cfg->bitmode == AIP_F_Y) ||
						(cfg->bitmode == AIP_F_old_C) ||
						(cfg->bitmode == AIP_F_new_C) ||
						(cfg->bitmode == AIP_F_FMU8_CHN16)) { //8bits
					uint32_t v0 = (d0 * w_x0 + d1 * w_x1) >> 4;
					uint32_t v1 = (d2 * w_x0 + d3 * w_x1) >> 4;
					uint32_t res = (((v0 * w_y0) >> 16) +
							((v1 * w_y1) >> 16) + 2) >> 2;
					if (cfg->bitmode == AIP_F_new_C) {//NV12_C
						if ((dst_x%2 == 0) && (dst_y%2 == 0)) {
							int dst_idx = (dst_y/2) * cfg->dst_stride + (dst_x & 0xfffe) + k;
							cfg->dst_cbase[dst_idx] = (uint8_t)(res & 0xFF);
							/* if(dst_y ==DBG_Y && dst_x == DBG_X && DEBUG && k == DBG_BPP) { */
							/*      printf("C:  {\n dst_idx: %d, value |  0x%0x \n",dst_idx, cfg->dst_cbase[dst_idx]);  */
							/*  } */

						}
					} else {
						int dst_idx = dst_y * cfg->dst_stride + dst_x * bpp + k;
						cfg->dst_base[dst_idx] = (uint8_t)(res & 0xFF);
						//debug
						/* if(dst_y ==DBG_Y && dst_x == DBG_X && DEBUG && k == DBG_BPP) { */
						    /* printf("debug:value: %d, (addr: %p) \n", cfg->dst_base[dst_idx], &cfg->dst_base[dst_idx]); */
						/* } */

					}
					//debug
					/* if(dst_y == DBG_Y && dst_x == DBG_X && DEBUG && k == DBG_BPP) { */
						/* printf("dst_y: %d, dst_x: %d, k: %d, src_x: %d , src_y: %d , w_x1: %d , w_y1: %d\n", */
								       /* dst_y, dst_x, k, src_x, src_y, w_x1, w_y1); */
					/* } */

				} else if (cfg->bitmode == AIP_F_FMU2_CHN32) {
					uint8_t res = 0;
					for (int b = 0; b < 4; b++) {
						uint8_t d0_2b = (d0 >> (b * 2)) & 0x3;
						uint8_t d1_2b = (d1 >> (b * 2)) & 0x3;
						uint8_t d2_2b = (d2 >> (b * 2)) & 0x3;
						uint8_t d3_2b = (d3 >> (b * 2)) & 0x3;
						uint32_t v0 = (d0_2b * w_x0 + d1_2b * w_x1) >> 4;
						uint32_t v1 = (d2_2b * w_x0 + d3_2b * w_x1) >> 4;
						uint32_t res_2b = (((v0 * w_y0) >> 16) +
								((v1 * w_y1) >> 16) + 2) >> 2;
						res |= res_2b << (b * 2);
						/* if (dst_x == aip_f_debug_dx && */
						/*     dst_y == aip_f_debug_dy && */
						/*     k  == aip_f_debug_dc) { */
						/*     printf("dst(%d,%d,%d) -- src(%d,%d) -- ", */
						/*            dst_x, dst_y, k, src_x, src_y); */
						/*     printf("w:%d,%d,%d,%d -- da:%d,%d,%d,%d -- res_2b:%d, res:0x%0x -- da(w):%d,%d,%d,%d\n", */
						/*            w_x0, w_x1, w_y0, w_y1, d0_2b, d1_2b, d2_2b, d3_2b, */
						/*            res_2b, res, d0, d1, d2, d3); */
						/* } */

						if (res_2b > 3) {
							printf("overflow!\n");
							assert(0);
						}
					}
					int dst_idx = dst_y * cfg->dst_stride + dst_x * bpp + k;
					cfg->dst_base[dst_idx] = (uint8_t)(res & 0xFF);
					/* if (dst_x == aip_f_debug_dx && dst_y == aip_f_debug_dy && k  == aip_f_debug_dc) { */
					/*     printf("--value: 0x%0x,  addr:%p\n", cfg->dst_base[dst_idx], &cfg->dst_base[dst_idx]); */
					/* } */
				} else if (cfg->bitmode == AIP_F_FMU4_CHN32) {
					uint8_t res = 0;
					for (int b = 0; b < 2; b++) {
						uint8_t d0_4b = (d0 >> (b * 4)) & 0xF;
						uint8_t d1_4b = (d1 >> (b * 4)) & 0xF;
						uint8_t d2_4b = (d2 >> (b * 4)) & 0xF;
						uint8_t d3_4b = (d3 >> (b * 4)) & 0xF;
						uint32_t v0 = (d0_4b * w_x0 + d1_4b * w_x1) >> 4;
						uint32_t v1 = (d2_4b * w_x0 + d3_4b * w_x1) >> 4;
						uint32_t res_4b = (((v0 * w_y0) >> 16) +
								((v1 * w_y1) >> 16) + 2) >> 2;
						res |= res_4b << (b * 4);
						/* if (dst_x == aip_f_debug_dx && */
						/*     dst_y == aip_f_debug_dy && */
						/*     (k * 2 + b) == aip_f_debug_dc) { */
						/*     printf("dst(%d,%d,%d) -- src(%d,%d) -- ", */
						/*            dst_x, dst_y, k, src_x, src_y); */
						/*     printf("w:%d,%d,%d,%d -- da:%d,%d,%d,%d -- dst:%d\n", */
						/*            w_x0, w_x1, w_y0, w_y1, d0_4b, d1_4b, d2_4b, d3_4b, */
						/*            res_4b); */
						/* } */
						if (res_4b > 15) {
							printf("overflow!\n");
							assert(0);
						}
					}
					int dst_idx = dst_y * cfg->dst_stride + dst_x * bpp + k;
					cfg->dst_base[dst_idx] = (uint8_t)(res & 0xFF);
				}
			}
			src_fx += cfg->scale_x;

		}
		src_fy += cfg->scale_y;
	}
}

void aip_f_model(const data_info_s *src,
                    const int box_num, const data_info_s *dst,
                    const box_resize_info_s *boxes)
{
	int i;
	uint8_t bw;
	int x, y, h, w;
	float scale_x_f, scale_y_f;
	uint32_t src_base_y = 0, src_base_uv = 0;
	uint32_t dst_base_y = 0, dst_base_uv = 0;
	uint32_t offset_y = 0, offset_uv = 0;
	aip_f_cfg_s *node = (aip_f_cfg_s *)malloc(sizeof(aip_f_cfg_s)*box_num);

	for(i = 0; i < box_num; i++){
		if(src->format == BS_DATA_Y) {
			bw = 0;
		} else if(src->format == BS_DATA_UV) {
			bw = 1;
		} else if(src->format <= BS_DATA_ARGB && src->format >= BS_DATA_BGRA){
			bw = 2;
		} else if(src->format == BS_DATA_FMU2) {
			bw = 3;
		} else if(src->format == BS_DATA_FMU4) {
			bw = 4;
		} else if(src->format == BS_DATA_FMU8) {
			bw = 5;
		} else if(src->format == BS_DATA_FMU8_H) {
			bw = 6;
		} else if(src->format == BS_DATA_NV12) {
			bw = 7;
		}
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

		node[i].scale_x 	= (int32_t)(scale_x_f * 65536);
		node[i].scale_y 	= (int32_t)(scale_y_f * 65536);
		node[i].trans_x 	= (int32_t)((scale_x_f * 0.5 - 0.5) * 65536);
		node[i].trans_y 	= (int32_t)((scale_y_f * 0.5 - 0.5) * 65536);
		node[i].src_base 	= (uint8_t *)(src_base_y + offset_y);
		node[i].src_cbase 	= (uint8_t *)(src_base_uv + offset_uv);
		node[i].dst_base 	= (uint8_t *)dst_base_y;
		node[i].dst_cbase 	= (uint8_t *)dst_base_uv;
		node[i].src_w 		= w;//src->width;
		node[i].src_h 		= h;//src->height;
		node[i].dst_w 		= dst[i].width;
		node[i].dst_h		= dst[i].height;
		node[i].src_stride 	= src->line_stride;
		node[i].dst_stride 	= dst[i].line_stride;
		node[i].bitmode		= bw;
	}

	for(i = 0; i < box_num; i++){
		aip_f_mdl(&node[i]);
	}

	free(node);
}
