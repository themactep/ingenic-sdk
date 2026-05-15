#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

#include "bscaler_api.h"
#include "aie_mmap.h"
#include "bs_src.h"
#include "aip_p_mdl.h"
#include "aip_f_mdl.h"
#include "Matrix.h"

// #define MAX_SIZE

/* #define USER_ORAM_I */
/* #define USER_ORAM_O */

#define WIDTH_MAX	512
#define HEIGHT_MAX	512

#define CHAIN_NUM 	5
#define TASK_LEN	128

int aip_resize_test(bs_data_format_e format, uint32_t box_num);
int aip_covert_test(uint32_t task_len);
int aip_affine_test(bs_data_format_e format, uint32_t box_num);
int aip_perspective_test(bs_data_format_e format, uint32_t box_num);
int aip_affine_test_true(bs_data_format_e format, uint32_t box_num, char *filename);
int aip_affine_test1(bs_data_format_e format, uint32_t box_num);

unsigned int nv2bgr_ofst[2] = {16, 128};
unsigned int nv2bgr_coef[9] = {
	1220542, 2116026, 0,
	1220542, 409993, 852492,
	1220542, 0, 1673527
};
int box[][4] = {
	{0, 0, 256 , 256},
	{0, 100, 128, 128},
	{100, 0, 128, 128},
	{100, 100, 128, 128},
	{0, 0, 128, 128},
};
float matrix_g[][9] = {
	{
		1, 0, 0,
		0, 1, 0,
		0, 0, 1
	},
	{
		2, 0, 0,
		0, 2, 0,
		0, 0, 1
	},
	{
		1, 0, 0,
		0.5, 1, 0,
		0, 0, 1
	},

	{
		0.5, 0, 0,
		0, 0.5, 0,
		0, 0, 1
	},
};

char const *menu[] ={
	"***************** MENU ******************\n",
	"0. exit\n",
	"1. AIP Test Resize\n",
	"2. AIP Test Covert\n",
	"3. AIP Test Affine\n",
	"4. AIP Test Perspective\n",
	"5. P/T/F Test\n",
	"6. P True Attr Test\n",
	"7. P test 1\n",
};


void *start_resize_thread(void *arg)
{
	int chain_num = *(int *)arg;

	printf("Test AIP Resize\n");
	aip_resize_test(BS_DATA_BGRA, chain_num);
	pthread_exit(NULL);
}
void *start_covert_thread(void *arg)
{
	int chain_num = *(int *)arg;

	printf("Test AIP Covert\n");
	aip_covert_test(128);
	pthread_exit(NULL);
}
void *start_affine_thread(void *arg)
{
	int chain_num = *(int *)arg;

	printf("Test AIP Affine\n");
	aip_affine_test(BS_DATA_BGRA, chain_num);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int i;
	char c, c1;
	int chain_num = CHAIN_NUM;

	nna_cache_attr_t desram_cache_attr = NNA_UNCACHED_ACCELERATED;
	nna_cache_attr_t oram_cache_attr = NNA_UNCACHED_ACCELERATED;
	nna_cache_attr_t ddr_cache_attr = NNA_CACHED;

	if ((ret = __aie_mmap(0x4000000, 1, desram_cache_attr, oram_cache_attr, ddr_cache_attr)) == 0) { //16MB
		printf ("nna box_base virtual generate failed !!\n");
		return 0;
	}
	for(i = 0; i < sizeof(menu)/sizeof(menu[0]); i++){
		printf("%s\n",menu[i]);
	}
	printf("Please Enter:\n");

	while(1) {
		scanf("%c", &c);
		printf("\n\n\n");
		switch (c) {
		case '0':
			return 0;
		case '1':
			printf("Test AIP Resize\n");
			printf("0. exit\n");
			printf("1. BGRA\n");
			printf("2. NV12\n");
			printf("3. Y\n");
			printf("4. UV\n");
			printf("5  FMU2\n");
			printf("6. FMU4\n");
			printf("7. FMU8\n");
			printf("8. FMU8_H\n");
			printf("Please Enter:\n");

			getchar();
			scanf("%c", &c1);
			printf("\n\n\n");
			switch (c1) {
			case '0':
				return 0;
			case '1':
				aip_resize_test(BS_DATA_BGRA, chain_num);
				break;
			case '2':
				aip_resize_test(BS_DATA_NV12, chain_num);
				break;
			case '3':
				aip_resize_test(BS_DATA_Y, chain_num);
				break;
			case '4':
				aip_resize_test(BS_DATA_UV, chain_num);
				break;
			case '5':
				aip_resize_test(BS_DATA_FMU2, chain_num);
				break;
			case '6':
				aip_resize_test(BS_DATA_FMU4, chain_num);
				break;
			case '7':
				aip_resize_test(BS_DATA_FMU8, chain_num);
				break;
			case '8':
				aip_resize_test(BS_DATA_FMU8_H, chain_num);
				break;
			default:
				printf("Invalid Argument\n ");
				break;
			}
			break;
		case '2':
			printf("Test AIP Covert\n");
			aip_covert_test(TASK_LEN);
			break;
		case '3':
			printf("Test AIP Affine\n");
			printf("0. exit\n");
			printf("1. BGRA\n");
			printf("2. NV12\n");
			printf("3. Y\n");
			printf("4. UV\n");
			printf("Please Enter:\n");

			getchar();
			scanf("%c", &c1);
			printf("\n\n\n");
			switch (c1) {
			case '0':
				return 0;
			case '1':
				aip_affine_test(BS_DATA_BGRA, chain_num);
				break;
			case '2':
				aip_affine_test(BS_DATA_NV12, chain_num);
				break;
			case '3':
				aip_affine_test(BS_DATA_Y, chain_num);
				break;
			case '4':
				aip_affine_test(BS_DATA_UV, chain_num);
				break;
			default:
				printf("Invalid Argument\n ");
				break;
			}
			break;
		case '4':
			printf("Test AIP Affine\n");
			printf("0. exit\n");
			printf("1. BGRA\n");
			printf("2. NV12\n");
			printf("3. Y\n");
			printf("4. UV\n");
			printf("Please Enter:\n");

			getchar();
			scanf("%c", &c1);
			printf("\n\n\n");
			switch (c1) {
			case '0':
				return 0;
			case '1':
				aip_perspective_test(BS_DATA_BGRA, chain_num);
				break;
			case '2':
				aip_perspective_test(BS_DATA_NV12, chain_num);
				break;
			case '3':
				aip_perspective_test(BS_DATA_Y, chain_num);
				break;
			case '4':
				aip_perspective_test(BS_DATA_UV, chain_num);
				break;
			default:
				printf("Invalid Argument\n ");
				break;
			}
			break;
		case '5':
			pthread_t resize_tid;
			pthread_t covert_tid;
			pthread_t affine_tid;

			pthread_create(&resize_tid, NULL, start_resize_thread, (void *)&chain_num);
			pthread_create(&covert_tid, NULL, start_covert_thread, (void *)&chain_num);
			pthread_create(&affine_tid, NULL, start_affine_thread, (void *)&chain_num);
			pthread_join(resize_tid, NULL);
			pthread_join(covert_tid, NULL);
			pthread_join(affine_tid, NULL);
		case '6':
			if(argc < 2) {
				printf("eg. %s  filename\n",argv[0]);
				exit(0);
			}
			aip_affine_test_true(BS_DATA_BGRA, chain_num, argv[1]);
			break;
		case '7':
			aip_affine_test1(BS_DATA_BGRA, chain_num);
			break;
		default :
			printf("Invalid Argument\n ");
			break;
		}
		for(i = 0; i < sizeof(menu)/sizeof(menu[0]); i++){
			printf("%s\n",menu[i]);
		}
		printf("\n Please continue input:");
		getchar();
	};

	__aie_munmap();

	return 0;
}

