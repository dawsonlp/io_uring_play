# io_uring_async_cpp23 Library Makefile

CXX := g++
MODE ?= release
STRESS_RUNS ?= 25

BASE_CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -fcoroutines -MMD -MP
INCLUDES := -I./include -I./tests
BASE_LDFLAGS := -luring
TEST_CHECK_FLAGS := -include ./tests/support/check.hpp
INTEGRATION_CHECK_FLAGS := $(TEST_CHECK_FLAGS) -include ./tests/support/kernel_gate.hpp

ifeq ($(MODE),release)
MODE_CXXFLAGS := -O3 -DNDEBUG
else ifeq ($(MODE),debug)
MODE_CXXFLAGS := -g -O0 -DDEBUG_MODE
else ifeq ($(MODE),asan)
MODE_CXXFLAGS := -g -O1 -fsanitize=address -fno-omit-frame-pointer
MODE_LDFLAGS := -fsanitize=address
TEST_ENV := ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
else ifeq ($(MODE),ubsan)
MODE_CXXFLAGS := -g -O1 -fsanitize=undefined -fno-omit-frame-pointer
MODE_LDFLAGS := -fsanitize=undefined
else
$(error Unknown MODE '$(MODE)'; expected release, debug, asan, or ubsan)
endif

CXXFLAGS := $(BASE_CXXFLAGS) $(MODE_CXXFLAGS)
LDFLAGS := $(BASE_LDFLAGS) $(MODE_LDFLAGS)

SRC_DIR := src
BUILD_ROOT := build
BUILD_DIR := $(BUILD_ROOT)/$(MODE)
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
TEST_BIN_DIR := $(BUILD_DIR)/tests

LIB_SRCS := $(wildcard $(SRC_DIR)/runtime/*.cpp) $(wildcard $(SRC_DIR)/io/*.cpp)
LIB_OBJS := $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
UNIT_SRCS := $(wildcard tests/unit/*.cpp)
UNIT_BINS := $(UNIT_SRCS:tests/unit/%.cpp=$(TEST_BIN_DIR)/unit/%)
INTEGRATION_SRCS := $(wildcard tests/integration/*.cpp)
INTEGRATION_BINS := $(INTEGRATION_SRCS:tests/integration/%.cpp=$(TEST_BIN_DIR)/integration/%)
EXAMPLE_BIN := $(BIN_DIR)/echo_server
DEPS := $(LIB_OBJS:.o=.d) $(UNIT_BINS:=.d) $(INTEGRATION_BINS:=.d) $(EXAMPLE_BIN).d

-include $(DEPS)

.PHONY: all release debug asan ubsan build examples test-build test unit-test integration-test shutdown-stress routing-stress stress matrix clean help

all: release

release debug asan ubsan:
	+$(MAKE) MODE=$@ build

build: examples test-build
	@echo "✓ C++23 $(MODE) build complete"

$(OBJ_DIR)/runtime $(OBJ_DIR)/io $(BIN_DIR) $(TEST_BIN_DIR)/unit $(TEST_BIN_DIR)/integration:
	@mkdir -p $@

$(OBJ_DIR)/runtime/%.o: $(SRC_DIR)/runtime/%.cpp | $(OBJ_DIR)/runtime
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/io/%.o: $(SRC_DIR)/io/%.cpp | $(OBJ_DIR)/io
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

examples: $(EXAMPLE_BIN)

$(EXAMPLE_BIN): examples/echo_server.cpp $(LIB_OBJS) | $(BIN_DIR)
	@echo "Building example: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(LIB_OBJS) $(LDFLAGS) -o $@

test-build: $(UNIT_BINS) $(INTEGRATION_BINS)

$(TEST_BIN_DIR)/unit/%: tests/unit/%.cpp $(LIB_OBJS) | $(TEST_BIN_DIR)/unit
	@echo "Building unit test: $@"
	$(CXX) $(CXXFLAGS) -UNDEBUG -DIO_URING_ASYNC_TESTING $(TEST_CHECK_FLAGS) $(INCLUDES) $< $(LIB_OBJS) $(LDFLAGS) -o $@

$(TEST_BIN_DIR)/integration/%: tests/integration/%.cpp $(LIB_OBJS) | $(TEST_BIN_DIR)/integration
	@echo "Building integration test: $@"
	$(CXX) $(CXXFLAGS) -UNDEBUG -DIO_URING_ASYNC_TESTING $(INTEGRATION_CHECK_FLAGS) $(INCLUDES) $< $(LIB_OBJS) $(LDFLAGS) -pthread -o $@

unit-test: $(UNIT_BINS)
	@failures=0; for test in $(UNIT_BINS); do \
		echo "Running $$test"; $(TEST_ENV) $$test; status=$$?; \
		if [ $$status -ne 0 ]; then echo "FAIL: $$test (status $$status)"; \
			failures=$$((failures + 1)); fi; \
	done; test $$failures -eq 0

integration-test: $(INTEGRATION_BINS)
	@failures=0; skips=0; for test in $(INTEGRATION_BINS); do \
		echo "Running $$test"; $(TEST_ENV) $$test; status=$$?; \
		if [ $$status -eq 77 ]; then echo "SKIPPED: $$test"; \
			skips=$$((skips + 1)); \
		elif [ $$status -ne 0 ]; then echo "FAIL: $$test (status $$status)"; \
			failures=$$((failures + 1)); fi; \
	done; echo "Integration summary: $$skips skipped, $$failures failed"; \
	test $$failures -eq 0

shutdown-stress: $(TEST_BIN_DIR)/integration/test_graceful_shutdown $(TEST_BIN_DIR)/integration/test_descriptor_lifetime
	@set -e; run=1; while [ $$run -le $(STRESS_RUNS) ]; do \
		$(TEST_ENV) $(TEST_BIN_DIR)/integration/test_graceful_shutdown; \
		$(TEST_ENV) $(TEST_BIN_DIR)/integration/test_descriptor_lifetime; \
		run=$$((run + 1)); \
	done
	@echo "✓ C++23 $(MODE) shutdown stress passed ($(STRESS_RUNS) runs)"

routing-stress: $(TEST_BIN_DIR)/integration/test_operation_routing
	@set -e; run=1; while [ $$run -le $(STRESS_RUNS) ]; do \
		$(TEST_ENV) TEST_SEED=$$run $(TEST_BIN_DIR)/integration/test_operation_routing; \
		run=$$((run + 1)); \
	done
	@echo "✓ C++23 $(MODE) randomized routing stress passed ($(STRESS_RUNS) seeds)"

stress: shutdown-stress routing-stress

test: examples unit-test integration-test
	@echo "✓ C++23 $(MODE) tests passed"

matrix:
	+$(MAKE) MODE=release test
	+$(MAKE) MODE=debug test
	+$(MAKE) MODE=asan test
	+$(MAKE) MODE=ubsan test
	@echo "✓ Complete build-and-test matrix passed"

check-compiler:
	@echo "Current compiler: $(CXX)"
	@$(CXX) --version

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_ROOT)
	rm -f examples/echo_server valgrind.log *.valgrind *.log
	@echo "✓ Clean complete"

compare:
	@echo "C++20 headers: $$(find ../04_coroutine_io/include -name '*.hpp' | xargs wc -l | tail -1)"
	@echo "C++23 headers: $$(find include -name '*.hpp' | xargs wc -l | tail -1)"

help:
	@echo "Targets: release debug asan ubsan test unit-test integration-test stress matrix clean"
	@echo "Artifacts are isolated under build/<mode>/"

format:
	find include src examples tests -type f \( -name '*.hpp' -o -name '*.cpp' \) -exec clang-format -i {} +
