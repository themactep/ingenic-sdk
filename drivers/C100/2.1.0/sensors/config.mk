ifeq (${CONFIG_SOC_T10}, y)
SOC_TYPE := t10
else ifeq (${CONFIG_SOC_T20}, y)
SOC_TYPE := t20
else ifeq (${CONFIG_SOC_T30}, y)
SOC_TYPE := t30
else ifeq (${CONFIG_SOC_T31}, y)
SOC_TYPE := t31
else ifeq (${CONFIG_SOC_C100}, y)
SOC_TYPE := c100
endif
