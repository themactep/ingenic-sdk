/*
 * Image Process Unit of A series SoC.
 *
 * Copyright 2021
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

extern int jz_ipu_init(void);
extern void jz_ipu_exit(void);

static int __init ipu_module_init(void)
{
	return jz_ipu_init();
}

static void __exit ipu_module_exit(void)
{
	jz_ipu_exit();
}

module_init(ipu_module_init);
module_exit(ipu_module_exit);

MODULE_DESCRIPTION("JZ IPU Driver");
MODULE_AUTHOR("Danny.ywhan@ingenic.com>");
MODULE_LICENSE("GPL v2");
