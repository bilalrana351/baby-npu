#include "../../include/assembler/parser.hpp"
#include <iostream>
#include <sstream>

namespace npu {

// =============================================================================
// Parser Implementation
// =============================================================================

Parser::Parser(const std::vector<Token> &tokens)
    : tokens_(tokens), pos_(0), currentAddress_(TEXT_SEGMENT_START) {}

Token Parser::current() const {
  return pos_ < tokens_.size() ? tokens_[pos_] : Token(TokenType::END_OF_FILE);
}

Token Parser::peek(size_t ahead) const {
  size_t idx = pos_ + ahead;
  return idx < tokens_.size() ? tokens_[idx] : Token(TokenType::END_OF_FILE);
}

void Parser::advance() {
  if (pos_ < tokens_.size()) {
    pos_++;
  }
}

bool Parser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

bool Parser::check(TokenType type) const { return current().type == type; }

void Parser::expect(TokenType type, const std::string &msg) {
  if (!match(type)) {
    error(msg + " at line " + std::to_string(current().line));
  }
}

void Parser::skipNewlines() {
  while (check(TokenType::NEWLINE)) {
    advance();
  }
}

void Parser::error(const std::string &msg) const {
  throw std::runtime_error("Parser error: " + msg);
}

void Parser::parse() {
  pass1();
  pos_ = 0;
  currentAddress_ = TEXT_SEGMENT_START;
  pass2();
}

// Pass 1: Build symbol table (collect labels)
void Parser::pass1() {
  while (!check(TokenType::END_OF_FILE)) {
    skipNewlines();

    if (check(TokenType::LABEL)) {
      symbolTable_[current().value] = currentAddress_;
      advance();
    } else if (check(TokenType::DIRECTIVE)) {
      // Skip directives for now
      while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
        advance();
      }
    } else if (check(TokenType::OPCODE)) {
      // Count instruction size (4 bytes each)
      currentAddress_ += 4;
      while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
        advance();
      }
    } else if (check(TokenType::END_OF_FILE)) {
      break;
    } else {
      advance();
    }
  }
}

// Pass 2: Parse and collect instructions
void Parser::pass2() {
  while (!check(TokenType::END_OF_FILE)) {
    skipNewlines();

    if (check(TokenType::LABEL)) {
      advance(); // Skip label (already in symbol table)
      skipNewlines();
    }

    if (check(TokenType::DIRECTIVE)) {
      handleDirective();
    } else if (check(TokenType::OPCODE)) {
      ParsedInstruction instr = parseInstruction();
      instr.address = currentAddress_;
      instructions_.push_back(instr);
      currentAddress_ += 4;
    } else if (check(TokenType::END_OF_FILE)) {
      break;
    } else {
      advance();
    }
  }
}

void Parser::handleDirective() {
  std::string dir = current().value;
  advance();

  // Skip to end of line
  while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
    advance();
  }
}

ParsedInstruction Parser::parseInstruction() {
  ParsedInstruction instr;
  instr.line = current().line;
  instr.opcode = current().value;
  advance();

  parseOperands(instr);

  return instr;
}

void Parser::parseOperands(ParsedInstruction &instr) {
  while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE)) {
    if (check(TokenType::COMMA)) {
      advance();
      continue;
    }

    if (check(TokenType::REGISTER) || check(TokenType::VREG)) {
      instr.operands.push_back(current().value);
      advance();
    } else if (check(TokenType::IMMEDIATE)) {
      // Check for memory operand: imm(reg)
      std::string imm = current().value;
      advance();

      if (check(TokenType::LPAREN)) {
        advance();
        if (check(TokenType::REGISTER)) {
          std::string reg = current().value;
          advance();
          expect(TokenType::RPAREN, "Expected ')'");
          instr.operands.push_back(imm + "(" + reg + ")");
        } else {
          error("Expected register in memory operand");
        }
      } else {
        instr.operands.push_back(imm);
      }
    } else if (check(TokenType::IDENTIFIER)) {
      // Label reference (for branches/jumps)
      instr.targetLabel = current().value;
      instr.needsLabelResolution = true;
      instr.operands.push_back(current().value);
      advance();
    } else if (check(TokenType::LPAREN)) {
      // Memory operand with implicit 0 offset: (reg)
      advance();
      if (check(TokenType::REGISTER)) {
        std::string reg = current().value;
        advance();
        expect(TokenType::RPAREN, "Expected ')'");
        instr.operands.push_back("0(" + reg + ")");
      } else {
        error("Expected register in memory operand");
      }
    } else {
      break;
    }
  }
}

