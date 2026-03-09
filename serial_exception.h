/**
 * MIT License
 *
 * Copyright (c) 2026 nguyenchiemminhvu@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * serial_exception.h
 * ============================================================
 * Exception hierarchy for the serial_helper library.
 *
 * All exceptions derive from serial_error, which itself derives
 * from std::runtime_error, so callers can catch at any granularity:
 *
 *   catch (const serial::serial_timeout_error& e) { ... }  // specific
 *   catch (const serial::serial_error& e)         { ... }  // any serial error
 *   catch (const std::runtime_error& e)           { ... }  // any runtime error
 *
 * Hierarchy:
 *   std::runtime_error
 *   └── serial_error
 *       ├── serial_open_error    – open()/fcntl() failed
 *       ├── serial_config_error  – termios configuration failed
 *       ├── serial_read_error    – read()/ioctl(FIONREAD) failed
 *       ├── serial_write_error   – write() failed
 *       └── serial_timeout_error – operation timed out
 */

#ifndef SERIAL_EXCEPTION_H
#define SERIAL_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace serial
{

/**
 * @brief Base class for all serial_helper exceptions.
 */
class serial_error : public std::runtime_error
{
public:
    explicit serial_error(const std::string& msg)
        : std::runtime_error(msg) {}
};

/**
 * @brief Thrown when the device node cannot be opened.
 *
 * Common causes: device path does not exist, permission denied,
 * device busy (another process holds the port).
 */
class serial_open_error : public serial_error
{
public:
    explicit serial_open_error(const std::string& msg)
        : serial_error(msg) {}
};

/**
 * @brief Thrown when applying termios settings fails.
 *
 * Common causes: unsupported baud rate for the hardware driver,
 * invalid combination of data/parity/stop parameters.
 */
class serial_config_error : public serial_error
{
public:
    explicit serial_config_error(const std::string& msg)
        : serial_error(msg) {}
};

/**
 * @brief Thrown when a read operation fails.
 *
 * Does NOT cover timeout (zero-byte return); only covers negative
 * return values from read() / ioctl(FIONREAD).
 */
class serial_read_error : public serial_error
{
public:
    explicit serial_read_error(const std::string& msg)
        : serial_error(msg) {}
};

/**
 * @brief Thrown when a write operation fails.
 */
class serial_write_error : public serial_error
{
public:
    explicit serial_write_error(const std::string& msg)
        : serial_error(msg) {}
};

/**
 * @brief Thrown when an operation exceeds its configured timeout.
 *
 * Raised by higher-level helpers that implement explicit deadline
 * semantics on top of the non-blocking / VTIME termios mechanisms.
 */
class serial_timeout_error : public serial_error
{
public:
    explicit serial_timeout_error(const std::string& msg)
        : serial_error(msg) {}
};

} // namespace serial

#endif // SERIAL_EXCEPTION_H
