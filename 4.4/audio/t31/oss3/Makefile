CROSS_COMPILE ?= mips-linux-gnu-

#ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94
#endif

KDIR := ${ISVP_ENV_KERNEL_DIR}

MODULE_NAME := audio

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := audio_dsp.o audio_debug.o

ifeq (${CONFIG_SOC_T31}, y)
	$(MODULE_NAME)-objs += host/audio_aic.o host/audio_dmic.o
	$(MODULE_NAME)-objs += inner_codecs/codec.o
	$(MODULE_NAME)-objs += boards/t31_platform.o
endif

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
