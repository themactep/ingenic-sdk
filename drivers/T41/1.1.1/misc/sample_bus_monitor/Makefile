

MODULE_NAME := bus_monitor

obj-m    += $(MODULE_NAME).o

CURRENT_PATH   := $(PWD)

LINUX_KERNEL_PATH   := $(PWD)/../../../kernel-4.4.94/

.PHONY:all clean install uninstall

all:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules

clean:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean
