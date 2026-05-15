
CROSS_COMPILE ?= mips-linux-gnu-

ISVP_ENV_KERNEL_DIR=../../../kernel-4.4.94
KDIR := ${ISVP_ENV_KERNEL_DIR}

MODULE_NAME := motor

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := stepper_motor.o

default: modules

modules :
	@$(MAKE) -C $(KDIR) M=$(PWD) $@
clean :
	@$(MAKE) -C $(KDIR) M=$(PWD) $@
	-rm -f $(Target)

Target=step_test
CC=mips-linux-gnu-gcc

$(Target) : $(Target).o
	$(CC) -static -muclibc $(Target).c -o $@

