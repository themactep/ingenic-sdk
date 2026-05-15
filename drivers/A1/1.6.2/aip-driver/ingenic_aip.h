#ifndef __JZ_AIP_H__
#define __JZ_AIP_H__

#define JZ_AIP_T_IOBASE		0x13090000
#define JZ_AIP_F_IOBASE		0x13090200
#define JZ_AIP_P_IOBASE		0x13090300
#define JZ_AIP_IOSIZE		0x100


#define IOCTL_AIP_IRQ_WAIT_CMP		_IOWR('P', 0, int)
#define IOCTL_AIP_MALLOC		_IOWR('P', 1, int)
#define IOCTL_AIP_FREE			_IOWR('P', 2, int)


struct jz_aip_chainbuf {
	void *vaddr;
	unsigned int paddr;
	unsigned int size;
};

#endif

