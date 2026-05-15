CROSS_COMPILE ?= mips-linux-uclibc-gnu-gcc

ifeq (${TASSADAR_ENV_KERNEL_DIR}, )
	TASSADAR_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94
endif

KDIR := ${TASSADAR_ENV_KERNEL_DIR}
OPENSOURCE_DIR = $(TASSADAR_ENV_OPENSOURCE_DIR)


################### name define #########
MODULE_NAME := tx-isp-PRJ007
################### name end #########

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := libPRJ007-firmware.a tx-isp-funcs.o tx-isp-module.o

obj-m := $(MODULE_NAME).o

modules:
	@if test "${KDIR}" = "${OPENSOURCE_DIR}/kernel-4.4.94";then \
		echo "现在正在编译toolchain:540 kernel:4.4.94 的ko."; \
		cp libPRJ007-firmware-4494.a libPRJ007-firmware.a; \
	else \
		echo "现在正在编译toolchain:540 kernel:3.10 的ko."; \
		cp libPRJ007-firmware-310.a libPRJ007-firmware.a; \
	fi
	@if test "${ENV_SDK_TYPE}" = "ZERATUL";then \
		cp -r include/* $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/include/; \
		cp tx-isp-funcs.c $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/; \
		cp libPRJ007-firmware.a $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/libPRJ007-firmware.hex; \
	fi
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
