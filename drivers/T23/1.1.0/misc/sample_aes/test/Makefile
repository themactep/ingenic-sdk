CC=mips-linux-gnu-gcc
#CC=mips-linux-uclibc-gnu-gcc
#WARNINGS = -Wall -Wextra
CFLAGS = -muclibc
target = aes_test
objects = test_aes.o

$(target):$(objects)
	@$(CC) $(CFLAGS) -o $@ $^
%.o:%.c
	@$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean :
	@rm -f $(target) $(objects)
