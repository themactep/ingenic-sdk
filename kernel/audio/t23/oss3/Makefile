CROSS_COMPILE ?= mips-linux-gnu-

KDIR := $(PWD)/../../../kernel

MODULE_NAME := audio

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include -w

$(MODULE_NAME)-objs := audio_dsp.o audio_debug.o

$(MODULE_NAME)-objs += host/T23/audio_aic.o
$(MODULE_NAME)-objs += inner_codecs/T23/codec.o
$(MODULE_NAME)-objs += boards/t23_platform.o

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@
