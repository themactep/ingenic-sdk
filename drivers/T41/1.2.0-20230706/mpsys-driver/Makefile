CROSS_COMPILE ?= mips-linux-gnu-

KDIR := $(ISVP_ENV_KERNEL_DIR)

ifeq ($(findstring 4.4.94,$(KDIR)),4.4.94)
	KVERSION := 4-4-94
else
	KVERSION := 3-10-14
endif

MODULE_NAME := $(ISVP_SOC_TYPE)-mpsys-$(KVERSION)
FIRMWARE_NAME := $(ISVP_SOC_TYPE)-mpsys-firmware-$(KVERSION)

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := mpsys_module.o private-funcs.o $(FIRMWARE_NAME).a


EXTRA_CFLAGS += -I$(PWD)/include


all: modules

modules:
	@echo "目前编译驱动使用的内核版本是Kernel-$(KVERSION)"
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
