#include "../../include/utils/logger.hpp"
#include <iomanip>
#include <sstream>

namespace npu {

// =============================================================================
// Log Cycle
// =============================================================================

void Logger::logCycle(uint64_t cycle, Address pc, const DecodedInstr &instr,
                      const RegisterFile &regs) {
  CycleLogEntry entry;
  entry.cycle = cycle;
  entry.pc = pc;
  entry.instruction = instr.mnemonic;

  // Format operands based on instruction type
  if (instr.isNPU) {
    switch (instr.npuOp) {
    case NPUOp::VLOAD:
    case NPUOp::VLBC:
    case NPUOp::VRELU:
    case NPUOp::VCLP:
    case NPUOp::VCLR:
      entry.rd = "V" + std::to_string(instr.rd);
      entry.rs1 = scalarRegName(instr.rs1);
      entry.rs2 = "-";
      entry.result = vectorToString(regs.vector.read(instr.rd));
      break;
    case NPUOp::VSTORE:
      entry.rd = "-";
      entry.rs1 = scalarRegName(instr.rs1);
      entry.rs2 = "V" + std::to_string(instr.rs2);
      entry.result = "stored";
      break;
    case NPUOp::VADD:
    case NPUOp::VMUL:
    case NPUOp::VMAC:
      entry.rd = "V" + std::to_string(instr.rd);
      entry.rs1 = "V" + std::to_string(instr.rs1);
      entry.rs2 = "V" + std::to_string(instr.rs2);
      entry.result = vectorToString(regs.vector.read(instr.rd));
      break;
    case NPUOp::VREDMAX:
    case NPUOp::VARGMAX:
      entry.rd = scalarRegName(instr.rd);
      entry.rs1 = "V" + std::to_string(instr.rs1);
      entry.rs2 = "-";
      entry.result = std::to_string(regs.scalar.read(instr.rd));
      break;
    default:
      break;
    }
  } else {
    entry.rd = instr.writesReg ? scalarRegName(instr.rd) : "-";
    entry.rs1 = scalarRegName(instr.rs1);
    entry.rs2 =
        (instr.type == InstrType::R_TYPE || instr.type == InstrType::S_TYPE ||
         instr.type == InstrType::B_TYPE)
            ? scalarRegName(instr.rs2)
            : "-";
    entry.result =
        instr.writesReg ? std::to_string(regs.scalar.read(instr.rd)) : "-";
  }

  cycleLog_.push_back(entry);
}

// =============================================================================
// Write CSV
// =============================================================================

void Logger::writeCSV(const std::string &filename) {
  if (!csvEnabled_)
    return;

  std::string fname =
      filename.empty() ? outputDir_ + "/traces/trace_" + getTimestamp() + ".csv"
                       : filename;

  std::ofstream file(fname);
  if (!file.is_open()) {
    std::cerr << "Warning: Could not open CSV file: " << fname << "\n";
    return;
  }

  // Header
  file << "cycle,pc,instruction,rd,rs1,rs2,result\n";

  // Data
  for (const auto &entry : cycleLog_) {
    file << entry.cycle << "," << addrToHex(entry.pc) << ","
         << entry.instruction << "," << entry.rd << "," << entry.rs1 << ","
         << entry.rs2 << ","
         << "\"" << entry.result << "\"\n";
  }

  file.close();
  std::cout << "CSV trace written to: " << fname << "\n";
}

// =============================================================================
// Write JSON
// =============================================================================

void Logger::writeJSON(const std::string &filename, const Stats &stats,
                       const RegisterFile &regs) {
  if (!jsonEnabled_)
    return;

  std::string fname = filename.empty()
                          ? outputDir_ + "/logs/run_" + getTimestamp() + ".json"
                          : filename;

  std::ofstream file(fname);
  if (!file.is_open()) {
    std::cerr << "Warning: Could not open JSON file: " << fname << "\n";
    return;
  }

  file << "{\n";
  file << "  \"program\": \"" << programName_ << "\",\n";
  file << "  \"timestamp\": \"" << getTimestamp() << "\",\n";
  file << "  \"total_cycles\": " << stats.cycleCount << ",\n";
  file << "  \"instruction_count\": " << stats.instructionCount << ",\n";
  file << "  \"cpi\": " << std::fixed << std::setprecision(3) << stats.getCPI()
       << ",\n";
  file << "  \"npu_instructions\": " << stats.npuInstructions << ",\n";
  file << "  \"scalar_instructions\": " << stats.scalarInstructions << ",\n";
  file << "  \"memory_reads\": " << stats.memoryReads << ",\n";
  file << "  \"memory_writes\": " << stats.memoryWrites << ",\n";
  file << "  \"branches_taken\": " << stats.branchesTaken << ",\n";
  file << "  \"branches_not_taken\": " << stats.branchesNotTaken << ",\n";

  // Final scalar registers (non-zero only)
  file << "  \"final_scalar_registers\": {\n";
  bool first = true;
  for (int i = 1; i < 32; ++i) {
    SWord val = regs.scalar.read(i);
    if (val != 0) {
      if (!first)
        file << ",\n";
      file << "    \"" << scalarRegName(i) << "\": " << val;
      first = false;
    }
  }
  file << "\n  },\n";

  // Final vector registers
  file << "  \"final_vector_registers\": {\n";
  for (int i = 0; i < 8; ++i) {
    const Vector128 &vec = regs.vector.read(i);
    file << "    \"V" << i << "\": [" << vec[0] << ", " << vec[1] << ", "
         << vec[2] << ", " << vec[3] << "]";
    if (i < 7)
      file << ",";
    file << "\n";
  }
  file << "  }\n";

  file << "}\n";
  file.close();

  std::cout << "JSON log written to: " << fname << "\n";
}

// =============================================================================
// Print Statistics
// =============================================================================

void Logger::printStats(const Stats &stats, std::ostream &os) {
  os << "\n";
  os << "============================================\n";
  os << "          EXECUTION STATISTICS              \n";
  os << "============================================\n";
  os << "Total Cycles:        " << std::setw(10) << stats.cycleCount << "\n";
  os << "Instructions:        " << std::setw(10) << stats.instructionCount
     << "\n";
  os << "CPI:                 " << std::setw(10) << std::fixed
     << std::setprecision(3) << stats.getCPI() << "\n";
  os << "--------------------------------------------\n";
  os << "NPU Instructions:    " << std::setw(10) << stats.npuInstructions
     << "\n";
  os << "Scalar Instructions: " << std::setw(10) << stats.scalarInstructions
     << "\n";
  os << "Memory Reads:        " << std::setw(10) << stats.memoryReads << "\n";
  os << "Memory Writes:       " << std::setw(10) << stats.memoryWrites << "\n";
  os << "Branches Taken:      " << std::setw(10) << stats.branchesTaken << "\n";
  os << "Branches Not Taken:  " << std::setw(10) << stats.branchesNotTaken
     << "\n";
  os << "============================================\n";
}

// =============================================================================
// Print Registers
// =============================================================================

void Logger::printRegisters(const RegisterFile &regs, std::ostream &os) {
  os << "\n";
  os << "============================================\n";
  os << "           FINAL REGISTER STATE             \n";
  os << "============================================\n";

  os << "Scalar Registers (non-zero):\n";
  for (int i = 1; i < 32; ++i) {
    SWord val = regs.scalar.read(i);
    if (val != 0) {
      os << "  " << std::setw(4) << scalarRegName(i) << " = " << std::setw(10)
         << val << " (" << wordToHex(static_cast<Word>(val)) << ")\n";
    }
  }

  os << "\nVector Registers:\n";
  for (int i = 0; i < 8; ++i) {
    const Vector128 &vec = regs.vector.read(i);
    bool isZero = (vec[0] == 0 && vec[1] == 0 && vec[2] == 0 && vec[3] == 0);
    if (!isZero) {
      os << "  V" << i << " = " << vectorToString(vec) << "\n";
    }
  }
  os << "============================================\n";
}

// =============================================================================
// Get Timestamp
// =============================================================================

std::string Logger::getTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm *tm = std::localtime(&time);

  std::ostringstream oss;
  oss << std::put_time(tm, "%Y-%m-%d_%H-%M-%S");
  return oss.str();
}

} // namespace npu
