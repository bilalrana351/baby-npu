#include "../../include/assembler/lexer.hpp"
#include <cctype>
#include <unordered_set>

namespace npu {

// =============================================================================
// Opcode Set
// =============================================================================

const std::unordered_set<std::string> &Lexer::getOpcodes() {
  static const std::unordered_set<std::string> opcodes = {
      // RV32I Arithmetic
      "ADD", "SUB", "ADDI", "AND", "OR", "XOR", "ANDI", "ORI", "XORI", "SLL",
      "SRL", "SRA", "SLLI", "SRLI", "SRAI", "SLT", "SLTU", "SLTI", "SLTIU",

      // RV32I Memory
      "LW", "SW",

      // RV32I Branches
      "BEQ", "BNE", "BLT", "BGE", "BLTU", "BGEU",

      // RV32I Jumps
      "JAL", "JALR",

      // RV32I Upper Immediate
      "LUI", "AUIPC",

      // Pseudo instructions
      "NOP", "J", "MV", "LI", "LA", "CALL", "RET", "HALT",

      // NPU Instructions
      "VLOAD", "VSTORE", "VLBC", "VADD", "VMUL", "VMAC", "VRELU", "VCLP",
      "VREDMAX", "VARGMAX", "VCLR"};
  return opcodes;
}

// =============================================================================
// Lexer Implementation
// =============================================================================

Lexer::Lexer(const std::string &source)
    : source_(source), pos_(0), line_(1), column_(1) {}

char Lexer::current() const {
  return pos_ < source_.size() ? source_[pos_] : '\0';
}

char Lexer::peek(size_t ahead) const {
  size_t idx = pos_ + ahead;
  return idx < source_.size() ? source_[idx] : '\0';
}

void Lexer::advance() {
  if (pos_ < source_.size()) {
    if (source_[pos_] == '\n') {
      line_++;
      column_ = 1;
    } else {
      column_++;
    }
    pos_++;
  }
}

void Lexer::skipWhitespace() {
  while (current() == ' ' || current() == '\t' || current() == '\r') {
    advance();
  }
}

void Lexer::skipComment() {
  if (current() == '#') {
    while (current() != '\n' && current() != '\0') {
      advance();
    }
  }
}

bool Lexer::isAlpha(char c) const {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::isDigit(char c) const { return c >= '0' && c <= '9'; }

bool Lexer::isAlphaNum(char c) const { return isAlpha(c) || isDigit(c); }

Token Lexer::readNumber() {
  int startLine = line_;
  int startCol = column_;
  std::string num;

  bool negative = false;
  if (current() == '-') {
    negative = true;
    num += current();
    advance();
  }

  // Check for hex
  if (current() == '0' && (peek() == 'x' || peek() == 'X')) {
    num += current();
    advance();
    num += current();
    advance();
    while (isDigit(current()) || (current() >= 'a' && current() <= 'f') ||
           (current() >= 'A' && current() <= 'F')) {
      num += current();
      advance();
    }
  } else {
    while (isDigit(current())) {
      num += current();
      advance();
    }
  }

  return Token(TokenType::IMMEDIATE, num, startLine, startCol);
}

Token Lexer::readIdentifierOrOpcode() {
  int startLine = line_;
  int startCol = column_;
  std::string ident;

  while (isAlphaNum(current())) {
    ident += current();
    advance();
  }

  // Check if it's a label definition (followed by :)
  skipWhitespace();
  if (current() == ':') {
    advance();
    return Token(TokenType::LABEL, ident, startLine, startCol);
  }

  // Convert to uppercase for opcode matching
  std::string upper = ident;
  for (char &c : upper)
    c = std::toupper(c);

  // Check if it's an opcode
  if (getOpcodes().count(upper)) {
    return Token(TokenType::OPCODE, upper, startLine, startCol);
  }

  // Check for scalar register names
  int regNum = parseScalarRegister(ident);
  if (regNum >= 0) {
    return Token(TokenType::REGISTER, ident, startLine, startCol);
  }

  // Check for vector register (V0-V7)
  if ((ident[0] == 'V' || ident[0] == 'v') && ident.size() == 2 &&
      ident[1] >= '0' && ident[1] <= '7') {
    return Token(TokenType::VREG, ident, startLine, startCol);
  }

  // Otherwise it's an identifier (label reference)
  return Token(TokenType::IDENTIFIER, ident, startLine, startCol);
}

Token Lexer::readDirective() {
  int startLine = line_;
  int startCol = column_;
  std::string dir;

  advance(); // skip '.'
  while (isAlpha(current())) {
    dir += current();
    advance();
  }

  return Token(TokenType::DIRECTIVE, dir, startLine, startCol);
}

Token Lexer::nextToken() {
  skipWhitespace();

  if (current() == '\0') {
    return Token(TokenType::END_OF_FILE, "", line_, column_);
  }

  if (current() == '#') {
    skipComment();
    skipWhitespace();
    if (current() == '\n') {
      advance();
      return Token(TokenType::NEWLINE, "\\n", line_ - 1, column_);
    }
    return nextToken();
  }

  if (current() == '\n') {
    advance();
    return Token(TokenType::NEWLINE, "\\n", line_ - 1, column_);
  }

  if (current() == '.') {
    return readDirective();
  }

  if (current() == '(') {
    int l = line_, c = column_;
    advance();
    return Token(TokenType::LPAREN, "(", l, c);
  }

  if (current() == ')') {
    int l = line_, c = column_;
    advance();
    return Token(TokenType::RPAREN, ")", l, c);
  }

  if (current() == ',') {
    int l = line_, c = column_;
    advance();
    return Token(TokenType::COMMA, ",", l, c);
  }

  if (current() == '-' && isDigit(peek())) {
    return readNumber();
  }

  if (isDigit(current())) {
    return readNumber();
  }

  if (isAlpha(current())) {
    return readIdentifierOrOpcode();
  }

  // Unknown character
  int l = line_, c = column_;
  char ch = current();
  advance();
  return Token(TokenType::UNKNOWN, std::string(1, ch), l, c);
}

Token Lexer::peekToken() {
  size_t savedPos = pos_;
  int savedLine = line_;
  int savedCol = column_;

  Token tok = nextToken();

  pos_ = savedPos;
  line_ = savedLine;
  column_ = savedCol;

  return tok;
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  Token tok;

  do {
    tok = nextToken();
    if (tok.type != TokenType::COMMENT) {
      tokens.push_back(tok);
    }
  } while (tok.type != TokenType::END_OF_FILE);

  return tokens;
}

// =============================================================================
// Register Parsing
// =============================================================================

int parseScalarRegister(const std::string &name) {
  // ABI names
  static const std::unordered_map<std::string, int> abiNames = {
      {"zero", 0}, {"ra", 1},  {"sp", 2},  {"gp", 3},   {"tp", 4},   {"t0", 5},
      {"t1", 6},   {"t2", 7},  {"s0", 8},  {"fp", 8},   {"s1", 9},   {"a0", 10},
      {"a1", 11},  {"a2", 12}, {"a3", 13}, {"a4", 14},  {"a5", 15},  {"a6", 16},
      {"a7", 17},  {"s2", 18}, {"s3", 19}, {"s4", 20},  {"s5", 21},  {"s6", 22},
      {"s7", 23},  {"s8", 24}, {"s9", 25}, {"s10", 26}, {"s11", 27}, {"t3", 28},
      {"t4", 29},  {"t5", 30}, {"t6", 31}};

  // Check ABI names (case insensitive)
  std::string lower = name;
  for (char &c : lower)
    c = std::tolower(c);

  auto it = abiNames.find(lower);
  if (it != abiNames.end()) {
    return it->second;
  }

  // Check xN format
  if ((name[0] == 'x' || name[0] == 'X') && name.size() > 1) {
    try {
      int num = std::stoi(name.substr(1));
      if (num >= 0 && num < 32) {
        return num;
      }
    } catch (...) {
    }
  }

  return -1; // Not a valid register
}

int parseVectorRegister(const std::string &name) {
  if ((name[0] == 'V' || name[0] == 'v') && name.size() == 2) {
    int num = name[1] - '0';
    if (num >= 0 && num < 8) {
      return num;
    }
  }
  return -1;
}

} // namespace npu
