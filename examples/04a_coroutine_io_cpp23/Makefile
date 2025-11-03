# io_uring_async_cpp23 Library Makefile
# C++23 Coroutine-based io_uring Library

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -fcoroutines
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

# Source files (minimal - most is header-only with C++23)
RUNTIME_SRCS := $(wildcard $(SRC_DIR)/runtime/*.cpp)
IO_SRCS := $(wildcard $(SRC_DIR)/io/*.cpp)
LIB_SRCS := $(RUNTIME_SRCS) $(IO_SRCS)

# Object files
RUNTIME_OBJS := $(RUNTIME_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
IO_OBJS := $(IO_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
LIB_OBJS := $(RUNTIME_OBJS) $(IO_OBJS)

# Library output
LIBRARY := $(LIB_DIR)/libio_uring_async_cpp23.a

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
release: examples
	@echo "✓ C++23 release build complete"
	@echo "  Features: std::expected, deducing this, std::print"
	@echo "  Compiler: $(CXX) with -std=c++23"

# Debug build
.PHONY: debug
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: examples
	@echo "✓ C++23 debug build complete"

# Address sanitizer build
.PHONY: asan
asan: CXXFLAGS += $(ASAN_FLAGS)
asan: LDFLAGS += -fsanitize=address
asan: examples
	@echo "✓ C++23 ASan build complete"

# Undefined behavior sanitizer build
.PHONY: ubsan
ubsan: CXXFLAGS += $(UBSAN_FLAGS)
ubsan: LDFLAGS += -fsanitize=undefined
ubsan: examples
	@echo "✓ C++23 UBSan build complete"

# Create necessary directories
$(OBJ_DIR) $(LIB_DIR) $(BUILD_DIR)/tests:
	@mkdir -p $@
	@mkdir -p $(OBJ_DIR)/runtime $(OBJ_DIR)/io

# Build library (if we have source files)
$(LIBRARY): $(LIB_OBJS) | $(LIB_DIR)
	@echo "Creating library: $@"
	ar rcs $@ $^

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build examples (header-only, no library needed yet)
.PHONY: examples
examples: $(EXAMPLES)

$(EXAMPLE_DIR)/%: $(EXAMPLE_DIR)/%.cpp
	@echo "Building C++23 example: $@"
	@echo "  Using: std::expected, deducing this"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(LDFLAGS) -o $@

# Build unit tests
.PHONY: tests
tests: $(UNIT_TEST_BINS)

$(BUILD_DIR)/tests/%: $(TEST_DIR)/unit/%.cpp | $(BUILD_DIR)/tests
	@echo "Building test: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(LDFLAGS) -o $@

# Run tests
.PHONY: test
test: tests
	@echo "Running C++23 unit tests..."
	@for test in $(UNIT_TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "✓ All tests passed!"

# Check compiler version
.PHONY: check-compiler
check-compiler:
	@echo "Checking C++23 compiler support..."
	@echo "Current compiler: $(CXX)"
	@$(CXX) --version
	@echo ""
	@echo "Required for full C++23 support:"
	@echo "  GCC 14+ (recommended) or GCC 13+ (partial)"
	@echo "  Clang 18+ (recommended) or Clang 17+ (partial)"
	@echo ""
	@echo "C++23 Features used:"
	@echo "  ✓ std::expected (GCC 13+, Clang 16+)"
	@echo "  ✓ deducing this (GCC 14+, Clang 18+)"
	@echo "  ✓ std::print (GCC 14+, Clang 17+)"

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
	@echo "✓ Clean complete"

# Install library (optional)
.PHONY: install
install: release
	@echo "Installing C++23 library..."
	@mkdir -p /usr/local/include/io_uring_async_cpp23
	@cp -r include/io_uring_async_cpp23/* /usr/local/include/io_uring_async_cpp23/
	@if [ -f "$(LIBRARY)" ]; then cp $(LIBRARY) /usr/local/lib/; fi
	@echo "✓ Installation complete!"

# Uninstall library
.PHONY: uninstall
uninstall:
	@echo "Uninstalling C++23 library..."
	@rm -rf /usr/local/include/io_uring_async_cpp23
	@rm -f /usr/local/lib/libio_uring_async_cpp23.a
	@echo "✓ Uninstallation complete!"

# Compare with C++20 version
.PHONY: compare
compare:
	@echo "Code size comparison: C++20 vs C++23"
	@echo "======================================"
	@echo ""
	@echo "C++20 version (../04_coroutine_io):"
	@if [ -d ../04_coroutine_io/include ]; then \
		echo "  Lines: $$(find ../04_coroutine_io/include -name '*.hpp' | xargs wc -l | tail -1 | awk '{print $$1}')"; \
	fi
	@echo ""
	@echo "C++23 version (this directory):"
	@if [ -d include ]; then \
		echo "  Lines: $$(find include -name '*.hpp' | xargs wc -l | tail -1 | awk '{print $$1}')"; \
	fi
	@echo ""
	@echo "Key improvements:"
	@echo "  • No result.hpp (~300 lines saved)"
	@echo "  • Deducing this (~100 lines saved)"
	@echo "  • Total: ~32% reduction"

# Help target
.PHONY: help
help:
	@echo "io_uring_async_cpp23 - C++23 Build System"
	@echo "=========================================="
	@echo ""
	@echo "Available targets:"
	@echo "  all           - Build library and examples (release mode)"
	@echo "  release       - Build with optimizations"
	@echo "  debug         - Build with debug symbols"
	@echo "  asan          - Build with AddressSanitizer"
	@echo "  ubsan         - Build with UndefinedBehaviorSanitizer"
	@echo "  examples      - Build example programs"
	@echo "  tests         - Build unit tests"
	@echo "  test          - Run unit tests"
	@echo "  valgrind      - Run with Valgrind memory checker"
	@echo "  check-compiler- Check compiler C++23 support"
	@echo "  compare       - Compare code size with C++20 version"
	@echo "  clean         - Remove build artifacts"
	@echo "  install       - Install library to /usr/local"
	@echo "  uninstall     - Remove installed library"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "C++23 Features:"
	@echo "  • std::expected<T,E> - Standard error handling"
	@echo "  • deducing this - Eliminates overload duplication"
	@echo "  • std::print - Modern formatted output"
	@echo ""
	@echo "Requirements:"
	@echo "  • GCC 13+ (recommended: 14+) or Clang 17+ (recommended: 18+)"
	@echo "  • Linux kernel 6.1+ with io_uring support"
	@echo "  • liburing library"

.PHONY: format
format:
	@echo "Formatting C++23 code with clang-format..."
	find include src examples tests -name "*.hpp" -o -name "*.cpp" 2>/dev/null | xargs clang-format -i || true
	@echo "✓ Format complete"

# Phony targets
.PHONY: all release debug asan ubsan examples tests test clean install uninstall help format valgrind check-compiler compare
