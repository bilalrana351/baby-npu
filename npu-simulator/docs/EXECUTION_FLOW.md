# Execution Flow Guide

This document explains the complete execution flow when you run a command like:

```bash
./bin/npu_sim -v programs/sample/program1_arithmetic.asm
```

## High-Level Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              EXECUTION FLOW                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  1. STARTUP                                                                  │
│     main.cpp ─────────────────────────────────────────────────────────────  │
│         │                                                                    │
│         ▼                                                                    │
│  2. COMMAND LINE PARSING                                                     │
│     utils/config.hpp (ConfigParser::parse)                                   │
│         │                                                                    │
│         ▼                                                                    │
│  3. ASSEMBLY PHASE                                                           │
│     assembler/assembler.cpp ──► assembler/lexer.cpp ──► assembler/parser.cpp │
│         │                                                                    │
│         ▼                                                                    │
│  4. MEMORY INITIALIZATION                                                    │
│     memory.hpp (Memory::loadProgram, Memory::loadData)                       │
│         │                                                                    │
│         ▼                                                                    │
│  5. CPU EXECUTION                                                            │
│     cpu/single_cycle.cpp ──► decoder.cpp ──► alu.cpp / npu_unit.cpp          │
│         │                                                                    │
│         ▼                                                                    │
│  6. LOGGING & OUTPUT                                                         │
│     utils/logger.cpp (writeCSV, writeJSON)                                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Detailed Execution Trace

### Phase 1: Program Entry (`main.cpp`)

**File:** `src/main.cpp`

```cpp
int main(int argc, char* argv[]) {
    // Entry point - receives command line arguments
    // argc = 3, argv = ["./bin/npu_sim", "-v", "programs/sample/program1_arithmetic.asm"]
```

**What happens:**
1. Prints welcome banner
2. Calls `ConfigParser::parse()` to process arguments
3. Orchestrates the entire simulation pipeline

---

### Phase 2: Command Line Parsing (`utils/config.hpp`)

**File:** `include/utils/config.hpp`

```cpp
Config config = ConfigParser::parse(argc, argv);
// Parses: -v flag → config.verbose = true
// Extracts input file: "programs/sample/program1_arithmetic.asm"
```

**Config structure populated:**
```cpp
Config {
    verbose: true,          // -v flag
    traceEnabled: true,     // default
    jsonOutput: true,       // default
    csvOutput: true,        // default
    outputDir: "output",    // default
    memorySize: 65536       // 64KB default
}
```

**Files NOT invoked in this phase:**
- ❌ Memory, Registers, Decoder, ALU, NPU - not needed yet

---

### Phase 3: Assembly Phase

This is the most complex phase with multiple sub-steps.

#### Step 3.1: Read Source File (`assembler/assembler.cpp`)

**File:** `src/assembler/assembler.cpp`

```cpp
AssemblyResult Assembler::assembleFile(const std::string& filename) {
    std::string source = readFile(filename);  // Read .asm file
    return assemble(source);
}
```

#### Step 3.2: Lexical Analysis (`assembler/lexer.cpp`)

**File:** `src/assembler/lexer.cpp`

```cpp
Lexer lexer(source);
std::vector<Token> tokens = lexer.tokenize();
```

**Input:** Raw assembly text
```assembly
ADDI x10, x0, 256
VLOAD V1, 0(x10)
```

**Output:** Token stream
```
[OPCODE("ADDI"), REGISTER("x10"), COMMA, REGISTER("x0"), COMMA, IMMEDIATE("256"), NEWLINE,
 OPCODE("VLOAD"), VREG("V1"), COMMA, IMMEDIATE("0"), LPAREN, REGISTER("x10"), RPAREN, ...]
```

**Key functions called:**
- `Lexer::nextToken()` - Main tokenization loop
- `Lexer::readIdentifierOrOpcode()` - Identifies opcodes and registers
- `Lexer::readNumber()` - Parses numeric literals
- `parseScalarRegister()` - Validates register names (x0-x31, t0-t6, etc.)
- `parseVectorRegister()` - Validates V0-V7

#### Step 3.3: Parsing - Pass 1 (`assembler/parser.cpp`)

**File:** `src/assembler/parser.cpp`

```cpp
Parser parser(tokens);
parser.parse();  // Calls pass1() then pass2()
```

**Pass 1: Build Symbol Table**
```cpp
void Parser::pass1() {
    // Scans for labels, builds: label → address mapping
    // Example: "main:" at address 0x0000
}
```

**Symbol Table after Pass 1:**
```
{
    "main": 0x0000,
    "LOOP_START": 0x0010,
    "END_SIM": 0x002C
}
```

