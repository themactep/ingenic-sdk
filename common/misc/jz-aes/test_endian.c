/*
 * Endianness probe for JZ AES hardware.
 *
 * Tests NIST AES-128-ECB vector (FIPS 197 Appendix B) with all 4
 * combinations of key/data byte ordering to determine what the
 * hardware expects on a little-endian MIPS.
 *
 * NIST vector:
 *   Key:    2b7e1516 28aed2a6 abf71588 09cf4f3c
 *   Plain:  3243f6a8 885a308d 313198a2 e0370734
 *   Cipher: 3925841d 02dc09fb dc118597 196a0b32
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include "test_aes.h"

static const unsigned char nist_key[16] = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};
static const unsigned char nist_plain[16] = {
	0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d,
	0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34
};
static const unsigned char nist_cipher[16] = {
	0x39, 0x25, 0x84, 0x1d, 0x02, 0xdc, 0x09, 0xfb,
	0xdc, 0x11, 0x85, 0x97, 0x19, 0x6a, 0x0b, 0x32
};

static inline unsigned int bswap32(unsigned int x)
{
	return ((x >> 24) & 0xff) | ((x >> 8) & 0xff00) |
	       ((x << 8) & 0xff0000) | ((x << 24) & 0xff000000);
}

/* Load 4 bytes as a big-endian 32-bit word */
static inline unsigned int load_be32(const unsigned char *p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/* Load 4 bytes as a little-endian 32-bit word */
static inline unsigned int load_le32(const unsigned char *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static void hexdump(const char *label, const unsigned char *buf, int len)
{
	printf("  %-8s", label);
	for (int i = 0; i < len; i++)
		printf("%02x", buf[i]);
	printf("\n");
}

/*
 * Try one combination:
 *   key_be:  load key words as big-endian (0) or little-endian (1)
 *   data_swap: byte-swap DMA data words before/after (0=no, 1=yes)
 *
 * Note: data_swap swaps each 4-byte group in the plaintext buffer
 * before sending to hardware, and swaps back in the output.
 */
static int try_combo(int fd, int key_le, int data_swap)
{
	struct aes_para p;
	unsigned char plain[16], cipher[16];
	int i;

	memset(&p, 0, sizeof(p));

	/* load key */
	for (i = 0; i < 4; i++) {
		if (key_le)
			p.aeskey[i] = load_le32(&nist_key[i * 4]);
		else
			p.aeskey[i] = load_be32(&nist_key[i * 4]);
	}

	/* prepare plaintext */
	memcpy(plain, nist_plain, 16);
	if (data_swap) {
		for (i = 0; i < 16; i += 4) {
			unsigned char t;
			t = plain[i+0]; plain[i+0] = plain[i+3]; plain[i+3] = t;
			t = plain[i+1]; plain[i+1] = plain[i+2]; plain[i+2] = t;
		}
	}

	p.enworkmode = JZ_AES_MODE_ENC_ECB;
	p.src = plain;
	p.dst = cipher;
	p.datalen = 16;

	if (ioctl(fd, IOCTL_AES_INIT, &p) < 0) return -1;
	if (ioctl(fd, IOCTL_AES_PROCESSING, &p) < 0) return -1;

	/* swap output back if needed */
	if (data_swap) {
		for (i = 0; i < 16; i += 4) {
			unsigned char t;
			t = cipher[i+0]; cipher[i+0] = cipher[i+3]; cipher[i+3] = t;
			t = cipher[i+1]; cipher[i+1] = cipher[i+2]; cipher[i+2] = t;
		}
	}

	int match = (memcmp(cipher, nist_cipher, 16) == 0);
	printf("key=%s data_swap=%d -> %s\n",
	       key_le ? "LE" : "BE", data_swap,
	       match ? "MATCH!" : "no");
	if (!match)
		hexdump("got:", cipher, 16);

	return match;
}

int main(void)
{
	int fd = open("/dev/aes", O_RDWR);
	if (fd < 0) { perror("open /dev/aes"); return 1; }

	printf("NIST AES-128-ECB test vector endianness probe\n");
	hexdump("expect:", nist_cipher, 16);
	printf("\n");

	int found = 0;
	for (int key_le = 0; key_le <= 1; key_le++) {
		for (int data_swap = 0; data_swap <= 1; data_swap++) {
			if (try_combo(fd, key_le, data_swap))
				found = 1;
		}
	}

	if (!found)
		printf("\nNo combination matched NIST vector.\n");

	close(fd);
	return found ? 0 : 1;
}
