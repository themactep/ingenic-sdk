CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${TASSADAR_ENV_KERNEL_DIR}, )
	TASSADAR_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94
endif

KDIR := ${TASSADAR_ENV_KERNEL_DIR}
DIR_NAME = $(shell basename ${PWD})

ISP_DRIVER_DIR = $(PWD)/../../isp/tx-isp/

KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include
EXTRA_CFLAGS += -DTVERSION='"V20251027"'
EXTRA_CFLAGS += -DAUTHOR='"Ingenic"'
KERNEL_CONFIG := $(KDIR)/.config
ifneq ($(wildcard $(KERNEL_CONFIG)),)
-include $(KDIR)/.config
else
    $(error $(KERNEL_CONFIG) does not exist!)
endif

all: modules

.PHONY: modules clean

ifeq ($(CONFIG_SOC_PRJ008),y)
    PROJECT_SOC_TYPE := PRJ008
    SOURCE_DIR := PRJ008
    ifeq ($(wildcard $(PWD)/$(SOURCE_DIR)/$(DIR_NAME).c),)
        $(error PRJ008/$(DIR_NAME).c does not exist! T33 does not support this sensor)
    endif
else ifeq ($(CONFIG_SOC_PRJ007),y)
    PROJECT_SOC_TYPE := PRJ007
    SOURCE_DIR := PRJ007
    ifeq ($(wildcard $(PWD)/$(SOURCE_DIR)/$(DIR_NAME).c),)
        $(error $(DIR_NAME).c does not exist! T32 does not support this sensor)
    endif
endif


FILE_NAME := $(DIR_NAME)
ifneq ($(wildcard $(PWD)/$(SOURCE_DIR)/$(FILE_NAME).c),)
    MODULE_NAME := sensor_$(FILE_NAME)_$(PROJECT_SOC_TYPE)
    $(MODULE_NAME)_files := $(SOURCE_DIR)/$(FILE_NAME).c
    $(MODULE_NAME)_existing := $(wildcard $($(MODULE_NAME)_files))
    obj-m += $(if $(MODULE_NAME)_existing,$(MODULE_NAME).o)
    $(MODULE_NAME)-objs := $(patsubst %.c,%.o,$(SOURCE_DIR)/$(FILE_NAME).o)
endif

FILE_NAME := $(DIR_NAME)s1
ifneq ($(wildcard $(PWD)/$(SOURCE_DIR)/$(FILE_NAME).c),)
    MODULE_NAME := sensor_$(FILE_NAME)_$(PROJECT_SOC_TYPE)
    $(MODULE_NAME)_files := $(SOURCE_DIR)/$(FILE_NAME).c
    $(MODULE_NAME)_existing := $(wildcard $($(MODULE_NAME)_files))
    obj-m += $(if $(MODULE_NAME)_existing,$(MODULE_NAME).o)
    $(MODULE_NAME)-objs := $(patsubst %.c,%.o,$(SOURCE_DIR)/$(FILE_NAME).o)
endif

FILE_NAME := $(DIR_NAME)s2
ifneq ($(wildcard $(PWD)/$(SOURCE_DIR)/$(FILE_NAME).c),)
    MODULE_NAME := sensor_$(FILE_NAME)_$(PROJECT_SOC_TYPE)
    $(MODULE_NAME)_files := $(SOURCE_DIR)/$(FILE_NAME).c
    $(MODULE_NAME)_existing := $(wildcard $($(MODULE_NAME)_files))
    obj-m += $(if $(MODULE_NAME)_existing,$(MODULE_NAME).o)
    $(MODULE_NAME)-objs := $(patsubst %.c,%.o,$(SOURCE_DIR)/$(FILE_NAME).o)
endif

FILE_NAME := $(DIR_NAME)s3
ifneq ($(wildcard $(PWD)/$(SOURCE_DIR)/$(FILE_NAME).c),)
    MODULE_NAME := sensor_$(FILE_NAME)_$(PROJECT_SOC_TYPE)
    $(MODULE_NAME)_files := $(SOURCE_DIR)/$(FILE_NAME).c
    $(MODULE_NAME)_existing := $(wildcard $($(MODULE_NAME)_files))
    obj-m += $(if $(MODULE_NAME)_existing,$(MODULE_NAME).o)
    $(MODULE_NAME)-objs := $(patsubst %.c,%.o,$(SOURCE_DIR)/$(FILE_NAME).o)
endif

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

release:
	@if [ -e sensor_$(DIR_NAME)_$(PROJECT_SOC_TYPE).o ]; then \
		cp sensor_$(DIR_NAME)_$(PROJECT_SOC_TYPE).o $(KDIR)/arch/mips/xburst/lib/isp/sensor_$(PROJECT_SOC_TYPE)/$(DIR_NAME).hex; \
	fi
	@if [ -e sensor_$(DIR_NAME)s1_$(PROJECT_SOC_TYPE).o ]; then \
		cp sensor_$(DIR_NAME)s1_$(PROJECT_SOC_TYPE).o $(KDIR)/arch/mips/xburst/lib/isp/sensor_$(PROJECT_SOC_TYPE)/$(DIR_NAME)s1.hex; \
	fi
	@if [ -e sensor_$(DIR_NAME)s2_$(PROJECT_SOC_TYPE).o ]; then \
		cp sensor_$(DIR_NAME)s2_$(PROJECT_SOC_TYPE).o $(KDIR)/arch/mips/xburst/lib/isp/sensor_$(PROJECT_SOC_TYPE)/$(DIR_NAME)s2.hex; \
	fi
	@if [ -e sensor_$(DIR_NAME)s3_$(PROJECT_SOC_TYPE).o ]; then \
		cp sensor_$(DIR_NAME)s3_$(PROJECT_SOC_TYPE).o $(KDIR)/arch/mips/xburst/lib/isp/sensor_$(PROJECT_SOC_TYPE)/$(DIR_NAME)s3.hex; \
	fi

clean:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) clean
	@rm -rf *.o *~ .depend .*.cmd *.mod.c .tmp_versions *.ko *.symvers modules.order
ifneq ($(SOURCE_DIR),.)
	@rm -rf $(SOURCE_DIR)/*.o
endif