int aip_affine_test(bs_data_format_e format, uint32_t box_num)
{
	int ret = 0;
	FILE *fp;
	int i, j;
	char name[32] = {0};
	uint32_t num_t = 0;

	uint8_t chn, bw;
	uint32_t src_w, src_h;
	uint32_t dst_w, dst_h;
	uint32_t dst_size, src_size;
	uint32_t src_bpp, dst_bpp;
	uint32_t src_stride, dst_stride;
	uint8_t *src_base, *src_base1;
	uint8_t *dst_base, *dst_base1;
	uint8_t *opencv_base, *opencv_base1;
	float matrix[9] = {0};
	float angel = 0;

#ifdef MAX_SIZE
	src_size = 2048*2048*4;
	dst_size = 2048*2048*4;
#else
	src_size = 1024*2048*4;
	dst_size = 1024*2048*4;
#endif

#ifndef USER_ORAM_I
	src_base = (uint8_t *)ddr_memalign(32, src_size);
#else
	src_base = (uint8_t *)oram_memalign(32, src_size);
#endif
#ifndef USER_ORAM_O
	dst_base = (uint8_t *)ddr_memalign(32, dst_size * box_num);
#else
	dst_base = (uint8_t *)oram_memalign(32, dst_size * box_num);
#endif
	opencv_base = (uint8_t *)ddr_memalign(32, dst_size * box_num);

	while(1) {
		memset(src_base, 0x00, src_size);
		memset(dst_base, 0x00, dst_size * box_num);
		memset(opencv_base, 0x00, dst_size * box_num);
#ifdef MAX_SIZE
		src_w = 2048;
		src_h = 2048;
		dst_w = 2048;
		dst_h = 2048;
#else
		src_w = 1024;
		src_h = 1024;
		dst_w = 1024;
		dst_h = 1024;
		/*
		src_w = (rand()%WIDTH_MAX + 2)&0xfffe;
		src_h = (rand()%HEIGHT_MAX + 2)&0xfffe;
		dst_w = (rand()%WIDTH_MAX + 2)&0xfffe;
		dst_h = (rand()%HEIGHT_MAX + 2)&0xfffe;
		*/
#endif
		if(format == BS_DATA_Y) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 1;
			bw = 0;
		} else if(format == BS_DATA_UV) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 2;
			bw = 1;
		} else if(format == BS_DATA_NV12) {
			src_bpp = 12;
			dst_bpp = 8;
			chn = 4;
			bw = 2;
		} else if(format <= BS_DATA_ARGB && format >= BS_DATA_BGRA) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 4;
			bw = 3;
		}

		if(format == BS_DATA_NV12) {
			src_stride =  src_w;
			dst_stride =  dst_w * chn;
			src_size = src_stride * src_h * 3 / 2;
			dst_size = dst_stride * dst_h;
		} else {
			src_stride =  src_w * chn;
			dst_stride =  dst_w * chn;
			src_size = src_stride * src_h;
			dst_size = dst_stride * dst_h;
		}

		fp = fopen("./1111_resize.bmp.nv12","r");
		//fp = fopen("./1111_resize.bmp.data","r");
		if(fp == NULL) {
			perror("Affine fopen picture Error");
			exit(-1);
		}
		fread(src_base, 32, src_size/32, fp);
		fclose(fp);
		__aie_flushcache_dir((void *)src_base, src_size, NNA_DMA_TO_DEVICE);

		//get_matrixs(&matrix[0], angel, src_w, src_h, dst_w, dst_h, 0);
		box_affine_info_s *boxes = (box_affine_info_s *)malloc(sizeof(box_affine_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
			boxes[i].box.x = 32;	//box[i][0];
			boxes[i].box.y = 32;	//box[i][1];
			boxes[i].box.w = 720;	//box[i][2];
			boxes[i].box.h = 720;	//box[i][3];
			boxes[i].zero_point = 0x0;
			angel = rand()%180;
			memcpy(boxes[i].matrix, &matrix_g[0], sizeof(matrix));
		}

		data_info_s *src = (data_info_s *)malloc(sizeof(data_info_s));
#ifndef USER_ORAM_I
		src->base = __aie_get_ddr_paddr((uint32_t)src_base);
#else
		src->base = __aie_get_oram_paddr((uint32_t)src_base);
#endif
		src->base1 = src->base + src_w * src_h;
		src->format = format;
		src->width = src_w;
		src->height = src_h;
		src->line_stride = src_stride;
#ifndef USER_ORAM_I
		src->locate = BS_DATA_RMEM;
#else
		src->locate = BS_DATA_ORAM;
#endif

		data_info_s *dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
#ifndef USER_ORAM_O
			dst[i].base = __aie_get_ddr_paddr((uint32_t)dst_base) + i* dst_size;
#else
			dst[i].base = __aie_get_oram_paddr((uint32_t)dst_base) + i* dst_size;
#endif
			dst[i].base1 = dst[i].base + dst_w * dst_h;
			dst[i].format = format;
			dst[i].width = dst_w;
			dst[i].height = dst_h;
			dst[i].line_stride = dst_stride;
#ifndef USER_ORAM_O
			dst[i].locate = BS_DATA_RMEM;
#else
			dst[i].locate = BS_DATA_ORAM;
#endif
		}

		__aie_flushcache_dir((void *)dst_base, dst_size * box_num, NNA_DMA_FROM_DEVICE);
		bs_affine_start(src, box_num, dst, boxes, &nv2bgr_coef[0], &nv2bgr_ofst[0]);
		bs_affine_wait();
		data_info_s *model_src = (data_info_s *)malloc(sizeof(data_info_s));
		data_info_s *model_dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		model_src->base = (uint32_t)src_base;
		model_src->base1 = model_src->base + src_w * src_h;
		model_src->format = format;
		model_src->width = src_w;
		model_src->height = src_h;
		model_src->line_stride = src_stride;

		for(i = 0; i < box_num; i++) {
			model_dst[i].base = (uint32_t)opencv_base + i * dst_size;
			model_dst[i].base1 = model_dst[i].base + dst_w * dst_h;
			model_dst[i].format = format;
			model_dst[i].width = dst_w;
			model_dst[i].height = dst_h;
			model_dst[i].line_stride = dst_stride;
		}
		aip_p_model_affine(model_src, box_num, model_dst, boxes, &nv2bgr_coef[0], &nv2bgr_ofst[0]);

		for(i = 0; i < box_num; i++) {
			printf("Affine: src_w:%3d src_h:%3d -> dst_w:%3d dst_h:%3d ---%d\n",src_w,src_h,dst_w,dst_h,i);
			compare_bgra(dst_base + i * dst_size, opencv_base + i * dst_size, dst_h, dst_w, chn);
		}
		printf("%08d------------^_^-----------p\n",num_t++);
