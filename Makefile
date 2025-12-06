# =============================================================================
# NPU Simulator Makefile
# =============================================================================

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2
DEBUG_FLAGS := -g -DDEBUG
INCLUDE := -Iinclude

# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
BIN_DIR := bin

# Target executable
TARGET := $(BIN_DIR)/npu_sim

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp) \
        $(wildcard $(SRC_DIR)/assembler/*.cpp) \
        $(wildcard $(SRC_DIR)/cpu/*.cpp) \
        $(wildcard $(SRC_DIR)/utils/*.cpp)

# Object files
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Header files (for dependency tracking)
HEADERS := $(wildcard $(INC_DIR)/*.hpp) \
           $(wildcard $(INC_DIR)/assembler/*.hpp) \
           $(wildcard $(INC_DIR)/cpu/*.hpp) \
           $(wildcard $(INC_DIR)/utils/*.hpp)

# =============================================================================
# Main Targets
# =============================================================================

.PHONY: all clean debug run test help dirs compile_commands

all: dirs $(TARGET)

debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: clean all

# Create build directories
dirs:
	@mkdir -p $(BUILD_DIR)/assembler
	@mkdir -p $(BUILD_DIR)/cpu
	@mkdir -p $(BUILD_DIR)/utils
	@mkdir -p $(BIN_DIR)
	@mkdir -p output/traces
	@mkdir -p output/logs

# Link
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)
	@echo "Build complete: $(TARGET)"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# =============================================================================
# Run Targets
# =============================================================================

# Run with a sample program
run: all
	@echo "Running NPU Simulator..."
	@$(TARGET) programs/sample/program1_arithmetic.asm

# Run with verbose output
run-verbose: all
	@echo "Running NPU Simulator (verbose)..."
	@$(TARGET) -v programs/sample/program1_arithmetic.asm

# Run neural network demos
run-dense: all
	@$(TARGET) -v programs/neural_nets/dense_layer.asm

run-mlp: all
	@$(TARGET) -v programs/neural_nets/mlp_2layer.asm

run-rnn: all
	@$(TARGET) -v programs/neural_nets/simple_rnn.asm

# Run all demos
run-all: all
	@echo "=== Running Program 1: Arithmetic ==="
	@$(TARGET) programs/sample/program1_arithmetic.asm
	@echo ""
	@echo "=== Running Program 2: Loop ==="
	@$(TARGET) programs/sample/program2_loop.asm
	@echo ""
	@echo "=== Running Program 3: Function ==="
	@$(TARGET) programs/sample/program3_function.asm
	@echo ""
	@echo "=== Running Dense Layer ==="
	@$(TARGET) programs/neural_nets/dense_layer.asm
	@echo ""
	@echo "=== Running 2-Layer MLP ==="
	@$(TARGET) programs/neural_nets/mlp_2layer.asm
	@echo ""
	@echo "=== Running Simple RNN ==="
	@$(TARGET) programs/neural_nets/simple_rnn.asm

# =============================================================================
# Test Targets
# =============================================================================

test: all
	@echo "Running tests..."
	@$(CXX) $(CXXFLAGS) $(INCLUDE) tests/test_assembler.cpp $(filter-out $(BUILD_DIR)/main.o, $(OBJS)) -o $(BIN_DIR)/test_assembler
	@$(CXX) $(CXXFLAGS) $(INCLUDE) tests/test_decoder.cpp $(filter-out $(BUILD_DIR)/main.o, $(OBJS)) -o $(BIN_DIR)/test_decoder
	@$(CXX) $(CXXFLAGS) $(INCLUDE) tests/test_execution.cpp $(filter-out $(BUILD_DIR)/main.o, $(OBJS)) -o $(BIN_DIR)/test_execution
	@$(BIN_DIR)/test_assembler
	@$(BIN_DIR)/test_decoder
	@$(BIN_DIR)/test_execution

# =============================================================================
# Compile Commands (for clangd)
# =============================================================================

compile_commands:
	@echo "Generating compile_commands.json..."
	@command -v bear >/dev/null 2>&1 && bear -- make clean all || echo "Install 'bear' for automatic generation"

# =============================================================================
# Clean
# =============================================================================

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@rm -rf output/traces/* output/logs/*
	@echo "Clean complete."

# =============================================================================
# Help
# =============================================================================

help:
	@echo "NPU Simulator Build System"
	@echo "=========================="
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build the simulator (default)"
	@echo "  debug        Build with debug symbols"
	@echo "  clean        Remove build artifacts"
	@echo "  run          Run with sample program"
	@echo "  run-verbose  Run with verbose output"
	@echo "  run-dense    Run dense layer demo"
	@echo "  run-mlp      Run MLP demo"
	@echo "  run-rnn      Run RNN demo"
	@echo "  run-all      Run all demo programs"
	@echo "  test         Run unit tests"
	@echo "  help         Show this help message"
	@echo ""
	@echo "Manual run:"
	@echo "  $(TARGET) [options] <program.asm>"
	@echo ""
	@echo "Options:"
	@echo "  -v           Verbose output"
	@echo "  -o <dir>     Output directory for logs"
	@echo "  -h           Show help"


