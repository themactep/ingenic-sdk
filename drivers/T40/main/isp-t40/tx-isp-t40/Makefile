CROSS_COMPILE ?= mips-linux-gnu-

ISVP_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94/

KDIR := ${ISVP_ENV_KERNEL_DIR}

################### name define #########
MODULE_NAME := tx-isp-t40
################### name end #########

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := libt40-firmware.a tx-isp-funcs.o tx-isp-module.o

obj-m := $(MODULE_NAME).o

modules:
	@echo "现在正在编译toolchain 720 r511 的ko.";
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
