/*
 * Ingenic JZ AES — session key coherence regression test
 *
 * The hardware key registers are shared by every open of /dev/aes. A session
 * that skips loading its key because it believes the engine still holds it can
 * encrypt with another session's key. This test drives IOCTL_AES_DO through
 * interleaved file descriptors and checks every result against a known-answer
 * vector.
 *
 * Assumes the T31 byte order: AES_FLAG_BSWAP on, key words big-endian. Build
 * statically with the target toolchain:
 *
 *   mipsel-linux-gcc -std=c11 -O2 -Wall -Wextra -static \
 *       test_aes_shared.c -o test_aes_shared
 *
 * Exit status is 0 when every check passes, 1 otherwise.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "test_aes.h"

#define PLAIN "00112233445566778899aabbccddeeff"

static const unsigned char key128[16] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};

static const unsigned char key192[24] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
};

static const unsigned char key256[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};

/* FIPS 197 Appendix C, plaintext 00112233445566778899aabbccddeeff */
static const unsigned char ct128[16] = {
	0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
	0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a,
};

static const unsigned char ct192[16] = {
	0xdd, 0xa9, 0x7c, 0xa4, 0x86, 0x4c, 0xdf, 0xe0,
	0x6e, 0xaf, 0x70, 0xa0, 0xec, 0x0d, 0x71, 0x91,
};

static const unsigned char ct256[16] = {
	0x8e, 0xa2, 0xb7, 0xca, 0x51, 0x67, 0x45, 0xbf,
	0xea, 0xfc, 0x49, 0x90, 0x4b, 0x49, 0x60, 0x89,
};

static const unsigned char pt[16] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};

static void set_words(unsigned int *dst, const unsigned char *bytes, int n)
{
	int i;

	for (i = 0; i < n; i++)
		dst[i] = ((unsigned int)bytes[4 * i] << 24) |
			 ((unsigned int)bytes[4 * i + 1] << 16) |
			 ((unsigned int)bytes[4 * i + 2] << 8) |
			 bytes[4 * i + 3];
}

static void fill(struct aes_para *p, const unsigned char *key, int keybits,
		 int mode, const unsigned char *iv, unsigned char *src,
		 unsigned char *dst, unsigned int len)
{
	memset(p, 0, sizeof(*p));
	set_words(p->aeskey, key, keybits / 32);
	if (iv)
		set_words(p->aesiv, iv, 4);
	p->keybits = keybits;
	p->enworkmode = mode;
	p->src = src;
	p->dst = dst;
	p->datalen = len;
	p->flags = AES_FLAG_BSWAP;
}

static int do_op(int fd, struct aes_para *p)
{
	return ioctl(fd, IOCTL_AES_DO, p);
}

static int proc_op(int fd, struct aes_para *p)
{
	return ioctl(fd, IOCTL_AES_PROCESSING, p);
}

static void hex(const unsigned char *p, int n)
{
	int i;

	for (i = 0; i < n; i++)
		printf("%02x", p[i]);
}

static int check(const char *what, const unsigned char *got,
		 const unsigned char *want, int n)
{
	if (memcmp(got, want, n) == 0)
		return 0;
	printf("FAIL: %s\n  got  ", what);
	hex(got, n);
	printf("\n  want ");
	hex(want, n);
	printf("\n");
	return -1;
}

/* Known-answer vectors through IOCTL_AES_DO, all three key sizes. */
static int test_known_answers(int fd)
{
	struct aes_para p;
	unsigned char out[16];
	int ret = 0;

	fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL, (unsigned char *)pt, out, 16);
	if (do_op(fd, &p) < 0) { perror("DO AES-128"); return -1; }
	ret |= check("AES-128 ECB known answer", out, ct128, 16);

	fill(&p, key192, 192, JZ_AES_MODE_ENC_ECB, NULL, (unsigned char *)pt, out, 16);
	if (do_op(fd, &p) < 0) { perror("DO AES-192"); return -1; }
	ret |= check("AES-192 ECB known answer", out, ct192, 16);

	fill(&p, key256, 256, JZ_AES_MODE_ENC_ECB, NULL, (unsigned char *)pt, out, 16);
	if (do_op(fd, &p) < 0) { perror("DO AES-256"); return -1; }
	ret |= check("AES-256 ECB known answer", out, ct256, 16);

	if (!ret)
		printf("PASS: AES-128/192/256 ECB known answers\n");
	return ret;
}

