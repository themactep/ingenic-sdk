#include <rotate.h>

#if 1


char* rotateaddr	= (void*)0xE400000;
char* buf1			= NULL;
char* buf2			= NULL;
int   W 			= 1920;
int   H  			= 1080;
int   BOX_W			= 64;
int   BOX_H  		= 64;


#else
static char* buf1	= (void*)0x6400000;
static char* buf2	= (void*)0x6900000;
#endif


int  get_rotate_info( struct jz_rotate_info *rotate_info)
{
	int index = get_flag();

	if(rotate_info == NULL){
		return -1;
	}

	memcpy(rotate_info, rotateaddr, sizeof(struct jz_rotate_info));
	if(rotate_info->index != index){
		printf("get_rotate_info fail recv index:%d get index:%d \n",index, rotate_info->index);
		return -1;
	}

	buf1 = (char*)rotate_info->src;
	buf2 = (char*)rotate_info->des;
	W	 = rotate_info->width;
	H    = rotate_info->height;
	BOX_W= rotate_info->box_width;
	BOX_H= rotate_info->boxh_height;

/*
	printf("recv info: \n \r weight:%d \n\r height:%d \n\r index:%d \n\r src:0x%08x \n\r des:0x%08x \n\r type:%d \n\r  angle:%d \n\r ",\
	rotate_info->width,\
	rotate_info->height,\
	rotate_info->index,\
	rotate_info->src,\
	rotate_info->des,\
	rotate_info->type,\
	rotate_info->angle);
*/
	return 0;
}



/*获取box的坐标和尺寸*/
int  get_box_info( int width,int height,int index,int *x,int *y,int *w,int *h,int statd_w,int statd_h)
{
	int line_count = 0,column_count = 0;
	int box_count =0;

	if(width < 0 || height < 0 || index < 0){
		return -1;
	}

	line_count = DIV_ROUND_UP(height, statd_h);
	column_count = DIV_ROUND_UP(width, statd_w);
	box_count = line_count * column_count;

	if(index >box_count){
		printf("windows info index :%d mux:%d \n",index,box_count);
		return -1;
	}

	int y_num = index/column_count;
	int x_num = index%column_count;

	*x = (x_num)*statd_w;
	*y = (y_num)*statd_h;
	*h = statd_h;
	*w = statd_w;

	if((*y + *h) > height)
	{
		*h = height - *y;
	}

	if((*x + *w) > width)
	{
		*w = width - *x;
	}

	return 0;
}

/*nv12格式 顺时针旋转90度*/
int nv12rotate90(void)
{
	int box_width = 0,box_height = 0,box_x = 0,box_y = 0;
	int box_num = DIV_ROUND_UP(W, BOX_W)*DIV_ROUND_UP(H, BOX_H);
	int i = 0,j = 0,n = 0;

	char* src_uv  = (char*)buf1 + W*H;
	char* des_uv = (char*)buf2 + W*H;
	char* src_y = buf1;
	char* des_y = buf2;
	int offset = 0;
	/*Y*/
	for(n = 0; n < box_num; n++){
		get_box_info(W, H, n, &box_x, &box_y, &box_width, &box_height,BOX_W,BOX_H);
		src_y = buf1 + (box_y*W) + box_x;
		des_y = buf2 + (H*box_x) + H -1 -box_y;
		for(i = 0; i < box_height; i++){
			offset = (i*W);
			for(j = 0; j < box_width; j++){
				*(des_y + (j*H) - i) = *(src_y + j + offset);
			}
		}
	}

	/*UV*/
	int halfWidth  = W / 2;
	int halfHeight = H / 2;

	box_num = DIV_ROUND_UP(W, BOX_W)*DIV_ROUND_UP(H/2, BOX_H);
	for(n = 0; n < box_num; n++){
		get_box_info(W/2, H/2, n, &box_x, &box_y, &box_width, &box_height,BOX_W/2,BOX_H);
		char* src_offfset = src_uv + (box_y *halfWidth + box_x)*2 ;
		char* des_offfset = des_uv + (box_x*halfHeight + halfHeight -box_y - 1)*2;
		for(i = 0; i < box_height; i++){
			for(j = 0; j < box_width; j++){
				*(des_offfset + (j * halfHeight - i)*2)     = *(src_offfset + (halfWidth * i + j)*2);//U
				*(des_offfset + (j * halfHeight - i)*2 + 1) = *(src_offfset + (halfWidth * i + j)*2 + 1);//V
			}
		}
	}
	return 0;
}

