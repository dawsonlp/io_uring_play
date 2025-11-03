# io_uring_async Library Makefile
# C++20 Coroutine-based io_uring Library

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -fcoroutines
INCLUDES := -I./include
LDFLAGS := -luring

# Build modes
DEBUG_FLAGS := -g -O0 -DDEBUG_MODE
RELEASE_FLAGS := -O3 -DNDEBUG -march=native
ASAN_FLAGS := -g -O1 -fsanitize=address -fno-omit-frame-pointer
UBSAN_FLAGS := -g -O1 -fsanitize=undefined -fno-omit-frame-pointer

# Directories
SRC_DIR := src
BUILD_DIR := build
LIB_DIR := $(BUILD_DIR)/lib
OBJ_DIR := $(BUILD_DIR)/obj
EXAMPLE_DIR := examples
TEST_DIR := tests

# Source files  
RUNTIME_SRCS := $(wildcard $(SRC_DIR)/runtime/*.cpp)
IO_SRCS := $(wildcard $(SRC_DIR)/io/*.cpp)
LIB_SRCS := $(RUNTIME_SRCS) $(IO_SRCS)

# Ensure we have the awaitable.cpp file
ifeq ($(wildcard $(SRC_DIR)/runtime/awaitable.cpp),)
$(warning Warning: awaitable.cpp not found in $(SRC_DIR)/runtime/)
endif

# Object files
RUNTIME_OBJS := $(RUNTIME_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
IO_OBJS := $(IO_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
LIB_OBJS := $(RUNTIME_OBJS) $(IO_OBJS)

# Library output
LIBRARY := $(LIB_DIR)/libio_uring_async.a

# Example programs
EXAMPLES := $(EXAMPLE_DIR)/echo_server
EXAMPLE_OBJS := $(EXAMPLES:=.o)

# Test programs
UNIT_TESTS := $(wildcard $(TEST_DIR)/unit/*.cpp)
UNIT_TEST_BINS := $(UNIT_TESTS:$(TEST_DIR)/unit/%.cpp=$(BUILD_DIR)/tests/%)

# Default target
.PHONY: all
all: release

# Release build
.PHONY: release
release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(LIBRARY) examples

# Debug build
.PHONY: debug
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(LIBRARY) examples

# Address sanitizer build
.PHONY: asan
asan: CXXFLAGS += $(ASAN_FLAGS)
asan: LDFLAGS += -fsanitize=address
asan: $(LIBRARY) examples

# Undefined behavior sanitizer build
.PHONY: ubsan
ubsan: CXXFLAGS += $(UBSAN_FLAGS)
ubsan: LDFLAGS += -fsanitize=undefined
ubsan: $(LIBRARY) examples

# Create necessary directories
$(OBJ_DIR) $(LIB_DIR) $(BUILD_DIR)/tests:
	@mkdir -p $@
	@mkdir -p $(OBJ_DIR)/runtime $(OBJ_DIR)/io

# Build library
$(LIBRARY): $(LIB_OBJS) | $(LIB_DIR)
	@echo "Creating library: $@"
	ar rcs $@ $^

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build examples
.PHONY: examples
examples: $(EXAMPLES)

$(EXAMPLE_DIR)/%: $(EXAMPLE_DIR)/%.cpp $(LIBRARY)
	@echo "Building example: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L$(LIB_DIR) -lio_uring_async $(LDFLAGS) -o $@

# Build unit tests
.PHONY: tests
tests: $(UNIT_TEST_BINS)

$(BUILD_DIR)/tests/%: $(TEST_DIR)/unit/%.cpp $(LIBRARY) | $(BUILD_DIR)/tests
	@echo "Building test: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L$(LIB_DIR) -lio_uring_async $(LDFLAGS) -o $@

# Run tests
.PHONY: test
test: tests
	@echo "Running unit tests..."
	@for test in $(UNIT_TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

# Run with Valgrind
.PHONY: valgrind
valgrind: debug examples
	@echo "Running echo_server with Valgrind..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--verbose --log-file=valgrind.log $(EXAMPLE_DIR)/echo_server

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(EXAMPLES)
	rm -f $(EXAMPLE_OBJS)
	rm -f valgrind.log *.valgrind *.log

# Install library (optional)
.PHONY: install
install: release
	@echo "Installing library..."
	@mkdir -p /usr/local/include/io_uring_async
	@cp -r include/io_uring_async/* /usr/local/include/io_uring_async/
	@cp $(LIBRARY) /usr/local/lib/
	@echo "Installation complete!"

# Uninstall library
.PHONY: uninstall
uninstall:
	@echo "Uninstalling library..."
	@rm -rf /usr/local/include/io_uring_async
	@rm -f /usr/local/lib/libio_uring_async.a
	@echo "Uninstallation complete!"

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all       - Build library and examples (release mode)"
	@echo "  release   - Build with optimizations"
	@echo "  debug     - Build with debug symbols"
	@echo "  asan      - Build with AddressSanitizer"
	@echo "  ubsan     - Build with UndefinedBehaviorSanitizer"
	@echo "  examples  - Build example programs"
	@echo "  tests     - Build unit tests"
	@echo "  test      - Run unit tests"
	@echo "  valgrind  - Run with Valgrind memory checker"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install library to /usr/local"
	@echo "  uninstall - Remove installed library"
	@echo "  help      - Show this help message"

.PHONY: format
format:
	@echo "Formatting code with clang-format..."
	find include src examples tests -name "*.hpp" -o -name "*.cpp" | xargs clang-format -i

# Phony targets don't correspond to actual files
.PHONY: all release debug asan ubsan examples tests test clean install uninstall help format valgrind
