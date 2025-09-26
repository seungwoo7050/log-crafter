# LogCrafter-C Makefile - MVP2 with thread pool
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2 -D_GNU_SOURCE
LDFLAGS = -pthread
INCLUDES = -I./include

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# Target executable
TARGET = $(BIN_DIR)/logcrafter-c

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Build target
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Clean complete"

# Run the server
run: $(TARGET)
	./$(TARGET)

# Build and run
rebuild: clean all

# Phony targets
.PHONY: all clean run rebuild directories