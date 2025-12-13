#ifndef NPU_UNIT_HPP
#define NPU_UNIT_HPP

#include "common.hpp"

namespace npu {

// =============================================================================
// NPU Execution Result
// =============================================================================

struct NPUResult {
    Vector128 vectorResult;
    SWord scalarResult;     // For VREDMAX, VARGMAX
    bool writesVector;      // Result goes to vector register
    bool writesScalar;      // Result goes to scalar register
    
    NPUResult() : scalarResult(0), writesVector(false), writesScalar(false) {
        vectorResult.fill(0);
    }
};

// =============================================================================
// NPU (Vector) Execution Unit
// =============================================================================

class NPUUnit {
public:
    // Execute NPU operation
    static NPUResult execute(NPUOp op, 
                            const Vector128& vs1, 
                            const Vector128& vs2,
                            const Vector128& vd,  // For accumulate
                            SWord imm = 0);
    
    // Individual operations
    static Vector128 vadd(const Vector128& a, const Vector128& b);
    static Vector128 vmul(const Vector128& a, const Vector128& b);
    static Vector128 vmac(const Vector128& acc, const Vector128& a, const Vector128& b);
    static Vector128 vrelu(const Vector128& a);
    static Vector128 vclp(const Vector128& a, SWord max);
    static SWord vredmax(const Vector128& a);
    static SWord vargmax(const Vector128& a);
    static Vector128 vclr();
    static Vector128 vbroadcast(SWord value);
};

} // namespace npu

#endif // NPU_UNIT_HPP


