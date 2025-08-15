# Project settings
SRC_DIR := src
BIN_DIR := bin
BUILD_DIR := build
INCLUDE_DIR := include
LIB_DIR := lib
TESTS_DIR := tests

# Executable settings
PLIMIT := plimit
PLIMIT_VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "0.0.0")

# Libraries settings
LIB_ARGTABLE_REPO := https://github.com/argtable/argtable3/releases/download/v3.3.1/argtable-v3.3.1-amalgamation.tar.gz
LIB_ARGTABLE_NAME := argtable3
LIB_ARGTABLE_DIR := $(LIB_DIR)/argtable
LIB_ARGTABLE_SRC := $(LIB_ARGTABLE_DIR)/argtable3.c

# Compiler settings
CC ?= gcc
LINTER := clang-tidy
FORMATTER := clang-format

# Compiler and Linker flags Settings:
# 	-std=gnu17: Use the GNU17 standard
# 	-D _GNU_SOURCE: Use GNU extensions
# 	-D __STDC_WANT_LIB_EXT1__: Use C11 extensions
# 	-Wall: Enable all warnings
# 	-Wextra: Enable extra warnings
# 	-Werror: Turn all warnings into errors
# 	-pedantic: Enable pedantic warnings
# 	-I$(INCLUDE_DIR): Include the include directory
# 	-I$(LIB_ARGTABLE_DIR): Include the argtable library directory
CFLAGS := -O2 -std=gnu17 -D _GNU_SOURCE -D __STDC_WANT_LIB_EXT1__ -Wall -Wextra -Werror -pedantic -I$(INCLUDE_DIR) -I$(LIB_ARGTABLE_DIR)
LIBS ?= -largtable3
LDFLAGS ?=
PREFIX ?= /usr/local/bin

OBJS := $(PLIMIT).o cgroups.o utils.o $(LIB_ARGTABLE_NAME).o

# Build plimit executable
$(PLIMIT): dir $(OBJS)
	@echo "Building $(PLIMIT) version $(PLIMIT_VERSION)..."
	@$(CC) $(CFLAGS) $(LFLAGS) -o $(BIN_DIR)/$(PLIMIT) $(foreach file,$(OBJS),$(BUILD_DIR)/$(file))
	@echo "Build complete!"

# Build object files
%.o:  dir $(SRC_DIR)/%.c
	@echo "Compiling $*.c..."
	@$(CC) $(CFLAGS) -DPLIMIT_VERSION=\"$(PLIMIT_VERSION)\"  -o $(BUILD_DIR)/$*.o -c $(SRC_DIR)/$*.c

# Build third-party libraries
$(LIB_ARGTABLE_NAME).o: $(LIB_ARGTABLE_SRC)
	@echo "Compiling $(LIB_ARGTABLE_NAME)..."
	@$(CC) $(CFLAGS) -o $(BUILD_DIR)/$(LIB_ARGTABLE_NAME).o -c $(LIB_ARGTABLE_SRC)

.PHONY: build
build: $(PLIMIT) ## Build binary executable

.PHONY: install
install: ## Install binary to $(PREFIX) directory (default: /usr/local/bin)
	@if [ "$$(id -u)" -ne 0 ]; then \
        echo "Error: You must run 'make install' with sudo or as root."; \
        exit 1; \
    fi
	@if [ ! -f "$(BIN_DIR)/$(PLIMIT)" ]; then \
        echo "Error: $(PLIMIT) not built. Run 'make build' first."; \
        exit 1; \
    fi
	@echo "Installing $(BIN_DIR)/$(PLIMIT) $(PREFIX)/$(PLIMIT)..."
	@install -m 700 -o root -g root $(BIN_DIR)/$(PLIMIT) $(PREFIX)/$(PLIMIT)
	@echo "Installation complete."

.PHONY: lint
lint: ## Run linter on source directories
	@echo "Running linter..."
	@$(LINTER) --config-file=.clang-tidy $(SRC_DIR)/* $(INCLUDE_DIR)/* -- $(CFLAGS)
	@echo "Linting complete."

.PHONY: format
format: ## Run formatter on source directories
	@echo "Running formatter..."
	@$(FORMATTER) -style=file -i $(SRC_DIR)/* $(INCLUDE_DIR)/*
	@echo "Formatting complete."

.PHONY: dir
dir:
	@echo "Creating directories..."
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)
	@echo "Directories created."

.PHONY: clean
clean: ## Cleanup build artifacts
	@echo "Cleaning up build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Cleanup complete."

help: ## Shows the help
	@echo 'Usage: make <OPTIONS> ... <TARGETS>'
	@echo ''
	@echo 'Available targets are:'
	@echo ''
	@grep -E '^[ a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
        awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'
	@echo ''
