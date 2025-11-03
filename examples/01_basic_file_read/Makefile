# Makefile for io_uring Basic File Read Example
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
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
LDFLAGS = -luring

# Target executable
TARGET = io_uring_example

# Source files
SRCS = main.c

# Object files (derived from sources)
OBJS = $(SRCS:.c=.o)

# Default target
.PHONY: all
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	@echo "[+] Linking $(TARGET)..."
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)
	@echo "[+] Build successful: $(TARGET)"

# Compile source files to object files
%.o: %.c
	@echo "[+] Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
.PHONY: clean
clean:
	@echo "[+] Cleaning build artifacts..."
	rm -f $(OBJS) $(TARGET)
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
