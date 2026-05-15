#include <rotate.h>

struct jz_rotate_info rotate_info;



static int run_rotate(void)
{
	int ret = 0;
	if(rotate_info.type == ARGB1555){
		if(rotate_info.angle == ROTATE90){
			argb1555rotate90();
		}else if(rotate_info.angle == ROTATE180){
			//argb1555rotate180();
			argb1555rotateinline180();
		}else if(rotate_info.angle == ROTATE270){
			argb1555rotate270();
		}else{
			rotate360(rotate_info);;
		}

	}else if(rotate_info.type == NV12){
		if(rotate_info.angle == ROTATE90){
			nv12rotate90();
		}else if(rotate_info.angle == ROTATE180){
			nv12rotateinline180();
		}else if(rotate_info.angle == ROTATE270){
			nv12rotate270();
		}else{
			rotate360(rotate_info);
		}
	}else if(rotate_info.type == ARGB8888){
		if(rotate_info.angle == ROTATE90){
			argb8888rotate90();
		}else if(rotate_info.angle == ROTATE180){
			//argb8888rotate90();
			argb8888rotateinline180();
		}else if(rotate_info.angle == ROTATE270){
			argb8888rotate270();
		}else{
			rotate360(rotate_info);
		}
	}else{
		printf("pic type fail\n");
		ret = -1;
	}

	return ret;
}



int main()
{

	CSR_SET(MSTATUS, MSTATUS_MIE);
    CSR_SET(MIE, MIE_MSIE);
	int ret = 0;

	asm volatile("wfi");
	while(1){
		printf("riscv run\n");
		ret = get_rotate_info(&rotate_info);
		if(ret == 0){
			if(run_rotate() != 0){
				*(volatile unsigned int*)(0x12a0000c) = 0x123;//send fail signal
			}else{
				*(volatile unsigned int*)(0x12a0000c) = 1;//send succ signal
			}
			asm volatile("fence.i");//sync
			set_flag(0);
		}
		asm volatile("wfi");
	}

    return 0;
}

