#include "../include/alu.hpp"

namespace npu {

// =============================================================================
// ALU Execute
// =============================================================================

ALUResult ALU::execute(ALUOp op, SWord a, SWord b) {
  SWord result = 0;

  switch (op) {
  case ALUOp::ADD:
    result = add(a, b);
    break;
  case ALUOp::SUB:
    result = sub(a, b);
    break;
  case ALUOp::AND:
    result = andOp(a, b);
    break;
  case ALUOp::OR:
    result = orOp(a, b);
    break;
  case ALUOp::XOR:
    result = xorOp(a, b);
    break;
  case ALUOp::SLL:
    result = sll(a, b);
    break;
  case ALUOp::SRL:
    result = srl(a, b);
    break;
  case ALUOp::SRA:
    result = sra(a, b);
    break;
  case ALUOp::SLT:
    result = slt(a, b);
    break;
  case ALUOp::SLTU:
    result = sltu(a, b);
    break;
  default:
    result = 0;
    break;
  }

  return ALUResult(result);
}

// =============================================================================
// Individual ALU Operations
// =============================================================================

SWord ALU::add(SWord a, SWord b) { return a + b; }

SWord ALU::sub(SWord a, SWord b) { return a - b; }

SWord ALU::andOp(SWord a, SWord b) { return a & b; }

SWord ALU::orOp(SWord a, SWord b) { return a | b; }

SWord ALU::xorOp(SWord a, SWord b) { return a ^ b; }

SWord ALU::sll(SWord a, SWord b) {
  return a << (b & 0x1F); // Only use lower 5 bits
}

SWord ALU::srl(SWord a, SWord b) {
  return static_cast<SWord>(static_cast<Word>(a) >> (b & 0x1F));
}

SWord ALU::sra(SWord a, SWord b) {
  return a >> (b & 0x1F); // Arithmetic shift (sign-extended)
}

SWord ALU::slt(SWord a, SWord b) { return (a < b) ? 1 : 0; }

SWord ALU::sltu(SWord a, SWord b) {
  return (static_cast<Word>(a) < static_cast<Word>(b)) ? 1 : 0;
}

// =============================================================================
// Branch Condition Evaluation
// =============================================================================

bool ALU::evaluateBranch(BranchOp op, SWord a, SWord b) {
  Word ua = static_cast<Word>(a);
  Word ub = static_cast<Word>(b);

  switch (op) {
  case BranchOp::BEQ:
    return a == b;
  case BranchOp::BNE:
    return a != b;
  case BranchOp::BLT:
    return a < b;
  case BranchOp::BGE:
    return a >= b;
  case BranchOp::BLTU:
    return ua < ub;
  case BranchOp::BGEU:
    return ua >= ub;
  default:
    return false;
  }
}

} // namespace npu
