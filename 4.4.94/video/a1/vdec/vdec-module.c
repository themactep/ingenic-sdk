/*
 * Video Class definitions of A1 series SoC.
 *
 * Copyright 2017
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

extern int vdec_init(void);
extern void vdec_exit(void);

static int __init vdec_module_init(void)
{
	return vdec_init();
}

static void __exit vdec_module_exit(void)
{
	vdec_exit();
}

module_init(vdec_module_init);
module_exit(vdec_module_exit);

MODULE_AUTHOR("Ingenic Cason");
MODULE_DESCRIPTION("vdec driver");
MODULE_LICENSE("GPL");
