CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = /home/hyli/fpga-project/fpga_kernel
endif

ISP_DRIVER_DIR = $(PWD)/../../tx-isp/

KDIR := ${ISVP_ENV_KERNEL_DIR}

############ name define #############
include $(PWD)/../config.mk
MODULE_NAME := sensor_ps5258_$(SOC_TYPE)
############ name end #############


KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := ps5258.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