#if 1
		for(i = 0; i < box_num; i++) {
			sprintf(name, "%s_%d", "/tmp/test_affine", i);
			fp = fopen(name, "w+");
			fwrite(dst_base + (i * dst_size), 32, dst_size/32, fp);
			fclose(fp);
		}
#endif

		free(model_dst);
		free(model_src);
		free(dst);
		free(src);
		free(boxes);
	}
	ddr_free(opencv_base);
#ifndef USER_ORAM_O
	ddr_free(dst_base);
#else
	oram_free(dst_base);
#endif
#ifndef USER_ORAM_I
	ddr_free(src_base);
#else
	oram_free(src_base);
#endif

	return 0;
}

int aip_affine_test_true(bs_data_format_e format, uint32_t box_num, char *filename)
{
	int ret = 0;
	FILE *fp = NULL;
	int i, j;
	char name[32] = {0};
	uint32_t num_t = 0;

	uint8_t chn, bw;
	uint32_t src_w, src_h;
	uint32_t dst_w, dst_h;
	uint32_t dst_size, src_size;
	uint32_t src_bpp, dst_bpp;
	uint32_t src_stride, dst_stride;
	uint8_t *src_base, *src_base1;
	uint8_t *dst_base, *dst_base1;
	uint8_t *opencv_base, *opencv_base1;
	float matrix[9] = {0};
	uint32_t file_size;
	uint8_t *param_buffer;
	float *ptr;

	struct stat statbuf;
	stat(filename, &statbuf);
	file_size = statbuf.st_size;

	param_buffer = (uint8_t *)malloc(file_size);
	if(param_buffer == NULL) {
		perror("malloc error");
		return -1;
	}

	fp = fopen(filename, "rb");
	if (fp == NULL) {
		fprintf(stderr, "[Error][%s]: Open %s failed!\n", __func__, filename);
		exit(1);
	}
	uint32_t ret_size = fread(param_buffer, 1, file_size, fp);
	if (ret_size != file_size) {
		fprintf(stderr, "[Error][%s]: fread failed!\n", __func__);
		exit(1);
	}
	fclose(fp);

	for(j = 100; j < (file_size / 44); j++)
	{
		ptr = (float *)(param_buffer + j * 44);
		src_w = (uint32_t)ptr[0];
		src_h = (uint32_t)ptr[1];
		dst_w = src_w;
		dst_h = src_h;

		if(format == BS_DATA_Y) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 1;
			bw = 0;
		} else if(format == BS_DATA_UV) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 2;
			bw = 1;
		} else if(format == BS_DATA_NV12) {
			src_bpp = 12;
			dst_bpp = 8;
			chn = 4;
			bw = 2;
		} else if(format <= BS_DATA_ARGB && format >= BS_DATA_BGRA) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 4;
			bw = 3;
		}

		if(format == BS_DATA_NV12) {
			src_stride =  src_w;
			dst_stride =  dst_w * chn;
			src_size = src_stride * src_h * 3 / 2;
			dst_size = dst_stride * dst_h;
		} else {
			src_stride =  src_w * chn;
			dst_stride =  dst_w * chn;
			src_size = src_stride * src_h;
			dst_size = dst_stride * dst_h;
		}

#ifndef USER_ORAM_I
		src_base = (uint8_t *)ddr_memalign(32, src_size);
#else
		src_base = (uint8_t *)oram_memalign(32, src_size);
#endif
#ifndef USER_ORAM_O
		dst_base = (uint8_t *)ddr_memalign(32, dst_size * box_num);
#else
		dst_base = (uint8_t *)oram_memalign(32, dst_size * box_num);
