/*
 * USBGuard Kernel Module
 *
 * This module demonstrates USB device access control in Linux.
 *
 * Features:
 * - Loads allowed USB device rules (VID/PID) from /etc/usbguard.rules
 * - Supports dynamic modification via sysfs (/sys/usbguard/rules, /sys/usbguard/blocked_serials)
 * - Checks device serial numbers against blocked list
 * - Logs all device connection attempts
 *
 * Notes:
 * - This is a demo: it prevents the driver from binding but does not fully block device usage.
 * - For production, the USB Authorization framework should be used.
 *
 * Author: pnasis
 * License: GPL
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/byteorder/generic.h>

#define MAX_RULES 128
#define MAX_SERIALS 128
#define RULES_FILE "/etc/usbguard.rules"
#define RULE_LINE_MAX 128

struct vidpid {
    u16 vid;
    u16 pid;
};

static struct vidpid *rules;
static size_t rule_count;

static char *blocked_serials[MAX_SERIALS];
static size_t blocked_serial_count;

static DEFINE_MUTEX(rules_lock);

static struct kobject *usbguard_kobj;

/* Trim whitespace */
static char *trim(char *s)
{
    char *end;
    while (isspace(*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace(*end)) *end-- = '\0';
    return s;
}

/* Parse VID/PID line */
static int parse_vidpid_line(char *line, u16 *vid, u16 *pid)
{
    char *p = trim(line);
    unsigned long v, pnum;
    int rc;

    if (p[0] == '\0' || p[0] == '#') return -EINVAL;

    rc = kstrtoul(p, 16, &v);
    if (rc) return rc;
    p = strchr(p, ' ');
    if (!p) return -EINVAL;
    while (*p && isspace(*p)) p++;
    rc = kstrtoul(p, 16, &pnum);
    if (rc) return rc;

    if (v > 0xFFFF || pnum > 0xFFFF) return -ERANGE;
    *vid = (u16)v;
    *pid = (u16)pnum;
    return 0;
}

/* Load rules from file */
static int load_rules_from_file(void)
{
    struct file *filp;
    mm_segment_t oldfs;
    loff_t pos = 0;
    char buf[RULE_LINE_MAX];
    int ret = 0;

    filp = filp_open(RULES_FILE, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        pr_info("usbguard: could not open rules file %s\n", RULES_FILE);
        return PTR_ERR(filp);
    }

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    while (1) {
        ssize_t bytes = kernel_read(filp, buf, RULE_LINE_MAX - 1, &pos);
        char *line, *saveptr;
        if (bytes <= 0)
            break;
        buf[bytes] = '\0';

        line = buf;
        while (line) {
            char *next = strchr(line, '\n');
            if (next) *next++ = '\0';

            u16 vid, pid;
            if (parse_vidpid_line(line, &vid, &pid) == 0) {
                mutex_lock(&rules_lock);
                if (rule_count < MAX_RULES) {
                    rules[rule_count].vid = vid;
                    rules[rule_count].pid = pid;
                    rule_count++;
                    pr_info("usbguard: loaded rule %04x:%04x\n", vid, pid);
                }
                mutex_unlock(&rules_lock);
            }

            line = next;
        }
    }

    set_fs(oldfs);
    filp_close(filp, NULL);
    return ret;
}

/* Match device VID/PID */
static bool match_rules(struct usb_device *udev)
{
    int i;
    u16 vid = le16_to_cpu(udev->descriptor.idVendor);
    u16 pid = le16_to_cpu(udev->descriptor.idProduct);

    mutex_lock(&rules_lock);
    for (i = 0; i < rule_count; i++) {
        if (rules[i].vid == vid && rules[i].pid == pid) {
            mutex_unlock(&rules_lock);
            return true;
        }
    }
    mutex_unlock(&rules_lock);
    return false;
}

/* Check blocked serials */
static bool serial_blocked(const char *s)
{
    size_t i;
    if (!s || s[0] == '\0') return false;

    mutex_lock(&rules_lock);
    for (i = 0; i < blocked_serial_count; i++) {
        if (blocked_serials[i] && strcmp(blocked_serials[i], s) == 0) {
            mutex_unlock(&rules_lock);
            return true;
        }
    }
    mutex_unlock(&rules_lock);
    return false;
}

/* Stub for interface class check */
static bool check_interface_classes(struct usb_interface *interface)
{
    return true;
}

/* Probe function */
static int usbguard_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    char serial[128] = {0};
    int ret;

    pr_info("usbguard: device VID=%04x PID=%04x attached\n",
            le16_to_cpu(udev->descriptor.idVendor),
            le16_to_cpu(udev->descriptor.idProduct));

    if (!match_rules(udev)) {
        pr_alert("usbguard: VID/PID not allowed, rejecting device\n");
        return -EACCES;
    }

    if (!check_interface_classes(interface)) {
        pr_alert("usbguard: interface class not allowed, rejecting device\n");
        return -EACCES;
    }

    if (udev->descriptor.iSerialNumber) {
        ret = usb_string(udev, udev->descriptor.iSerialNumber, serial, sizeof(serial));
        if (ret > 0 && serial_blocked(serial)) {
            pr_alert("usbguard: blocked serial %s, rejecting device\n", serial);
            return -EACCES;
        }
    }

    pr_info("usbguard: device accepted\n");
    return 0;
}

