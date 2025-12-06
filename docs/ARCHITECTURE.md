# NPU Simulator Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        NPU Simulator                             │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐  │
│  │  Assembler  │───▶│   Memory    │◀──▶│   SingleCycleCPU    │  │
│  │  (2-pass)   │    │  (64 KB)    │    │                     │  │
│  └─────────────┘    └─────────────┘    │  ┌───────────────┐  │  │
│                                         │  │ RegisterFile  │  │  │
│  ┌─────────────┐                        │  │ - 32 Scalar   │  │  │
│  │   Lexer     │                        │  │ - 8 Vector    │  │  │
│  └─────────────┘                        │  └───────────────┘  │  │
│        │                                │         │           │  │
│        ▼                                │         ▼           │  │
│  ┌─────────────┐                        │  ┌───────────────┐  │  │
│  │   Parser    │                        │  │    Decoder    │  │  │
│  └─────────────┘                        │  └───────────────┘  │  │
│        │                                │         │           │  │
│        ▼                                │    ┌────┴────┐      │  │
│  ┌─────────────┐                        │    ▼         ▼      │  │
│  │  Encoder    │                        │  ┌───┐    ┌─────┐   │  │
│  └─────────────┘                        │  │ALU│    │ NPU │   │  │
│                                         │  └───┘    │ Unit│   │  │
│  ┌─────────────┐                        │           └─────┘   │  │
│  │   Logger    │◀───────────────────────│                     │  │
│  └─────────────┘                        └─────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Components

### 1. Assembler

The assembler converts assembly source code to machine code using a two-pass approach:

```
Pass 1: Build Symbol Table
┌─────────────────────────────────────┐
│ Input: Source tokens                │
│ Output: Label → Address mapping     │
│                                     │
│ - Scan for label definitions        │
│ - Track instruction addresses       │
│ - Build symbol table                │
└─────────────────────────────────────┘

Pass 2: Encode Instructions
┌─────────────────────────────────────┐
│ Input: Tokens + Symbol Table        │
│ Output: Machine code words          │
│                                     │
│ - Parse instruction operands        │
│ - Resolve label references          │
│ - Encode to 32-bit words            │
└─────────────────────────────────────┘
```

### 2. Memory Subsystem

```cpp
class Memory {
    std::vector<uint8_t> data;  // Byte-addressable
    
    // Word access (32-bit)
    uint32_t readWord(Address addr);
    void writeWord(Address addr, uint32_t value);
    
    // Vector access (128-bit)
    Vector128 readVector(Address addr);
    void writeVector(Address addr, const Vector128& vec);
};
```

**Memory Map:**
```
0x0000 - 0x1FFF : Text Segment (8 KB)
0x2000 - 0x7FFF : Data Segment (24 KB)
0x8000 - 0xFFFF : Stack/Heap (32 KB)
```

### 3. Register File

```cpp
class RegisterFile {
    ScalarRegisterFile scalar;  // 32 × 32-bit
    VectorRegisterFile vector;  // 8 × 128-bit
};
```

**Scalar Registers:**
- x0 is hardwired to 0
- Writes to x0 are ignored
- ABI names supported (ra, sp, t0-t6, a0-a7, s0-s11)

**Vector Registers:**
- V0-V7, each 128 bits (4 × int32)
- All elements accessible in parallel

### 4. Decoder

The decoder extracts instruction fields and determines operation type:

```cpp
struct DecodedInstr {
    InstrType type;     // R, I, S, B, U, J
    uint8_t opcode, rd, rs1, rs2;
    uint8_t funct3, funct7;
    int32_t imm;
    
    bool isNPU;
    NPUOp npuOp;
    ALUOp aluOp;
    BranchOp branchOp;
    
    bool isLoad, isStore, isBranch, isJump;
};
```

### 5. ALU (Scalar)

Supports RV32I operations:

| Operation | Description |
|-----------|-------------|
| ADD | Addition |
| SUB | Subtraction |
| AND | Bitwise AND |
| OR | Bitwise OR |
| XOR | Bitwise XOR |
| SLT | Set less than (signed) |
| SLTU | Set less than (unsigned) |
| SLL | Shift left logical |
| SRL | Shift right logical |
| SRA | Shift right arithmetic |

### 6. NPU Unit (Vector)

Executes SIMD operations on 4-element vectors:

```cpp
class NPUUnit {
    static Vector128 vadd(const Vector128& a, const Vector128& b);
    static Vector128 vmul(const Vector128& a, const Vector128& b);
    static Vector128 vmac(const Vector128& acc, const Vector128& a, const Vector128& b);
    static Vector128 vrelu(const Vector128& a);
    static Vector128 vclp(const Vector128& a, int32_t max);
    static int32_t vredmax(const Vector128& a);
    static int32_t vargmax(const Vector128& a);
    static Vector128 vclr();
    static Vector128 vbroadcast(int32_t value);
};
```

## Execution Pipeline (Single-Cycle)

