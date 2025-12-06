#ifndef SINGLE_CYCLE_HPP
#define SINGLE_CYCLE_HPP

#include "cpu_base.hpp"
#include "../utils/logger.hpp"

namespace npu {

// =============================================================================
// Single-Cycle CPU Implementation
// =============================================================================

class SingleCycleCPU : public CPUBase {
private:
    // Maximum instructions to prevent infinite loops
    static constexpr uint64_t MAX_INSTRUCTIONS = 1000000;
    
public:
    SingleCycleCPU(Memory& mem, Logger* logger = nullptr, bool verbose = false)
        : CPUBase(mem, logger, verbose) {}
    
    // Execute until halt or max instructions
    void run() override;
    
    // Execute one instruction
    void step() override;
    
private:
    // Execute stages (all in one cycle for single-cycle)
    Word fetch();
    DecodedInstr decode(Word instruction);
    void execute(const DecodedInstr& instr);
    
    // Execute RV32I instruction
    void executeRV32I(const DecodedInstr& instr);
    
    // Execute NPU instruction
    void executeNPU(const DecodedInstr& instr);
    
    // Memory stage helpers
    SWord memoryLoad(Address addr);
    void memoryStore(Address addr, SWord value);
    Vector128 vectorLoad(Address addr);
    void vectorStore(Address addr, const Vector128& vec);
    
    // Logging helpers
    void logInstruction(const DecodedInstr& instr);
    void logVectorOp(const std::string& op, uint8_t vd, 
                     const Vector128& result,
                     const Vector128* vs1 = nullptr,
                     const Vector128* vs2 = nullptr);
};

} // namespace npu

#endif // SINGLE_CYCLE_HPP


