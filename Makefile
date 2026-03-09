# ==============================================================================
# serial_helper – Makefile
# ==============================================================================
# Target:   Linux (Yocto embedded / x86_64 development host)
# Compiler: g++ (C++14)
#
# Cross-compilation example (Yocto SDK):
#   make CXX=aarch64-poky-linux-g++ \
#        CXXFLAGS="-std=c++14 -Wall -Wextra -O2 --sysroot=<SDK_SYSROOT>"
# ==============================================================================

CXX      := g++
CXXFLAGS := -std=c++14 -Wall -Wextra -Wpedantic -g -pthread

TARGET   := test_serial

SRCS     := serial_port.cpp \
             serial_wrapper.cpp \
             test_serial.cpp

OBJS     := $(SRCS:.cpp=.o)
DEPS     := $(OBJS:.o=.d)

# -----------------------------------------------------------------------
.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Auto-dependency generation
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

# Run without hardware (scenarios 1-4 only)
run: $(TARGET)
	./$(TARGET)

# Run with hardware (set GNSS_DEVICE to your TTY device node)
run-hw: $(TARGET)
	GNSS_DEVICE=$(GNSS_DEVICE) ./$(TARGET)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)
