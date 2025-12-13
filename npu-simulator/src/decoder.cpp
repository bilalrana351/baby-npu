#include "../include/decoder.hpp"
#include <sstream>

namespace npu {

// =============================================================================
// Main Decode Function
// =============================================================================

DecodedInstr Decoder::decode(Word instruction) {
  DecodedInstr decoded;
  decoded.raw = instruction;
  decoded.opcode = Instruction::getOpcode(instruction);
  decoded.rd = Instruction::getRd(instruction);
  decoded.rs1 = Instruction::getRs1(instruction);
  decoded.rs2 = Instruction::getRs2(instruction);
  decoded.funct3 = Instruction::getFunct3(instruction);
  decoded.funct7 = Instruction::getFunct7(instruction);

  // Determine instruction type and decode based on opcode
  switch (decoded.opcode) {
  case OPCODE_NPU:
    decoded.isNPU = true;
    decodeNPU(instruction, decoded);
    break;

  case OPCODE_OP:
    decoded.type = InstrType::R_TYPE;
    decodeRType(instruction, decoded);
    break;

  case OPCODE_OP_IMM:
  case OPCODE_LOAD:
  case OPCODE_JALR:
    decoded.type = InstrType::I_TYPE;
    decodeIType(instruction, decoded);
    break;

  case OPCODE_STORE:
    decoded.type = InstrType::S_TYPE;
    decodeSType(instruction, decoded);
    break;

  case OPCODE_BRANCH:
    decoded.type = InstrType::B_TYPE;
    decodeBType(instruction, decoded);
    break;

  case OPCODE_LUI:
  case OPCODE_AUIPC:
    decoded.type = InstrType::U_TYPE;
    decodeUType(instruction, decoded);
    break;

  case OPCODE_JAL:
    decoded.type = InstrType::J_TYPE;
    decodeJType(instruction, decoded);
    break;

  case OPCODE_HALT:
    decoded.isHalt = true;
    decoded.mnemonic = "HALT";
    break;

  default:
    decoded.type = InstrType::UNKNOWN;
    decoded.mnemonic = "UNKNOWN";
  }

  return decoded;
}

// =============================================================================
// NPU Instruction Decoding
// =============================================================================

void Decoder::decodeNPU(Word instr, DecodedInstr &decoded) {
  decoded.isNPU = true;

  // Determine if it's a store by checking funct7 == VSTORE (0100000)
  bool isStore = (decoded.funct3 == NPUFunct3::MEM) &&
                 (decoded.funct7 == NPUFunct7::VSTORE);

  decoded.npuOp = getNPUOp(decoded.funct3, decoded.funct7, isStore);

  switch (decoded.npuOp) {
  case NPUOp::VLOAD:
  case NPUOp::VLBC:
  case NPUOp::VCLP:
    decoded.type = InstrType::I_TYPE;
    decoded.imm = Instruction::getImmI(instr);
    decoded.isLoad =
        (decoded.npuOp == NPUOp::VLOAD || decoded.npuOp == NPUOp::VLBC);
    decoded.writesReg = true;
    break;

  case NPUOp::VSTORE:
    decoded.type = InstrType::S_TYPE;
    decoded.imm = Instruction::getImmS(instr);
    decoded.isStore = true;
    break;

  case NPUOp::VADD:
  case NPUOp::VMUL:
  case NPUOp::VMAC:
  case NPUOp::VRELU:
  case NPUOp::VREDMAX:
  case NPUOp::VARGMAX:
  case NPUOp::VCLR:
    decoded.type = InstrType::R_TYPE;
    decoded.writesReg = true;
    break;

  default:
    break;
  }

  decoded.mnemonic = buildMnemonic(decoded);
}

NPUOp Decoder::getNPUOp(uint8_t funct3, uint8_t funct7, bool isStore) {
  switch (funct3) {
  case NPUFunct3::ARITH:
    return (funct7 == NPUFunct7::VCLR) ? NPUOp::VCLR : NPUOp::VADD;

  case NPUFunct3::MUL:
    return NPUOp::VMUL;

  case NPUFunct3::MAC:
    return NPUOp::VMAC;

  case NPUFunct3::RELU:
    return NPUOp::VRELU;

  case NPUFunct3::CLP:
    return NPUOp::VCLP;

  case NPUFunct3::REDUCE:
    return (funct7 == NPUFunct7::VARGMAX) ? NPUOp::VARGMAX : NPUOp::VREDMAX;

  case NPUFunct3::MEM:
    if (isStore)
      return NPUOp::VSTORE;
    // Check immediate for broadcast bit
    return NPUOp::VLOAD; // VLBC detected by imm bit during execution

  default:
    return NPUOp::NONE;
  }
}

// =============================================================================
// R-Type Decoding
// =============================================================================

void Decoder::decodeRType(Word instr, DecodedInstr &decoded) {
  decoded.aluOp = getALUOpR(decoded.funct3, decoded.funct7);
  decoded.writesReg = (decoded.rd != 0);
  decoded.mnemonic = buildMnemonic(decoded);
}

ALUOp Decoder::getALUOpR(uint8_t funct3, uint8_t funct7) {
  switch (funct3) {
  case 0b000:
    return (funct7 == RV32IFunct7::ALT) ? ALUOp::SUB : ALUOp::ADD;
  case 0b001:
    return ALUOp::SLL;
  case 0b010:
    return ALUOp::SLT;
  case 0b011:
    return ALUOp::SLTU;
  case 0b100:
    return ALUOp::XOR;
  case 0b101:
    return (funct7 == RV32IFunct7::ALT) ? ALUOp::SRA : ALUOp::SRL;
  case 0b110:
    return ALUOp::OR;
  case 0b111:
    return ALUOp::AND;
  default:
    return ALUOp::NONE;
  }
}

// =============================================================================
// I-Type Decoding
// =============================================================================

void Decoder::decodeIType(Word instr, DecodedInstr &decoded) {
  decoded.imm = Instruction::getImmI(instr);

  switch (decoded.opcode) {
  case OPCODE_OP_IMM:
    decoded.aluOp = getALUOpI(decoded.funct3, decoded.imm);
    decoded.writesReg = (decoded.rd != 0);
    break;

  case OPCODE_LOAD:
    decoded.isLoad = true;
    decoded.writesReg = (decoded.rd != 0);
    decoded.aluOp = ALUOp::ADD; // Address calculation
    break;

  case OPCODE_JALR:
    decoded.isJump = true;
    decoded.writesReg = (decoded.rd != 0);
    decoded.aluOp = ALUOp::ADD; // Target calculation
    break;
  }

  decoded.mnemonic = buildMnemonic(decoded);
}

ALUOp Decoder::getALUOpI(uint8_t funct3, SWord imm) {
  switch (funct3) {
  case 0b000:
    return ALUOp::ADD;
  case 0b001:
    return ALUOp::SLL;
  case 0b010:
    return ALUOp::SLT;
  case 0b011:
    return ALUOp::SLTU;
  case 0b100:
    return ALUOp::XOR;
  case 0b101:
    return (imm & 0x400) ? ALUOp::SRA : ALUOp::SRL;
  case 0b110:
    return ALUOp::OR;
  case 0b111:
    return ALUOp::AND;
  default:
    return ALUOp::NONE;
  }
}

// =============================================================================
// S-Type Decoding
// =============================================================================

void Decoder::decodeSType(Word instr, DecodedInstr &decoded) {
  decoded.imm = Instruction::getImmS(instr);
  decoded.isStore = true;
  decoded.aluOp = ALUOp::ADD; // Address calculation
  decoded.mnemonic = buildMnemonic(decoded);
}

// =============================================================================
// B-Type Decoding
// =============================================================================

void Decoder::decodeBType(Word instr, DecodedInstr &decoded) {
  decoded.imm = Instruction::getImmB(instr);
  decoded.isBranch = true;
  decoded.branchOp = getBranchOp(decoded.funct3);
  decoded.mnemonic = buildMnemonic(decoded);
}

BranchOp Decoder::getBranchOp(uint8_t funct3) {
  switch (funct3) {
  case RV32IFunct3::BEQ:
    return BranchOp::BEQ;
  case RV32IFunct3::BNE:
    return BranchOp::BNE;
  case RV32IFunct3::BLT:
    return BranchOp::BLT;
  case RV32IFunct3::BGE:
    return BranchOp::BGE;
  case RV32IFunct3::BLTU:
    return BranchOp::BLTU;
  case RV32IFunct3::BGEU:
    return BranchOp::BGEU;
  default:
    return BranchOp::NONE;
  }
}

// =============================================================================
// U-Type Decoding
// =============================================================================

void Decoder::decodeUType(Word instr, DecodedInstr &decoded) {
  decoded.imm = Instruction::getImmU(instr);
  decoded.writesReg = (decoded.rd != 0);
  decoded.mnemonic = buildMnemonic(decoded);
}

// =============================================================================
// J-Type Decoding
// =============================================================================

void Decoder::decodeJType(Word instr, DecodedInstr &decoded) {
  decoded.imm = Instruction::getImmJ(instr);
  decoded.isJump = true;
  decoded.writesReg = (decoded.rd != 0);
  decoded.mnemonic = buildMnemonic(decoded);
}

// =============================================================================
// Mnemonic Builder
// =============================================================================

std::string Decoder::buildMnemonic(const DecodedInstr &decoded) {
  std::ostringstream oss;

  if (decoded.isNPU) {
    // NPU mnemonics
    switch (decoded.npuOp) {
    case NPUOp::VLOAD:
      oss << "VLOAD";
      break;
    case NPUOp::VSTORE:
      oss << "VSTORE";
      break;
    case NPUOp::VLBC:
      oss << "VLBC";
      break;
    case NPUOp::VADD:
      oss << "VADD";
      break;
    case NPUOp::VMUL:
      oss << "VMUL";
      break;
    case NPUOp::VMAC:
      oss << "VMAC";
      break;
    case NPUOp::VRELU:
      oss << "VRELU";
      break;
    case NPUOp::VCLP:
      oss << "VCLP";
      break;
    case NPUOp::VREDMAX:
      oss << "VREDMAX";
      break;
    case NPUOp::VARGMAX:
      oss << "VARGMAX";
      break;
    case NPUOp::VCLR:
      oss << "VCLR";
      break;
    default:
      oss << "NPU_UNK";
      break;
    }
    return oss.str();
  }

  // RV32I mnemonics
  switch (decoded.opcode) {
  case OPCODE_OP:
    switch (decoded.aluOp) {
    case ALUOp::ADD:
      oss << "ADD";
      break;
    case ALUOp::SUB:
      oss << "SUB";
      break;
    case ALUOp::AND:
      oss << "AND";
      break;
    case ALUOp::OR:
      oss << "OR";
      break;
    case ALUOp::XOR:
      oss << "XOR";
      break;
    case ALUOp::SLL:
      oss << "SLL";
      break;
    case ALUOp::SRL:
      oss << "SRL";
      break;
    case ALUOp::SRA:
      oss << "SRA";
      break;
    case ALUOp::SLT:
      oss << "SLT";
      break;
    case ALUOp::SLTU:
      oss << "SLTU";
      break;
    default:
      oss << "OP?";
      break;
    }
    break;

  case OPCODE_OP_IMM:
    // Check for NOP
    if (decoded.rd == 0 && decoded.rs1 == 0 && decoded.imm == 0) {
      return "NOP";
    }
    switch (decoded.aluOp) {
    case ALUOp::ADD:
      oss << "ADDI";
      break;
    case ALUOp::AND:
      oss << "ANDI";
      break;
    case ALUOp::OR:
      oss << "ORI";
      break;
    case ALUOp::XOR:
      oss << "XORI";
      break;
    case ALUOp::SLL:
      oss << "SLLI";
      break;
    case ALUOp::SRL:
      oss << "SRLI";
      break;
    case ALUOp::SRA:
      oss << "SRAI";
      break;
    case ALUOp::SLT:
      oss << "SLTI";
      break;
    case ALUOp::SLTU:
      oss << "SLTIU";
      break;
    default:
      oss << "OP_IMM?";
      break;
    }
    break;

  case OPCODE_LOAD:
    oss << "LW";
    break;

  case OPCODE_STORE:
    oss << "SW";
    break;

  case OPCODE_BRANCH:
    switch (decoded.branchOp) {
    case BranchOp::BEQ:
      oss << "BEQ";
      break;
    case BranchOp::BNE:
      oss << "BNE";
      break;
    case BranchOp::BLT:
      oss << "BLT";
      break;
    case BranchOp::BGE:
      oss << "BGE";
      break;
    case BranchOp::BLTU:
      oss << "BLTU";
      break;
    case BranchOp::BGEU:
      oss << "BGEU";
      break;
    default:
      oss << "B?";
      break;
    }
    break;

  case OPCODE_JAL:
    if (decoded.rd == 0) {
      return "J";
    }
    oss << "JAL";
    break;

  case OPCODE_JALR:
    if (decoded.rd == 0 && decoded.rs1 == 1 && decoded.imm == 0) {
      return "RET";
    }
    oss << "JALR";
    break;

  case OPCODE_LUI:
    oss << "LUI";
    break;

  case OPCODE_AUIPC:
    oss << "AUIPC";
    break;

  case OPCODE_HALT:
    return "HALT";

  default:
    oss << "UNKNOWN";
  }

  return oss.str();
}

} // namespace npu
