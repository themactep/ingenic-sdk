cmd_/home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.o := mips-linux-gnu-gcc -Wp,-MD,/home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/.system_i2c.o.d  -nostdinc -isystem /opt/mips-gcc472-glibc216-64bit/bin/../lib/gcc/mips-linux-gnu/4.7.2/include -I/home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include -Iarch/mips/include/generated  -Iinclude -I/home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi -Iarch/mips/include/generated/uapi -I/home_c/xbyu/work/isvp_w/opensource/kernel/include/uapi -Iinclude/generated/uapi -include /home_c/xbyu/work/isvp_w/opensource/kernel/include/linux/kconfig.h -D__KERNEL__ -DVMLINUX_LOAD_ADDRESS=0xffffffff80010000 -DDATAOFFSET=0 -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -fno-delete-null-pointer-checks -Os -Wno-maybe-uninitialized -mno-check-zero-division -mabi=32 -mgp32 -mfp32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ffreestanding -EL -UMIPSEB -U_MIPSEB -U__MIPSEB -U__MIPSEB__ -UMIPSEL -U_MIPSEL -U__MIPSEL -U__MIPSEL__ -DMIPSEL -D_MIPSEL -D__MIPSEL -D__MIPSEL__ -march=mips32 -Wa,-mips32 -Wa,--trap -I/home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/xburst/core/include -I/home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/xburst/soc-t30/include -I/home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/mach-generic -Wno-array-bounds -Wframe-larger-than=1024 -fno-stack-protector -Wno-unused-but-set-variable -fomit-frame-pointer -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fconserve-stack -DCC_HAVE_ASM_GOTO -I/home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/include  -DMODULE -mlong-calls  -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(system_i2c)"  -D"KBUILD_MODNAME=KBUILD_STR(tx_isp_t30)" -c -o /home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.o /home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.c

source_/home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.o := /home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.c

