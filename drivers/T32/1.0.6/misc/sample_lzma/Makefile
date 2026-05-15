CROSS_COMPILE ?= mips-linux-uclibc-gnu-gcc

# MODULE_NAME := back
MODULE_NAME := ingenic_lzma

obj-m    += $(MODULE_NAME).o

KDIR :=$(TASSADAR_ENV_KERNEL_DIR)

.PHONY:all clean install uninstall

all:
	make -C $(KDIR) M=$(shell pwd) modules

clean:
	make -C $(KDIR) M=$(shell pwd) clean
