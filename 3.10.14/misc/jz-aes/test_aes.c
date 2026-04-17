/*
 * Ingenic JZ AES hardware accelerator — userspace test
 *
 * Tests:
 *  1. ECB encrypt/decrypt roundtrip
 *  2. CBC encrypt/decrypt roundtrip
 *  3. Concurrent sessions (two fds open simultaneously)
 *  4. Throughput benchmark (1MB)
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "test_aes.h"

static int aes_init(int fd, struct aes_para *p)
{
	return ioctl(fd, IOCTL_AES_INIT, p);
}

static int aes_process(int fd, struct aes_para *p)
{
	return ioctl(fd, IOCTL_AES_PROCESSING, p);
}

static long elapsed_us(struct timeval *t0, struct timeval *t1)
{
	return (t1->tv_sec - t0->tv_sec) * 1000000L +
	       (t1->tv_usec - t0->tv_usec);
}

static void fill_para(struct aes_para *p, unsigned int mode,
		      unsigned char *src, unsigned char *dst, unsigned int len)
{
	memset(p, 0, sizeof(*p));
	p->aeskey[0] = 0x01020304;
	p->aeskey[1] = 0x05060708;
	p->aeskey[2] = 0x090a0b0c;
	p->aeskey[3] = 0x0d0e0f10;
	p->aesiv[0] = 0xdeadbeef;
	p->aesiv[1] = 0xcafebabe;
	p->aesiv[2] = 0x12345678;
	p->aesiv[3] = 0x9abcdef0;
	p->enworkmode = mode;
	p->src = src;
	p->dst = dst;
	p->datalen = len;
}

static int test_ecb(int fd)
{
	struct aes_para p;
	unsigned char plain[16], cipher[16], check[16];
	int i;

	for (i = 0; i < 16; i++)
		plain[i] = (unsigned char)(i * 0x11);

	fill_para(&p, JZ_AES_MODE_ENC_ECB, plain, cipher, 16);
	if (aes_init(fd, &p) < 0) { perror("ECB enc init"); return -1; }
	if (aes_process(fd, &p) < 0) { perror("ECB enc proc"); return -1; }

	fill_para(&p, JZ_AES_MODE_DEC_ECB, cipher, check, 16);
	if (aes_init(fd, &p) < 0) { perror("ECB dec init"); return -1; }
	if (aes_process(fd, &p) < 0) { perror("ECB dec proc"); return -1; }

	if (memcmp(check, plain, 16) != 0) {
		printf("FAIL: ECB roundtrip mismatch\n");
		return -1;
	}
	printf("PASS: ECB encrypt/decrypt roundtrip (16 bytes)\n");
	return 0;
}

static int test_cbc(int fd)
{
	struct aes_para p;
	unsigned char plain[64], cipher[64], check[64];
	int i;

	for (i = 0; i < 64; i++)
		plain[i] = (unsigned char)i;

	fill_para(&p, JZ_AES_MODE_ENC_CBC, plain, cipher, 64);
	if (aes_init(fd, &p) < 0) { perror("CBC enc init"); return -1; }
	if (aes_process(fd, &p) < 0) { perror("CBC enc proc"); return -1; }

	fill_para(&p, JZ_AES_MODE_DEC_CBC, cipher, check, 64);
	if (aes_init(fd, &p) < 0) { perror("CBC dec init"); return -1; }
	if (aes_process(fd, &p) < 0) { perror("CBC dec proc"); return -1; }

	if (memcmp(check, plain, 64) != 0) {
		printf("FAIL: CBC roundtrip mismatch\n");
		return -1;
	}
	printf("PASS: CBC encrypt/decrypt roundtrip (64 bytes)\n");
	return 0;
}

static int test_concurrent(void)
{
	int fd1, fd2;
	struct aes_para p1, p2;
	unsigned char plain1[32], cipher1[32], check1[32];
	unsigned char plain2[32], cipher2[32], check2[32];
	int i;

	fd1 = open("/dev/aes", O_RDWR);
	fd2 = open("/dev/aes", O_RDWR);
	if (fd1 < 0) { perror("open fd1"); return -1; }
	if (fd2 < 0) {
		printf("FAIL: second open returned %d (concurrent open not supported?)\n", fd2);
		close(fd1);
		return -1;
	}

	/* different plaintext on each session */
	for (i = 0; i < 32; i++) {
		plain1[i] = (unsigned char)i;
		plain2[i] = (unsigned char)(0xff - i);
	}

	/* session 1: ECB encrypt */
	fill_para(&p1, JZ_AES_MODE_ENC_ECB, plain1, cipher1, 32);
	if (aes_init(fd1, &p1) < 0) { perror("s1 init"); goto fail; }
	if (aes_process(fd1, &p1) < 0) { perror("s1 proc"); goto fail; }

	/* session 2: ECB encrypt with DIFFERENT key */
	fill_para(&p2, JZ_AES_MODE_ENC_ECB, plain2, cipher2, 32);
	p2.aeskey[0] = 0xaabbccdd;  /* different key */
	p2.aeskey[1] = 0x11223344;
	p2.aeskey[2] = 0x55667788;
	p2.aeskey[3] = 0x99aabbcc;
	if (aes_init(fd2, &p2) < 0) { perror("s2 init"); goto fail; }
	if (aes_process(fd2, &p2) < 0) { perror("s2 proc"); goto fail; }

	/* verify session 1 can still decrypt with its own key */
	fill_para(&p1, JZ_AES_MODE_DEC_ECB, cipher1, check1, 32);
	if (aes_init(fd1, &p1) < 0) { perror("s1 dec init"); goto fail; }
	if (aes_process(fd1, &p1) < 0) { perror("s1 dec proc"); goto fail; }

	/* verify session 2 can still decrypt with its own key */
	fill_para(&p2, JZ_AES_MODE_DEC_ECB, cipher2, check2, 32);
	p2.aeskey[0] = 0xaabbccdd;
	p2.aeskey[1] = 0x11223344;
	p2.aeskey[2] = 0x55667788;
	p2.aeskey[3] = 0x99aabbcc;
	if (aes_init(fd2, &p2) < 0) { perror("s2 dec init"); goto fail; }
	if (aes_process(fd2, &p2) < 0) { perror("s2 dec proc"); goto fail; }

	if (memcmp(check1, plain1, 32) != 0) {
		printf("FAIL: session 1 roundtrip mismatch\n");
		goto fail;
	}
	if (memcmp(check2, plain2, 32) != 0) {
		printf("FAIL: session 2 roundtrip mismatch\n");
		goto fail;
	}

	/* verify the ciphertexts are different (different keys) */
	if (memcmp(cipher1, cipher2, 32) == 0) {
		printf("FAIL: different keys produced same ciphertext!\n");
		goto fail;
	}

	printf("PASS: concurrent sessions with different keys\n");
	close(fd1);
	close(fd2);
	return 0;

