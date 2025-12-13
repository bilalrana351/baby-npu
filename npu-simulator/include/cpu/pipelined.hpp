#ifndef PIPELINED_HPP
#define PIPELINED_HPP

#include "cpu_base.hpp"
#include "../utils/logger.hpp"

namespace npu {

// =============================================================================
// Pipeline Register Structures
// =============================================================================

// IF/ID Pipeline Register - holds fetched instruction
struct IF_ID_Reg {
    Word instruction;       // Raw 32-bit instruction
    Address pc;             // PC of this instruction
    Address nextPC;         // PC + 4 (predicted next)
    bool valid;             // Is this a valid instruction (not a bubble)
    
    IF_ID_Reg() : instruction(0), pc(0), nextPC(0), valid(false) {}
    
    void clear() {
        instruction = 0;
        pc = 0;
        nextPC = 0;
        valid = false;
    }
};

// ID/EX Pipeline Register - holds decoded instruction and register values
struct ID_EX_Reg {
    DecodedInstr decoded;   // Decoded instruction info
    SWord rs1Val;           // Value read from rs1
    SWord rs2Val;           // Value read from rs2
    Vector128 vs1Val;       // Vector value from vs1 (for NPU)
    Vector128 vs2Val;       // Vector value from vs2 (for NPU)
    Vector128 vdVal;        // Vector value from vd (for MAC accumulate)
    Address pc;             // PC of this instruction
    bool valid;             // Is this a valid instruction
    
    ID_EX_Reg() : rs1Val(0), rs2Val(0), pc(0), valid(false) {
        vs1Val.fill(0);
        vs2Val.fill(0);
        vdVal.fill(0);
    }
    
    void clear() {
        decoded = DecodedInstr();
        rs1Val = 0;
        rs2Val = 0;
        vs1Val.fill(0);
        vs2Val.fill(0);
        vdVal.fill(0);
        pc = 0;
        valid = false;
    }
};

// EX/MEM Pipeline Register - holds execution results
struct EX_MEM_Reg {
    DecodedInstr decoded;   // Decoded instruction info
    SWord aluResult;        // Result from ALU
    SWord rs2Val;           // rs2 value for stores
    Vector128 vecResult;    // Result from NPU unit
    Vector128 vs2Val;       // Vector value for stores
    Address memAddr;        // Calculated memory address
    Address pc;             // PC of this instruction
    bool branchTaken;       // Was branch taken?
    Address branchTarget;   // Branch target address
    bool valid;             // Is this a valid instruction
    
    EX_MEM_Reg() : aluResult(0), rs2Val(0), memAddr(0), pc(0), 
                   branchTaken(false), branchTarget(0), valid(false) {
        vecResult.fill(0);
        vs2Val.fill(0);
    }
    
    void clear() {
        decoded = DecodedInstr();
        aluResult = 0;
        rs2Val = 0;
        vecResult.fill(0);
        vs2Val.fill(0);
        memAddr = 0;
        pc = 0;
        branchTaken = false;
        branchTarget = 0;
        valid = false;
    }
};

// MEM/WB Pipeline Register - holds memory data and final results
struct MEM_WB_Reg {
    DecodedInstr decoded;   // Decoded instruction info
    SWord memData;          // Data read from memory
    SWord aluResult;        // ALU result (passed through)
    Vector128 vecData;      // Vector data from memory
    Vector128 vecResult;    // Vector result (passed through)
    Address pc;             // PC of this instruction
    bool valid;             // Is this a valid instruction
    
    MEM_WB_Reg() : memData(0), aluResult(0), pc(0), valid(false) {
        vecData.fill(0);
        vecResult.fill(0);
    }
    
    void clear() {
        decoded = DecodedInstr();
        memData = 0;
        aluResult = 0;
        vecData.fill(0);
        vecResult.fill(0);
        pc = 0;
        valid = false;
    }
};

// =============================================================================
// Forwarding Source Enum
// =============================================================================

enum class ForwardSrc {
    NONE,       // No forwarding needed, use register file value
    EX_MEM,     // Forward from EX/MEM pipeline register
    MEM_WB      // Forward from MEM/WB pipeline register
};

// =============================================================================
// Pipeline Statistics (extends base Stats)
// =============================================================================

struct PipelineStats {
    uint64_t stallCycles;       // Total stall cycles
    uint64_t flushCycles;       // Total flush cycles (bubbles from branches)
    uint64_t forwardCount;      // Number of forwarding events
    uint64_t loadUseStalls;     // Stalls due to load-use hazards
    uint64_t branchFlushes;     // Flushes due to branch mispredictions
    uint64_t dataHazards;       // Total data hazards detected
    uint64_t controlHazards;    // Total control hazards detected
    
