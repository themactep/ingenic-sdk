#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>

#define MAX_BUTTONS 10

static struct gpio_keys_button gpio_buttons[MAX_BUTTONS];
static struct gpio_keys_platform_data gpio_keys_data = {
    .buttons = gpio_buttons,
    .nbuttons = 0,
    .poll_interval = 20, // Polling interval in milliseconds
};

static struct platform_device gpio_keys_device = {
    .name = "gpio-keys-polled", // Using "gpio-keys-polled" for the polled version to co-exist
    .id = -1,
    .dev = {
        .platform_data = &gpio_keys_data,
    },
};

static char *gpio_config = "";
module_param(gpio_config, charp, 0000);
MODULE_PARM_DESC(gpio_config, "GPIO configuration: <KEYCODE,GPIO,ACTIVE_LOW>;...");

static int __init parse_gpio_config(void)
{
    char *token;
    char *cur = gpio_config;
    int button_count = 0;

    while ((token = strsep(&cur, ";")) != NULL && button_count < MAX_BUTTONS) {
        int keycode, gpio, active_low;
        if (sscanf(token, "%d,%d,%d", &keycode, &gpio, &active_low) == 3) {
            gpio_buttons[button_count].code = keycode;
            gpio_buttons[button_count].gpio = gpio;
            gpio_buttons[button_count].active_low = active_low;
            gpio_buttons[button_count].desc = "GPIO Button";
            gpio_buttons[button_count].type = EV_KEY;
            button_count++;
        }
    }
    gpio_keys_data.nbuttons = button_count;
    return 0;
}

static int __init gpio_keys_init(void)
{
    int ret;

    ret = parse_gpio_config();
    if (ret)
        return ret;

    ret = platform_device_register(&gpio_keys_device);
    if (ret)
        printk(KERN_ERR "Unable to register gpio-keys-polled device\n");
    else
        printk(KERN_INFO "gpio-keys-polled device registered\n");

    return ret;
}

static void __exit gpio_keys_exit(void)
{
    platform_device_unregister(&gpio_keys_device);
    printk(KERN_INFO "gpio-keys-polled device unregistered\n");
}

module_init(gpio_keys_init);
module_exit(gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gtxaspec/thingino");
MODULE_DESCRIPTION("A GPIO Keys Polled Kernel Module with Configurable GPIOs");
MODULE_VERSION("0.5");
