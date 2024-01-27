#!/bin/bash

if [ -z "$1" ]; then
  echo "Usage: $0 <soc> <build|clean>"
  exit 1
fi

export OUTPUT_DIR=/home/paul/openipc-fw-output/cinnado_d1-br2023.11
export PATH=$OUTPUT_DIR/host/bin:$PATH
export CROSS_COMPILE=mipsel-openipc-linux-musl-
export ISP_ENV_KERNEL_DIR=$OUTPUT_DIR/build/linux-custom
export SOC_FAMILY=$1

if [[ "$2" == "build" ]]; then
    make ARCH=mips -C $ISP_ENV_KERNEL_DIR M=$PWD

    #ccache make V=2 \
    #    ARCH=mips CROSS_COMPILE=/path/to/openipc/firmware/output/host/bin/mipsel-openipc-linux-musl- \
    #    -C /path/to/openipc/firmware/output/build/linux-custom/ \
    #    SOC=t31 \
    #    M=$PWD -j8

elif [[ "$2" == "clean" ]]; then
    find . -name "*.o" -type f -exec rm -f {} \;
    find . -name "*.o.*" -type f -exec rm -f {} \;
    find . -name "*.ko" -type f -exec rm -f {} \;
    find . -name "*.ko.*" -type f -exec rm -f {} \;
    find . -name "*.mod.c" -type f -exec rm -f {} \;
    rm -f Module.symvers
    rm -f modules.order
    rm -rf .tmp_versions

elif [[ "$2" == "" ]]; then
    echo "build, or clean"
fi
