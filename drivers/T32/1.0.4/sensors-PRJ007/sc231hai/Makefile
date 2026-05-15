CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${TASSADAR_ENV_KERNEL_DIR}, )
	TASSADAR_ENV_KERNEL_DIR = $(PWD)/../../../kernel
endif

KDIR := ${TASSADAR_ENV_KERNEL_DIR}
DIR_NAME = $(shell basename ${PWD})

ISP_DRIVER_DIR = $(PWD)/../../isp-PRJ007/tx-isp-PRJ007/

############ name define #############
MODULE_NAME := sensor_$(DIR_NAME)_PRJ007
############ name end #############

KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := $(DIR_NAME).o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

release:
	cp $(DIR_NAME).o $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/sensor/$(DIR_NAME).hex

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
