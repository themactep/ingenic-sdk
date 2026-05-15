
CROSS_COMPILE ?= mips-linux-gnu-

ISVP_ENV_KERNEL_DIR=../../../kernel-4.4.94
KDIR := ${ISVP_ENV_KERNEL_DIR}

MODULE_NAME := motor

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := stepper_motor.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

Target=step_test
CC=mips-linux-gnu-gcc

$(Target) : $(Target).o
	$(CC) -static -muclibc $(Target).c -o $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions  *.symvers modules.order