#### Step 3.4: Parsing - Pass 2 (`assembler/parser.cpp`)

**Pass 2: Parse Instructions**
```cpp
void Parser::pass2() {
    // Parses each instruction into ParsedInstruction structs
    // Resolves label references to offsets
}
```

**ParsedInstruction example:**
```cpp
ParsedInstruction {
    opcode: "ADDI",
    operands: ["x10", "x0", "256"],
    address: 0x0000,
    needsLabelResolution: false
}
```

#### Step 3.5: Instruction Encoding (`assembler/parser.cpp`)

**File:** `src/assembler/parser.cpp` (InstructionEncoder class)

```cpp
InstructionEncoder encoder(symbolTable);
Word encoded = encoder.encode(parsedInstr);
```

**Encoding functions called based on instruction type:**

| Instruction | Encoder Function | Machine Code |
|-------------|------------------|--------------|
| `ADDI x10, x0, 256` | `encodeIType()` | `0x10000513` |
| `VLOAD V1, 0(x10)` | `encodeNPU_IType()` | `0x000570f7` |
| `VADD V3, V1, V2` | `encodeNPU_RType()` | `0x002081f7` |
| `BNE t0, x0, loop` | `encodeBType()` | Branch with offset |

**Helper used:** `include/instruction.hpp`
- `Instruction::encodeR()` - R-type encoding
- `Instruction::encodeI()` - I-type encoding
- `Instruction::encodeS()` - S-type encoding
- `Instruction::encodeB()` - B-type encoding
- `Instruction::encodeJ()` - J-type encoding

**Assembly Result:**
```cpp
AssemblyResult {
    machineCode: [0x10000513, 0x20000593, 0x30000613, 0x000570f7, ...],
    symbolTable: {"main": 0x0000},
    success: true
}
```

---

### Phase 4: Memory Initialization

**File:** `include/memory.hpp`

#### Step 4.1: Load Program into Memory

```cpp
Memory memory(config.memorySize);  // Create 64KB memory
memory.loadProgram(asmResult.machineCode);  // Load at 0x0000
```

**Memory layout after loading:**
```
Address    Contents
0x0000     0x10000513  (ADDI x10, x0, 256)
0x0004     0x20000593  (ADDI x11, x0, 512)
0x0008     0x30000613  (ADDI x12, x0, 768)
0x000C     0x000570f7  (VLOAD V1, 0(x10))
...
```

#### Step 4.2: Initialize Test Data

```cpp
initializeTestData(memory);  // Defined in main.cpp
```

**Test vectors loaded:**
```
Address 0x100 (256):  [10, 20, 30, 40]   // Vector A
Address 0x200 (512):  [1, 2, 3, 4]       // Vector B
Address 0x300 (768):  [0, 0, 0, 0]       // Result area
```

---

### Phase 5: CPU Execution

**File:** `src/cpu/single_cycle.cpp`

#### Step 5.1: CPU Initialization

```cpp
Logger logger(programName, config.outputDir, config.csvOutput, config.jsonOutput);
SingleCycleCPU cpu(memory, &logger, config.verbose);
```

**CPU state initialized:**
```cpp
CPUBase {
    pc_: 0x0000,           // Start at text segment
    halted_: false,
    regs_: {
        scalar: [0, 0, 0, ... 0],  // 32 zeros (x0-x31)
        vector: [[0,0,0,0], ...]   // 8 zero vectors (V0-V7)
    },
    stats_: { cycleCount: 0, instructionCount: 0, ... }
}
```

#### Step 5.2: Main Execution Loop

```cpp
void SingleCycleCPU::run() {
    while (!halted_ && stats_.instructionCount < MAX_INSTRUCTIONS) {
        step();  // Execute one instruction
    }
}
```

#### Step 5.3: Single Instruction Execution (`step()`)

For each instruction, the following sequence occurs:

