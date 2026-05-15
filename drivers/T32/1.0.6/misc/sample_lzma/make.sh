#!/bin/bash
make clean;make;

cd test/
mips-linux-gnu-gcc -muclibc -O2 -Wall -march=mips32r2 -lpthread sample_lzma.c -o sample_lzma
