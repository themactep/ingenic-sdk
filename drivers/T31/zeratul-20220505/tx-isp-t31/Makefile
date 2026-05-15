CROSS_COMPILE ?= mips-linux-gnu-
AR = $(CROSS_COMPILE)ar cr

KDIR := $(PWD)/../../kernel/

################### name define #########
MODULE_NAME := libtxisp-t31
TARGET := libtxisp-t31.hex
################### name end #########

all: modules

.PHONY: modules clean release

EXTRA_CFLAGS += -I$(PWD)/include

ifeq ($(ZRT_ENV_TOOL_CHAIN), 5.4.0)
	$(MODULE_NAME)-objs += libt31-firmware-540.hex 
else
	$(MODULE_NAME)-objs += libt31-firmware.hex 
endif

$(MODULE_NAME)-objs += tx-isp-fast.o tx-isp-debug.o

obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@
	$(AR) $(TARGET) $($(MODULE_NAME)-objs)

clean:
	rm -f $(TARGET)
	@$(MAKE) -C $(KDIR) M=$(PWD) clean

release: $(TARGET)
ifeq ($(ZRT_ENV_TOOL_CHAIN), 5.4.0)
	cp $(TARGET) ${KDIR}/arch/mips/xburst/lib/isp-t31/libtxisp-t31-540.hex
else
	cp $(TARGET) ${KDIR}/arch/mips/xburst/lib/isp-t31/libtxisp-t31.hex
endif
