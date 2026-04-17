/* Userspace ioctl/struct definitions — must match jz-aes.h */
#ifndef __AES_TEST_H__
#define __AES_TEST_H__

#include <sys/ioctl.h>

#define JZAES_IOC_MAGIC		'A'
#define IOCTL_AES_GET_PARA	_IOW(JZAES_IOC_MAGIC, 110, unsigned int)
#define IOCTL_AES_PROCESSING	_IOW(JZAES_IOC_MAGIC, 111, unsigned int)
#define IOCTL_AES_INIT		_IOW(JZAES_IOC_MAGIC, 112, unsigned int)
#define IOCTL_AES_DO		_IOW(JZAES_IOC_MAGIC, 113, unsigned int)

#define AES_FLAG_BSWAP	(1 << 0)

enum jz_aes_work_mode {
	JZ_AES_MODE_ENC_ECB = 0,
	JZ_AES_MODE_DEC_ECB = 1,
	JZ_AES_MODE_ENC_CBC = 2,
	JZ_AES_MODE_DEC_CBC = 3,
};

struct aes_para {
	unsigned int status;
	unsigned int enworkmode;
	unsigned int aeskey[8];
	unsigned int aesiv[4];
	unsigned char *src;
	unsigned char *dst;
	unsigned int datalen;
	unsigned int donelen;
	int keybits;
	unsigned int flags;
};

#endif
