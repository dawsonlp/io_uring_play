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
OBJ_DIR := $(BUILD_DIR)/obj
EXAMPLE_DIR := examples
TEST_DIR := tests

# Source files
RUNTIME_SRCS := $(wildcard $(SRC_DIR)/runtime/*.cpp)
IO_SRCS := $(wildcard $(SRC_DIR)/io/*.cpp)
LIB_SRCS := $(RUNTIME_SRCS) $(IO_SRCS)

# Object files
LIB_OBJS := $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Default target
.PHONY: all
all: release

# Release build
.PHONY: release
release: CXXFLAGS += $(RELEASE_FLAGS)
release: examples test-build
	@echo "✓ C++23 release build complete"

# Debug build
.PHONY: debug
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: examples test-build
	@echo "✓ C++23 debug build complete"

# Address sanitizer build
.PHONY: asan
asan: CXXFLAGS += $(ASAN_FLAGS)
asan: LDFLAGS += -fsanitize=address
asan: examples test-build
	@echo "✓ C++23 ASan build complete"

# Undefined behavior sanitizer build
.PHONY: ubsan
ubsan: CXXFLAGS += $(UBSAN_FLAGS)
ubsan: LDFLAGS += -fsanitize=undefined
ubsan: examples test-build
	@echo "✓ C++23 UBSan build complete"

# Create necessary directories
$(OBJ_DIR)/runtime $(OBJ_DIR)/io $(BUILD_DIR)/tests:
	@mkdir -p $@

# Compile source files
$(OBJ_DIR)/runtime/%.o: $(SRC_DIR)/runtime/%.cpp | $(OBJ_DIR)/runtime
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/io/%.o: $(SRC_DIR)/io/%.cpp | $(OBJ_DIR)/io
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build examples (link with library objects)
.PHONY: examples
examples: $(EXAMPLE_DIR)/echo_server

$(EXAMPLE_DIR)/echo_server: $(EXAMPLE_DIR)/echo_server.cpp $(LIB_OBJS)
	@echo "Building C++23 example: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

# Build tests
.PHONY: test-build
test-build: $(BUILD_DIR)/tests/test_headers

$(BUILD_DIR)/tests/test_headers: $(TEST_DIR)/unit/test_headers.cpp $(LIB_OBJS) | $(BUILD_DIR)/tests
	@echo "Building test: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

# Run tests
.PHONY: test
test: test-build
	@echo "Running C++23 unit tests..."
	$(BUILD_DIR)/tests/test_headers
	@echo ""

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

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(EXAMPLE_DIR)/echo_server
	rm -f valgrind.log *.valgrind *.log
	@echo "✓ Clean complete"

# Compare with C++20 version
.PHONY: compare
compare:
	@echo "Code size comparison: C++20 vs C++23"
	@echo "======================================"
	@echo ""
	@echo "C++20 version (../04_coroutine_io):"
	@if [ -d ../04_coroutine_io/include ]; then \
		echo "  Header lines: $$(find ../04_coroutine_io/include -name '*.hpp' | xargs wc -l 2>/dev/null | tail -1 | awk '{print $$1}')"; \
	fi
	@echo ""
	@echo "C++23 version (this directory):"
	@echo "  Header lines: $$(find include -name '*.hpp' | xargs wc -l 2>/dev/null | tail -1 | awk '{print $$1}')"
	@echo ""
	@echo "Key improvements:"
	@echo "  - No result.hpp (~300 lines saved)"
	@echo "  - Deducing this (~100 lines saved)"
	@echo "  - std::expected throughout"

# Help target
.PHONY: help
help:
	@echo "io_uring_async_cpp23 - C++23 Build System"
	@echo "=========================================="
	@echo ""
	@echo "Available targets:"
	@echo "  all           - Build examples and tests (release mode)"
	@echo "  release       - Build with optimizations"
	@echo "  debug         - Build with debug symbols"
	@echo "  asan          - Build with AddressSanitizer"
	@echo "  ubsan         - Build with UndefinedBehaviorSanitizer"
	@echo "  examples      - Build example programs"
	@echo "  test-build    - Build unit tests"
	@echo "  test          - Build and run unit tests"
	@echo "  check-compiler- Check compiler C++23 support"
	@echo "  compare       - Compare code size with C++20 version"
	@echo "  clean         - Remove build artifacts"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "API Surface (all in namespace io_uring_async_cpp23):"
	@echo "  Core:    Error, Task<T>, IoUring"
	@echo "  Runtime: IoUringExecutor, *Awaitable types"
	@echo "  I/O:     TcpListener, TcpStream"
	@echo ""
	@echo "Single include: #include <io_uring_async_cpp23/io_uring_async_cpp23.hpp>"

.PHONY: format
format:
	@echo "Formatting C++23 code..."
	find include src examples tests -name "*.hpp" -o -name "*.cpp" 2>/dev/null | xargs clang-format -i || true
	@echo "✓ Format complete"
