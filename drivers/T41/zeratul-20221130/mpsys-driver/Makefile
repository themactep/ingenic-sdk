CROSS_COMPILE ?= mips-linux-gnu-

ifeq ($(ZRT_ENV_KERNEL_RELEASE), 4.4.94)
ISVP_ENV_KERNEL_DIR ?= $(PWD)/../../kernel-4.4.94
else
ISVP_ENV_KERNEL_DIR ?= $(PWD)/../../kernel
endif
KDIR := $(ISVP_ENV_KERNEL_DIR)

obj-m := mpsys.o
mpsys-objs := mpsys_module.o private-funcs.o libmpsys-firmware.a


EXTRA_CFLAGS += -I$(PWD)/include

all: modules

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
