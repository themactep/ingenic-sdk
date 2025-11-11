#include <linux/types.h> /* for bool in old headers */
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

extern struct platform_device jz_button_device;
extern struct gpio_keys_button board_buttons[];
extern struct gpio_keys_platform_data board_button_data;

#define MAX_ADDITIONAL_BUTTONS 10

static struct gpio_keys_button *new_buttons;
static struct gpio_keys_platform_data new_button_data;
static struct platform_device *new_button_device;

static void gpio_keys_device_release(struct device *dev)
{
    /* nothing to free here; platform data owned by module */
}
static char *gpio_config = "";
module_param(gpio_config, charp, 0000);
MODULE_PARM_DESC(gpio_config, "GPIO configuration: <KEYCODE,GPIO,ACTIVE_LOW>;...");

static int parse_gpio_config(void)
{
    char *token;
    char *cur;
    int button_count = 0;
    int existing_buttons = board_button_data.nbuttons;

    cur = kstrdup(gpio_config, GFP_KERNEL);
    if (!cur)
        return -ENOMEM;

    while ((token = strsep(&cur, ";")) != NULL && button_count < MAX_ADDITIONAL_BUTTONS) {
        int keycode, gpio, active_low;
        if (sscanf(token, "%d,%d,%d", &keycode, &gpio, &active_low) == 3) {
            new_buttons[existing_buttons + button_count].code = keycode;
            new_buttons[existing_buttons + button_count].gpio = gpio;
            new_buttons[existing_buttons + button_count].active_low = active_low;
            new_buttons[existing_buttons + button_count].desc = "GPIO Button";
            new_buttons[existing_buttons + button_count].type = EV_KEY;
            button_count++;
        }
    }

    new_button_data.buttons = new_buttons;
    new_button_data.nbuttons = existing_buttons + button_count;
    kfree(cur);
    return 0;
}

static int __init add_gpio_keys_init(void)
{
    int ret, i, existing_buttons, additional_buttons = 0;
    char *cur;

    existing_buttons = board_button_data.nbuttons;

    // Count additional buttons specified in gpio_config
    cur = kstrdup(gpio_config, GFP_KERNEL);
    if (cur) {
        while (strsep(&cur, ";") != NULL && additional_buttons < MAX_ADDITIONAL_BUTTONS) {
            additional_buttons++;
        }
        kfree(cur);
    }

    // Allocate memory for new buttons array
    new_buttons = kzalloc(sizeof(struct gpio_keys_button) * (existing_buttons + additional_buttons),
                          GFP_KERNEL);
    if (!new_buttons) {
        return -ENOMEM;
    }

    // Copy existing buttons to new buttons array
    for (i = 0; i < existing_buttons; i++) {
        new_buttons[i] = board_buttons[i];
    }

    // Parse and add new buttons
    ret = parse_gpio_config();
    if (ret) {
        kfree(new_buttons);
        return ret;
    }

    // Initialize new platform data
    new_button_data.buttons = new_buttons;
    new_button_data.nbuttons = existing_buttons + additional_buttons;

    // Allocate and register the new platform device
    new_button_device = platform_device_alloc("gpio-keys", -1);
    if (!new_button_device) {
        ret = -ENOMEM;
        goto err_alloc;
    }

    new_button_device->dev.platform_data = &new_button_data;
    new_button_device->dev.release = gpio_keys_device_release;

    // Unregister the existing device
    platform_device_unregister(&jz_button_device);

    // Register the new device
    ret = platform_device_add(new_button_device);
    if (ret) {
        platform_device_put(new_button_device);
        goto err_alloc;
    }

    printk(KERN_INFO "Additional GPIO keys device registered with %d buttons\n",
           new_button_data.nbuttons);
    return 0;

err_alloc:
    kfree(new_buttons);
    return ret;
}

static void __exit add_gpio_keys_exit(void)
{
    // Clear the additional buttons data
    int existing_buttons = board_button_data.nbuttons;
    int i;

    if (new_button_device) {
        platform_device_unregister(new_button_device);
        new_button_device = NULL;
    }

    for (i = existing_buttons; i < new_button_data.nbuttons; i++) {
        new_buttons[i].code = 0;
        new_buttons[i].gpio = 0;
        new_buttons[i].active_low = 0;
        new_buttons[i].desc = NULL;
        new_buttons[i].type = 0;
    }

    printk(KERN_INFO "Cleared additional GPIO keys data\n");

    kfree(new_buttons);
}

module_init(add_gpio_keys_init);
module_exit(add_gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gtxaspec/thingino");
MODULE_DESCRIPTION("Add gpio-keys Dynamically");
MODULE_VERSION("0.1");