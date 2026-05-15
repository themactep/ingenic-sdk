ifeq (${ISVP_ENV_KERNEL_DIR}, )
ISVP_ENV_KERNEL_DIR := $(PWD)/../../../kernel
endif

KDIR := ${ISVP_ENV_KERNEL_DIR}
include $(KDIR)/.config

ifeq ($(CONFIG_SOC_A1), y)
CROSS_COMPILE ?= mips-v720s229-linux-gnu-
endif

MODULE_NAME := ipu

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := libipu-firmware.a private-funcs.o ipu-module.o

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
