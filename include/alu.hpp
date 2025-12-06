#ifndef ALU_HPP
#define ALU_HPP

#include "common.hpp"

namespace npu {

// =============================================================================
// ALU Result
// =============================================================================

struct ALUResult {
    SWord result;
    bool zero;       // Result is zero
    bool negative;   // Result is negative
    bool overflow;   // Overflow occurred
    
    ALUResult() : result(0), zero(true), negative(false), overflow(false) {}
    ALUResult(SWord r) : result(r), zero(r == 0), negative(r < 0), overflow(false) {}
};

// =============================================================================
// Scalar ALU
// =============================================================================

class ALU {
public:
    // Execute ALU operation
    static ALUResult execute(ALUOp op, SWord a, SWord b);
    
    // Individual operations
    static SWord add(SWord a, SWord b);
    static SWord sub(SWord a, SWord b);
    static SWord andOp(SWord a, SWord b);
    static SWord orOp(SWord a, SWord b);
    static SWord xorOp(SWord a, SWord b);
    static SWord sll(SWord a, SWord b);
    static SWord srl(SWord a, SWord b);
    static SWord sra(SWord a, SWord b);
    static SWord slt(SWord a, SWord b);
    static SWord sltu(SWord a, SWord b);
    
    // Branch condition evaluation
    static bool evaluateBranch(BranchOp op, SWord a, SWord b);
};

} // namespace npu

#endif // ALU_HPP


