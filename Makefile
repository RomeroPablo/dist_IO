CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
TARGET := a.out
BUILD_DIR := build
SRCS := main.cpp serial.cpp tcp.cpp ringbuffer.cpp candb.cpp
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# compile each .cpp into build/ directory
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

