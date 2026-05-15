CROSS_COMPILE ?= mips-linux-gnu-

TARGET=jxf37

ifeq ($(ZRT_ENV_KERNEL_RELEASE), 4.4.94)
KDIR := $(PWD)/../../../../../os/kernel-4.4.94
else
KDIR := $(PWD)/../../../../../os/kernel
endif

############ name define #############
MODULE_NAME := sensor_$(TARGET)_t41
############ name end #############

EXTRA_CFLAGS += -I$(PWD)/../include
EXTRA_CFLAGS +=-Wno-error=date-time
EXTRA_CFLAGS +=-Wno-date-time

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := $(TARGET).o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order

release:
	cp $(TARGET).o $(KDIR)/arch/mips/xburst2/lib/isp-t41/$(TARGET).hex
