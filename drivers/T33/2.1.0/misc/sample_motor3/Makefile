CROSS_COMPILE ?= mips-linux-uclibc-gnu-

KDIR := ${TASSADAR_ENV_KERNEL_DIR}

MODULE_NAME := sample_motor

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := ms419xx_spi_dev.o motor.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd *.mod *.mod.c .tmp_versions *.ko *.symvers modules.order
