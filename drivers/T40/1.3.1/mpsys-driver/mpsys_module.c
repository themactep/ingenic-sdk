#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>

extern int mpsys_utils_init(void);
extern void mpsys_utils_exit(void);
extern int mpsys_data_init(void);
extern void mpsys_data_exit(void);

static int __init mpsys_init(void)
{
	int ret = 0;

	printk("@@@@@ multi-process system driver init @@@@@\n");

	ret = mpsys_utils_init();
	if (ret)
	{
		printk("mpsys utils init failed!\n");
		goto err_utils_init;
	}

	ret = mpsys_data_init();
	if (ret)
	{
		printk("mpsys data init failed!\n");
		goto err_data_init;
	}

	return ret;

err_data_init:
    mpsys_utils_exit();
err_utils_init:
	return ret;
}

static void __exit mpsys_exit(void)
{
	printk("@@@@@ multi-process system driver exit @@@@@\n");

	mpsys_utils_exit();
	mpsys_data_exit();
}

module_init(mpsys_init);
module_exit(mpsys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wenjie Shi <scott.wjshi@ingenic.com>");
MODULE_DESCRIPTION("Multi process system, contain lock/nmem alloc/process communication.");
