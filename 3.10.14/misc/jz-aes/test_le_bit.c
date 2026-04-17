/*
 * Test if ASCR bit 12 (LITTLE_ENDIAN) works on T31.
 * If it does, we can skip software byte-swapping.
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "test_aes.h"

/* NIST FIPS 197 Appendix B */
static const unsigned char nist_key[16] = {
    0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
    0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c
};
static const unsigned char nist_plain[16] = {
    0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,
    0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34
};
static const unsigned char nist_cipher[16] = {
    0x39,0x25,0x84,0x1d,0x02,0xdc,0x09,0xfb,
    0xdc,0x11,0x85,0x97,0x19,0x6a,0x0b,0x32
};

static inline unsigned int load_be32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

/*
 * Poke bit 12 of ASCR via /dev/mem, then test with NO byte-swapping.
 * If NIST vector matches, the hardware supports LE mode on T31.
 */
int main(void)
{
    int fd;
    struct aes_para p;
    unsigned char cipher[16];
    int devmem_fd;
    volatile unsigned int *ascr;

    printf("Testing ASCR bit 12 (LITTLE_ENDIAN) on T31...\n\n");

    /* First: test WITHOUT bit 12, WITHOUT byte-swap (should fail, baseline) */
    fd = open("/dev/aes", O_RDWR);
    if (fd < 0) { perror("open /dev/aes"); return 1; }

    memset(&p, 0, sizeof(p));
    p.aeskey[0] = load_be32(nist_key);
    p.aeskey[1] = load_be32(nist_key + 4);
    p.aeskey[2] = load_be32(nist_key + 8);
    p.aeskey[3] = load_be32(nist_key + 12);
    p.enworkmode = JZ_AES_MODE_ENC_ECB;
    p.src = (unsigned char *)nist_plain;
    p.dst = cipher;
    p.datalen = 16;

    ioctl(fd, IOCTL_AES_INIT, &p);
    ioctl(fd, IOCTL_AES_PROCESSING, &p);
    close(fd);

    printf("Without bit 12, no swap: ");
    if (memcmp(cipher, nist_cipher, 16) == 0)
        printf("MATCH (unexpected)\n");
    else {
        printf("no match (expected)\n  got: ");
        for (int i = 0; i < 16; i++) printf("%02x", cipher[i]);
        printf("\n");
    }

    /* Now: set bit 12 via /dev/mem before the AES operation */
    devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem_fd < 0) {
        perror("open /dev/mem");
        printf("Cannot test bit 12 without /dev/mem access\n");
        return 1;
    }

    /* AES_IOBASE = 0x13430000, ASCR = offset 0x00 */
    ascr = (volatile unsigned int *)mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                                         MAP_SHARED, devmem_fd, 0x13430000);
    if (ascr == MAP_FAILED) {
        perror("mmap");
        close(devmem_fd);
        return 1;
    }

    printf("\nASCR before: 0x%08x\n", ascr[0]);

    /* Open /dev/aes, do INIT (which configures ASCR), then poke bit 12 */
    fd = open("/dev/aes", O_RDWR);
    if (fd < 0) { perror("open /dev/aes"); munmap((void*)ascr, 4096); close(devmem_fd); return 1; }

    memset(&p, 0, sizeof(p));
    p.aeskey[0] = load_be32(nist_key);
    p.aeskey[1] = load_be32(nist_key + 4);
    p.aeskey[2] = load_be32(nist_key + 8);
    p.aeskey[3] = load_be32(nist_key + 12);
    p.enworkmode = JZ_AES_MODE_ENC_ECB;
    p.src = (unsigned char *)nist_plain;
    p.dst = cipher;
    p.datalen = 16;

    ioctl(fd, IOCTL_AES_INIT, &p);

    /* Poke bit 12 after INIT configured the hardware */
    unsigned int val = ascr[0];
    printf("ASCR after INIT: 0x%08x\n", val);
    ascr[0] = val | (1 << 12);
    printf("ASCR with bit 12: 0x%08x\n", ascr[0]);

    ioctl(fd, IOCTL_AES_PROCESSING, &p);
    close(fd);

    printf("\nWith bit 12, no swap: ");
    if (memcmp(cipher, nist_cipher, 16) == 0)
        printf("MATCH! T31 supports LITTLE_ENDIAN bit!\n");
    else {
        printf("no match\n  got: ");
        for (int i = 0; i < 16; i++) printf("%02x", cipher[i]);
        printf("\n  want: ");
        for (int i = 0; i < 16; i++) printf("%02x", nist_cipher[i]);
        printf("\n");
    }

    munmap((void *)ascr, 4096);
    close(devmem_fd);
    return 0;
}
