CROSS_COMPILE ?= mips-linux-gnu-

KDIR := ${ZRT_ENV_KERNEL_DIR}

obj-m := mpsys.o
ifeq ($(ZRT_ENV_KERNEL_RELEASE), 4.4.94)
mpsys-objs := mpsys_module.o private-funcs.o T41-mpsys-firmware-4-4-94.a
else
mpsys-objs := mpsys_module.o private-funcs.o T41-mpsys-firmware-3-10-14.a
endif


EXTRA_CFLAGS += -I$(PWD)/include

all: modules

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
