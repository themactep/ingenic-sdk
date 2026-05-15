CROSS_COMPILE ?= mips-linux-gnu-

TARGET=common

KDIR := $(PWD)/../../../../../os/kernel-4.4.94/
MODULE_NAME := sensor_common

#EXTRA_CFLAGS += -I$(PWD)/../include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := $(TARGET).o
#$(MODULE_NAME)-objs := common.o

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order


release:
	cp $(TARGET).o $(KDIR)/arch/mips/xburst2/lib/isp-t41/$(TARGET).hex
