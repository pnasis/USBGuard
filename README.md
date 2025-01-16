# USB Device Guard Driver

A Linux kernel module to monitor and restrict USB device connections based on dynamically configurable rules. This project is aimed at enhancing system security by allowing only authorized USB devices to connect.

---

## Features

### Core Features
1. **Dynamic Rule Management**:
   - Add or remove rules for allowed USB devices using Vendor ID (VID) and Product ID (PID).
   - Configuration via sysfs interface.

2. **Advanced Device Checks**:
   - Restrict USB devices based on:
     - Device Class (e.g., USB storage, keyboards).
     - Serial Numbers or Unique Device IDs.

3. **Logging and Alerts**:
   - Kernel logs for allowed and blocked USB device events.

---

## Getting Started

### Prerequisites
- A Linux distribution with kernel headers installed.
- Basic knowledge of compiling and loading kernel modules.
- USB devices for testing (both allowed and blocked).

### Setup

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

4. **Verify Kernel Logs**:
   ```bash
   dmesg | tail
   ```

5. **Add Rules**:
   Add a new rule using sysfs to allow a specific USB device:
   ```bash
   echo "0x1234 0x5678" > /sys/class/usbguard/add_rule
   ```
   - Replace `0x1234` with the VID and `0x5678` with the PID of your device.

6. **Test the Driver**:
   - Connect USB devices and monitor logs for access decisions.

---

## Usage

### Adding Rules
To dynamically add rules for allowed USB devices:
```bash
echo "<VID> <PID>" > /sys/class/usbguard/add_rule
```
Example:
```bash
echo "0x0781 0x5567" > /sys/class/usbguard/add_rule
```

### Removing the Module
To remove the kernel module:
```bash
sudo rmmod usbguard
```

### Viewing Logs
To see recent kernel logs related to the module:
```bash
dmesg | tail -n 20
```

---

## Implementation Details

### File Structure
- `usbguard.c`: Main source file containing the kernel module implementation.
- `Makefile`: Build script for compiling and managing the kernel module.
- `README.md`: Documentation for the project.

### Dynamic Rule Management
Rules are stored dynamically in the kernel and managed through the sysfs interface:
- **Add Rule**: Write `VID PID` to `/sys/class/usbguard/add_rule`.
- **Rule Matching**: Each connected device is checked against the stored rules.

### Advanced Device Checks
The module performs additional checks for:
1. **Device Classes**:
   - Example: USB storage (class 08), keyboards (class 03).
2. **Serial Numbers**:
   - Matches serial numbers against a blocklist (e.g., `BLOCKED_SERIAL`).

---

## Contributing
Contributions are welcome! Please open an issue or submit a pull request for any enhancements or bug fixes.

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
This project is licensed under the [GPL v3](https://www.gnu.org/licenses/gpl-3.0.en.html).

---

## Author
- **Prodromos Nasis** - [pnasis](https://github.com/pnasis)

---

## Acknowledgments
- Linux Kernel Development Documentation.
- Open-source contributors and community resources.




