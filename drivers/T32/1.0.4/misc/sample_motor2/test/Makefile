Target=step_test
CC=mips-linux-gnu-gcc

$(Target) : $(Target).o
	$(CC) -static -muclibc $(Target).c -o $@

