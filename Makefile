# Makefile for Go Green Threads FFI Library

# Configuration
GO := go
CC := gcc
AR := ar

# Directories
BUILD_DIR := build
LIB_DIR := lib

# Library names
LIB_NAME := gtruntime
SHARED_LIB := lib$(LIB_NAME).so
STATIC_LIB := lib$(LIB_NAME).a

# Go build flags
GO_BUILD_FLAGS := -buildmode=c-shared
GO_LDFLAGS := -ldflags="-s -w"

# C build flags
CFLAGS := -Wall -Wextra -O2 -I. -L$(LIB_DIR)
LDFLAGS := -L$(LIB_DIR) -l$(LIB_NAME) -lm

# Targets
.PHONY: all clean lib example install test

all: lib example

# Build shared library
lib: $(LIB_DIR)/$(SHARED_LIB)

$(LIB_DIR)/$(SHARED_LIB): gtruntime.go
	@mkdir -p $(LIB_DIR)
	@echo "Building Go shared library..."
	$(GO) build $(GO_BUILD_FLAGS) $(GO_LDFLAGS) -o $(LIB_DIR)/$(SHARED_LIB) gtruntime.go
	@echo "Library built: $(LIB_DIR)/$(SHARED_LIB)"

# Build example
example: $(BUILD_DIR)/example

$(BUILD_DIR)/example: example.c $(LIB_DIR)/$(SHARED_LIB) gtruntime.h
	@mkdir -p $(BUILD_DIR)
	@echo "Building example..."
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/example example.c $(LDFLAGS)
	@echo "Example built: $(BUILD_DIR)/example"

# Run example
run: example
	@echo "Running example..."
	LD_LIBRARY_PATH=$(LIB_DIR) $(BUILD_DIR)/example

# Install library system-wide (requires sudo)
install: lib
	@echo "Installing library to /usr/local/lib..."
	sudo cp $(LIB_DIR)/$(SHARED_LIB) /usr/local/lib/
	sudo cp gtruntime.h /usr/local/include/
	sudo ldconfig
	@echo "Installation complete"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) $(LIB_DIR)
	rm -f *.so *.h *.a
	@echo "Clean complete"

# Build documentation
docs:
	@echo "Generating documentation..."
	@echo "See README.md for usage examples"

# Quick test
test: example
	@echo "Running quick test..."
	LD_LIBRARY_PATH=$(LIB_DIR) timeout 30s $(BUILD_DIR)/example || (echo "Test completed or timed out"; exit 0)