#endif
		opencv_base = (uint8_t *)ddr_memalign(32, dst_size * box_num);
		memset(src_base, 0x00, src_size);
		memset(dst_base, 0x00, dst_size * box_num);
		memset(opencv_base, 0x00, dst_size * box_num);

		fp = fopen("./1920_1080.bgra","r");
		if(fp == NULL) {
			perror("Affine fopen picture Error");
			exit(-1);
		}
		fread(src_base, 32, src_size/32, fp);
		fclose(fp);
		__aie_flushcache_dir((void *)src_base, src_size, NNA_DMA_TO_DEVICE);

		/* get_matrixs(&matrix[0], rand()%180, src_w, src_h, dst_w, dst_h, 0); */
		matrix[0] = ptr[2];
		matrix[1] = ptr[3];
		matrix[2] = ptr[4];
		matrix[3] = ptr[5];
		matrix[4] = ptr[6];
		matrix[5] = ptr[7];
		matrix[6] = ptr[8];
		matrix[7] = ptr[9];
		matrix[8] = ptr[10];

		box_affine_info_s *boxes = (box_affine_info_s *)malloc(sizeof(box_affine_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
			boxes[i].box.x = 0;	//box[i][0];
			boxes[i].box.y = 0;	//box[i][1];
			boxes[i].box.w = src_w;	//box[i][2];
			boxes[i].box.h = src_h;	//box[i][3];
			boxes[i].zero_point = 0x0;
			memcpy(boxes[i].matrix, &matrix[0], sizeof(matrix));
		}

		data_info_s *src = (data_info_s *)malloc(sizeof(data_info_s));
#ifndef USER_ORAM_I
		src->base = __aie_get_ddr_paddr((uint32_t)src_base);
#else
		src->base = __aie_get_oram_paddr((uint32_t)src_base);
#endif
		src->base1 = src->base + src_w * src_h;
		src->format = format;
		src->width = src_w;
		src->height = src_h;
		src->line_stride = src_stride;
#ifndef USER_ORAM_I
		src->locate = BS_DATA_RMEM;
#else
		src->locate = BS_DATA_ORAM;
#endif

		data_info_s *dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
#ifndef USER_ORAM_O
			dst[i].base = __aie_get_ddr_paddr((uint32_t)dst_base) + i* dst_size;
#else
			dst[i].base = __aie_get_oram_paddr((uint32_t)dst_base) + i* dst_size;
#endif
			dst[i].base1 = dst[i].base + dst_w * dst_h;
			dst[i].format = format;
			dst[i].width = dst_w;
			dst[i].height = dst_h;
			dst[i].line_stride = dst_stride;
#ifndef USER_ORAM_O
			dst[i].locate = BS_DATA_RMEM;
#else
			dst[i].locate = BS_DATA_ORAM;
#endif
		}

		__aie_flushcache_dir((void *)dst_base, dst_size * box_num, NNA_DMA_FROM_DEVICE);
		bs_affine_start(src, box_num, dst, boxes, &nv2bgr_coef[0], &nv2bgr_ofst[0]);
		bs_affine_wait();

		data_info_s *model_src = (data_info_s *)malloc(sizeof(data_info_s));
		data_info_s *model_dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		model_src->base = (uint32_t)src_base;
		model_src->base1 = model_src->base + src_w * src_h;
		model_src->format = format;
		model_src->width = src_w;
		model_src->height = src_h;
		model_src->line_stride = src_stride;

		for(i = 0; i < box_num; i++) {
			model_dst[i].base = (uint32_t)opencv_base + i * dst_size;
			model_dst[i].base1 = model_dst[i].base + dst_w * dst_h;
			model_dst[i].format = format;
			model_dst[i].width = dst_w;
			model_dst[i].height = dst_h;
			model_dst[i].line_stride = dst_stride;
		}
		aip_p_model_affine(model_src, box_num, model_dst, boxes, &nv2bgr_coef[0], &nv2bgr_ofst[0]);

		for(i = 0; i < box_num; i++) {
			printf("Affine: src_w:%3d src_h:%3d -> dst_w:%3d dst_h:%3d ---%d\n",src_w,src_h,dst_w,dst_h,i);
			compare_bgra(dst_base + i * dst_size, opencv_base + i * dst_size, dst_h, dst_w, chn);
		}
		printf("%08d------------^_^-----------p\n",num_t++);
#if 0
		for(i = 0; i < box_num; i++) {
			sprintf(name, "%s_%d", "/tmp/test_affine", i);
			fp = fopen(name, "w+");
			fwrite(dst_base + (i * dst_size), 32, dst_size/32, fp);
			fclose(fp);
		}
#endif

		free(model_dst);
		free(model_src);
		free(dst);
		free(src);
		free(boxes);
		ddr_free(opencv_base);
#ifndef USER_ORAM_O
		ddr_free(dst_base);
#else
		oram_free(dst_base);
#endif
#ifndef USER_ORAM_I
		ddr_free(src_base);
#else
		oram_free(src_base);
#endif
	}
	free(param_buffer);

	return 0;
}
#if 1
int aip_affine_test1(bs_data_format_e format, uint32_t box_num)
{
	int ret = 0;
	FILE *fp;
	int i, j;
	char name[32] = {0};
	uint32_t num_t = 0;

	uint8_t chn, bw;
	uint32_t src_w, src_h;
	uint32_t dst_w[10], dst_h[10];
	uint32_t dst_size[10], src_size;
	uint32_t src_bpp, dst_bpp;
	uint32_t src_stride, dst_stride[10];
	uint8_t *src_base, *src_base1;
	uint8_t *dst_base[10], *dst_base1[10];
	uint8_t *opencv_base[10], *opencv_base1[10];
	float matrix[9] = {0};
	float angel = 0;

	while(1)
	{
#if 0
		src_w = 174;
		src_h = 136;
		for(i = 0; i<box_num; i++) {
			dst_w[i] = tmp_w[i];
			dst_h[i] = tmp_h[i];
		}
#else
		src_w = (rand()%WIDTH_MAX + 2)&0xfffe;
		src_h = (rand()%HEIGHT_MAX + 2)&0xfffe;
		for(i = 0; i<box_num; i++) {
			dst_w[i] = (rand()%WIDTH_MAX + 2)&0xfffe;
			dst_h[i] = (rand()%HEIGHT_MAX + 2)&0xfffe;
		}
#endif
		if(format == BS_DATA_Y) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 1;
			bw = 0;
		} else if(format == BS_DATA_UV) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 2;
			bw = 1;
		} else if(format == BS_DATA_NV12) {
			src_bpp = 12;
			dst_bpp = 8;
			chn = 4;
			bw = 2;
		} else if(format <= BS_DATA_ARGB && format >= BS_DATA_BGRA) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 4;
			bw = 3;
		}

		if(format == BS_DATA_NV12) {
			src_stride =  src_w;
			src_size = src_stride * src_h * 3 / 2;
			for(i = 0; i<box_num; i++){
				dst_stride[i] =  dst_w[i] * chn;
				dst_size[i] = dst_stride[i] * dst_h[i];
			}
		} else {
			src_stride =  src_w * chn;
			src_size = src_stride * src_h;
			for(i = 0; i<box_num; i++){
				dst_stride[i] =  dst_w[i] * chn;
				dst_size[i] = dst_stride[i] * dst_h[i];
			}
		}

#ifndef USER_ORAM_I
		src_base = (uint8_t *)ddr_memalign(32, src_size);
		memset(src_base, 0x00, src_size);
#else
		src_base = (uint8_t *)oram_memalign(32, src_size);
		memset(src_base, 0x00, src_size);
