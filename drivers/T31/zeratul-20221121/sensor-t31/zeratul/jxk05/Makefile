CROSS_COMPILE ?= mips-linux-gnu-

TARGET=jxk05

KDIR := $(PWD)/../../../../../os/kernel/

############ name define #############
MODULE_NAME := sensor_$(TARGET)_t31
############ name end #############

EXTRA_CFLAGS += -I$(PWD)/../include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := $(TARGET).o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order

release:
	cp $(TARGET).o $(KDIR)/arch/mips/xburst/lib/isp-t31/$(TARGET).hex
