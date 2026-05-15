#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>


#include "../common/sample-common.h"

#define NV12_TEST_FILE 		"640x360.nv12"
#define ARGB1555_TEST_FILE 	"640x360.argb1555"
#define ROTATE_PATH			"/dev/rotate"
#define FB0DEV 				"/dev/fb0"
#define CANVAS_WIDTH 		(1080)
#define CANVAS_HEIGHT 		(1920)
#define CANVAS_BPP 			(16)
#define DISPLAY_WIDTH 		(1920)
#define DISPLAY_HEIGHT 		(1080)
#define DISPLAY_BPP 		(16)
#define DISPLAY_INTFSYNC	(VO_OUTPUT_1080P60)


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


typedef struct MemBuffer
{
	char  		 s8PoolName[16];
	unsigned int u32BufferPool;
	unsigned int u32PoolSize;
} MemBuffer_st;
MemBuffer_st g_stMemBuffer;


#define BUF_SIZE (1920*1080*2)



#define JZ_ROTATE_MAGIC			'R'
#define IOCTL_ROTATE_SET_SCALER_TABLE	_IOW(JZ_ROTATE_MAGIC, 4, struct jz_rotate_info)





static int load_picture(void *addr, char *filename)
{
	int ret;
	int32_t read_fd = 0;
	int32_t size = 0;

	read_fd = open(filename, O_RDONLY);
	if(read_fd < 0) {
		printf("open test file(%s) Failed\n", filename);
		goto err_open;
	}

	size = lseek(read_fd, 0, SEEK_END);

	lseek(read_fd, 0, SEEK_SET);
	ret = read(read_fd, addr, size);
	if(ret < 0) {
		printf("read %s Failed\n", filename);
		goto err_read;
	}
	return 0;

err_read:
err_open:
	return -1;
}


static int save_pic(void * addr,int count)
{
	int ret = 0;
	int32_t pic = 0;
	char *file_buf = NULL;
	char pic_name[128];
	sprintf(pic_name,"/mnt/test/rotate90_%d.nv12", count);

	pic = open(pic_name, O_CREAT|O_RDWR);
	if (pic < 0) {
		printf("Failed to open file %s\n",pic_name);
		goto err_filp_open;

	}

	ret = write(pic, addr, 1920*1080*2);
	if(ret != 1920*1080*2) {
		printf("[%s:%d] vfs_read %s Failed!! read_len=%d,file_len=%d\n",__func__,__LINE__,pic_name, ret, 1920*1080*2);
		goto err_vfs_read;
	}

	printf("save  %s succ\n",pic_name);


err_vfs_read:
err_vfs_stat:
	close(pic);
err_filp_open:


	return ret;

}


