CROSS_COMPILE ?= mips-v720s229-linux-gnu-

ISVP_ENV_KERNEL_DIR ?= $(PWD)/../../../kernel/

KDIR := ${ISVP_ENV_KERNEL_DIR}

MODULE_NAME := vdec

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := libvdec-firmware.a vdec-funcs.o vdec-module.o

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
