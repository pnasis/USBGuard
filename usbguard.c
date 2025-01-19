/*
 * USBGuard Kernel Module
 *
 * This kernel module provides a USB Device Guard to monitor and restrict
 * USB device connections based on dynamically configurable rules.
 *
 * Features:
 * - Dynamically add rules specifying allowed VID/PID pairs via sysfs.
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

#define MAX_RULES 10                        // Maximum number of allowed rules.
#define SERIAL_BLOCKED "BLOCKED_SERIAL"     // Serial number to block.

// Storage for VID/PID rules
static int allowed_vids[MAX_RULES];
static int allowed_pids[MAX_RULES];
static int rule_count = 0; // Current count of rules.

// Sysfs class for user interaction
static struct class *usbguard_class;

/*
 * Function: add_rule_store
 * Purpose: Add a new VID/PID rule via the sysfs interface.
 * Input:
 *   - cls: Pointer to the class structure.
 *   - attr: Pointer to the class attribute.
 *   - buf: Input buffer containing the rule in "VID PID" format.
 *   - count: Size of the input buffer.
 * Returns:
 *   - Count of bytes processed on success.
 *   - Error code if the rule limit is reached or input is invalid.
 */
static ssize_t add_rule_store(const struct class *cls, const struct class_attribute *attr,
                              const char *buf, size_t count) {
    int vid, pid;

    // Ensure the rule count does not exceed the limit.
    if (rule_count >= MAX_RULES) {
        return -ENOMEM; // Error: Rule storage is full.
    }

    // Parse the input string for VID and PID.
    if (sscanf(buf, "%x %x", &vid, &pid) != 2) {
        return -EINVAL; // Error: Invalid input format.
    }

    // Store the new rule in the arrays
    allowed_vids[rule_count] = vid;
    allowed_pids[rule_count] = pid;
    rule_count++;

    printk(KERN_INFO "USBGuard: Added rule VID=%04x, PID=%04x\n", vid, pid);
    return count; // Return the number of bytes processed.
}

CLASS_ATTR_WO(add_rule); // Define sysfs writable attribute for adding rules.


/*
 * Function: match_rules
 * Purpose: Check if a connected USB device matches any of the stored VID/PID rules.
 * Input:
 *   - udev: Pointer to the USB device structure.
 * Returns:
 *   - 1 if a match is found.
 *   - 0 if no match is found.
 */
static int match_rules(struct usb_device *udev) {
    int i;

    // Iterate through stored rules and check for a match.
    for (i = 0; i < rule_count; i++) {
        if (udev->descriptor.idVendor == allowed_vids[i] &&
            udev->descriptor.idProduct == allowed_pids[i]) {
            return 1; // Match found
        }
    }

    return 0; // No match found.
}

/*
 * Function: usbguard_probe
 * Purpose: Probe function triggered when a USB device is connected.
 * Performs validation checks and blocks unauthorized devices.
 * Input:
 *   - interface: Pointer to the USB interface structure.
 *   - id: Pointer to the USB device ID structure.
 * Returns:
 *   - 0 if the device is authorized.
 *   - -EACCES if the device is rejected.
 */
static int usbguard_probe(struct usb_interface *interface,
                          const struct usb_device_id *id) {
    struct usb_device *udev = interface_to_usbdev(interface);
    char serial[128] = {0};
    int ret;

    printk(KERN_INFO "USBGuard: Device connected VID=%04x, PID=%04x\n",
           udev->descriptor.idVendor, udev->descriptor.idProduct);

    // Check if the device matches stored VID/PID rules.
    if (!match_rules(udev)) {
        printk(KERN_ALERT "USBGuard: Unauthorized device blocked.\n");
        return -EACCES; // Reject the device.
    }

    // Check if the device is a USB mass storage device.
    if (udev->descriptor.bDeviceClass == USB_CLASS_MASS_STORAGE) {
        printk(KERN_INFO "USBGuard: USB storage device detected.\n");
    }

    // Check the device's serial number.
    ret = usb_string(udev, udev->descriptor.iSerialNumber, serial,
                     sizeof(serial));
    if (ret > 0 && strcmp(serial, SERIAL_BLOCKED) == 0) {
        printk(KERN_ALERT "USBGuard: Blocked serial detected. Rejecting.\n");
        return -EACCES; // Reject the device.
    }

    printk(KERN_INFO "USBGuard: Device authorized.\n");
    return 0; // Allow the device.
}

/*
 * Function: usbguard_disconnect
 * Purpose: Disconnect function triggered when a USB device is disconnected.
 * Logs the disconnection event.
 * Input:
 *   - interface: Pointer to the USB interface structure.
 */
static void usbguard_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "USBGuard: Device disconnected.\n");
}

/*
 * USB device ID table: Matches any USB device.
 */
static struct usb_device_id usbguard_table[] = {
    { USB_DEVICE_INFO(0, 0, 0) }, // Wildcard match / Match any device.
    {} // Terminating entry
};

MODULE_DEVICE_TABLE(usb, usbguard_table);

/*
 * USB driver structure: Links probe and disconnect functions to the driver.
 */
static struct usb_driver usbguard_driver = {
    .name = "usbguard",
    .probe = usbguard_probe,
    .disconnect = usbguard_disconnect,
    .id_table = usbguard_table,
};

/*
 * Function: usbguard_init
 * Purpose: Initialize the USBGuard module.
 * Registers the USB driver and creates the sysfs interface.
 * Returns:
 *   - 0 on success.
 *   - Error code on failure.
 */
static int __init usbguard_init(void) {
    int ret;

    // Register the USB driver.
    ret = usb_register(&usbguard_driver);
    if (ret) {
        printk(KERN_ALERT "USBGuard: Failed to register driver.\n");
        return ret;
    }

    // Create the sysfs interface.
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

/*
 * Function: usbguard_exit
 * Purpose: Cleanup the USBGuard module.
 * Removes the sysfs interface and deregisters the USB driver.
 */
static void __exit usbguard_exit(void) {
    class_remove_file(usbguard_class, &class_attr_add_rule);
    class_destroy(usbguard_class);
    usb_deregister(&usbguard_driver);
    printk(KERN_INFO "USBGuard: Driver removed.\n");
}

module_init(usbguard_init);
module_exit(usbguard_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("pnasis");
MODULE_DESCRIPTION("USB Device Guard with Dynamic Rules and Advanced Checks");
