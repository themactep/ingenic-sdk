#ifndef __RISCV_H__
#define __RISCV_H__

#include "stdint.h"
#include "log.h"

#define CCU_BASE                    0x12a00000
// bit0:reset/rw bit1:sleep/r
#define CCU_CCSR                    (volatile unsigned int *)(CCU_BASE + 0x0 * 4)
// bit31~0:reset entry/rw
#define CCU_CRER                    (volatile unsigned int *)(CCU_BASE + 0x1 * 4)
// bit31~0:host to riscv mailbox, gen intrrrupt/rw
#define CCU_FROM_HOST               (volatile unsigned int *)(CCU_BASE + 0x2 * 4)
// bit31~0:riscv to host mailbox, gen intrrrupt/rw
#define CCU_TO_HOST                 (volatile unsigned int *)(CCU_BASE + 0x3 * 4)
#define CCU_TIME_L                  (volatile unsigned int *)(CCU_BASE + 0x4 * 4)
#define CCU_TIME_H                  (volatile unsigned int *)(CCU_BASE + 0x5 * 4)
#define CCU_TIME_CMP_L              (volatile unsigned int *)(CCU_BASE + 0x6 * 4)
#define CCU_TIME_CMP_H              (volatile unsigned int *)(CCU_BASE + 0x7 * 4)
#define CCU_INTC_MASK_L             (volatile unsigned int *)(CCU_BASE + 0x8 * 4)
#define CCU_INTC_MASK_H             (volatile unsigned int *)(CCU_BASE + 0x9 * 4)
#define CCU_INTC_PEND_L             (volatile unsigned int *)(CCU_BASE + 0xa * 4)
#define CCU_INTC_PEND_H             (volatile unsigned int *)(CCU_BASE + 0xb * 4)
#define CCU_CATCH                   (volatile unsigned int *)(CCU_BASE + 0xd * 4)
#define CCU_RAND_INT                (volatile unsigned int *)(CCU_BASE + 0xe * 4)

#define CCU_PMA_CNT                 4
#define CCU_PMA_ADR_0               (volatile unsigned int *)(CCU_BASE + 0x40)
#define CCU_PMA_ADR_1               (volatile unsigned int *)(CCU_BASE + 0x44)
#define CCU_PMA_ADR_2               (volatile unsigned int *)(CCU_BASE + 0x48)
#define CCU_PMA_ADR_3               (volatile unsigned int *)(CCU_BASE + 0x4c)

#define CCU_PMA_CFG_0               (volatile unsigned int *)(CCU_BASE + 0x60)
#define CCU_PMA_CFG_1               (volatile unsigned int *)(CCU_BASE + 0x64)
#define CCU_PMA_CFG_2               (volatile unsigned int *)(CCU_BASE + 0x68)
#define CCU_PMA_CFG_3               (volatile unsigned int *)(CCU_BASE + 0x6c)


#define MSTATUS         (0x300)
#define MSTATUS_MIE     (1 << 3)

#define MIE             (0x304)
#define MIE_MSIE        (1 << 3)
#define MIE_MTIE        (1 << 7)
#define MIE_MEIE        (1 << 11)

#define MIP             (0x344)
#define MIP_MSIP        (1 << 3)
#define MIP_MTIP        (1 << 7)
#define MIP_MEIP        (1 << 11)

#define MCAUSE          (0x342)
#define MEPC            (0x341)

#define MTVEC 0x305

#define PCER 0x7a0
#define PCCR 0x780

#define CSR_SET(csr, value)      \
  asm volatile ("csrs %[d], %[s]"\
      :                          \
      : [d] "i" (csr)            \
      , [s] "r"  (value)         \
  );

#define CSR_WRITE(csr, value)    \
  asm volatile ("csrw %[d], %[s]"\
      :                          \
      : [d] "i" (csr)            \
      , [s] "r"  (value)         \
  );

#define CSR_CLEAR(csr, value)    \
  asm volatile ("csrc %[d], %[s]"\
      :                          \
      : [d] "i" (csr)            \
      , [s] "r"  (value)         \
  );

#define CSR_READ(csr, value)     \
  asm volatile ("csrr %[d], %[s]"\
      : [d] "=r" (value)         \
      : [s] "i"  (csr)           \
  );

//#define PASS *CCU_CATCH = 0
//#define FAIL *CCU_CATCH = 1

#define CHECK  0xa0a0a0a0
#define FAIL   *CCU_TO_HOST = 0xa0a0a0a0
#define PASS   *CCU_TO_HOST = 0x0a0a0a0a

extern uint32_t get_mtime();

void set_flag(int stat);
int get_flag(void);

#endif
