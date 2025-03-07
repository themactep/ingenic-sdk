#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <string.h>
#include <pthread.h>

#include "jz_aip.h"

//#define TEST_RATE
#ifdef TEST_RATE
#define TIME_OUT_EN 	0x1<<6
#else
#define TIME_OUT_EN 	0x0<<6
#endif

pthread_mutex_t mutex_f;
pthread_mutex_t mutex_p;
pthread_mutex_t mutex_t;

struct jz_aip_chainbuf {
	void *vaddr;
	uint32_t paddr;
	uint32_t size;
};

struct  aip_p_chaindesc {
	uint32_t	timeout[2];
	uint32_t	mode[2];
	uint32_t	src_base_y[2];
	uint32_t	src_base_uv[2];
	uint32_t	src_stride[2];
	uint32_t	dst_base[2];
	uint32_t	dst_stride[2];
	uint32_t	dst_size[2];
	uint32_t	src_size[2];
	uint32_t	dummy_val[2];
	uint32_t	coef0[2];
	int32_t		coef1[2];
	int32_t		coef2[2];
	int32_t		coef3[2];
	int32_t		coef4[2];
	int32_t		coef5[2];
	int32_t		coef6[2];
	int32_t		coef7[2];
	int32_t		coef8[2];
	int32_t		coef9[2];
	int32_t		coef10[2];
	int32_t		coef11[2];
	int32_t		coef12[2];
	int32_t		coef13[2];
	int32_t		coef14[2];
	uint32_t	offset[2];
	uint32_t	p_ctrl[2];
};

struct aip_t_chaindesc {
	uint32_t		timeout[2];
	uint32_t		src_base_y[2];
	uint32_t		src_base_uv[2];
	uint32_t		dst_base[2];
	uint32_t		src_size[2];
	uint32_t		stride[2];
	uint32_t		task_len[2];
	uint32_t		task_skip_base[2];
	uint32_t		t_ctrl[2];
};

struct aip_f_chaindesc {
	uint32_t		cfg[2];
	uint32_t		timeout[2];
	uint32_t		scale_x[2];
	uint32_t		scale_y[2];
	int32_t			trans_x[2];
	int32_t			trans_y[2];
	uint32_t		src_base_y[2];
	uint32_t		src_base_uv[2];
	uint32_t		dst_base_y[2];
	uint32_t		dst_base_uv[2];
	uint32_t		src_size[2];
	uint32_t		dst_size[2];
	uint32_t		stride[2];
	uint32_t		f_ctrl[2];
};

struct aip_manage {
	char index;
	char name[16];
	int mem_fd;
	int fd;
	uint32_t aip_io_base;
	uint32_t aip_io_size;
	uint32_t aip_io_vaddr;

	struct jz_aip_chainbuf chainbuf;
	uint32_t chain_num;
};

unsigned int jz_aip_readl(unsigned int base, unsigned int offset)
{
	unsigned int value = *(volatile unsigned int *)(base + offset);
	return value;
}

unsigned int jz_aip_writel(unsigned int base, unsigned int offset, unsigned int value)
{
	*(volatile unsigned int *)(base + offset) = value;
	return 0;
}

struct aip_manage *manage_p = NULL;
struct aip_manage *manage_t = NULL;
struct aip_manage *manage_f = NULL;

static int get_mapped_addr(struct aip_manage *manage)
{
	int ret = 0;
	unsigned int phy_addr;
	unsigned int mapped_size;
	unsigned int offset;
	void *mapped_vbase = NULL;

	switch(manage->index) {
		case INDEX_AIP_T:
			offset = 0;
			break;
		case INDEX_AIP_F:
			offset = 0x200;
			break;
		case INDEX_AIP_P:
			offset = 0x300;
			break;
	}

	phy_addr = manage->aip_io_base;
	mapped_size = manage->aip_io_size;
	manage->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (manage->mem_fd < 0) {
		printf("open /dev/mem failed\n");
		return -1;
	}

	mapped_vbase = mmap(NULL,
			mapped_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			manage->mem_fd,
			(off_t)(phy_addr & 0xffffffff));
	if (mapped_vbase == 0) {
		printf("mem mmap failed\n");
		ret = -1;
		goto close_fd;
	}

	manage->aip_io_vaddr = (uint32_t)mapped_vbase + offset ;
	/* printf("aip_io_vaddr = %p\n",manage->aip_io_vaddr); */

	return 0;
close_fd:
	close(manage->mem_fd);
	return ret;
}

int release_mapped_addr(struct aip_manage *manage)
{
	unsigned int offset;
	unsigned int mapped_vbase;

	switch(manage->index) {
		case INDEX_AIP_T:
			offset = 0;
			break;
		case INDEX_AIP_F:
			offset = 0x200;
			break;
		case INDEX_AIP_P:
			offset = 0x300;
			break;
	}
	mapped_vbase = manage->aip_io_vaddr - offset;

	munmap((void *)mapped_vbase, manage->aip_io_size);
	close(manage->mem_fd);
	return 0;
}

