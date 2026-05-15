CROSS_COMPILE ?= mips-linux-uclibc-gnu-gcc

ifeq (${TASSADAR_ENV_KERNEL_DIR}, )
	TASSADAR_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94
endif

KDIR := ${TASSADAR_ENV_KERNEL_DIR}

MODULE_NAME := audio

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := audio_dsp.o audio_debug.o

$(MODULE_NAME)-objs += host/PRJ007/audio_aic.o
$(MODULE_NAME)-objs += inner_codecs/PRJ007/codec.o
$(MODULE_NAME)-objs += host/PRJ007/dmic/as-fmtcov/as-fmtcov.o
$(MODULE_NAME)-objs += host/PRJ007/dmic/as-dbus/as-dbus.o
$(MODULE_NAME)-objs += host/PRJ007/dmic/as-dma/as-dma.o
$(MODULE_NAME)-objs += host/PRJ007/dmic/as-dmic/as-dmic.o
$(MODULE_NAME)-objs += boards/PRJ007_platform.o

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
