# NPU Simulator

A RISC-V processor simulator with custom Neural Processing Unit (NPU) extensions for AI/ML workloads. Features both **single-cycle** and **5-stage pipelined** CPU implementations with realistic timing simulation.

## Overview

This simulator implements:
- **Single-cycle CPU** - All stages in one cycle (CPI = 1.0, 100 MHz)
- **5-stage pipelined CPU** - Concurrent execution with hazard handling (CPI ≈ 1.2, 500 MHz)
  - Data forwarding (EX-to-EX, MEM-to-EX)
  - Load-use hazard detection and stalling
  - Control hazard handling with branch flushing
  - **3-5x faster than single-cycle in practice!**
- **Custom NPU ISA** with 11 vector instructions for neural network operations
- **RV32I subset** for scalar operations (arithmetic, memory, branches, jumps)
- **Two-pass assembler** that converts `.asm` files to machine code
- **Realistic timing simulation** showing actual execution time, not just cycles
- **Execution traces** in terminal, JSON, and CSV formats
- **Neural network demos** (Dense layer, MLP, RNN)
- **Hazard test programs** demonstrating pipeline behavior

## Quick Start

```bash
# Build the simulator
make

# Run with single-cycle CPU (default)
./bin/npu_sim programs/sample/program1_arithmetic.asm

# Run with pipelined CPU
./bin/npu_sim -p programs/sample/program1_arithmetic.asm

# Compare performance (shows pipelined is 3-5x faster!)
make compare-all

# Run with verbose output (cycle-by-cycle trace)
./bin/npu_sim -p -v programs/sample/program1_arithmetic.asm

# Run neural network demo
./bin/npu_sim -p programs/neural_nets/mlp_2layer.asm
```

📖 **For comprehensive usage instructions, see [USER_GUIDE.md](docs/USER_GUIDE.md)**

## Project Structure

```
npu-simulator/
├── include/              # Header files
│   ├── common.hpp        # Type definitions, constants
│   ├── instruction.hpp   # Instruction encoding/decoding
│   ├── memory.hpp        # Memory subsystem
│   ├── registers.hpp     # Scalar + Vector register files
│   ├── decoder.hpp       # Instruction decoder
│   ├── alu.hpp           # ALU operations
│   ├── npu_unit.hpp      # NPU/Vector execution unit
│   ├── assembler/        # Assembler components
│   └── cpu/              # CPU implementations
├── src/                  # Source files
├── programs/             # Assembly programs
│   ├── sample/           # Basic test programs (3 programs)
│   ├── neural_nets/      # Neural network demos (3 programs)
│   └── hazards/          # Pipeline hazard tests (5 programs)
├── tests/                # Unit tests
├── output/               # Generated traces and logs
└── docs/                 # Documentation
```

## Supported Instructions

### NPU Instructions (Opcode 0x77)

| Instruction | Description | Example |
|-------------|-------------|---------|
| `VLOAD`     | Load 128-bit vector from memory | `VLOAD V1, 0(x10)` |
| `VSTORE`    | Store 128-bit vector to memory | `VSTORE V1, 0(x10)` |
| `VLBC`      | Broadcast load (scalar → vector) | `VLBC V2, 4(x10)` |
| `VADD`      | Vector addition | `VADD V3, V1, V2` |
| `VMUL`      | Vector multiplication | `VMUL V3, V1, V2` |
| `VMAC`      | Multiply-accumulate | `VMAC V0, V1, V2` |
| `VRELU`     | ReLU activation | `VRELU V1, V1` |
| `VCLP`      | Clamp values | `VCLP V1, V1, 255` |
| `VREDMAX`   | Reduce to max value | `VREDMAX x5, V1` |
| `VARGMAX`   | Argmax (index of max) | `VARGMAX x5, V1` |
| `VCLR`      | Clear vector to zeros | `VCLR V0` |

### RV32I Subset

| Category | Instructions |
|----------|--------------|
| Arithmetic | `ADD`, `SUB`, `ADDI` |
| Logic | `AND`, `OR`, `XOR`, `ANDI`, `ORI`, `XORI` |
| Shifts | `SLL`, `SRL`, `SRA`, `SLLI`, `SRLI`, `SRAI` |
| Compare | `SLT`, `SLTU`, `SLTI`, `SLTIU` |
| Memory | `LW`, `SW` |
| Branch | `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU` |
| Jump | `JAL`, `JALR` |
| Upper | `LUI`, `AUIPC` |
| Pseudo | `NOP`, `RET`, `J`, `MV`, `LI`, `HALT` |

## Architecture

### Registers

- **32 Scalar Registers** (x0-x31): 32-bit each, x0 hardwired to 0
- **8 Vector Registers** (V0-V7): 128-bit each (4 × 32-bit integers)

### Memory

- 64KB byte-addressable memory (configurable)
- Little-endian byte ordering
- Text segment starts at 0x0000
- Data segment starts at 0x2000

### Execution Models

#### Single-Cycle CPU
Each instruction completes in one cycle:

```
┌─────────┬─────────┬─────────┬─────────┬─────────┐
│   IF    │   ID    │   EX    │   MEM   │   WB    │
│  Fetch  │ Decode  │ Execute │ Memory  │  Write  │
└─────────┴─────────┴─────────┴─────────┴─────────┘
          All stages in one cycle
          CPI = 1.0, Clock = 100 MHz
```

#### Pipelined CPU (5-Stage)
Instructions flow through stages concurrently:

