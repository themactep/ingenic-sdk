CROSS_COMPILE ?= mips-linux-uclibc-gnu-

ifeq (${TASSADAR_ENV_KERNEL_DIR}, )
	TASSADAR_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94
endif

KDIR := ${TASSADAR_ENV_KERNEL_DIR}

ifeq ($(CONFIG_SOC_PRJ008), y)
################### name define #########
MODULE_NAME := tx-isp-PRJ008
################### name end #########
else ifeq ($(CONFIG_SOC_PRJ007), y)
################### name define #########
MODULE_NAME := tx-isp-PRJ007
################### name end #########
endif
all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

ifeq ($(CONFIG_SOC_PRJ008), y)
$(MODULE_NAME)-objs := libPRJ008-firmware.a tx-isp-funcs.o tx-isp-module.o
else ifeq ($(CONFIG_SOC_PRJ007), y)
$(MODULE_NAME)-objs := libPRJ007-firmware.a tx-isp-funcs.o tx-isp-module.o
endif

obj-m := $(MODULE_NAME).o

modules:
	@if test "mips-linux-gnu-gcc (Ingenic r3.3.7.mxu2.cve2-gcc540 2025.07-04) 5.4.0" = "`$(CROSS_COMPILE)gcc --version | head -n1`";then \
		if grep -q "^CONFIG_SOC_PRJ008=y" "${KDIR}/.config"; then\
			if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
				echo "现在正在编译T33, toolchain:540 cve2 kernel:4.4.94 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
					cp libPRJ008-firmware-4494-540-pm.a libPRJ008-firmware.a; \
				else \
					echo "Use ISP firmware 4.4.94"; \
					cp libPRJ008-firmware-4494-540.a libPRJ008-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
				echo "现在正在编译T33, toolchain:540 cve2 kernel:3.10 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 3.10 CONFIG_PM"; \
					cp libPRJ008-firmware-310-540-pm.a libPRJ008-firmware.a; \
				else\
					echo "Use ISP firmware 3.10 ${MODULE_NAME} ${KDIR}"; \
					cp libPRJ008-firmware-310-540.a libPRJ008-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_5_15=y" "${KDIR}/.config"; then \
				echo "现在正在编译T33, toolchain:540 cve2 kernel:5.15 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 5.15 CONFIG_PM"; \
					cp libPRJ008-firmware-515-540-pm.a libPRJ008-firmware.a; \
				else\
					echo "Use ISP firmware 5.15"; \
					cp libPRJ008-firmware-515-540.a libPRJ008-firmware.a; \
				fi;\
			else \
				echo "Kernel config not found.";\
			fi;\
		elif grep -q "^CONFIG_SOC_PRJ007=y" "${KDIR}/.config"; then\
			if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
				echo "现在正在编译T32,toolchain:540 cve2 kernel:4.4.94 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
					cp libPRJ007-firmware-4494-540-pm.a libPRJ007-firmware.a; \
				else \
					echo "Use ISP firmware 4.4.94"; \
					cp libPRJ007-firmware-4494-540.a libPRJ007-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
				echo "现在正在编译T32,toolchain:540 cve2 kernel:3.10 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 3.10 CONFIG_PM"; \
					cp libPRJ007-firmware-310-540-pm.a libPRJ007-firmware.a; \
				else\
					echo "Use ISP firmware 3.10"; \
					cp libPRJ007-firmware-310-540.a libPRJ007-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_5_15=y" "${KDIR}/.config"; then \
				echo "现在正在编译T32, toolchain:540 cve2 kernel:5.15 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 5.15 CONFIG_PM"; \
					cp libPRJ007-firmware-515-540-pm.a libPRJ007-firmware.a; \
				else\
					echo "Use ISP firmware 5.15"; \
					cp libPRJ007-firmware-515-540.a libPRJ007-firmware.a; \
				fi;\
			else \
				echo "Kernel config not found.";\
			fi;\
		fi;\
	elif test "mips-linux-uclibc-gcc (Ingenic r3.3.7.mxu2.cve2-gcc540 2025.07-17) 5.4.0" = "`$(CROSS_COMPILE)gcc --version | head -n1`";then \
		if grep -q "^CONFIG_SOC_PRJ008=y" "${KDIR}/.config"; then\
			if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
				echo "现在正在编译T33, toolchain:540 uclibc cve2 kernel:4.4.94 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
					cp libPRJ008-firmware-4494-540-pm.a libPRJ008-firmware.a; \
				else \
					echo "Use ISP firmware 4.4.94"; \
					cp libPRJ008-firmware-4494-540.a libPRJ008-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
				echo "现在正在编译T33, toolchain:540 uclibc cve2 kernel:3.10 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 3.10 CONFIG_PM"; \
					cp libPRJ008-firmware-310-540-pm.a libPRJ008-firmware.a; \
				else\
					echo "Use ISP firmware 3.10 ${MODULE_NAME} ${KDIR}"; \
					cp libPRJ008-firmware-310-540.a libPRJ008-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_5_15=y" "${KDIR}/.config"; then \
				echo "现在正在编译T33, toolchain:540 uclibc cve2 kernel:5.15 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 5.15 CONFIG_PM"; \
					cp libPRJ008-firmware-515-540-pm.a libPRJ008-firmware.a; \
				else\
					echo "Use ISP firmware 5.15"; \
					cp libPRJ008-firmware-515-540.a libPRJ008-firmware.a; \
				fi;\
			else \
				echo "Kernel config not found.";\
			fi;\
		elif grep -q "^CONFIG_SOC_PRJ007=y" "${KDIR}/.config"; then\
			if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
				echo "现在正在编译T32,toolchain:540 uclibc cve2 kernel:4.4.94 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
					cp libPRJ007-firmware-4494-540-pm.a libPRJ007-firmware.a; \
				else \
					echo "Use ISP firmware 4.4.94"; \
					cp libPRJ007-firmware-4494-540.a libPRJ007-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
				echo "现在正在编译T32,toolchain:540 uclibc cve2 kernel:3.10 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 3.10 CONFIG_PM"; \
					cp libPRJ007-firmware-310-540-pm.a libPRJ007-firmware.a; \
				else\
					echo "Use ISP firmware 3.10"; \
					cp libPRJ007-firmware-310-540.a libPRJ007-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_5_15=y" "${KDIR}/.config"; then \
				echo "现在正在编译T32, toolchain:540 uclibc cve2 kernel:5.15 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 5.15 CONFIG_PM"; \
					cp libPRJ007-firmware-515-540-pm.a libPRJ007-firmware.a; \
				else\
					echo "Use ISP firmware 5.15"; \
					cp libPRJ007-firmware-515-540.a libPRJ007-firmware.a; \
				fi;\
			else \
				echo "Kernel config not found.";\
			fi;\
		fi;\
	elif test "mips-linux-gnu-gcc (Ingenic r2.4.2 gcc add npu uclibc sub rpc sub locale 2024.11-08) 4.7.2" = "`$(CROSS_COMPILE)gcc --version | head -n1`";then \
		if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
			echo "现在正在编译toolchain:472 kernel:4.4.94 的ko."; \
			if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
				echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
				cp libPRJ008-firmware-4494-472-pm.a libPRJ008-firmware.a; \
			else \
				echo "Use ISP firmware 4.4.94"; \
				cp libPRJ008-firmware-4494-472.a libPRJ008-firmware.a; \
			fi;\
		elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
			echo "现在正在编译toolchain:472 kernel:3.10 的ko."; \
			if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
				echo "Use ISP firmware 3.10 CONFIG_PM"; \
				cp libPRJ008-firmware-310-472-pm.a libPRJ008-firmware.a; \
			else\
				echo "Use ISP firmware 3.10"; \
				cp libPRJ008-firmware-310-472.a libPRJ008-firmware.a; \
			fi;\
		else \
			echo "Kernel config not found.";\
		fi;\
	elif test "mips-linux-gnu-gcc (Ingenic r2.4.2.mxu2.cve2 2025.07-17) 4.7.2" = "`$(CROSS_COMPILE)gcc --version | head -n1`";then \
		if grep -q "^CONFIG_SOC_PRJ008=y" "${KDIR}/.config"; then\
			if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
				echo "现在正在编译toolchain:472_mxu2_cve2 kernel:4.4.94 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
					cp libPRJ008-firmware-4494-472-pm.a libPRJ008-firmware.a; \
				else \
					echo "Use ISP firmware 4.4.94"; \
					cp libPRJ008-firmware-4494-472.a libPRJ008-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
				echo "现在正在编译toolchain:472_mxu2_cve2 kernel:3.10 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 3.10 CONFIG_PM"; \
					cp libPRJ008-firmware-310-472-pm.a libPRJ008-firmware.a; \
				else\
					echo "Use ISP firmware 3.10"; \
					cp libPRJ008-firmware-310-472.a libPRJ008-firmware.a; \
				fi;\
			else \
				echo "Kernel config not found.";\
			fi;\
		elif grep -q "^CONFIG_SOC_PRJ007=y" "${KDIR}/.config"; then\
			if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
				echo "现在正在编译toolchain:472_mxu2_cve2 kernel:4.4.94 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
					cp libPRJ007-firmware-4494-472-pm.a libPRJ007-firmware.a; \
				else \
					echo "Use ISP firmware 4.4.94"; \
					cp libPRJ007-firmware-4494-472.a libPRJ007-firmware.a; \
				fi;\
			elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
				echo "现在正在编译toolchain:472_mxu2_cve2 kernel:3.10 的ko."; \
				if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
					echo "Use ISP firmware 3.10 CONFIG_PM"; \
					cp libPRJ007-firmware-310-472-pm.a libPRJ007-firmware.a; \
				else\
					echo "Use ISP firmware 3.10"; \
					cp libPRJ007-firmware-310-472.a libPRJ007-firmware.a; \
				fi;\
			else \
				echo "Kernel config not found.";\
			fi;\
		fi;\
	fi

	@if test "${ENV_SDK_TYPE}" = "ZERATUL";then \
		cp -r include/* $(KDIR)/arch/mips/xburst/lib/isp/include/; \
		cp tx-isp-funcs.c $(KDIR)/arch/mips/xburst/lib/isp/; \
		if grep -q "^CONFIG_SOC_PRJ008=y" "${KDIR}/.config"; then\
			if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
				cp -v libPRJ008-firmware.a $(KDIR)/arch/mips/xburst/lib/isp/libPRJ008-firmware-pm.hex; \
			else\
				cp -v libPRJ008-firmware.a $(KDIR)/arch/mips/xburst/lib/isp/libPRJ008-firmware.hex; \
			fi;\
		elif grep -q "^CONFIG_SOC_PRJ007=y" "${KDIR}/.config"; then\
			if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
				cp -v libPRJ007-firmware.a $(KDIR)/arch/mips/xburst/lib/isp/libPRJ007-firmware-pm.hex; \
			else\
				cp -v libPRJ007-firmware.a $(KDIR)/arch/mips/xburst/lib/isp/libPRJ007-firmware.hex; \
			fi;\
		fi;\
	fi;\

	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c'  -o -name '*.mod' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
