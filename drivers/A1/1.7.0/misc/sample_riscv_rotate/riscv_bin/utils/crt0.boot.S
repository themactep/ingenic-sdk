/* ========================================================= [ entry ] === */
  .section .text
save_context:
  add x2, x2, -(31 * 4)
  sw  x1, 0(x2)
  sw  x2, 4(x2)
  sw  x3, 8(x2)
  sw  x4, 12(x2)
  sw  x5, 16(x2)
  sw  x6, 20(x2)
  sw  x7, 24(x2)
  sw  x8, 28(x2)
  sw  x9, 32(x2)
  sw  x10, 36(x2)
  sw  x11, 40(x2)
  sw  x12, 44(x2)
  sw  x13, 48(x2)
  sw  x14, 52(x2)
  sw  x15, 56(x2)
  sw  x16, 60(x2)
  sw  x17, 64(x2)
  sw  x18, 68(x2)
  sw  x19, 72(x2)
  sw  x20, 76(x2)
  sw  x21, 80(x2)
  sw  x22, 84(x2)
  sw  x23, 88(x2)
  sw  x24, 92(x2)
  sw  x25, 96(x2)
  sw  x26, 100(x2)
  sw  x27, 104(x2)
  sw  x28, 108(x2)
  sw  x29, 112(x2)
  sw  x30, 116(x2)
  sw  x31, 120(x2)
  jal x1, exc_handler
  j restore_context

restore_context:
  lw  x1, 0(x2)
  lw  x2, 4(x2)
  lw  x3, 8(x2)
  lw  x4, 12(x2)
  lw  x5, 16(x2)
  lw  x6, 20(x2)
  lw  x7, 24(x2)
  lw  x8, 28(x2)
  lw  x9, 32(x2)
  lw  x10, 36(x2)
  lw  x11, 40(x2)
  lw  x12, 44(x2)
  lw  x13, 48(x2)
  lw  x14, 52(x2)
  lw  x15, 56(x2)
  lw  x16, 60(x2)
  lw  x17, 64(x2)
  lw  x18, 68(x2)
  lw  x19, 72(x2)
  lw  x20, 76(x2)
  lw  x21, 80(x2)
  lw  x22, 84(x2)
  lw  x23, 88(x2)
  lw  x24, 92(x2)
  lw  x25, 96(x2)
  lw  x26, 100(x2)
  lw  x27, 104(x2)
  lw  x28, 108(x2)
  lw  x29, 112(x2)
  lw  x30, 116(x2)
  lw  x31, 120(x2)
  add x2, x2, (31 * 4)
  mret

__exc_handler:
  j save_context

reset_handler:
  /* set all registers to zero */
  mv  x1, x0
  mv  x2, x1
  mv  x3, x1
  mv  x4, x1
  mv  x5, x1
  mv  x6, x1
  mv  x7, x1
  mv  x8, x1
  mv  x9, x1
  mv x10, x1
  mv x11, x1
  mv x12, x1
  mv x13, x1
  mv x14, x1
  mv x15, x1
  mv x16, x1
  mv x17, x1
  mv x18, x1
  mv x19, x1
  mv x20, x1
  mv x21, x1
  mv x22, x1
  mv x23, x1
  mv x24, x1
  mv x25, x1
  mv x26, x1
  mv x27, x1
  mv x28, x1
  mv x29, x1
  mv x30, x1
  mv x31, x1

  // open mie
  li x1, 8
  csrw 0x300, x1

  // set mtvec
  la x1, __MTVEC
  csrw 0x305, x1

  mv x1, x0

  /* stack initilization */
  la   x2, _stack_start

_start:
  .global _start

  /* clear BSS */
  la x26, _bss_start
  la x27, _bss_end

  bge x26, x27, zero_loop_end

zero_loop:
  sw x0, 0(x26)
  addi x26, x26, 4
  ble x26, x27, zero_loop
zero_loop_end:


main_entry:
  /* jump to main program entry point (argc = argv = 0) */
  addi x10, x0, 0
  addi x11, x0, 0
  jal x1, main
  #la x1, 0x80001000
  #sw x10, 0(x1)
1:
  wfi
  j 1b

/* =================================================== [ exceptions ] === */
/* This section has to be down here, since we have to disable rvc for it  */

  .section .vectors, "ax"
  .option norvc;
__MTVEC:
  j __exc_handler
  .org 0x80
  j reset_handler
  .org 0x100

