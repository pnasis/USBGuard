# Define the module name
obj-m := usbguard.o

# Define the kernel build directory
KDIR := /lib/modules/$(shell uname -r)/build

# Define the current directory
PWD := $(shell pwd)

# Default target
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean target
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
