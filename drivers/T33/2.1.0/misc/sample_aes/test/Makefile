CC = mips-linux-uclibc-gnu-gcc
src := $(wildcard ./*.c)
objs := $(patsubst %.c, %.o, $(src))

OUTPUT_NAME := sample_aes

$(OUTPUT_NAME): $(objs)
	$(CC) -o $@ $^
%.o:%.c
	$(CC) -std=gnu99  -c -o $@ $<

.PHONY: clean
clean:
	rm $(objs) $(OUTPUT_NAME)