/* Same fd, same key, twice. Wrong if ASCR_CLR wipes the key schedule. */
static int test_same_fd_repeat(int fd)
{
	struct aes_para p;
	unsigned char out[16];
	int i, ret = 0;

	for (i = 0; i < 3; i++) {
		fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
		     (unsigned char *)pt, out, 16);
		if (do_op(fd, &p) < 0) { perror("DO repeat"); return -1; }
		ret |= check("same fd repeat", out, ct128, 16);
	}
	if (!ret)
		printf("PASS: repeated DO on one fd\n");
	return ret;
}

/*
 * The regression: fd A's key must still be used after fd B loads another key.
 * Also alternates key sizes on one fd.
 */
static int test_cross_fd(int fda, int fdb)
{
	unsigned char out[16];
	struct aes_para p;
	int i, ret = 0;

	for (i = 0; i < 5; i++) {
		fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
		     (unsigned char *)pt, out, 16);
		if (do_op(fda, &p) < 0) { perror("A DO"); return -1; }
		ret |= check("A after B (128-bit key)", out, ct128, 16);

		fill(&p, key256, 256, JZ_AES_MODE_ENC_ECB, NULL,
		     (unsigned char *)pt, out, 16);
		if (do_op(fdb, &p) < 0) { perror("B DO"); return -1; }
		ret |= check("B after A (256-bit key)", out, ct256, 16);
	}

	/* key size switch on one fd */
	fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
	     (unsigned char *)pt, out, 16);
	if (do_op(fda, &p) < 0) { perror("A 128"); return -1; }
	ret |= check("A 128 before switch", out, ct128, 16);

	fill(&p, key256, 256, JZ_AES_MODE_ENC_ECB, NULL,
	     (unsigned char *)pt, out, 16);
	if (do_op(fda, &p) < 0) { perror("A 256"); return -1; }
	ret |= check("A 256 after switch", out, ct256, 16);

	fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
	     (unsigned char *)pt, out, 16);
	if (do_op(fda, &p) < 0) { perror("A back to 128"); return -1; }
	ret |= check("A 128 after switching back", out, ct128, 16);

	if (!ret)
		printf("PASS: interleaved file descriptors keep their keys\n");
	return ret;
}

/* CBC with an IV change, interleaved with another session. */
static int test_cbc(int fda, int fdb)
{
	static const unsigned char iv1[16] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	};
	static const unsigned char iv2[16] = {
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	};
	unsigned char plain[64], c1[64], c2[64], back[64];
	struct aes_para p;
	int i, ret = 0;

	for (i = 0; i < 64; i++)
		plain[i] = (unsigned char)(i * 7 + 3);

	fill(&p, key128, 128, JZ_AES_MODE_ENC_CBC, iv1, plain, c1, 64);
	if (do_op(fda, &p) < 0) { perror("CBC enc 1"); return -1; }

	fill(&p, key128, 128, JZ_AES_MODE_DEC_CBC, iv1, c1, back, 64);
	if (do_op(fda, &p) < 0) { perror("CBC dec"); return -1; }
	ret |= check("CBC roundtrip", back, plain, 64);

	/* another session changes the engine key */
	fill(&p, key256, 256, JZ_AES_MODE_ENC_ECB, NULL,
	     (unsigned char *)pt, back, 16);
	if (do_op(fdb, &p) < 0) { perror("B ECB"); return -1; }

	fill(&p, key128, 128, JZ_AES_MODE_ENC_CBC, iv1, plain, back, 64);
	if (do_op(fda, &p) < 0) { perror("CBC enc 2"); return -1; }
	ret |= check("CBC after another session", back, c1, 64);

	fill(&p, key128, 128, JZ_AES_MODE_ENC_CBC, iv2, plain, c2, 64);
	if (do_op(fda, &p) < 0) { perror("CBC enc iv2"); return -1; }
	if (memcmp(c1, c2, 16) == 0) {
		printf("FAIL: a different IV produced the same first block\n");
		ret = -1;
	}

	if (!ret)
		printf("PASS: CBC roundtrip, IV change, cross-session reuse\n");
	return ret;
}

