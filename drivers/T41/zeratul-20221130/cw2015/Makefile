CROSS_COMPILE ?= mips-linux-gnu-

ifeq ($(ZRT_ENV_KERNEL_RELEASE), 4.4.94)
KDIR := $(PWD)/../../kernel-4.4.94
else
KDIR := $(PWD)/../../kernel
endif
MODULE_NAME := pm_cw2015

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := cw2015.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
