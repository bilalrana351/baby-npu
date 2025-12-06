#include "decoder.hpp"
#include "instruction.hpp"
#include <iostream>

using namespace npu;

// Simple test framework
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    test_##name(); \
    std::cout << "PASSED\n"; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::cerr << "\nAssertion failed: " << #a << " != " << #b << "\n"; \
        std::cerr << "  Got: " << (a) << "\n"; \
        std::cerr << "  Expected: " << (b) << "\n"; \
        exit(1); \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "\nAssertion failed: " << #cond << "\n"; \
        exit(1); \
    } \
} while(0)

// =============================================================================
// Tests
// =============================================================================

TEST(decode_add) {
    Decoder decoder;
    // ADD x10, x5, x6 
    Word instr = Instruction::encodeR(OPCODE_OP, 10, 0b000, 5, 6, 0b0000000);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_EQ(decoded.type, InstrType::R_TYPE);
    ASSERT_EQ(decoded.rd, 10);
    ASSERT_EQ(decoded.rs1, 5);
    ASSERT_EQ(decoded.rs2, 6);
    ASSERT_EQ(decoded.aluOp, ALUOp::ADD);
}

TEST(decode_sub) {
    Decoder decoder;
    // SUB x10, x5, x6
    Word instr = Instruction::encodeR(OPCODE_OP, 10, 0b000, 5, 6, 0b0100000);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_EQ(decoded.aluOp, ALUOp::SUB);
}

TEST(decode_addi) {
    Decoder decoder;
    // ADDI x10, x0, 256
    Word instr = Instruction::encodeI(OPCODE_OP_IMM, 10, 0b000, 0, 256);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_EQ(decoded.type, InstrType::I_TYPE);
    ASSERT_EQ(decoded.rd, 10);
    ASSERT_EQ(decoded.rs1, 0);
    ASSERT_EQ(decoded.imm, 256);
    ASSERT_EQ(decoded.aluOp, ALUOp::ADD);
}

TEST(decode_lw) {
    Decoder decoder;
    // LW x10, 4(x5)
    Word instr = Instruction::encodeI(OPCODE_LOAD, 10, 0b010, 5, 4);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_TRUE(decoded.isLoad);
    ASSERT_EQ(decoded.rd, 10);
    ASSERT_EQ(decoded.rs1, 5);
    ASSERT_EQ(decoded.imm, 4);
}

TEST(decode_sw) {
    Decoder decoder;
    // SW x6, 8(x5)
    Word instr = Instruction::encodeS(OPCODE_STORE, 0b010, 5, 6, 8);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_TRUE(decoded.isStore);
    ASSERT_EQ(decoded.rs1, 5);
    ASSERT_EQ(decoded.rs2, 6);
    ASSERT_EQ(decoded.imm, 8);
}

TEST(decode_beq) {
    Decoder decoder;
    // BEQ x5, x6, 16
    Word instr = Instruction::encodeB(OPCODE_BRANCH, 0b000, 5, 6, 16);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_TRUE(decoded.isBranch);
    ASSERT_EQ(decoded.branchOp, BranchOp::BEQ);
    ASSERT_EQ(decoded.rs1, 5);
    ASSERT_EQ(decoded.rs2, 6);
}

TEST(decode_jal) {
    Decoder decoder;
    // JAL x1, 100
    Word instr = Instruction::encodeJ(OPCODE_JAL, 1, 100);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_TRUE(decoded.isJump);
    ASSERT_EQ(decoded.rd, 1);
}

TEST(decode_npu_vadd) {
    Decoder decoder;
    // VADD V3, V1, V2
    Word instr = Instruction::encodeNPU_R(NPUFunct3::ARITH, NPUFunct7::DEFAULT, 3, 1, 2);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_TRUE(decoded.isNPU);
    ASSERT_EQ(decoded.npuOp, NPUOp::VADD);
    ASSERT_EQ(decoded.rd, 3);
    ASSERT_EQ(decoded.rs1, 1);
    ASSERT_EQ(decoded.rs2, 2);
}

TEST(decode_npu_vmac) {
    Decoder decoder;
    // VMAC V0, V1, V2
    Word instr = Instruction::encodeNPU_R(NPUFunct3::MAC, NPUFunct7::DEFAULT, 0, 1, 2);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_TRUE(decoded.isNPU);
    ASSERT_EQ(decoded.npuOp, NPUOp::VMAC);
}

TEST(decode_npu_vclr) {
    Decoder decoder;
    // VCLR V0
    Word instr = Instruction::encodeNPU_R(NPUFunct3::ARITH, NPUFunct7::VCLR, 0, 0, 0);
    DecodedInstr decoded = decoder.decode(instr);
    
    ASSERT_TRUE(decoded.isNPU);
    ASSERT_EQ(decoded.npuOp, NPUOp::VCLR);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== Decoder Tests ===\n";
    
    RUN_TEST(decode_add);
    RUN_TEST(decode_sub);
    RUN_TEST(decode_addi);
    RUN_TEST(decode_lw);
    RUN_TEST(decode_sw);
    RUN_TEST(decode_beq);
    RUN_TEST(decode_jal);
    RUN_TEST(decode_npu_vadd);
    RUN_TEST(decode_npu_vmac);
    RUN_TEST(decode_npu_vclr);
    
    std::cout << "\nAll decoder tests passed!\n";
    return 0;
}