/*nv12格式 顺时针旋转180度*/
int nv12rotate180(void)
{

	int box_width = 0,box_height = 0,box_x = 0,box_y = 0;
	int box_num = DIV_ROUND_UP(W, BOX_W)*DIV_ROUND_UP(H, BOX_H);
	int i = 0,j = 0,n = 0;

	char* src_uv = (char*)buf1 + W*H;
	char* des_uv = (char*)buf2 + W*H;
	char* src_y  = buf1;
	char* des_y  = buf2;
	int offset = 0;

	/*Y*/
	//(x,y) -> (W-x-1,H-y-1)
	for(n = 0; n < box_num; n++){
		get_box_info(W, H, n, &box_x, &box_y, &box_width, &box_height,BOX_W,BOX_H);
		src_y = buf1 + (box_y*W) + box_x;
		des_y = buf2 + W*(H - box_y - 1) + W -1 -box_x;
		for(i = 0; i < box_height; i++){
			offset = (i*W);
			for(j = 0; j < box_width; j++){
				*(des_y - (i*W) - j) = *(src_y + j + offset);
			}
		}
	}

	/*UV*/
	int halfWidth  = W / 2;
	int halfHeight = H / 2;

	//(x,y) -> (halfWidth-x-1,halfHeight-y-1)
	box_num = DIV_ROUND_UP(W, BOX_W) * DIV_ROUND_UP(H/2, BOX_H);
	for(n = 0; n < box_num; n++){
		get_box_info(W/2, H/2, n, &box_x, &box_y, &box_width, &box_height,BOX_W/2,BOX_H);
		char* src_offfset = src_uv + (box_y *halfWidth + box_x)*2 ;
		char* des_offfset = des_uv + (((halfHeight - box_y - 1) * halfWidth) + halfWidth - box_x - 1)*2;
		for(i = 0; i < box_height; i++){
			for(j = 0; j < box_width; j++){
				*(des_offfset - (i * halfWidth + j)*2)     = *(src_offfset + (halfWidth * i + j)*2);//U
				*(des_offfset - (i * halfWidth + j)*2 + 1) = *(src_offfset + (halfWidth * i + j)*2 + 1);//V
			}
		}
	}


	return 0;
}

/*nv12格式 顺时针旋转270度*/
int nv12rotate270(void)
{
	int box_width = 0,box_height = 0,box_x = 0,box_y = 0;
	int box_num = DIV_ROUND_UP(W, BOX_W)*DIV_ROUND_UP(H, BOX_H);
	int i = 0,j = 0,n = 0;
	//W:10  H:24
	char* src_uv  = (char*)buf1 + W*H;
	char* des_uv = (char*)buf2 + W*H;
	char* src_y = buf1;
	char* des_y = buf2;
	int offset = 0;

	//Y
	//(box_x,box_y) -> (box_y, W - 1 -box_x)
	for(n = 0; n < box_num; n++){
		get_box_info(W, H, n, &box_x, &box_y, &box_width, &box_height,BOX_W,BOX_H);
		src_y = buf1 + (box_y*W) + box_x;
		des_y = buf2 + (W - 1 -box_x)*H + box_y;
		for(i = 0; i < box_height; i++){
			offset = (i*W);
			for(j = 0; j < box_width; j++){
				*(des_y - (j*H) + i) = *(src_y + j + offset);
				//*(des_y + (j*H) - i) = *(src_y + j + offset);
			}
		}
	}

		/*UV*/
	int halfWidth  = W / 2;
	int halfHeight = H / 2;

	//(box_x,box_y) -> (box_y, halfHeight - 1 -box_x)
	box_num = DIV_ROUND_UP(W, BOX_W) * DIV_ROUND_UP(H/2, BOX_H);
	for(n = 0; n < box_num; n++){
		get_box_info(W/2, H/2, n, &box_x, &box_y, &box_width, &box_height,BOX_W/2,BOX_H);
		char* src_offfset = src_uv + (box_y *halfWidth + box_x)*2 ;
		char* des_offfset = des_uv + ((halfWidth - 1 - box_x) * halfHeight + box_y)*2;
		for(i = 0; i < box_height; i++){
			for(j = 0; j < box_width; j++){
				*(des_offfset - (j * halfHeight - i)*2)     = *(src_offfset + (halfWidth * i + j)*2);//U
				*(des_offfset - (j * halfHeight - i)*2 + 1) = *(src_offfset + (halfWidth * i + j)*2 + 1);//V
			}
		}
	}

	return 0;
}


/*ARGB1555格式 顺时针旋转90度*/

