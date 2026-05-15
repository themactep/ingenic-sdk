CC=mips-linux-gnu-gcc
#CC=mips-linux-uclibc-gnu-gcc
#WARNINGS = -Wall -Wextra
CFLAGS = -muclibc
target = hash_test
objects = test_hash.o

$(target):$(objects)
	@$(CC) $(CFLAGS) -o $@ $^
%.o:%.c
	@$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean :
	@rm -f $(target) $(objects)
