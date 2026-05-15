#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "test_rsa.h"

int main(int argc,const char *argv[])
{
	rsa_data_t data;
	int i = 0;
	int j = 0;
	int pass = 0;
	int ret = 0;
	int buf_len = 64;

	unsigned int *out_buf = (unsigned int *)malloc(buf_len * sizeof(unsigned int));
	if(!out_buf) {
		return 0;
	}
	unsigned int *cipher = (unsigned int *)malloc(buf_len * sizeof(unsigned int));
	if(!cipher) {
		return 0;
	}
	int fd= open("/dev/rsa",0);
	if(fd < 0){
		printf("Failed to open /dev/rsa\n");
		return 0;
	}
	
	for(j = 0;j < TEST_NUM;j++){
		memset(cipher,0,buf_len);
		memset(&data,0,sizeof(rsa_data_t));
		ret = 0;
		
		data.e_or_d = test_2048_e[j];
		data.n = test_2048_n[j];
		data.rsa_mode = 2048;
		data.input = test_2048_m[j];
		data.mode = RSA_ENC;
		
		data.output = cipher;
		ret = ioctl(fd,IOCTL_RSA_ENC_DEC,&data);
		if(ret == -1){
			printf("rsa enc process error\n");
		}
		
		ret = 0;
		memset(&data,0,sizeof(rsa_data_t));
		
		data.e_or_d = test_2048_d[j];
		data.n = test_2048_n[j];
		data.rsa_mode = 2048;
		data.input = cipher;
		data.mode = RSA_DEC;
		
		data.output = out_buf;
		ret = ioctl(fd,IOCTL_RSA_ENC_DEC,&data);
		if(ret == -1){
			printf("rsa dec process error\n");
		}

		for(i = 0; i < (data.rsa_mode / 32); i++) {
			if(out_buf[i] != test_2048_m[j][i]) {
				printf("#### Failed: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i, out_buf[i], i,	test_2048_m[j][i]);
				printf("#### Failed: %u bits\n", data.rsa_mode);	
				return -1;
			}else {
				printf("#### Success: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i, out_buf[i], i, test_2048_m[j][i]);
			}
		}
		
		pass++;
	}
	printf("-----------------RSA---------------------\n");
	printf("RSA test %d done\n",j);
	printf("-----------------RSA---------------------\n");

	printf("RSA Test done,pass=%d \n",pass);
	free(out_buf);
	free(cipher);
	close(fd);
	return 0;
}
