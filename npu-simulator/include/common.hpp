#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint>
#include <string>
#include <array>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace npu {

// =============================================================================
// Type Definitions
// =============================================================================

using Word = uint32_t;          // 32-bit word
using SWord = int32_t;          // Signed 32-bit word
using Byte = uint8_t;           // 8-bit byte
using Address = uint32_t;       // Memory address

// Vector type: 4 x 32-bit integers (128 bits total)
using Vector128 = std::array<int32_t, 4>;

// =============================================================================
// Constants
// =============================================================================

// Memory configuration
constexpr size_t DEFAULT_MEMORY_SIZE = 64 * 1024;  // 64 KB
constexpr Address TEXT_SEGMENT_START = 0x0000;
constexpr Address DATA_SEGMENT_START = 0x2000;     // 8 KB offset
constexpr Address STACK_START = 0xFFFC;            // Top of 64KB

// Register counts
constexpr size_t NUM_SCALAR_REGS = 32;  // x0-x31
constexpr size_t NUM_VECTOR_REGS = 8;   // V0-V7
constexpr size_t VECTOR_LANES = 4;      // 4 elements per vector

// Opcodes
constexpr Word OPCODE_NPU      = 0x77;  // 1110111 - Custom NPU instructions
constexpr Word OPCODE_LOAD     = 0x03;  // 0000011 - LW
constexpr Word OPCODE_STORE    = 0x23;  // 0100011 - SW
constexpr Word OPCODE_OP       = 0x33;  // 0110011 - R-type (ADD, SUB, etc.)
constexpr Word OPCODE_OP_IMM   = 0x13;  // 0010011 - I-type (ADDI, etc.)
constexpr Word OPCODE_BRANCH   = 0x63;  // 1100011 - Branch
constexpr Word OPCODE_JAL      = 0x6F;  // 1101111 - JAL
constexpr Word OPCODE_JALR     = 0x67;  // 1100111 - JALR
constexpr Word OPCODE_LUI      = 0x37;  // 0110111 - LUI
constexpr Word OPCODE_AUIPC    = 0x17;  // 0010111 - AUIPC
constexpr Word OPCODE_HALT     = 0x7F;  // Custom halt instruction

// =============================================================================
// Instruction Types
// =============================================================================

enum class InstrType {
    R_TYPE,     // Register-Register operations
    I_TYPE,     // Immediate operations
    S_TYPE,     // Store operations
    B_TYPE,     // Branch operations
    U_TYPE,     // Upper immediate
    J_TYPE,     // Jump operations
    UNKNOWN
};

// =============================================================================
// NPU Operation Types
// =============================================================================

enum class NPUOp {
    // Memory operations (funct3 = 111)
    VLOAD,      // Vector load (128-bit)
    VSTORE,     // Vector store (128-bit)
    VLBC,       // Vector load broadcast
    
    // Arithmetic operations
    VADD,       // Vector add (funct3 = 000, funct7 = 0000000)
    VMUL,       // Vector multiply (funct3 = 010, funct7 = 0000000)
    VMAC,       // Vector multiply-accumulate (funct3 = 011, funct7 = 0000000)
    
    // Activation/Clipping
    VRELU,      // ReLU activation (funct3 = 100, funct7 = 0000000)
    VCLP,       // Clamp/clip (funct3 = 101)
    
    // Reduction operations
    VREDMAX,    // Reduce max (funct3 = 110, funct7 = 0000000)
    VARGMAX,    // Argmax (funct3 = 110, funct7 = 0000001)
    
    // Utility
    VCLR,       // Clear vector (funct3 = 000, funct7 = 0000001)
    
    NONE        // Not an NPU operation
};

// =============================================================================
// ALU Operation Types (for RV32I)
// =============================================================================

enum class ALUOp {
    ADD,
    SUB,
    AND,
    OR,
    XOR,
    SLT,
    SLTU,
    SLL,
    SRL,
    SRA,
    NONE
};

// =============================================================================
// Branch Types
// =============================================================================

enum class BranchOp {
    BEQ,
    BNE,
    BLT,
    BGE,
    BLTU,
    BGEU,
    NONE
};

// =============================================================================
// Decoded Instruction Structure
// =============================================================================

struct DecodedInstr {
    Word raw;               // Raw 32-bit instruction
    InstrType type;         // Instruction format type
    Word opcode;            // 7-bit opcode
    uint8_t rd;             // Destination register
    uint8_t rs1;            // Source register 1
    uint8_t rs2;            // Source register 2
    uint8_t funct3;         // 3-bit function code
    uint8_t funct7;         // 7-bit function code
    SWord imm;              // Immediate value (sign-extended)
    
    bool isNPU;             // True if NPU instruction
    NPUOp npuOp;            // NPU operation type
    ALUOp aluOp;            // ALU operation type
    BranchOp branchOp;      // Branch operation type
    
