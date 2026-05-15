CROSS_COMPILE ?= mips-linux-gnu-

KDIR := ${ISVP_ENV_KERNEL_DIR}

#ISP_DRIVER_DIR = $(ISVP_ENV_DRIVERS_DIR)/isp-t23/tx-isp-t23
ISP_DRIVER_DIR = $(ISVP_ENV_DRIVERS_DIR)/isp-t23/tx-isp-t23-double

############ name define #############
MODULE_NAME := sensor_cv2003_t23
MODULE_NAMES1 := sensor_cv2003s1_t23
############ name end #############


KBUILD_EXTRA_SYMBOLS := $(ISP_DRIVER_DIR)/Module.symvers
EXTRA_CFLAGS += -I$(ISP_DRIVER_DIR)/include

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := cv2003.o
$(MODULE_NAMES1)-objs := cv2003s1.o
obj-m := $(MODULE_NAME).o $(MODULE_NAMES1).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