/* DO and PROCESSING must agree on a multi-chunk buffer. */
static int test_processing_match(void)
{
	unsigned char *plain, *a, *b;
	struct aes_para p;
	int fd, ret = 0;
	unsigned int i, len = 64 * 1024;

	plain = malloc(len);
	a = malloc(len);
	b = malloc(len);
	fd = open("/dev/aes", O_RDWR);
	if (!plain || !a || !b || fd < 0) {
		perror("processing match setup");
		free(plain); free(a); free(b);
		if (fd >= 0)
			close(fd);
		return -1;
	}
	for (i = 0; i < len; i++)
		plain[i] = (unsigned char)(i * 31 + 7);

	fill(&p, key256, 256, JZ_AES_MODE_ENC_ECB, NULL, plain, a, len);
	if (proc_op(fd, &p) < 0) { perror("PROCESSING"); ret = -1; goto out; }

	fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
	     (unsigned char *)pt, b, 16);
	if (do_op(fd, &p) < 0) { perror("DO small"); ret = -1; goto out; }

	fill(&p, key256, 256, JZ_AES_MODE_ENC_ECB, NULL, plain, b, len);
	if (do_op(fd, &p) < 0) { perror("DO large"); ret = -1; goto out; }
	ret |= check("64 KiB DO vs PROCESSING", b, a, len);

	if (!ret)
		printf("PASS: DO and PROCESSING agree over 64 KiB\n");
out:
	close(fd);
	free(plain); free(a); free(b);
	return ret;
}

/* Invalid parameters must fail cleanly and not poison the next operation. */
static int test_invalid(int fd)
{
	unsigned char out[16];
	struct aes_para p;
	int ret = 0;

	fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
	     (unsigned char *)pt, out, 16);
	p.datalen = 17;
	errno = 0;
	if (do_op(fd, &p) == 0 || errno != EINVAL) {
		printf("FAIL: datalen 17 returned success or wrong errno\n");
		ret = -1;
	}

	fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
	     (unsigned char *)pt, out, 16);
	p.keybits = 123;
	errno = 0;
	if (do_op(fd, &p) == 0 || errno != EINVAL) {
		printf("FAIL: keybits 123 returned success or wrong errno\n");
		ret = -1;
	}

	fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
	     (unsigned char *)pt, out, 16);
	p.enworkmode = 4;
	errno = 0;
	if (do_op(fd, &p) == 0 || errno != EINVAL) {
		printf("FAIL: mode 4 returned success or wrong errno\n");
		ret = -1;
	}

	fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
	     (unsigned char *)pt, out, 16);
	if (do_op(fd, &p) < 0) { perror("DO after invalid"); return -1; }
	ret |= check("valid operation after errors", out, ct128, 16);

	if (!ret)
		printf("PASS: invalid parameters rejected, engine still usable\n");
	return ret;
}

/* Alternate two sessions many times. */
static int test_stress(int fda, int fdb, int rounds)
{
	unsigned char out[16];
	struct aes_para p;
	int i, ret = 0;

	for (i = 0; i < rounds; i++) {
		fill(&p, key128, 128, JZ_AES_MODE_ENC_ECB, NULL,
		     (unsigned char *)pt, out, 16);
		if (do_op(fda, &p) < 0) { perror("stress A"); return -1; }
		ret |= check("stress A", out, ct128, 16);

		fill(&p, key256, 256, JZ_AES_MODE_ENC_ECB, NULL,
		     (unsigned char *)pt, out, 16);
		if (do_op(fdb, &p) < 0) { perror("stress B"); return -1; }
		ret |= check("stress B", out, ct256, 16);

		if (ret)
			break;
	}
	if (!ret)
		printf("PASS: %d interleaved DO rounds\n", rounds);
	return ret;
}

int main(void)
{
	int fda, fdb, ret = 0;

	fda = open("/dev/aes", O_RDWR);
	fdb = open("/dev/aes", O_RDWR);
	if (fda < 0 || fdb < 0) {
		perror("open /dev/aes");
		return 1;
	}

	ret |= test_known_answers(fda);
	ret |= test_same_fd_repeat(fda);
	ret |= test_cross_fd(fda, fdb);
	ret |= test_cbc(fda, fdb);
	ret |= test_processing_match();
	ret |= test_invalid(fda);
	ret |= test_stress(fda, fdb, 1000);

	close(fda);
	close(fdb);

	if (ret)
		printf("\nFAILED\n");
	else
		printf("\nOK\n");
	return ret ? 1 : 0;
}
