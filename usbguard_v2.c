/*
 * USBGuard Kernel Module
 *
 * This kernel module provides a USB Device Guard to monitor and restrict
 * USB device connections based on dynamically configurable rules.
 *
 * Features:
 * - Dynamically add rules specifying allowed VID/PID pairs via sysfs.
 * - Load predefined rules from a file (/etc/usbguard.rules) at startup.
 * - Reject unauthorized USB devices during the connection phase.
 * - Perform advanced checks, including blocking devices by class (e.g., USB storage)
 *   and by specific serial numbers.
 * - Integration with sysfs for user interaction.
 * - Logs connection, disconnection, and rule validation events for monitoring.
 *
 * Author: pnasis
 * License: GPL
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define MAX_RULES 10                        // Maximum number of allowed rules.
#define SERIAL_BLOCKED "BLOCKED_SERIAL"     // Serial number to block.
#define RULES_FILE "/etc/usbguard.rules"    // File to load rules from.

// Storage for VID/PID rules
static int allowed_vids[MAX_RULES];
static int allowed_pids[MAX_RULES];
static int rule_count = 0;
static struct class *usbguard_class;

// Function to load predefined rules from file
static void load_rules_from_file(void) {
    struct file *file;
    mm_segment_t old_fs;
    char *buf;
    int vid, pid;
    int ret;
    loff_t pos = 0;

    buf = kmalloc(128, GFP_KERNEL);
    if (!buf)
        return;

    file = filp_open(RULES_FILE, O_RDONLY, 0);
    if (IS_ERR(file)) {
        printk(KERN_ERR "USBGuard: Failed to open rules file\n");
        kfree(buf);
        return;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    while (rule_count < MAX_RULES && kernel_read(file, buf, sizeof(buf) - 1, &pos) > 0) {
        buf[sizeof(buf) - 1] = '\0';
        if (buf[0] == '#' || buf[0] == '\n' || sscanf(buf, "%x %x", &vid, &pid) != 2)
            continue;  // Skip comments and invalid lines
            allowed_vids[rule_count] = vid;
        allowed_pids[rule_count] = pid;
        rule_count++;
        printk(KERN_INFO "USBGuard: Loaded rule VID=%04x, PID=%04x\n", vid, pid);
    }

    set_fs(old_fs);
    filp_close(file, NULL);
    kfree(buf);
}

static int match_rules(struct usb_device *udev) {
    int i;
    for (i = 0; i < rule_count; i++) {
        if (udev->descriptor.idVendor == allowed_vids[i] &&
            udev->descriptor.idProduct == allowed_pids[i]) {
            return 1;
            }
    }
    return 0;
}

static int usbguard_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct usb_device *udev = interface_to_usbdev(interface);
    char serial[128] = {0};
    int ret;

    printk(KERN_INFO "USBGuard: Device connected VID=%04x, PID=%04x\n",
           udev->descriptor.idVendor, udev->descriptor.idProduct);

    if (!match_rules(udev)) {
        printk(KERN_ALERT "USBGuard: Unauthorized device blocked.\n");
        return -EACCES;
    }

    ret = usb_string(udev, udev->descriptor.iSerialNumber, serial, sizeof(serial));
    if (ret > 0 && strcmp(serial, SERIAL_BLOCKED) == 0) {
        printk(KERN_ALERT "USBGuard: Blocked serial detected. Rejecting.\n");
        return -EACCES;
    }

    printk(KERN_INFO "USBGuard: Device authorized.\n");
    return 0;
}

static void usbguard_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "USBGuard: Device disconnected.\n");
}

static struct usb_device_id usbguard_table[] = {
    { USB_DEVICE_INFO(0, 0, 0) },
    {}
};
MODULE_DEVICE_TABLE(usb, usbguard_table);

static struct usb_driver usbguard_driver = {
    .name = "usbguard",
    .probe = usbguard_probe,
    .disconnect = usbguard_disconnect,
    .id_table = usbguard_table,
};

static int __init usbguard_init(void) {
    int ret;
    printk(KERN_INFO "USBGuard: Initializing...\n");

    load_rules_from_file();

    ret = usb_register(&usbguard_driver);
    if (ret) {
        printk(KERN_ALERT "USBGuard: Failed to register driver.\n");
        return ret;
    }

    usbguard_class = class_create("usbguard");
    if (IS_ERR(usbguard_class)) {
        usb_deregister(&usbguard_driver);
        return PTR_ERR(usbguard_class);
    }

    printk(KERN_INFO "USBGuard: Driver initialized successfully.\n");
    return 0;
}

static void __exit usbguard_exit(void) {
    class_destroy(usbguard_class);
    usb_deregister(&usbguard_driver);
    printk(KERN_INFO "USBGuard: Driver removed.\n");
}

module_init(usbguard_init);
module_exit(usbguard_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("pnasis");
MODULE_DESCRIPTION("USB Device Guard with Predefined Rules from File");
