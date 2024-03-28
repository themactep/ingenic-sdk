#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenIPC");
MODULE_DESCRIPTION("GPIO Claimer");

#define PROC_DIR "gpio_claim"
#define PROC_ENTRY "gpio"

static struct proc_dir_entry *claim_proc_dir;

static int debug = 1;  // Default: Debugging on
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable debugging output: 0=off, 1=on");

#define DEBUG_PRINTK(fmt, ...) do { if (debug) printk(fmt, ##__VA_ARGS__); } while (0)
//#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

int claim_gpio(int gpio) {
    dynamic_pr_debug("gpio_claim: GPIO[%i] Requesting...\n", gpio);

    if (!gpio_is_valid(gpio)) {
        dynamic_pr_debug("gpio_claim: GPIO[%i] is not valid.\n", gpio);
        return -1;
    }

    if (gpio_request(gpio, "gpio_claimer") < 0) {
        dynamic_pr_debug("gpio_claim: Failed to request GPIO[%i]. It might be already in use or there's a conflict.\n", gpio);
        return -1;
    }

    dynamic_pr_debug("gpio_claim: GPIO[%i] Setting direction...\n", gpio);
    gpio_direction_output(gpio, 0);
    dynamic_pr_debug("gpio_claim: GPIO[%i] Exporting...\n", gpio);
    gpio_export(gpio, true);

    return 0;
}

ssize_t claim_proc_write(struct file *filp, const char *buf, size_t len, loff_t *off) {
    int gpio;
    int ret = 0;
    char cmd[4] = {0};

    if (len > 4) {
        return -EFAULT;
    }
    if (copy_from_user(cmd, buf, len)) {
        return -EFAULT;
    }
    gpio = simple_strtoul(cmd, NULL, 0);
    ret = claim_gpio(gpio);
    if (ret) {
        DEBUG_PRINTK("gpio_claim: GPIO[%i] Error %i \n", gpio, ret);
        return -EFAULT;
    } else {
        DEBUG_PRINTK("gpio_claim: GPIO[%i] Claiming...\n", gpio);
    }

    return len;
}

static const struct file_operations claim_proc_fops = {
    .owner = THIS_MODULE,
    .write = claim_proc_write,
};

static __init int init_claim(void) {
    claim_proc_dir = proc_mkdir(PROC_DIR, NULL);
    if (!claim_proc_dir) {
        dynamic_pr_debug("gpio_claim: err: proc_mkdir failed\n");
        return -ENOMEM;
    }

    if (!proc_create(PROC_ENTRY, 0644, claim_proc_dir, &claim_proc_fops)) {
        dynamic_pr_debug("gpio_claim: err: proc_create failed\n");
        proc_remove(claim_proc_dir);
        return -ENOMEM;
    }

    printk("GPIO claim module (c) OpenIPC.org\n");
    return 0;
}

static __exit void exit_claim(void) {
    remove_proc_entry(PROC_ENTRY, claim_proc_dir);
    proc_remove(claim_proc_dir);
}

module_init(init_claim);
module_exit(exit_claim);
