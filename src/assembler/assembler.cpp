#include "../../include/assembler/assembler.hpp"
#include <fstream>
#include <sstream>

namespace npu {

// =============================================================================
// Assembler Implementation
// =============================================================================

std::string Assembler::readFile(const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

AssemblyResult Assembler::assemble(const std::string &source) {
  AssemblyResult result;

  try {
    // Phase 1: Lexical analysis
    if (verbose_) {
      std::cout << "Tokenizing...\n";
    }
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    if (verbose_) {
      std::cout << "Tokens: " << tokens.size() << "\n";
      for (const auto &tok : tokens) {
        if (tok.type != TokenType::NEWLINE &&
            tok.type != TokenType::END_OF_FILE) {
          std::cout << "  " << tok.toString() << "\n";
        }
      }
    }

    // Phase 2: Parsing (two-pass)
    if (verbose_) {
      std::cout << "Parsing...\n";
    }
    Parser parser(tokens);
    parser.parse();

    result.symbolTable = parser.getSymbolTable();

    if (verbose_) {
      std::cout << "Symbol Table:\n";
      for (const auto &[label, addr] : result.symbolTable) {
        std::cout << "  " << label << " -> " << addrToHex(addr) << "\n";
      }
    }

    // Phase 3: Encoding
    if (verbose_) {
      std::cout << "Encoding...\n";
    }
    InstructionEncoder encoder(result.symbolTable);

    for (const auto &instr : parser.getInstructions()) {
      Word encoded = encoder.encode(instr);
      result.machineCode.push_back(encoded);

      if (verbose_) {
        std::cout << "  " << addrToHex(instr.address) << ": " << instr.opcode;
        for (const auto &op : instr.operands) {
          std::cout << " " << op;
        }
        std::cout << " -> " << wordToHex(encoded) << "\n";
      }
    }

    result.success = true;

  } catch (const std::exception &e) {
    result.errors.push_back(e.what());
    result.success = false;
  }

  return result;
}

AssemblyResult Assembler::assembleFile(const std::string &filename) {
  try {
    std::string source = readFile(filename);
    return assemble(source);
  } catch (const std::exception &e) {
    AssemblyResult result;
    result.errors.push_back(e.what());
    result.success = false;
    return result;
  }
}

std::string Assembler::disassemble(Word instruction, Address addr) {
  return Disassembler::disassemble(instruction, addr);
}

void Assembler::printProgram(const AssemblyResult &result, std::ostream &os) {
  os << "=== Assembled Program ===\n";
  os << "Instructions: " << result.machineCode.size() << "\n";
  os << "Size: " << result.machineCode.size() * 4 << " bytes\n\n";

  os << "Machine Code:\n";
  Address addr = TEXT_SEGMENT_START;
  for (Word instr : result.machineCode) {
    os << "  " << addrToHex(addr) << ": " << wordToHex(instr) << "  ; "
       << Disassembler::disassemble(instr, addr) << "\n";
    addr += 4;
  }

  if (!result.symbolTable.empty()) {
    os << "\nSymbol Table:\n";
    for (const auto &[label, labelAddr] : result.symbolTable) {
      os << "  " << label << ": " << addrToHex(labelAddr) << "\n";
    }
  }
}

// =============================================================================
// Disassembler Implementation
// =============================================================================

std::string Disassembler::disassemble(Word instr, Address addr) {
  Word opcode = Instruction::getOpcode(instr);

  if (opcode == OPCODE_NPU) {
    return disassembleNPU(instr);
  }

  return disassembleRV32I(instr);
}

std::string Disassembler::disassembleNPU(Word instr) {
  uint8_t funct3 = Instruction::getFunct3(instr);
  uint8_t funct7 = Instruction::getFunct7(instr);
  uint8_t rd = Instruction::getRd(instr);
  uint8_t rs1 = Instruction::getRs1(instr);
  uint8_t rs2 = Instruction::getRs2(instr);
  SWord imm = Instruction::getImmI(instr);

  std::ostringstream oss;

  switch (funct3) {
  case NPUFunct3::ARITH:
    if (funct7 == NPUFunct7::VCLR) {
      oss << "VCLR V" << (int)rd;
    } else {
      oss << "VADD V" << (int)rd << ", V" << (int)rs1 << ", V" << (int)rs2;
    }
    break;

  case NPUFunct3::MUL:
    oss << "VMUL V" << (int)rd << ", V" << (int)rs1 << ", V" << (int)rs2;
    break;

  case NPUFunct3::MAC:
    oss << "VMAC V" << (int)rd << ", V" << (int)rs1 << ", V" << (int)rs2;
    break;

  case NPUFunct3::RELU:
    oss << "VRELU V" << (int)rd << ", V" << (int)rs1;
    break;

  case NPUFunct3::CLP:
    oss << "VCLP V" << (int)rd << ", V" << (int)rs1 << ", " << imm;
    break;

  case NPUFunct3::REDUCE:
    if (funct7 == NPUFunct7::VARGMAX) {
      oss << "VARGMAX " << scalarRegName(rd) << ", V" << (int)rs1;
    } else {
      oss << "VREDMAX " << scalarRegName(rd) << ", V" << (int)rs1;
    }
    break;

  case NPUFunct3::MEM:
    // Check if it's a store by looking at funct7 == VSTORE (0100000)
    if (funct7 == NPUFunct7::VSTORE) {
      // VSTORE - vs in rs2, offset in rd
      oss << "VSTORE V" << (int)rs2 << ", " << (int)rd << "("
          << scalarRegName(rs1) << ")";
    } else if (imm & 0x800) {
      // VLBC (broadcast)
      oss << "VLBC V" << (int)rd << ", " << (imm & 0x7FF) << "("
          << scalarRegName(rs1) << ")";
    } else {
      // VLOAD
      oss << "VLOAD V" << (int)rd << ", " << imm << "(" << scalarRegName(rs1)
          << ")";
    }
    break;

  default:
    oss << "NPU_UNKNOWN";
  }

  return oss.str();
}

std::string Disassembler::disassembleRV32I(Word instr) {
  Word opcode = Instruction::getOpcode(instr);
  uint8_t funct3 = Instruction::getFunct3(instr);
  uint8_t funct7 = Instruction::getFunct7(instr);
  uint8_t rd = Instruction::getRd(instr);
  uint8_t rs1 = Instruction::getRs1(instr);
  uint8_t rs2 = Instruction::getRs2(instr);

  std::ostringstream oss;

  switch (opcode) {
  case OPCODE_OP: {
    std::string op;
    if (funct3 == 0b000 && funct7 == 0b0000000)
      op = "ADD";
    else if (funct3 == 0b000 && funct7 == 0b0100000)
      op = "SUB";
    else if (funct3 == 0b111)
      op = "AND";
    else if (funct3 == 0b110)
      op = "OR";
    else if (funct3 == 0b100)
      op = "XOR";
    else if (funct3 == 0b001)
      op = "SLL";
    else if (funct3 == 0b101 && funct7 == 0b0000000)
      op = "SRL";
    else if (funct3 == 0b101 && funct7 == 0b0100000)
      op = "SRA";
    else if (funct3 == 0b010)
      op = "SLT";
    else if (funct3 == 0b011)
      op = "SLTU";
    else
      op = "OP?";
    oss << op << " " << scalarRegName(rd) << ", " << scalarRegName(rs1) << ", "
        << scalarRegName(rs2);
    break;
  }

  case OPCODE_OP_IMM: {
    SWord imm = Instruction::getImmI(instr);
    if (rd == 0 && rs1 == 0 && imm == 0) {
      oss << "NOP";
    } else {
      std::string op;
      if (funct3 == 0b000)
        op = "ADDI";
      else if (funct3 == 0b111)
        op = "ANDI";
      else if (funct3 == 0b110)
        op = "ORI";
      else if (funct3 == 0b100)
        op = "XORI";
      else if (funct3 == 0b010)
        op = "SLTI";
      else if (funct3 == 0b011)
        op = "SLTIU";
      else if (funct3 == 0b001) {
        op = "SLLI";
        imm &= 0x1F;
      } else if (funct3 == 0b101 && (imm & 0x400)) {
        op = "SRAI";
        imm &= 0x1F;
      } else if (funct3 == 0b101) {
        op = "SRLI";
        imm &= 0x1F;
      } else
        op = "OP_IMM?";
      oss << op << " " << scalarRegName(rd) << ", " << scalarRegName(rs1)
          << ", " << imm;
    }
    break;
  }

  case OPCODE_LOAD: {
    SWord imm = Instruction::getImmI(instr);
    oss << "LW " << scalarRegName(rd) << ", " << imm << "("
        << scalarRegName(rs1) << ")";
    break;
  }

  case OPCODE_STORE: {
    SWord imm = Instruction::getImmS(instr);
    oss << "SW " << scalarRegName(rs2) << ", " << imm << "("
        << scalarRegName(rs1) << ")";
    break;
  }

  case OPCODE_BRANCH: {
    SWord imm = Instruction::getImmB(instr);
    std::string op;
    if (funct3 == RV32IFunct3::BEQ)
      op = "BEQ";
    else if (funct3 == RV32IFunct3::BNE)
      op = "BNE";
    else if (funct3 == RV32IFunct3::BLT)
      op = "BLT";
    else if (funct3 == RV32IFunct3::BGE)
      op = "BGE";
    else if (funct3 == RV32IFunct3::BLTU)
      op = "BLTU";
    else if (funct3 == RV32IFunct3::BGEU)
      op = "BGEU";
    else
      op = "B?";
    oss << op << " " << scalarRegName(rs1) << ", " << scalarRegName(rs2) << ", "
        << imm;
    break;
  }

  case OPCODE_JAL: {
    SWord imm = Instruction::getImmJ(instr);
    if (rd == 0) {
      oss << "J " << imm;
    } else {
      oss << "JAL " << scalarRegName(rd) << ", " << imm;
    }
    break;
  }

  case OPCODE_JALR: {
    SWord imm = Instruction::getImmI(instr);
    if (rd == 0 && rs1 == 1 && imm == 0) {
      oss << "RET";
    } else {
      oss << "JALR " << scalarRegName(rd) << ", " << scalarRegName(rs1) << ", "
          << imm;
    }
    break;
  }

  case OPCODE_LUI: {
    SWord imm = Instruction::getImmU(instr);
    oss << "LUI " << scalarRegName(rd) << ", " << (imm >> 12);
    break;
  }

  case OPCODE_AUIPC: {
    SWord imm = Instruction::getImmU(instr);
    oss << "AUIPC " << scalarRegName(rd) << ", " << (imm >> 12);
    break;
  }

  case OPCODE_HALT:
    oss << "HALT";
    break;

  default:
    oss << "UNKNOWN (" << wordToHex(instr) << ")";
  }

  return oss.str();
}

} // namespace npu
