cmd_/home_d/jszhang/work/isvp/opensource/drivers/soc-nna/platform.o := mips-linux-gnu-gcc -Wp,-MD,/home_d/jszhang/work/isvp/opensource/drivers/soc-nna/.platform.o.d  -nostdinc -isystem /opt/mips-gcc720-glibc229-r5.1.4/bin/../lib/gcc/mips-linux-gnu/7.2.0/include -I/home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include -Iarch/mips/include/generated  -Iinclude -I/home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi -Iarch/mips/include/generated/uapi -I/home_d/jszhang/work/isvp/opensource/kernel/include/uapi -Iinclude/generated/uapi -include /home_d/jszhang/work/isvp/opensource/kernel/include/linux/kconfig.h -D__KERNEL__ -DVMLINUX_LOAD_ADDRESS=0xffffffff80010000 -DDATAOFFSET=0 -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -fno-delete-null-pointer-checks -std=gnu89 -fno-PIE -Os -Wno-maybe-uninitialized -Wno-frame-address -Wno-format-truncation -mno-check-zero-division -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -Wno-array-bounds -Wno-bool-operation -DGAS_HAS_SET_HARDFLOAT -Wa,-msoft-float -ffreestanding -EL -UMIPSEB -U_MIPSEB -U__MIPSEB -U__MIPSEB__ -UMIPSEL -U_MIPSEL -U__MIPSEL -U__MIPSEL__ -DMIPSEL -D_MIPSEL -D__MIPSEL -D__MIPSEL__ -march=mips32r2 -Wa,-mips32r2 -Wa,--trap -I/home_d/jszhang/work/isvp/opensource/kernel/arch/mips/xburst2/core/include -I/home_d/jszhang/work/isvp/opensource/kernel/arch/mips/xburst2/soc-t41/include -I/home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/mach-generic -Wframe-larger-than=1024 -fno-stack-protector -Wno-unused-but-set-variable -Wno-unused-const-variable -fomit-frame-pointer -fno-var-tracking-assignments -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -DCC_HAVE_ASM_GOTO  -DMODULE -mlong-calls -Wno-array-bounds -Wno-bool-operation  -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(platform)"  -D"KBUILD_MODNAME=KBUILD_STR(soc_nna)" -c -o /home_d/jszhang/work/isvp/opensource/drivers/soc-nna/platform.o /home_d/jszhang/work/isvp/opensource/drivers/soc-nna/platform.c

source_/home_d/jszhang/work/isvp/opensource/drivers/soc-nna/platform.o := /home_d/jszhang/work/isvp/opensource/drivers/soc-nna/platform.c

