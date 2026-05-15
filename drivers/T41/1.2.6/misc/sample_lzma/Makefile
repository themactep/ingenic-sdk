CROSS_COMPILE ?= mips-linux-gnu-

MODULE_NAME := jz_lzma

obj-m    += $(MODULE_NAME).o

KDIR := ${ISVP_ENV_KERNEL_DIR}

.PHONY:all clean install uninstall

all:
	make -C $(KDIR) M=$(shell pwd) modules

clean:
	make -C $(KDIR) M=$(shell pwd) clean
