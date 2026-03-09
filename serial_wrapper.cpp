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
 * serial_wrapper.cpp
 * ============================================================
 * Implementation of the high-level serial_wrapper class.
 */

#include "serial_wrapper.h"
#include "serial_exception.h"

#include <stdexcept>

namespace serial
{

// ============================================================
//  Constructor / Destructor
// ============================================================

serial_wrapper::serial_wrapper(std::unique_ptr<i_serial_port> port)
    : port_(std::move(port))
{
    if (!port_)
    {
        throw std::invalid_argument("serial_wrapper: port must not be null");
    }
}

serial_wrapper::~serial_wrapper()
{
    // Best-effort shutdown – exceptions must not escape a destructor.
    try
    {
        shutdown();
    }
    catch (...)
    {
    }
}

// ============================================================
//  Lifecycle
// ============================================================

bool serial_wrapper::initialize(const serial_config& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->open(config);
}

void serial_wrapper::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (port_->is_open())
    {
        port_->close();
    }
}

bool serial_wrapper::reconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    const serial_config saved = port_->get_config();
    if (port_->is_open())
    {
        port_->close();
    }
    return port_->open(saved);
}

bool serial_wrapper::reconfigure(const serial_config& new_config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (port_->is_open())
    {
        port_->close();
    }
    return port_->open(new_config);
}

// ============================================================
//  Status
// ============================================================

bool serial_wrapper::is_ready() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->is_open();
}

const serial_config& serial_wrapper::get_config() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->get_config();
}

// ============================================================
//  fd accessor
// ============================================================

int serial_wrapper::get_fd() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (port_->is_open())
    {
        return port_->get_fd();
    }
    else
    {
        return -1;
    }
}

// ============================================================
//  Read operations
// ============================================================

ssize_t serial_wrapper::read_byte(char& c)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->read_byte(c);
}

ssize_t serial_wrapper::read_chars(char* buf, std::size_t len)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->read_chars(buf, len);
}

ssize_t serial_wrapper::read_uchars(unsigned char* buf, std::size_t len)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->read_uchars(buf, len);
}

ssize_t serial_wrapper::read_string(std::string& str, std::size_t max_len)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->read_string(str, max_len);
}

ssize_t serial_wrapper::read_bytes(std::vector<uint8_t>& buf, std::size_t max_len)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->read_bytes(buf, max_len);
}

int serial_wrapper::available() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->available();
}

// ============================================================
//  Write operations
// ============================================================

ssize_t serial_wrapper::write_byte(char c)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->write_byte(c);
}

ssize_t serial_wrapper::write_chars(const char* buf, std::size_t len)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->write_chars(buf, len);
}

ssize_t serial_wrapper::write_uchars(const unsigned char* buf, std::size_t len)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->write_uchars(buf, len);
}

ssize_t serial_wrapper::write_string(const std::string& str)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->write_string(str);
}

ssize_t serial_wrapper::write_bytes(const std::vector<uint8_t>& buf)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->write_bytes(buf);
}

// ============================================================
//  Buffer management
// ============================================================

bool serial_wrapper::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->flush();
}

bool serial_wrapper::drain()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_->drain();
}

} // namespace serial
