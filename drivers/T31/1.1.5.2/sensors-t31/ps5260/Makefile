CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = /home/hyli/fpga-project/fpga_kernel
endif

ISP_DRIVER_DIR = $(PWD)/../../isp-t31/tx-isp-t31/

KDIR := ${ISVP_ENV_KERNEL_DIR}

############ name define #############
ifeq (${CONFIG_SOC_T30}, y)
MODULE_NAME := sensor_ps5260_t30
endif
ifeq (${CONFIG_SOC_T20}, y)
MODULE_NAME := sensor_ps5260_t20
endif
ifeq (${CONFIG_SOC_T10}, y)
MODULE_NAME := sensor_ps5260_t10
endif
ifeq (${CONFIG_SOC_T31}, y)
MODULE_NAME := sensor_ps5260_t31
endif
############ name end #############
MODULE_NAME := sensor_ps5260_t31


KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := ps5260.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
