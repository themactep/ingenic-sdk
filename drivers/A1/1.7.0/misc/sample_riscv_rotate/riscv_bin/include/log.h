#ifndef __LOG_H__
#define __LOG_H__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;

#define NULL (void *)0

#define REG8(addr)	*((volatile u8 *)(addr))
#define REG16(addr)	*((volatile u16 *)(addr))
#define REG32(addr)	*((volatile u32 *)(addr))

//#define UART_BASE     0x10032000 /* uart2 */
#define UART_BASE     0x10031000 /* uart1 */
//#define UART_BASE     0x10030000 /* uart0 */
//#define UART_BASE     0x80001080 /* RV12 mmio uart */
//#define UART_BASE     0x12a0003c /* J2 TOP Env RV12 uart */

#define OFF_RDR		    (0x00)	   /* R  8b H'xx */
#define OFF_LSR		    (0x14)	   /* R  8b H'00 */
#define OFF_TDR		    (0x00)	   /* W  8b H'xx */
#define UART_LSR_TDRQ	(1 << 5)   /* 1: transmit FIFO half "empty" */
#define UART_LSR_TEMT	(1 << 6)   /* 1: transmit FIFO and shift registers empty */
#define UART_LSR_DR	    (1 << 0)   /* 0: receive FIFO is empty  1: receive data is ready */

int printf(const char *fmt, ...);
#endif
