#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>

#define MAX_RULES 10
#define SERIAL_BLOCKED "BLOCKED_SERIAL"

// Rule storage
static int allowed_vids[MAX_RULES];
static int allowed_pids[MAX_RULES];
static int rule_count = 0;

// Sysfs class
static struct class *usbguard_class;

// Add rule via sysfs
static ssize_t add_rule_store(const struct class *cls, const struct class_attribute *attr,
                              const char *buf, size_t count) {
    int vid, pid;

    if (rule_count >= MAX_RULES) {
        return -ENOMEM; // Rule limit reached
    }

    // Parse input: expect "VID PID"
    if (sscanf(buf, "%x %x", &vid, &pid) != 2) {
        return -EINVAL; // Invalid input format
    }

    allowed_vids[rule_count] = vid;
    allowed_pids[rule_count] = pid;
    rule_count++;

    printk(KERN_INFO "USBGuard: Added rule VID=%04x, PID=%04x\n", vid, pid);
    return count;
}

CLASS_ATTR_WO(add_rule);

// Check if device matches rules
static int match_rules(struct usb_device *udev) {
    int i;

    for (i = 0; i < rule_count; i++) {
        if (udev->descriptor.idVendor == allowed_vids[i] &&
            udev->descriptor.idProduct == allowed_pids[i]) {
            return 1; // Match found
        }
    }

    return 0; // No match
}

// USB probe function
static int usbguard_probe(struct usb_interface *interface,
                          const struct usb_device_id *id) {
    struct usb_device *udev = interface_to_usbdev(interface);
    char serial[128] = {0};
    int ret;

    printk(KERN_INFO "USBGuard: Device connected VID=%04x, PID=%04x\n",
           udev->descriptor.idVendor, udev->descriptor.idProduct);

    // Check VID/PID rules
    if (!match_rules(udev)) {
        printk(KERN_ALERT "USBGuard: Unauthorized device blocked.\n");
        return -EACCES; // Reject device
    }

    // Check device class (e.g., USB storage)
    if (udev->descriptor.bDeviceClass == USB_CLASS_MASS_STORAGE) {
        printk(KERN_INFO "USBGuard: USB storage device detected.\n");
    }

    // Check serial number
    ret = usb_string(udev, udev->descriptor.iSerialNumber, serial,
                     sizeof(serial));
    if (ret > 0 && strcmp(serial, SERIAL_BLOCKED) == 0) {
        printk(KERN_ALERT "USBGuard: Blocked serial detected. Rejecting.\n");
        return -EACCES; // Reject device
    }

    printk(KERN_INFO "USBGuard: Device authorized.\n");
    return 0; // Allow device
}

// USB disconnect function
static void usbguard_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "USBGuard: Device disconnected.\n");
}

// USB device ID table (wildcard match for all devices)
static struct usb_device_id usbguard_table[] = {
    { USB_DEVICE_INFO(0, 0, 0) }, // Match any device
    {} // Terminating entry
};

MODULE_DEVICE_TABLE(usb, usbguard_table);

// USB driver structure
static struct usb_driver usbguard_driver = {
    .name = "usbguard",
    .probe = usbguard_probe,
    .disconnect = usbguard_disconnect,
    .id_table = usbguard_table,
};

// Initialize driver and sysfs
static int __init usbguard_init(void) {
    int ret;

    // Register USB driver
    ret = usb_register(&usbguard_driver);
    if (ret) {
        printk(KERN_ALERT "USBGuard: Failed to register driver.\n");
        return ret;
    }

    // Create sysfs interface
    usbguard_class = class_create("usbguard");
    if (IS_ERR(usbguard_class)) {
        usb_deregister(&usbguard_driver);
        return PTR_ERR(usbguard_class);
    }

    if (class_create_file(usbguard_class, &class_attr_add_rule)) {
        class_destroy(usbguard_class);
        usb_deregister(&usbguard_driver);
        return -ENOMEM;
    }

    printk(KERN_INFO "USBGuard: Driver initialized successfully.\n");
    return 0;
}

// Cleanup driver and sysfs
static void __exit usbguard_exit(void) {
    class_remove_file(usbguard_class, &class_attr_add_rule);
    class_destroy(usbguard_class);
    usb_deregister(&usbguard_driver);
    printk(KERN_INFO "USBGuard: Driver removed.\n");
}

module_init(usbguard_init);
module_exit(usbguard_exit);

MODULE_LICENSE("GPL v3");
MODULE_AUTHOR("pnasis");
MODULE_DESCRIPTION("USB Device Guard with Dynamic Rules and Advanced Checks");
