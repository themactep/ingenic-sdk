CROSS_COMPILE ?= mips-linux-gnu-

TARGET=gc2053
TARGETS1=gc2053s1

KDIR := $(ZRT_ENV_KERNEL_DIR)

############ name define #############
MODULE_NAME := sensor_$(TARGET)_t23
MODULE_NAMES1 := sensor_$(TARGETS1)_t23
############ name end #############

EXTRA_CFLAGS += -I$(PWD)/../include_double

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := $(TARGET).o
$(MODULE_NAMES1)-objs := $(TARGETS1).o
obj-m := $(MODULE_NAME).o  $(MODULE_NAMES1).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order

release:
	cp $(TARGET).o $(KDIR)/arch/mips/xburst/lib/isp-t23/$(TARGET).hex
	cp $(TARGETS1).o $(KDIR)/arch/mips/xburst/lib/isp-t23/$(TARGETS1).hex
