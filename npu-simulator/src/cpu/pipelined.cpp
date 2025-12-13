#include "../../include/cpu/pipelined.hpp"
#include <iomanip>
#include <iostream>

namespace npu {

// =============================================================================
// Run Loop
// =============================================================================

void PipelinedCPU::run() {
    while (!halted_ && stats_.cycleCount < MAX_CYCLES) {
        step();
    }
    
    if (stats_.cycleCount >= MAX_CYCLES) {
        std::cerr << "Warning: Execution stopped after " << MAX_CYCLES
                  << " cycles (possible infinite loop)\n";
    }
}

// =============================================================================
// Single Pipeline Cycle
// =============================================================================

void PipelinedCPU::step() {
    if (halted_) return;
    
    // Execute stages in reverse order (WB → MEM → EX → ID → IF)
    // This ensures proper data flow and hazard handling
    stageWriteBack();
    stageMemory();
    stageExecute();
    stageDecode();
    stageFetch();
    
    // Log pipeline state if verbose
    if (verbose_) {
        logPipelineState();
    }
    
    // Advance all pipeline registers
    advancePipeline();
    
    // Increment cycle count
    stats_.cycleCount++;
}

// =============================================================================
// IF Stage - Instruction Fetch
// =============================================================================

void PipelinedCPU::stageFetch() {
    // Check if we should stall (don't fetch new instruction)
    if (stall_) {
        // Insert bubble in IF/ID - don't update if_id_next_
        return;
    }
    
    // Check if we should flush (branch taken)
    if (flush_) {
        // Insert bubble in IF/ID
        if_id_next_.clear();
        flush_ = false;  // Clear flush signal
        return;
    }
    
    // Fetch instruction from memory
    try {
        if_id_next_.instruction = mem_.readWord(pc_);
        if_id_next_.pc = pc_;
        if_id_next_.nextPC = pc_ + 4;
        if_id_next_.valid = true;
        
        // Update PC for next fetch
        pc_ += 4;
        instructionsFetched_++;
        
    } catch (const std::exception& e) {
        // Memory access error - treat as NOP
        if_id_next_.clear();
    }
}

// =============================================================================
// ID Stage - Instruction Decode
// =============================================================================

void PipelinedCPU::stageDecode() {
    // Clear next state
    id_ex_next_.clear();
    
    // Check if IF/ID contains a valid instruction
    if (!if_id_.valid) {
        return;
    }
    
    // Decode the instruction
    id_ex_next_.decoded = decoder_.decode(if_id_.instruction);
    id_ex_next_.pc = if_id_.pc;
    id_ex_next_.valid = true;
    
    const DecodedInstr& instr = id_ex_next_.decoded;
    
    // Read scalar register values
    id_ex_next_.rs1Val = regs_.readScalar(instr.rs1);
    id_ex_next_.rs2Val = regs_.readScalar(instr.rs2);
    
    // Read vector register values (for NPU instructions)
    if (instr.isNPU) {
        // Only read vector registers if they're actually vector operations
        // (not memory operations that use scalar base addresses)
        if (instr.npuOp != NPUOp::VLOAD && instr.npuOp != NPUOp::VLBC && 
            instr.npuOp != NPUOp::VSTORE && instr.npuOp != NPUOp::VCLR) {
            // These operations use vector registers
            if (instr.rs1 < NUM_VECTOR_REGS) {
                id_ex_next_.vs1Val = regs_.vector.read(instr.rs1);
            }
            if (instr.rs2 < NUM_VECTOR_REGS) {
                id_ex_next_.vs2Val = regs_.vector.read(instr.rs2);
            }
            if (instr.rd < NUM_VECTOR_REGS) {
                id_ex_next_.vdVal = regs_.vector.read(instr.rd);
            }
        }
    }
    
    // Detect hazards
    if (detectLoadUseHazard()) {
        // Load-use hazard detected - stall the pipeline
        stall_ = true;
        pipeStats_.loadUseStalls++;
        pipeStats_.stallCycles++;
        pipeStats_.dataHazards++;
        
        // Insert bubble in ID/EX
        id_ex_next_.clear();
        
        // Keep IF/ID unchanged (don't advance PC)
        pc_ = if_id_.pc;  // Roll back PC
        
        if (verbose_) {
            std::cout << "  [STALL] Load-use hazard detected\n";
        }
    } else {
        stall_ = false;
    }
    
    // Update instruction count
    if (id_ex_next_.valid && !stall_) {
        stats_.instructionCount++;
        if (instr.isNPU) {
            stats_.npuInstructions++;
        } else {
            stats_.scalarInstructions++;
        }
    }
}

// =============================================================================
// EX Stage - Execute
// =============================================================================

void PipelinedCPU::stageExecute() {
    // Clear next state
    ex_mem_next_.clear();
    
    // Check if ID/EX contains a valid instruction
    if (!id_ex_.valid) {
        return;
    }
    
    // Copy decoded instruction info
    ex_mem_next_.decoded = id_ex_.decoded;
    ex_mem_next_.pc = id_ex_.pc;
    ex_mem_next_.valid = true;
    
    const DecodedInstr& instr = id_ex_.decoded;
    
    // Handle HALT
    if (instr.isHalt) {
        halted_ = true;
        return;
    }
    
    // Execute based on instruction type
    if (instr.isNPU) {
        executeNPU();
    } else {
        executeRV32I();
    }
}

// =============================================================================
// EX Stage - RV32I Execution
// =============================================================================

void PipelinedCPU::executeRV32I() {
    const DecodedInstr& instr = id_ex_.decoded;
    
    // Get forwarded operands
    SWord rs1Val = getForwardedRs1();
    SWord rs2Val = getForwardedRs2();
    
    // Pass through rs2 value for stores
    ex_mem_next_.rs2Val = rs2Val;
    
    switch (instr.opcode) {
    case OPCODE_OP: {
        // R-type: rd = rs1 op rs2
        ALUResult result = ALU::execute(instr.aluOp, rs1Val, rs2Val);
        ex_mem_next_.aluResult = result.result;
        break;
    }
    
    case OPCODE_OP_IMM: {
        // I-type: rd = rs1 op imm
        SWord immVal = instr.imm;
        if (instr.aluOp == ALUOp::SLL || instr.aluOp == ALUOp::SRL ||
            instr.aluOp == ALUOp::SRA) {
            immVal &= 0x1F;
        }
        ALUResult result = ALU::execute(instr.aluOp, rs1Val, immVal);
        ex_mem_next_.aluResult = result.result;
        break;
    }
    
    case OPCODE_LOAD: {
        // LW: calculate address
        ex_mem_next_.memAddr = static_cast<Address>(rs1Val + instr.imm);
        break;
    }
    
    case OPCODE_STORE: {
        // SW: calculate address
        ex_mem_next_.memAddr = static_cast<Address>(rs1Val + instr.imm);
        break;
    }
    
    case OPCODE_BRANCH: {
        // Branch: evaluate condition
        bool taken = ALU::evaluateBranch(instr.branchOp, rs1Val, rs2Val);
        ex_mem_next_.branchTaken = taken;
        if (taken) {
            ex_mem_next_.branchTarget = id_ex_.pc + instr.imm;
            // Signal to flush IF/ID stage
            flush_ = true;
            pipeStats_.branchFlushes++;
            pipeStats_.flushCycles++;
            pipeStats_.controlHazards++;
            stats_.branchesTaken++;
        } else {
            stats_.branchesNotTaken++;
        }
        break;
    }
    
    case OPCODE_JAL: {
        // JAL: save return address, calculate target
        ex_mem_next_.aluResult = id_ex_.pc + 4;
        ex_mem_next_.branchTaken = true;
        ex_mem_next_.branchTarget = id_ex_.pc + instr.imm;
        flush_ = true;
        pipeStats_.branchFlushes++;
        pipeStats_.flushCycles++;
        pipeStats_.controlHazards++;
        break;
    }
    
    case OPCODE_JALR: {
        // JALR: save return address, calculate target
        ex_mem_next_.aluResult = id_ex_.pc + 4;
        ex_mem_next_.branchTaken = true;
        ex_mem_next_.branchTarget = (rs1Val + instr.imm) & ~1;
        flush_ = true;
        pipeStats_.branchFlushes++;
        pipeStats_.flushCycles++;
        pipeStats_.controlHazards++;
        break;
    }
    
    case OPCODE_LUI: {
        // LUI: rd = imm
        ex_mem_next_.aluResult = instr.imm;
        break;
    }
    
    case OPCODE_AUIPC: {
        // AUIPC: rd = PC + imm
        ex_mem_next_.aluResult = id_ex_.pc + instr.imm;
        break;
    }
    }
    
    // Update PC if branch/jump taken
    if (ex_mem_next_.branchTaken) {
        pc_ = ex_mem_next_.branchTarget;
    }
}

// =============================================================================
// EX Stage - NPU Execution
// =============================================================================

void PipelinedCPU::executeNPU() {
    const DecodedInstr& instr = id_ex_.decoded;
    
    // Get forwarded operands
    SWord rs1Val = getForwardedRs1();
    Vector128 vs1Val = getForwardedVs1();
    Vector128 vs2Val = getForwardedVs2();
    Vector128 vdVal = getForwardedVd();
    
    switch (instr.npuOp) {
    case NPUOp::VLOAD: {
        // VLOAD: calculate address
        SWord base = rs1Val;
        ex_mem_next_.memAddr = static_cast<Address>(base + (instr.imm & 0x7FF));
        break;
    }
    
    case NPUOp::VLBC: {
        // VLBC: calculate address for broadcast load
        SWord base = rs1Val;
        ex_mem_next_.memAddr = static_cast<Address>(base + (instr.imm & 0x7FF));
        break;
    }
    
    case NPUOp::VSTORE: {
        // VSTORE: calculate address
        SWord base = rs1Val;
        SWord offset = instr.rd;
        ex_mem_next_.memAddr = static_cast<Address>(base + offset);
        ex_mem_next_.vs2Val = regs_.vector.read(instr.rs2);  // Vector to store
        break;
    }
    
    case NPUOp::VADD:
    case NPUOp::VMUL:
    case NPUOp::VMAC: {
        NPUResult result = NPUUnit::execute(instr.npuOp, vs1Val, vs2Val, vdVal, 0);
        ex_mem_next_.vecResult = result.vectorResult;
        break;
    }
    
    case NPUOp::VRELU: {
        NPUResult result = NPUUnit::execute(NPUOp::VRELU, vs1Val, {}, {}, 0);
        ex_mem_next_.vecResult = result.vectorResult;
        break;
    }
    
    case NPUOp::VCLP: {
        NPUResult result = NPUUnit::execute(NPUOp::VCLP, vs1Val, {}, {}, instr.imm);
        ex_mem_next_.vecResult = result.vectorResult;
        break;
    }
    
    case NPUOp::VREDMAX: {
        NPUResult result = NPUUnit::execute(NPUOp::VREDMAX, vs1Val, {}, {}, 0);
        ex_mem_next_.aluResult = result.scalarResult;
        break;
    }
    
    case NPUOp::VARGMAX: {
        NPUResult result = NPUUnit::execute(NPUOp::VARGMAX, vs1Val, {}, {}, 0);
        ex_mem_next_.aluResult = result.scalarResult;
        break;
    }
    
    case NPUOp::VCLR: {
        Vector128 zeros = NPUUnit::vclr();
        ex_mem_next_.vecResult = zeros;
        break;
    }
    
    default:
        break;
    }
}

// =============================================================================
// MEM Stage - Memory Access
// =============================================================================

void PipelinedCPU::stageMemory() {
    // Clear next state
    mem_wb_next_.clear();
    
    // Check if EX/MEM contains a valid instruction
    if (!ex_mem_.valid) {
        return;
    }
    
    // Copy instruction info
    mem_wb_next_.decoded = ex_mem_.decoded;
    mem_wb_next_.aluResult = ex_mem_.aluResult;
    mem_wb_next_.vecResult = ex_mem_.vecResult;
    mem_wb_next_.pc = ex_mem_.pc;
    mem_wb_next_.valid = true;
    
    const DecodedInstr& instr = ex_mem_.decoded;
    
    // Handle memory operations
    if (instr.isLoad) {
        if (instr.isNPU && (instr.npuOp == NPUOp::VLOAD || instr.npuOp == NPUOp::VLBC)) {
            // Vector load
            if (instr.npuOp == NPUOp::VLOAD) {
                mem_wb_next_.vecData = vectorLoad(ex_mem_.memAddr);
            } else {  // VLBC
                SWord value = memoryLoad(ex_mem_.memAddr);
                mem_wb_next_.vecData = NPUUnit::vbroadcast(value);
            }
            stats_.memoryReads++;
        } else {
            // Scalar load
            mem_wb_next_.memData = memoryLoad(ex_mem_.memAddr);
            stats_.memoryReads++;
        }
    } else if (instr.isStore) {
        if (instr.isNPU && instr.npuOp == NPUOp::VSTORE) {
            // Vector store
            vectorStore(ex_mem_.memAddr, ex_mem_.vs2Val);
            stats_.memoryWrites++;
        } else {
            // Scalar store
            memoryStore(ex_mem_.memAddr, ex_mem_.rs2Val);
            stats_.memoryWrites++;
        }
    }
}

// =============================================================================
// WB Stage - Write Back
// =============================================================================

void PipelinedCPU::stageWriteBack() {
    // Check if MEM/WB contains a valid instruction
    if (!mem_wb_.valid) {
        return;
    }
    
    const DecodedInstr& instr = mem_wb_.decoded;
    
    // Count retired instruction
    instructionsRetired_++;
    
    // Write back to register file
    if (instr.writesReg && !instr.isNPU) {
        // Scalar write
        if (instr.isLoad) {
            regs_.writeScalar(instr.rd, mem_wb_.memData);
        } else {
            regs_.writeScalar(instr.rd, mem_wb_.aluResult);
        }
    } else if (instr.isNPU) {
        // Vector write
        if (instr.npuOp == NPUOp::VLOAD || instr.npuOp == NPUOp::VLBC) {
            regs_.vector.write(instr.rd, mem_wb_.vecData);
        } else if (instr.npuOp == NPUOp::VREDMAX || instr.npuOp == NPUOp::VARGMAX) {
            // Vector reduction to scalar
            regs_.writeScalar(instr.rd, mem_wb_.aluResult);
        } else if (instr.npuOp != NPUOp::VSTORE) {
            // Other vector operations
            regs_.vector.write(instr.rd, mem_wb_.vecResult);
        }
    }
    
    // Log to logger if available
    if (logger_) {
        logger_->logCycle(stats_.cycleCount, mem_wb_.pc, instr, regs_);
    }
}

// =============================================================================
// Pipeline Advancement
// =============================================================================

void PipelinedCPU::advancePipeline() {
    // Copy next state to current state for all pipeline registers
    if_id_ = if_id_next_;
    id_ex_ = id_ex_next_;
    ex_mem_ = ex_mem_next_;
    mem_wb_ = mem_wb_next_;
}

// =============================================================================
// Hazard Detection
// =============================================================================

bool PipelinedCPU::detectLoadUseHazard() {
    // Check if instruction in ID/EX is a load
    if (!id_ex_.valid) return false;
    
    const DecodedInstr& ex_instr = id_ex_.decoded;
    const DecodedInstr& id_instr = id_ex_next_.decoded;
    
    bool isLoad = ex_instr.isLoad;
    
    if (!isLoad || !id_instr.writesReg) return false;
    
    // Check for scalar load-use hazard
    if (!ex_instr.isNPU && !id_instr.isNPU) {
        uint8_t load_rd = ex_instr.rd;
        if ((id_instr.rs1 == load_rd && id_instr.rs1 != 0) ||
            (id_instr.rs2 == load_rd && id_instr.rs2 != 0)) {
            return true;
        }
    }
    
    // Check for vector load-use hazard
    if (ex_instr.isNPU && id_instr.isNPU) {
        uint8_t load_vd = ex_instr.rd;
        if ((id_instr.rs1 == load_vd) || (id_instr.rs2 == load_vd)) {
            return true;
        }
    }
    
    return false;
}

// =============================================================================
// Forwarding Logic - Determine Forward Source
// =============================================================================

ForwardSrc PipelinedCPU::getForwardA() {
    if (!id_ex_.valid) return ForwardSrc::NONE;
    
    uint8_t rs1 = id_ex_.decoded.rs1;
    if (rs1 == 0) return ForwardSrc::NONE;  // x0 never forwarded
    
    // Check EX/MEM stage (most recent)
    if (ex_mem_.valid && ex_mem_.decoded.writesReg && !ex_mem_.decoded.isNPU) {
        if (ex_mem_.decoded.rd == rs1) {
            pipeStats_.forwardCount++;
            return ForwardSrc::EX_MEM;
        }
    }
    
    // Check MEM/WB stage
    if (mem_wb_.valid && mem_wb_.decoded.writesReg && !mem_wb_.decoded.isNPU) {
        if (mem_wb_.decoded.rd == rs1) {
            pipeStats_.forwardCount++;
            return ForwardSrc::MEM_WB;
        }
    }
    
    return ForwardSrc::NONE;
}

ForwardSrc PipelinedCPU::getForwardB() {
    if (!id_ex_.valid) return ForwardSrc::NONE;
    
    uint8_t rs2 = id_ex_.decoded.rs2;
    if (rs2 == 0) return ForwardSrc::NONE;
    
    // Check EX/MEM stage
    if (ex_mem_.valid && ex_mem_.decoded.writesReg && !ex_mem_.decoded.isNPU) {
        if (ex_mem_.decoded.rd == rs2) {
            pipeStats_.forwardCount++;
            return ForwardSrc::EX_MEM;
        }
    }
    
    // Check MEM/WB stage
    if (mem_wb_.valid && mem_wb_.decoded.writesReg && !mem_wb_.decoded.isNPU) {
        if (mem_wb_.decoded.rd == rs2) {
            pipeStats_.forwardCount++;
            return ForwardSrc::MEM_WB;
        }
    }
    
    return ForwardSrc::NONE;
}

ForwardSrc PipelinedCPU::getVectorForwardVs1() {
    if (!id_ex_.valid || !id_ex_.decoded.isNPU) return ForwardSrc::NONE;
    
    uint8_t vs1 = id_ex_.decoded.rs1;
    
    // Check EX/MEM stage
    if (ex_mem_.valid && ex_mem_.decoded.isNPU && 
        ex_mem_.decoded.npuOp != NPUOp::VSTORE) {
        if (ex_mem_.decoded.rd == vs1) {
            pipeStats_.forwardCount++;
            return ForwardSrc::EX_MEM;
        }
    }
    
    // Check MEM/WB stage
    if (mem_wb_.valid && mem_wb_.decoded.isNPU &&
        mem_wb_.decoded.npuOp != NPUOp::VSTORE) {
        if (mem_wb_.decoded.rd == vs1) {
            pipeStats_.forwardCount++;
            return ForwardSrc::MEM_WB;
        }
    }
    
    return ForwardSrc::NONE;
}

ForwardSrc PipelinedCPU::getVectorForwardVs2() {
    if (!id_ex_.valid || !id_ex_.decoded.isNPU) return ForwardSrc::NONE;
    
    uint8_t vs2 = id_ex_.decoded.rs2;
    
    // Check EX/MEM stage
    if (ex_mem_.valid && ex_mem_.decoded.isNPU &&
        ex_mem_.decoded.npuOp != NPUOp::VSTORE) {
        if (ex_mem_.decoded.rd == vs2) {
            pipeStats_.forwardCount++;
            return ForwardSrc::EX_MEM;
        }
    }
    
    // Check MEM/WB stage
    if (mem_wb_.valid && mem_wb_.decoded.isNPU &&
        mem_wb_.decoded.npuOp != NPUOp::VSTORE) {
        if (mem_wb_.decoded.rd == vs2) {
            pipeStats_.forwardCount++;
            return ForwardSrc::MEM_WB;
        }
    }
    
    return ForwardSrc::NONE;
}

ForwardSrc PipelinedCPU::getVectorForwardVd() {
    if (!id_ex_.valid || !id_ex_.decoded.isNPU) return ForwardSrc::NONE;
    
    uint8_t vd = id_ex_.decoded.rd;
    
    // Only for MAC which needs current accumulator value
    if (id_ex_.decoded.npuOp != NPUOp::VMAC) return ForwardSrc::NONE;
    
    // Check EX/MEM stage
    if (ex_mem_.valid && ex_mem_.decoded.isNPU &&
        ex_mem_.decoded.npuOp != NPUOp::VSTORE) {
        if (ex_mem_.decoded.rd == vd) {
            pipeStats_.forwardCount++;
            return ForwardSrc::EX_MEM;
        }
    }
    
    // Check MEM/WB stage
    if (mem_wb_.valid && mem_wb_.decoded.isNPU &&
        mem_wb_.decoded.npuOp != NPUOp::VSTORE) {
        if (mem_wb_.decoded.rd == vd) {
            pipeStats_.forwardCount++;
            return ForwardSrc::MEM_WB;
        }
    }
    
    return ForwardSrc::NONE;
}

// =============================================================================
// Forwarding Logic - Get Forwarded Values
// =============================================================================

SWord PipelinedCPU::getForwardedRs1() {
    ForwardSrc fwd = getForwardA();
    
    switch (fwd) {
    case ForwardSrc::EX_MEM:
        return ex_mem_.aluResult;
    case ForwardSrc::MEM_WB:
        return mem_wb_.decoded.isLoad ? mem_wb_.memData : mem_wb_.aluResult;
    case ForwardSrc::NONE:
    default:
        return id_ex_.rs1Val;
    }
}

SWord PipelinedCPU::getForwardedRs2() {
    ForwardSrc fwd = getForwardB();
    
    switch (fwd) {
    case ForwardSrc::EX_MEM:
        return ex_mem_.aluResult;
    case ForwardSrc::MEM_WB:
        return mem_wb_.decoded.isLoad ? mem_wb_.memData : mem_wb_.aluResult;
    case ForwardSrc::NONE:
    default:
        return id_ex_.rs2Val;
    }
}

Vector128 PipelinedCPU::getForwardedVs1() {
    ForwardSrc fwd = getVectorForwardVs1();
    
    switch (fwd) {
    case ForwardSrc::EX_MEM:
        return ex_mem_.vecResult;
    case ForwardSrc::MEM_WB:
        return (mem_wb_.decoded.npuOp == NPUOp::VLOAD || 
                mem_wb_.decoded.npuOp == NPUOp::VLBC) ? 
                mem_wb_.vecData : mem_wb_.vecResult;
    case ForwardSrc::NONE:
    default:
        return id_ex_.vs1Val;
    }
}

Vector128 PipelinedCPU::getForwardedVs2() {
    ForwardSrc fwd = getVectorForwardVs2();
    
    switch (fwd) {
    case ForwardSrc::EX_MEM:
        return ex_mem_.vecResult;
    case ForwardSrc::MEM_WB:
        return (mem_wb_.decoded.npuOp == NPUOp::VLOAD || 
                mem_wb_.decoded.npuOp == NPUOp::VLBC) ? 
                mem_wb_.vecData : mem_wb_.vecResult;
    case ForwardSrc::NONE:
    default:
        return id_ex_.vs2Val;
    }
}

Vector128 PipelinedCPU::getForwardedVd() {
    ForwardSrc fwd = getVectorForwardVd();
    
    switch (fwd) {
    case ForwardSrc::EX_MEM:
        return ex_mem_.vecResult;
    case ForwardSrc::MEM_WB:
        return (mem_wb_.decoded.npuOp == NPUOp::VLOAD || 
                mem_wb_.decoded.npuOp == NPUOp::VLBC) ? 
                mem_wb_.vecData : mem_wb_.vecResult;
    case ForwardSrc::NONE:
    default:
        return id_ex_.vdVal;
    }
}

// =============================================================================
// Memory Helpers
// =============================================================================

SWord PipelinedCPU::memoryLoad(Address addr) {
    return static_cast<SWord>(mem_.readWord(addr));
}

void PipelinedCPU::memoryStore(Address addr, SWord value) {
    mem_.writeWord(addr, static_cast<Word>(value));
}

Vector128 PipelinedCPU::vectorLoad(Address addr) {
    return mem_.readVector(addr);
}

void PipelinedCPU::vectorStore(Address addr, const Vector128& vec) {
    mem_.writeVector(addr, vec);
}

// =============================================================================
// Logging Helpers
// =============================================================================

void PipelinedCPU::logPipelineState() {
    std::cout << "\n[Cycle " << std::setw(4) << stats_.cycleCount << "] Pipeline State:\n";
    std::cout << "  IF: ";
    logStageInfo("IF", DecodedInstr(), if_id_next_.pc, if_id_next_.valid);
    
    std::cout << "  ID: ";
    logStageInfo("ID", id_ex_next_.decoded, id_ex_next_.pc, id_ex_next_.valid);
    
    std::cout << "  EX: ";
    logStageInfo("EX", ex_mem_next_.decoded, ex_mem_next_.pc, ex_mem_next_.valid);
    
    std::cout << "  MEM: ";
    logStageInfo("MEM", mem_wb_next_.decoded, mem_wb_next_.pc, mem_wb_next_.valid);
    
    std::cout << "  WB: ";
    logStageInfo("WB", mem_wb_.decoded, mem_wb_.pc, mem_wb_.valid);
    
    if (stall_) {
        std::cout << "  [STALL]";
    }
    if (flush_) {
        std::cout << "  [FLUSH]";
    }
    std::cout << "\n";
}

void PipelinedCPU::logStageInfo(const std::string& stage, const DecodedInstr& instr, 
                                 Address pc, bool valid) {
    if (!valid) {
        std::cout << "bubble\n";
    } else {
        std::cout << instr.mnemonic << " (PC=" << addrToHex(pc) << ")\n";
    }
}

} // namespace npu

