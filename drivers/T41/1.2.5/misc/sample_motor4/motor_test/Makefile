
CC=mips-linux-gnu-gcc
CCFLAGS += -pthread -Wall -muclibc -g -static
target = motortest
sources = $(wildcard *.c)
objects = $(patsubst %.c, %.o, $(sources))

$(target):$(objects)
	    $(CC) $(CCFLAGS) -o $@ $^
		    rm $(objects)
			    echo "generate $@"

%.o:%.c
	    $(CC) -Wall -muclibc -c -g -o $@ $<

.PHONY : clean
clean:
	rm -f $(target) *.o

