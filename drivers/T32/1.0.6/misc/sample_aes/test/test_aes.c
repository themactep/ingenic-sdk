#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include "test_aes.h"

// #define  GETCBCOUTFILE
#define AES_KEY_SIZE         (128)   // AES-128
#define DMABUFSIZE           (16*1024)

int g_fd;

// 生成随机数据
void generate_random_data(unsigned char *data, size_t len) {
	size_t i;
	for (i = 0; i < len; i++) {
		data[i] = (unsigned char)(rand() % 256);
	}
}

void print_hex(const char *label, const unsigned char *data, size_t len) {
	printf("%s: ", label);
	for (size_t i = 0; i < len; i++) {
		printf("%02X", data[i]);
	}
	printf("\n");
}

static int aes_crypt_hw( struct aes_para *para,
                    int mode,
                    size_t length,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output )
{
	int i;
	char temp_iv[16];
	const unsigned char *input_for_iv = input;
	unsigned char *output_for_iv = output;

	if( length % 16 ){
		printf("err,hw aes make sure datalen align 16\n");
		return( -1 );
	}

	for (i = 0; i < 4; ++i) {
		para->aesiv[i] = ((uint32_t)iv[i*4 +3] << 24) |
					((uint32_t)iv[i*4 +2] << 16) |
					((uint32_t)iv[i*4 +1] << 8)  |
					(uint32_t)iv[i*4 +0];
	}

	para->status = 0;
	para->datalen = length;

	 if( mode == IN_UNF_CIPHER_WORK_MODE_DEC_CBC ) {
		memcpy(temp_iv, input_for_iv + length - 16, 16);

		para->enworkmode = IN_UNF_CIPHER_WORK_MODE_DEC_CBC;

		ioctl(g_fd, IOCTL_AES_PROCESSING, para);

		memcpy(iv,temp_iv,16);

	} else {
		para->enworkmode = IN_UNF_CIPHER_WORK_MODE_ENC_CBC;
		ioctl(g_fd, IOCTL_AES_PROCESSING, para);
		output_for_iv += length - 16;
		memcpy(iv,output_for_iv,16);
	}

}

static int aes_crypt_hw_cbc(void *src_dma_map_buf,void *dst_dma_map_buf)
{
	struct aes_para para;
	char *src = NULL;
	char *dst = NULL;
	char *check = NULL;
	unsigned int len = 1 * 1024 * 1024;
	unsigned int offset = 0;
	int i = 0;
	int dmalen = 0;
	unsigned char iv[16],iv_dec[16];
	unsigned char key[AES_KEY_SIZE / 8];

	src = malloc(len * 3);
	if(!src){
		printf("Failed to malloc buffer!\n");
		return -1;
	}

	dst = src + len;
	check = dst + len;

	srand((int)(time(0)));
	memset(src, 0, len*3);

	for(i = 0; i < len; i++){
		src[i] =  rand() % 256;
	}

	memset(&para, 0, sizeof(para));

	generate_random_data(key, sizeof(key));
	generate_random_data(iv, sizeof(iv));
	memcpy(iv_dec, iv,16);

	para.keybits = sizeof(key)*8;
	for (i = 0; i < (para.keybits >> 5); ++i) {
		para.aeskey[i] = ((uint32_t)key[i * 4 + 3] << 24) |
					((uint32_t)key[i * 4 + 2] << 16) |
					((uint32_t)key[i * 4 + 1] << 8)  |
					(uint32_t)key[i * 4 + 0];
	}

	//enc
	while(offset < len){
		dmalen = len - offset > DMABUFSIZE ? DMABUFSIZE : len - offset;
		memcpy(src_dma_map_buf ,src + offset,dmalen);

		aes_crypt_hw(&para,IN_UNF_CIPHER_WORK_MODE_ENC_CBC,dmalen,iv,src_dma_map_buf,dst_dma_map_buf);

		memcpy(dst + offset,dst_dma_map_buf ,dmalen);
		offset += dmalen;
	}

	//dec
	offset = 0;
	while(offset < len){
		dmalen = len - offset > DMABUFSIZE ? DMABUFSIZE : len - offset;
		memcpy(src_dma_map_buf ,dst + offset,dmalen);

		aes_crypt_hw(&para,IN_UNF_CIPHER_WORK_MODE_DEC_CBC,dmalen,iv_dec,src_dma_map_buf,dst_dma_map_buf);

		memcpy(check + offset,dst_dma_map_buf ,dmalen);
		offset += dmalen;
	}

	if (memcmp(src, check, len) != 0) {
		printf("[FAIL] aes cbc enc and dec err\n");
		print_hex("src", (const unsigned char *)src, len);
		print_hex("dst", (const unsigned char *)check, len);
		return -1;
	}
	printf("aes cbc run succeed\n");
	return 0;
}