    bool isLoad;            // Is memory load
    bool isStore;           // Is memory store
    bool isBranch;          // Is branch instruction
    bool isJump;            // Is jump instruction (JAL/JALR)
    bool writesReg;         // Writes to register file
    bool isHalt;            // Is halt instruction
    
    std::string mnemonic;   // Instruction mnemonic for logging
    
    DecodedInstr() : raw(0), type(InstrType::UNKNOWN), opcode(0),
                     rd(0), rs1(0), rs2(0), funct3(0), funct7(0), imm(0),
                     isNPU(false), npuOp(NPUOp::NONE), aluOp(ALUOp::NONE),
                     branchOp(BranchOp::NONE), isLoad(false), isStore(false),
                     isBranch(false), isJump(false), writesReg(false),
                     isHalt(false), mnemonic("UNKNOWN") {}
};

// =============================================================================
// Timing Constants (in nanoseconds)
// =============================================================================

// Realistic clock periods based on typical ASIC implementations
constexpr double SINGLE_CYCLE_CLOCK_PERIOD_NS = 10.0;  // 100 MHz (long critical path)
constexpr double PIPELINED_CLOCK_PERIOD_NS = 2.0;      // 500 MHz (shorter stages)

// Alternative: More aggressive values
// constexpr double SINGLE_CYCLE_CLOCK_PERIOD_NS = 15.0;  // 67 MHz
// constexpr double PIPELINED_CLOCK_PERIOD_NS = 1.5;      // 667 MHz

// =============================================================================
// Execution Statistics
// =============================================================================

struct Stats {
    uint64_t cycleCount;
    uint64_t instructionCount;
    uint64_t npuInstructions;
    uint64_t scalarInstructions;
    uint64_t memoryReads;
    uint64_t memoryWrites;
    uint64_t branchesTaken;
    uint64_t branchesNotTaken;
    
    // Timing information
    bool isPipelined;
    double clockPeriodNs;
    
    Stats() : cycleCount(0), instructionCount(0), npuInstructions(0),
              scalarInstructions(0), memoryReads(0), memoryWrites(0),
              branchesTaken(0), branchesNotTaken(0),
              isPipelined(false), clockPeriodNs(SINGLE_CYCLE_CLOCK_PERIOD_NS) {}
    
    double getCPI() const {
        return instructionCount > 0 ? 
               static_cast<double>(cycleCount) / instructionCount : 0.0;
    }
    
    // Calculate total execution time in nanoseconds
    double getExecutionTimeNs() const {
        return cycleCount * clockPeriodNs;
    }
    
    // Calculate execution time in microseconds
    double getExecutionTimeUs() const {
        return getExecutionTimeNs() / 1000.0;
    }
    
    // Calculate throughput (MIPS = Million Instructions Per Second)
    double getMIPS() const {
        double timeSeconds = getExecutionTimeNs() / 1e9;
        return timeSeconds > 0 ? (instructionCount / 1e6) / timeSeconds : 0.0;
    }
    
    // Calculate clock frequency in MHz
    double getClockFrequencyMHz() const {
        return 1000.0 / clockPeriodNs;
    }
};

// =============================================================================
// Configuration
// =============================================================================

struct Config {
    bool verbose;           // Verbose output
    bool traceEnabled;      // Enable cycle-by-cycle trace
    bool jsonOutput;        // Output JSON log
    bool csvOutput;         // Output CSV trace
    bool pipelined;         // Use pipelined CPU (default: single-cycle)
    std::string outputDir;  // Output directory
    size_t memorySize;      // Memory size in bytes
    
    Config() : verbose(false), traceEnabled(true), jsonOutput(true),
               csvOutput(true), pipelined(false), outputDir("output"), 
               memorySize(DEFAULT_MEMORY_SIZE) {}
};

// =============================================================================
// Helper Functions
// =============================================================================

// Convert vector to string for logging
inline std::string vectorToString(const Vector128& vec) {
    std::ostringstream oss;
    oss << "[" << vec[0] << ", " << vec[1] << ", " << vec[2] << ", " << vec[3] << "]";
    return oss.str();
}

// Convert address to hex string
inline std::string addrToHex(Address addr) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(4) << addr;
    return oss.str();
}

// Convert word to hex string
inline std::string wordToHex(Word word) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(8) << word;
    return oss.str();
}

// Get scalar register name
inline std::string scalarRegName(uint8_t reg) {
    static const char* names[] = {
        "x0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
    };
    return reg < 32 ? names[reg] : "x?";
}

// Get vector register name
inline std::string vectorRegName(uint8_t reg) {
    return "V" + std::to_string(reg);
}

// Sign extend from bit position
inline SWord signExtend(Word value, int bits) {
    int shift = 32 - bits;
    return static_cast<SWord>(value << shift) >> shift;
}

// Extract bits from instruction
inline Word extractBits(Word instr, int high, int low) {
    return (instr >> low) & ((1U << (high - low + 1)) - 1);
}

} // namespace npu

#endif // COMMON_HPP