// =============================================================================
// Instruction Encoder Implementation
// =============================================================================

Word InstructionEncoder::encode(const ParsedInstruction &instr) {
  const std::string &op = instr.opcode;

  // NPU Instructions
  if (op == "VLOAD" || op == "VSTORE" || op == "VLBC" || op == "VADD" ||
      op == "VMUL" || op == "VMAC" || op == "VRELU" || op == "VCLP" ||
      op == "VREDMAX" || op == "VARGMAX" || op == "VCLR") {
    return encodeNPU(instr);
  }

  // Pseudo instructions
  if (op == "NOP" || op == "J" || op == "MV" || op == "LI" || op == "RET" ||
      op == "CALL" || op == "HALT") {
    return encodePseudo(instr);
  }

  // R-type instructions
  if (op == "ADD" || op == "SUB" || op == "AND" || op == "OR" || op == "XOR" ||
      op == "SLL" || op == "SRL" || op == "SRA" || op == "SLT" ||
      op == "SLTU") {
    return encodeRType(instr);
  }

  // I-type instructions
  if (op == "ADDI" || op == "ANDI" || op == "ORI" || op == "XORI" ||
      op == "SLTI" || op == "SLTIU" || op == "SLLI" || op == "SRLI" ||
      op == "SRAI" || op == "JALR" || op == "LW") {
    return encodeIType(instr);
  }

  // S-type instructions
  if (op == "SW") {
    return encodeSType(instr);
  }

  // B-type instructions
  if (op == "BEQ" || op == "BNE" || op == "BLT" || op == "BGE" ||
      op == "BLTU" || op == "BGEU") {
    return encodeBType(instr);
  }

  // U-type instructions
  if (op == "LUI" || op == "AUIPC") {
    return encodeUType(instr);
  }

  // J-type instructions
  if (op == "JAL") {
    return encodeJType(instr);
  }

  throw std::runtime_error("Unknown instruction: " + op);
}

uint8_t InstructionEncoder::parseReg(const std::string &reg) const {
  int num = parseScalarRegister(reg);
  if (num < 0) {
    throw std::runtime_error("Invalid register: " + reg);
  }
  return static_cast<uint8_t>(num);
}

uint8_t InstructionEncoder::parseVReg(const std::string &reg) const {
  int num = parseVectorRegister(reg);
  if (num < 0) {
    throw std::runtime_error("Invalid vector register: " + reg);
  }
  return static_cast<uint8_t>(num);
}

SWord InstructionEncoder::parseImmediate(const std::string &imm) const {
  try {
    if (imm.size() > 2 && imm[0] == '0' && (imm[1] == 'x' || imm[1] == 'X')) {
      return static_cast<SWord>(std::stol(imm, nullptr, 16));
    }
    return static_cast<SWord>(std::stol(imm));
  } catch (...) {
    throw std::runtime_error("Invalid immediate value: " + imm);
  }
}

SWord InstructionEncoder::resolveLabelOffset(const std::string &label,
                                             Address currentAddr) const {
  auto it = symbolTable_.find(label);
  if (it == symbolTable_.end()) {
    throw std::runtime_error("Undefined label: " + label);
  }
  return static_cast<SWord>(it->second) - static_cast<SWord>(currentAddr);
}

std::pair<SWord, uint8_t>
InstructionEncoder::parseMemoryOperand(const std::string &operand) const {
  // Parse "offset(base)" format
  size_t lparen = operand.find('(');
  size_t rparen = operand.find(')');

  if (lparen == std::string::npos || rparen == std::string::npos) {
    throw std::runtime_error("Invalid memory operand format: " + operand);
  }

  std::string offsetStr = operand.substr(0, lparen);
  std::string baseStr = operand.substr(lparen + 1, rparen - lparen - 1);

  SWord offset = offsetStr.empty() ? 0 : parseImmediate(offsetStr);
  uint8_t base = parseReg(baseStr);

  return {offset, base};
}

// =============================================================================
// R-Type Encoding
// =============================================================================

