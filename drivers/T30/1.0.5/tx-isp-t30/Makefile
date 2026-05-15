CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISP_ENV_KERNEL_DIR}, )
	ISP_ENV_KERNEL_DIR = $(PWD)/../../../kernel
endif

KDIR := ${ISP_ENV_KERNEL_DIR}

################### name define #########
ifeq (${CONFIG_SOC_T30}, y)
MODULE_NAME := tx-isp-t30
endif
ifeq (${CONFIG_SOC_T20}, y)
MODULE_NAME := tx-isp-t20
endif
ifeq (${CONFIG_SOC_T10}, y)
MODULE_NAME := tx-isp-t10
endif
################### name end #########

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := board/soc-t30/platform.o

$(MODULE_NAME)-objs += videoin/tx-isp-vic.o videoin/tx-isp-video-in.o videoin/tx-isp-csi.o

$(MODULE_NAME)-objs += apical-isp/tx-isp-core-tuning.o apical-isp/tx-isp-core.o \
			apical-isp/apical_dynamic_calibrations.o apical-isp/apical_static_calibrations.o \
			apical-isp/apical_calibrations.o apical-isp/apical_calibrations_init.o apical-isp/tx-isp-load-parameters.o \
			apical-isp/apical_custom_initialization.o apical-isp/ApicalDefault.o \
			apical-isp/system_isp_io.o apical-isp/log.o apical-isp/system_io.o \
			apical-isp/system_i2c.o apical-isp/system_spi.o apical-isp/system_timer.o apical-isp/system_uart.o \
			apical-isp/system_semaphore.o apical-isp/sensor_drv.o apical-isp/system_chardev.o

$(MODULE_NAME)-objs += apical-isp/libapical_isp.a

$(MODULE_NAME)-objs += txx-funcs.o tx-isp-debug.o tx-isp-videobuf.o tx-isp-interrupt.o tx-isp-ncu.o tx-isp-ldc.o\
										tx-isp-mscaler.o tx-isp-frame-channel.o tx-isp-device.o

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
