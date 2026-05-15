CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94
endif

KDIR := ${ISVP_ENV_KERNEL_DIR}

ISP_DRIVER_DIR = $(PWD)/../../isp-t40/tx-isp-t40/

############ name define #############
MODULE_NAME := sensor_ov9732_t40
MODULE_NAMES1 := sensor_ov9732s1_t40
############ name end #############


KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := ov9732.o
$(MODULE_NAMES1)-objs := ov9732s1.o
obj-m := $(MODULE_NAME).o $(MODULE_NAMES1).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