Word InstructionEncoder::encodeRType(const ParsedInstruction &instr) {
  uint8_t rd = parseReg(instr.operands[0]);
  uint8_t rs1 = parseReg(instr.operands[1]);
  uint8_t rs2 = parseReg(instr.operands[2]);

  uint8_t funct3 = 0, funct7 = 0;

  if (instr.opcode == "ADD") {
    funct3 = 0b000;
    funct7 = 0b0000000;
  } else if (instr.opcode == "SUB") {
    funct3 = 0b000;
    funct7 = 0b0100000;
  } else if (instr.opcode == "AND") {
    funct3 = 0b111;
    funct7 = 0b0000000;
  } else if (instr.opcode == "OR") {
    funct3 = 0b110;
    funct7 = 0b0000000;
  } else if (instr.opcode == "XOR") {
    funct3 = 0b100;
    funct7 = 0b0000000;
  } else if (instr.opcode == "SLL") {
    funct3 = 0b001;
    funct7 = 0b0000000;
  } else if (instr.opcode == "SRL") {
    funct3 = 0b101;
    funct7 = 0b0000000;
  } else if (instr.opcode == "SRA") {
    funct3 = 0b101;
    funct7 = 0b0100000;
  } else if (instr.opcode == "SLT") {
    funct3 = 0b010;
    funct7 = 0b0000000;
  } else if (instr.opcode == "SLTU") {
    funct3 = 0b011;
    funct7 = 0b0000000;
  }

  return Instruction::encodeR(OPCODE_OP, rd, funct3, rs1, rs2, funct7);
}

// =============================================================================
// I-Type Encoding
// =============================================================================

Word InstructionEncoder::encodeIType(const ParsedInstruction &instr) {
  uint8_t rd, rs1;
  SWord imm;
  uint8_t funct3 = 0;
  uint8_t opcode = OPCODE_OP_IMM;

  if (instr.opcode == "LW") {
    opcode = OPCODE_LOAD;
    funct3 = RV32IFunct3::LW;
    rd = parseReg(instr.operands[0]);
    auto [offset, base] = parseMemoryOperand(instr.operands[1]);
    rs1 = base;
    imm = offset;
  } else if (instr.opcode == "JALR") {
    opcode = OPCODE_JALR;
    funct3 = RV32IFunct3::JALR;
    rd = parseReg(instr.operands[0]);
    if (instr.operands.size() == 2) {
      // JALR rd, rs1 (imm = 0)
      rs1 = parseReg(instr.operands[1]);
      imm = 0;
    } else {
      // JALR rd, rs1, imm
      rs1 = parseReg(instr.operands[1]);
      imm = parseImmediate(instr.operands[2]);
    }
  } else {
    rd = parseReg(instr.operands[0]);
    rs1 = parseReg(instr.operands[1]);
    imm = parseImmediate(instr.operands[2]);

    if (instr.opcode == "ADDI")
      funct3 = 0b000;
    else if (instr.opcode == "ANDI")
      funct3 = 0b111;
    else if (instr.opcode == "ORI")
      funct3 = 0b110;
    else if (instr.opcode == "XORI")
      funct3 = 0b100;
    else if (instr.opcode == "SLTI")
      funct3 = 0b010;
    else if (instr.opcode == "SLTIU")
      funct3 = 0b011;
    else if (instr.opcode == "SLLI") {
      funct3 = 0b001;
      imm &= 0x1F;
    } else if (instr.opcode == "SRLI") {
      funct3 = 0b101;
      imm &= 0x1F;
    } else if (instr.opcode == "SRAI") {
      funct3 = 0b101;
      imm = (imm & 0x1F) | 0x400;
    }
  }

  return Instruction::encodeI(opcode, rd, funct3, rs1, imm);
}

// =============================================================================
// S-Type Encoding
// =============================================================================

Word InstructionEncoder::encodeSType(const ParsedInstruction &instr) {
  uint8_t rs2 = parseReg(instr.operands[0]);
  auto [offset, base] = parseMemoryOperand(instr.operands[1]);

  return Instruction::encodeS(OPCODE_STORE, RV32IFunct3::SW, base, rs2, offset);
}

// =============================================================================
// B-Type Encoding
// =============================================================================