int aip_manage_init(struct aip_manage *manage)
{
	int ret = 0;
	char name[32];
	struct jz_aip_chainbuf chainbuf;

	ret = get_mapped_addr(manage);
	if(ret) {
		printf("get_mapped_addr error\n");
		goto err_get_mapped_addr;
	}
	sprintf(name,"/dev/%s",manage->name);
	manage->fd = open(name, O_RDWR);
	if (manage->fd < 0) {
		printf("open %s failed\n",name);
		goto err_open_fd;
	}

	if(manage->chain_num > 1) {
		chainbuf.size = manage->chainbuf.size;
		if((ret = ioctl(manage->fd, IOCTL_AIP_MALLOC, &chainbuf)) < 0) {
			printf("IOCTL_AIP_MALLOC failed\n");
			goto err_malloc;
		}

		manage->chainbuf.size = chainbuf.size;
		manage->chainbuf.paddr = chainbuf.paddr;
		manage->chainbuf.vaddr = mmap(NULL,
				manage->chainbuf.size,
				PROT_READ | PROT_WRITE,
				MAP_SHARED,
				manage->fd,
				(off_t)(manage->chainbuf.paddr & 0xffffffff));
		if(manage->chainbuf.vaddr == 0) {
			printf("aip map failed\n");
			ret = -1;
			goto err_mmap;
		}
	}
	/* printf("aip_io_base = 0x%08x, aip_io_vaddr = %p; aip_io_size = %d\n", */
	/* manage->aip_io_base,manage->aip_io_vaddr, manage->aip_io_size); */
	/* printf("chainbuf.paddr = 0x%08x, chainbuf.vaddr = %p; chainbuf.size = %d\n", */
	/* manage->chainbuf.paddr, manage->chainbuf.vaddr, manage->chainbuf.size); */

	return 0;

err_mmap:
err_malloc:
	ioctl(manage->fd, IOCTL_AIP_FREE, NULL);
	close(manage->fd);
err_open_fd:
err_get_mapped_addr:
	free(manage);
	return ret;
}

int aip_manage_deinit(struct aip_manage *manage)
{
	int ret = 0;

	if(manage->chain_num > 1) {
		munmap(manage->chainbuf.vaddr, manage->chainbuf.size);
		if((ret = ioctl(manage->fd, IOCTL_AIP_FREE, NULL)) < 0) {
			printf("IOCTL_AIP_FREE failed\n");
		}
	}
	close(manage->fd);
	release_mapped_addr(manage);

	return ret;
}

int aip_p_reset()
{
	uint32_t value;
	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_CTRL, 0x2);
	value = jz_aip_readl(manage_p->aip_io_vaddr, AIP_P_CTRL) & 0x2;
	while(value) {
		usleep(10);
		value = jz_aip_readl(manage_p->aip_io_vaddr, AIP_P_CTRL) & 0x2;
	}
	return 0;
}

int aip_t_reset()
{
	uint32_t value;
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_CTRL, 0x2);
	value = jz_aip_readl(manage_t->aip_io_vaddr, AIP_T_CTRL) & 0x2;
	while(value) {
		usleep(10);
		value = jz_aip_readl(manage_t->aip_io_vaddr, AIP_T_CTRL) & 0x2;
	}
	return 0;
}

int aip_f_reset()
{
	uint32_t value;
	jz_aip_writel(manage_f->aip_io_vaddr, AIP_F_CTRL, 0x2);
	value = jz_aip_readl(manage_f->aip_io_vaddr, AIP_F_CTRL) & 0x2;
	while(value) {
		usleep(10);
		value = jz_aip_readl(manage_f->aip_io_vaddr, AIP_F_CTRL) & 0x2;
	}
	return 0;
}

