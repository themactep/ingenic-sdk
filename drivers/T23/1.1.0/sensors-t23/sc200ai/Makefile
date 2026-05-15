CROSS_COMPILE ?= mips-linux-gnu-

KDIR := ${ISVP_ENV_KERNEL_DIR}

ISP_DRIVER_DIR = $(ISVP_ENV_DRIVERS_DIR)/isp-t23/tx-isp-t23

############ name define #############
MODULE_NAME := sensor_sc200ai_t23
############ name end #############


KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := sc200ai.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
