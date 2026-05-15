CROSS_COMPILE ?= mips-linux-gnu-

KDIR := $(PWD)/../../../kernel-4.4.94

MODULE_NAME := dtrng_dev

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := jz-dtrng.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
