#include "assembler/assembler.hpp"
#include <iostream>
#include <cassert>

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

TEST(lexer_basic) {
    Lexer lexer("ADDI x10, x0, 256");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() >= 4);
    ASSERT_EQ(tokens[0].type, TokenType::OPCODE);
    ASSERT_EQ(tokens[0].value, "ADDI");
    ASSERT_EQ(tokens[1].type, TokenType::REGISTER);
}

TEST(lexer_label) {
    Lexer lexer("main:\n    NOP");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens[0].type, TokenType::LABEL);
    ASSERT_EQ(tokens[0].value, "main");
}

TEST(lexer_npu_instruction) {
    Lexer lexer("VLOAD V1, 0(x10)");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens[0].type, TokenType::OPCODE);
    ASSERT_EQ(tokens[0].value, "VLOAD");
    ASSERT_EQ(tokens[1].type, TokenType::VREG);
    ASSERT_EQ(tokens[1].value, "V1");
}

TEST(register_parsing) {
    ASSERT_EQ(parseScalarRegister("x0"), 0);
    ASSERT_EQ(parseScalarRegister("x10"), 10);
    ASSERT_EQ(parseScalarRegister("x31"), 31);
    ASSERT_EQ(parseScalarRegister("zero"), 0);
    ASSERT_EQ(parseScalarRegister("ra"), 1);
    ASSERT_EQ(parseScalarRegister("sp"), 2);
    ASSERT_EQ(parseScalarRegister("t0"), 5);
    ASSERT_EQ(parseScalarRegister("a0"), 10);
    ASSERT_EQ(parseVectorRegister("V0"), 0);
    ASSERT_EQ(parseVectorRegister("V7"), 7);
}

TEST(assembler_addi) {
    Assembler assembler;
    auto result = assembler.assemble("ADDI x10, x0, 256\nHALT");
    
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.machineCode.size(), 2);
    
    // ADDI x10, x0, 256 should encode to 0x10000513
    Word instr = result.machineCode[0];
    ASSERT_EQ(Instruction::getOpcode(instr), OPCODE_OP_IMM);
    ASSERT_EQ(Instruction::getRd(instr), 10);
    ASSERT_EQ(Instruction::getRs1(instr), 0);
}

TEST(assembler_vload) {
    Assembler assembler;
    auto result = assembler.assemble("VLOAD V1, 0(x10)\nHALT");
    
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.machineCode.size(), 2);
    
    Word instr = result.machineCode[0];
    ASSERT_EQ(Instruction::getOpcode(instr), OPCODE_NPU);
}

TEST(assembler_branch_label) {
    Assembler assembler;
    auto result = assembler.assemble(
        "main:\n"
        "    ADDI t0, x0, 5\n"
        "loop:\n"
        "    ADDI t0, t0, -1\n"
        "    BNE t0, x0, loop\n"
        "    HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.machineCode.size(), 4);
    ASSERT_TRUE(result.symbolTable.count("main") > 0);
    ASSERT_TRUE(result.symbolTable.count("loop") > 0);
}

TEST(assembler_full_program) {
    Assembler assembler;
    auto result = assembler.assemble(
        ".text\n"
        "main:\n"
        "    ADDI x10, x0, 256\n"
        "    VLOAD V1, 0(x10)\n"
        "    VLOAD V2, 16(x10)\n"
        "    VADD V3, V1, V2\n"
        "    VSTORE V3, 32(x10)\n"
        "    HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.machineCode.size(), 6);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== Assembler Tests ===\n";
    
    RUN_TEST(lexer_basic);
    RUN_TEST(lexer_label);
    RUN_TEST(lexer_npu_instruction);
    RUN_TEST(register_parsing);
    RUN_TEST(assembler_addi);
    RUN_TEST(assembler_vload);
    RUN_TEST(assembler_branch_label);
    RUN_TEST(assembler_full_program);
    
    std::cout << "\nAll assembler tests passed!\n";
    return 0;
}