#endif
		for(i = 0; i < box_num; i++){
#ifndef USER_ORAM_O
			dst_base[i] = (uint8_t *)ddr_memalign(32, dst_size[i]);
			memset(dst_base[i], 0x12, dst_size[i]);
#else
			dst_base[i] = (uint8_t *)oram_memalign(32, dst_size[i]);
			memset(dst_base[i], 0x12, dst_size[i]);
#endif
			opencv_base[i] = (uint8_t *)ddr_memalign(32, dst_size[i]);
			memset(opencv_base[i], 0x12, dst_size[i]);
		}

		fp = fopen("./1111_resize.bmp.data","r");
		if(fp == NULL) {
			perror("Affine fopen picture  Error");
			exit(-1);
		}
		fread(src_base, 32, src_size/32, fp);
		fclose(fp);
		__aie_flushcache_dir((void *)src_base, src_size, NNA_DMA_TO_DEVICE);


		box_affine_info_s *boxes = (box_affine_info_s *)malloc(sizeof(box_affine_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
			boxes[i].box.x = 0;	//box[i][0];
			boxes[i].box.y = 0;	//box[i][1];
			boxes[i].box.w = src_w;	//box[i][2];
			boxes[i].box.h = src_h;	//box[i][3];
			boxes[i].zero_point = 0x0;
			angel = rand()%180;
			get_matrixs(&matrix[0], angel, src_w, src_h, dst_w[i], dst_h[i], 0);
			memcpy(boxes[i].matrix, &matrix[0], sizeof(matrix));
		}

		data_info_s *src = (data_info_s *)malloc(sizeof(data_info_s));
#ifndef USER_ORAM_I
		src->base = __aie_get_ddr_paddr((uint32_t)src_base);
#else
		src->base = __aie_get_oram_paddr((uint32_t)src_base);
#endif
		src->base1 = src->base + src_w * src_h;
		src->format = format;
		src->width = src_w;
		src->height = src_h;
		src->line_stride = src_stride;
#ifndef USER_ORAM_I
		src->locate = BS_DATA_RMEM;
#else
		src->locate = BS_DATA_ORAM;
#endif

		data_info_s *dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
#ifndef USER_ORAM_O
			dst[i].base = __aie_get_ddr_paddr((uint32_t)dst_base[i]);
#else
			dst[i].base = __aie_get_oram_paddr((uint32_t)dst_base[i]);
#endif
			dst[i].base1 = dst[i].base + dst_w[i] * dst_h[i];
			dst[i].format = format;
			dst[i].width = dst_w[i];
			dst[i].height = dst_h[i];
			dst[i].line_stride = dst_stride[i];
#ifndef USER_ORAM_O
			dst[i].locate = BS_DATA_RMEM;
#else
			dst[i].locate = BS_DATA_ORAM;
#endif
		}

		for(i = 0; i < box_num; i++){
			__aie_flushcache_dir((void *)dst_base[i], dst_size[i], NNA_DMA_FROM_DEVICE);
		}
		bs_affine_start(src, box_num, dst, boxes, &nv2bgr_coef[0], &nv2bgr_ofst[0]);
		bs_affine_wait();

		data_info_s *model_src = (data_info_s *)malloc(sizeof(data_info_s));
		data_info_s *model_dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		model_src->base = (uint32_t)src_base;
		model_src->base1 = model_src->base + src_w * src_h;
		model_src->format = format;
		model_src->width = src_w;
		model_src->height = src_h;
		model_src->line_stride = src_stride;

		for(i = 0; i < box_num; i++) {
			model_dst[i].base = (uint32_t)opencv_base[i];
			model_dst[i].base1 = model_dst[i].base + dst_w[i] * dst_h[i];
			model_dst[i].format = format;
			model_dst[i].width = dst_w[i];
			model_dst[i].height = dst_h[i];
			model_dst[i].line_stride = dst_stride[i];
		}
		/* aip_p_debug_point(4, 0,0); */
		aip_p_model_affine(model_src, box_num, model_dst, boxes, &nv2bgr_coef[0], &nv2bgr_ofst[0]);

		for(i = 0; i < box_num; i++) {
			printf("Affine: src_w:%3d src_h:%3d -> dst_w:%3d dst_h:%3d ---%d\n",src_w,src_h,dst_w[i],dst_h[i],i);
			compare_bgra(dst_base[i], opencv_base[i], dst_h[i], dst_w[i], chn);
		}
		printf("%08d------------^_^-----------p\n",num_t++);
#if 0
		for(i = 0; i < box_num; i++) {
			sprintf(name, "%s_%d", "/tmp/test_affine", i);
			fp = fopen(name, "w+");
			fwrite(dst_base + (i * dst_size), 32, dst_size/32, fp);
			fclose(fp);
		}
#endif

		free(model_dst);
		free(model_src);
		free(dst);
		free(src);
		free(boxes);

		for(i = 0; i < box_num; i++){
			ddr_free(opencv_base[i]);
#ifndef USER_ORAM_O
			ddr_free(dst_base[i]);
#else
			oram_free(dst_base[i]);
#endif
		}
#ifndef USER_ORAM_I
		ddr_free(src_base);
#else
		oram_free(src_base);
#endif
	}

	return 0;
}
#endif