/* Disconnect function */
static void usbguard_disconnect(struct usb_interface *interface)
{
    pr_info("usbguard: device disconnected\n");
}

/* Match all devices (demo purposes) */
static const struct usb_device_id usbguard_table[] = {
    { USB_DEVICE_INFO(0, 0, 0) },
    {}
};
MODULE_DEVICE_TABLE(usb, usbguard_table);

/* USB driver */
static struct usb_driver usbguard_driver = {
    .name = "usbguard_demo",
    .probe = usbguard_probe,
    .disconnect = usbguard_disconnect,
    .id_table = usbguard_table,
};

/* Sysfs: show rules */
static ssize_t rules_show(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    int i;
    mutex_lock(&rules_lock);
    for (i = 0; i < rule_count; i++)
        len += scnprintf(buf+len, PAGE_SIZE-len, "%04x %04x\n",
                         rules[i].vid, rules[i].pid);
        mutex_unlock(&rules_lock);
    return len;
}

/* Sysfs: add rules */
static ssize_t rules_store(struct kobject *k, struct kobj_attribute *attr,
                           const char *buf, size_t count)
{
    char *tmp, *line;
    tmp = kstrdup(buf, GFP_KERNEL);
    if (!tmp) return -ENOMEM;

    line = tmp;
    while (line) {
        char *next = strchr(line, '\n');
        if (next) *next++ = '\0';

        u16 vid, pid;
        if (parse_vidpid_line(trim(line), &vid, &pid) == 0) {
            mutex_lock(&rules_lock);
            if (rule_count < MAX_RULES) {
                rules[rule_count].vid = vid;
                rules[rule_count].pid = pid;
                rule_count++;
                pr_info("usbguard: sysfs added rule %04x:%04x\n", vid, pid);
            }
            mutex_unlock(&rules_lock);
        }

        line = next;
    }
    kfree(tmp);
    return count;
}

static struct kobj_attribute rules_attr = __ATTR(rules, 0664, rules_show, rules_store);

/* Sysfs: show blocked serials */
static ssize_t blocked_show(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
    ssize_t len = 0;
    int i;
    mutex_lock(&rules_lock);
    for (i = 0; i < blocked_serial_count; i++)
        if (blocked_serials[i])
            len += scnprintf(buf+len, PAGE_SIZE-len, "%s\n", blocked_serials[i]);
    mutex_unlock(&rules_lock);
    return len;
}

/* Sysfs: add blocked serials */
static ssize_t blocked_store(struct kobject *k, struct kobj_attribute *attr,
                             const char *buf, size_t count)
{
    char *tmp, *line;
    tmp = kstrdup(buf, GFP_KERNEL);
    if (!tmp) return -ENOMEM;

    line = tmp;
    while (line) {
        char *next = strchr(line, '\n');
        if (next) *next++ = '\0';

        char *s = trim(line);
        if (*s) {
            mutex_lock(&rules_lock);
            if (blocked_serial_count < MAX_SERIALS)
                blocked_serials[blocked_serial_count++] = kstrdup(s, GFP_KERNEL);
            mutex_unlock(&rules_lock);
        }

        line = next;
    }
    kfree(tmp);
    return count;
}

static struct kobj_attribute blocked_attr = __ATTR(blocked_serials, 0664, blocked_show, blocked_store);

/* Module init */
static int __init usbguard_init(void)
{
    int rc;

    rules = kzalloc(sizeof(*rules)*MAX_RULES, GFP_KERNEL);
    if (!rules) return -ENOMEM;

    rule_count = 0;
    blocked_serial_count = 0;

    load_rules_from_file();

    usbguard_kobj = kobject_create_and_add("usbguard", kernel_kobj);
    if (!usbguard_kobj) {
        kfree(rules);
        return -ENOMEM;
    }

    rc = sysfs_create_file(usbguard_kobj, &rules_attr.attr);
    if (rc) goto out_kobj;
    rc = sysfs_create_file(usbguard_kobj, &blocked_attr.attr);
    if (rc) {
        sysfs_remove_file(usbguard_kobj, &rules_attr.attr);
        goto out_kobj;
    }

    rc = usb_register(&usbguard_driver);
    if (rc) {
        pr_alert("usbguard: usb_register failed %d\n", rc);
        sysfs_remove_file(usbguard_kobj, &rules_attr.attr);
        sysfs_remove_file(usbguard_kobj, &blocked_attr.attr);
        kobject_put(usbguard_kobj);
        kfree(rules);
        return rc;
    }

    pr_info("usbguard: demo module loaded\n");
    return 0;

    out_kobj:
    kobject_put(usbguard_kobj);
    kfree(rules);
    return rc;
}

/* Module exit */
static void __exit usbguard_exit(void)
{
    int i;
    usb_deregister(&usbguard_driver);
    sysfs_remove_file(usbguard_kobj, &rules_attr.attr);
    sysfs_remove_file(usbguard_kobj, &blocked_attr.attr);
    kobject_put(usbguard_kobj);

    mutex_lock(&rules_lock);
    for (i = 0; i < blocked_serial_count; i++)
        kfree(blocked_serials[i]);
    mutex_unlock(&rules_lock);

    kfree(rules);
    pr_info("usbguard: demo module unloaded\n");
}

module_init(usbguard_init);
module_exit(usbguard_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("pnasis");
MODULE_DESCRIPTION("USB Device Guard with Rule File Loading");
