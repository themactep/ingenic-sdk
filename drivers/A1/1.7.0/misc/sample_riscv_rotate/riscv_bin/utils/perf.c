#include "riscv.h"

/*
void enable_performance()
{
    CSR_WRITE(PCCR, 0x0);
    CSR_WRITE(PCER, 0x1);
}

void disable_performance()
{
    CSR_WRITE(PCER, 0x0);
}
*/

static int flag = 0;

uint32_t get_mtime()
{
    return *CCU_TIME_L;
}

void set_flag(int stat)
{
	flag = stat;
}

int get_flag(void)
{
	return flag;
}