```
┌─────────────────────────────────────────────────────────────┐
│                    ONE CYCLE (step())                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────┐    ┌─────────┐    ┌─────────────────────────┐  │
│  │   IF    │───▶│   ID    │───▶│          EX             │  │
│  │ fetch() │    │decode() │    │ executeRV32I() or       │  │
│  │         │    │         │    │ executeNPU()            │  │
│  └─────────┘    └─────────┘    └─────────────────────────┘  │
│       │              │                    │                  │
│       │              │                    ▼                  │
│       │              │         ┌─────────────────────────┐  │
│       │              │         │   MEM (if load/store)   │  │
│       │              │         │ memoryLoad/memoryStore  │  │
│       │              │         │ vectorLoad/vectorStore  │  │
│       │              │         └─────────────────────────┘  │
│       │              │                    │                  │
│       │              │                    ▼                  │
│       │              │         ┌─────────────────────────┐  │
│       │              │         │          WB             │  │
│       │              │         │ regs_.writeScalar() or  │  │
│       │              │         │ regs_.vector.write()    │  │
│       │              │         └─────────────────────────┘  │
│       │              │                    │                  │
│       ▼              ▼                    ▼                  │
│  memory.hpp     decoder.cpp      alu.cpp / npu_unit.cpp     │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

#### Detailed Trace for `ADDI x10, x0, 256`:

**IF Stage:**
```cpp
Word instruction = fetch();  // memory.readWord(0x0000) → 0x10000513
```

**ID Stage:**
```cpp
DecodedInstr decoded = decode(instruction);
// Calls: decoder.cpp → Decoder::decode(0x10000513)
```

**Decoder output:**
```cpp
DecodedInstr {
    type: I_TYPE,
    opcode: 0x13 (OPCODE_OP_IMM),
    rd: 10,
    rs1: 0,
    imm: 256,
    aluOp: ALUOp::ADD,
    writesReg: true,
    mnemonic: "ADDI"
}
```

**EX Stage:**
```cpp
void SingleCycleCPU::executeRV32I(const DecodedInstr& instr) {
    SWord rs1Val = regs_.readScalar(0);  // → 0
    // Case OPCODE_OP_IMM:
    ALUResult result = ALU::execute(ALUOp::ADD, 0, 256);  // alu.cpp
    // result.result = 256
}
```

**WB Stage:**
```cpp
regs_.writeScalar(10, 256);  // x10 = 256
```

---

#### Detailed Trace for `VLOAD V1, 0(x10)`:

**IF Stage:**
```cpp
Word instruction = fetch();  // → 0x000570f7
```

**ID Stage:**
```cpp
DecodedInstr decoded = decode(instruction);
// Decoder sees opcode 0x77 → NPU instruction
// Calls: decodeNPU()
```

**Decoder output:**
```cpp
DecodedInstr {
    isNPU: true,
    npuOp: NPUOp::VLOAD,
    rd: 1,      // V1
    rs1: 10,    // x10 (base address)
    imm: 0,     // offset
}
```

**EX Stage:**
```cpp
void SingleCycleCPU::executeNPU(const DecodedInstr& instr) {
    case NPUOp::VLOAD: {
        SWord base = regs_.readScalar(10);  // → 256
        Address addr = base + 0;            // → 256 (0x100)
        Vector128 vec = vectorLoad(addr);   // memory.readVector(256)
        // vec = [10, 20, 30, 40]
        regs_.vector.write(1, vec);         // V1 = [10, 20, 30, 40]
    }
}
```

---

#### Detailed Trace for `VMAC V0, V1, V2`:

**EX Stage:**
```cpp
case NPUOp::VMAC: {
    Vector128 vs1 = regs_.vector.read(1);  // [10, 20, 30, 40]
    Vector128 vs2 = regs_.vector.read(2);  // [1, 2, 3, 4]
    Vector128 vd = regs_.vector.read(0);   // Current accumulator
    
    // Calls npu_unit.cpp
    NPUResult result = NPUUnit::execute(NPUOp::VMAC, vs1, vs2, vd, 0);
    // NPUUnit::vmac() performs:
    //   result[i] = vd[i] + vs1[i] * vs2[i]
    
    regs_.vector.write(0, result.vectorResult);
}
```

**NPU Unit calculation:**
```cpp
Vector128 NPUUnit::vmac(const Vector128& acc, const Vector128& a, const Vector128& b) {
    // acc = [0, 0, 0, 0]
    // a = [10, 20, 30, 40]
    // b = [1, 2, 3, 4]
    // result = [0+10*1, 0+20*2, 0+30*3, 0+40*4] = [10, 40, 90, 160]
}
```

---

### Phase 6: Logging & Output

**File:** `src/utils/logger.cpp`

#### Step 6.1: During Execution (if verbose)

```cpp
if (verbose_) {
    logInstruction(decoded);  // Prints cycle-by-cycle trace
}
// Output: [Cycle    1] PC=0x0000 | ADDI a0, x0, 256
```

#### Step 6.2: Log Each Cycle

```cpp
if (logger_) {
    logger_->logCycle(stats_.cycleCount, pc_ - 4, decoded, regs_);
}
```

Stores `CycleLogEntry` for later CSV/JSON output.

#### Step 6.3: Print Final Statistics

```cpp
Logger::printStats(stats);      // Print execution statistics
Logger::printRegisters(regs);   // Print final register state
```

#### Step 6.4: Write Output Files

```cpp
logger.writeCSV();   // → output/traces/trace_YYYY-MM-DD_HH-MM-SS.csv
logger.writeJSON("", stats, cpu.getRegisters());  // → output/logs/run_*.json
```

---

## Files Invoked Summary

### Always Invoked:

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point, orchestration |
| `utils/config.hpp` | Command line parsing |
| `assembler/assembler.cpp` | Assembly orchestration |
| `assembler/lexer.cpp` | Tokenization |
| `assembler/parser.cpp` | Parsing & encoding |
| `instruction.hpp` | Encoding helpers |
| `memory.hpp` | Memory operations |
| `registers.hpp` | Register file |
| `decoder.cpp` | Instruction decoding |
| `cpu/single_cycle.cpp` | Execution engine |
| `utils/logger.cpp` | Output generation |

### Conditionally Invoked:

| File | When Used |
|------|-----------|
| `alu.cpp` | RV32I arithmetic/logic instructions |
| `npu_unit.cpp` | NPU vector instructions |

### Never Directly Invoked at Runtime:

| File | Purpose |
|------|---------|
| `cpu/cpu_base.hpp` | Abstract interface only |
| `common.hpp` | Type definitions (header-only) |
| `tests/*.cpp` | Only during `make test` |

---

## Call Graph for Sample Execution

```
main()
├── ConfigParser::parse()
├── Assembler::assembleFile()
│   ├── readFile()
│   └── assemble()
│       ├── Lexer::tokenize()
│       │   └── nextToken() [repeated]
│       │       ├── readIdentifierOrOpcode()
│       │       ├── readNumber()
│       │       └── parseScalarRegister()
│       ├── Parser::parse()
│       │   ├── pass1() [symbol table]
│       │   └── pass2() [parse instructions]
│       │       └── parseInstruction()
│       │           └── parseOperands()
│       └── InstructionEncoder::encode() [for each instruction]
│           ├── encodeRType() / encodeIType() / encodeBType() ...
│           └── encodeNPU() / encodeNPU_RType() / encodeNPU_IType() ...
├── Memory::loadProgram()
├── initializeTestData()
│   └── Memory::loadData()
├── SingleCycleCPU::run()
│   └── step() [repeated until HALT]
│       ├── fetch()
│       │   └── Memory::readWord()
│       ├── decode()
│       │   └── Decoder::decode()
│       │       ├── decodeNPU() / decodeRType() / decodeIType() ...
│       │       └── getNPUOp() / getALUOpR() / getBranchOp() ...
│       ├── execute()
│       │   ├── executeRV32I()
│       │   │   ├── RegisterFile::readScalar()
│       │   │   ├── ALU::execute()
│       │   │   ├── Memory::readWord() / writeWord()
│       │   │   └── RegisterFile::writeScalar()
│       │   └── executeNPU()
│       │       ├── RegisterFile::readVector()
│       │       ├── Memory::readVector() / writeVector()
│       │       ├── NPUUnit::execute()
│       │       │   └── vadd() / vmul() / vmac() / vrelu() ...
│       │       └── RegisterFile::writeVector()
│       └── Logger::logCycle()
├── Logger::printStats()
├── Logger::printRegisters()
├── Logger::writeCSV()
└── Logger::writeJSON()
```

---

## Timing of Operations

For `program1_arithmetic.asm` with 9 instructions:

```
Time (conceptual)    Operation
─────────────────────────────────────────────
T0                   main() starts
T0-T1                ConfigParser::parse()
T1-T3                Lexer::tokenize() (110 tokens)
T3-T5                Parser::pass1() (build symbol table)
T5-T7                Parser::pass2() (parse instructions)
T7-T9                InstructionEncoder::encode() (9 instructions)
T9-T10               Memory::loadProgram()
T10-T11              initializeTestData()
T11-T12              SingleCycleCPU created
T12-T21              step() × 9 (one per instruction)
  T12                  Cycle 1: ADDI x10, x0, 256
  T13                  Cycle 2: ADDI x11, x0, 512
  T14                  Cycle 3: ADDI x12, x0, 768
  T15                  Cycle 4: VLOAD V1, 0(x10)
  T16                  Cycle 5: VLOAD V2, 0(x11)
  T17                  Cycle 6: VADD V3, V1, V2
  T18                  Cycle 7: VMUL V3, V3, V1
  T19                  Cycle 8: VSTORE V3, 0(x12)
  T20                  Cycle 9: HALT (sets halted_ = true)
T21-T22              Logger::printStats()
T22-T23              Logger::writeCSV()
T23-T24              Logger::writeJSON()
T24                  main() returns 0
```

