CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = $(PWD)/../../../kernel
endif

KDIR := ${ISVP_ENV_KERNEL_DIR}

################### name define #########
MODULE_NAME := tx-isp-t23
################### name end #########

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := libt23-firmware.a tx-isp-debug.o tx-isp-module.o

obj-m := $(MODULE_NAME).o

modules:
	@if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
		echo "现在正在编译toolchain:540 kernel:4.4.94 的ko."; \
		cp libt23-firmware-540-4494.a libt23-firmware.a; \
	elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
		echo "现在正在编译toolchain:540 kernel:3.10 的ko."; \
		cp libt23-firmware-540-310.a libt23-firmware.a; \
	else \
		echo "Kernel config not found.";\
	fi;
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
