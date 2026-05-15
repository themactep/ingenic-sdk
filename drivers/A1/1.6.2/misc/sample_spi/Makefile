CROSS_COMPILE ?= mips-v720s229-linux-gnu-

ISVP_ENV_KERNEL_DIR ?= $(PWD)/../../../kernel

KDIR := $(ISVP_ENV_KERNEL_DIR)

MODULE_NAME := jzspiflash

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := jz_spi_norflash.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
