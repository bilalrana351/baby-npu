#ifndef PARSER_HPP
#define PARSER_HPP

#include "../common.hpp"
#include "../instruction.hpp"
#include "lexer.hpp"

namespace npu {

// =============================================================================
// Parsed Instruction (before encoding)
// =============================================================================

struct ParsedInstruction {
    std::string opcode;
    std::vector<std::string> operands;
    int line;
    Address address;
    
    // For branch/jump instructions with label targets
    std::string targetLabel;
    bool needsLabelResolution;
    
    ParsedInstruction() : line(0), address(0), needsLabelResolution(false) {}
};

// =============================================================================
// Parser Class (Two-Pass)
// =============================================================================

class Parser {
private:
    std::vector<Token> tokens_;
    size_t pos_;
    
    // Symbol table: label -> address
    std::map<std::string, Address> symbolTable_;
    
    // Parsed instructions
    std::vector<ParsedInstruction> instructions_;
    
    // Current address during parsing
    Address currentAddress_;
    
public:
    explicit Parser(const std::vector<Token>& tokens);
    
    // Two-pass parsing
    void parse();
    
    // Get results
    const std::map<std::string, Address>& getSymbolTable() const { return symbolTable_; }
    const std::vector<ParsedInstruction>& getInstructions() const { return instructions_; }
    
private:
    // Token handling
    Token current() const;
    Token peek(size_t ahead = 1) const;
    void advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    void expect(TokenType type, const std::string& msg);
    void skipNewlines();
    
    // Pass 1: Build symbol table
    void pass1();
    
    // Pass 2: Parse instructions
    void pass2();
    
    // Parse a single instruction
    ParsedInstruction parseInstruction();
    
    // Parse operands based on instruction format
    void parseOperands(ParsedInstruction& instr);
    
    // Handle directives
    void handleDirective();
    
    // Error reporting
    void error(const std::string& msg) const;
};

// =============================================================================
// Instruction Encoder
// =============================================================================

class InstructionEncoder {
private:
    const std::map<std::string, Address>& symbolTable_;
    
public:
    explicit InstructionEncoder(const std::map<std::string, Address>& symTable)
        : symbolTable_(symTable) {}
    
    // Encode a parsed instruction to machine code
    Word encode(const ParsedInstruction& instr);
    
private:
    // RV32I encoding
    Word encodeRType(const ParsedInstruction& instr);
    Word encodeIType(const ParsedInstruction& instr);
    Word encodeSType(const ParsedInstruction& instr);
    Word encodeBType(const ParsedInstruction& instr);
    Word encodeUType(const ParsedInstruction& instr);
    Word encodeJType(const ParsedInstruction& instr);
    
    // NPU encoding
    Word encodeNPU(const ParsedInstruction& instr);
    Word encodeNPU_RType(const ParsedInstruction& instr);
    Word encodeNPU_IType(const ParsedInstruction& instr);
    Word encodeNPU_SType(const ParsedInstruction& instr);
    
    // Pseudo instruction expansion
    Word encodePseudo(const ParsedInstruction& instr);
    
    // Helper functions
    uint8_t parseReg(const std::string& reg) const;
    uint8_t parseVReg(const std::string& reg) const;
    SWord parseImmediate(const std::string& imm) const;
    SWord resolveLabelOffset(const std::string& label, Address currentAddr) const;
    
    // Parse memory operand like "offset(base)" returns {offset, base_reg}
    std::pair<SWord, uint8_t> parseMemoryOperand(const std::string& operand) const;
};

} // namespace npu

#endif // PARSER_HPP


