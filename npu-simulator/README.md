# NPU Simulator

A single-cycle RISC-V processor simulator with custom Neural Processing Unit (NPU) extensions for AI/ML workloads.

## Overview

This simulator implements:
- **Single-cycle CPU** with IF → ID → EX → MEM → WB stages
- **Custom NPU ISA** with 11 vector instructions for neural network operations
- **RV32I subset** for scalar operations (arithmetic, memory, branches, jumps)
- **Two-pass assembler** that converts `.asm` files to machine code
- **Execution traces** in terminal, JSON, and CSV formats
- **Neural network demos** (Dense layer, MLP, RNN)

## Quick Start

```bash
# Build the simulator
make

# Run a sample program
./bin/npu_sim programs/sample/program1_arithmetic.asm

# Run with verbose output (cycle-by-cycle trace)
./bin/npu_sim -v programs/sample/program1_arithmetic.asm

# Run neural network demo
./bin/npu_sim -v programs/neural_nets/mlp_2layer.asm
```

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
│   ├── sample/           # Basic test programs
│   └── neural_nets/      # Neural network demos
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

### Execution Model

Single-cycle execution: each instruction completes in one cycle.

```
┌─────────┬─────────┬─────────┬─────────┬─────────┐
│   IF    │   ID    │   EX    │   MEM   │   WB    │
│  Fetch  │ Decode  │ Execute │ Memory  │  Write  │
│         │         │         │ Access  │  Back   │
└─────────┴─────────┴─────────┴─────────┴─────────┘
          All stages in one cycle (CPI = 1.0)
```

## Command Line Options

```
./bin/npu_sim [options] <program.asm>

Options:
  -v, --verbose     Enable verbose output (cycle-by-cycle trace)
  -o, --output DIR  Output directory for logs (default: output)
  -t, --no-trace    Disable trace logging
  -j, --no-json     Disable JSON output
  -c, --no-csv      Disable CSV trace output
  -m, --memory KB   Memory size in KB (default: 64)
  -h, --help        Show help message
```

## Output Files

### Terminal Output

```
[Cycle    5] PC=0x0010 | VMAC V0, V1, V2
  VMAC V0 | V_src1: [10, 20, 30, 40] | V_src2: [1, 2, 3, 4] -> [10, 40, 90, 160]
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

## Building

```bash
# Build release
make

# Build with debug symbols
make debug

# Run tests
make test

# Clean build artifacts
make clean

# Run all demos
make run-all
```

## Future Extensions

The architecture supports easy extension to a pipelined implementation:

```cpp
class PipelinedCPU : public CPUBase {
    IF_ID_Reg if_id;
    ID_EX_Reg id_ex;
    EX_MEM_Reg ex_mem;
    MEM_WB_Reg mem_wb;
    HazardUnit hazardUnit;
    ForwardingUnit forwardingUnit;
};
```

## Authors

- Muhammad Haseeb ul Haq (454512)
- Bilal Rana (454035)
- Muhammad Moiz (464192)
- Muhammad Samama Usaman (454520)

## License

This project is part of the Computer Architecture course at SEECS, NUST.

