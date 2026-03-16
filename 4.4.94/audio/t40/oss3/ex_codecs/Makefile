CROSS_COMPILE ?= mips-linux-gnu-

CODEC_NAME=es8374

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = $(PWD)/../../../../kernel
endif

KDIR := ${ISVP_ENV_KERNEL_DIR}
MODULE_NAME := $(CODEC_NAME)

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/../include

$(MODULE_NAME)-objs := $(CODEC_NAME)_codec.o


obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
