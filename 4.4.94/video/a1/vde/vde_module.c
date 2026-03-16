#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

extern int ingenic_vde_init(void);
extern void ingenic_vde_exit(void);

static int __init vo_module_init(void)
{
	return ingenic_vde_init();
}

static void __exit vo_module_exit(void)
{
	ingenic_vde_exit();
}

module_init(vo_module_init);
module_exit(vo_module_exit);

MODULE_AUTHOR("Sadick<weijie.xu@ingenic.com>");
MODULE_DESCRIPTION("VO driver");
MODULE_LICENSE("GPL v2");
