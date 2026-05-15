CROSS_COMPILE ?= mips-linux-gnu-

KDIR := ${ISVP_ENV_KERNEL_DIR}
DIR_NAME = $(shell basename ${PWD} | grep -o "^[a-zA-Z0-9]*")

ISP_DRIVER_DIR = $(ISVP_ENV_DRIVERS_DIR)/isp-t23/tx-isp-t23-double

############ name define #############
MODULE_NAME := sensor_$(DIR_NAME)_t23
MODULE_NAMES1 := sensor_$(DIR_NAME)s1_t23
############ name end #############

KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := $(DIR_NAME).o
$(MODULE_NAMES1)-objs := $(DIR_NAME)s1.o
obj-m := $(MODULE_NAME).o $(MODULE_NAMES1).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
