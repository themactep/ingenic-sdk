CROSS_COMPILE ?= mips-linux-gnu-

ifeq (${ISVP_ENV_KERNEL_DIR}, )
	ISVP_ENV_KERNEL_DIR = ../../../kernel
endif

KDIR := ${ISVP_ENV_KERNEL_DIR}

MODULE_NAME := timer

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := jz_timer.o

default: modules

modules :
	@$(MAKE) -C $(KDIR) M=$(PWD) $@
clean :
	@$(MAKE) -C $(KDIR) M=$(PWD) $@
	-rm -f $(Target)

Target=sample_timer
CC=mips-linux-gnu-gcc

$(Target) : $(Target).o
	$(CC) -static -muclibc $(Target).c -o $@
