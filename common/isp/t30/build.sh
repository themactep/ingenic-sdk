#!/bin/bash

if [ -z "$1" ]; then
  echo "Usage: $0 <soc>"
  exit 1
fi

export OUTPUT_DIR=/home/paul/localwork/openipc/firmware/output/t21_lite
export PATH=$OUTPUT_DIR/host/bin:$PATH
export CROSS_COMPILE=mipsel-openipc-linux-musl-
export ISP_ENV_KERNEL_DIR=$OUTPUT_DIR/build/linux-custom
export SOC=$1

make ARCH=mips -C $ISP_ENV_KERNEL_DIR M=$PWD
