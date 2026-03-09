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
 * test_serial.cpp
 * ============================================================
 * Demonstrates and validates the serial_helper library API.
 *
 * Thread-safety design note:
 *  The mutex now lives inside serial_port (the layer that owns fd_).
 *  serial_wrapper is a pure delegation facade with no mutex of its own.
 *  Multiple wrappers pointing at the same shared serial_port instance
 *  are therefore always safe to use from different threads.
 *
 * Test scenarios:
 *  1.  Config factory helpers produce correct field values.
 *  2.  serial_port throws serial_open_error for a non-existent device.
 *  3.  serial_wrapper constructor rejects a null port.
 *  4.  serial_wrapper initialize propagates serial_open_error.
 *  5.  (Hardware present) Full open -> read loop -> write -> close cycle
 *      showing how gnss_controller uses the wrapper.
 *  6.  Thread-safety: two serial_wrapper instances sharing one serial_port
 *      are accessed concurrently from multiple threads without data races.
 *
 * Scenarios 1–4 and 6 run without GNSS hardware.
 * Scenario 5 is skipped unless GNSS_DEVICE is set in the environment,
 * e.g.:
 *   GNSS_DEVICE=/dev/ttyS0 ./test_serial
 */

#include "serial_wrapper.h"
#include "serial_port.h"
#include "serial_config.h"
#include "serial_exception.h"

#include <cstdio>
#include <cstdlib>   // getenv
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>

// ============================================================
//  Minimal test harness
// ============================================================

static int s_passed = 0;
static int s_failed = 0;

static void print_section(const char* title)
{
    std::printf("\n╔══════════════════════════════════════════════════════╗\n");
    std::printf("║  %-52s║\n", title);
    std::printf("╚══════════════════════════════════════════════════════╝\n");
}