fail:
	close(fd1);
	close(fd2);
	return -1;
}

static int bench(int fd, unsigned int mode, const char *label, int len)
{
	struct aes_para p;
	unsigned char *src, *dst;
	struct timeval t0, t1;
	long us;
	double mbps;

	src = malloc(len);
	dst = malloc(len);
	if (!src || !dst) { perror("malloc"); return -1; }

	srand(time(NULL));
	for (int i = 0; i < len; i++)
		src[i] = rand() & 0xff;

	fill_para(&p, mode, src, dst, len);
	if (aes_init(fd, &p) < 0) { perror("bench init"); free(src); free(dst); return -1; }

	gettimeofday(&t0, NULL);
	if (aes_process(fd, &p) < 0) { perror("bench proc"); free(src); free(dst); return -1; }
	gettimeofday(&t1, NULL);

	us = elapsed_us(&t0, &t1);
	mbps = (double)len / us;
	printf("%-20s %d bytes in %ld us = %.1f MB/s\n", label, len, us, mbps);

	free(src);
	free(dst);
	return 0;
}

int main(void)
{
	int fd = open("/dev/aes", O_RDWR);
	if (fd < 0) {
		perror("open /dev/aes");
		return 1;
	}

	printf("=== Roundtrip tests ===\n");
	if (test_ecb(fd) < 0) goto out;
	if (test_cbc(fd) < 0) goto out;

	printf("\n=== Concurrent session test ===\n");
	/* uses its own fds */
	if (test_concurrent() < 0) goto out;

	printf("\n=== Throughput benchmark ===\n");
	bench(fd, JZ_AES_MODE_ENC_ECB, "ECB encrypt 1MB:", 1024 * 1024);
	bench(fd, JZ_AES_MODE_DEC_ECB, "ECB decrypt 1MB:", 1024 * 1024);
	bench(fd, JZ_AES_MODE_ENC_CBC, "CBC encrypt 1MB:", 1024 * 1024);
	bench(fd, JZ_AES_MODE_DEC_CBC, "CBC decrypt 1MB:", 1024 * 1024);

out:
	close(fd);
	return 0;
}
