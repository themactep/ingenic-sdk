CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISP_ENV_KERNEL_DIR}, )
	ISP_ENV_KERNEL_DIR = $(PWD)/../../../kernel
endif

ISP_DRIVER_DIR = $(PWD)/../../isp-t21/tx-isp-t21

KDIR := ${ISP_ENV_KERNEL_DIR}

############ name define #############
ifeq (${CONFIG_SOC_T30}, y)
MODULE_NAME := sensor_sc2235_t30
endif
ifeq (${CONFIG_SOC_T20}, y)
MODULE_NAME := sensor_sc2235_t20
endif
ifeq (${CONFIG_SOC_T10}, y)
MODULE_NAME := sensor_sc2235_t10
endif
ifeq (${CONFIG_SOC_T21}, y)
MODULE_NAME := sensor_sc2235_t21
endif

############ name end #############


KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := sc2235.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
