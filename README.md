# USBGuard Kernel Module

## Overview
The **USBGuard Kernel Module** is a Linux kernel module that provides a robust mechanism to monitor and restrict USB device connections. It allows dynamic configuration of rules to authorize or block USB devices based on their Vendor ID (VID), Product ID (PID), class, or serial number. The module integrates with `sysfs` for user interaction and logs all connection and disconnection events.

---

## Features
1. **Dynamic Rule Configuration**: Add allowed VID/PID rules dynamically via a `sysfs` interface.
2. **Rule File Support**:
   - Reads allowed USB devices from `/etc/usbguard.rules`.
   - Supports comments and empty lines for better organization.
3. **Advanced USB Checks**:
   - Block USB devices based on their VID/PID.
   - Identify and block USB devices based on specific serial numbers.
   - Detect and monitor USB device classes, such as USB mass storage.
4. **Security Enforcement**:
   - Unauthorized USB devices are blocked during the connection phase.
   - Logs unauthorized connection attempts for auditing.
5. **Lightweight Design**: Integrates seamlessly with the Linux kernel.

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

3. **Install the Module and Setup Environment**:
   ```bash
   sudo ./install.sh
   ```

4. **Verify the Module**:
   ```bash
   lsmod | grep usbguard
   ```

---

## Usage

### Using the Install Script
The `install.sh` script automates the installation process:
- Builds the module.
- Loads the module into the kernel.
- Copies the default rule file to `/etc/usbguard.rules`.

To run the script:
```bash
sudo ./install.sh
```

### Adding Rules

To add a rule for allowing a specific USB device (based on VID/PID), edit `/etc/usbguard.rules`:

Example:
```bash
# Allow specific USB device
1234 5678
```

### Logging
The module logs events to the kernel log. Use `dmesg` to view connection, disconnection, and validation logs:
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
- `usbguard.rules`: Default rule file with sample configurations.
- `install.sh`: Installation script to set up the environment and copy necessary files.
- `README.md`: Documentation for the project.

---

## Architecture

### Rule Management
- Rules are loaded from `/etc/usbguard.rules` on module initialization.
- The file supports comments (lines starting with `#`) and empty lines.
- Rules define allowed USB devices using VID/PID format.

### USB Event Handling
- **Probe Function**: Triggered when a USB device is connected. Performs rule matching and advanced checks (class and serial number).
- **Disconnect Function**: Triggered when a USB device is disconnected. Logs the event.

### Logging and Security
- Logs unauthorized connection attempts and authorized device connections.
- Blocks devices based on matching rules, class checks, or blocked serial numbers.

### Dynamic Rule Management
Rules are stored dynamically in the kernel and managed through the rule file:
- **Rule File Loading**: Rules are read from `/etc/usbguard.rules`.
- **Rule Matching**: Each connected device is checked against the stored rules.

---

### Limitations
- The maximum number of rules is currently set to **10** (`MAX_RULES`).
- Rules must be manually updated in `/etc/usbguard.rules`.
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
