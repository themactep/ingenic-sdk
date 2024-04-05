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
$(info Building OSS for Kernel $(KERNEL_VERSION))
ccflags-y += -I$(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss2/include
else
$(info Building OSS for Kernel $(KERNEL_VERSION))
ccflags-y += -I$(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss3/include
endif

#### ALL #####
ifeq ($(KERNEL_VERSION),3.10)
$(info Building ISP for Kernel $(KERNEL_VERSION))
include $(src)/$(KERNEL_VERSION)/isp/Kbuild
endif

$(info Building GPIO for Kernel $(KERNEL_VERSION))
include $(src)/$(KERNEL_VERSION)/gpio/Kbuild

$(info Building PWM for Kernel $(KERNEL_VERSION))
include $(src)/$(KERNEL_VERSION)/misc/sample_pwm/Kbuild

$(info Building Motor for Kernel $(KERNEL_VERSION))
include $(src)/$(KERNEL_VERSION)/misc/motor/Kbuild

#### PLATFORM ####
ifeq ($(KERNEL_VERSION),3.10)
$(info Building Audio for Kernel $(KERNEL_VERSION))
include $(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss2/Kbuild
else
$(info Building Audio for Kernel $(KERNEL_VERSION))
include $(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss3/Kbuild
endif

ifeq ($(CONFIG_SOC_T23)$(CONFIG_SOC_T31)$(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
$(info Building AVPU for Kernel $(KERNEL_VERSION))
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
