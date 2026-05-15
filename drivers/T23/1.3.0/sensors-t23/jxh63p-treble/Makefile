CROSS_COMPILE ?= mips-linux-gnu-

KDIR := ${ISVP_ENV_KERNEL_DIR}

ISP_DRIVER_DIR = $(ISVP_ENV_DRIVERS_DIR)/isp-t23/tx-isp-t23-double

############ name define #############
MODULE_NAME := sensor_jxh63p_t23
MODULE_NAMES1 := sensor_jxh63ps1_t23
MODULE_NAMES2 := sensor_jxh63ps2_t23
############ name end #############


KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := jxh63p.o
$(MODULE_NAMES1)-objs := jxh63ps1.o
$(MODULE_NAMES2)-objs := jxh63ps2.o
obj-m := $(MODULE_NAME).o $(MODULE_NAMES1).o $(MODULE_NAMES2).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
