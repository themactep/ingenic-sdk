CROSS_COMPILE ?= mips-linux-gnu-

ISVP_ENV_KERNEL_DIR = $(PWD)/../../../linux-4.4.94

KDIR := ${ISVP_ENV_KERNEL_DIR}

ISP_DRIVER_DIR = $(PWD)/../../isp-t31/tx-isp-t31/

############ name define #############
MODULE_NAME := sensor_c2399_t31
############ name end #############


KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := c2399.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
