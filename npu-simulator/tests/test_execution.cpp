#include "cpu/single_cycle.hpp"
#include "assembler/assembler.hpp"
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

TEST(execute_addi) {
    Memory mem;
    Assembler assembler;
    auto result = assembler.assemble(
        "ADDI x10, x0, 100\n"
        "ADDI x11, x10, 50\n"
        "HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    mem.loadProgram(result.machineCode);
    
    SingleCycleCPU cpu(mem);
    cpu.run();
    
    ASSERT_EQ(cpu.getRegisters().scalar.read(10), 100);
    ASSERT_EQ(cpu.getRegisters().scalar.read(11), 150);
}

TEST(execute_add_sub) {
    Memory mem;
    Assembler assembler;
    auto result = assembler.assemble(
        "ADDI x10, x0, 50\n"
        "ADDI x11, x0, 30\n"
        "ADD x12, x10, x11\n"
        "SUB x13, x10, x11\n"
        "HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    mem.loadProgram(result.machineCode);
    
    SingleCycleCPU cpu(mem);
    cpu.run();
    
    ASSERT_EQ(cpu.getRegisters().scalar.read(12), 80);
    ASSERT_EQ(cpu.getRegisters().scalar.read(13), 20);
}

TEST(execute_load_store) {
    Memory mem;
    Assembler assembler;
    auto result = assembler.assemble(
        "ADDI x10, x0, 1024\n"    // Address
        "ADDI x11, x0, 42\n"      // Value to store
        "SW x11, 0(x10)\n"        // Store 42 at address 1024
        "LW x12, 0(x10)\n"        // Load it back
        "HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    mem.loadProgram(result.machineCode);
    
    SingleCycleCPU cpu(mem);
    cpu.run();
    
    ASSERT_EQ(cpu.getRegisters().scalar.read(12), 42);
}

TEST(execute_branch) {
    Memory mem;
    Assembler assembler;
    auto result = assembler.assemble(
        "    ADDI x10, x0, 3\n"      // Counter = 3
        "    ADDI x11, x0, 0\n"      // Sum = 0
        "loop:\n"
        "    ADD x11, x11, x10\n"    // Sum += Counter
        "    ADDI x10, x10, -1\n"    // Counter--
        "    BNE x10, x0, loop\n"    // if Counter != 0, loop
        "    HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    mem.loadProgram(result.machineCode);
    
    SingleCycleCPU cpu(mem);
    cpu.run();
    
    // Sum should be 3 + 2 + 1 = 6
    ASSERT_EQ(cpu.getRegisters().scalar.read(11), 6);
}

TEST(execute_vload_vstore) {
    Memory mem;
    
    // Put test data in memory
    std::vector<int32_t> testData = {10, 20, 30, 40};
    mem.loadData(testData, 1024);
    
    Assembler assembler;
    auto result = assembler.assemble(
        "ADDI x10, x0, 1024\n"    // Source address
        "ADDI x11, x0, 2048\n"    // Destination address
        "VLOAD V1, 0(x10)\n"      // Load vector
        "VSTORE V1, 0(x11)\n"     // Store to different location
        "HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    mem.loadProgram(result.machineCode);
    
    SingleCycleCPU cpu(mem);
    cpu.run();
    
    // Check the vector was loaded correctly
    const Vector128& v1 = cpu.getRegisters().vector.read(1);
    ASSERT_EQ(v1[0], 10);
    ASSERT_EQ(v1[1], 20);
    ASSERT_EQ(v1[2], 30);
    ASSERT_EQ(v1[3], 40);
    
    // Check it was stored correctly
    Vector128 stored = mem.readVector(2048);
    ASSERT_EQ(stored[0], 10);
    ASSERT_EQ(stored[1], 20);
}

TEST(execute_vadd_vmul) {
    Memory mem;
    
    // Put test vectors in memory
    std::vector<int32_t> vecA = {1, 2, 3, 4};
    std::vector<int32_t> vecB = {10, 10, 10, 10};
    mem.loadData(vecA, 1024);
    mem.loadData(vecB, 1040);
    
    Assembler assembler;
    auto result = assembler.assemble(
        "ADDI x10, x0, 1024\n"
        "VLOAD V1, 0(x10)\n"
        "VLOAD V2, 16(x10)\n"
        "VADD V3, V1, V2\n"       // V3 = [11, 12, 13, 14]
        "VMUL V4, V1, V2\n"       // V4 = [10, 20, 30, 40]
        "HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    mem.loadProgram(result.machineCode);
    
    SingleCycleCPU cpu(mem);
    cpu.run();
    
    const Vector128& v3 = cpu.getRegisters().vector.read(3);
    ASSERT_EQ(v3[0], 11);
    ASSERT_EQ(v3[1], 12);
    ASSERT_EQ(v3[2], 13);
    ASSERT_EQ(v3[3], 14);
    
    const Vector128& v4 = cpu.getRegisters().vector.read(4);
    ASSERT_EQ(v4[0], 10);
    ASSERT_EQ(v4[1], 20);
    ASSERT_EQ(v4[2], 30);
    ASSERT_EQ(v4[3], 40);
}

TEST(execute_vmac) {
    Memory mem;
    
    std::vector<int32_t> vecA = {1, 2, 3, 4};
    std::vector<int32_t> vecB = {2, 2, 2, 2};
    mem.loadData(vecA, 1024);
    mem.loadData(vecB, 1040);
    
    Assembler assembler;
    auto result = assembler.assemble(
        "ADDI x10, x0, 1024\n"
        "VCLR V0\n"               // V0 = [0, 0, 0, 0]
        "VLOAD V1, 0(x10)\n"      // V1 = [1, 2, 3, 4]
        "VLOAD V2, 16(x10)\n"     // V2 = [2, 2, 2, 2]
        "VMAC V0, V1, V2\n"       // V0 = [0,0,0,0] + [1*2, 2*2, 3*2, 4*2] = [2, 4, 6, 8]
        "VMAC V0, V1, V2\n"       // V0 = [2,4,6,8] + [2,4,6,8] = [4, 8, 12, 16]
        "HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    mem.loadProgram(result.machineCode);
    
    SingleCycleCPU cpu(mem);
    cpu.run();
    
    const Vector128& v0 = cpu.getRegisters().vector.read(0);
    ASSERT_EQ(v0[0], 4);
    ASSERT_EQ(v0[1], 8);
    ASSERT_EQ(v0[2], 12);
    ASSERT_EQ(v0[3], 16);
}

TEST(execute_vrelu_vclp) {
    Memory mem;
    
    // Vector with negative values
    std::vector<int32_t> testVec = {-5, 20, -10, 300};
    mem.loadData(testVec, 1024);
    
    Assembler assembler;
    auto result = assembler.assemble(
        "ADDI x10, x0, 1024\n"
        "VLOAD V1, 0(x10)\n"
        "VRELU V2, V1\n"          // V2 = [0, 20, 0, 300]
        "VCLP V3, V2, 255\n"      // V3 = [0, 20, 0, 255]
        "HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    mem.loadProgram(result.machineCode);
    
    SingleCycleCPU cpu(mem);
    cpu.run();
    
    const Vector128& v2 = cpu.getRegisters().vector.read(2);
    ASSERT_EQ(v2[0], 0);
    ASSERT_EQ(v2[1], 20);
    ASSERT_EQ(v2[2], 0);
    ASSERT_EQ(v2[3], 300);
    
    const Vector128& v3 = cpu.getRegisters().vector.read(3);
    ASSERT_EQ(v3[0], 0);
    ASSERT_EQ(v3[1], 20);
    ASSERT_EQ(v3[2], 0);
    ASSERT_EQ(v3[3], 255);
}

TEST(execute_vredmax_vargmax) {
    Memory mem;
    
    std::vector<int32_t> testVec = {10, 50, 30, 20};
    mem.loadData(testVec, 1024);
    
    Assembler assembler;
    auto result = assembler.assemble(
        "ADDI x10, x0, 1024\n"
        "VLOAD V1, 0(x10)\n"
        "VREDMAX x20, V1\n"       // x20 = 50 (max value)
        "VARGMAX x21, V1\n"       // x21 = 1 (index of max)
        "HALT\n"
    );
    
    ASSERT_TRUE(result.success);
    mem.loadProgram(result.machineCode);
    
    SingleCycleCPU cpu(mem);
    cpu.run();
    
    ASSERT_EQ(cpu.getRegisters().scalar.read(20), 50);
    ASSERT_EQ(cpu.getRegisters().scalar.read(21), 1);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== Execution Tests ===\n";
    
    RUN_TEST(execute_addi);
    RUN_TEST(execute_add_sub);
    RUN_TEST(execute_load_store);
    RUN_TEST(execute_branch);
    RUN_TEST(execute_vload_vstore);
    RUN_TEST(execute_vadd_vmul);
    RUN_TEST(execute_vmac);
    RUN_TEST(execute_vrelu_vclp);
    RUN_TEST(execute_vredmax_vargmax);
    
    std::cout << "\nAll execution tests passed!\n";
    return 0;
}


