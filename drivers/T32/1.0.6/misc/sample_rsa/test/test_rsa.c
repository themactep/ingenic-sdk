#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "test_rsa.h"

#define RSA1024 1
#define RSA2048 2

int main(int argc, const char *argv[])
{
	rsa_data_t data;
	int i = 0;
	int j = 0;
	int pass = 0;
	int failed = 0;
	int ret = 0;
	int mode = 0;
	int buf_len = 0;

	if (argc != 2) {
		printf("###############################\n");
		printf("./test_rsa [RSA_MODE]\n");
		printf("###############################\n");
		return 0;
	}

	const char *_mode = argv[1];
	if (0 == strcmp(_mode, "RSA1024")) {
		mode = RSA1024;
	} else if (0 == strcmp(_mode, "RSA2048")) {
		mode = RSA2048;
	} else {
		printf("###############################\n");
		printf("[RSA_MODE]:\n\tRSA1024\n\tRSA2048\t\n");
		printf("###############################\n");
		return 0;
	}

	printf("mode = %d\n", mode);
	buf_len = (mode == RSA1024) ? 32 : 64;

	unsigned int *out_buf = (unsigned int *)malloc(buf_len * sizeof(unsigned int));
	if (!out_buf) return 0;

	unsigned int *cipher = (unsigned int *)malloc(buf_len * sizeof(unsigned int));
	if (!cipher) return 0;

	int fd = open("/dev/rsa", 0);
	if (fd < 0) {
		printf("Failed to open /dev/rsa\n");
		return 0;
	}

	for (j = 0; j < TEST_NUM; j++) {
		memset(cipher, 0, buf_len);
		memset(&data, 0, sizeof(rsa_data_t));
		ret = 0;

		if (mode == RSA1024) {
			data.e_or_d = test_1024_e[j];
			data.n = test_1024_n[j];
			data.rsa_mode = 1024;
			data.input = test_1024_m[j];
			data.inlen = 128;
		} else {
			data.e_or_d = test_2048_e[j];
			data.n = test_2048_n[j];
			data.rsa_mode = 2048;
			data.input = test_2048_m[j];
			data.inlen = 256;
		}

		data.output = cipher;
		ret = ioctl(fd, IOCTL_RSA_ENC_DEC, &data);
		if (ret == -1) {
			printf("rsa enc process error\n");
		}

		ret = 0;
		memset(&data, 0, sizeof(rsa_data_t));

		if (mode == RSA1024) {
			data.e_or_d = test_1024_d[j];
			data.n = test_1024_n[j];
			data.rsa_mode = 1024;
			data.input = cipher;
			data.inlen = 128;
		} else {
			data.e_or_d = test_2048_d[j];
			data.n = test_2048_n[j];
			data.rsa_mode = 2048;
			data.input = cipher;
			data.inlen = 256;
		}

		data.output = out_buf;
		ret = ioctl(fd, IOCTL_RSA_ENC_DEC, &data);
		if (ret == -1) {
			printf("rsa dec process error\n");
		}

		for (i = 0; i < buf_len; i++) {
			if (mode == RSA2048) {
				if (out_buf[i] != test_2048_m[j][i]) {
					printf("#### Failed: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i, out_buf[i], i, test_2048_m[j][i]);
					ret = -1;
				} else {
					printf("#### Success: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i, out_buf[i], i, test_2048_m[j][i]);
				}
			} else {
				if (out_buf[i] != test_1024_m[j][i]) {
					printf("#### Failed: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i, out_buf[i], i, test_1024_m[j][i]);
					ret = -1;
				} else {
					printf("#### Success: out[%d] = 0x%08x  check[%d] = 0x%08x\n", i, out_buf[i], i, test_1024_m[j][i]);
				}
			}
		}

		if (ret == -1) {
			failed++;
		} else {
			pass++;
		}

		printf("-----------------RSA---------------------\n");
		printf("RSA test %d done\n", j);
		printf("-----------------RSA---------------------\n");
	}

	printf("RSA %s Test done, pass=%d failed=%d\n", _mode, pass, failed);
	close(fd);
	return 0;
}
