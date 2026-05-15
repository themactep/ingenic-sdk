CC=mips-linux-uclibc-gnu-gcc
CFLAGS = -muclibc
target = dtrng_test
objects = test_dtrng.o

$(target):$(objects)
	@$(CC) $(CFLAGS) -o $@ $^
%.o:%.c
	@$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean :
	@rm -f $(target) $(objects)
