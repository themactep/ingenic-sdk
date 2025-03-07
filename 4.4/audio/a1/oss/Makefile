CROSS_COMPILE ?= mips-v720s229-linux-gnu-

ISVP_ENV_KERNEL_DIR ?= $(PWD)/../../kernel

KDIR := ${ISVP_ENV_KERNEL_DIR}

MODULE_NAME := audio

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := audio_dsp.o audio_debug.o

$(MODULE_NAME)-objs += host/A1/audio_aic.o
$(MODULE_NAME)-objs += inner_codecs/A1/codec.o
$(MODULE_NAME)-objs += boards/a1_platform.o

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
