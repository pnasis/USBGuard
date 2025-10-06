/*
 * USBGuard Kernel Module
 *
 * This module reads allowed USB device rules from /etc/usbguard.rules
 * and enforces them during device connection.
 *
 * Features:
 * - Reads VID/PID rules from /etc/usbguard.rules on module load.
 * - Ignores empty lines and comments in the rules file.
 * - Blocks unauthorized USB devices.
 * - Checks for blocked serial numbers.
 *
 * Author: pnasis
 * License: GPL
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define MAX_RULES 10
#define SERIAL_BLOCKED "BLOCKED_SERIAL"
#define RULES_FILE "/etc/usbguard.rules"

static int allowed_vids[MAX_RULES];
static int allowed_pids[MAX_RULES];
static int rule_count = 0;

/* Function to load rules from file */
static int load_rules_from_file(void) {
    struct file *file;
    char buf[128];
    int vid, pid;
    ssize_t bytes_read;
    loff_t pos = 0;

    file = filp_open(RULES_FILE, O_RDONLY, 0);
    if (IS_ERR(file)) {
        printk(KERN_ALERT "USBGuard: Failed to open rules file.\n");
        return PTR_ERR(file);
    }

    while (rule_count < MAX_RULES) {
        bytes_read = kernel_read(file, buf, sizeof(buf) - 1, &pos);
        if (bytes_read <= 0)
            break;
        buf[bytes_read] = '\0';
        if (buf[0] == '#' || buf[0] == '\n' || sscanf(buf, "%x %x", &vid, &pid) != 2)
            continue;  // Skip comments and invalid lines
        allowed_vids[rule_count] = vid;
        allowed_pids[rule_count] = pid;
        rule_count++;
        printk(KERN_INFO "USBGuard: Loaded rule VID=%04x, PID=%04x\n", vid, pid);
    }

    filp_close(file, NULL);
    return 0;
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

    printk(KERN_INFO "USBGuard: Device connected VID=%04x, PID=%04x\n", udev->descriptor.idVendor, udev->descriptor.idProduct);

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
    ret = load_rules_from_file();
    if (ret)
        return ret;
    ret = usb_register(&usbguard_driver);
    if (ret)
        printk(KERN_ALERT "USBGuard: Failed to register driver.\n");
    else
        printk(KERN_INFO "USBGuard: Driver initialized successfully.\n");
    return ret;
}

static void __exit usbguard_exit(void) {
    usb_deregister(&usbguard_driver);
    printk(KERN_INFO "USBGuard: Driver removed.\n");
}

module_init(usbguard_init);
module_exit(usbguard_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("pnasis");
MODULE_DESCRIPTION("USB Device Guard with Rule File Loading");
