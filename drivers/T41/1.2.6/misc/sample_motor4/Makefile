CROSS_COMPILE ?= mips-linux-gnu-

#KDIR := ${ISVP_ENV_KERNEL_DIR}
KDIR := $(ISVP_ENV_KERNEL_DIR)

MODULE_NAME := sample_motor4

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := tmi8152_spi_dev.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd *.mod *.mod.c .tmp_versions *.ko *.symvers modules.order
