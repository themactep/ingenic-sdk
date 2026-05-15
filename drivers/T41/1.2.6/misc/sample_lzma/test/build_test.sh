#/bin/bash
mips-linux-gnu-gcc -muclibc -O2 -Wall -march=mips32r2 test_lzma.c -o test_lzma
mips-linux-gnu-gcc -muclibc -O2 -Wall -march=mips32r2 test_lzma1.c -o test_lzma1
