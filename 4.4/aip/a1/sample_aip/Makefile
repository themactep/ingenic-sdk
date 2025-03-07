libc = 1
ifeq ($(libc), 1)
	CC = mips-v720s229-linux-uclibc-gnu-g++
	C++ = mips-v720s229-linux-uclibc-gnu-g++
	LIBS = ./nna/uclibc/libnna.a
else
	CC = mips-linux-gnu-g++
	C++ = mips-linux-gnu-g++
	LIBS = ./nna/glibc/libnna.a
endif

src := $(wildcard ./sample_aip.c ./src/*.c)
objs += $(patsubst %.c, %.o, $(src))
model := $(wildcard ./model/*.c)
objs += $(patsubst %.c, %.o, $(model))
objs += ./model/bs_src.o ./model/3rd_party/Matrix.o

INCLUDE_COMMON = -I ./include/jz_aip -I ./include/model/ -I ./include/3rd_party -I ./nna/include/

OUTPUT_NAME := sample_aip

$(OUTPUT_NAME): $(objs)
	$(CC) -o $@ $^ $(LIBS) $(INCLUDE_COMMON) -lpthread
%.o:%.c
	$(CC) -c -o $@ $< $(INCLUDE_COMMON)
%.o:%.cc
	$(C++) -c -o $@ $< $(INCLUDE_COMMON)

.PHONY: clean
clean:
	rm -rf $(objs) $(OUTPUT_NAME)
