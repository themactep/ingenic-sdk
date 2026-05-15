#ifndef __AES_TEST_H__
#define __AES_TEST_H__

#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>

//#define USE_AES_CPU_MODE
#undef AES_256
#define AES_DMA_MODE

#define JZAES_IOC_MAGIC  'A'
#define IOCTL_AES_GET_PARA			_IOW(JZAES_IOC_MAGIC, 110, unsigned int)
#define IOCTL_AES_PROCESSING		_IOW(JZAES_IOC_MAGIC, 111, unsigned int)
#define IOCTL_AES_INIT				_IOW(JZAES_IOC_MAGIC, 112, unsigned int)
#define IOCTL_AES_PDMA				_IOW(JZAES_IOC_MAGIC, 113, unsigned int)
#define IOCTL_AES_EFUSE				_IOW(JZAES_IOC_MAGIC, 114, unsigned int)
#define IOCTL_AES_SG_PROCESSING		_IOW(JZAES_IOC_MAGIC, 117, unsigned int)


typedef enum jz_aes_status {
	JZ_AES_STATUS_PREPARE = 0,
	JZ_AES_STATUS_WORKING,
	JZ_AES_STATUS_DONE,
} JZ_AES_STATUS;

typedef enum IN_UNF_CIPHER_WORK_MODE_E
{
	IN_UNF_CIPHER_WORK_MODE_ENC_ECB = 0x0,
	IN_UNF_CIPHER_WORK_MODE_DEC_ECB = 0x1,
	IN_UNF_CIPHER_WORK_MODE_ENC_CBC = 0x2,
	IN_UNF_CIPHER_WORK_MODE_DEC_CBC = 0x3,
	IN_UNF_CIPHER_WORK_MODE_OTHER = 0x4,
}IN_UNF_CIPHER_WORK_MODE;

struct aes_para {
	unsigned int status;
	unsigned int enworkmode;
	unsigned int datalen; // it should be 16bytes aligned
	unsigned int aeskey[8];
	unsigned int aesiv[4]; // when work mode is cbc, the parameter must be setted.
	unsigned char *src;
	unsigned char *dst;
	unsigned int donelen; // The length of the processed data.
	int keybits;
};

#endif
