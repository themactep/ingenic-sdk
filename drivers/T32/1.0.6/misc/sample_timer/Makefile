CROSS_COMPILE ?= mips-linux-uclibc-gnu-gcc

ifeq (${TASSADAR_ENV_KERNEL_DIR}, )
	TASSADAR_ENV_KERNEL_DIR = ../../../kernel
endif

KDIR := ${TASSADAR_ENV_KERNEL_DIR}
MODULE_NAME := sample_timer

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := sample_timer_main.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd *.mod *.mod.c .tmp_versions *.ko *.symvers modules.order