int aip_p_config_nodes(struct aip_manage *manage, struct aip_p_chainnode *chainnode, int chain_num)
{
	uint32_t i;
	struct aip_p_chainnode *node = NULL;
	struct aip_p_chaindesc *aip_p = aip_p = (struct aip_p_chaindesc *)manage->chainbuf.vaddr;

	if(chain_num == 0) {
		node = chainnode;
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_TIMEOUT	,0xffffffff);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_MODE		,node->mode);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_SRC_Y_BASE	,node->src_base_y);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_SRC_UV_BASE	,node->src_base_uv);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_SRC_STRIDE	,node->src_stride);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_DST_BASE	,node->dst_base);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_DST_STRIDE	,node->dst_stride);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_DST_SIZE	,node->dst_height<<16 | node->dst_width);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_SRC_SIZE	,node->src_height<<16 | node->src_width);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_DUMMY_VAL	,node->dummy_val);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF0		,node->coef_parm0);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF1		,node->coef_parm1);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF2		,node->coef_parm2);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF3		,node->coef_parm3);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF4		,node->coef_parm4);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF5		,node->coef_parm5);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF6		,node->coef_parm6);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF7		,node->coef_parm7);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF8		,node->coef_parm8);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF9		,node->coef_parm9);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF10	,node->coef_parm10);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF11	,node->coef_parm11);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF12	,node->coef_parm12);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF13	,node->coef_parm13);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_COEF14	,node->coef_parm14);
		jz_aip_writel(manage->aip_io_vaddr, AIP_P_OFFSET	,node->offset);
	} else {
		for(i = 0; i < chain_num; i++) {
			node = &chainnode[i];
			memset(&aip_p[i], 0x00, sizeof(struct aip_p_chaindesc));
			aip_p[i].timeout[0] 	= 0xffffffff;
			aip_p[i].timeout[1] 	= JZ_AIP_P_IOBASE + AIP_P_TIMEOUT;
			aip_p[i].mode[0] 	= node->mode;
			aip_p[i].mode[1] 	= JZ_AIP_P_IOBASE + AIP_P_MODE;
			aip_p[i].src_base_y[0] 	= node->src_base_y;
			aip_p[i].src_base_y[1] 	= JZ_AIP_P_IOBASE + AIP_P_SRC_Y_BASE;
			aip_p[i].src_base_uv[0] = node->src_base_uv;
			aip_p[i].src_base_uv[1] = JZ_AIP_P_IOBASE + AIP_P_SRC_UV_BASE;
			aip_p[i].src_stride[0] 	= node->src_stride;
			aip_p[i].src_stride[1] 	= JZ_AIP_P_IOBASE + AIP_P_SRC_STRIDE;
			aip_p[i].dst_base[0] 	= node->dst_base;
			aip_p[i].dst_base[1] 	= JZ_AIP_P_IOBASE + AIP_P_DST_BASE;
			aip_p[i].dst_stride[0] 	= node->dst_stride;
			aip_p[i].dst_stride[1] 	= JZ_AIP_P_IOBASE + AIP_P_DST_STRIDE;
			aip_p[i].dst_size[0] 	= node->dst_height<<16 | node->dst_width;
			aip_p[i].dst_size[1] 	= JZ_AIP_P_IOBASE + AIP_P_DST_SIZE;
			aip_p[i].src_size[0] 	= node->src_height<<16 | node->src_width;
			aip_p[i].src_size[1] 	= JZ_AIP_P_IOBASE + AIP_P_SRC_SIZE;
			aip_p[i].dummy_val[0] 	= node->dummy_val;
			aip_p[i].dummy_val[1] 	= JZ_AIP_P_IOBASE + AIP_P_DUMMY_VAL;
			aip_p[i].coef0[0] 	= node->coef_parm0;
			aip_p[i].coef0[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF0;
			aip_p[i].coef1[0] 	= node->coef_parm1;
			aip_p[i].coef1[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF1;
			aip_p[i].coef2[0] 	= node->coef_parm2;
			aip_p[i].coef2[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF2;
			aip_p[i].coef3[0] 	= node->coef_parm3;
			aip_p[i].coef3[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF3;
			aip_p[i].coef4[0] 	= node->coef_parm4;
			aip_p[i].coef4[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF4;
			aip_p[i].coef5[0] 	= node->coef_parm5;
			aip_p[i].coef5[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF5;
			aip_p[i].coef6[0] 	= node->coef_parm6;
			aip_p[i].coef6[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF6;
			aip_p[i].coef7[0] 	= node->coef_parm7;
			aip_p[i].coef7[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF7;
			aip_p[i].coef8[0] 	= node->coef_parm8;
			aip_p[i].coef8[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF8;
			aip_p[i].coef9[0] 	= node->coef_parm9;
			aip_p[i].coef9[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF9;
			aip_p[i].coef10[0] 	= node->coef_parm10;
			aip_p[i].coef10[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF10;
			aip_p[i].coef11[0] 	= node->coef_parm11;
			aip_p[i].coef11[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF11;
			aip_p[i].coef12[0] 	= node->coef_parm12;
			aip_p[i].coef12[1] 	= JZ_AIP_P_IOBASE + AIP_P_COEF12;
			aip_p[i].coef13[0] 	= node->coef_parm13;
			aip_p[i].coef13[1]	= JZ_AIP_P_IOBASE + AIP_P_COEF13;
			aip_p[i].coef14[0]	= node->coef_parm14;
			aip_p[i].coef14[1]	= JZ_AIP_P_IOBASE + AIP_P_COEF14;
			aip_p[i].offset[0]	= node->offset;
			aip_p[i].offset[1]	= JZ_AIP_P_IOBASE + AIP_P_OFFSET;
			aip_p[i].p_ctrl[0]	= 1;
			aip_p[i].p_ctrl[1]	= JZ_AIP_P_IOBASE + AIP_P_CTRL | 0x1<<31;
		}
	}

	return 0;
}

int aip_t_config_nodes(struct aip_manage *manage, struct aip_t_chainnode *chainnode, int chain_num)
{
	uint32_t i;
	struct aip_t_chainnode *node = NULL;
	struct aip_t_chaindesc *aip_t = (struct aip_t_chaindesc *)manage->chainbuf.vaddr;

	if(chain_num == 0) {
		node = chainnode;
		jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_TIMEOUT	,0xffffffff);
		jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_SRC_Y_BASE	,node->src_base_y);
		jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_SRC_UV_BASE	,node->src_base_uv);
		jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_DST_BASE	,node->dst_base);
		jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_SRC_SIZE	,node->src_height<<16 | chainnode->src_width);
		jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_STRIDE	,node->dst_stride<<16 | chainnode->src_stride);
		jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_SRC_Y_BASE	,node->src_base_y);
	} else {
		for(i = 0; i < chain_num; i++) {
			node = &chainnode[i];
			memset(&aip_t[i], 0x00, sizeof(struct aip_t_chaindesc));
			aip_t[i].timeout[0] 	= 0xffffffff;
			aip_t[i].timeout[1] 	= JZ_AIP_T_IOBASE + AIP_T_TIMEOUT;
			aip_t[i].src_base_y[0] 	= node->src_base_y;
			aip_t[i].src_base_y[1] 	= JZ_AIP_T_IOBASE + AIP_T_SRC_Y_BASE;
			aip_t[i].src_base_uv[0] = node->src_base_uv;
			aip_t[i].src_base_uv[1] = JZ_AIP_T_IOBASE + AIP_T_SRC_UV_BASE;
			aip_t[i].dst_base[0] 	= node->dst_base;
			aip_t[i].dst_base[1] 	= JZ_AIP_T_IOBASE + AIP_T_DST_BASE;
			aip_t[i].src_size[0] 	= node->src_height<<16 | node->src_width;
			aip_t[i].src_size[1] 	= JZ_AIP_T_IOBASE + AIP_T_SRC_SIZE;
			aip_t[i].stride[0] 	= node->dst_stride<<16 | node->src_stride;
			aip_t[i].stride[1] 	= JZ_AIP_T_IOBASE + AIP_T_STRIDE;
			aip_t[i].t_ctrl[0] 	= 1;
			aip_t[i].t_ctrl[1] 	= JZ_AIP_T_IOBASE + AIP_T_CTRL | node->t_ctrl;
		}
	}

	return 0;
}

int aip_f_config_nodes(struct aip_manage *manage, struct aip_f_chainnode *chainnode, int chain_num, unsigned int cfg)
{
	uint32_t i;
	struct aip_f_chainnode *node = NULL;
	struct aip_f_chaindesc *aip_f = (struct aip_f_chaindesc *)manage->chainbuf.vaddr;

	if(chain_num == 0) {
		node = chainnode;
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_TIMEOUT	,0xffffffff);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_SCALE_X	,node->scale_x);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_SCALE_Y	,node->scale_y);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_TRANS_X	,node->trans_x + 16);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_TRANS_Y	,node->trans_y + 16);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_SRC_Y_BASE	,node->src_base_y);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_SRC_UV_BASE	,node->src_base_uv);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_DST_Y_BASE	,node->dst_base_y);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_DST_UV_BASE	,node->dst_base_uv);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_SRC_SIZE	,node->src_height<<16 | node->src_width);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_DST_SIZE	,node->dst_height<<16 | node->dst_width);
		jz_aip_writel(manage->aip_io_vaddr, AIP_F_STRIDE	,node->dst_stride<<16 | node->src_stride);
	} else {
		for(i = 0; i < chain_num -1; i++) {
			node = &chainnode[i];
			memset(&aip_f[i], 0x00, sizeof(struct aip_f_chaindesc));
			aip_f[i].cfg[0] 	= cfg | 0x1<<6 | 0x1<<5|0x1<<4 |0x1<<3 | 0x1 << 2;
			aip_f[i].cfg[1] 	= JZ_AIP_F_IOBASE + AIP_F_CFG;
			aip_f[i].timeout[0] 	= 0xffffffff;
			aip_f[i].timeout[1] 	= JZ_AIP_F_IOBASE + AIP_F_TIMEOUT;
			aip_f[i].scale_x[0] 	= node->scale_x;
			aip_f[i].scale_x[1] 	= JZ_AIP_F_IOBASE + AIP_F_SCALE_X;
			aip_f[i].scale_y[0] 	= node->scale_y;
			aip_f[i].scale_y[1] 	= JZ_AIP_F_IOBASE + AIP_F_SCALE_Y;
			aip_f[i].trans_x[0] 	= node->trans_x + 16;
			aip_f[i].trans_x[1] 	= JZ_AIP_F_IOBASE + AIP_F_TRANS_X;
			aip_f[i].trans_y[0] 	= node->trans_y + 16;
			aip_f[i].trans_y[1] 	= JZ_AIP_F_IOBASE + AIP_F_TRANS_Y;
			aip_f[i].src_base_y[0] 	= node->src_base_y;
			aip_f[i].src_base_y[1] 	= JZ_AIP_F_IOBASE + AIP_F_SRC_Y_BASE;
			aip_f[i].src_base_uv[0] = node->src_base_uv;
			aip_f[i].src_base_uv[1] = JZ_AIP_F_IOBASE + AIP_F_SRC_UV_BASE;
			aip_f[i].dst_base_y[0] 	= node->dst_base_y;
			aip_f[i].dst_base_y[1] 	= JZ_AIP_F_IOBASE + AIP_F_DST_Y_BASE;
			aip_f[i].dst_base_uv[0] = node->dst_base_uv;
			aip_f[i].dst_base_uv[1] = JZ_AIP_F_IOBASE + AIP_F_DST_UV_BASE;
			aip_f[i].src_size[0] 	= node->src_height<<16 | node->src_width;
			aip_f[i].src_size[1] 	= JZ_AIP_F_IOBASE + AIP_F_SRC_SIZE;
			aip_f[i].dst_size[0] 	= node->dst_height<<16 | node->dst_width;
			aip_f[i].dst_size[1] 	= JZ_AIP_F_IOBASE + AIP_F_DST_SIZE;
			aip_f[i].stride[0] 	= node->dst_stride<<16 | node->src_stride;
			aip_f[i].stride[1] 	= JZ_AIP_F_IOBASE + AIP_F_STRIDE;
			aip_f[i].f_ctrl[0] 	= 1;
			aip_f[i].f_ctrl[1] 	= JZ_AIP_F_IOBASE + AIP_F_CTRL | 0x1<<31;
		}
		node = &chainnode[i];
		memset(&aip_f[i], 0x00, sizeof(struct aip_f_chaindesc));
		aip_f[i].cfg[0] 	= cfg | 0x1<<6 | 0x1<<5|0x1<<4 |0x0<<3 | 0x1 << 2;
		aip_f[i].cfg[1] 	= JZ_AIP_F_IOBASE + AIP_F_CFG;
		aip_f[i].timeout[0] 	= 0xf;
		aip_f[i].timeout[1] 	= JZ_AIP_F_IOBASE + AIP_F_TIMEOUT;
		aip_f[i].scale_x[0] 	= node->scale_x;
		aip_f[i].scale_x[1] 	= JZ_AIP_F_IOBASE + AIP_F_SCALE_X;
		aip_f[i].scale_y[0] 	= node->scale_y;
		aip_f[i].scale_y[1] 	= JZ_AIP_F_IOBASE + AIP_F_SCALE_Y;
		aip_f[i].trans_x[0] 	= node->trans_x + 16;
		aip_f[i].trans_x[1] 	= JZ_AIP_F_IOBASE + AIP_F_TRANS_X;
		aip_f[i].trans_y[0] 	= node->trans_y + 16;
		aip_f[i].trans_y[1] 	= JZ_AIP_F_IOBASE + AIP_F_TRANS_Y;
		aip_f[i].src_base_y[0] 	= node->src_base_y;
		aip_f[i].src_base_y[1] 	= JZ_AIP_F_IOBASE + AIP_F_SRC_Y_BASE;
		aip_f[i].src_base_uv[0] = node->src_base_uv;
		aip_f[i].src_base_uv[1] = JZ_AIP_F_IOBASE + AIP_F_SRC_UV_BASE;
		aip_f[i].dst_base_y[0] 	= node->dst_base_y;
		aip_f[i].dst_base_y[1] 	= JZ_AIP_F_IOBASE + AIP_F_DST_Y_BASE;
		aip_f[i].dst_base_uv[0] = node->dst_base_uv;
		aip_f[i].dst_base_uv[1] = JZ_AIP_F_IOBASE + AIP_F_DST_UV_BASE;
		aip_f[i].src_size[0] 	= node->src_height<<16 | node->src_width;
		aip_f[i].src_size[1] 	= JZ_AIP_F_IOBASE + AIP_F_SRC_SIZE;
		aip_f[i].dst_size[0] 	= node->dst_height<<16 | node->dst_width;
		aip_f[i].dst_size[1] 	= JZ_AIP_F_IOBASE + AIP_F_DST_SIZE;
		aip_f[i].stride[0] 	= node->dst_stride<<16 | node->src_stride;
		aip_f[i].stride[1] 	= JZ_AIP_F_IOBASE + AIP_F_STRIDE;
		aip_f[i].f_ctrl[0] 	= 1;
		aip_f[i].f_ctrl[1] 	= JZ_AIP_F_IOBASE + AIP_F_CTRL | 0x1<<31;
	}

	return 0;
}

