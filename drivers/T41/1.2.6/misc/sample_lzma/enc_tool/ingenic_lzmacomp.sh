#!/bin/bash
.lzma -z -k -f -9 vmlinux.bin
cp jz_lzma_out.bin vmlinux.jzlzma
sync