int argb1555rotate90(void)
{

	int box_width = 0, box_height = 0, box_x = 0, box_y = 0;
	int i 		  = 0, j   		  = 0, 	   n = 0;
	int box_num   = DIV_ROUND_UP(W, BOX_W) * DIV_ROUND_UP(H, BOX_H);
	short * src   = (short *)buf1;
	short * des   = (short *)buf2;
	int offset    = 0;

	for(n = 0; n < box_num; n++){
		get_box_info(W, H, n, &box_x, &box_y, &box_width, &box_height,BOX_W,BOX_H);
		src = (short *)buf1 + (box_y*W) + box_x;
		des = (short *)buf2 + (H*box_x) + H -1 -box_y;
		for(i = 0; i < box_height; i++){
			offset = (i*W);
			for(j = 0; j < box_width; j++){
				*(des + (j*H) - i) = *(src + j + offset);
			}
		}
	}


	return 0;
}

/*nv12格式 顺时针旋转180度*/
int nv12rotateinline180(void)
{


	int n = 0;

	short* src_uv = (short*)(buf1 + W*H);
	short* des_uv = (short*)(buf2 + W*H);
	char* src_y  = buf1;
	char* des_y  = buf2;

	/*Y*/
	//(x,y) -> (W-x-1,H-y-1)
	for(n = 0; n < W * H; n++){
		des_y[ W * H -1 -n] = src_y[n];
	}

	/*UV*/
	int halfWidth  = W / 2;
	int halfHeight = H / 2;

	for(n = 0; n < halfWidth * halfHeight; n++){
		des_uv[ halfWidth * halfHeight -1 -n] = src_uv[n];
	}


	return 0;
}


int argb1555rotateinline180(void)
{

	int i 		  = 0;
	short * src   = (short *)buf1;
	short * des   = (short *)buf2;

	for(i = 0; i < W * H; i++){
		des[i] = src[W * H - i - 1];
	}

	return 0;
}

int argb8888rotateinline180(void)
{

	int i 		  = 0;
	int * src   = (int *)buf1;
	int * des   = (int *)buf2;

	for(i = 0; i < W * H; i++){
		des[i] = src[W * H - i - 1];
	}

	return 0;
}



int argb1555rotate180(void)
{
	int box_width = 0, box_height = 0, box_x = 0, box_y = 0;
	int i 		  = 0, j   		  = 0, 	   n = 0;
	int box_num   = DIV_ROUND_UP(W, BOX_W) * DIV_ROUND_UP(H, BOX_H);
	short * src   = (short *)buf1;
	short * des   = (short *)buf2;
	int offset    = 0;


	/*Y*/
	//(x,y) -> (W-x-1,H-y-1)
	for(n = 0; n < box_num; n++){
		get_box_info(W, H, n, &box_x, &box_y, &box_width, &box_height,BOX_W,BOX_H);
		src = (short *)buf1 + (box_y*W) + box_x;
		des = (short *)buf2 + W*(H - box_y - 1) + W -1 -box_x;
		for(i = 0; i < box_height; i++){
			offset = (i*W);
			for(j = 0; j < box_width; j++){
				*(des - (i*W) - j) = *(src + j + offset);
			}
		}
	}


	return 0;
}


int argb1555rotate270(void)
{
	int box_width = 0, box_height = 0, box_x = 0, box_y = 0;
	int i 		  = 0, j   		  = 0, 	   n = 0;
	int box_num   = DIV_ROUND_UP(W, BOX_W) * DIV_ROUND_UP(H, BOX_H);
	short * src   = (short *)buf1;
	short * des   = (short *)buf2;
	int offset    = 0;

	//Y
	//(box_x,box_y) -> (box_y, W - 1 -box_x)
	for(n = 0; n < box_num; n++){
		get_box_info(W, H, n, &box_x, &box_y, &box_width, &box_height,BOX_W,BOX_H);
		src = (short *)buf1 + (box_y*W) + box_x;
		des = (short *)buf2 + (W - 1 -box_x)*H + box_y;
		for(i = 0; i < box_height; i++){
			offset = (i*W);
			for(j = 0; j < box_width; j++){
				*(des - (j*H) + i) = *(src + j + offset);
			}
		}
	}


	return 0;
}



/*ARGB1555格式 顺时针旋转90度*/