int aip_perspective_test(bs_data_format_e format, uint32_t box_num)
{
	int ret = 0;
	FILE *fp;
	int i, j;
	char name[32] = {0};
	uint32_t num_t = 0;

	uint8_t chn, bw;
	uint32_t src_w, src_h;
	uint32_t dst_w, dst_h;
	uint32_t dst_size, src_size;
	uint32_t src_bpp, dst_bpp;
	uint32_t src_stride, dst_stride;
	uint8_t *src_base, *src_base1;
	uint8_t *dst_base, *dst_base1;
	uint8_t *opencv_base, *opencv_base1;
	float matrix[9] = {0};

#ifdef MAX_SIZE
	src_size = 2048*2048*4;
	dst_size = 2048*2048*4;
#else
	src_size = 512*512*4;
	dst_size = 512*512*4;
#endif

#ifndef USER_ORAM_I
	src_base = (uint8_t *)ddr_memalign(32, src_size);
#else
	src_base = (uint8_t *)oram_memalign(32, src_size);
#endif
#ifndef USER_ORAM_O
	dst_base = (uint8_t *)ddr_memalign(32, dst_size * box_num);
#else
	dst_base = (uint8_t *)oram_memalign(32, dst_size * box_num);
#endif
	opencv_base = (uint8_t *)ddr_memalign(32, dst_size * box_num);

	while(1) {
		memset(src_base, 0x00, src_size);
		memset(dst_base, 0x00, dst_size * box_num);
		memset(opencv_base, 0x00, dst_size * box_num);
#ifdef MAX_SIZE
		src_w = 2048;
		src_h = 2048;
		dst_w = 2048;
		dst_h = 2048;
#else
		src_w =   256;
		src_h =   256;
		dst_w =   256;
		dst_h =   256;
		/*
		src_w =   (rand()%WIDTH_MAX + 2)&0xfffe;
		src_h =   (rand()%HEIGHT_MAX + 2)&0xfffe;
		dst_w =   (rand()%WIDTH_MAX + 2)&0xfffe;
		dst_h =   (rand()%HEIGHT_MAX + 2)&0xfffe;
		*/
#endif
		if(format == BS_DATA_Y) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 1;
			bw = 0;
		} else if(format == BS_DATA_UV) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 2;
			bw = 1;
		} else if(format == BS_DATA_NV12) {
			src_bpp = 12;
			dst_bpp = 8;
			chn = 4;
			bw = 2;
		} else if(format <= BS_DATA_ARGB && format >= BS_DATA_BGRA) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 4;
			bw = 3;
		}

		if(format == BS_DATA_NV12) {
			src_stride =  src_w;
			dst_stride =  dst_w * chn;
			src_size = src_stride * src_h * 3 / 2;
			dst_size = dst_stride * dst_h;
		} else {
			src_stride =  src_w * chn;
			dst_stride =  dst_w * chn;
			src_size = src_stride * src_h;
			dst_size = dst_stride * dst_h;
		}

		fp = fopen("./1111_resize.bmp.nv12","r");
		//fp = fopen("./1111_resize.bmp.data","r");
		if(fp == NULL) {
			perror("Perspective fopen picture Error");
			exit(-1);
		}
		fread(src_base, 32, src_size/32, fp);
		fclose(fp);
		__aie_flushcache_dir((void *)src_base, src_size, NNA_DMA_TO_DEVICE);

		get_matrixs(&matrix[0], rand()%180, src_w, src_h, dst_w, dst_h, 1);
		box_affine_info_s *boxes = (box_affine_info_s *)malloc(sizeof(box_affine_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
			boxes[i].box.x = 32;	//box[i][0];
			boxes[i].box.y = 32;	//box[i][1];
			boxes[i].box.w = 192;	//box[i][2];
			boxes[i].box.h = 192;	//box[i][3];
			boxes[i].zero_point = 0x0;
			memcpy(boxes[i].matrix, &matrix[0], sizeof(matrix));
		}

		data_info_s *src = (data_info_s *)malloc(sizeof(data_info_s));
#ifndef USER_ORAM_I
		src->base = __aie_get_ddr_paddr((uint32_t)src_base);
#else
		src->base = __aie_get_oram_paddr((uint32_t)src_base);
#endif
		src->base1 = src->base + src_w * src_h;
		src->format = format;
		src->width = src_w;
		src->height = src_h;
		src->line_stride = src_stride;
#ifndef USER_ORAM_I
		src->locate = BS_DATA_RMEM;
#else
		src->locate = BS_DATA_ORAM;
#endif

		data_info_s *dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
#ifndef USER_ORAM_O
			dst[i].base = __aie_get_ddr_paddr((uint32_t)dst_base) + i* dst_size;
#else
			dst[i].base = __aie_get_oram_paddr((uint32_t)dst_base) + i* dst_size;
#endif
			dst[i].base1 = dst[i].base + dst_w * dst_h;
			dst[i].format = format;
			dst[i].width = dst_w;
			dst[i].height = dst_h;
			dst[i].line_stride = dst_stride;
#ifndef USER_ORAM_O
			dst[i].locate = BS_DATA_RMEM;
#else
			dst[i].locate = BS_DATA_ORAM;
#endif
		}

		__aie_flushcache_dir((void *)dst_base, dst_size * box_num, NNA_DMA_FROM_DEVICE);
		bs_perspective_start(src, box_num, dst, boxes, &nv2bgr_coef[0], &nv2bgr_ofst[0]);
		bs_perspective_wait();

		data_info_s *model_src = (data_info_s *)malloc(sizeof(data_info_s));
		data_info_s *model_dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		model_src->base = (uint32_t)src_base;
		model_src->base1 = model_src->base + src_w * src_h;
		model_src->format = format;
		model_src->width = src_w;
		model_src->height = src_h;
		model_src->line_stride = src_stride;

		for(i = 0; i < box_num; i++) {
			model_dst[i].base = (uint32_t)opencv_base + i * dst_size;
			model_dst[i].base1 = model_dst[i].base + dst_w * dst_h;
			model_dst[i].format = format;
			model_dst[i].width = dst_w;
			model_dst[i].height = dst_h;
			model_dst[i].line_stride = dst_stride;
		}
		aip_p_model_perspective(model_src, box_num, model_dst, boxes, &nv2bgr_coef[0], &nv2bgr_ofst[0]);

		for(i = 0; i < box_num; i++) {
			printf("Perspective: src_w:%3d src_h:%3d -> dst_w:%3d dst_h:%3d ---%d\n",src_w,src_h,dst_w,dst_h,i);
			compare_bgra(dst_base + i * dst_size, opencv_base + i * dst_size, dst_h, dst_w, chn);
		}

		printf("%d------------^_^-----------\n",num_t++);

#if 1
		for(i = 0; i < box_num; i++) {
			sprintf(name, "%s_%d", "/tmp/test_perspective", i);
			fp = fopen(name, "w+");
			fwrite(dst_base + (i * dst_size), 32, dst_size/32, fp);
			fclose(fp);
		}
#endif
		free(model_dst);
		free(model_src);
		free(dst);
		free(src);
		free(boxes);
	}
	ddr_free(opencv_base);
#ifndef USER_ORAM_O
	ddr_free(dst_base);
#else
	oram_free(dst_base);
#endif
#ifndef USER_ORAM_I
	ddr_free(src_base);
#else
	oram_free(src_base);
#endif

	return 0;
}