deps_/home_d/jszhang/work/isvp/opensource/drivers/soc-nna/platform.o := \
  /home_d/jszhang/work/isvp/opensource/drivers/soc-nna/soc_nna_common.h \
  include/linux/platform_device.h \
    $(wildcard include/config/suspend.h) \
    $(wildcard include/config/hibernate/callbacks.h) \
    $(wildcard include/config/pm/sleep.h) \
  include/linux/device.h \
    $(wildcard include/config/debug/devres.h) \
    $(wildcard include/config/acpi.h) \
    $(wildcard include/config/pinctrl.h) \
    $(wildcard include/config/numa.h) \
    $(wildcard include/config/cma.h) \
    $(wildcard include/config/devtmpfs.h) \
    $(wildcard include/config/printk.h) \
    $(wildcard include/config/dynamic/debug.h) \
    $(wildcard include/config/sysfs/deprecated.h) \
  include/linux/ioport.h \
    $(wildcard include/config/memory/hotremove.h) \
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
  include/linux/compiler-gcc7.h \
    $(wildcard include/config/gcov/kernel.h) \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  include/uapi/linux/types.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/types.h \
    $(wildcard include/config/64bit/phys/addr.h) \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/types.h \
  /home_d/jszhang/work/isvp/opensource/kernel/include/uapi/linux/posix_types.h \
  include/linux/stddef.h \
  include/uapi/linux/stddef.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/posix_types.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/sgidefs.h \
  /home_d/jszhang/work/isvp/opensource/kernel/include/uapi/asm-generic/posix_types.h \
  include/linux/kobject.h \
  include/linux/list.h \
    $(wildcard include/config/debug/list.h) \
  include/linux/poison.h \
    $(wildcard include/config/illegal/pointer/value.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/include/uapi/linux/const.h \
  include/linux/sysfs.h \
    $(wildcard include/config/debug/lock/alloc.h) \
    $(wildcard include/config/sysfs.h) \
  include/linux/errno.h \
  include/uapi/linux/errno.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/errno.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/errno.h \
  /home_d/jszhang/work/isvp/opensource/kernel/include/uapi/asm-generic/errno-base.h \
  include/linux/lockdep.h \
    $(wildcard include/config/lockdep.h) \
    $(wildcard include/config/lock/stat.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/prove/locking.h) \
    $(wildcard include/config/prove/rcu.h) \
  include/linux/kobject_ns.h \
  include/linux/atomic.h \
    $(wildcard include/config/arch/has/atomic/or.h) \
    $(wildcard include/config/generic/atomic64.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/atomic.h \
  include/linux/irqflags.h \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  include/linux/typecheck.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/irqflags.h \
    $(wildcard include/config/cpu/mipsr2.h) \
    $(wildcard include/config/mips/mt/smtc.h) \
    $(wildcard include/config/irq/cpu.h) \
  include/linux/stringify.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/hazards.h \
    $(wildcard include/config/cpu/cavium/octeon.h) \
    $(wildcard include/config/cpu/mipsr1.h) \
    $(wildcard include/config/mips/alchemy.h) \
    $(wildcard include/config/cpu/bmips.h) \
    $(wildcard include/config/cpu/loongson2.h) \
    $(wildcard include/config/cpu/r10000.h) \
    $(wildcard include/config/cpu/r5500.h) \
    $(wildcard include/config/cpu/xlr.h) \
    $(wildcard include/config/cpu/sb1.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/barrier.h \
    $(wildcard include/config/cpu/has/sync.h) \
    $(wildcard include/config/sgi/ip28.h) \
    $(wildcard include/config/cpu/has/wb.h) \
    $(wildcard include/config/weak/ordering.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/weak/reordering/beyond/llsc.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/addrspace.h \
    $(wildcard include/config/cpu/r8000.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/mach-generic/spaces.h \
    $(wildcard include/config/32bit.h) \
    $(wildcard include/config/kvm/guest.h) \
    $(wildcard include/config/dma/noncoherent.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/cpu-features.h \
    $(wildcard include/config/sys/supports/micromips.h) \
    $(wildcard include/config/mach/xburst.h) \
    $(wildcard include/config/mach/xburst2.h) \
    $(wildcard include/config/cpu/has/msa.h) \
    $(wildcard include/config/cpu/mipsr2/irq/vi.h) \
    $(wildcard include/config/cpu/mipsr2/irq/ei.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/cpu.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/cpu-info.h \
    $(wildcard include/config/mips/mt/smp.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/cache.h \
    $(wildcard include/config/mips/l1/cache/shift.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/mach-generic/kmalloc.h \
    $(wildcard include/config/dma/coherent.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/xburst2/soc-t41/include/cpu-feature-overrides.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/xburst2/core/include/soc-ver.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/cmpxchg.h \
  include/linux/bug.h \
    $(wildcard include/config/generic/bug.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/bug.h \
    $(wildcard include/config/bug.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/break.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/break.h \
  include/asm-generic/bug.h \
    $(wildcard include/config/generic/bug/relative/pointers.h) \
    $(wildcard include/config/debug/bugverbose.h) \
  include/linux/kernel.h \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/atomic/sleep.h) \
    $(wildcard include/config/ring/buffer.h) \
    $(wildcard include/config/tracing.h) \
    $(wildcard include/config/ftrace/mcount/record.h) \
  /opt/mips-gcc720-glibc229-r5.1.4/lib/gcc/mips-linux-gnu/7.2.0/include/stdarg.h \
  include/linux/linkage.h \
  include/linux/export.h \
    $(wildcard include/config/have/underscore/symbol/prefix.h) \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/unused/symbols.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/linkage.h \
  include/linux/bitops.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/bitops.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/byteorder.h \
  include/linux/byteorder/little_endian.h \
  include/uapi/linux/byteorder/little_endian.h \
  include/linux/swab.h \
  include/uapi/linux/swab.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/swab.h \
  include/linux/byteorder/generic.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/war.h \
    $(wildcard include/config/cpu/r4000/workarounds.h) \
    $(wildcard include/config/cpu/r4400/workarounds.h) \
    $(wildcard include/config/cpu/daddi/workarounds.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/xburst2/soc-t41/include/war.h \
  include/asm-generic/bitops/non-atomic.h \
  include/asm-generic/bitops/fls64.h \
  include/asm-generic/bitops/ffz.h \
  include/asm-generic/bitops/find.h \
    $(wildcard include/config/generic/find/first/bit.h) \
  include/asm-generic/bitops/sched.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/arch_hweight.h \
  include/asm-generic/bitops/arch_hweight.h \
  include/asm-generic/bitops/const_hweight.h \
  include/asm-generic/bitops/le.h \
  include/asm-generic/bitops/ext2-atomic.h \
  include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  include/linux/printk.h \
    $(wildcard include/config/early/printk.h) \
  include/linux/init.h \
    $(wildcard include/config/broken/rodata.h) \
  include/linux/kern_levels.h \
  include/linux/dynamic_debug.h \
  include/linux/string.h \
    $(wildcard include/config/binary/printf.h) \
  include/uapi/linux/string.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/string.h \
    $(wildcard include/config/cpu/r3000.h) \
  include/uapi/linux/kernel.h \
  /home_d/jszhang/work/isvp/opensource/kernel/include/uapi/linux/sysinfo.h \
  include/asm-generic/cmpxchg-local.h \
  include/asm-generic/atomic-long.h \
  include/asm-generic/atomic64.h \
  include/linux/spinlock.h \
    $(wildcard include/config/debug/spinlock.h) \
    $(wildcard include/config/generic/lockbreak.h) \
    $(wildcard include/config/preempt.h) \
  include/linux/preempt.h \
    $(wildcard include/config/debug/preempt.h) \
    $(wildcard include/config/context/tracking.h) \
    $(wildcard include/config/preempt/count.h) \
    $(wildcard include/config/preempt/notifiers.h) \
  include/linux/thread_info.h \
    $(wildcard include/config/compat.h) \
    $(wildcard include/config/debug/stack/usage.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/thread_info.h \
    $(wildcard include/config/page/size/4kb.h) \
    $(wildcard include/config/page/size/8kb.h) \
    $(wildcard include/config/page/size/16kb.h) \
    $(wildcard include/config/page/size/32kb.h) \
    $(wildcard include/config/page/size/64kb.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/processor.h \
    $(wildcard include/config/cpu/little/endian.h) \
    $(wildcard include/config/cavium/octeon/cvmseg/size.h) \
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
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/cachectl.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/mipsregs.h \
    $(wildcard include/config/cpu/vr41xx.h) \
    $(wildcard include/config/mips/huge/tlb/support.h) \
    $(wildcard include/config/cpu/micromips.h) \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/prefetch.h \
  include/linux/bottom_half.h \
  include/linux/spinlock_types.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/spinlock_types.h \
  include/linux/rwlock_types.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/spinlock.h \
  include/linux/rwlock.h \
  include/linux/spinlock_api_smp.h \
    $(wildcard include/config/inline/spin/lock.h) \
    $(wildcard include/config/inline/spin/lock/bh.h) \
    $(wildcard include/config/inline/spin/lock/irq.h) \
    $(wildcard include/config/inline/spin/lock/irqsave.h) \
    $(wildcard include/config/inline/spin/trylock.h) \
    $(wildcard include/config/inline/spin/trylock/bh.h) \
    $(wildcard include/config/uninline/spin/unlock.h) \
    $(wildcard include/config/inline/spin/unlock/bh.h) \
    $(wildcard include/config/inline/spin/unlock/irq.h) \
    $(wildcard include/config/inline/spin/unlock/irqrestore.h) \
  include/linux/rwlock_api_smp.h \
    $(wildcard include/config/inline/read/lock.h) \
    $(wildcard include/config/inline/write/lock.h) \
    $(wildcard include/config/inline/read/lock/bh.h) \
    $(wildcard include/config/inline/write/lock/bh.h) \
    $(wildcard include/config/inline/read/lock/irq.h) \
    $(wildcard include/config/inline/write/lock/irq.h) \
    $(wildcard include/config/inline/read/lock/irqsave.h) \
    $(wildcard include/config/inline/write/lock/irqsave.h) \
    $(wildcard include/config/inline/read/trylock.h) \
    $(wildcard include/config/inline/write/trylock.h) \
    $(wildcard include/config/inline/read/unlock.h) \
    $(wildcard include/config/inline/write/unlock.h) \
    $(wildcard include/config/inline/read/unlock/bh.h) \
    $(wildcard include/config/inline/write/unlock/bh.h) \
    $(wildcard include/config/inline/read/unlock/irq.h) \
    $(wildcard include/config/inline/write/unlock/irq.h) \
    $(wildcard include/config/inline/read/unlock/irqrestore.h) \
    $(wildcard include/config/inline/write/unlock/irqrestore.h) \
  include/linux/kref.h \
  include/linux/mutex.h \
    $(wildcard include/config/debug/mutexes.h) \
    $(wildcard include/config/mutex/spin/on/owner.h) \
    $(wildcard include/config/have/arch/mutex/cpu/relax.h) \
  include/linux/wait.h \
  arch/mips/include/generated/asm/current.h \
  include/asm-generic/current.h \
  include/uapi/linux/wait.h \
  include/linux/klist.h \
  include/linux/pinctrl/devinfo.h \
  include/linux/pm.h \
    $(wildcard include/config/vt/console/sleep.h) \
    $(wildcard include/config/pm.h) \
    $(wildcard include/config/pm/runtime.h) \
    $(wildcard include/config/pm/clk.h) \
    $(wildcard include/config/pm/generic/domains.h) \
  include/linux/workqueue.h \
    $(wildcard include/config/debug/objects/work.h) \
    $(wildcard include/config/freezer.h) \
  include/linux/timer.h \
    $(wildcard include/config/timer/stats.h) \
    $(wildcard include/config/debug/objects/timers.h) \
  include/linux/ktime.h \
    $(wildcard include/config/ktime/scalar.h) \
  include/linux/time.h \
    $(wildcard include/config/arch/uses/gettimeoffset.h) \
  include/linux/cache.h \
    $(wildcard include/config/arch/has/cache/line/size.h) \
  include/linux/seqlock.h \
  include/linux/math64.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/div64.h \
  include/asm-generic/div64.h \
  include/uapi/linux/time.h \
  include/linux/jiffies.h \
  include/linux/timex.h \
  include/uapi/linux/timex.h \
  /home_d/jszhang/work/isvp/opensource/kernel/include/uapi/linux/param.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/uapi/asm/param.h \
  include/asm-generic/param.h \
    $(wildcard include/config/hz.h) \
  include/uapi/asm-generic/param.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/timex.h \
  include/linux/debugobjects.h \
    $(wildcard include/config/debug/objects.h) \
    $(wildcard include/config/debug/objects/free.h) \
  include/linux/completion.h \
  include/linux/ratelimit.h \
  include/linux/uidgid.h \
    $(wildcard include/config/uidgid/strict/type/checks.h) \
    $(wildcard include/config/user/ns.h) \
  include/linux/highuid.h \
  /home_d/jszhang/work/isvp/opensource/kernel/arch/mips/include/asm/device.h \
  include/linux/pm_wakeup.h \
  include/linux/mod_devicetable.h \
  include/linux/uuid.h \
  include/uapi/linux/uuid.h \
  /home_d/jszhang/work/isvp/opensource/drivers/soc-nna/soc_nna.h \
  /opt/mips-gcc720-glibc229-r5.1.4/lib/gcc/mips-linux-gnu/7.2.0/include/stdbool.h \
  /home_d/jszhang/work/isvp/opensource/drivers/soc-nna/soc_nna_hw.h \

/home_d/jszhang/work/isvp/opensource/drivers/soc-nna/platform.o: $(deps_/home_d/jszhang/work/isvp/opensource/drivers/soc-nna/platform.o)

$(deps_/home_d/jszhang/work/isvp/opensource/drivers/soc-nna/platform.o):
