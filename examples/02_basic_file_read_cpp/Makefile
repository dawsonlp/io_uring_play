# Makefile for Modern C++23 io_uring File Read Example
#
# Targets:
#   all     - Build the executable (default)
#   clean   - Remove build artifacts
#   test    - Build and run the application
#
# Usage:
#   make        # Build the application
#   make clean  # Clean build artifacts
#   make test   # Build and run

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -g -O2
CXXFLAGS += -MMD -MP  # Generate dependency files
LDFLAGS = -luring

# Target executable
TARGET = io_uring_example_cpp

# Source files
SRCS = main.cpp

# Object files (derived from sources)
OBJS = $(SRCS:.cpp=.o)

# Dependency files
DEPS = $(OBJS:.o=.d)

# Default target
.PHONY: all
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	@echo "[+] Linking $(TARGET)..."
	$(CXX) $(OBJS) $(LDFLAGS) -o $(TARGET)
	@echo "[+] Build successful: $(TARGET)"

# Compile source files to object files
%.o: %.cpp
	@echo "[+] Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include dependency files
-include $(DEPS)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "[+] Cleaning build artifacts..."
	rm -f $(OBJS) $(DEPS) $(TARGET)
	@echo "[+] Clean complete"

# Build and run the application
.PHONY: test
test: $(TARGET)
	@echo ""
	@echo "[+] Running $(TARGET)..."
	@echo "================================"
	./$(TARGET)
	@echo "================================"
	@echo "[+] Test complete"

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all     - Build the executable (default)"
	@echo "  clean   - Remove build artifacts"
	@echo "  test    - Build and run the application"
	@echo "  help    - Display this help message"
	@echo ""
	@echo "Compiler: $(CXX)"
	@echo "C++ Standard: C++23"
	@echo "Flags: $(CXXFLAGS)"
