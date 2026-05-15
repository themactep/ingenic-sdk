

MODULE_NAME := bus_monitor

obj-m    += $(MODULE_NAME).o

CURRENT_PATH   := $(PWD)

LINUX_KERNEL_PATH   := $(ISVP_ENV_KERNEL_DIR)

.PHONY:all clean install uninstall

all:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
