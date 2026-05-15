CROSS_COMPILE ?= mips-linux-uclibc-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -g -O2
STRIP := $(CROSS_COMPILE)strip
TARGET = sound_play sound_record
INC = ./
SRC = $(wildcard ./*.c)
OBJ = $(patsubst ./%.c,./%.o,$(SRC))

all : $(TARGET)

sound_play : sound_play.o
	$(CC) $(CFLAGS) $^ -o $@
	${STRIP} $@

sound_record : sound_record.o
	$(CC) $(CFLAGS) $^ -o $@
	${STRIP} $@

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY:clean

clean:
	rm $(OBJ) $(TARGET) -f

