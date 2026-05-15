Target=sample-spi
CC=mips-linux-gnu-gcc

.PHONY:clean


$(Target) : $(Target).o
	$(CC) -static -muclibc $(Target).c -o $@

clean:
	@rm -rf *.o  $(Target)
