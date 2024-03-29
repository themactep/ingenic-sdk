#include <linux/compiler.h>
#include <linux/stringify.h>
#include <asm/compiler.h>
#include <asm/hazards.h>

#if defined(CONFIG_CPU_MIPSR2) || defined (CONFIG_CPU_MIPSR6)

static void arch_local_irq_disable(void)
{
	__asm__ __volatile__(
	"	.set	push						\n"
	"	.set	noat						\n"
	"	di							\n"
	"	" __stringify(__irq_disable_hazard) "			\n"
	"	.set	pop						\n"
	: /* no outputs */
	: /* no inputs */
	: "memory");
}

static unsigned long arch_local_irq_save(void)
{
	unsigned long flags;

	asm __volatile__(
	"	.set	push						\n"
	"	.set	reorder						\n"
	"	.set	noat						\n"
	"	di	%[flags]					\n"
	"	andi	%[flags], 1					\n"
	"	" __stringify(__irq_disable_hazard) "			\n"
	"	.set	pop						\n"
	: [flags] "=r" (flags)
	: /* no inputs */
	: "memory");

	return flags;
}

static void arch_local_irq_restore(unsigned long flags)
{
	unsigned long __tmp1;

	__asm__ __volatile__(
	"	.set	push						\n"
	"	.set	noreorder					\n"
	"	.set	noat						\n"
#if defined(CONFIG_IRQ_MIPS_CPU)
	/*
	 * Slow, but doesn't suffer from a relatively unlikely race
	 * condition we're having since days 1.
	 */
	"	beqz	%[flags], 1f					\n"
	"	di							\n"
	"	ei							\n"
	"1:								\n"
#else
	/*
	 * Fast, dangerous.  Life is fun, life is good.
	 */
	"	mfc0	$1, $12						\n"
	"	ins	$1, %[flags], 0, 1				\n"
	"	mtc0	$1, $12						\n"
#endif
	"	" __stringify(__irq_disable_hazard) "			\n"
	"	.set	pop						\n"
	: [flags] "=r" (__tmp1)
	: "0" (flags)
	: "memory");
}

static void __arch_local_irq_restore(unsigned long flags)
{
	__asm__ __volatile__(
	"	.set	push						\n"
	"	.set	noreorder					\n"
	"	.set	noat						\n"
#if defined(CONFIG_IRQ_MIPS_CPU)
	/*
	 * Slow, but doesn't suffer from a relatively unlikely race
	 * condition we're having since days 1.
	 */
	"	beqz	%[flags], 1f					\n"
	"	di							\n"
	"	ei							\n"
	"1:								\n"
#else
	/*
	 * Fast, dangerous.  Life is fun, life is good.
	 */
	"	mfc0	$1, $12						\n"
	"	ins	$1, %[flags], 0, 1				\n"
	"	mtc0	$1, $12						\n"
#endif
	"	" __stringify(__irq_disable_hazard) "			\n"
	"	.set	pop						\n"
	: [flags] "=r" (flags)
	: "0" (flags)
	: "memory");
}
#else
/* Functions that require preempt_{dis,en}able() are in mips-atomic.c */
void arch_local_irq_disable(void);
unsigned long arch_local_irq_save(void);
void arch_local_irq_restore(unsigned long flags);
void __arch_local_irq_restore(unsigned long flags);
#endif /* CONFIG_CPU_MIPSR2 || CONFIG_CPU_MIPSR6 */

static void arch_local_irq_enable(void)
{
    __asm__ __volatile__(
            "	.set	push						\n"
            "	.set	reorder						\n"
            "	.set	noat						\n"
            #if   defined(CONFIG_CPU_MIPSR2) || defined(CONFIG_CPU_MIPSR6)
            "	ei							\n"
            #else
            "	mfc0	$1,$12						\n"
            "	ori	$1,0x1f						\n"
            "	xori	$1,0x1e						\n"
            "	mtc0	$1,$12						\n"
            #endif
            "	" __stringify(__irq_enable_hazard) "			\n"
                                                    "	.set	pop						\n"
    : /* no outputs */
    : /* no inputs */
    : "memory");
}

static unsigned long read_cp0_30_flags(void)
{
    unsigned long flags;

    asm __volatile__(
            "	.set	push						\n"
            "	.set	reorder						\n"
            "	mfc0	%[flags], $30					\n"
            "	.set	pop						\n"
            : [flags] "=r" (flags));

    return flags;
}

static unsigned long arch_local_save_flags(void)
{
    unsigned long flags;

    asm __volatile__(
            "	.set	push						\n"
            "	.set	reorder						\n"
            "	mfc0	%[flags], $12					\n"
            "	.set	pop						\n"
            : [flags] "=r" (flags));

    return flags;
}


static int arch_irqs_disabled_flags(unsigned long flags)
{
    return !(flags & 1);
}

#endif /* #ifndef __ASSEMBLY__ */

/*
 * Do the CPU's IRQ-state tracing from assembly code.
 */
#ifdef CONFIG_TRACE_IRQFLAGS
/* Reload some registers clobbered by trace_hardirqs_on */
#ifdef CONFIG_64BIT
# define TRACE_IRQS_RELOAD_REGS						\
	LONG_L	$11, PT_R11(sp);					\
	LONG_L	$10, PT_R10(sp);					\
	LONG_L	$9, PT_R9(sp);						\
	LONG_L	$8, PT_R8(sp);						\
	LONG_L	$7, PT_R7(sp);						\
	LONG_L	$6, PT_R6(sp);						\
	LONG_L	$5, PT_R5(sp);						\
	LONG_L	$4, PT_R4(sp);						\
	LONG_L	$2, PT_R2(sp)
#else
# define TRACE_IRQS_RELOAD_REGS						\
	LONG_L	$7, PT_R7(sp);						\
	LONG_L	$6, PT_R6(sp);						\
	LONG_L	$5, PT_R5(sp);						\
	LONG_L	$4, PT_R4(sp);						\
	LONG_L	$2, PT_R2(sp)
#endif
# define TRACE_IRQS_ON							\
	CLI;	/* make sure trace_hardirqs_on() is called in kernel level */ \
	jal	trace_hardirqs_on
# define TRACE_IRQS_ON_RELOAD						\
	TRACE_IRQS_ON;							\
	TRACE_IRQS_RELOAD_REGS
# define TRACE_IRQS_OFF							\
	jal	trace_hardirqs_off
#else
# define TRACE_IRQS_ON
# define TRACE_IRQS_ON_RELOAD
# define TRACE_IRQS_OFF
#endif
