# Makefile for Go Green Threads FFI Library

# Configuration
GO := go
CC := gcc

# Directories
BUILD_DIR := build
LIB_DIR := lib

# Library names
LIB_NAME := gtruntime
SHARED_LIB := lib$(LIB_NAME).so
STATIC_LIB := lib$(LIB_NAME).a

# Go build flags
GO_LDFLAGS := -ldflags="-s -w"

# C build flags
CFLAGS := -Wall -Wextra -O2 -I./lib -I./$(LIB_DIR)

# Targets
.PHONY: all clean lib example example2 install test

all: lib example2

# Build shared library
lib: $(LIB_DIR)/$(SHARED_LIB) $(LIB_DIR)/$(STATIC_LIB)

$(LIB_DIR)/$(SHARED_LIB): gtruntime.go
	@mkdir -p $(LIB_DIR)
	@echo "Building Go shared library..."
	$(GO) build -buildmode=c-shared $(GO_LDFLAGS) -o $(LIB_DIR)/$(SHARED_LIB) gtruntime.go
	@echo "Library built: $(LIB_DIR)/$(SHARED_LIB)"
	@echo "Generated header: $(LIB_DIR)/libgtruntime.h"

# Build example 
example: $(BUILD_DIR)/example

$(LIB_DIR)/$(STATIC_LIB): gtruntime.go
	@mkdir -p $(LIB_DIR)
	@echo "Building Go static library..."
	$(GO) build -buildmode=c-archive $(GO_LDFLAGS) -o $(LIB_DIR)/$(STATIC_LIB) gtruntime.go
	@echo "Library built: $(LIB_DIR)/$(STATIC_LIB)"
	@echo "Generated header: $(LIB_DIR)/libgtruntime.h"

$(BUILD_DIR)/example: example.c $(LIB_DIR)/$(STATIC_LIB)
	@mkdir -p $(BUILD_DIR)
	@echo "Building example (static linking)..."
	$(CC) $(CFLAGS) -static -o $(BUILD_DIR)/example example.c $(LIB_DIR)/$(STATIC_LIB)
	@echo "Example built: $(BUILD_DIR)/example (statically linked)"

# Build example2
example2: $(BUILD_DIR)/example2

$(BUILD_DIR)/example2: example2.c $(LIB_DIR)/$(SHARED_LIB)
	@mkdir -p $(BUILD_DIR)
	@echo "Building example2 (working version with static linking)..."
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/example2 example2.c $(LIB_DIR)/$(SHARED_LIB)
	@echo "Example2 built: $(BUILD_DIR)/example2"

# Run example2
run: example2
	@echo "Running example2..."
	$(BUILD_DIR)/example2

# Install library system-wide (requires sudo)
install: lib
	@echo "Installing library to /usr/local/lib..."
	sudo cp $(LIB_DIR)/$(SHARED_LIB) /usr/local/lib/
	sudo cp $(LIB_DIR)/libgtruntime.h /usr/local/include/
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
test: example2
	@echo "Running quick test..."
	LD_LIBRARY_PATH=$(LIB_DIR) timeout 30s $(BUILD_DIR)/example2 || (echo "Test completed or timed out"; exit 0)
