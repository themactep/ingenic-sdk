CROSS_COMPILE ?= mips-linux-gnu-

TARGET=gc2063
TARGET1=gc2063s1

ifeq ($(ZRT_ENV_KERNEL_RELEASE), 4.4.94)
KDIR := $(PWD)/../../../../../os/kernel-4.4.94
else
KDIR := $(PWD)/../../../../../os/kernel
endif

############ name define #############
MODULE_NAME := sensor_$(TARGET)_t41
MODULE_NAMES1 := sensor_$(TARGET1)_t41
############ name end #############

EXTRA_CFLAGS += -I$(PWD)/../include_double
EXTRA_CFLAGS +=-Wno-error=date-time
EXTRA_CFLAGS +=-Wno-date-time

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := $(TARGET).o
$(MODULE_NAMES1)-objs := $(TARGET1).o
obj-m := $(MODULE_NAME).o $(MODULE_NAMES1).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order

release:
	cp $(TARGET).o $(KDIR)/arch/mips/xburst2/lib/isp-t41/dual_sensor/$(TARGET).hex
	cp $(TARGETS1).o $(KDIR)/arch/mips/xburst2/lib/isp-t41/dual_sensor/$(TARGETS1).hex
