CROSS_COMPILE ?= mips-linux-uclibc-gnu-gcc

KDIR := $(TASSADAR_ENV_KERNEL_DIR)

MODULE_NAME := des_dev

all: modules

.PHONY: modules clean
ifneq ($(CONFIG_SOC_PRJ007),)
$(MODULE_NAME)-objs := jz-des.o
else
$(info PRJ008 not support des)
endif
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
