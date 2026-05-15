CROSS_COMPILE ?= mips-linux-uclibc-gnu-

ifeq (${TASSADAR_ENV_KERNEL_DIR}, )
	TASSADAR_ENV_KERNEL_DIR = $(PWD)/../../../kernel-4.4.94
endif

KDIR := ${TASSADAR_ENV_KERNEL_DIR}


################### name define #########
MODULE_NAME := tx-isp-PRJ007
################### name end #########

all: modules

.PHONY: modules clean

EXTRA_CFLAGS += -I$(PWD)/include

$(MODULE_NAME)-objs := libPRJ007-firmware.a tx-isp-funcs.o tx-isp-module.o

obj-m := $(MODULE_NAME).o

modules:
	@if test "mips-linux-gnu-gcc (Ingenic r3.3.7.mxu2-gcc540 memcpy memset memcmp memmove strcpy strlen only on mxu2 simd core fix02. 2024.12-19) 5.4.0" = "`$(CROSS_COMPILE)gcc --version | head -n1`";then \
		if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
			echo "现在正在编译toolchain:540 kernel:4.4.94 的ko."; \
			if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
				echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
				cp libPRJ007-firmware-4494-540-pm.a libPRJ007-firmware.a; \
			else \
				echo "Use ISP firmware 4.4.94"; \
				cp libPRJ007-firmware-4494-540.a libPRJ007-firmware.a; \
			fi;\
		elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
			echo "现在正在编译toolchain:540 kernel:3.10 的ko."; \
			if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
				echo "Use ISP firmware 3.10 CONFIG_PM"; \
				cp libPRJ007-firmware-310-540-pm.a libPRJ007-firmware.a; \
			else\
				echo "Use ISP firmware 3.10"; \
				cp libPRJ007-firmware-310-540.a libPRJ007-firmware.a; \
			fi;\
		else \
			echo "Kernel config not found.";\
		fi;\
	elif test "mips-linux-gnu-gcc (Ingenic r3.3.7.mxu2.cve-gcc540 memcpy memset memcmp memmove strcpy strlen only on mxu2 simd core and uclibc CVE-2021-43523 fix03. 2025.05-08) 5.4.0" = "`$(CROSS_COMPILE)gcc --version | head -n1`";then \
		if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
			echo "现在正在编译toolchain:540 cve kernel:4.4.94 的ko."; \
			if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
				echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
				cp libPRJ007-firmware-4494-540-pm.a libPRJ007-firmware.a; \
			else \
				echo "Use ISP firmware 4.4.94"; \
				cp libPRJ007-firmware-4494-540.a libPRJ007-firmware.a; \
			fi;\
		elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
			echo "现在正在编译toolchain:540 cve kernel:3.10 的ko."; \
			if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
				echo "Use ISP firmware 3.10 CONFIG_PM"; \
				cp libPRJ007-firmware-310-540-pm.a libPRJ007-firmware.a; \
			else\
				echo "Use ISP firmware 3.10"; \
				cp libPRJ007-firmware-310-540.a libPRJ007-firmware.a; \
			fi;\
		else \
			echo "Kernel config not found.";\
		fi;\
	elif test "mips-linux-gnu-gcc (Ingenic r2.4.2 gcc add npu uclibc sub rpc sub locale 2024.11-08) 4.7.2" = "`$(CROSS_COMPILE)gcc --version | head -n1`";then \
		if grep -q "^CONFIG_KERNEL_4_4_94=y" "${KDIR}/.config"; then\
			echo "现在正在编译toolchain:472 kernel:4.4.94 的ko."; \
			if grep -q "^CONFIG_PM=y" "${KDIR}/.config"; then\
				echo "Use ISP firmware 4.4.94 CONFIG_PM"; \
				cp libPRJ007-firmware-4494-472-pm.a libPRJ007-firmware.a; \
			else \
				echo "Use ISP firmware 4.4.94"; \
				cp libPRJ007-firmware-4494-472.a libPRJ007-firmware.a; \
			fi;\
		elif grep -q "^CONFIG_KERNEL_3_10=y" "${KDIR}/.config"; then \
			echo "现在正在编译toolchain:472 kernel:3.10 的ko."; \
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
	fi

	@if test "${ENV_SDK_TYPE}" = "ZERATUL";then \
		cp -r include/* $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/include/; \
		cp tx-isp-funcs.c $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/; \
		cp libPRJ007-firmware.a $(KDIR)/arch/mips/xburst/lib/isp-PRJ007/libPRJ007-firmware.hex; \
	fi;\

	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@find . -name '*.o' -o -name '*~' -o -name '.depend' -o -name '.*.cmd' \
		-o -name '*.mod.c' -o -name '.tmp_versions' -o -name '*.ko' \
		-o -name '*.symvers' -o -name 'modules.order' | xargs rm -rf
