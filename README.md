# serial_helper

A lightweight, SOLID-designed C++ serial port library for Linux (POSIX termios), targeting automotive embedded systems — in particular, GNSS/UBX hardware communication in Location module applications.

## Features

- **SOLID architecture** — segregated reader/writer interfaces, dependency-invertible abstractions, open for extension without modification.
- **Thread-safe wrapper** — `serial_wrapper` guards all I/O behind a mutex, safe to share between a reader thread (UBX frame pump) and a writer thread (UBX command sender).
- **Rich configuration** — covers all standard UART baud rates (50 bps to 4 Mbps), data bits, parity, stop bits, and flow control (none / hardware RTS-CTS / software XON-XOFF).
- **GNSS-ready factory helpers** — one-liner configs for u-blox M8/M9/F9 default (9600-8N1) and high-speed (115200-8N1) modes.
- **Structured exception hierarchy** — fine-grained catch at any level (`serial_open_error`, `serial_config_error`, `serial_read_error`, `serial_write_error`, `serial_timeout_error`).
- **Reconnect / reconfigure** — built-in recovery from framing errors and baud-rate switching without restarting the application.
- **C++17**, no external dependencies beyond the C++ standard library and POSIX.

---

## Repository Layout

```
serial_helper/
├── serial_types.h        # Enumerations: baud_rate, data_bits, parity, stop_bits, flow_control
├── serial_config.h       # serial_config struct + gnss_default / gnss_high_speed / custom factories
├── serial_exception.h    # Exception hierarchy (serial_error and five derived types)
├── i_serial_reader.h     # ISP: read-only interface (read_byte, read_chars, read_string, read_bytes)
├── i_serial_writer.h     # ISP: write-only interface (write_byte, write_chars, write_string, write_bytes)
├── i_serial_port.h       # Full-duplex interface (inherits reader + writer) + lifecycle (open/close/flush/drain)
├── serial_port.h/.cpp    # Concrete POSIX termios implementation of i_serial_port
├── serial_wrapper.h/.cpp # High-level, thread-safe session manager (DI-based)
├── test_serial.cpp       # Test/demo: 5 scenarios (1-4 without hardware, 5 with real GNSS device)
├── Makefile              # Build rules; supports cross-compilation for Yocto targets
└── architecture.puml     # PlantUML class + sequence diagrams
```

---

## Architecture Overview

```
gnss_controller (consumer)
        │
        │  depends on abstraction only
        ▼
 serial_wrapper          ← owns & guards i_serial_port via std::unique_ptr + std::mutex
        │
        ▼
  i_serial_port          ← inherits i_serial_reader + i_serial_writer
        │
        ▼
  serial_port            ← POSIX termios, open()/read()/write()/tcsetattr()
```

Consumers that only receive data depend on `i_serial_reader`; those that only send data depend on `i_serial_writer`. The combined `i_serial_port` is used by `serial_wrapper`. High-level components (e.g. `gnss_controller`) interact exclusively through `serial_wrapper` and never touch the raw port directly.

---

## Building

```bash
# Development host (x86-64)
make

# Run tests without hardware (scenarios 1–4)
make run

# Run full hardware test (scenario 5)
GNSS_DEVICE=/dev/ttyUSB0 make run-hw

# Cross-compile for an aarch64 Yocto target
make CXX=aarch64-poky-linux-g++ \
     CXXFLAGS="-std=c++17 -Wall -Wextra -O2 --sysroot=<SDK_SYSROOT>"

# Clean
make clean
```

**Requirements:** GCC/G++ 7+ with C++17 support, Linux kernel 4.x or later.

---

## Quick Start

### 1. Open a port with a factory config

```cpp
#include "serial_wrapper.h"
#include "serial_port.h"
#include "serial_config.h"
#include "serial_exception.h"

using namespace serial;

// Inject a concrete serial_port into the thread-safe wrapper
auto wrapper = std::make_unique<serial_wrapper>(std::make_unique<serial_port>());

// Use factory helper for u-blox default: 9600-8N1, 100 ms read timeout
serial_config cfg = serial_config::gnss_default("/dev/ttyUSB0");

try {
    wrapper->initialize(cfg);
} catch (const serial_open_error& e) {
    // device node not found, permission denied, etc.
} catch (const serial_config_error& e) {
    // unsupported baud rate or termios parameters
}
```

### 2. Read NMEA sentences

```cpp
std::string line;
while (wrapper->is_ready()) {
    ssize_t n = wrapper->read_string(line, 256);
    if (n > 0) {
        // process NMEA sentence in `line`
    }
}
```

### 3. Send a UBX binary command

```cpp
// UBX-CFG-RATE: set 10 Hz measurement rate
const std::vector<uint8_t> ubx_cfg_rate = {
    0xB5, 0x62, 0x06, 0x08, 0x06, 0x00,
    0x64, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x7A, 0x12
};
wrapper->write_bytes(ubx_cfg_rate);
wrapper->drain(); // ensure the chip receives it before reading the ACK
```

### 4. Reconfigure baud rate at runtime

```cpp
// Switch to 115200 after sending UBX-CFG-PRT
wrapper->reconfigure(serial_config::gnss_high_speed("/dev/ttyUSB0"));
```

### 5. Recover from a framing error

```cpp
wrapper->reconnect(); // closes and re-opens with the current config
```

---

## Configuration Reference

| Field | Type | Default | Description |
|---|---|---|---|
| `device_path` | `std::string` | `""` | Device node, e.g. `/dev/ttyS0`, `/dev/ttyUSB0` |
| `baud` | `baud_rate` | `B_9600` | Line speed (50 bps – 4 Mbps) |
| `data` | `data_bits` | `EIGHT` | Character width (5–8 bits) |
| `par` | `parity` | `NONE` | `NONE`, `ODD`, `EVEN` |
| `stop` | `stop_bits` | `ONE` | `ONE` or `TWO` |
| `flow` | `flow_control` | `NONE` | `NONE`, `HARDWARE` (RTS/CTS), `SOFTWARE` (XON/XOFF) |
| `read_timeout_ms` | `int` | `0` | `0` = blocking; `>0` = timeout in milliseconds |
| `non_blocking` | `bool` | `false` | `true` applies `O_NONBLOCK` on `open()` |

**Factory helpers (`serial_config` static methods):**

| Helper | Baud | Frame | Timeout | Typical use |
|---|---|---|---|---|
| `gnss_default(dev)` | 9600 | 8N1 | 100 ms | u-blox factory default on UART1 |
| `gnss_high_speed(dev)` | 115200 | 8N1 | 100 ms | After sending UBX-CFG-PRT baud switch |
| `custom(dev, baud)` | any | 8N1 | 100 ms | Any non-standard rate |

---

## Exception Hierarchy

```
std::runtime_error
└── serial_error
    ├── serial_open_error     open() failed (device not found, permission denied, busy)
    ├── serial_config_error   termios apply failed (unsupported baud rate, bad parameters)
    ├── serial_read_error     read() / ioctl(FIONREAD) returned an error
    ├── serial_write_error    write() returned an error
    └── serial_timeout_error  operation exceeded its configured deadline
```

Catch at any granularity:

```cpp
try { /* ... */ }
catch (const serial::serial_timeout_error& e) { /* handle timeout specifically */ }
catch (const serial::serial_error& e)         { /* handle any serial error */ }
catch (const std::runtime_error& e)           { /* fallback */ }
```

---

## License

MIT — see [LICENSE](LICENSE).
