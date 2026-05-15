
CROSS_COMPILE ?= mips-linux-uclibc-gnu-

KDIR := ${TASSADAR_ENV_KERNEL_DIR}

MODULE_NAME := motor

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := stepper_motor2.o

default: modules

modules :
	@$(MAKE) -C $(KDIR) M=$(PWD) $@
clean :
	@$(MAKE) -C $(KDIR) M=$(PWD) $@
	-rm -f $(Target)

