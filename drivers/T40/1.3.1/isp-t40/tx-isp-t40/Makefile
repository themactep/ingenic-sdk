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
	@if test "mips-linux-gnu-gcc (Ingenic Linux-Release4.1.9-Default_xburst2_glibc2.26 refer to 5.1.9.smaller2.3 2023.09-21 10:28:34) 7.2.0" = "`$(CROSS_COMPILE)gcc --version | head -n1`"; \
	then \
		echo "现在正在编译toolchain R4.1.9的ko."; \
		cp libt40-firmware-r419.a libt40-firmware.a; \
		$(MAKE) -C $(KDIR) M=$(shell pwd) $@; \
	elif test "mips-linux-gnu-gcc (Ingenic Linux-Release5.1.11-Default_xburst2_glibc2.29 support backtrace 2024.01-31 03:00:50) 7.2.0" = "`$(CROSS_COMPILE)gcc --version | head -n1`"; \
	then \
		echo "现在正在编译toolchain R5.1.11的ko."; \
		cp libt40-firmware-r5111.a libt40-firmware.a; \
		$(MAKE) -C $(KDIR) M=$(shell pwd) $@; \
	else \
		echo "工具链版本匹配失败，不支持当前工具链！！！"; \
	fi

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
