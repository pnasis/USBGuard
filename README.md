# USBGuard Kernel Module

## Overview
The **USBGuard Kernel Module** is a Linux kernel module that provides a robust mechanism to monitor and restrict USB device connections. It allows dynamic configuration of rules to authorize or block USB devices based on their Vendor ID (VID), Product ID (PID), class, or serial number. The module integrates with `sysfs` for user interaction and logs all connection and disconnection events.

---

## Features
1. **Dynamic Rule Configuration**: Add allowed VID/PID rules dynamically via a `sysfs` interface.
2. **Advanced USB Checks**:
  - Block USB devices based on their VID/PID.
  - Identify and block USB devices based on specific serial numbers.
  - Detect and monitor USB device classes, such as USB mass storage.
3. **Security Enforcement**:
  - Unauthorized USB devices are blocked during the connection phase.
  - Logs unauthorized connection attempts for auditing.
4. **Lightweight Design**: Integrates seamlessly with the Linux kernel.

---

## Requirements
- Linux kernel version 4.0 or later.
- Root privileges for loading/unloading the module and interacting with `sysfs`.


## Installation

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/pnasis/USB-Device-Guard-Driver.git
   cd USB-Device-Guard-Driver
   ```

2. **Build the Module**:
   ```bash
   make
   ```

3. **Load the Module**:
   ```bash
   sudo insmod usbguard.ko
   ```

4. **Verify the Module**:
Check if the module is loaded using:
   ```bash
   lsmod | grep usbguard
   ```
   
---

## Usage

### Adding Rules

To add a rule for allowing a specific USB device (based on VID/PID), use the `add_rule` attribute provided via `sysfs`.
Format: `VID PID` (in hexadecimal format).

Example:
```bash
echo "1234 5678" > /sys/class/usbguard/add_rule
```

### Logging
The module logs events to the kernel log. Use dmesg to view connection, disconnection, and validation logs:
```bash
dmesg | grep USBGuard
```

### Unloading the Module
To remove the module from the kernel:
```bash
sudo rmmod usbguard
```

---

## File Structure
- `usbguard.c`: Main source file containing the kernel module implementation.
- `Makefile`: Build script for compiling and managing the kernel module.
- `README.md`: Documentation for the project.

---

## Architecture

### Rule Management
- Rules are stored as arrays of VIDs and PIDs in the kernel memory.
- The `sysfs` interface (`/sys/class/usbguard/add_rule`) allows users to dynamically add rules.

### USB Event Handling
- **Probe Function**: Triggered when a USB device is connected. Performs rule matching and advanced checks (class and serial number).
- **Disconnect Function**: Triggered when a USB device is disconnected. Logs the event.

### Logging and Security
- Logs unauthorized connection attempts and authorized device connections.
- Blocks devices based on matching rules, class checks, or blocked serial numbers.


### Dynamic Rule Management
Rules are stored dynamically in the kernel and managed through the sysfs interface:
- **Add Rule**: Write `VID PID` to `/sys/class/usbguard/add_rule`.
- **Rule Matching**: Each connected device is checked against the stored rules.

---

### Limitations
- The maximum number of rules is currently set to **10** (`MAX_RULES`).
- Rules are not persistent across reboots. They need to be re-added after a module reload.
- Only VID/PID, device class, and serial number are checked.

---

## Contributing
Contributions to enhance functionality, performance, or compatibility are welcome! Please open an issue or submit a pull request.

1. Fork the repository.
2. Create a new branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. Commit your changes:
   ```bash
   git commit -m "Add your message here"
   ```
4. Push to the branch:
   ```bash
   git push origin feature/your-feature-name
   ```
5. Open a pull request.

---

## License
This project is licensed under the [GPL v2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).

---

## Author
- **Prodromos Nasis** - [pnasis](https://github.com/pnasis)

---

## Acknowledgments
- Linux Kernel Development Documentation.
- Open-source contributors and community resources.




