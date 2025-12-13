# NPU Simulator - User Guide

## Table of Contents
1. [Quick Start](#quick-start)
2. [Building the Simulator](#building-the-simulator)
3. [Running Programs](#running-programs)
4. [CPU Modes](#cpu-modes)
5. [Make Targets Reference](#make-targets-reference)
6. [Command Line Options](#command-line-options)
7. [Example Workflows](#example-workflows)
8. [Understanding Output](#understanding-output)

---

## Quick Start

```bash
# Build the simulator
make

# Run a simple program (single-cycle CPU)
./bin/npu_sim programs/sample/program1_arithmetic.asm

# Run with pipelined CPU
./bin/npu_sim -p programs/sample/program1_arithmetic.asm

# Compare performance of both modes
make compare-all
```

---

## Building the Simulator

### Standard Build

```bash
make
```

This compiles the simulator with optimizations (`-O2`) and creates the executable at `bin/npu_sim`.

### Debug Build

```bash
make debug
```

Builds with debug symbols (`-g`) and enables debugging macros.

### Clean Build

```bash
make clean  # Remove all build artifacts
make        # Rebuild from scratch
```

### Build Output

```
Compiling src/alu.cpp...
Compiling src/decoder.cpp...
...
Linking bin/npu_sim...
Build complete: bin/npu_sim
```

---

## Running Programs

### Basic Execution

```bash
# Single-cycle CPU (default)
./bin/npu_sim <program.asm>

# Pipelined CPU
./bin/npu_sim -p <program.asm>

# Verbose output (cycle-by-cycle trace)
./bin/npu_sim -v <program.asm>

# Pipelined with verbose output
./bin/npu_sim -p -v <program.asm>
```

### Available Programs

#### Sample Programs (Basic Tests)

| Program | Description | Instructions |
|---------|-------------|--------------|
| `programs/sample/program1_arithmetic.asm` | Vector arithmetic operations | 9 |
| `programs/sample/program2_loop.asm` | Loop with dot product | 28 |
| `programs/sample/program3_function.asm` | Function calls and ReLU | 12 |

#### Neural Network Programs

| Program | Description | Instructions |
|---------|-------------|--------------|
| `programs/neural_nets/dense_layer.asm` | Single dense layer | 22 |
| `programs/neural_nets/mlp_2layer.asm` | 2-layer MLP | 33 |
| `programs/neural_nets/simple_rnn.asm` | Simple RNN cell | 80 |

#### Hazard Test Programs (Pipelined-Specific)

| Program | Description | Purpose |
|---------|-------------|---------|
| `programs/hazards/data_forwarding.asm` | RAW hazards with forwarding | Test forwarding logic |
| `programs/hazards/load_use.asm` | Load-use hazards | Test pipeline stalls |
| `programs/hazards/control_hazard.asm` | Branch/jump hazards | Test pipeline flushes |
| `programs/hazards/mixed_hazards.asm` | Combined hazard scenarios | Test complex interactions |
| `programs/hazards/npu_intensive.asm` | NPU vector operations | Test vector forwarding |

---

## CPU Modes

### Single-Cycle CPU (Default)

**Characteristics:**
- All 5 stages (IF, ID, EX, MEM, WB) execute in one cycle
- CPI = 1.000 (by definition)
- Clock frequency: 100 MHz (10 ns period)
- Simple, no hazards to handle
- More cycles for long critical path

**When to use:**
- Understanding basic instruction execution
- Baseline performance comparison
- Simple programs without complex dependencies

**Example:**
```bash
./bin/npu_sim programs/sample/program1_arithmetic.asm
```

**Output shows:**
```
CPU Mode:            Single-Cycle
Clock Frequency:          100.0 MHz
Total Cycles:                 9
CPI:                      1.000
Execution Time:           90.00 ns
```

### Pipelined CPU (5-Stage)

**Characteristics:**
- Instructions flow through 5 stages concurrently
- CPI ≈ 1.1-1.3 (due to hazards)
- Clock frequency: 500 MHz (2 ns period)
- Handles hazards with stalls, flushes, and forwarding
- **Much faster in practice** despite more cycles

**When to use:**
- Realistic performance modeling
- Understanding pipeline hazards
- Demonstrating modern CPU design
- Performance optimization studies

**Example:**
```bash
./bin/npu_sim -p programs/sample/program1_arithmetic.asm
```

**Output shows:**
```
CPU Mode:            Pipelined (5-stage)
Clock Frequency:          500.0 MHz
Total Cycles:                13
CPI:                      1.182
Execution Time:           26.00 ns
Pipeline Stalls:              1
Forwarding Events:            3
```

---

## Make Targets Reference

### Build Targets

```bash
make              # Build simulator (default)
make all          # Same as 'make'
make debug        # Build with debug symbols
make clean        # Remove all build artifacts
```

### Single Program Execution

```bash
make run              # Run program1_arithmetic.asm (single-cycle)
make run-verbose      # Run with verbose output
make run-pipelined    # Run with pipelined CPU
```

### Neural Network Demos

```bash
make run-dense    # Run dense_layer.asm with verbose output
make run-mlp      # Run mlp_2layer.asm with verbose output
make run-rnn      # Run simple_rnn.asm with verbose output
make run-all      # Run ALL demo programs (sample + neural nets)
```

**Example Output (run-all):**
```
=== Running Program 1: Arithmetic ===
[execution output]

=== Running Program 2: Loop ===
[execution output]

=== Running Dense Layer ===
[execution output]
...
```

### Hazard Testing

```bash
make run-hazards  # Run all 5 hazard test programs (pipelined mode)
```

**Runs:**
1. data_forwarding.asm
2. load_use.asm
3. control_hazard.asm
4. mixed_hazards.asm
5. npu_intensive.asm

### Performance Comparison

```bash
make compare      # Quick comparison (one program)
make compare-all  # Comprehensive comparison (ALL programs)
```

**`make compare` output:**
```
=== Comparing Single-Cycle vs Pipelined ===

--- Single-Cycle (program2_loop.asm) ---
Total Cycles:                28
Execution Time:          280.00 ns

--- Pipelined (program2_loop.asm) ---
Total Cycles:                40
Execution Time:           80.00 ns
```

**`make compare-all` output:**
```
╔════════════════════════════════════════════════════════╗
║          PERFORMANCE COMPARISON                         ║
╚════════════════════════════════════════════════════════╝

Program: program1_arithmetic
─────────────────────────────────────────────────────
Metric          | Single-Cycle | Pipelined  | Speedup
Cycles          |            9 |         13 | -
Execution Time  |       90 ns  |      26 ns | 3.46x
Throughput      |  100.0 MIPS  | 423.1 MIPS | 4.23x
⚡ RESULT: Pipelined is 3.46x FASTER!
...
```

### Testing

```bash
make test    # Run unit tests (assembler, decoder, execution)
```

### Help

```bash
make help    # Show all available targets
```

---

## Command Line Options

### Basic Options

```bash
./bin/npu_sim [options] <program.asm>
```

| Option | Description |
|--------|-------------|
| `-v, --verbose` | Enable verbose cycle-by-cycle trace output |
| `-p, --pipelined` | Use 5-stage pipelined CPU (default: single-cycle) |
| `-o, --output DIR` | Output directory for logs (default: `output`) |
| `-t, --no-trace` | Disable trace logging |
| `-j, --no-json` | Disable JSON output |
| `-c, --no-csv` | Disable CSV trace output |
| `-m, --memory KB` | Memory size in KB (default: 64) |
| `-h, --help` | Show help message |

### Examples

```bash
# Verbose output with pipelined CPU
./bin/npu_sim -p -v programs/neural_nets/mlp_2layer.asm

# Custom output directory
./bin/npu_sim -o results programs/sample/program2_loop.asm

# Disable file output, just show terminal results
./bin/npu_sim -t programs/sample/program1_arithmetic.asm

# Pipelined with larger memory
./bin/npu_sim -p -m 128 programs/neural_nets/simple_rnn.asm
```

---

## Example Workflows

### Workflow 1: Basic Program Execution

```bash
# 1. Build the simulator
make

# 2. Run a simple program
./bin/npu_sim programs/sample/program1_arithmetic.asm

# 3. Check the output
cat output/logs/run_*.json
```

### Workflow 2: Comparing CPU Modes

```bash
# Run with single-cycle
./bin/npu_sim programs/sample/program2_loop.asm > single_cycle.txt

# Run with pipelined
./bin/npu_sim -p programs/sample/program2_loop.asm > pipelined.txt

# Compare outputs
diff single_cycle.txt pipelined.txt
```

### Workflow 3: Testing Neural Networks

```bash
# Run all neural network programs
make run-dense
make run-mlp
make run-rnn

# Or run all at once
make run-all
```

### Workflow 4: Analyzing Pipeline Performance

```bash
# Run hazard tests with verbose output
./bin/npu_sim -p -v programs/hazards/load_use.asm

# Look for stalls and forwarding in output
# Check performance metrics
```

### Workflow 5: Comprehensive Performance Analysis

```bash
# Run full comparison
./compare_performance.sh

# Or use make target
make compare-all

# Review timing analysis
# Shows speedup for all programs
```

### Workflow 6: Debugging a Program

```bash
# Build with debug symbols
make debug

# Run with verbose output
./bin/npu_sim -v programs/sample/program3_function.asm

# Check cycle-by-cycle execution
# Inspect register values at each step
```

---

## Understanding Output

### Terminal Output Structure

```
================================================
         NPU Simulator - RISC-V + NPU          
================================================

Input file: programs/sample/program1_arithmetic.asm
CPU Mode:   Single-Cycle  (or Pipelined)
Verbose:    disabled
Output dir: output

=== ASSEMBLY PHASE ===
Assembly successful: 9 instructions

=== MEMORY INITIALIZATION ===
Memory initialized with test data

=== EXECUTION PHASE ===
[Verbose cycle-by-cycle output if -v enabled]

=== EXECUTION COMPLETE ===

[STATISTICS TABLE]

[REGISTER STATE]
```

### Statistics Output (Single-Cycle)

```
============================================
          EXECUTION STATISTICS              
============================================
CPU Mode:            Single-Cycle
Clock Frequency:          100.0 MHz
Clock Period:             10.00 ns
--------------------------------------------
Total Cycles:                 9
Instructions:                 9
CPI:                      1.000
--------------------------------------------
Execution Time:           90.00 ns     ← Real time!
                          0.090 μs
Throughput:               100.0 MIPS
--------------------------------------------
NPU Instructions:             5
Scalar Instructions:          4
Memory Reads:                 2
Memory Writes:                1
============================================
```

### Statistics Output (Pipelined)

```
============================================
    PIPELINED EXECUTION STATISTICS          
============================================
Clock Frequency:          500.0 MHz
Clock Period:              2.00 ns
--------------------------------------------
Total Cycles:                13
Instructions:                11
CPI:                      1.182
--------------------------------------------
Execution Time:           26.00 ns     ← 3.46x faster!
                          0.026 μs
Throughput:               423.1 MIPS
--------------------------------------------
Pipeline Stalls:              1          ← Hazard info
  Load-Use Stalls:            1
Pipeline Flushes:             0
  Branch Flushes:             0
Forwarding Events:            3          ← Optimization
Data Hazards:                 1
Control Hazards:              0
--------------------------------------------
NPU Instructions:             6
Scalar Instructions:          5
============================================
```

### Key Metrics Explained

| Metric | Meaning |
|--------|---------|
| **Total Cycles** | Number of clock cycles executed |
| **CPI** | Cycles Per Instruction (1.0 for single-cycle, >1.0 for pipelined) |
| **Execution Time** | **Most important!** Actual time in nanoseconds |
| **Throughput** | Million Instructions Per Second (MIPS) |
| **Pipeline Stalls** | Cycles wasted due to data hazards |
| **Pipeline Flushes** | Cycles wasted due to control hazards |
| **Forwarding Events** | Data dependencies resolved without stalling |

### Verbose Output (with -v flag)

```
[Cycle    1] PC=0x0000 | ADDI a0, x0, 256
[Cycle    2] PC=0x0004 | ADDI a1, x0, 512
  VLOAD V1 <- Mem[0x0100] = [10, 20, 30, 40]
...
```

For pipelined mode with -v:
```
[Cycle    1] Pipeline State:
  IF: ADDI (PC=0x0000)
  ID: bubble
  EX: bubble
  MEM: bubble
  WB: bubble

[Cycle    2] Pipeline State:
  IF: ADDI (PC=0x0004)
  ID: ADDI (PC=0x0000)
  EX: bubble
  [STALL] Load-use hazard detected
...
```

### Output Files

After execution, check these directories:

```
output/
├── traces/
│   └── trace_2025-12-13_17-19-03.csv    ← Cycle-by-cycle CSV
└── logs/
    └── run_2025-12-13_17-19-03.json     ← Summary JSON
```

**CSV Format:**
```csv
cycle,pc,instruction,rd,rs1,rs2,result
1,0x0000,ADDI,a0,x0,-,256
2,0x0004,VLOAD,V1,a0,-,"[10, 20, 30, 40]"
```

**JSON Format:**
```json
{
  "program": "program1_arithmetic.asm",
  "timestamp": "2025-12-13_17-19-03",
  "total_cycles": 9,
  "cpi": 1.000,
  "execution_time_ns": 90.0,
  "final_scalar_registers": {
    "a0": 256,
    "a1": 512
  },
  "final_vector_registers": {
    "V1": [10, 20, 30, 40]
  }
}
```

---

## Common Use Cases

### 1. Quick Test of a New Program

```bash
make
./bin/npu_sim programs/sample/program1_arithmetic.asm
```

### 2. Performance Comparison

```bash
make compare-all
# Shows speedup for all programs
```

### 3. Understanding Pipeline Behavior

```bash
./bin/npu_sim -p -v programs/hazards/load_use.asm
# Watch for [STALL] messages
```

### 4. Validating Correctness

```bash
# Run same program in both modes
./bin/npu_sim programs/neural_nets/mlp_2layer.asm
./bin/npu_sim -p programs/neural_nets/mlp_2layer.asm
# Compare final register values (should be identical)
```

### 5. Batch Processing

```bash
# Run all programs
make run-all

# Run all hazard tests
make run-hazards
```

---

## Troubleshooting

### Build Issues

**Problem:** Compilation errors
```bash
# Clean and rebuild
make clean
make
```

**Problem:** Missing dependencies
- Requires: g++ with C++17 support
- Check: `g++ --version`

### Runtime Issues

**Problem:** "Assembly failed"
- Check your .asm syntax
- Ensure labels are defined before use
- Verify instruction spelling

**Problem:** "Memory access out of bounds"
- Check memory addresses in your program
- Use `-m` flag to increase memory if needed

**Problem:** Output files not generated
- Check `output/` directory exists
- Use `-o` flag to specify different location
- Verify write permissions

### Performance Issues

**Problem:** Pipelined showing unexpected stalls
- Use `-v` flag to see detailed pipeline state
- Check for load-use dependencies
- Review forwarding events

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────┐
│                  NPU Simulator Commands                  │
├─────────────────────────────────────────────────────────┤
│ BUILD                                                    │
│   make              Build simulator                      │
│   make clean        Clean build artifacts               │
│                                                          │
│ RUN SINGLE PROGRAM                                       │
│   ./bin/npu_sim <file>         Single-cycle            │
│   ./bin/npu_sim -p <file>      Pipelined               │
│   ./bin/npu_sim -v <file>      Verbose output          │
│   ./bin/npu_sim -p -v <file>   Pipelined + verbose     │
│                                                          │
│ RUN DEMOS                                                │
│   make run-all      All demos (sample + neural nets)   │
│   make run-hazards  All hazard tests (pipelined)       │
│                                                          │
│ COMPARE PERFORMANCE                                      │
│   make compare-all  Full comparison with timing         │
│                                                          │
│ KEY FILES                                                │
│   bin/npu_sim                  Simulator executable    │
│   programs/sample/*.asm        Basic test programs     │
│   programs/neural_nets/*.asm   AI demos                │
│   programs/hazards/*.asm       Pipeline tests          │
│   output/traces/*.csv          Execution traces        │
│   output/logs/*.json           Performance logs        │
└─────────────────────────────────────────────────────────┘
```

---

## Next Steps

1. **Try the examples**: Start with `make run-all`
2. **Compare modes**: Run `make compare-all`
3. **Write your own**: Create a custom .asm program
4. **Explore hazards**: Test with `make run-hazards`
5. **Read the docs**: Check other documentation files:
   - `ARCHITECTURE.md` - System design
   - `ISA_REFERENCE.md` - Instruction set
   - `EXECUTION_FLOW.md` - Detailed execution
   - `PIPELINE_IMPLEMENTATION.md` - Pipeline details
   - `TIMING_ANALYSIS.md` - Performance analysis

---

## Support & Documentation

For more information, see:
- Architecture: `docs/ARCHITECTURE.md`
- ISA Reference: `docs/ISA_REFERENCE.md`
- Pipeline Details: `PIPELINE_IMPLEMENTATION.md`
- Timing Analysis: `TIMING_ANALYSIS.md`

Questions? Check the project README or examine the example programs in `programs/`.

