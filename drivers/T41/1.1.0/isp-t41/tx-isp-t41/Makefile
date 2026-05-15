CROSS_COMPILE ?= mips-linux-gnu-

# ISVP_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94/
KDIR := ${ISVP_ENV_KERNEL_DIR}

################### name define #########
MODULE_NAME := tx-isp-t41
################### name end #########

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := libt41-firmware.a tx-isp-funcs.o tx-isp-module.o

obj-m := $(MODULE_NAME).o

modules:
	@if test "${ISVP_ENV_KERNEL_DIR}" = "${ISVP_ENV_OPENSOURCE_DIR}/kernel-4.4.94";then \
		echo "现在正在编译toolchain:720r511 kernel:4.4.94 的ko."; \
		cp libt41-firmware-4494.a libt41-firmware.a; \
	else \
		echo "现在正在编译toolchain:720r511 kernel:3.10 的ko."; \
		cp libt41-firmware-310.a libt41-firmware.a; \
	fi
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