    PipelineStats() : stallCycles(0), flushCycles(0), forwardCount(0),
                      loadUseStalls(0), branchFlushes(0), 
                      dataHazards(0), controlHazards(0) {}
};

// =============================================================================
// Pipelined CPU Implementation
// =============================================================================

class PipelinedCPU : public CPUBase {
private:
    // Maximum instructions to prevent infinite loops
    static constexpr uint64_t MAX_CYCLES = 10000000;
    
    // Pipeline registers (current state)
    IF_ID_Reg if_id_;
    ID_EX_Reg id_ex_;
    EX_MEM_Reg ex_mem_;
    MEM_WB_Reg mem_wb_;
    
    // Pipeline registers (next state - written during cycle)
    IF_ID_Reg if_id_next_;
    ID_EX_Reg id_ex_next_;
    EX_MEM_Reg ex_mem_next_;
    MEM_WB_Reg mem_wb_next_;
    
    // Pipeline control
    bool stall_;                // Stall the pipeline (IF and ID stages)
    bool flush_;                // Flush IF/ID register (branch taken)
    
    // Pipeline-specific statistics
    PipelineStats pipeStats_;
    
    // Track instructions completed (for proper termination)
    uint64_t instructionsRetired_;
    uint64_t instructionsFetched_;
    
public:
    PipelinedCPU(Memory& mem, Logger* logger = nullptr, bool verbose = false)
        : CPUBase(mem, logger, verbose), 
          stall_(false), flush_(false),
          instructionsRetired_(0), instructionsFetched_(0) {
        stats_.isPipelined = true;
        stats_.clockPeriodNs = PIPELINED_CLOCK_PERIOD_NS;
    }
    
    // Execute until halt or max cycles
    void run() override;
    
    // Execute one pipeline cycle
    void step() override;
    
    // Get pipeline-specific statistics
    const PipelineStats& getPipelineStats() const { return pipeStats_; }
    
    // Reset pipeline state
    void reset() override {
        CPUBase::reset();
        if_id_.clear();
        id_ex_.clear();
        ex_mem_.clear();
        mem_wb_.clear();
        if_id_next_.clear();
        id_ex_next_.clear();
        ex_mem_next_.clear();
        mem_wb_next_.clear();
        stall_ = false;
        flush_ = false;
        pipeStats_ = PipelineStats();
        instructionsRetired_ = 0;
        instructionsFetched_ = 0;
    }
    
private:
    // Pipeline stages (executed in reverse order for correct semantics)
    void stageWriteBack();
    void stageMemory();
    void stageExecute();
    void stageDecode();
    void stageFetch();
    
    // Advance pipeline registers at end of cycle
    void advancePipeline();
    
    // Hazard detection
    bool detectLoadUseHazard();
    bool detectControlHazard();
    
    // Forwarding logic
    ForwardSrc getForwardA();
    ForwardSrc getForwardB();
    ForwardSrc getVectorForwardVs1();
    ForwardSrc getVectorForwardVs2();
    ForwardSrc getVectorForwardVd();
    
    // Apply forwarding to get actual operand values
    SWord getForwardedRs1();
    SWord getForwardedRs2();
    Vector128 getForwardedVs1();
    Vector128 getForwardedVs2();
    Vector128 getForwardedVd();
    
    // Execute RV32I instruction in EX stage
    void executeRV32I();
    
    // Execute NPU instruction in EX stage
    void executeNPU();
    
    // Memory access helpers
    SWord memoryLoad(Address addr);
    void memoryStore(Address addr, SWord value);
    Vector128 vectorLoad(Address addr);
    void vectorStore(Address addr, const Vector128& vec);
    
    // Logging helpers
    void logPipelineState();
    void logStageInfo(const std::string& stage, const DecodedInstr& instr, 
                      Address pc, bool valid);
};

} // namespace npu

#endif // PIPELINED_HPP

