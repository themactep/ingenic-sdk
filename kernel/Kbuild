ifeq ($(SOC_FAMILY),)
$(error SOC_FAMILY missing)
else
$(info Bulding for SoC $(SOC_FAMILY))
endif

ccflags-y := -DRELEASE -DUSER_BIT_32 -DKERNEL_BIT_32 -Wno-date-time -D_GNU_SOURCE
ccflags-y += -I$(src)/isp/$(SOC_FAMILY)/include
ccflags-y += -I$(src)/audio/$(SOC_FAMILY)/oss3/include

#### ALL #####
include $(src)/isp/Kbuild
include $(src)/gpio/Kbuild
include $(src)/misc/sample_pwm/Kbuild
include $(src)/misc/motor/Kbuild

#### PLATFORM ####
ifeq ($(CONFIG_SOC_T23)$(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
include $(src)/audio/$(SOC_FAMILY)/oss3/Kbuild
else
include $(src)/audio/$(SOC_FAMILY)/oss2/Kbuild
endif

ifeq ($(CONFIG_SOC_T23)$(CONFIG_SOC_T31)$(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
include $(src)/avpu/Kbuild
endif

ifeq ($(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
include $(src)/misc/mpsys-driver/Kbuild
include $(src)/misc/soc-nna/Kbuild
include $(src)/misc/jz-dtrng/Kbuild
endif

#### SENSORS ####
ifeq ($(SENSOR_MODEL),)
$(warning SENSOR_MODEL missing)
else
$(info Bulding for sensor $(SENSOR_MODEL))
include $(src)/sensors/$(SOC_FAMILY)/$(SENSOR_MODEL)/Kbuild
endif
