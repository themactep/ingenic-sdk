/***
 * @Description  : ingenic_lzma.h
 * @* Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 */

#ifndef __INGENIC_LZMA_H
#define __INGENIC_LZMA_H

#include "linux/types.h"

#define DEBUG_LZMA

#define JZ_LZMA_START_ADDRESS 0x131f0000
#define LZMA_CTRL				0*4
#define LZMA_BS_BASE			1*4
#define LZMA_BS_SIZE			2*4
#define LZMA_DST_BASE			3*4
#define LZMA_TIMEOUT			4*4
#define LZMA_FINAL_SIZE			5*4

#define LZMA_FSM_DBG			6*4
#define LZMA_RADDR_CNT_DBG		7*4
#define LZMA_RADDR_CFG_NUM		8*4
#define LZMA_RADDR0_DBG			9*4
#define LZMA_RADDR1_DBG			10*4
#define LZMA_RDATA_CNT_DBG		11*4
#define LZMA_RDATA_CFG_NUM		12*4
#define LZMA_RDATAL0_DBG		13*4
#define LZMA_RDATAH0_DBG		14*4
#define LZMA_CRC_RDATA			15*4
#define LZMA_CRC_HDEC_DATA		16*4
#define LZMA_CRC_FSM_DATA		17*4
#define LZMA_CRC_DIST_DATA		18*4
#define LZMA_CRC_LEN_DATA		19*4
#define LZMA_CRC_LIT_DATA		20*4
#define LZMA_CRC_WDATA			21*4
#define LZMA_VERISON			22*4

#define DEBUG() printk("%s %s %d\n", __FILE__, __func__, __LINE__)

#define JZ_CPM_START_ADDRESS 0x10000000
#define CPM_CPCCR 0x00
#define CPM_CLKGR1 0x28
#define CPM_TNPUCDR 0x98
#define CPM_ISPACDR 0xac
#define CPM_SRBC 0xc4

#define IN_SIZE 10
#define OUT_SIZE 10

struct lzma_file_info
{
	char *input_name;
	char *output_name;
	off_t size;
	char channel;
};

#define JZLZMA_IOC_MAGIC  'L'
#define IOCTL_LAMA_WORK			_IOW(JZLZMA_IOC_MAGIC, 01, struct lzma_file_info)

#endif