void aip_p_dump(struct aip_manage *manage)
{
	return;
	printf("AIP_P_CTRL		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_CTRL	));
	printf("AIP_P_IQR		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_IQR		));
	printf("AIP_P_CFG		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_CFG		));
	printf("AIP_P_TIMEOUT		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_TIMEOUT	));
	printf("AIP_P_MODE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_MODE	));
	printf("AIP_P_SRC_Y_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_SRC_Y_BASE	));
	printf("AIP_P_SRC_UV_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_SRC_UV_BASE	));
	printf("AIP_P_SRC_STRIDE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_SRC_STRIDE	));
	printf("AIP_P_DST_BASE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_DST_BASE	));
	printf("AIP_P_DST_STRIDE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_DST_STRIDE	));
	printf("AIP_P_DST_SIZE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_DST_SIZE	));
	printf("AIP_P_SRC_SIZE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_SRC_SIZE	));
	printf("AIP_P_DUMMY_VAL		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_DUMMY_VAL	));
	printf("AIP_P_COEF0		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF0	));
	printf("AIP_P_COEF1		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF1	));
	printf("AIP_P_COEF2		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF2	));
	printf("AIP_P_COEF3		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF3	));
	printf("AIP_P_COEF4		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF4	));
	printf("AIP_P_COEF5		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF5	));
	printf("AIP_P_COEF6		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF6	));
	printf("AIP_P_COEF7		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF7	));
	printf("AIP_P_COEF8		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF8	));
	printf("AIP_P_COEF9		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF9	));
	printf("AIP_P_COEF10		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF10	));
	printf("AIP_P_COEF11		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF11	));
	printf("AIP_P_COEF12		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF12	));
	printf("AIP_P_COEF13		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF13	));
	printf("AIP_P_COEF14		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_COEF14	));
	printf("AIP_P_PARAM0		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_PARAM0	));
	printf("AIP_P_PARAM1		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_PARAM1	));
	printf("AIP_P_PARAM2		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_PARAM2	));
	printf("AIP_P_PARAM3		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_PARAM3	));
	printf("AIP_P_PARAM4		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_PARAM4	));
	printf("AIP_P_PARAM5		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_PARAM5	));
	printf("AIP_P_PARAM6		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_PARAM6	));
	printf("AIP_P_PARAM7		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_PARAM7	));
	printf("AIP_P_PARAM8		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_PARAM8	));
	printf("AIP_P_OFFSET		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_OFFSET	));
	printf("AIP_P_CHAIN_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_CHAIN_BASE	));
	printf("AIP_P_CHAIN_SIZE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_CHAIN_SIZE	));
	printf("AIP_P_ISUM		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_ISUM	));
	printf("AIP_P_OSUM		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_P_OSUM	));

}
void aip_t_dump(struct aip_manage *manage)
{
	return;
	printf("AIP_T_CTRL		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_CTRL	));
	printf("AIP_T_IQR		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_IQR		));
	printf("AIP_T_CFG		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_CFG		));
	printf("AIP_T_TIMEOUT		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_TIMEOUT	));
	printf("AIP_T_TASK_LEN		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_TASK_LEN	));
	printf("AIP_T_SRC_Y_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_SRC_Y_BASE	));
	printf("AIP_T_SRC_UV_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_SRC_UV_BASE	));
	printf("AIP_T_DST_BASE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_DST_BASE	));
	printf("AIP_T_STRIDE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_STRIDE	));
	printf("AIP_T_SRC_SIZE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_SRC_SIZE	));
	printf("AIP_T_PARAM0		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_PARAM0	));
	printf("AIP_T_PARAM1		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_PARAM1	));
	printf("AIP_T_PARAM2		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_PARAM2	));
	printf("AIP_T_PARAM3		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_PARAM3	));
	printf("AIP_T_PARAM4		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_PARAM4	));
	printf("AIP_T_PARAM5		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_PARAM5	));
	printf("AIP_T_PARAM6		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_PARAM6	));
	printf("AIP_T_PARAM7		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_PARAM7	));
	printf("AIP_T_PARAM8		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_PARAM8	));
	printf("AIP_T_OFFSET		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_OFFSET	));
	printf("AIP_T_CHAIN_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_CHAIN_BASE	));
	printf("AIP_T_CHAIN_SIZE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_CHAIN_SIZE	));
	printf("AIP_T_TASL_SKIP_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_TASL_SKIP_BASE));
	printf("AIP_T_ISUM		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_ISUM	));
	printf("AIP_T_OSUM		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_T_OSUM	));

}
void aip_f_dump(struct aip_manage *manage)
{
	return;
	printf("AIP_F_CTRL		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_CTRL	));
	printf("AIP_F_IQR		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_IQR		));
	printf("AIP_F_CFG		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_CFG		));
	printf("AIP_F_TIMEOUT		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_TIMEOUT	));
	printf("AIP_F_SCALE_X		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_SCALE_X	));
	printf("AIP_F_SCALE_Y		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_SCALE_Y	));
	printf("AIP_F_TRANS_X		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_TRANS_X	));
	printf("AIP_F_TRANS_Y		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_TRANS_Y	));
	printf("AIP_F_SRC_Y_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_SRC_Y_BASE	));
	printf("AIP_F_SRC_UV_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_SRC_UV_BASE	));
	printf("AIP_F_DST_Y_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_DST_Y_BASE	));
	printf("AIP_F_DST_UV_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_DST_UV_BASE	));
	printf("AIP_F_SRC_SIZE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_SRC_SIZE	));
	printf("AIP_F_DST_SIZE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_DST_SIZE	));
	printf("AIP_F_STRIDE		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_STRIDE	));
	printf("AIP_F_CHAIN_BASE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_CHAIN_BASE	));
	printf("AIP_F_CHAIN_SIZE	= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_CHAIN_SIZE	));
	printf("AIP_F_ISUM		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_ISUM	));
	printf("AIP_F_OSUM		= 0x%08x\n",jz_aip_readl(manage->aip_io_vaddr,AIP_F_OSUM	));
}
int aip_cfg_ddr_oram(struct aip_manage *manage, unsigned int bus_type, unsigned int reg)
{
	unsigned int value = 0;
	value = jz_aip_readl(manage->aip_io_vaddr, reg);
	value |= bus_type;
	jz_aip_writel(manage->aip_io_vaddr, reg, value);

	return 0;
}

int aip_cfg_timeout(struct aip_manage *manage, unsigned int timeout, unsigned int reg)
{
	jz_aip_writel(manage->aip_io_vaddr, reg, timeout);

	return 0;
}

int aip_p_init(struct aip_p_chainnode *chainnode, int chain_num,
		const uint32_t *nv2rgb_parm, uint32_t cfg)
{
	unsigned int chain_size;
	int ret = 0;

	chain_size = sizeof(struct aip_p_chaindesc) * chain_num;
	
	pthread_mutex_lock(&mutex_p);
	if(manage_p)
		return -2;
	manage_p = (struct aip_manage *)malloc(sizeof(struct aip_manage));
	if(!manage_p)
		return -1;
	memset(manage_p, 0x00, sizeof(struct aip_manage));
	strcpy(manage_p->name, AIP_P_DEV);
	manage_p->chain_num = chain_num;
	manage_p->index = INDEX_AIP_P;
	manage_p->aip_io_base = JZ_AIP_IOBASE;
	manage_p->aip_io_size = JZ_AIP_IOSIZE;
	manage_p->chainbuf.size = chain_size;

	ret = aip_manage_init(manage_p);
	if(ret) {
		printf(" aip_p initialization error\n");
		return -1;
	}

	aip_p_reset();
	aip_p_dump(manage_p);

	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_PARAM0, nv2rgb_parm[0]);
	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_PARAM1, nv2rgb_parm[1]);
	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_PARAM2, nv2rgb_parm[2]);
	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_PARAM3, nv2rgb_parm[3]);
	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_PARAM4, -nv2rgb_parm[4]);//Positive to negative
	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_PARAM5, -nv2rgb_parm[5]);//Positive to negative
	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_PARAM6, nv2rgb_parm[6]);
	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_PARAM7, nv2rgb_parm[7]);
	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_PARAM8, nv2rgb_parm[8]);

	if (chain_num == 1) {
		ret = aip_p_config_nodes(manage_p, chainnode, 0);
		if(ret < 0)
			printf("aip_config_node error\n");
		jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_CFG, cfg | TIME_OUT_EN |0x1<<4 | 0x1 << 2);
	} else if (chain_num > 1) {
		ret = aip_p_config_nodes(manage_p, chainnode, chain_num);
		if(ret < 0)
			printf("aip_config_node error\n");

		jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_CFG, cfg | TIME_OUT_EN | 0x1 << 3 | 0x1 << 2);
		jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_CHAIN_BASE, manage_p->chainbuf.paddr);
		jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_CHAIN_SIZE, chain_size);
	}

	return 0;
}

int aip_p_exit()
{
	if(manage_p)
		aip_manage_deinit(manage_p);
	free(manage_p);
	manage_p = NULL;
	pthread_mutex_unlock(&mutex_p);
	return 0;
}


int aip_t_init(struct aip_t_chainnode *chainnode, int chain_num,
		const uint32_t *nv2rgb_parm, uint32_t offset, uint32_t cfg)
{
	unsigned int chain_size;
	int ret = 0;
	int i;

	pthread_mutex_lock(&mutex_t);
	if(manage_t)
		return -2;
	manage_t = (struct aip_manage *)malloc(sizeof(struct aip_manage));
	if(!manage_t)
		return -1;
	memset(manage_t, 0x00, sizeof(struct aip_manage));

	chain_size = sizeof(struct aip_t_chaindesc) * chain_num;
	strcpy(manage_t->name, AIP_T_DEV);
	manage_t->chain_num = chain_num;
	manage_t->index = INDEX_AIP_T;
	manage_t->aip_io_base = JZ_AIP_IOBASE;
	manage_t->aip_io_size = JZ_AIP_IOSIZE;
	manage_t->chainbuf.size = chain_size;

	ret = aip_manage_init(manage_t);
	if(ret) {
		printf(" aip_t initialization error\n");
		return -1;
	}

	aip_t_reset();
	aip_t_dump(manage_t);

	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_PARAM0, nv2rgb_parm[0]);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_PARAM1, nv2rgb_parm[1]);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_PARAM2, nv2rgb_parm[2]);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_PARAM3, nv2rgb_parm[3]);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_PARAM4, -nv2rgb_parm[4]);//Positive to negative
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_PARAM5, -nv2rgb_parm[5]);//Positive to negative
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_PARAM6, nv2rgb_parm[6]);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_PARAM7, nv2rgb_parm[7]);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_PARAM8, nv2rgb_parm[8]);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_OFFSET, offset);

	ret = aip_t_config_nodes(manage_t, chainnode, 0);
	if(ret < 0)
		printf("aip_config_node error\n");
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_CFG, cfg | TIME_OUT_EN | 0x1<<4|0x1 << 2);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_CTRL, 0x1);

	return 0;
}

int aip_t_exit()
{
	if(manage_t)
		aip_manage_deinit(manage_t);
	free(manage_t);
	manage_t = NULL;
	pthread_mutex_unlock(&mutex_t);
	return 0;
}

int aip_f_init(struct aip_f_chainnode *chainnode, int chain_num , unsigned int cfg)
{
	chain_num ++;
	unsigned int chain_size;
	int ret = 0;
	int i;
	
	pthread_mutex_lock(&mutex_f);
	if(manage_f)
		return -2;
	manage_f = (struct aip_manage *)malloc(sizeof(struct aip_manage));
	if(!manage_f)
		return -1;

	memset(manage_f, 0x00, sizeof(struct aip_manage));

	chain_size = sizeof(struct aip_f_chaindesc) * chain_num;
	strcpy(manage_f->name, AIP_F_DEV);
	manage_f->chain_num = chain_num;
	manage_f->index = INDEX_AIP_F;
	manage_f->aip_io_base = JZ_AIP_IOBASE;
	manage_f->aip_io_size = JZ_AIP_IOSIZE;
	manage_f->chainbuf.size = chain_size;

	ret = aip_manage_init(manage_f);
	if(ret) {
		printf("aip_f initialization error\n");
		return -1;
	}
	aip_f_reset();
	aip_f_dump(manage_f);

	if(chain_num == 2 ) {
		ret = aip_f_config_nodes(manage_f, chainnode, 0, cfg);
		if(ret < 0)
			printf("aip_config_node error\n");

		jz_aip_writel(manage_f->aip_io_vaddr, AIP_F_CFG, cfg | 0x1<<6 |0x1<<4 | 0x1 << 2);
	}else if(chain_num > 2) {
		ret = aip_f_config_nodes(manage_f, chainnode, chain_num, cfg);
		if(ret < 0)
			printf("aip_config_node error\n");

	//	jz_aip_writel(manage_f->aip_io_vaddr, AIP_F_CFG, cfg | 0x1<<6 | 0x1<<4 |0x1<<3 | 0x1 << 2);
		jz_aip_writel(manage_f->aip_io_vaddr, AIP_F_CHAIN_BASE, manage_f->chainbuf.paddr);
		jz_aip_writel(manage_f->aip_io_vaddr, AIP_F_CHAIN_SIZE, chain_size);
	}
	return 0;
}

int aip_f_exit()
{
	if(manage_f)
		aip_manage_deinit(manage_f);
	free(manage_f);
	manage_f = NULL;
	pthread_mutex_unlock(&mutex_f);
	return 0;
}

int aip_p_start(int chain_num)
{
	unsigned int value = 0;

	value = jz_aip_readl(manage_p->aip_io_vaddr, AIP_P_CTRL);

	if (chain_num == 1)
		value |= 0x1<<0;
	else
		value |= 0x1<<2;

	jz_aip_writel(manage_p->aip_io_vaddr, AIP_P_CTRL, value);
	aip_p_dump(manage_p);

	return 0;
}

int aip_t_start(unsigned int task_len, unsigned int skip_dst_base)
{
	unsigned int value = 0;

	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_TASK_LEN, task_len);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_DST_BASE, skip_dst_base);

	value = (0x1<<4);
	jz_aip_writel(manage_t->aip_io_vaddr, AIP_T_CTRL, value);
	aip_t_dump(manage_t);

	return 0;
}

int aip_f_start(int chain_num)
{
	unsigned int value = 0;

	value = jz_aip_readl(manage_f->aip_io_vaddr, AIP_F_CTRL);

	if (chain_num == 1)
		value |= 0x1<<0;
	else
		value |= 0x1<<2;

	jz_aip_writel(manage_f->aip_io_vaddr, AIP_F_CTRL, value);
	aip_f_dump(manage_f);
	return 0;
}

int aip_p_wait()
{
	int status = 0;
	int ret;
	if ((ret = ioctl(manage_p->fd, IOCTL_AIP_IRQ_WAIT_CMP, &status)) < 0) {
		printf("ret = %d, IOCTL_AIP_IRQ_WAIT_CMP failed, status = %d\n",ret, status);
		return ret;
	}

#ifdef TEST_RATE
	uint32_t size = jz_aip_readl(manage_p->aip_io_vaddr, AIP_P_SRC_SIZE);
	uint32_t bw = jz_aip_readl(manage_p->aip_io_vaddr, AIP_P_MODE);
	uint32_t w = size & 0xffff;
	uint32_t h = (size & 0xffff0000) >> 16;
	uint32_t pixel_time = 0xffffffff - jz_aip_readl(manage_p->aip_io_vaddr, AIP_P_TIMEOUT);
	uint32_t rate = 1000000000 /(1920 *1080) * (w * h) / (pixel_time ? pixel_time : 1);
	FILE *fp;
	fp = fopen("/tmp/aip_p.txt", "a+");
	fprintf(fp, "%s\n","AIP_P:");
	fprintf(fp, "%s%d  %s%d\n","width : ",w,"height : ",h);
	fprintf(fp, "%s%d\n","bw: ",bw);
	fprintf(fp, "%s%d\n","cycle_value = ",pixel_time);
	fprintf(fp, "%s%d\n\n","rate_1080p = ",rate);
	printf("rate_1080p = %d\n",rate);
	fclose(fp);
#endif

	return 0;
}

int aip_t_wait()
{
	int status = 0;
	int ret;
	if ((ret = ioctl(manage_t->fd, IOCTL_AIP_IRQ_WAIT_CMP, &status)) < 0) {
		printf("ret = %d, IOCTL_AIP_IRQ_WAIT_CMP failed, status = %d\n",ret, status);
		return ret;
	}

#ifdef TEST_RATE
	uint32_t size = jz_aip_readl(manage_t->aip_io_vaddr, AIP_T_SRC_SIZE);
	uint32_t w = size & 0xffff;
	uint32_t h = (size & 0xffff0000) >> 16;
	uint32_t pixel_time = 0xffffffff - jz_aip_readl(manage_t->aip_io_vaddr, AIP_T_TIMEOUT);
	uint32_t rate = 1000000000 /(1920 *1080) * (w * h) / (pixel_time ? pixel_time : 1);
	FILE *fp;
	fp = fopen("/tmp/aip_t.txt", "a+");
	fprintf(fp, "%s\n","AIP_T:");
	fprintf(fp, "%s%d  %s%d\n","width : ",w,"height : ",h);
	fprintf(fp, "%s%d\n","cycle_value = ",pixel_time);
	fprintf(fp, "%s%d\n\n","rate_1080p = ",rate);
	printf("rate_1080p = %d\n",rate);
	fclose(fp);
#endif

	return 0;
}

int aip_f_wait()
{
	int status = 0;
	int ret;
	if ((ret = ioctl(manage_f->fd, IOCTL_AIP_IRQ_WAIT_CMP, &status)) < 0) {
		printf("ret = %d, IOCTL_AIP_IRQ_WAIT_CMP failed, status = %d\n",ret, status);
		return ret;
	}

#ifdef TEST_RATE
	uint32_t size = jz_aip_readl(manage_f->aip_io_vaddr, AIP_F_SRC_SIZE);
	uint32_t bw = (jz_aip_readl(manage_f->aip_io_vaddr,AIP_F_CFG) & 0x3c0 )>>7;
	uint32_t w = size & 0xffff;
	uint32_t h = (size & 0xffff0000) >> 16;
	uint32_t pixel_time = 0xffffffff - jz_aip_readl(manage_f->aip_io_vaddr, AIP_F_TIMEOUT);
	uint32_t rate = 1000000000 /(1920 *1080) * (w * h) / (pixel_time ? pixel_time : 1);
	uint32_t fum_rate;
	if(bw > 2 && bw < 7)
		fum_rate = 1000000000 /(256 *16 *16 * 512 / 32) * (w * h) / (pixel_time ? pixel_time : 1);
	FILE *fp;
	fp = fopen("/tmp/aip_f.txt", "a+");
	fprintf(fp, "%s\n","AIP_F:");
	fprintf(fp, "%s%d  %s%d\n","width : ",w,"height : ",h);
	fprintf(fp, "%s%d\n","bw:",bw);
	fprintf(fp, "%s%d\n","cycle_value = ",pixel_time);
	fprintf(fp, "%s%d\n\n","rate_1080p = ",rate);
	printf("rate_1080p = %d\n",rate);
	if(bw > 2 && bw < 7) {
		fprintf(fp, "%s%d\n\n","rate_256p = ",fum_rate);
		printf("rate_256p = %d\n",fum_rate);
	}
	fclose(fp);
#endif

	return 0;
}


