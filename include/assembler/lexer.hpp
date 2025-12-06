#ifndef LEXER_HPP
#define LEXER_HPP

#include "../common.hpp"
#include <regex>
#include <unordered_set>

namespace npu {

// =============================================================================
// Token Types
// =============================================================================

enum class TokenType {
    LABEL,          // label:
    OPCODE,         // ADD, VLOAD, etc.
    REGISTER,       // x0-x31, t0-t6, a0-a7, etc.
    VREG,           // V0-V7
    IMMEDIATE,      // Integer literal
    LPAREN,         // (
    RPAREN,         // )
    COMMA,          // ,
    DIRECTIVE,      // .text, .data, .globl
    IDENTIFIER,     // Label reference
    NEWLINE,        // End of line
    END_OF_FILE,    // End of input
    COMMENT,        // # comment (usually skipped)
    UNKNOWN
};

// =============================================================================
// Token Structure
// =============================================================================

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
    
    Token(TokenType t = TokenType::UNKNOWN, const std::string& v = "", 
          int l = 0, int c = 0)
        : type(t), value(v), line(l), column(c) {}
    
    std::string toString() const {
        static const char* typeNames[] = {
            "LABEL", "OPCODE", "REGISTER", "VREG", "IMMEDIATE",
            "LPAREN", "RPAREN", "COMMA", "DIRECTIVE", "IDENTIFIER",
            "NEWLINE", "EOF", "COMMENT", "UNKNOWN"
        };
        return std::string(typeNames[static_cast<int>(type)]) + "(" + value + ")";
    }
};

// =============================================================================
// Lexer Class
// =============================================================================

class Lexer {
private:
    std::string source_;
    size_t pos_;
    int line_;
    int column_;
    
    // Opcode set for recognition
    static const std::unordered_set<std::string>& getOpcodes();
    
public:
    explicit Lexer(const std::string& source);
    
    // Tokenize entire input
    std::vector<Token> tokenize();
    
    // Get next token
    Token nextToken();
    
    // Peek at current token without consuming
    Token peekToken();
    
private:
    char current() const;
    char peek(size_t ahead = 1) const;
    void advance();
    void skipWhitespace();
    void skipComment();
    
    Token readIdentifierOrOpcode();
    Token readNumber();
    Token readRegister();
    Token readVectorRegister();
    Token readDirective();
    
    bool isAlpha(char c) const;
    bool isDigit(char c) const;
    bool isAlphaNum(char c) const;
};

// =============================================================================
// Helper Functions
// =============================================================================

// Convert register name to number
int parseScalarRegister(const std::string& name);
int parseVectorRegister(const std::string& name);

} // namespace npu

#endif // LEXER_HPP


