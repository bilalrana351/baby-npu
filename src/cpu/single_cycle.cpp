#include "../../include/cpu/single_cycle.hpp"
#include <iomanip>
#include <iostream>

namespace npu {

// =============================================================================
// Run Loop
// =============================================================================

void SingleCycleCPU::run() {
  while (!halted_ && stats_.instructionCount < MAX_INSTRUCTIONS) {
    step();
  }

  if (stats_.instructionCount >= MAX_INSTRUCTIONS) {
    std::cerr << "Warning: Execution stopped after " << MAX_INSTRUCTIONS
              << " instructions (possible infinite loop)\n";
  }
}

// =============================================================================
// Single Step (One Instruction)
// =============================================================================

void SingleCycleCPU::step() {
  if (halted_)
    return;

  // IF: Instruction Fetch
  Word instruction = fetch();

  // ID: Instruction Decode
  DecodedInstr decoded = decode(instruction);

  // Log instruction before execution
  if (verbose_) {
    logInstruction(decoded);
  }

  // EX + MEM + WB: Execute, Memory, Write-back
  execute(decoded);

  // Update statistics
  stats_.cycleCount++;
  stats_.instructionCount++;

  if (decoded.isNPU) {
    stats_.npuInstructions++;
  } else {
    stats_.scalarInstructions++;
  }

  // Log to logger if available
  if (logger_) {
    logger_->logCycle(stats_.cycleCount, pc_ - 4, decoded, regs_);
  }
}

// =============================================================================
// Fetch Stage
// =============================================================================

Word SingleCycleCPU::fetch() {
  Word instruction = mem_.readWord(pc_);
  return instruction;
}

// =============================================================================
// Decode Stage
// =============================================================================

DecodedInstr SingleCycleCPU::decode(Word instruction) {
  return decoder_.decode(instruction);
}

// =============================================================================
// Execute Stage
// =============================================================================

void SingleCycleCPU::execute(const DecodedInstr &instr) {
  if (instr.isHalt) {
    halted_ = true;
    return;
  }

  if (instr.isNPU) {
    executeNPU(instr);
  } else {
    executeRV32I(instr);
  }
}

// =============================================================================
// RV32I Execution
// =============================================================================

void SingleCycleCPU::executeRV32I(const DecodedInstr &instr) {
  SWord rs1Val = regs_.readScalar(instr.rs1);
  SWord rs2Val = regs_.readScalar(instr.rs2);
  Address nextPC = pc_ + 4;

  switch (instr.opcode) {
  case OPCODE_OP: {
    // R-type: rd = rs1 op rs2
    ALUResult result = ALU::execute(instr.aluOp, rs1Val, rs2Val);
    if (instr.writesReg) {
      regs_.writeScalar(instr.rd, result.result);
    }
    break;
  }

  case OPCODE_OP_IMM: {
    // I-type: rd = rs1 op imm
    SWord immVal = instr.imm;
    if (instr.aluOp == ALUOp::SLL || instr.aluOp == ALUOp::SRL ||
        instr.aluOp == ALUOp::SRA) {
      immVal &= 0x1F; // Shift amount is 5 bits
    }
    ALUResult result = ALU::execute(instr.aluOp, rs1Val, immVal);
    if (instr.writesReg) {
      regs_.writeScalar(instr.rd, result.result);
    }
    break;
  }

  case OPCODE_LOAD: {
    // LW: rd = Mem[rs1 + imm]
    Address addr = static_cast<Address>(rs1Val + instr.imm);
    SWord value = memoryLoad(addr);
    if (instr.writesReg) {
      regs_.writeScalar(instr.rd, value);
    }
    stats_.memoryReads++;
    break;
  }

  case OPCODE_STORE: {
    // SW: Mem[rs1 + imm] = rs2
    Address addr = static_cast<Address>(rs1Val + instr.imm);
    memoryStore(addr, rs2Val);
    stats_.memoryWrites++;
    break;
  }

  case OPCODE_BRANCH: {
    // B-type: if (rs1 cmp rs2) PC = PC + imm
    bool taken = ALU::evaluateBranch(instr.branchOp, rs1Val, rs2Val);
    if (taken) {
      nextPC = pc_ + instr.imm;
      stats_.branchesTaken++;
    } else {
      stats_.branchesNotTaken++;
    }
    break;
  }

  case OPCODE_JAL: {
    // JAL: rd = PC + 4; PC = PC + imm
    if (instr.writesReg) {
      regs_.writeScalar(instr.rd, pc_ + 4);
    }
    nextPC = pc_ + instr.imm;
    break;
  }

  case OPCODE_JALR: {
    // JALR: rd = PC + 4; PC = (rs1 + imm) & ~1
    if (instr.writesReg) {
      regs_.writeScalar(instr.rd, pc_ + 4);
    }
    nextPC = (rs1Val + instr.imm) & ~1;
    break;
  }

  case OPCODE_LUI: {
    // LUI: rd = imm (already shifted)
    if (instr.writesReg) {
      regs_.writeScalar(instr.rd, instr.imm);
    }
    break;
  }

  case OPCODE_AUIPC: {
    // AUIPC: rd = PC + imm
    if (instr.writesReg) {
      regs_.writeScalar(instr.rd, pc_ + instr.imm);
    }
    break;
  }
  }

  pc_ = nextPC;
}

// =============================================================================
// NPU Execution
// =============================================================================

void SingleCycleCPU::executeNPU(const DecodedInstr &instr) {
  Address nextPC = pc_ + 4;

  switch (instr.npuOp) {
  case NPUOp::VLOAD: {
    // VLOAD Vd, imm(rs1)
    SWord base = regs_.readScalar(instr.rs1);
    Address addr = static_cast<Address>(base + (instr.imm & 0x7FF));
    Vector128 vec = vectorLoad(addr);
    regs_.vector.write(instr.rd, vec);
    stats_.memoryReads++;

    if (verbose_) {
      std::cout << "  VLOAD V" << (int)instr.rd << " <- Mem[" << addrToHex(addr)
                << "] = " << vectorToString(vec) << "\n";
    }
    break;
  }

  case NPUOp::VLBC: {
    // VLBC Vd, imm(rs1) - Broadcast load
    SWord base = regs_.readScalar(instr.rs1);
    Address addr = static_cast<Address>(base + (instr.imm & 0x7FF));
    SWord value = memoryLoad(addr);
    Vector128 vec = NPUUnit::vbroadcast(value);
    regs_.vector.write(instr.rd, vec);
    stats_.memoryReads++;

    if (verbose_) {
      std::cout << "  VLBC V" << (int)instr.rd << " <- broadcast(" << value
                << ") = " << vectorToString(vec) << "\n";
    }
    break;
  }

  case NPUOp::VSTORE: {
    // VSTORE Vs, imm(rs1) - encoded with vs in rs2 field, imm in rd field
    SWord base = regs_.readScalar(instr.rs1);
    SWord offset = instr.rd; // Offset stored in rd field for our encoding
    Address addr = static_cast<Address>(base + offset);
    Vector128 vec = regs_.vector.read(instr.rs2);
    vectorStore(addr, vec);
    stats_.memoryWrites++;

    if (verbose_) {
      std::cout << "  VSTORE V" << (int)instr.rs2 << " -> Mem["
                << addrToHex(addr) << "] = " << vectorToString(vec) << "\n";
    }
    break;
  }

  case NPUOp::VADD:
  case NPUOp::VMUL:
  case NPUOp::VMAC: {
    Vector128 vs1 = regs_.vector.read(instr.rs1);
    Vector128 vs2 = regs_.vector.read(instr.rs2);
    Vector128 vd = regs_.vector.read(instr.rd); // For MAC

    NPUResult result = NPUUnit::execute(instr.npuOp, vs1, vs2, vd, 0);
    regs_.vector.write(instr.rd, result.vectorResult);

    if (verbose_) {
      logVectorOp(instr.mnemonic, instr.rd, result.vectorResult, &vs1, &vs2);
    }
    break;
  }

  case NPUOp::VRELU: {
    Vector128 vs1 = regs_.vector.read(instr.rs1);
    NPUResult result = NPUUnit::execute(NPUOp::VRELU, vs1, {}, {}, 0);
    regs_.vector.write(instr.rd, result.vectorResult);

    if (verbose_) {
      logVectorOp("VRELU", instr.rd, result.vectorResult, &vs1, nullptr);
    }
    break;
  }

  case NPUOp::VCLP: {
    Vector128 vs1 = regs_.vector.read(instr.rs1);
    NPUResult result = NPUUnit::execute(NPUOp::VCLP, vs1, {}, {}, instr.imm);
    regs_.vector.write(instr.rd, result.vectorResult);

    if (verbose_) {
      std::cout << "  VCLP V" << (int)instr.rd << " = clamp(V" << (int)instr.rs1
                << ", " << instr.imm
                << ") = " << vectorToString(result.vectorResult) << "\n";
    }
    break;
  }

  case NPUOp::VREDMAX: {
    Vector128 vs1 = regs_.vector.read(instr.rs1);
    NPUResult result = NPUUnit::execute(NPUOp::VREDMAX, vs1, {}, {}, 0);
    regs_.writeScalar(instr.rd, result.scalarResult);

    if (verbose_) {
      std::cout << "  VREDMAX " << scalarRegName(instr.rd) << " = max(V"
                << (int)instr.rs1 << ") = " << result.scalarResult << "\n";
    }
    break;
  }

  case NPUOp::VARGMAX: {
    Vector128 vs1 = regs_.vector.read(instr.rs1);
    NPUResult result = NPUUnit::execute(NPUOp::VARGMAX, vs1, {}, {}, 0);
    regs_.writeScalar(instr.rd, result.scalarResult);

    if (verbose_) {
      std::cout << "  VARGMAX " << scalarRegName(instr.rd) << " = argmax(V"
                << (int)instr.rs1 << ") = " << result.scalarResult << " (from "
                << vectorToString(vs1) << ")\n";
    }
    break;
  }

  case NPUOp::VCLR: {
    Vector128 zeros = NPUUnit::vclr();
    regs_.vector.write(instr.rd, zeros);

    if (verbose_) {
      std::cout << "  VCLR V" << (int)instr.rd << " = [0, 0, 0, 0]\n";
    }
    break;
  }

  default:
    break;
  }

  pc_ = nextPC;
}

// =============================================================================
// Memory Helpers
// =============================================================================

SWord SingleCycleCPU::memoryLoad(Address addr) {
  return static_cast<SWord>(mem_.readWord(addr));
}

void SingleCycleCPU::memoryStore(Address addr, SWord value) {
  mem_.writeWord(addr, static_cast<Word>(value));
}

Vector128 SingleCycleCPU::vectorLoad(Address addr) {
  return mem_.readVector(addr);
}

void SingleCycleCPU::vectorStore(Address addr, const Vector128 &vec) {
  mem_.writeVector(addr, vec);
}

// =============================================================================
// Logging Helpers
// =============================================================================

void SingleCycleCPU::logInstruction(const DecodedInstr &instr) {
  std::cout << "[Cycle " << std::setw(4) << stats_.cycleCount + 1 << "] "
            << "PC=" << addrToHex(pc_) << " | " << instr.mnemonic;

  if (instr.isNPU) {
    // NPU instruction formatting
    switch (instr.npuOp) {
    case NPUOp::VLOAD:
    case NPUOp::VLBC:
      std::cout << " V" << (int)instr.rd << ", " << (instr.imm & 0x7FF) << "("
                << scalarRegName(instr.rs1) << ")";
      break;
    case NPUOp::VSTORE:
      std::cout << " V" << (int)instr.rs2 << ", " << instr.imm << "("
                << scalarRegName(instr.rs1) << ")";
      break;
    case NPUOp::VADD:
    case NPUOp::VMUL:
    case NPUOp::VMAC:
      std::cout << " V" << (int)instr.rd << ", V" << (int)instr.rs1 << ", V"
                << (int)instr.rs2;
      break;
    case NPUOp::VRELU:
      std::cout << " V" << (int)instr.rd << ", V" << (int)instr.rs1;
      break;
    case NPUOp::VCLP:
      std::cout << " V" << (int)instr.rd << ", V" << (int)instr.rs1 << ", "
                << instr.imm;
      break;
    case NPUOp::VREDMAX:
    case NPUOp::VARGMAX:
      std::cout << " " << scalarRegName(instr.rd) << ", V" << (int)instr.rs1;
      break;
    case NPUOp::VCLR:
      std::cout << " V" << (int)instr.rd;
      break;
    default:
      break;
    }
  } else {
    // RV32I instruction formatting
    switch (instr.type) {
    case InstrType::R_TYPE:
      std::cout << " " << scalarRegName(instr.rd) << ", "
                << scalarRegName(instr.rs1) << ", " << scalarRegName(instr.rs2);
      break;
    case InstrType::I_TYPE:
      if (instr.isLoad) {
        std::cout << " " << scalarRegName(instr.rd) << ", " << instr.imm << "("
                  << scalarRegName(instr.rs1) << ")";
      } else if (instr.isJump) {
        std::cout << " " << scalarRegName(instr.rd) << ", "
                  << scalarRegName(instr.rs1) << ", " << instr.imm;
      } else {
        std::cout << " " << scalarRegName(instr.rd) << ", "
                  << scalarRegName(instr.rs1) << ", " << instr.imm;
      }
      break;
    case InstrType::S_TYPE:
      std::cout << " " << scalarRegName(instr.rs2) << ", " << instr.imm << "("
                << scalarRegName(instr.rs1) << ")";
      break;
    case InstrType::B_TYPE:
      std::cout << " " << scalarRegName(instr.rs1) << ", "
                << scalarRegName(instr.rs2) << ", " << instr.imm;
      break;
    case InstrType::U_TYPE:
    case InstrType::J_TYPE:
      std::cout << " " << scalarRegName(instr.rd) << ", " << instr.imm;
      break;
    default:
      break;
    }
  }

  std::cout << "\n";
}

void SingleCycleCPU::logVectorOp(const std::string &op, uint8_t vd,
                                 const Vector128 &result, const Vector128 *vs1,
                                 const Vector128 *vs2) {
  std::cout << "  " << op << " V" << (int)vd;
  if (vs1)
    std::cout << " | V_src1: " << vectorToString(*vs1);
  if (vs2)
    std::cout << " | V_src2: " << vectorToString(*vs2);
  std::cout << " -> " << vectorToString(result) << "\n";
}

} // namespace npu