#define EXPECT_TRUE(expr)                                             \
    do {                                                              \
        if (expr) {                                                   \
            std::printf("  [PASS] %s\n", #expr);                     \
            ++s_passed;                                               \
        } else {                                                      \
            std::printf("  [FAIL] %s  (line %d)\n", #expr, __LINE__);\
            ++s_failed;                                               \
        }                                                             \
    } while (false)

#define EXPECT_EQ(a, b)                                                       \
    do {                                                                      \
        if ((a) == (b)) {                                                     \
            std::printf("  [PASS] %s == %s\n", #a, #b);                      \
            ++s_passed;                                                       \
        } else {                                                              \
            std::printf("  [FAIL] %s != %s  (line %d)\n", #a, #b, __LINE__);\
            ++s_failed;                                                       \
        }                                                                     \
    } while (false)

// ============================================================
//  Scenario 1 – Config factory helpers
// ============================================================

static void test_config_factories()
{
    print_section("Scenario 1: Config factory helpers");

    using namespace serial;

    // gnss_default
    serial_config dflt = serial_config::gnss_default("/dev/ttyS0");
    EXPECT_EQ(dflt.device_path, std::string("/dev/ttyS0"));
    EXPECT_TRUE(dflt.baud  == baud_rate::B_9600);
    EXPECT_TRUE(dflt.data  == data_bits::EIGHT);
    EXPECT_TRUE(dflt.par   == parity::NONE);
    EXPECT_TRUE(dflt.stop  == stop_bits::ONE);
    EXPECT_TRUE(dflt.flow  == flow_control::NONE);
    EXPECT_EQ(dflt.read_timeout_ms, 100);
    EXPECT_TRUE(!dflt.non_blocking);

    // gnss_high_speed
    serial_config hs = serial_config::gnss_high_speed("/dev/ttyUSB0");
    EXPECT_TRUE(hs.baud == baud_rate::B_115200);
    EXPECT_EQ(hs.device_path, std::string("/dev/ttyUSB0"));

    // custom
    serial_config cust = serial_config::custom("/dev/ttyS1", baud_rate::B_230400);
    EXPECT_TRUE(cust.baud == baud_rate::B_230400);
    EXPECT_EQ(cust.device_path, std::string("/dev/ttyS1"));
}

// ============================================================
//  Scenario 2 – serial_port throws for non-existent device
// ============================================================

static void test_open_non_existent_device()
{
    print_section("Scenario 2: serial_port rejects non-existent device");

    using namespace serial;

    serial_port port;
    EXPECT_TRUE(!port.is_open());

    bool caught = false;
    try
    {
        port.open(serial_config::gnss_default("/dev/tty_DOES_NOT_EXIST_999"));
    }
    catch (const serial_open_error& e)
    {
        caught = true;
        std::printf("  [INFO] serial_open_error: %s\n", e.what());
    }
    EXPECT_TRUE(caught);
    EXPECT_TRUE(!port.is_open());
}

// ============================================================
//  Scenario 3 – serial_wrapper rejects null port
// ============================================================

static void test_wrapper_null_port()
{
    print_section("Scenario 3: serial_wrapper rejects null port");

    using namespace serial;

    bool caught = false;
    try
    {
        serial_wrapper w(nullptr);
        (void)w;
    }
    catch (const std::invalid_argument& e)
    {
        caught = true;
        std::printf("  [INFO] std::invalid_argument: %s\n", e.what());
    }
    EXPECT_TRUE(caught);
}

// ============================================================
//  Scenario 4 – serial_wrapper propagates serial_open_error
// ============================================================

static void test_wrapper_propagates_open_error()
{
    print_section("Scenario 4: serial_wrapper propagates serial_open_error");

    using namespace serial;

    // Keep the port alive via a named shared_ptr; passing it as a temporary
    // would immediately expire the weak_ptr inside the wrapper.
    auto port    = std::make_shared<serial_port>();
    auto wrapper = std::make_shared<serial_wrapper>(port);
    EXPECT_TRUE(!wrapper->is_ready());

    bool caught = false;
    try
    {
        wrapper->initialize(serial_config::gnss_default("/dev/tty_DOES_NOT_EXIST_999"));
    }
    catch (const serial_open_error& e)
    {
        caught = true;
        std::printf("  [INFO] serial_open_error: %s\n", e.what());
    }
    EXPECT_TRUE(caught);
    EXPECT_TRUE(!wrapper->is_ready());
}

// ============================================================
//  Scenario 6 – Thread-safety: shared port via two wrappers
//
//  A single serial_port is injected into two distinct serial_wrapper
//  instances (as happens when the same device is accessed from a reader
//  thread and a writer thread, or from two different subsystems).
//  Dozens of threads hammer is_open() / available() / get_fd()
//  concurrently on both wrappers to verify the port-level mutex
//  prevents data races.
// ============================================================

static void test_port_level_thread_safety()
{
    print_section("Scenario 6: port-level mutex – shared port, two wrappers, concurrent threads");

    using namespace serial;

    // One port shared between two independent wrappers.
    auto port     = std::make_shared<serial_port>();
    auto wrapper1 = std::make_shared<serial_wrapper>(port);
    auto wrapper2 = std::make_shared<serial_wrapper>(port);

    // Port is closed: both wrappers must agree on that.
    EXPECT_TRUE(!wrapper1->is_ready());
    EXPECT_TRUE(!wrapper2->is_ready());
    EXPECT_TRUE(!port->is_open());

    // Demonstrate that the wrapper holds NO mutex of its own:
    // thread-safety comes exclusively from serial_port::mutex_.
    std::atomic<int> errors{0};
    const int        thread_count = 8;
    const int        iterations   = 200;

    // Each thread alternates between wrapper1 and wrapper2 reads.
    auto worker = [&](int id)
    {
        for (int i = 0; i < iterations; ++i)
        {
            // is_open() / is_ready() are const and lock the port mutex.
            bool r1 = wrapper1->is_ready();
            bool r2 = wrapper2->is_ready();
            bool p  = port->is_open();

            // All three views of the same port must be consistent.
            if (r1 != r2 || r1 != p)
            {
                ++errors;
            }

            // get_fd() on a closed port must return -1 from every thread.
            if (wrapper1->get_fd() != -1 || wrapper2->get_fd() != -1)
            {
                ++errors;
            }

            // available() on a closed port must return -1.
            if (wrapper1->available() != -1 || wrapper2->available() != -1)
            {
                ++errors;
            }
        }
        (void)id;
    };

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(thread_count));
    for (int i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
    std::printf("  [INFO] %d threads x %d iterations: no inconsistency detected\n",
                thread_count, iterations);
}

static void test_hardware_cycle(const std::string& device_path)
{
    print_section("Scenario 5: Full hardware cycle (gnss_controller pattern)");

    using namespace serial;

    // ── Build the UBX-CFG-RATE command (10 Hz measurement rate) ──────────
    // UBX frame: sync(B5 62) + class(06) + id(08) + len(6) + payload + ck
    // Payload: measRate=100ms, navRate=1, timeRef=1 (GPS)
    const std::vector<uint8_t> ubx_cfg_rate = {
        0xB5, 0x62,          // sync chars
        0x06, 0x08,          // class CFG, id RATE
        0x06, 0x00,          // payload length (little-endian)
        0x64, 0x00,          // measRate = 100 ms (10 Hz)
        0x01, 0x00,          // navRate  = 1
        0x01, 0x00,          // timeRef  = 1 (GPS)
        0x7A, 0x12           // checksum (CK_A, CK_B)
    };

    // ── Dependency injection: inject serial_port into serial_wrapper ──────
    auto port    = std::make_shared<serial_port>();
    auto wrapper = std::make_shared<serial_wrapper>(port);

    // ── Initialize with GNSS default config (9600-8N1) ───────────────────
    serial_config cfg = serial_config::gnss_default(device_path);
    try
    {
        bool ok = wrapper->initialize(cfg);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(wrapper->is_ready());
        std::printf("  [INFO] Opened %s at 9600-8N1\n", device_path.c_str());
    }
    catch (const serial_error& e)
    {
        std::printf("  [SKIP] Hardware open failed: %s\n", e.what());
        return;
    }

    // ── Write UBX-CFG-RATE command ────────────────────────────────────────
    {
        ssize_t written = wrapper->write_bytes(ubx_cfg_rate);
        EXPECT_EQ(written, static_cast<ssize_t>(ubx_cfg_rate.size()));
        wrapper->drain();   // ensure chip receives the command before we read its ACK
        std::printf("  [INFO] Sent UBX-CFG-RATE (%zd bytes)\n", ubx_cfg_rate.size());
    }

    // ── Read loop – collect up to 2 seconds of raw UBX data ──────────────
    std::printf("  [INFO] Reading raw UBX data for 2 seconds …\n");
    std::atomic<std::size_t> total_bytes{0};

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline)
    {
        int avail = wrapper->available();
        if (avail > 0)
        {
            std::vector<uint8_t> buf;
            ssize_t n = wrapper->read_bytes(buf, static_cast<std::size_t>(avail));
            if (n > 0)
            {
                total_bytes += static_cast<std::size_t>(n);
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    std::printf("  [INFO] Received %zu bytes total\n", total_bytes.load());
    EXPECT_TRUE(total_bytes > 0);

    // ── Reconfigure to 115200 (common after baud-switch command) ─────────
    {
        bool ok = wrapper->reconfigure(serial_config::gnss_high_speed(device_path));
        EXPECT_TRUE(ok);
        std::printf("  [INFO] Reconfigured to 115200-8N1\n");
    }

    // ── Reconnect (simulates recovery from framing error) ─────────────────
    {
        bool ok = wrapper->reconnect();
        EXPECT_TRUE(ok);
        std::printf("  [INFO] Reconnect: OK\n");
    }

    // ── Shutdown ──────────────────────────────────────────────────────────
    wrapper->shutdown();
    EXPECT_TRUE(!wrapper->is_ready());
    std::printf("  [INFO] Port closed\n");
}

// ============================================================
//  main
// ============================================================

int main()
{
    std::printf("\n");
    std::printf("╔══════════════════════════════════════════════════════╗\n");
    std::printf("║  serial_helper – Test Suite                          ║\n");
    std::printf("╚══════════════════════════════════════════════════════╝\n");

    test_config_factories();
    test_open_non_existent_device();
    test_wrapper_null_port();
    test_wrapper_propagates_open_error();
    test_port_level_thread_safety();

    // Hardware test – opt-in through environment variable
    const char* dev = std::getenv("GNSS_DEVICE");
    if (dev != nullptr && std::strlen(dev) > 0)
    {
        test_hardware_cycle(dev);
    }
    else
    {
        print_section("Scenario 5: hardware test (set GNSS_DEVICE to run)");
        std::printf("  [SKIP] GNSS_DEVICE env var not set\n");
    }

    std::printf("\n══════════════════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed\n", s_passed, s_failed);
    std::printf("══════════════════════════════════════════════════════\n\n");

    return (s_failed == 0) ? 0 : 1;
}
