#!/bin/bash

set -e

# Define installation paths
MODULE_NAME="usbguard"
RULES_FILE="/etc/usbguard.rules"
KERNEL_MODULE_DIR="/lib/modules/$(uname -r)/extra"

# Ensure script is run as root
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root." >&2
    exit 1
fi

# Copy predefined rules file if it does not already exist
if [[ ! -f "$RULES_FILE" ]]; then
    echo "Copying predefined usbguard rules file..."
    cp usbguard.rules "$RULES_FILE"
fi

# Build the kernel module
make clean
make

# Create module directory if it does not exist
mkdir -p "$KERNEL_MODULE_DIR"

# Copy module to kernel directory
cp "$MODULE_NAME.ko" "$KERNEL_MODULE_DIR/"

# Refresh module dependencies
depmod

# Load the module
modprobe "$MODULE_NAME"

# Ensure module loads at boot
echo "$MODULE_NAME" > /etc/modules-load.d/usbguard.conf

# Done
echo "USBGuard module installed and configured successfully."
