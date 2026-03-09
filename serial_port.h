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
 * serial_port.h
 * ============================================================
 * Concrete Linux (POSIX termios) implementation of i_serial_port.
 *
 * Uses the standard POSIX termios API to configure baud rate,
 * frame format, flow control, and read-timeout.  The underlying
 * file descriptor is opened as O_RDWR | O_NOCTTY; O_NONBLOCK is
 * added when serial_config::non_blocking == true.
 *
 * Thread-safety: this class IS internally synchronised.
 * Every public method acquires an internal mutex before accessing fd_,
 * so the same serial_port instance may safely be used from a reader
 * thread and a writer thread simultaneously.
 *
 * Targeting: Linux 4.x+ (Yocto / embedded).
 *            High baud rates (>=B_460800) require Linux-specific
 *            termios constants and a UART driver that supports them.
 */

#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include "i_serial_port.h"
#include <termios.h>
#include <mutex>

namespace serial
{

class serial_port final : public i_serial_port
{
public:
    serial_port();
    ~serial_port() override;

    // Non-copyable – the fd cannot be duplicated safely.
    serial_port(const serial_port&)            = delete;
    serial_port& operator=(const serial_port&) = delete;

    // Movable
    serial_port(serial_port&& other) noexcept;
    serial_port& operator=(serial_port&& other) noexcept;

    // -----------------------------------------------------------
    // i_serial_port – Lifecycle
    // -----------------------------------------------------------
    bool open(const serial_config& config) override;
    void close() override;
    bool is_open() const override;
    const serial_config& get_config() const override;
    bool flush() override;
    bool drain() override;
    bool reconnect() override;
    bool reconfigure(const serial_config& new_config) override;

    // -----------------------------------------------------------
    // i_serial_reader
    // -----------------------------------------------------------
    ssize_t read_byte(char& c) override;
    ssize_t read_chars(char* buf, std::size_t len) override;
    ssize_t read_uchars(unsigned char* buf, std::size_t len) override;
    ssize_t read_string(std::string& str, std::size_t max_len) override;
    ssize_t read_bytes(std::vector<uint8_t>& buf, std::size_t max_len) override;
    int available() const override;

    // -----------------------------------------------------------
    // i_serial_writer
    // -----------------------------------------------------------
    ssize_t write_byte(char c) override;
    ssize_t write_chars(const char* buf, std::size_t len) override;
    ssize_t write_uchars(const unsigned char* buf, std::size_t len) override;
    ssize_t write_string(const std::string& str) override;
    ssize_t write_bytes(const std::vector<uint8_t>& buf) override;

    // -----------------------------------------------------------
    // fd accessor – for poll()/epoll()/select() by external components
    // -----------------------------------------------------------
    int get_fd() const noexcept override;

private:
    int                   fd_;      ///< POSIX file descriptor; -1 when closed
    serial_config         config_;  ///< Last successfully applied configuration
    mutable std::mutex    mutex_;   ///< Guards all fd_ accesses

    /**
     * @brief Open the device and apply termios settings.
     *
     * Must be called with mutex_ already held.
     * If the port is already open it is closed first.
     */
    bool open_locked(const serial_config& config);

    /**
     * @brief Flush and close the port.
     *
     * Must be called with mutex_ already held.
     * Safe to call when the port is already closed (no-op).
     */
    void close_locked();

    /**
     * @brief Translate serial_config into termios flags and apply them.
     *
     * Must be called with mutex_ already held (invoked from open_locked).
     * Throws serial_config_error on any tcgetattr/tcsetattr failure.
     */
    void apply_termios_config();

    /**
     * @brief Convert baud_rate enum to the POSIX speed_t constant.
     *
     * @throws serial_config_error for unsupported values.
     */
    speed_t baud_to_speed(baud_rate baud) const;
};

} // namespace serial

#endif // SERIAL_PORT_H
