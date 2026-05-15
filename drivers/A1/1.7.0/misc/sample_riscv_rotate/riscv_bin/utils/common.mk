LD_SCRIPT ?= ../utils/link.ld

PREFIX = riscv32-unknown-elf-
GXX = $(PREFIX)g++
GCC = $(PREFIX)gcc
LD  = $(PREFIX)gcc
DUMP= $(PREFIX)objdump
LIB =
OBJCOPY=$(PREFIX)objcopy

OBJCOPY_VERLOG_OPT = --target verilog --strip-all
OBJCOPY_BIN_OPT	= -O binary -R .note -R .comment -S

GCC_CFLAGS ?= $(EXC_FLAGS) -O2 -march=rv32im -mabi=ilp32 -Wno-abi -fno-builtin -fno-exceptions -ffunction-sections -fomit-frame-pointer \
	   -fshort-wchar -Wall -static

GCC_OPT = $(GCC_CFLAGS) -I ../include

LD_OPT  = -T $(LD_SCRIPT)
DUMP_OPT = $(EXTRA_DUMP_OPT)

vpath %.c  ../utils
vpath %.S  ../utils

KERNEL_OBJS = crt0.boot.o
KERNEL_SRCS = exception.c
KERNEL_OBJS += $(KERNEL_SRCS:.c=.o)

APP_OBJS  = $(APP_C_SRCS:.c=.o)
APP_OBJS += $(APP_CXX_SRCS:.S=.o)
APP_OBJS += $(APP_CXX_SRCS:.cc=.o)

OBJS = $(KERNEL_OBJS) $(APP_OBJS)
#for debug
#.PRECIOUS: %.o %.out %.dump
.PRECIOUS: %.out %.dump
all:$(NAME)
%.o	: %.S
	@echo "Information: compile $@"
	$(GCC) $(GCC_OPT) -c -o $@ 	$<

%.o	: %.c
	@echo "Information: compile $@"
	$(GCC) $(GCC_OPT) -c -o $@ 	$<

%.o	: %.cc
	@echo "Information: compile $@"
	$(GXX) $(GCC_OPT) -c -o	$@ 	$<

%.out : $(OBJS) $(LIB_BSP)
	@echo "Information: link $@"
	$(LD) $(LD_OPT)  $^ -o $@ -nostdlib

%.dump : %.out
	@echo "Information: dump $@"
	$(DUMP) $(DUMP_OPT) -D $< >| $@

%.ver : %.out %.dump
	@echo "Information: generate $@"
	$(OBJCOPY) $(OBJCOPY_VERLOG_OPT) $< $@

%.bin : %.out %.dump
	@echo "Information: generate $@"
	$(OBJCOPY) $(OBJCOPY_BIN_OPT) $< $@

%.vmem : %.ver
	@echo "Information: generate $@ memfile"
	v2m $< $@

help:
	@echo "Use EXC_FLAGS(-DMAILBOX_EXC) specify exception handler, for example:"
	@echo "make EXC_FLAGS=-DMAILBOX_EXC"
	@echo ""
	@echo "Use LINK(sram, ddr, mix) specify link file, for example:"
	@echo "make LINK=sram"

clean:
	rm -f *.o *.out *.dump *.ver *.bin
