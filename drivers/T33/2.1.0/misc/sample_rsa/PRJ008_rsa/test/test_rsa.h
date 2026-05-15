#ifndef __RSA_H__
#define __RSA_H__

#define JZRSA_IOC_MAGIC                    'R'
#define IOCTL_RSA_ENC_DEC _IOW(JZRSA_IOC_MAGIC, 110, unsigned int)
#define  RSA_ENC 1
#define  RSA_DEC 2
#define  TEST_NUM 1000

typedef struct rsa_data {
	unsigned int *e_or_d;
    unsigned int *n;
    unsigned int rsa_mode;
    unsigned int *input;
    unsigned int inlen;
    unsigned int *output;
    unsigned int mode;
    unsigned int times;
}rsa_data_t;

#include "include/test_rsad_2048.h"
#include "include/test_rsae_2048.h"
#include "include/test_rsam_2048.h"
#include "include/test_rsan_2048.h"

#include "include/test_rsad_1024.h"
#include "include/test_rsae_1024.h"
#include "include/test_rsam_1024.h"
#include "include/test_rsan_1024.h"

#endif
