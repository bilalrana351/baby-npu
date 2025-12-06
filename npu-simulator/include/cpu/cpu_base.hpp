#ifndef CPU_BASE_HPP
#define CPU_BASE_HPP

#include "../common.hpp"
#include "../memory.hpp"
#include "../registers.hpp"
#include "../decoder.hpp"
#include "../alu.hpp"
#include "../npu_unit.hpp"

namespace npu {

// Forward declaration
class Logger;

// =============================================================================
// Abstract CPU Interface
// =============================================================================

class CPUBase {
protected:
    RegisterFile regs_;
    Memory& mem_;
    Decoder decoder_;
    Address pc_;
    bool halted_;
    Stats stats_;
    Logger* logger_;
    bool verbose_;
    
public:
    CPUBase(Memory& mem, Logger* logger = nullptr, bool verbose = false)
        : mem_(mem), pc_(TEXT_SEGMENT_START), halted_(false), 
          logger_(logger), verbose_(verbose) {}
    
    virtual ~CPUBase() = default;
    
    // Execute until halt
    virtual void run() = 0;
    
    // Execute one instruction (one cycle for single-cycle, one stage for pipelined)
    virtual void step() = 0;
    
    // Reset CPU state
    virtual void reset() {
        regs_.clear();
        pc_ = TEXT_SEGMENT_START;
        halted_ = false;
        stats_ = Stats();
    }
    
    // Getters
    const Stats& getStats() const { return stats_; }
    const RegisterFile& getRegisters() const { return regs_; }
    RegisterFile& getRegisters() { return regs_; }
    Address getPC() const { return pc_; }
    bool isHalted() const { return halted_; }
    
    // Set PC
    void setPC(Address pc) { pc_ = pc; }
    
    // Initialize data in memory (for test vectors, weights, etc.)
    void initializeData(const std::vector<int32_t>& data, Address startAddr) {
        mem_.loadData(data, startAddr);
    }
};

} // namespace npu

#endif // CPU_BASE_HPP