int aip_covert_test(uint32_t len)
{
	int ret = 0;
	FILE *fp;
	int i, j;
	uint32_t task_off;
	char name[32] = {0};
	uint32_t num_t = 0;

	task_info_s task_info;
	uint32_t task_len;
	uint32_t src_w, src_h;
	uint32_t dst_w, dst_h;
	uint32_t dst_size, src_size;
	uint32_t src_bpp, dst_bpp;
	uint8_t *src_base, *src_base1;
	uint8_t *dst_base, *dst_base1;
	uint8_t *opencv_base, *opencv_base1;

#ifdef MAX_SIZE
	src_size = 3840*2048*4;
	dst_size = 3840*2048*4;
#else
	src_size = 512*512*4;
	dst_size = 512*512*4;
#endif

#ifndef USER_ORAM_I
	src_base = (uint8_t *)ddr_memalign(32, src_size);
#else
	src_base = (uint8_t *)oram_memalign(32, src_size);
#endif
#ifndef USER_ORAM_O
	dst_base = (uint8_t *)ddr_memalign(32, dst_size);
#else
	dst_base = (uint8_t *)oram_memalign(32, dst_size);
#endif
	opencv_base = (uint8_t *)ddr_memalign(32, dst_size);

	while(1) {
		memset(src_base, 0x00, src_size);
		memset(dst_base, 0x00, dst_size);
		memset(opencv_base, 0x00, dst_size);
#ifdef MAX_SIZE
		src_w = 3840;
		src_h = 2048;
		dst_w = src_w;
		dst_h = src_h;
#else
		//src_w = (rand()%WIDTH_MAX + 2)&0xfffe;
		//src_h = (rand()%HEIGHT_MAX + 2)&0xfffe;
		src_w = 256;
		src_h = 256;
		dst_w = src_w;
		dst_h = src_h;
#endif

		src_bpp = 12;
		dst_bpp = 32;
		src_size = src_w * src_h * src_bpp >> 3;
		dst_size = dst_w * dst_h * dst_bpp >> 3;

		fp = fopen("./1111_resize.bmp.nv12","r");
		if(fp == NULL) {
			perror("Covert fopen picture Error");
			exit(-1);
		}
		fread(src_base, 32, src_size/32, fp);
		fclose(fp);
		__aie_flushcache_dir((void *)src_base, src_size, NNA_DMA_TO_DEVICE);

		data_info_s *src =  (data_info_s *)malloc(sizeof(data_info_s));
#ifndef USER_ORAM_I
		src->base = __aie_get_ddr_paddr((uint32_t)src_base);
#else
		src->base = __aie_get_oram_paddr((uint32_t)src_base);
#endif
		src->base1 = src->base + src_w * src_h;
		src->format = BS_DATA_NV12;
		src->width = src_w;
		src->height = src_h;
		src->line_stride = src_w;
#ifndef USER_ORAM_I
		src->locate = BS_DATA_RMEM;
#else
		src->locate = BS_DATA_ORAM;
#endif

		data_info_s *dst = (data_info_s *)malloc(sizeof(data_info_s));
#ifndef USER_ORAM_O
		dst->base = __aie_get_ddr_paddr((uint32_t)dst_base);
#else
		dst->base = __aie_get_oram_paddr((uint32_t)dst_base);
#endif
		dst->base1 = 0;
		dst->format = BS_DATA_BGRA;
		dst->width = dst_w;
		dst->height = dst_h;
		dst->line_stride = dst_w * dst_bpp >> 3;
#ifndef USER_ORAM_O
		dst->locate = BS_DATA_RMEM;
#else
		dst->locate = BS_DATA_ORAM;
#endif
		task_len = len;
		__aie_flushcache_dir((void *)dst_base, dst_size, NNA_DMA_FROM_DEVICE);
		bs_covert_cfg(src, dst, &nv2bgr_coef[0], &nv2bgr_ofst[0], &task_info);
		for(i = 0; i < dst_h;) {
			task_off = dst_h - i;
			if(task_len <= task_off) {
				task_len = task_len;
			} else {
				task_len = task_off;
			}
			i = i + task_len;
			task_info.task_len = task_len;
			bs_covert_step_start(&task_info, dst->base, BS_DATA_RMEM);
			dst->base = dst->base + task_len * dst->line_stride;
			bs_covert_step_wait();
		}
		bs_covert_step_exit();

		nv12_bgra(src_base, src_base + src_h * src_w, src_h, src_w,
				src_w, opencv_base, dst_w * 4, BS_DATA_BGRA);
		printf("Covert: src_w:%3d src_h:%3d -> dst_w:%3d dst_h:%3d\n",src_w,src_h,dst_w,dst_h);
		compare_bgra(dst_base, opencv_base, dst_h, dst_w, 4);
		printf("%08d------------^_^-----------t\n",num_t++);
#if 0
		sprintf(name, "%s", "/tmp/test_covert");
		fp = fopen(name, "w+");
		fwrite(dst_base, 32, dst_size/32, fp);
		fclose(fp);
#endif
		free(dst);
		free(src);
	}
	ddr_free(opencv_base);
#ifndef USER_ORAM_O
	ddr_free(dst_base);
#else
	oram_free(dst_base);
#endif
#ifndef USER_ORAM_I
	ddr_free(src_base);
#else
	oram_free(src_base);
#endif

	return 0;
}