```
Cycle 1: │  IF   │       │       │       │       │
Cycle 2: │  IF   │  ID   │       │       │       │
Cycle 3: │  IF   │  ID   │  EX   │       │       │
Cycle 4: │  IF   │  ID   │  EX   │  MEM  │       │
Cycle 5: │  IF   │  ID   │  EX   │  MEM  │  WB   │ ← Steady state
         └───────┴───────┴───────┴───────┴───────┘
         CPI ≈ 1.2 (with hazards), Clock = 500 MHz
         3-5x faster than single-cycle in practice!
```

**Hazard Handling:**
- ✅ Data forwarding (EX→EX, MEM→EX)
- ✅ Load-use stalls (1 cycle penalty)
- ✅ Branch flushes (predict not taken)
- ✅ Works for both scalar and vector operations

## Command Line Options

```
./bin/npu_sim [options] <program.asm>

Options:
  -v, --verbose     Enable verbose output (cycle-by-cycle trace)
  -p, --pipelined   Use 5-stage pipelined CPU (default: single-cycle)
  -o, --output DIR  Output directory for logs (default: output)
  -t, --no-trace    Disable trace logging
  -j, --no-json     Disable JSON output
  -c, --no-csv      Disable CSV trace output
  -m, --memory KB   Memory size in KB (default: 64)
  -h, --help        Show help message
```

## Output Files

### Terminal Output (Single-Cycle)

```
============================================
          EXECUTION STATISTICS              
============================================
CPU Mode:            Single-Cycle
Clock Frequency:          100.0 MHz
Total Cycles:                80
CPI:                      1.000
Execution Time:          800.00 ns
Throughput:               100.0 MIPS
============================================
```

### Terminal Output (Pipelined)

```
============================================
    PIPELINED EXECUTION STATISTICS          
============================================
Clock Frequency:          500.0 MHz
Total Cycles:               134
CPI:                      1.252
Execution Time:          268.00 ns  ← 3x faster!
Throughput:               399.3 MIPS
--------------------------------------------
Pipeline Stalls:             24
Forwarding Events:           61
============================================
```

### JSON Log (output/logs/run_*.json)

```json
{
  "program": "mlp_2layer.asm",
  "total_cycles": 47,
  "instruction_count": 47,
  "cpi": 1.0,
  "npu_instructions": 28,
  "final_vector_registers": {
    "V0": [10, 40, 90, 160]
  }
}
```

### CSV Trace (output/traces/trace_*.csv)

```csv
cycle,pc,instruction,rd,rs1,rs2,result
1,0x0000,ADDI,x10,x0,-,256
2,0x0004,VLOAD,V1,x10,-,"[10, 20, 30, 40]"
```

## Neural Network Demos

### 1. Dense Layer (`dense_layer.asm`)
Single fully-connected layer with ReLU activation.

### 2. 2-Layer MLP (`mlp_2layer.asm`)
Two-layer perceptron: Input → Hidden (ReLU) → Output → ArgMax

### 3. Simple RNN (`simple_rnn.asm`)
Recurrent neural network: `h_new = ReLU(W_h * h_old + W_x * x)`

## Building and Running

```bash
# Build release
make

# Build with debug symbols
make debug

# Run tests
make test

# Clean build artifacts
make clean

# Run all demos (single-cycle + neural nets)
make run-all

# Run hazard tests (pipelined-specific)
make run-hazards

# Full performance comparison
make compare-all
```

### Make Targets

| Target | Description |
|--------|-------------|
| `make` | Build the simulator |
| `make run` | Run sample program (single-cycle) |
| `make run-pipelined` | Run with pipelined CPU |
| `make run-all` | Run all demo programs |
| `make run-hazards` | Run hazard test programs |
| `make compare-all` | Compare single-cycle vs pipelined |
| `make test` | Run unit tests |
| `make clean` | Remove build artifacts |

📖 **See [USER_GUIDE.md](docs/USER_GUIDE.md) for detailed usage instructions**

## Performance Comparison

### Simple RNN Example (80 instructions)

| Mode | Cycles | Execution Time | Throughput |
|------|--------|----------------|------------|
| **Single-Cycle** | 80 | 800 ns | 100 MIPS |
| **Pipelined** | 134 (+68%) | **268 ns** | **399 MIPS** |
| **Speedup** | - | **2.98x faster!** | 4x higher |

Despite having 68% more cycles, pipelined is **3x faster** due to 5x higher clock frequency!

### All Programs

| Program | Single-Cycle Time | Pipelined Time | Speedup |
|---------|------------------|----------------|---------|
| program1_arithmetic | 90 ns | 26 ns | **3.46x** |
| program2_loop | 280 ns | 80 ns | **3.50x** |
| mlp_2layer | 330 ns | 106 ns | **3.11x** |
| simple_rnn | 800 ns | 268 ns | **2.98x** |

Run `./compare_performance.sh` or `make compare-all` to see full comparison!

## Documentation

| Document | Description |
|----------|-------------|
| [USER_GUIDE.md](docs/USER_GUIDE.md) | **Comprehensive usage guide** (make targets, options, workflows) |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | System architecture and design |
| [ISA_REFERENCE.md](docs/ISA_REFERENCE.md) | Complete instruction set reference |
| [EXECUTION_FLOW.md](docs/EXECUTION_FLOW.md) | Detailed execution flow |
| [PIPELINE_IMPLEMENTATION.md](PIPELINE_IMPLEMENTATION.md) | Pipeline design and hazards |
| [TIMING_ANALYSIS.md](TIMING_ANALYSIS.md) | Performance analysis and timing |

## Authors

- Muhammad Haseeb ul Haq (454512)
- Bilal Rana (454035)
- Muhammad Moiz (464192)
- Muhammad Samama Usaman (454520)

## License

This project is part of the Computer Architecture course at SEECS, NUST.