Word InstructionEncoder::encodeBType(const ParsedInstruction &instr) {
  uint8_t rs1 = parseReg(instr.operands[0]);
  uint8_t rs2 = parseReg(instr.operands[1]);
  SWord offset;

  if (instr.needsLabelResolution) {
    offset = resolveLabelOffset(instr.targetLabel, instr.address);
  } else {
    offset = parseImmediate(instr.operands[2]);
  }

  uint8_t funct3 = 0;
  if (instr.opcode == "BEQ")
    funct3 = RV32IFunct3::BEQ;
  else if (instr.opcode == "BNE")
    funct3 = RV32IFunct3::BNE;
  else if (instr.opcode == "BLT")
    funct3 = RV32IFunct3::BLT;
  else if (instr.opcode == "BGE")
    funct3 = RV32IFunct3::BGE;
  else if (instr.opcode == "BLTU")
    funct3 = RV32IFunct3::BLTU;
  else if (instr.opcode == "BGEU")
    funct3 = RV32IFunct3::BGEU;

  return Instruction::encodeB(OPCODE_BRANCH, funct3, rs1, rs2, offset);
}

// =============================================================================
// U-Type Encoding
// =============================================================================

Word InstructionEncoder::encodeUType(const ParsedInstruction &instr) {
  uint8_t rd = parseReg(instr.operands[0]);
  SWord imm = parseImmediate(instr.operands[1]);

  uint8_t opcode = (instr.opcode == "LUI") ? OPCODE_LUI : OPCODE_AUIPC;

  return Instruction::encodeU(opcode, rd, imm << 12);
}

// =============================================================================
// J-Type Encoding
// =============================================================================

Word InstructionEncoder::encodeJType(const ParsedInstruction &instr) {
  uint8_t rd = parseReg(instr.operands[0]);
  SWord offset;

  if (instr.needsLabelResolution) {
    offset = resolveLabelOffset(instr.targetLabel, instr.address);
  } else {
    offset = parseImmediate(instr.operands[1]);
  }

  return Instruction::encodeJ(OPCODE_JAL, rd, offset);
}

// =============================================================================
// NPU Encoding
// =============================================================================

Word InstructionEncoder::encodeNPU(const ParsedInstruction &instr) {
  const std::string &op = instr.opcode;

  // Memory operations
  if (op == "VLOAD" || op == "VLBC") {
    return encodeNPU_IType(instr);
  }
  if (op == "VSTORE") {
    return encodeNPU_SType(instr);
  }

  // R-type NPU operations
  return encodeNPU_RType(instr);
}

Word InstructionEncoder::encodeNPU_RType(const ParsedInstruction &instr) {
  const std::string &op = instr.opcode;
  uint8_t funct3 = 0, funct7 = 0;
  uint8_t vd = 0, vs1 = 0, vs2 = 0;

  if (op == "VADD") {
    funct3 = NPUFunct3::ARITH;
    funct7 = NPUFunct7::DEFAULT;
    vd = parseVReg(instr.operands[0]);
    vs1 = parseVReg(instr.operands[1]);
    vs2 = parseVReg(instr.operands[2]);
  } else if (op == "VMUL") {
    funct3 = NPUFunct3::MUL;
    funct7 = NPUFunct7::DEFAULT;
    vd = parseVReg(instr.operands[0]);
    vs1 = parseVReg(instr.operands[1]);
    vs2 = parseVReg(instr.operands[2]);
  } else if (op == "VMAC") {
    funct3 = NPUFunct3::MAC;
    funct7 = NPUFunct7::DEFAULT;
    vd = parseVReg(instr.operands[0]);
    vs1 = parseVReg(instr.operands[1]);
    vs2 = parseVReg(instr.operands[2]);
  } else if (op == "VRELU") {
    funct3 = NPUFunct3::RELU;
    funct7 = NPUFunct7::DEFAULT;
    vd = parseVReg(instr.operands[0]);
    vs1 = parseVReg(instr.operands[1]);
    vs2 = 0;
  } else if (op == "VREDMAX") {
    funct3 = NPUFunct3::REDUCE;
    funct7 = NPUFunct7::DEFAULT;
    vd = parseReg(instr.operands[0]); // Writes to scalar register
    vs1 = parseVReg(instr.operands[1]);
    vs2 = 0;
  } else if (op == "VARGMAX") {
    funct3 = NPUFunct3::REDUCE;
    funct7 = NPUFunct7::VARGMAX;
    vd = parseReg(instr.operands[0]); // Writes to scalar register
    vs1 = parseVReg(instr.operands[1]);
    vs2 = 0;
  } else if (op == "VCLR") {
    funct3 = NPUFunct3::ARITH;
    funct7 = NPUFunct7::VCLR;
    vd = parseVReg(instr.operands[0]);
    vs1 = 0;
    vs2 = 0;
  }

  return Instruction::encodeNPU_R(funct3, funct7, vd, vs1, vs2);
}