int argb8888rotate90(void)
{

	int box_width = 0, box_height = 0, box_x = 0, box_y = 0;
	int i 		  = 0, j   		  = 0, 	   n = 0;
	int box_num   = DIV_ROUND_UP(W, BOX_W) * DIV_ROUND_UP(H, BOX_H);
	int * src   = (int *)buf1;
	int * des   = (int *)buf2;
	int offset    = 0;

	for(n = 0; n < box_num; n++){
		get_box_info(W, H, n, &box_x, &box_y, &box_width, &box_height,BOX_W,BOX_H);
		src = (int *)buf1 + (box_y*W) + box_x;
		des = (int *)buf2 + (H*box_x) + H -1 -box_y;
		for(i = 0; i < box_height; i++){
			offset = (i*W);
			for(j = 0; j < box_width; j++){
				*(des + (j*H) - i) = *(src + j + offset);
			}
		}
	}


	return 0;
}


int argb8888rotate180(void)
{
	int box_width = 0, box_height = 0, box_x = 0, box_y = 0;
	int i 		  = 0, j   		  = 0, 	   n = 0;
	int box_num   = DIV_ROUND_UP(W, BOX_W) * DIV_ROUND_UP(H, BOX_H);
	int * src   = (int *)buf1;
	int * des   = (int *)buf2;
	int offset    = 0;


	/*Y*/
	//(x,y) -> (W-x-1,H-y-1)
	for(n = 0; n < box_num; n++){
		get_box_info(W, H, n, &box_x, &box_y, &box_width, &box_height,BOX_W,BOX_H);
		src = (int *)buf1 + (box_y*W) + box_x;
		des = (int *)buf2 + W*(H - box_y - 1) + W -1 -box_x;
		for(i = 0; i < box_height; i++){
			offset = (i*W);
			for(j = 0; j < box_width; j++){
				*(des - (i*W) - j) = *(src + j + offset);
			}
		}
	}


	return 0;
}


int argb8888rotate270(void)
{
	int box_width = 0, box_height = 0, box_x = 0, box_y = 0;
	int i 		  = 0, j   		  = 0, 	   n = 0;
	int box_num   = DIV_ROUND_UP(W, BOX_W) * DIV_ROUND_UP(H, BOX_H);
	int * src   = (int *)buf1;
	int * des   = (int *)buf2;
	int offset    = 0;

	//Y
	//(box_x,box_y) -> (box_y, W - 1 -box_x)
	for(n = 0; n < box_num; n++){
		get_box_info(W, H, n, &box_x, &box_y, &box_width, &box_height,BOX_W,BOX_H);
		src = (int *)buf1 + (box_y*W) + box_x;
		des = (int *)buf2 + (W - 1 -box_x)*H + box_y;
		for(i = 0; i < box_height; i++){
			offset = (i*W);
			for(j = 0; j < box_width; j++){
				*(des - (j*H) + i) = *(src + j + offset);
			}
		}
	}


	return 0;
}


int rotateRGB90(unsigned char *src, unsigned char *dst, int width, int height, int bpp)
{

	int dstIndex = 0;
	int srcIndex = 0;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
		//(y, x) -> (x, height - y - 1)
		//y * width + x, -> x* height + height - y - 1
			dstIndex = (x * height + height - y - 1) * bpp;
			for (int i = 0; i < bpp; i++) {
				dst[dstIndex + i] = src[srcIndex++];
			}
		}
	}
	return 0;
}


int rotateRGB180(unsigned char *src, unsigned char *dst, int width, int height, int bpp)
{
	int dstIndex = 0;
	int srcIndex = 0;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			//(y, x) -> (height - y - 1, width - x - 1)
			//y * width + x, -> (height - y - 1) * width + width - x - 1
			dstIndex = ((height - y - 1) * width + width - x - 1) * bpp;
			for (int i = 0; i < bpp; i++) {
				dst[dstIndex + i] = src[srcIndex++];
			}
		}
	}

	return 0;
}


int rotateRGB270(unsigned char *src, unsigned char *dst, int width, int height, int bpp)
{
	int dstIndex = 0;
	int srcIndex = 0;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
		//(y, x) -> (width - x - 1, y)
			//y * width + x, -> (width - x - 1) * height + y
			dstIndex = ((width - x - 1) * height + y) * bpp;
			for (int i = 0; i < bpp; i++) {
				dst[dstIndex + i] = src[srcIndex++];
			}
		}
	}

	return 0;
}

int rotate360(struct jz_rotate_info rotate_info)
{
	if(rotate_info.type == ARGB1555){
		memcpy(buf2, buf1, rotate_info.width * rotate_info.height * 2);
	}else if(rotate_info.type == ARGB8888){
		memcpy(buf2, buf1, rotate_info.width * rotate_info.height * 4);
	}else{
		memcpy(buf2, buf1, rotate_info.width * rotate_info.height * 3/2);
	}

	return 0;
}



