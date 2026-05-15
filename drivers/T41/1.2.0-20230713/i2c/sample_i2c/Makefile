CROSS_COMPILE ?= mips-linux-gnu-

KDIR := /home_d/jszhang/work/isvp/opensource/kernel-4.4.94
MODULE_NAME := rtc_i2c

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := rtc_bm8563.o
obj-m := $(MODULE_NAME).o

modules:
	@if test "$(KDIR)" = ""; \
	then \
		echo "**************************************"; \
		echo "Please run env_setup.sh at top of isvp"; \
		echo "**************************************"; \
	fi
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@
clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
	@rm -f sample_sinfo
