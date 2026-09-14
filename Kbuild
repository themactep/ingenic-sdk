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

# ISP include directory, which also holds the shared sensor headers used by
# sensor-src. All ISP sources live under common/isp/<soc>; 3.10.14 t31 uses the
# t31-pp source set, 4.4.94 uses t31.
ifeq ($(KERNEL_VERSION),3.10.14)
ifeq ($(SOC_FAMILY),t31)
ISP_INCLUDE := $(src)/common/isp/t31-pp/include
else
ISP_INCLUDE := $(src)/common/isp/$(SOC_FAMILY)/include
endif
else
ISP_INCLUDE := $(src)/common/isp/$(SOC_FAMILY)/include
endif

ccflags-y := -DRELEASE -DUSER_BIT_32 -DKERNEL_BIT_32 -Wno-date-time -D_GNU_SOURCE
ccflags-y += -I$(ISP_INCLUDE)

#### ALL #####

ifneq ($(CONFIG_SOC_A1),y)
$(info Building ISP for Kernel $(KERNEL_VERSION))
include $(src)/common/isp/Kbuild

ifeq ($(KERNEL_VERSION),3.10.14)
    $(info Building GPIO-UserKeys for Kernel $(KERNEL_VERSION))
    include $(src)/common/misc/gpio-userkeys/Kbuild
endif

ifeq ($(KERNEL_VERSION),3.10.14)
    $(info Building JZ-AES for Kernel $(KERNEL_VERSION))
    include $(src)/common/misc/jz-aes/Kbuild
endif


# Build TCU allocator (central ownership registry) for 3.10.14
ifeq ($(KERNEL_VERSION),3.10.14)
    $(info Building TCU allocator for Kernel $(KERNEL_VERSION))
    include $(src)/common/misc/tcu_alloc/Kbuild
endif

# PWM: 3.10.14 and 4.4.94 use different PWM driver implementations.
ifeq ($(KERNEL_VERSION),3.10.14)
    $(info Building PWM for Kernel $(KERNEL_VERSION))
    # 3.10.14 uses the PP/TCU PWM driver (source set: pwm-pp)
    include $(src)/common/misc/pwm-pp/Kbuild
else
    $(info Building PWM for Kernel $(KERNEL_VERSION))
    # 4.4.94 uses the GPIO/TCU PWM driver
    include $(src)/common/misc/pwm/Kbuild
endif

ifeq ($(BR2_THINGINO_MOTORS),y)
    $(info Building Motor for Kernel $(KERNEL_VERSION))
    ifeq ($(KERNEL_VERSION),3.10.14)
        # 3.10.14 uses the PP/TCU motor driver (source set: motors-pp)
        include $(src)/common/misc/motors-pp/Kbuild
    else
        # 4.4.94 uses the GPIO/TCU motor driver
        include $(src)/common/misc/motor/Kbuild
    endif
    ifeq ($(BR2_THINGINO_MOTORS_SPI),y)
        ifeq ($(KERNEL_VERSION),3.10.14)
            $(info Building Motor SPI for Kernel $(KERNEL_VERSION))
            include $(src)/common/misc/ms419xx/Kbuild
        endif
    endif
endif

endif

#### PLATFORM ####
# Audio drivers live under common/audio/<soc>/<driver>.
# t41 and t23 use oss3; every other SoC uses oss2 on 3.10.14 and oss3 on 4.4.94.
ifeq ($(SOC_FAMILY),t41)
    $(info Building Audio for $(SOC_FAMILY) using oss3)
    include $(src)/common/audio/$(SOC_FAMILY)/oss3/Kbuild
else ifeq ($(SOC_FAMILY),t23)
    $(info Building Audio for $(SOC_FAMILY) using oss3)
    include $(src)/common/audio/$(SOC_FAMILY)/oss3/Kbuild
else ifeq ($(KERNEL_VERSION),3.10.14)
    $(info Building Audio for $(SOC_FAMILY) using oss2)
    include $(src)/common/audio/$(SOC_FAMILY)/oss2/Kbuild
else
    $(info Building Audio for $(SOC_FAMILY) using oss3)
    include $(src)/common/audio/$(SOC_FAMILY)/oss3/Kbuild
endif

ifeq ($(CONFIG_SOC_T31)$(CONFIG_SOC_C100)$(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
    $(info Building AVPU for Kernel $(KERNEL_VERSION))
    include $(src)/common/avpu/Kbuild
endif

ifeq ($(CONFIG_SOC_T40)$(CONFIG_SOC_T41)$(CONFIG_SOC_A1),y)
    include $(src)/common/misc/soc-nna/Kbuild
endif

ifeq ($(CONFIG_SOC_T40)$(CONFIG_SOC_T41),y)
    include $(src)/common/misc/mpsys-driver/Kbuild
    include $(src)/common/misc/jz-dtrng/Kbuild
endif

#### SENSORS ####
ifneq ($(CONFIG_SOC_A1),y)
ifeq ($(strip $(SENSOR_1_MODEL)$(SENSOR_2_MODEL)),)
    $(info Sensor models missing, building sinfo module)
    include $(src)/sinfo/Kbuild
else
# Sensor drivers all live in common/sensor-src/<soc>; the per-kernel
# sensor-src/<soc> trees were merged away, so the Kbuild under
# <kernel>/sensor-src just builds from common.
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
include $(src)/common/aip/a1/Kbuild
include $(src)/common/fb/Kbuild
include $(src)/common/ipu/Kbuild
include $(src)/common/video/a1/vde/Kbuild
include $(src)/common/video/a1/vdec/Kbuild
include $(src)/common/audio/$(SOC_FAMILY)/hdmi_audio/Kbuild
endif
