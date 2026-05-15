Target = rsa_test_t32
CC = mips-linux-uclibc-gnu-gcc

sources_file = $(wildcard *.c)
object_file = $(patsubst %.c,%.o,$(sources_file))
include_file = $(wildcard *.h)

LDFLAGS += -muclibc
CFLAGS = -I./include -Wall

$(Target) : $(object_file)
		$(CC) $^ $(LDFLAGS) ${LIBS} -o $@

%.o : %.c $(include_file)
		$(CC) -c $< $(CFLAGS) -o $@

.PHONY: clean

clean:
		-rm -f *.o $(Target)