Word InstructionEncoder::encodeNPU_IType(const ParsedInstruction &instr) {
  const std::string &op = instr.opcode;
  uint8_t funct3;
  uint8_t vd = parseVReg(instr.operands[0]);
  auto [offset, base] = parseMemoryOperand(instr.operands[1]);

  if (op == "VLOAD") {
    funct3 = NPUFunct3::MEM;
    // Use funct7 bits in immediate to distinguish from VLBC
    return Instruction::encodeNPU_I(funct3, vd, base, offset);
  } else if (op == "VLBC") {
    funct3 = NPUFunct3::MEM;
    // Set a special bit to indicate broadcast
    return Instruction::encodeNPU_I(funct3, vd, base, offset | 0x800);
  } else if (op == "VCLP") {
    funct3 = NPUFunct3::CLP;
    uint8_t vs1 = parseVReg(instr.operands[1]);
    SWord imm = parseImmediate(instr.operands[2]);
    return Instruction::encodeNPU_I(funct3, vd, vs1, imm);
  }

  throw std::runtime_error("Unknown NPU I-type instruction: " + op);
}

Word InstructionEncoder::encodeNPU_SType(const ParsedInstruction &instr) {
  uint8_t vs = parseVReg(instr.operands[0]);
  auto [offset, base] = parseMemoryOperand(instr.operands[1]);

  // Use funct7 = 0100000 to distinguish VSTORE from VLOAD
  // Encode as R-type with special funct7 to make it identifiable
  // Format: funct7 | vs | rs1 | funct3 | 00000 | opcode
  return (static_cast<Word>(NPUFunct7::VSTORE) << 25) |
         (static_cast<Word>(vs) << 20) | (static_cast<Word>(base) << 15) |
         (static_cast<Word>(NPUFunct3::MEM) << 12) |
         (static_cast<Word>(offset & 0x1F) << 7) | OPCODE_NPU;
}

// =============================================================================
// Pseudo Instruction Encoding
// =============================================================================

Word InstructionEncoder::encodePseudo(const ParsedInstruction &instr) {
  const std::string &op = instr.opcode;

  if (op == "NOP") {
    // NOP = ADDI x0, x0, 0
    return Instruction::encodeI(OPCODE_OP_IMM, 0, 0, 0, 0);
  }

  if (op == "HALT") {
    // Custom halt instruction
    return 0x0000007F; // Custom opcode for halt
  }

  if (op == "RET") {
    // RET = JALR x0, ra, 0
    return Instruction::encodeI(OPCODE_JALR, 0, 0, 1, 0);
  }

  if (op == "J") {
    // J label = JAL x0, offset
    SWord offset = resolveLabelOffset(instr.targetLabel, instr.address);
    return Instruction::encodeJ(OPCODE_JAL, 0, offset);
  }

  if (op == "MV") {
    // MV rd, rs = ADDI rd, rs, 0
    uint8_t rd = parseReg(instr.operands[0]);
    uint8_t rs = parseReg(instr.operands[1]);
    return Instruction::encodeI(OPCODE_OP_IMM, rd, 0, rs, 0);
  }

  if (op == "LI") {
    // LI rd, imm = ADDI rd, x0, imm (for small immediates)
    uint8_t rd = parseReg(instr.operands[0]);
    SWord imm = parseImmediate(instr.operands[1]);
    return Instruction::encodeI(OPCODE_OP_IMM, rd, 0, 0, imm);
  }

  if (op == "CALL") {
    // CALL label = JAL ra, offset
    SWord offset = resolveLabelOffset(instr.targetLabel, instr.address);
    return Instruction::encodeJ(OPCODE_JAL, 1, offset);
  }

  throw std::runtime_error("Unknown pseudo instruction: " + op);
}

} // namespace npu
