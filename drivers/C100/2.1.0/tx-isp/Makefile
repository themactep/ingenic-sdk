CROSS_COMPILE ?= mips-linux-gnu-

ISVP_ENV_KERNEL_DIR ?= $(PWD)/../../kernel/
KDIR := ${ISVP_ENV_KERNEL_DIR}

ifeq (${CONFIG_SOC_T31}, y)
SOC_TYPE := t31
else ifeq (${CONFIG_SOC_C100}, y)
SOC_TYPE := c100
endif

################### name define #########
MODULE_NAME := tx-isp-$(SOC_TYPE)
################### name end #########
$(MODULE_NAME)-objs := libisp.a tx-isp-debug.o tx-isp-module.o
obj-m := $(MODULE_NAME).o

all: modules

.PHONY: modules clean $(obj)/libisp.a

EXTRA_CFLAGS += -I$(PWD)/include

$(obj)/libisp.a:
	@if test "mips-linux-gnu-gcc (Ingenic r3.3.0-gcc540 2018.04-11) 5.4.0" = "`$(CROSS_COMPILE)gcc --version | head -n1`"; \
	then \
		echo "目前正在使用的工具链是toolchain 540."; \
		ln -sf lib$(SOC_TYPE)-firmware-540.a   $(obj)/libisp.a; \
	elif test "mips-linux-gnu-gcc (Ingenic r2.3.3 2016.12) 4.7.2" = "`$(CROSS_COMPILE)gcc --version | head -n1`"; \
	then \
		echo "目前正在使用的工具链是toolchain 472."; \
		ln -sf lib$(SOC_TYPE)-firmware-472.a   $(obj)/libisp.a; \
	fi

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
	@rm -rf libisp.a
