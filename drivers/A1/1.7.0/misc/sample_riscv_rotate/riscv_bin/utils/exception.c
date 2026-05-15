#include <stdint.h>
#include <riscv.h>
#include <rotate.h>



__attribute__ ((weak)) void ost_handler (void){
    uint32_t time_h, time_l;
    uint64_t time;

    printf("OST handler\n");
    time_h = *CCU_TIME_H;
    time_l = *CCU_TIME_L;
    time = ((uint64_t)time_h << 32) | time_l;
    time += 0x6000;
    *CCU_TIME_CMP_H = -1;
    *CCU_TIME_CMP_L = (uint32_t)time;
    *CCU_TIME_CMP_H = (uint32_t)(time >> 32);
}

__attribute__ ((weak)) void ext_handler (void){
    printf("EXT handler\n");
	/* hardware */
}

__attribute__ ((weak)) void soft_handler (void){
    /*printf("SOFT handler\n");*/
	/* mailbox */
	int mailbox = *(volatile unsigned int*)(0x12a00008);
	/* clear mailbox */
	*(volatile unsigned int*)(0x12a00008) = 0;
	 printf("recv index :%d\n", mailbox);
	set_flag(mailbox);
	//*(volatile unsigned int*)(0x12a0000c) = mailbox * 3;
	/**(volatile unsigned int*)(0x09000000) = 0x12345678;*/
	if(mailbox == 0x123)
		asm volatile("fence.i");

}

__attribute__ ((weak)) void exc_handler (void){
    uint32_t cause, irq, exccode, mip, mie;

    CSR_READ(MCAUSE, cause);
    irq = cause >> 31;
    exccode = cause & 0x7fffffff;
	//printf("exc_handler 0x%08x\n", irq);
    if (irq) { // Interrupt
        CSR_READ(MIE, mie);
        CSR_READ(MIP, mip);
        if      (mie & mip & MIP_MSIP) soft_handler();
        else if (mie & mip & MIP_MTIP) ost_handler();
        else if (mie & mip & MIP_MEIP) ext_handler();
    } else {
        //printf("Find exception exccode:0x%x\n", exccode);
    }
}

