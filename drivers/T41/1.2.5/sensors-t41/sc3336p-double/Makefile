CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94
endif

KDIR := ${ISVP_ENV_KERNEL_DIR}

ISP_DRIVER_DIR = $(PWD)/../../isp-t41/tx-isp-t41-double/

############ name define #############
MODULE_NAME := sensor_sc3336p_t41
MODULE_NAMES1 := sensor_sc3336ps1_t41
############ name end #############

KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := sc3336p.o
$(MODULE_NAMES1)-objs := sc3336ps1.o
obj-m := $(MODULE_NAME).o $(MODULE_NAMES1).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
