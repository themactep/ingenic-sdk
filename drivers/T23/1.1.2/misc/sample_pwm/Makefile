Target = pwm_test
cp_path = ~/shared/

# CC = gcc
CC= mips-linux-gnu-gcc

# *.c
sources_file = $(wildcard *.c)
# *.o
object_file = $(patsubst %.c,%.o,$(sources_file))
# *.h
include_file = $(wildcard *.h)

LDFLAG += -muclibc
# LIBS += -lpthread
CCFLAGS += -Wall
CCFLAGS += -g
#-static

$(Target) : $(object_file)
	$(CC) $^ $(CCFLAGS) $(LDFLAG) ${LIBS} -o $@

%.o : %.c $(include_file)
	$(CC) $(LDFLAG) -c $< -o $@

.PHONY:clean cleanall cp

clean:
	-rm -rf *.o

cleanall: clean
	-rm -rf $(Target)

cp:
	cp ./$(Target) $(cp_path)
