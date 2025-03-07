
#ifndef __MACRO_DEF_H__
#define __MACRO_DEF_H__

#define VERSION_MAJOR 2
#define VERSION_MINOR 1
//
// 1.0 initialized verison
// 1.1 add INTC irq
// 1.2 add random flush a TLB entry
// 1.3 add default exception handle for print EPC EPC_INSTR EXCCODE
// 1.4 dynamin generate tlb refill handle, beause the DDR traning can destroy TLB refill handle
// 1.5 add invalidate cache
// 2.0 support palladium
// 2.1 the function "printk" is fast

/* print debug msg*/
//#define __DEBUG__

/* Enable randrom flush for function verify*/
//#define INGENIC_CPU_FUNCT_VERIFY

/* SMP miration will be enable, if define this macro*/
//#define SMP_MIRATION

/* enable ost */
//#define OST_ENABLE

/* config support msa. */
//#define  CONFIG_HAS_MSA

/* palladium imp*/
//#define PALLADIUM_IMP

/* OST compare value when define PALLADIUM_IMP */
#define PALLADIUM_OST_COMPARE 0xffffffff

/* IF you are not ITE project member, be careful to modify the config */

/* Thread num band core num, it is return error, if max thread num > max core num. */
/* eg:thread0 -> core0 ... thread3 -> core3 */
//#define THREAD_BAND_CORE

/* kernel can auto check ccu cncr, if undefine this macro*/
//#define CORE_NUM             4

/* config cpu mode for user space cases, 1 is user mode, 0 is kernel mode */
#define USER_SPACE_MODE        1
//#define USER_SPACE_MODE      0

/* intc memary map*/
#define INTC_BASE              0xb2300000

/* global ost memary map*/
#define GLOBAL_OST_BASE        0xb2000000
#define GLOBAL_OST_CCR         (GLOBAL_OST_BASE + 0x0)
#define GLOBAL_OST_ER          (GLOBAL_OST_BASE + 0x4)
#define GLOBAL_OST_CR          (GLOBAL_OST_BASE + 0x8)
#define GLOBAL_OST_CNTH        (GLOBAL_OST_BASE + 0xc)
#define GLOBAL_OST_CNTL        (GLOBAL_OST_BASE + 0x10)
#define GLOBAL_OST_CNTB        (GLOBAL_OST_BASE + 0x14)

/* core0 ost base addr*/
#define CORE0_OST_BASE         0xb2100000

/* OST compare value's address from CSE*/
#define OST_COMPARE_VALUE_ADDR 0xbfcfff00

/* max support SMP core number */
#define SMP_CORE_NUM           4

/* kernel mode case number */
#define K_MODE_CASESE_NUM_MAX  2
/* user mode case number */
#define U_MODE_CASESE_NUM_MAX  8
/* total casese number */
#define CASESE_NUM            (K_MODE_CASESE_NUM_MAX + U_MODE_CASESE_NUM_MAX)
/* user mode case number + kernel mode case number*/
#define RANDOM_NUM_MAX        (U_MODE_CASESE_NUM_MAX + K_MODE_CASESE_NUM_MAX)
/*the original value of the random*/
#define RANDOM                 17

#define PID_MIN                32
#define PID_MAX                128
#define K_PID_OFFSET           PID_MIN
#define K_PID_MAX              33
#define U_PID_OFFSET           (PID_MIN + 32)
#define U_PID_MAX              71

/*instr and time counter addr offset*/
#define COUNTER0_LO_OFFSET     0xa000
#define COUNTER0_HI_OFFSET     0xb000

#define COUNTER1_LO_OFFSET     0xa000 + 4
#define COUNTER1_HI_OFFSET     0xb000 + 4

/*ccu counter clear*/
#define CCU_CNT_CLEAR          0x9000

/*ccu_base*/
#define CCU_BASE_ADDR          0xb2200000

//#define USE_CP0_TIMER

////exit flags.
#define E_XFAIL                0x0
#define E_PASS                 0x1
#define E_FAIL                 0x2
#define E_RESERVED_CODE        0x3

#define SYSCALL_BASE           0xbfcffffc
#define SYS_READ               1
#define SYS_WRITE              2
#define SYS_OPEN               3
#define SYS_CLOSE              4
//#define SYS_FSTAT            5
#define SYS_LSEEK              5
//#define SYS_SBRK             12
#define SYS_PASS_EXIT          60
#define SYS_FAIL_EXIT          61
#define SYS_THREAD_GIVE        10
#define SYS_THREAD_GET         11
//#define PDMA_CHANNEL0        12
//#define PDMA_CHANNEL1        13
//#define PDMA_CHANNEL2        14
//#define PDMA_CHANNEL3        15
//
//
#define IRQ_NUMBER             8
#define IRQ_ID_INTC            0
#define IRQ_ID_MAILBOX         1
#define IRQ_ID_OST             2

#define CSE_FAIL_ADDR          0xbc7ffff4
#define CSE_PASS_ADDR          0xbc7ffff8

////kernel mode process entry base.
#define K_PROC_ENTRY_BASE      (0x80000000+((256-64)<<20))
#define K_PROCS_OFFSET         (0x40<<20)
#define K_DATA_START_ADDR      (K_PROC_ENTRY_BASE+(0xb<<20))
#define K_GP_START_ADDR        (K_DATA_START_ADDR+(0xa<<20))
#define K_SP_START_ADDR        (K_PROC_ENTRY_BASE+K_PROCS_OFFSET)

////user mode process entry base.
#define U_PROC_ENTRY_BASE      0x5c000000
#define U_PROCS_OFFSET         (0x40<<20)
#define U_DATA_START_ADDR      (U_PROC_ENTRY_BASE+(0xb<<20))
#define U_GP_START_ADDR        (U_DATA_START_ADDR+(0xa<<20))
#define U_SP_START_ADDR        (U_PROC_ENTRY_BASE+U_PROCS_OFFSET)

/* address space */
//core private space:  "code"  and  "data"  section.
//data: generally for core private stack in kernel mode.
#define CORE0_CODE_START       0x80010000
#define CORE0_DATA_START       0x80300000

//shared exception space. handler code.
#define EXCEPT_ENTRY_ADDR      0x80000000

#define  __INIT_ENTRY          .section "._initial_entry_sect.text", "ax"
#define  __INIT                .section "._init_code_sect.text", "ax"
#define  __EXP_HANDLE          .section "._exp_inter_handle.text", "ax"

#define  __init                __attribute__ ((section("._init_code_sect.text")))

#define  __c0_context          __attribute__ ((section("._c0_context")))
#define  __c1_context          __attribute__ ((section("._c1_context")))
#define  __c2_context          __attribute__ ((section("._c2_context")))
#define  __c3_context          __attribute__ ((section("._c3_context")))
#define  __k0_context          __attribute__ ((section("._k0_context")))
#define  __k1_context          __attribute__ ((section("._k1_context")))
#define  __u0_context          __attribute__ ((section("._u0_context")))
#define  __u1_context          __attribute__ ((section("._u1_context")))
#define  __u2_context          __attribute__ ((section("._u2_context")))
#define  __u3_context          __attribute__ ((section("._u3_context")))
#define  __u4_context          __attribute__ ((section("._u4_context")))
#define  __u5_context          __attribute__ ((section("._u5_context")))
#define  __u6_context          __attribute__ ((section("._u6_context")))
#define  __u7_context          __attribute__ ((section("._u6_context")))

#define  __kernel_data         __attribute__ ((section("._kernel_data")))

#endif
