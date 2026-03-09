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
 * serial_wrapper.h
 * ============================================================
 * High-level wrapper that manages a serial port session for use
 * by a gnss_controller (or any other hardware-adapter component).
 *
 * Responsibilities:
 *  • Own the i_serial_port implementation (dependency-injected).
 *  • Expose initialize / shutdown / reconfigure for lifecycle
 *    management with clear semantics.
 *  • Forward all read / write operations to the underlying port
 *    so callers never touch the raw interface directly.
 *  • Provide reconnect() to recover from transient UART errors
 *    without requiring the caller to manage the fd lifecycle.
 *
 * Thread-safety:
 *  All operations are thread-safe.  The mutex lives inside the
 *  concrete serial_port (the layer that owns fd_), so protection
 *  is applied exactly where the shared resource is accessed.
 *  serial_wrapper itself is a stateless delegation facade – it
 *  holds no mutable state of its own and therefore needs no
 *  additional locking.
 *
 * Design notes (SOLID):
 *  • Single Responsibility – manages context & lifecycle only;
 *    raw I/O logic and synchronisation live in serial_port.
 *  • Dependency Inversion  – depends on i_serial_port abstraction,
 *    not on serial_port directly; callers inject the implementation
 *    (facilitates unit testing with mock ports).
 *  • Open/Closed – new i_serial_port implementations slot in
 *    without touching serial_wrapper.
 */

#ifndef SERIAL_WRAPPER_H
#define SERIAL_WRAPPER_H

#include "i_serial_port.h"

#include <memory>
#include <string>

namespace serial
{

class serial_wrapper
{
public:
    /**
     * @brief Construct the wrapper with an injected port implementation.
     *
     * The wrapper takes ownership of the port object.  Pass a freshly
     * constructed serial_port (or a test mock) here:
     *
     *   auto wrapper = serial_wrapper(std::make_unique<serial_port>());
     *
     * @param port  Concrete i_serial_port to manage.
     * @throws std::invalid_argument if port is null.
     */
    explicit serial_wrapper(std::shared_ptr<i_serial_port> port);

    /**
     * @brief Destructor – calls shutdown() if the port is still open.
     */
    ~serial_wrapper();

    // Non-copyable (owns a unique_ptr and a mutex)
    serial_wrapper(const serial_wrapper&)            = delete;
    serial_wrapper& operator=(const serial_wrapper&) = delete;

    // -----------------------------------------------------------
    // Lifecycle management
    // -----------------------------------------------------------

    /**
     * @brief Open the port with the given configuration.
     *
     * Idempotent: if the port is already open with the same config,
     * this is a no-op.  If open with different config, the port is
     * closed and re-opened.
     *
     * @param config  Full UART configuration.
     * @return true on success.
     * @throws serial_open_error, serial_config_error on failure.
     */
    bool initialize(const serial_config& config);

    /**
     * @brief Flush and close the port.
     *
     * Safe to call multiple times (no-op when already closed).
     */
    void shutdown();

    /**
     * @brief Close and re-open the port with its current configuration.
     *
     * Delegates directly to i_serial_port::reconnect(), which performs
     * the close/open sequence atomically under the port's internal mutex.
     *
     * @return true on success.
     * @throws serial_open_error, serial_config_error on failure.
     */
    bool reconnect();

    /**
     * @brief Apply a new configuration to the already-open port.
     *
     * Delegates directly to i_serial_port::reconfigure(), which performs
     * the close/open sequence atomically under the port's internal mutex.
     *
     * @param new_config  Configuration to apply.
     * @return true on success.
     * @throws serial_open_error, serial_config_error on failure.
     */
    bool reconfigure(const serial_config& new_config);

    // -----------------------------------------------------------
    // Status
    // -----------------------------------------------------------

    /**
     * @brief Return true if the port is open and ready for I/O.
     */
    bool is_ready() const;

    /**
     * @brief Return the configuration currently applied to the port.
     *
     * Caller must ensure is_ready() before calling this.
     */
    const serial_config& get_config() const;

    // -----------------------------------------------------------
    // fd accessor – for poll()/epoll()/select() by external components
    // -----------------------------------------------------------
    /**
     * @brief Return the underlying POSIX file descriptor of the managed port.
     *
     * Allows external components (e.g. GNSS HAL frame pump) to use
     * poll(2)/epoll_ctl(2)/select(2) directly on the UART fd without
     * going through the serial_wrapper read path.
     *
     * Typical usage:
     * @code
     *   struct pollfd pfd{ wrapper.get_fd(), POLLIN, 0 };
     *   if (poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN))
     *       wrapper.read_bytes(buf, max_len);
     * @endcode
     *
     * @note Valid only while is_ready() == true.
     *       Returns -1 when the port is closed; poll() will then
     *       return POLLNVAL which the caller can detect safely.
     * @note The underlying port_ must be a serial_port instance
     *       (or any i_serial_port that carries a real fd).
     *       If the injected implementation does not expose get_fd()
     *       (e.g. a unit-test mock based purely on i_serial_port),
     *       this method returns -1.
     * @note The caller MUST NOT close(), dup(), or dup2() this fd.
     *       Ownership remains with the underlying serial_port.
     */
    int get_fd() const noexcept;

    // -----------------------------------------------------------
    // Read operations (thread-safe)
    // -----------------------------------------------------------
    ssize_t read_byte(char& c);
    ssize_t read_chars(char* buf, std::size_t len);
    ssize_t read_uchars(unsigned char* buf, std::size_t len);
    ssize_t read_string(std::string& str, std::size_t max_len);
    ssize_t read_bytes(std::vector<uint8_t>& buf, std::size_t max_len);
    int     available() const;

    // -----------------------------------------------------------
    // Write operations (thread-safe)
    // -----------------------------------------------------------
    ssize_t write_byte(char c);
    ssize_t write_chars(const char* buf, std::size_t len);
    ssize_t write_uchars(const unsigned char* buf, std::size_t len);
    ssize_t write_string(const std::string& str);
    ssize_t write_bytes(const std::vector<uint8_t>& buf);

    // -----------------------------------------------------------
    // Buffer management (thread-safe)
    // -----------------------------------------------------------
    bool flush();
    bool drain();

private:
    std::weak_ptr<i_serial_port>   port_;
};

} // namespace serial

#endif // SERIAL_WRAPPER_H
