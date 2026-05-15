/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : libxio.h
 * Authors    : tyliu@boaz.ic.jz.com
 * Create Time: 2017-07-20:15:27:19
 * Description:
 * Description:
 *
 */

#ifndef __LIBXIO_H__
#define __LIBXIO_H__

#include <stddef.h>
#include <sys/stat.h>
typedef int             __kernel_pid_t;
typedef __kernel_pid_t          pid_t;

extern int read(int __fd, void *__buf, size_t __nbyte);
extern int write(int __fd, const void *__buf, size_t __nbyte);
extern int open(const char * __cmd, int __abc, ...);
extern int close(int __fd);
extern int fstat(int __fd, struct stat *__sbuf);
extern off_t lseek(int __fildes, off_t __offset, int __whence);
extern int thread_give(int __asid_value);
extern int thread_get();
extern void exit(int data);
extern void _exit(int data);
extern caddr_t sbrk(int incr);
extern int isatty(int filedes);
extern int stat(const char *path, struct stat *buf);
extern int kill (pid_t PID, int SIGNUM);
extern pid_t getpid (void);
extern int pdma_channel0(unsigned int * origin_addr, unsigned int * dest_addr, unsigned int len);
extern int pdma_channel1(unsigned int * origin_addr, unsigned int * dest_addr, unsigned int len);
extern int pdma_channel2(unsigned int * origin_addr, unsigned int * dest_addr, unsigned int len);
extern int pdma_channel3(unsigned int * origin_addr, unsigned int * dest_addr, unsigned int len);
extern int sys_pdma(unsigned int * origin_addr, unsigned int * dest_addr, unsigned int len, unsigned int channel);

#endif /* __LIBXIO_H__ */

