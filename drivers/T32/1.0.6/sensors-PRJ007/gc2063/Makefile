CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${TASSADAR_ENV_KERNEL_DIR}, )
	TASSADAR_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94
endif

KDIR := ${TASSADAR_ENV_KERNEL_DIR}
DIR_NAME = $(shell basename ${PWD})

ISP_DRIVER_DIR = $(PWD)/../../isp-PRJ007/tx-isp-PRJ007/

KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include
EXTRA_CFLAGS += -DTVERSION='"V20250106"'
EXTRA_CFLAGS += -DAUTHOR='"Ingenic"'

all: modules

.PHONY: modules clean

FILE_NAME := $(DIR_NAME)
ifneq ($(wildcard $(PWD)/$(FILE_NAME).c),)
    MODULE_NAME := sensor_$(FILE_NAME)_PRJ007
    $(MODULE_NAME)_files := $(FILE_NAME).c
    $(MODULE_NAME)_existing := $(wildcard $(MODULE_NAME)_files)
    obj-m += $(if $(MODULE_NAME)_existing,$(MODULE_NAME).o)
    $(MODULE_NAME)-objs := $(patsubst %.c,%.o,$(FILE_NAME).o)
endif

FILE_NAME := $(DIR_NAME)s1
ifneq ($(wildcard $(PWD)/$(FILE_NAME).c),)
    MODULE_NAME := sensor_$(FILE_NAME)_PRJ007
    $(MODULE_NAME)_files := $(FILE_NAME).c
    $(MODULE_NAME)_existing := $(wildcard $(MODULE_NAME)_files)
    obj-m += $(if $(MODULE_NAME)_existing,$(MODULE_NAME).o)
    $(MODULE_NAME)-objs := $(patsubst %.c,%.o,$(FILE_NAME).o)
endif

FILE_NAME := $(DIR_NAME)s2
ifneq ($(wildcard $(PWD)/$(FILE_NAME).c),)
    MODULE_NAME := sensor_$(FILE_NAME)_PRJ007
    $(MODULE_NAME)_files := $(FILE_NAME).c
    $(MODULE_NAME)_existing := $(wildcard $(MODULE_NAME)_files)
    obj-m += $(if $(MODULE_NAME)_existing,$(MODULE_NAME).o)
    $(MODULE_NAME)-objs := $(patsubst %.c,%.o,$(FILE_NAME).o)
endif

FILE_NAME := $(DIR_NAME)s3
ifneq ($(wildcard $(PWD)/$(FILE_NAME).c),)
    MODULE_NAME := sensor_$(FILE_NAME)_PRJ007
    $(MODULE_NAME)_files := $(FILE_NAME).c
    $(MODULE_NAME)_existing := $(wildcard $(MODULE_NAME)_files)
    obj-m += $(if $(MODULE_NAME)_existing,$(MODULE_NAME).o)
    $(MODULE_NAME)-objs := $(patsubst %.c,%.o,$(FILE_NAME).o)
endif

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

release:
	@if [ -e $(DIR_NAME).o ]; then \
		cp $(DIR_NAME).o $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/sensor/$(DIR_NAME).hex; \
	fi
	@if [ -e $(DIR_NAME)s1.o ]; then \
		cp $(DIR_NAME)s1.o $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/sensor/$(DIR_NAME)s1.hex; \
	fi
	@if [ -e $(DIR_NAME)s2.o ]; then \
		cp $(DIR_NAME)s2.o $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/sensor/$(DIR_NAME)s2.hex; \
	fi
	@if [ -e $(DIR_NAME)s3.o ]; then \
		cp $(DIR_NAME)s3.o $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/sensor/$(DIR_NAME)s3.hex; \
	fi

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
