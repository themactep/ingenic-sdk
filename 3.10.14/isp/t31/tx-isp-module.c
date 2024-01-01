/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

extern int tx_isp_init(void);
extern void tx_isp_exit(void);

#ifdef CONFIG_JZ_ISP_TRACE
#include <mach/txx-funcs.h>
extern void *get_driver_common_interfaces(void);
extern void jz_isp_vtable_trace_init(struct jz_driver_common_interfaces *p);
#endif

static int __init tx_isp_module_init(void)
{
#ifdef CONFIG_JZ_ISP_TRACE
	jz_isp_vtable_trace_init(
		(struct jz_driver_common_interfaces *)get_driver_common_interfaces());
#endif
	return tx_isp_init();
}

static void __exit tx_isp_module_exit(void)
{
	tx_isp_exit();
}

module_init(tx_isp_module_init);
module_exit(tx_isp_module_exit);

MODULE_AUTHOR("Ingenic xhshen");
MODULE_DESCRIPTION("tx isp driver");
MODULE_LICENSE("GPL");
