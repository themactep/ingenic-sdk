#!/bin/bash

rm ssitest;
make clean;
make;
mips-linux-gnu-gcc -muclibc ssi_test.c -o ssitest;
