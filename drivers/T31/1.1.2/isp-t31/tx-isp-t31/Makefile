CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = $(PWD)/../../../kernel
endif

KDIR := ${ISVP_ENV_KERNEL_DIR}

################### name define #########
MODULE_NAME := tx-isp-t31
################### name end #########

all: modules

.PHONY: modules toolchain clean

toolchain:
	@if test "mips-linux-gnu-gcc (Ingenic r3.3.0-gcc540 2018.04-11) 5.4.0" != "`$(CROSS_COMPILE)gcc --version | head -n1`"; \
	then \
		cp libt31-firmware-472.a libt31-firmware.a; \
	elif test "mips-linux-gnu-gcc (Ingenic r2.3.3 2016.12) 4.7.2" != "`$(CROSS_COMPILE)gcc --version | head -n1`"; \
	then \
		cp libt31-firmware-540.a libt31-firmware.a; \
	fi


EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := libt31-firmware.a tx-isp-debug.o tx-isp-module.o

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
