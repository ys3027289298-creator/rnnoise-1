CC ?= gcc
CXX ?= g++

ROOT := $(CURDIR)
OUT_DIR ?= $(ROOT)/../output
BUILD_DIR := $(ROOT)/build

CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS := -I$(ROOT)/include -I$(ROOT)/third_party/lodepng \
            -I$(ROOT)/third_party/qoi

TEST_BIN := $(BUILD_DIR)/test_qoi

all: $(TEST_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/qoi_writer.o: src/qoi_writer.c include/packer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/png_writer.o: src/png_writer.cpp include/packer.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/lodepng.o: third_party/lodepng/lodepng.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/test_qoi.o: tests/test_qoi.c include/packer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(TEST_BIN): $(BUILD_DIR)/test_qoi.o $(BUILD_DIR)/qoi_writer.o \
             $(BUILD_DIR)/png_writer.o $(BUILD_DIR)/lodepng.o
	$(CXX) $^ -o $@

check: $(TEST_BIN)
	mkdir -p $(OUT_DIR)
	$(TEST_BIN) $(OUT_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all check clean
