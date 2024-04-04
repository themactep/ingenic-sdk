ifeq ($(SOC_FAMILY),)
$(error SOC_FAMILY missing)
else
$(info Building for SoC $(SOC_FAMILY))
endif

ifeq ($(KERNEL_VERSION),)
$(error KERNEL_VERSION missing)
else
$(info Building for Kernel $(KERNEL_VERSION))
endif

ccflags-y := -DRELEASE -DUSER_BIT_32 -DKERNEL_BIT_32 -Wno-date-time -D_GNU_SOURCE
ccflags-y += -I$(src)/$(KERNEL_VERSION)/isp/$(SOC_FAMILY)/include
ifeq ($(KERNEL_VERSION),3.10)
ccflags-y += -I$(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss2/include
else
ccflags-y += -I$(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss3/include
endif

#### ALL #####
ifeq ($(KERNEL_VERSION),3.10)
include $(src)/$(KERNEL_VERSION)/isp/Kbuild
endif
include $(src)/$(KERNEL_VERSION)/gpio/Kbuild
include $(src)/$(KERNEL_VERSION)/misc/sample_pwm/Kbuild
include $(src)/$(KERNEL_VERSION)/misc/motor/Kbuild

#### PLATFORM ####
ifeq ($(KERNEL_VERSION),4.4)
include $(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss3/Kbuild
else
include $(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss2/Kbuild
endif

ifeq ($(CONFIG_SOC_T23)$(CONFIG_SOC_T31)$(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
include $(src)/$(KERNEL_VERSION)/avpu/Kbuild
endif

ifeq ($(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
include $(src)/$(KERNEL_VERSION)/misc/mpsys-driver/Kbuild
include $(src)/$(KERNEL_VERSION)/misc/soc-nna/Kbuild
include $(src)/$(KERNEL_VERSION)/misc/jz-dtrng/Kbuild
endif

#### SENSORS ####
ifeq ($(SENSOR_MODEL),)
$(warning SENSOR_MODEL missing)
else
$(info Building for sensor $(SENSOR_MODEL))
include $(src)/$(KERNEL_VERSION)/sensor-src/Kbuild
endif
