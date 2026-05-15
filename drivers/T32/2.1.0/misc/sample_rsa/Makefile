CROSS_COMPILE ?= mips-linux-uclibc-gnu-gcc

KDIR := $(TASSADAR_ENV_KERNEL_DIR)

MODULE_NAME := rsa_dev

all: modules

.PHONY: modules clean

ifneq ($(CONFIG_SOC_PRJ007),)
$(MODULE_NAME)-objs := PRJ007_rsa/ingenic-rsa.o
SRC_DIR := PRJ007_rsa
else
$(MODULE_NAME)-objs := PRJ008_rsa/ingenic-rsa.o
SRC_DIR := PRJ008_rsa
endif

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
	@rm -f $(SRC_DIR)/*.o $(SRC_DIR)/.*.cmd
