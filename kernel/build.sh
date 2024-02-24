#!/bin/bash

case "$1" in
	clean)
		find . -name "*.o" -type f -exec rm -f {} \;
		find . -name "*.o.*" -type f -exec rm -f {} \;
		find . -name "*.ko" -type f -exec rm -f {} \;
		find . -name "*.ko.*" -type f -exec rm -f {} \;
		find . -name "*.mod.c" -type f -exec rm -f {} \;
		rm -f Module.symvers
		rm -f modules.order
		rm -rf .tmp_versions
		;;

	t10|t20|t21|t23|t30|t31|t40|t41)
		export SOC_FAMILY=$1
		export PATH=/opt/toolchains/thingino/mipsel-thingino-linux-musl_sdk-buildroot/bin:$PATH
		export CROSS_COMPILE=mipsel-linux-
		export ISP_ENV_KERNEL_DIR=${HOME}/output/escam-g16-t21n-jxh62/build/linux-custom 
		export OUTPUT_DIR=${HOME}/output/openingenic

		[ -d "$OUTPUT_DIR" ] && mkdir -p "$OUTPUT_DIR"

		FAM="SOC_FAMILY=${SOC_FAMILY} CONFIG_SOC_${SOC_FAMILY^^}=y"
		make ARCH=mips $FAM -C $ISP_ENV_KERNEL_DIR M=$PWD ${@:2}
		#ccache make V=2 \
		#    ARCH=mips CROSS_COMPILE=/path/to/openipc/firmware/output/host/bin/mipsel-openipc-linux-musl- \
		#    -C /path/to/openipc/firmware/output/build/linux-custom/ \
		#    SOC=t31 \
		#    M=$PWD -j8
		;;
	*)
		echo "Usage: $0 <soc|clean>"
		;;
esac

exit 0
