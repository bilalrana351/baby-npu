#ifndef ASSEMBLER_HPP
#define ASSEMBLER_HPP

#include "../common.hpp"
#include "lexer.hpp"
#include "parser.hpp"

namespace npu {

// =============================================================================
// Assembly Result
// =============================================================================

struct AssemblyResult {
    std::vector<Word> machineCode;
    std::map<std::string, Address> symbolTable;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    bool success;
    
    AssemblyResult() : success(false) {}
};

// =============================================================================
// Assembler Class
// =============================================================================

class Assembler {
private:
    bool verbose_;
    
public:
    explicit Assembler(bool verbose = false) : verbose_(verbose) {}
    
    // Assemble from source string
    AssemblyResult assemble(const std::string& source);
    
    // Assemble from file
    AssemblyResult assembleFile(const std::string& filename);
    
    // Get disassembly of machine code
    static std::string disassemble(Word instruction, Address addr = 0);
    
    // Print assembled program
    static void printProgram(const AssemblyResult& result, std::ostream& os = std::cout);
    
private:
    std::string readFile(const std::string& filename);
};

// =============================================================================
// Disassembler (for debugging)
// =============================================================================

class Disassembler {
public:
    static std::string disassemble(Word instr, Address addr = 0);
    static std::string disassembleNPU(Word instr);
    static std::string disassembleRV32I(Word instr);
};

} // namespace npu

#endif // ASSEMBLER_HPP


