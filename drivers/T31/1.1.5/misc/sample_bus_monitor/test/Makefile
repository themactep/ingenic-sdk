#===============================================================
#	 @Author: Damon <jiansheng.zhang@ingenic.com>
#	 @Created Time : Mon 27 Jul 2020 14:28:05 PM CST
#
#	 @File Name: Makefile
#	 @Description:
#
#================================================================

CC=mips-linux-gnu-gcc
CCFLAGS += -pthread -Wall -muclibc -g -static
target = bus_monitor_test
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
