CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = ../../../kernel
	#ISVP_ENV_KERNEL_DIR = ../../../kernel-4.4.94
	#ISVP_ENV_KERNEL_DIR = ../../../kernel-6.1
endif

KDIR := ${ISVP_ENV_KERNEL_DIR}
MODULE_NAME := sample_timer

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := sample_timer_main.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd *.mod *.mod.c .tmp_versions *.ko *.symvers modules.order
