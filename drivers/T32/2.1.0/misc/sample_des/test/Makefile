CC=mips-linux-uclibc-gnu-gcc
CFLAGS = -muclibc
target = des_test
objects = test_des.o

$(target):$(objects)
	@$(CC) $(CFLAGS) -o $@ $^
%.o:%.c
	@$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean :
	@rm -f $(target) $(objects)