```
┌─────────────────────────────────────────────────────────────────┐
│                     Single Clock Cycle                          │
├───────┬───────┬───────┬───────┬───────────────────────────────┤
│  IF   │  ID   │  EX   │  MEM  │  WB                            │
├───────┼───────┼───────┼───────┼───────────────────────────────┤
│       │       │       │       │                                │
│ Fetch │Decode │Execute│Memory │ Write                          │
│ Instr │ +Read │ ALU/  │Access │ Back                           │
│ from  │ Regs  │ NPU   │(if    │ to                             │
│ Memory│       │       │needed)│ Regs                           │
│       │       │       │       │                                │
└───────┴───────┴───────┴───────┴───────────────────────────────┘
         ▲                                    │
         └────────────────────────────────────┘
                    (next PC)
```

### Stage Details

**IF (Instruction Fetch):**
```cpp
Word instruction = memory.readWord(pc);
```

**ID (Instruction Decode):**
```cpp
DecodedInstr decoded = decoder.decode(instruction);
SWord rs1Val = regs.readScalar(decoded.rs1);
SWord rs2Val = regs.readScalar(decoded.rs2);
Vector128 vs1 = regs.readVector(decoded.rs1);  // If NPU
```

**EX (Execute):**
```cpp
if (decoded.isNPU) {
    result = NPUUnit::execute(decoded.npuOp, vs1, vs2, vd, imm);
} else {
    result = ALU::execute(decoded.aluOp, rs1Val, rs2Val);
}
```

**MEM (Memory Access):**
```cpp
if (decoded.isLoad) {
    memResult = memory.readWord(address);
} else if (decoded.isStore) {
    memory.writeWord(address, rs2Val);
}
```

**WB (Write Back):**
```cpp
if (decoded.writesReg) {
    if (decoded.isNPU && result.writesVector) {
        regs.writeVector(decoded.rd, result.vectorResult);
    } else {
        regs.writeScalar(decoded.rd, result.scalarResult);
    }
}
```

## Control Flow

### Branch Instructions

```cpp
bool taken = ALU::evaluateBranch(branchOp, rs1, rs2);
nextPC = taken ? (pc + imm) : (pc + 4);
```

| Branch | Condition |
|--------|-----------|
| BEQ | rs1 == rs2 |
| BNE | rs1 != rs2 |
| BLT | rs1 < rs2 (signed) |
| BGE | rs1 >= rs2 (signed) |
| BLTU | rs1 < rs2 (unsigned) |
| BGEU | rs1 >= rs2 (unsigned) |

### Jump Instructions

**JAL (Jump and Link):**
```cpp
regs.writeScalar(rd, pc + 4);  // Save return address
nextPC = pc + imm;              // Jump to target
```

**JALR (Jump and Link Register):**
```cpp
regs.writeScalar(rd, pc + 4);
nextPC = (rs1Val + imm) & ~1;   // Clear LSB
```

## Data Types

```cpp
using Word = uint32_t;          // 32-bit unsigned
using SWord = int32_t;          // 32-bit signed
using Address = uint32_t;       // Memory address
using Vector128 = std::array<int32_t, 4>;  // SIMD vector
```

## Extension to Pipelined

The architecture is designed for easy extension:

```cpp
class PipelinedCPU : public CPUBase {
    // Pipeline registers
    struct IF_ID { Word instr; Address pc; };
    struct ID_EX { DecodedInstr decoded; SWord rs1, rs2; Vector128 vs1, vs2; };
    struct EX_MEM { ALUResult aluResult; NPUResult npuResult; };
    struct MEM_WB { SWord memData; bool fromMem; };
    
    IF_ID if_id;
    ID_EX id_ex;
    EX_MEM ex_mem;
    MEM_WB mem_wb;
    
    // Hazard handling
    HazardUnit hazardUnit;
    ForwardingUnit forwardingUnit;
    
    void step() {
        WB();   // Write back first
        MEM();  // Memory access
        EX();   // Execute
        ID();   // Decode
        IF();   // Fetch last (uses new PC)
    }
};
```

## Performance Metrics

The simulator tracks:

```cpp
struct Stats {
    uint64_t cycleCount;
    uint64_t instructionCount;
    uint64_t npuInstructions;
    uint64_t scalarInstructions;
    uint64_t memoryReads;
    uint64_t memoryWrites;
    uint64_t branchesTaken;
    uint64_t branchesNotTaken;
    
    double getCPI() {
        return (double)cycleCount / instructionCount;
    }
};
```

For single-cycle: CPI = 1.0 (by definition)

## Logging System

```cpp
class Logger {
    void logCycle(uint64_t cycle, Address pc, DecodedInstr& instr, RegisterFile& regs);
    void writeCSV(const std::string& filename);
    void writeJSON(const std::string& filename, Stats& stats, RegisterFile& regs);
};
```

Outputs:
- Terminal: Real-time cycle-by-cycle trace
- CSV: Tabular execution trace
- JSON: Summary statistics and final state