static int aes_crypt_hw_ecb(void)
{
	struct aes_para para;
	char *src = NULL;
	char *dst = NULL;
	char *check = NULL;
	unsigned int len = 1 * 1024 * 1024;
	unsigned int offset = 0;
	int i = 0;
	int dmalen = 0;
	unsigned char iv[16],iv_dec[16];
	unsigned char key[AES_KEY_SIZE / 8];

	src = malloc(len * 3);
	if(!src){
		printf("Failed to malloc buffer!\n");
		return -1;
	}

	dst = src + len;
	check = dst + len;

	srand((int)(time(0)));
	memset(src, 0, len*3);

	for(i = 0; i < len; i++){
		src[i] =  rand() % 256;
	}

	memset(&para, 0, sizeof(para));

	generate_random_data(key, sizeof(key));
	generate_random_data(iv, sizeof(iv));
	memcpy(iv_dec, iv,16);

	para.keybits = sizeof(key)*8;
	for (i = 0; i < (para.keybits >> 5); ++i) {
		para.aeskey[i] = ((uint32_t)key[i * 4 + 3] << 24) |
					((uint32_t)key[i * 4 + 2] << 16) |
					((uint32_t)key[i * 4 + 1] << 8)  |
					(uint32_t)key[i * 4 + 0];
	}

	for (i = 0; i < 4; ++i) {
		para.aesiv[i] = ((uint32_t)iv[i*4 +3] << 24) |
					((uint32_t)iv[i*4 +2] << 16) |
					((uint32_t)iv[i*4 +1] << 8)  |
					(uint32_t)iv[i*4 +0];
	}

	//enc
	para.src = src;
	para.dst = dst;
	para.datalen = len;
	para.enworkmode = IN_UNF_CIPHER_WORK_MODE_ENC_ECB;
	ioctl(g_fd, IOCTL_AES_PROCESSING, &para);

	//dec
	para.src = dst;
	para.dst = check;
	para.datalen = len;
	para.enworkmode = IN_UNF_CIPHER_WORK_MODE_DEC_ECB;
	ioctl(g_fd, IOCTL_AES_PROCESSING, &para);

	if (memcmp(src, check, len) != 0) {
		printf("[FAIL] aes ecb enc and dec err\n");
		print_hex("src", (const unsigned char *)src, len);
		print_hex("dst", (const unsigned char *)check, len);
		return -1;
	}

	printf("aes ecb run succeed\n");
	return 0;
}

int main(int argc, const char* argv[])
{

	// struct aes_map_addr getaddr;

	g_fd = open("/dev/aes", O_RDWR);
	if(g_fd < 0){
		printf("Failed to open /dev/aes\n");
		return -1;
	}

	void *src_dma_map_buf = NULL;
	void *dst_dma_map_buf = NULL;
	src_dma_map_buf = mmap(NULL, DMABUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
	if (src_dma_map_buf == MAP_FAILED) {
		perror("mmap src failed");
		exit(EXIT_FAILURE);
	}

	if(aes_crypt_hw_cbc(src_dma_map_buf,src_dma_map_buf)){
		return -1;
	}

	if(aes_crypt_hw_ecb()){
		return -1;
	}

	return 0;
}
