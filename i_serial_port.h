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
 * i_serial_port.h
 * ============================================================
 * Combined abstract interface for a full-duplex serial port.
 *
 * Inherits both i_serial_reader and i_serial_writer, and adds
 * lifecycle management (open / close / flush / drain) plus
 * configuration inspection.
 *
 * Design notes (SOLID):
 *  • Open/Closed  – New transports (e.g. USB-CDC, SPI-UART bridge)
 *    implement this interface without modifying existing code.
 *  • Liskov       – Any i_serial_port implementation can substitute
 *    for any other without changing the consumer's behaviour.
 *  • Dependency Inversion – High-level components (e.g. gnss_controller)
 *    depend on this abstraction, not on serial_port (concrete class).
 */

#ifndef I_SERIAL_PORT_H
#define I_SERIAL_PORT_H

#include "i_serial_reader.h"
#include "i_serial_writer.h"
#include "serial_config.h"

namespace serial
{

class i_serial_port : public i_serial_reader, public i_serial_writer
{
public:
    virtual ~i_serial_port() = default;

    // -----------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------

    /**
     * @brief Open the device described by `config` and apply termios settings.
     *
     * Calling open() on an already-open port implicitly closes it first.
     *
     * @param[in] config  Full port configuration.
     * @return true on success.
     * @throws serial_open_error   if the device node cannot be opened.
     * @throws serial_config_error if termios parameters cannot be applied.
     */
    virtual bool open(const serial_config& config) = 0;

    /**
     * @brief Flush pending I/O, then close the file descriptor.
     *
     * Safe to call on an already-closed port (no-op).
     */
    virtual void close() = 0;

    /**
     * @brief Return whether the port is currently open.
     */
    virtual bool is_open() const = 0;

    // -----------------------------------------------------------
    // Configuration inspection
    // -----------------------------------------------------------

    /**
     * @brief Return the configuration that was last successfully applied.
     *
     * Undefined behaviour if called before a successful open().
     */
    virtual const serial_config& get_config() const = 0;

    // -----------------------------------------------------------
    // Buffer management
    // -----------------------------------------------------------

    /**
     * @brief Discard all data in the kernel Tx and Rx buffers.
     *
     * Equivalent to tcflush(fd, TCIOFLUSH).
     *
     * @return true on success, false if the port is not open.
     */
    virtual bool flush() = 0;

    /**
     * @brief Block until all data in the kernel Tx buffer has been sent.
     *
     * Equivalent to tcdrain(fd).  Call after write_bytes() when you
     * need to guarantee the chip has received the command before
     * reading its ACK.
     *
     * @return true on success, false if the port is not open.
     */
    virtual bool drain() = 0;

    // -----------------------------------------------------------
    // fd accessor – for poll()/epoll()/select() by external components
    // -----------------------------------------------------------
    /**
     * @brief Return the underlying POSIX file descriptor.
     *
     * Intended for use with poll(2)/epoll_ctl(2)/select(2) by external
     * components that need to monitor the port for readability without
     * calling into the serial_port read path directly (e.g. GNSS HAL
     * waiting for UBX/NMEA data on a non-blocking UART fd).
     *
     * @note Valid only while is_open() == true.
     *       Returns -1 when the port is closed; passing -1 to poll()
     *       yields POLLNVAL which is safe to detect and handle.
     * @note The caller MUST NOT close(), dup(), or dup2() this fd.
     *       Ownership remains with serial_port.
     */
    virtual int get_fd() const noexcept = 0;
};

} // namespace serial

#endif // I_SERIAL_PORT_H
