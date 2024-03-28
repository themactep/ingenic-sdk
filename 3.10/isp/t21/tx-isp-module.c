#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/bug.h>
#include "tx-isp-module.h"

static int __init tx_isp_driver_init(void)
{
	return tx_isp_init();
}
static void __exit tx_isp_driver_exit(void)
{
	return tx_isp_exit();
}

module_init(tx_isp_driver_init);
module_exit(tx_isp_driver_exit);

MODULE_AUTHOR("Ingenic xhshen");
MODULE_DESCRIPTION("tx isp driver");
MODULE_LICENSE("GPL");
