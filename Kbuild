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

#### ALL #####

ifneq ($(CONFIG_SOC_A1),y)
$(info Building ISP for Kernel $(KERNEL_VERSION))
include $(src)/$(KERNEL_VERSION)/isp/Kbuild

ifeq ($(KERNEL_VERSION),3.10)
    $(info Building GPIO-UserKeys for Kernel $(KERNEL_VERSION))
    include $(src)/$(KERNEL_VERSION)/misc/gpio-userkeys/Kbuild
endif


# Build TCU allocator (central ownership registry) for 3.10
ifeq ($(KERNEL_VERSION),3.10)
    $(info Building TCU allocator for Kernel $(KERNEL_VERSION))
    include $(src)/$(KERNEL_VERSION)/misc/tcu_alloc/Kbuild
endif

ifeq ($(KERNEL_VERSION),3.10)
    $(info Building PWM for Kernel $(KERNEL_VERSION))
    include $(src)/$(KERNEL_VERSION)/misc/pwm/Kbuild
endif

ifeq ($(BR2_THINGINO_MOTORS),y)
    $(info Building Motor for Kernel $(KERNEL_VERSION))
    include $(src)/$(KERNEL_VERSION)/misc/motor/Kbuild
    ifeq ($(BR2_THINGINO_MOTORS_SPI),y)
        ifeq ($(KERNEL_VERSION),3.10)
            $(info Building Motor SPI for Kernel $(KERNEL_VERSION))
            include $(src)/$(KERNEL_VERSION)/misc/ms419xx/Kbuild
        endif
    endif
endif

endif

#### PLATFORM ####
# Check if building for SOC T23, which overrides other conditions
ifeq ($(CONFIG_SOC_T23),y)
    $(info Building Audio for SOC T23 using oss)
    include $(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss3/Kbuild

# Handle other kernel versions and SOC configurations
else
    # Check for kernel version 3.10
    ifeq ($(KERNEL_VERSION),3.10)
        $(info Building Audio for Kernel $(KERNEL_VERSION) using oss2)
        include $(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss2/Kbuild
    else
        # Default to oss3 for any other kernel version
        $(info Building Audio for Kernel $(KERNEL_VERSION) using oss3)
        include $(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/oss3/Kbuild
    endif
endif

ifeq ($(CONFIG_SOC_T31)$(CONFIG_SOC_C100)$(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
    $(info Building AVPU for Kernel $(KERNEL_VERSION))
    ifeq ($(CONFIG_SOC_T31)$(CONFIG_SOC_C100),y)
        include $(src)/$(KERNEL_VERSION)/avpu/t31/Kbuild
    endif
    ifeq ($(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
        include $(src)/$(KERNEL_VERSION)/avpu/t40/Kbuild
    endif
endif

ifeq ($(CONFIG_SOC_T40)$(CONFIG_SOC_T41)$(CONFIG_SOC_A1),y)
    include $(src)/$(KERNEL_VERSION)/misc/soc-nna/Kbuild
endif

ifeq ($(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
    include $(src)/$(KERNEL_VERSION)/misc/mpsys-driver/Kbuild
    include $(src)/$(KERNEL_VERSION)/misc/jz-dtrng/Kbuild
endif

#### SENSORS ####
ifneq ($(CONFIG_SOC_A1),y)
ifeq ($(strip $(SENSOR_1_MODEL)$(SENSOR_2_MODEL)),)
    $(info Sensor models missing, building sinfo module)
    include $(src)/sinfo/Kbuild
else
ifneq ($(SENSOR_1_MODEL),)
    $(info Building for sensor $(SENSOR_2_MODEL))
    include $(src)/$(KERNEL_VERSION)/sensor-src/Kbuild
endif
ifneq ($(SENSOR_2_MODEL),)
    $(info Building for sensor $(SENSOR_1_MODEL))
    include $(src)/$(KERNEL_VERSION)/sensor-src/Kbuild
endif
endif
endif

#### A1 ######
ifeq ($(CONFIG_SOC_A1),y)
include $(src)/$(KERNEL_VERSION)/aip/a1/Kbuild
include $(src)/$(KERNEL_VERSION)/fb/Kbuild
include $(src)/$(KERNEL_VERSION)/ipu/Kbuild
include $(src)/$(KERNEL_VERSION)/video/a1/vde/Kbuild
include $(src)/$(KERNEL_VERSION)/video/a1/vdec/Kbuild
include $(src)/$(KERNEL_VERSION)/audio/$(SOC_FAMILY)/hdmi_audio/Kbuild
endif
