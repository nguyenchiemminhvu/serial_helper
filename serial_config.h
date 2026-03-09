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
 * serial_config.h
 * ============================================================
 * Plain-data configuration object that fully describes a UART
 * port session.  Passed to i_serial_port::open() to configure
 * the underlying termios settings in one atomic step.
 *
 * Factory helpers make it easy to build "known-good" configs
 * for common GNSS scenarios without remembering every field.
 */

#ifndef SERIAL_CONFIG_H
#define SERIAL_CONFIG_H

#include <string>
#include "serial_types.h"

namespace serial
{

/**
 * @brief Complete UART port configuration.
 *
 * All fields have sensible defaults (9600-8N1, no flow control,
 * blocking read).  Applications should set at least `device_path`
 * before calling open().
 */
struct serial_config
{
    std::string  device_path;                          ///< e.g. "/dev/ttyS0", "/dev/ttyUSB0"
    baud_rate    baud          = baud_rate::B_9600;    ///< Line speed
    data_bits    data          = data_bits::EIGHT;     ///< Character width
    parity       par           = parity::NONE;         ///< Parity mode
    stop_bits    stop          = stop_bits::ONE;       ///< Stop-bit count
    flow_control flow          = flow_control::NONE;   ///< Flow-control strategy
    int          read_timeout_ms = 0;                  ///< 0 = block; >0 = timeout in ms
    bool         non_blocking  = false;                ///< true = O_NONBLOCK on open()

    // -------------------------------------------------------
    // Factory helpers
    // -------------------------------------------------------

    /**
     * @brief 9600-8N1, no flow control, 100 ms read timeout.
     *
     * Matches the factory default of most u-blox GNSS chips
     * (M8, M9, F9 series) on their UART1 interface.
     *
     * @param dev  Device node path, e.g. "/dev/ttyS0".
     */
    static serial_config gnss_default(const std::string& dev)
    {
        serial_config cfg;
        cfg.device_path     = dev;
        cfg.baud            = baud_rate::B_9600;
        cfg.data            = data_bits::EIGHT;
        cfg.par             = parity::NONE;
        cfg.stop            = stop_bits::ONE;
        cfg.flow            = flow_control::NONE;
        cfg.read_timeout_ms = 100;
        cfg.non_blocking    = false;
        return cfg;
    }

    /**
     * @brief 115200-8N1, no flow control, 100 ms read timeout.
     *
     * Use after sending a UBX-CFG-PRT command to the chip to
     * switch it to 115200 bps for higher throughput.
     *
     * @param dev  Device node path.
     */
    static serial_config gnss_high_speed(const std::string& dev)
    {
        serial_config cfg = gnss_default(dev);
        cfg.baud           = baud_rate::B_115200;
        return cfg;
    }

    /**
     * @brief 9600-8N1, no flow control, 100 ms read timeout, non-blocking.
     *
     * Same as gnss_default() but opens the port with O_NONBLOCK so
     * read() returns immediately when no data is available.
     *
     * @param dev  Device node path, e.g. "/dev/ttyS0".
     */
    static serial_config gnss_default_nb(const std::string& dev)
    {
        serial_config cfg = gnss_default(dev);
        cfg.non_blocking  = true;
        return cfg;
    }

    /**
     * @brief 115200-8N1, no flow control, 100 ms read timeout, non-blocking.
     *
     * Same as gnss_high_speed() but opens the port with O_NONBLOCK so
     * read() returns immediately when no data is available.
     *
     * @param dev  Device node path.
     */
    static serial_config gnss_high_speed_nb(const std::string& dev)
    {
        serial_config cfg = gnss_high_speed(dev);
        cfg.non_blocking  = true;
        return cfg;
    }

    /**
     * @brief Custom baud rate, 8N1, no flow control.
     *
     * Convenience factory when applicaiton needs an arbitrary
     * baud rate (e.g. after dynamic baud negotiation).
     *
     * @param dev   Device node path.
     * @param baud  Desired baud rate from the baud_rate enum.
     * @param non_blocking  Whether to open the port in non-blocking mode.
     */
    static serial_config custom(const std::string& dev, baud_rate baud, bool non_blocking = false)
    {
        serial_config cfg = gnss_default(dev);
        cfg.baud          = baud;
        cfg.non_blocking  = non_blocking;
        return cfg;
    }
};

} // namespace serial

#endif // SERIAL_CONFIG_H
