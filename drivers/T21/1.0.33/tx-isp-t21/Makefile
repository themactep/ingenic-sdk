CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISP_ENV_KERNEL_DIR}, )
	ISP_ENV_KERNEL_DIR = $(PWD)/../../../kernel
endif

KDIR := ${ISP_ENV_KERNEL_DIR}

################### name define #########
MODULE_NAME := tx-isp-t21
################### name end #########

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)
EXTRA_CFLAGS += -I$(PWD)/include
$(MODULE_NAME)-objs = tx-isp-debug.o txx-funcs.o libt21-firmware.a tx-isp-module.o

obj-m := $(MODULE_NAME).o

modules: 
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
