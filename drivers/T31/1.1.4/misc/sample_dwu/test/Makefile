#===============================================================
#	 @Author: Damon <jiansheng.zhang@ingenic.com>
#	 @Created Time : Mon 7 Sep 2020 09:28:05 PM CST
#
#	 @File Name: Makefile
#	 @Description:
#
#================================================================

CC=mips-linux-gnu-gcc
CCFLAGS += -pthread -Wall -muclibc -g -static
target = dwu_test
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
