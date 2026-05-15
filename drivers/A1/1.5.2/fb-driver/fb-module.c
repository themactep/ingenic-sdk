/*
 * Ingenic Framebuffer of A series SoC.
 *
 * Copyright 2022
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

extern int ingenic_fb_init(void);
extern void ingenic_fb_exit(void);

static int __init fb_module_init(void)
{
	return ingenic_fb_init();
}

static void __exit fb_module_exit(void)
{
	ingenic_fb_exit();
}

module_init(fb_module_init);
module_exit(fb_module_exit);

MODULE_DESCRIPTION("Ingenic FB Driver");
MODULE_AUTHOR("Sadick.wjxu@ingenic.com>");
MODULE_LICENSE("GPL v2");