int aip_resize_test(bs_data_format_e format, uint32_t box_num)
{
	int ret = 0;
	FILE *fp;
	int i, j;
	int index;
	char name[32] = {0};
	uint32_t num_t = 0;

	uint8_t chn , bw;
	uint32_t src_w, src_h;
	uint32_t dst_w, dst_h;
	uint32_t dst_size, src_size;
	uint32_t src_bpp, dst_bpp;
	uint32_t src_stride, dst_stride;
	uint8_t *src_base, *src_base1;
	uint8_t *dst_base, *dst_base1;
	uint8_t *opencv_base, *opencv_base1;
	int x, y, h, w;

	float time_use=0;
	struct timeval start;
	struct timeval end;
#ifdef MAX_SIZE
	src_size = 3840*2048*4;
	dst_size = 3840*2048*4;
#else
	src_size = 512*512*4;
	dst_size = 512*512*4;
#endif

#ifndef USER_ORAM_I
	src_base = (uint8_t *)ddr_memalign(32, src_size);
#else
	src_base = (uint8_t *)oram_memalign(32, src_size);
#endif
#ifndef USER_ORAM_O
	dst_base = (uint8_t *)ddr_memalign(32, dst_size * box_num);
#else
	dst_base = (uint8_t *)oram_memalign(32, dst_size * box_num);
#endif
	opencv_base = (uint8_t *)ddr_memalign(32, dst_size * box_num);

	while(1) {
		memset(src_base, 0x00, src_size);
		memset(dst_base, 0x00, dst_size * box_num);
		memset(opencv_base, 0x00, dst_size * box_num);
#ifdef MAX_SIZE
		src_w = 3840;
		src_h = 2048;
		dst_w = 3840;
		dst_h = 2048;
#else
		/*
		src_w = (rand()%WIDTH_MAX + 2)&0xfffe;
		src_h = (rand()%HEIGHT_MAX + 2)&0xfffe;
		dst_w = (rand()%WIDTH_MAX + 2)&0xfffe;
		dst_h = (rand()%HEIGHT_MAX + 2)&0xfffe;
		*/
		src_w = 512;
		src_h = 512;
		dst_w = 144;
		dst_h = 144;
#endif
		if(format == BS_DATA_Y) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 1;
			bw = 0;
		} else if(format == BS_DATA_UV) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 2;
			bw = 1;
		} else if(format <= BS_DATA_ARGB && format >= BS_DATA_BGRA){
			src_bpp = 8;
			dst_bpp = 8;
			chn = 4;
			bw = 2;
		} else if(format == BS_DATA_FMU2) {
			src_bpp = 2;
			dst_bpp = 2;
			chn = 32;
			bw = 3;
		} else if(format == BS_DATA_FMU4) {
			src_bpp = 4;
			dst_bpp = 4;
			chn = 32;
			bw = 4;
		} else if(format == BS_DATA_FMU8) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 32;
			bw = 5;
		} else if(format == BS_DATA_FMU8_H) {
			src_bpp = 8;
			dst_bpp = 8;
			chn = 16;
			bw = 6;
		} else if(format == BS_DATA_NV12) {
			src_bpp = 12;
			dst_bpp = 12;
			chn = 1;
			bw = 7;
		}

		if(format == BS_DATA_NV12) {
			src_stride =  src_w;
			dst_stride =  dst_w;
			src_size = src_stride * src_h * src_bpp >> 3;
			dst_size = dst_stride * dst_h * dst_bpp >> 3;
		} else {
			src_stride =  src_w * chn * src_bpp >> 3;
			dst_stride =  dst_w * chn * dst_bpp >> 3;
			src_size = src_stride * src_h;
			dst_size = dst_stride * dst_h;
		}

		fp = fopen("./1111_resize.bmp.data","r");
		if(fp == NULL) {
			perror("Resize fopen picture Error");
			exit(-1);
		}
		fread(src_base, 32, src_size/32, fp);
		fclose(fp);
		__aie_flushcache_dir((void *)src_base, src_size, NNA_DMA_TO_DEVICE);

		box_resize_info_s *boxes = (box_resize_info_s *)malloc(sizeof(box_resize_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
			boxes[i].box.x = 0;	//box[index][0];
			boxes[i].box.y = 0;	//box[index][1];
			boxes[i].box.w = src_w;	//box[index][2];
			boxes[i].box.h = src_h;	//box[index][3];
		}

		data_info_s *src = (data_info_s *)malloc(sizeof(data_info_s));
#ifndef USER_ORAM_I
		src->base = __aie_get_ddr_paddr((uint32_t)src_base);
#else
		src->base = __aie_get_oram_paddr((uint32_t)src_base);
#endif
		src->base1 = src->base + src_w * src_h;
		src->format = format;
		src->width = src_w;
		src->height = src_h;
		src->line_stride = src_stride;
#ifndef USER_ORAM_I
		src->locate = BS_DATA_RMEM;
#else
		src->locate = BS_DATA_ORAM;
#endif

		data_info_s *dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		for(i = 0; i < box_num; i++) {
#ifndef USER_ORAM_O
			dst[i].base = __aie_get_ddr_paddr((uint32_t)dst_base) + i * dst_size;
#else
			dst[i].base = __aie_get_oram_paddr((uint32_t)dst_base) + i * dst_size;
#endif
			dst[i].base1 = dst[i].base + dst_w * dst_h;
			dst[i].format = format;
			dst[i].width = dst_w;
			dst[i].height = dst_h;
			dst[i].line_stride = dst_stride;
#ifndef USER_ORAM_O
			dst[i].locate = BS_DATA_RMEM;
#else
			dst[i].locate = BS_DATA_ORAM;
#endif
		}

		__aie_flushcache_dir((void *)dst_base, dst_size * box_num, NNA_DMA_FROM_DEVICE);
		gettimeofday(&start,NULL);
		bs_resize_start(src, box_num, dst, boxes, &nv2bgr_coef[0], &nv2bgr_ofst[0]);
		bs_resize_wait();
		gettimeofday(&end,NULL);
		time_use=(end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);//微秒
		printf("time_use is %.10f\n",time_use);
		data_info_s *model_src = (data_info_s *)malloc(sizeof(data_info_s));
		data_info_s *model_dst = (data_info_s *)malloc(sizeof(data_info_s) * box_num);
		model_src->base = (uint32_t)src_base;
		model_src->base1 = model_src->base + src_w * src_h;
		model_src->format = format;
		model_src->width = src_w;
		model_src->height = src_h;
		model_src->line_stride = src_stride;

		for(i = 0; i < box_num; i++) {
			model_dst[i].base = (uint32_t)(opencv_base) + i * dst_size;
			model_dst[i].base1 = model_dst[i].base + dst_w * dst_h;
			model_dst[i].format = format;
			model_dst[i].width = dst_w;
			model_dst[i].height = dst_h;
			model_dst[i].line_stride = dst_stride;
		}
		aip_f_model(model_src, box_num, model_dst, boxes);

		if(format == BS_DATA_NV12) {
			/* resize_nv12_nv12(src_base, src_w, src_h,opencv_base, dst_w, dst_h); */
			/* aip_f_mdl(&aip_f_s); */
			for(i = 0; i < box_num; i++) {
				printf("Resize: src_w:%3d src_h:%3d -> dst_w:%3d dst_h:%3d ---%d\n",src_w,src_h,dst_w,dst_h,i);
				compare_nv12(dst_base + i * dst_size, opencv_base + i * dst_size, dst_h, dst_w);
			}
		} else if(format == BS_DATA_FMU2) {
			for(i = 0; i < box_num; i++) {
				printf("Resize: src_w:%3d src_h:%3d -> dst_w:%3d dst_h:%3d ---%d\n",src_w,src_h,dst_w,dst_h,i);
				compare_bgra(dst_base + i * dst_size, opencv_base + i * dst_size, dst_h, dst_w / 4, chn);
			}
		} else if(format == BS_DATA_FMU4) {
			for(i = 0; i < box_num; i++) {
				printf("Resize: src_w:%3d src_h:%3d -> dst_w:%3d dst_h:%3d ---%d\n",src_w,src_h,dst_w,dst_h,i);
				compare_bgra(dst_base + i * dst_size, opencv_base + i * dst_size, dst_h, dst_w / 2, chn);
			}
		} else {
			/* opencv_resize(src_base, src_w, src_h, chn, opencv_base, dst_w, dst_h, chn); */
			/* aip_f_mdl(&aip_f_s); */
			for(i = 0; i < box_num; i++) {
				printf("Resize: src_w:%3d src_h:%3d -> dst_w:%3d dst_h:%3d ---%d\n",src_w,src_h,dst_w,dst_h,i);
				compare_bgra(dst_base + i * dst_size, opencv_base + i * dst_size, dst_h, dst_w, chn);
			}
		}
		printf("%08d------------^_^-----------f\n",num_t++);
#if 0
		for(i = 0; i < box_num; i++) {
			sprintf(name, "%s_%d", "/tmp/test_resize", i);
			fp = fopen(name, "w+");
			fwrite(dst_base + (i * dst_size), 32, dst_size/32, fp);
			fclose(fp);
		}

		sprintf(name, "%s_%d", "/tmp/test_opencv", 0);
		fp = fopen(name, "w+");
		fwrite(opencv_base, 32, dst_size/32, fp);
		fclose(fp);
#endif

		free(model_dst);
		free(model_src);
		free(dst);
		free(src);
		free(boxes);
	}
	ddr_free(opencv_base);
#ifndef USER_ORAM_O
	ddr_free(dst_base);
#else
	oram_free(dst_base);
#endif
#ifndef USER_ORAM_I
	ddr_free(src_base);
#else
	oram_free(src_base);
#endif

	return 0;
}