deps_/home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.o := \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/io.h \
    $(wildcard include/config/pci.h) \
    $(wildcard include/config/cpu/cavium/octeon.h) \
    $(wildcard include/config/64bit.h) \
    $(wildcard include/config/dma/noncoherent.h) \
  include/linux/compiler.h \
    $(wildcard include/config/sparse/rcu/pointer.h) \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
    $(wildcard include/config/kprobes.h) \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
  include/linux/compiler-gcc4.h \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  include/linux/kernel.h \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/atomic/sleep.h) \
    $(wildcard include/config/prove/locking.h) \
    $(wildcard include/config/ring/buffer.h) \
    $(wildcard include/config/tracing.h) \
    $(wildcard include/config/ftrace/mcount/record.h) \
  /opt/mips-gcc472-glibc216-64bit/bin/../lib/gcc/mips-linux-gnu/4.7.2/include/stdarg.h \
  include/linux/linkage.h \
  include/linux/stringify.h \
  include/linux/export.h \
    $(wildcard include/config/have/underscore/symbol/prefix.h) \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/unused/symbols.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/linkage.h \
  include/linux/stddef.h \
  include/uapi/linux/stddef.h \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
  include/uapi/linux/types.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/types.h \
    $(wildcard include/config/64bit/phys/addr.h) \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi/asm/types.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/include/uapi/linux/posix_types.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi/asm/posix_types.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi/asm/sgidefs.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/include/uapi/asm-generic/posix_types.h \
  include/linux/bitops.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/bitops.h \
    $(wildcard include/config/cpu/mipsr2.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/barrier.h \
    $(wildcard include/config/cpu/has/sync.h) \
    $(wildcard include/config/sgi/ip28.h) \
    $(wildcard include/config/cpu/has/wb.h) \
    $(wildcard include/config/weak/ordering.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/weak/reordering/beyond/llsc.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/addrspace.h \
    $(wildcard include/config/cpu/r8000.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/mach-generic/spaces.h \
    $(wildcard include/config/32bit.h) \
    $(wildcard include/config/kvm/guest.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/include/uapi/linux/const.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi/asm/byteorder.h \
  include/linux/byteorder/little_endian.h \
  include/uapi/linux/byteorder/little_endian.h \
  include/linux/swab.h \
  include/uapi/linux/swab.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi/asm/swab.h \
  include/linux/byteorder/generic.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/cpu-features.h \
    $(wildcard include/config/sys/supports/micromips.h) \
    $(wildcard include/config/cpu/mipsr2/irq/vi.h) \
    $(wildcard include/config/cpu/mipsr2/irq/ei.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/cpu.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/cpu-info.h \
    $(wildcard include/config/mips/mt/smp.h) \
    $(wildcard include/config/mips/mt/smtc.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/cache.h \
    $(wildcard include/config/mips/l1/cache/shift.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/mach-generic/kmalloc.h \
    $(wildcard include/config/dma/coherent.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/xburst/soc-t30/include/cpu-feature-overrides.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/xburst/core/include/soc-ver.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/war.h \
    $(wildcard include/config/cpu/r4000/workarounds.h) \
    $(wildcard include/config/cpu/r4400/workarounds.h) \
    $(wildcard include/config/cpu/daddi/workarounds.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/xburst/soc-t30/include/war.h \
  include/asm-generic/bitops/non-atomic.h \
  include/asm-generic/bitops/fls64.h \
  include/asm-generic/bitops/ffz.h \
  include/asm-generic/bitops/find.h \
    $(wildcard include/config/generic/find/first/bit.h) \
  include/asm-generic/bitops/sched.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/arch_hweight.h \
  include/asm-generic/bitops/arch_hweight.h \
  include/asm-generic/bitops/const_hweight.h \
  include/asm-generic/bitops/le.h \
  include/asm-generic/bitops/ext2-atomic.h \
  include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  include/linux/typecheck.h \
  include/linux/printk.h \
    $(wildcard include/config/early/printk.h) \
    $(wildcard include/config/printk.h) \
    $(wildcard include/config/dynamic/debug.h) \
  include/linux/init.h \
    $(wildcard include/config/broken/rodata.h) \
  include/linux/kern_levels.h \
  include/linux/dynamic_debug.h \
  include/linux/string.h \
    $(wildcard include/config/binary/printf.h) \
  include/uapi/linux/string.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/string.h \
    $(wildcard include/config/cpu/r3000.h) \
  include/linux/errno.h \
  include/uapi/linux/errno.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/errno.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi/asm/errno.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/include/uapi/asm-generic/errno-base.h \
  include/uapi/linux/kernel.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/include/uapi/linux/sysinfo.h \
  include/linux/irqflags.h \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/irqflags.h \
    $(wildcard include/config/irq/cpu.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/hazards.h \
    $(wildcard include/config/cpu/mipsr1.h) \
    $(wildcard include/config/mips/alchemy.h) \
    $(wildcard include/config/cpu/bmips.h) \
    $(wildcard include/config/cpu/loongson2.h) \
    $(wildcard include/config/cpu/r10000.h) \
    $(wildcard include/config/cpu/r5500.h) \
    $(wildcard include/config/cpu/xlr.h) \
    $(wildcard include/config/cpu/sb1.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/bug.h \
    $(wildcard include/config/bug.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/break.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi/asm/break.h \
  include/asm-generic/bug.h \
    $(wildcard include/config/generic/bug.h) \
    $(wildcard include/config/generic/bug/relative/pointers.h) \
    $(wildcard include/config/debug/bugverbose.h) \
  include/asm-generic/iomap.h \
    $(wildcard include/config/has/ioport.h) \
    $(wildcard include/config/generic/iomap.h) \
  include/asm-generic/pci_iomap.h \
    $(wildcard include/config/no/generic/pci/ioport/map.h) \
    $(wildcard include/config/generic/pci/iomap.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/page.h \
    $(wildcard include/config/page/size/4kb.h) \
    $(wildcard include/config/page/size/8kb.h) \
    $(wildcard include/config/page/size/16kb.h) \
    $(wildcard include/config/page/size/32kb.h) \
    $(wildcard include/config/page/size/64kb.h) \
    $(wildcard include/config/mips/huge/tlb/support.h) \
    $(wildcard include/config/cpu/mips32.h) \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/sparsemem.h) \
    $(wildcard include/config/need/multiple/nodes.h) \
  include/linux/pfn.h \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
  include/asm-generic/getorder.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/pgtable-bits.h \
    $(wildcard include/config/jzrisc/pep.h) \
    $(wildcard include/config/cpu/tx39xx.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/processor.h \
    $(wildcard include/config/cavium/octeon/cvmseg/size.h) \
    $(wildcard include/config/mach/xburst.h) \
    $(wildcard include/config/pmon/debug.h) \
    $(wildcard include/config/mips/mt/fpaff.h) \
    $(wildcard include/config/cpu/has/prefetch.h) \
  include/linux/cpumask.h \
    $(wildcard include/config/cpumask/offstack.h) \
    $(wildcard include/config/hotplug/cpu.h) \
    $(wildcard include/config/debug/per/cpu/maps.h) \
    $(wildcard include/config/disable/obsolete/cpumask/functions.h) \
  include/linux/threads.h \
    $(wildcard include/config/nr/cpus.h) \
    $(wildcard include/config/base/small.h) \
  include/linux/bitmap.h \
  include/linux/bug.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/uapi/asm/cachectl.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/mipsregs.h \
    $(wildcard include/config/cpu/vr41xx.h) \
    $(wildcard include/config/cpu/micromips.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/prefetch.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/mach-generic/ioremap.h \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/mach-generic/mangle-port.h \
    $(wildcard include/config/swap/io/space.h) \
  /home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.h \
  /home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/include/apical-isp/apical_types.h \
  /home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/include/apical-isp/apical_firmware_config.h \
    $(wildcard include/config/h//.h) \
  /home_c/xbyu/work/isvp_w/opensource/kernel/arch/mips/include/asm/div64.h \
  include/asm-generic/div64.h \
  include/linux/math64.h \

/home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.o: $(deps_/home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.o)

$(deps_/home_c/xbyu/work/isvp_w/opensource/drivers/isp-t30/tx-isp-t30/apical-isp/system_i2c.o):