int main(int argc, const char* argv[])
{
	int s32Ret = 0;
	void *src = NULL;
	void *des = NULL;
	void *rotate = NULL;
	struct jz_rotate_info rotate_info;
	MemBuffer_st *pstMemBuffer = &g_stMemBuffer;
	IMP_S32 fb_fd = 0;
	struct fb_fix_screeninfo fix_info;
	struct fb_var_screeninfo var_info;
	IMP_S32 i;
	IMP_BOOL bShow = IMP_FALSE;
	struct timeval time_began, time_end;

	IMP_S32 s32VoMod;
	VO_PUB_ATTR_S stVoPubAttr;
	int j = 1;

	memset(&stVoPubAttr, 0, sizeof(VO_PUB_ATTR_S));

	/* system init */
	s32Ret = sample_system_init();
	if (s32Ret != 0) {
		printf("sample_system_init failed! ret = 0x%08x\n", s32Ret);
		return s32Ret;
	}


	/* vo init */
	s32VoMod = SAMPLE_VO_MODULE;
	IMP_VO_Disable(s32VoMod);
	stVoPubAttr.stCanvasSize.u32Width = DISPLAY_WIDTH;
	stVoPubAttr.stCanvasSize.u32Height = DISPLAY_HEIGHT;
	stVoPubAttr.enIntfSync = DISPLAY_INTFSYNC;
	stVoPubAttr.enIntfType = VO_INTF_HDMI;
	s32Ret = sample_vo_start_module(s32VoMod, &stVoPubAttr);
	if (s32Ret != IMP_SUCCESS) {
		printf("sample_vo_start_module failed! ret = 0x%08x\n", s32Ret);
	}

	s32Ret = sample_vo_hdmi_start(stVoPubAttr.enIntfSync);
	if (s32Ret != IMP_SUCCESS) {
		printf("sample_vo_hdmi_start failed! ret = 0x%08x\n", s32Ret);
	}


	int rotate_fd = open(ROTATE_PATH, O_RDWR);
	if (rotate_fd < 0) {
		perror("FB0DEV open error!\n");
		return -1;
	}

	fb_fd = open(FB0DEV, O_RDWR);
	if (fb_fd < 0) {
		perror("FB0DEV open error!\n");
		return -1;
	}

	sprintf(pstMemBuffer->s8PoolName, "tde-bufferpool");
	pstMemBuffer->u32PoolSize = 2 * BUF_SIZE;
	s32Ret = IMP_System_CreatPool(&pstMemBuffer->u32BufferPool, pstMemBuffer->u32PoolSize,
			1, pstMemBuffer->s8PoolName);
	if (s32Ret != 0) {
		printf("IMP_buf_createpool failed! ret = 0x%08x\n", s32Ret);
	}

	src= IMP_System_GetBlock(pstMemBuffer->u32BufferPool, NULL, BUF_SIZE, "buffer");
		if(!src){
			printf("IMP_System_GetBlock is Failed!\n");
		}
	memset(src, 0x00, BUF_SIZE);


	des = IMP_System_GetBlock(pstMemBuffer->u32BufferPool, NULL, BUF_SIZE, "buffer");
	if(!des){
		printf("IMP_System_GetBlock is Failed!\n");
	}
	memset(des, 0x00, BUF_SIZE);


	/*FB*/
	/* get framebuffer's var_info */
	if ((s32Ret = ioctl(fb_fd, FBIOGET_VSCREENINFO, &var_info)) < 0) {
		printf("FBIOGET_VSCREENINFO failed");
	}

	//ARGB1555
	//var_info.activate = FB_ACTIVATE_NOW;
	var_info.activate = FB_ACTIVATE_FORCE;
	var_info.xres = DISPLAY_WIDTH;
	var_info.yres = DISPLAY_HEIGHT;
	var_info.bits_per_pixel = 16;
	//A
	var_info.transp.offset = 15;
	var_info.transp.length = 1;
	var_info.transp.msb_right = 0;
	//R
	var_info.red.offset = 10;
	var_info.red.length = 5;
	var_info.red.msb_right = 0;
	//G
	var_info.green.offset = 5;
	var_info.green.length = 5;
	var_info.green.msb_right = 0;
	//B
	var_info.blue.offset = 0;
	var_info.blue.length = 5;
	var_info.blue.msb_right = 0;

	/* put framebuffer's var_info */
	if ((s32Ret = ioctl(fb_fd, FBIOPUT_VSCREENINFO, &var_info)) < 0) {
		printf("FBIOPUT_VSCREENINFO failed");
	}

	if ((s32Ret = ioctl(fb_fd, FBIOGET_VSCREENINFO, &var_info)) < 0) {
		printf("FBIOGET_VSCREENINFO failed");
	}

	/* get framebuffer's fix_info */
	if ((s32Ret = ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix_info)) < 0) {
		printf("FBIOGET_FSCREENINFO failed");
	}
	printf("[xres,yres]=[%d,%d]\n",var_info.xres,var_info.yres);
	printf("[xres_virtual,yres_virtual]=[%d,%d]\n",var_info.xres_virtual,var_info.yres_virtual);
	printf("[bits_per_pixel]=[%d]\n",var_info.bits_per_pixel);

	rotate = mmap(0, fix_info.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if(!rotate) {
		printf("Map failed");
	}


	bShow = IMP_TRUE;
	if ((s32Ret = ioctl(fb_fd, FBIOPUT_SHOW, &bShow)) < 0) {
		printf("FBIOPUT_SHOW failed");
	}

	/*画图*/
	load_picture(src, "1920x1080.argb1555");
	//load_picture(src, "1920_1080.nv12");

	while(1)
	{
		j++;
		if(j > 0x7FFFFFFF){
			j = 1;
		}
		//load_picture(src, "1920x1080.argb1555");

		/*开始旋转*/
		rotate_info.index = j;
		rotate_info.des = (IMP_U32)IMP_System_Block2PhyAddr(des);
		rotate_info.src = (IMP_U32)IMP_System_Block2PhyAddr(src);
		rotate_info.width = 1920;
		rotate_info.height = 1080;
		rotate_info.type = ARGB1555;
		rotate_info.box_width = 32;
		rotate_info.boxh_height = 128;
		if(j%2 == 0){
			rotate_info.angle = ROTATE0;
		}else{
			rotate_info.angle = ROTATE180;
		}

		//printf("rotate angle:%d\n",rotate_info.angle);
		gettimeofday(&time_began,NULL);
		//memset(des,0,fix_info.smem_len);

		if ((s32Ret = ioctl(rotate_fd, IOCTL_ROTATE_SET_SCALER_TABLE, &rotate_info)) < 0) {
			perror("IOCTL_ROTATE_SET_SCALER_TABLE failed");
		}
		gettimeofday(&time_end,NULL);

		printf("spend time s:%ld  ms:%ld\n",time_end.tv_sec -time_began.tv_sec,(time_end.tv_usec -time_began.tv_usec)/1000);
		//sleep(1);
		//system("sync");

		/*fb更新显示*/

		memcpy(rotate,des,fix_info.smem_len);
		if ((s32Ret = ioctl(fb_fd,FBIOPAN_DISPLAY, &var_info)) < 0)
		{
			perror("FBIOPAN_DISPLAY failed");
		}


		//save_pic(des,i);


	}

	munmap(rotate, fix_info.smem_len);
	IMP_System_ReleaseBlock(pstMemBuffer->u32BufferPool, src);
	IMP_System_ReleaseBlock(pstMemBuffer->u32BufferPool, des);

	close(fb_fd);

	close(rotate_fd);

	IMP_System_DestroyPool(pstMemBuffer->u32BufferPool, pstMemBuffer->s8PoolName);

	return 0;
}
