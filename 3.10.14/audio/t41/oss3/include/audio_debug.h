#ifndef _JZ_AUDIO_DEBUG_H_
#define _JZ_AUDIO_DEBUG_H_
#include <linux/errno.h>
/* =================== switchs ================== */

/**
 * default debug level, if just switch AUDIO_WARNING
 * or AUDIO_INFO, this not effect DEBUG_REWRITE and
 * DEBUG_TIME_WRITE/READ
 **/
/* =================== print tools ================== */

#define AUDIO_INFO_LEVEL		0x0
#define AUDIO_WARNING_LEVEL		0x1
#define AUDIO_ERROR_LEVEL		0x2
#define AUDIO_PRINT(level, format, ...)			\
	pr_printf(level, format, ##__VA_ARGS__)
#define audio_info_print(...) AUDIO_PRINT(AUDIO_INFO_LEVEL, __VA_ARGS__)
#define audio_warn_print(...) AUDIO_PRINT(AUDIO_WARNING_LEVEL, __VA_ARGS__)
#define audio_err_print(...) AUDIO_PRINT(AUDIO_ERROR_LEVEL, __VA_ARGS__)

int pr_printf(unsigned int level, unsigned char *fmt, ...);
void *pr_kmalloc(unsigned long size);
void *pr_kzalloc(unsigned long size);
void pr_kfree(const void *addr);

#endif /* _JZ_AUDIO_DEBUG_H_ */
