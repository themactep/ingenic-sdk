CC = mips-linux-uclibc-gnu-gcc

src := $(wildcard ./*.c)
objs := $(patsubst %.c, %.o, $(src))

OUTPUT_NAME := sample_hash

$(OUTPUT_NAME): $(objs)
	$(CC) -o $@ $^
%.o:%.c
	$(CC) -c -o $@ $<

.PHONY: clean
clean:
	rm $(objs) $(OUTPUT_NAME)
