#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "test_rsa.h"
#define RSA1024 1
#define RSA2048 2
#define uint32_t unsigned int
#define BLSWAP32(val)													\
    ((uint32_t)((((uint32_t)(val) & (uint32_t)0x000000ffU) << 24) |		\
				(((uint32_t)(val) & (uint32_t)0x0000ff00U) <<	8) |	\
				(((uint32_t)(val) & (uint32_t)0x00ff0000U) >>	8) |	\
				(((uint32_t)(val) & (uint32_t)0xff000000U) >> 24)))
/*
 *rsa 非对称加密模块:
 *目前支持私钥加密,公钥解密,秘钥mod n可为1024bit或2048bit;
 *加密数据长度为1024bit或2048bit不支持其他长度,不足可补齐;
 *测试用例中的test_*_g,test_*_m,test_*_d,test_*_n,test_*_e中的test_*_d,test_*_n,test_*_e为从openssl生成的秘钥中提取的数据,(e,n)组成公钥对用于解密m生成g,(d,n)组成私钥对用于加密g生成m
 */
int main(int argc,const char *argv[])
{
	rsa_data_t data;
	int i = 0;
	int j = 0;
	int pass = 0;
	int failed = 0;
	int ret = 0;
	int mode = 0;
	int buf_len = 0;
	struct timeval tv;
	long time_use = 0;

	if(argc != 2){
		printf("###############################\n");
		printf("./test_rsa [RSA_MODE]\n");
		printf("###############################\n");
		return 0;
	}

	const char *_mode = argv[1];
	if(0 == strcmp(_mode,"RSA1024")){
		mode = RSA1024;
	}else if(0 == strcmp(_mode,"RSA2048")){
		mode = RSA2048;
	}else{
		printf("###############################\n");
		printf("[RSA_MODE]:\n\tRSA1024\n\tRSA2048\t\n");
		printf("###############################\n");
		return 0;
	}

	printf("mode = %d\n",mode);
	if(mode == RSA1024)
		buf_len = 32;
	else
		buf_len = 64;
	unsigned int *out_buf = (unsigned int *)malloc(buf_len * sizeof(unsigned int));
	if(!out_buf)
		return 0;
	unsigned int *cipher = (unsigned int *)malloc(buf_len * sizeof(unsigned int));
	if(!cipher)
		return 0;
	int fd= open("/dev/rsa",0);
	if(fd < 0){
		printf("Failed to open /dev/rsa\n");
		return 0;
	}

	for(j = 0;j < TEST_NUM;j++){
		memset(cipher,0,buf_len);
		memset(&data,0,sizeof(rsa_data_t));
		ret = 0;
		if(mode == RSA1024){
			data.e_or_d = test_1024_e;
			data.n = test_1024_n[j];
			data.rsa_mode = 1024;
			data.input = test_1024_m[j];
			data.inlen = 128;
		}else{
			data.e_or_d = test_2048_e;
			data.n = test_2048_n[j];
			data.rsa_mode = 2048;
			data.input = test_2048_m[j];
			data.inlen = 256;
		}
		data.output = cipher;
		printf("Dec begin-------------------------------------------------------------------------\n");
		gettimeofday(&tv,NULL);
		time_use = tv.tv_sec*1000000+tv.tv_usec;

		ret = ioctl(fd,IOCTL_RSA_ENC_DEC,&data);
		if(ret){
			printf("rsa enc process error\n");
		}
		gettimeofday(&tv,NULL);
		time_use = tv.tv_sec*1000000+tv.tv_usec-time_use;
		printf("time_use of Dec = %f ms\n",(double)time_use/1000);
		printf("Dec end---------------------------------------------------------------------------\n");
		for(i = 0; i < buf_len; i++){
			if(mode == RSA2048){
				if(BLSWAP32(cipher[i]) != test_2048_g[j][i]){
					printf("#### Failed: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i,cipher[i],i,test_2048_g[j][i]);
					ret = -1;
				}else{
					printf("#### Success: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i,cipher[i],i,test_2048_g[j][i]);
				}
			}else{
				if(BLSWAP32(cipher[i]) != test_1024_g[j][i]){
					printf("#### Failed: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i,cipher[i],i,test_1024_g[j][i]);
					ret = -1;
				}else{
					printf("#### Success: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i,cipher[i],i,test_1024_g[j][i]);
				}
			}
		}
		ret = 0;
		memset(&data,0,sizeof(rsa_data_t));

		if(mode == RSA1024){
			data.e_or_d = test_1024_d[j];
			data.n = test_1024_n[j];
			data.rsa_mode = 1024;
			data.input = test_1024_g[j];
			data.inlen = 128;
		}else{
			data.e_or_d = test_2048_d[j];
			data.n = test_2048_n[j];
			data.rsa_mode = 2048;
			data.input = test_2048_g[j];
			data.inlen = 256;
		}
		data.output = out_buf;
		printf("Enc begin-------------------------------------------------------------------------\n");
		gettimeofday(&tv,NULL);
		time_use = tv.tv_sec*1000000+tv.tv_usec;

		ret = ioctl(fd,IOCTL_RSA_ENC_DEC,&data);
		if(ret){
			printf("rsa dec process error\n");
		}
		gettimeofday(&tv,NULL);
		time_use = tv.tv_sec*1000000+tv.tv_usec-time_use;
		printf("time_use of Dec = %f ms\n",(double)time_use/1000);
		printf("Enc end---------------------------------------------------------------------------\n");
		for(i = 0; i < buf_len; i++){
			if(mode == RSA2048){
				if(BLSWAP32(out_buf[i]) != test_2048_m[j][i]){
					printf("#### Failed: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i,out_buf[i],i,test_2048_m[j][i]);
					ret = -1;
				}else{
					printf("#### Success: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i,out_buf[i],i,test_2048_m[j][i]);
				}
			}else{
				if(BLSWAP32(out_buf[i]) != test_1024_m[j][i]){
					printf("#### Failed: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i,out_buf[i],i,test_1024_m[j][i]);
					ret = -1;
				}else{
					printf("#### Success: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i,out_buf[i],i,test_1024_m[j][i]);
				}
			}
		}
		if(ret)
			failed++;
		else
			pass++;
		printf("-----------------RSA---------------------\n");
		printf("RSA test %d done\n",j);
		printf("-----------------RSA---------------------\n");
	}
	printf("RSA 2048 Test done,pass=%d failed=%d\n",pass,failed);
	close(fd);
	return 0;
}
