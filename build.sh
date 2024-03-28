#!/bin/bash

TOOLCHAIN_DIR=${HOME}/dev/toolchain/mipsel-thingino-linux-musl_sdk-buildroot
CROSS_COMPILE=mipsel-linux-
KERNEL_DIR=${HOME}/dev/linux
DEFAULT_KERNEL_VERSION="3.10"

# Extract SoC and Kernel Version from arguments
SOC_MODEL="$1"
KERNEL_VERSION="${2:-$DEFAULT_KERNEL_VERSION}" # Use second argument or default to 3.10


case "$SOC_MODEL" in
	clean)
		find . -type f -name "*.o"     -delete
		find . -type f -name "*.o.*"   -delete
		find . -type f -name "*.ko"    -delete
		find . -type f -name "*.ko.*"  -delete
		find . -type f -name "*.mod.c" -delete
		rm -f Module.symvers
		rm -f modules.order
		rm -rf .tmp_versions
		exit 0
		;;
	t10 | t20 | t21 | t23 | t30 | t31 | t40 | t41)
		export ISP_ENV_KERNEL_DIR="${KERNEL_DIR}"
		export CROSS_COMPILE="${TOOLCHAIN_DIR}/bin/${CROSS_COMPILE}"

		FAM="SOC_FAMILY=$SOC_MODEL CONFIG_SOC_${SOC_MODEL^^}=y KERNEL_VERSION=${KERNEL_VERSION}"
		make V=2 ARCH=mips $FAM -C $ISP_ENV_KERNEL_DIR M=$PWD ${@:3}
		;;
	*)
		echo "Usage: $0 <soc> [kernel_version] <make_args>"
		echo "Example: $0 t20 3.10"
		exit 1
		;;
esac

exit 0
