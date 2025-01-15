# Makefile for USB Device Guard Driver

# Specify the kernel build directory
KDIR := /lib/modules/$(shell uname -r)/build

# Name of the module
MODULE := usbguard

# Default target
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean up
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Load the module
load:
	sudo insmod $(MODULE).ko

# Unload the module
unload:
	sudo rmmod $(MODULE)

# Show kernel logs (for debugging)
logs:
	dmesg | tail -n 20
