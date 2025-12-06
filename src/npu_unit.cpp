#include "npu_unit.hpp"

namespace npu {

// =============================================================================
// NPU Execute
// =============================================================================

NPUResult NPUUnit::execute(NPUOp op, 
                           const Vector128& vs1, 
                           const Vector128& vs2,
                           const Vector128& vd,
                           SWord imm) {
    NPUResult result;
    
    switch (op) {
        case NPUOp::VADD:
            result.vectorResult = vadd(vs1, vs2);
            result.writesVector = true;
            break;
            
        case NPUOp::VMUL:
            result.vectorResult = vmul(vs1, vs2);
            result.writesVector = true;
            break;
            
        case NPUOp::VMAC:
            result.vectorResult = vmac(vd, vs1, vs2);
            result.writesVector = true;
            break;
            
        case NPUOp::VRELU:
            result.vectorResult = vrelu(vs1);
            result.writesVector = true;
            break;
            
        case NPUOp::VCLP:
            result.vectorResult = vclp(vs1, imm);
            result.writesVector = true;
            break;
            
        case NPUOp::VREDMAX:
            result.scalarResult = vredmax(vs1);
            result.writesScalar = true;
            break;
            
        case NPUOp::VARGMAX:
            result.scalarResult = vargmax(vs1);
            result.writesScalar = true;
            break;
            
        case NPUOp::VCLR:
            result.vectorResult = vclr();
            result.writesVector = true;
            break;
            
        default:
            break;
    }
    
    return result;
}

// =============================================================================
// Vector Operations
// =============================================================================

Vector128 NPUUnit::vadd(const Vector128& a, const Vector128& b) {
    Vector128 result;
    for (int i = 0; i < 4; ++i) {
        result[i] = a[i] + b[i];
    }
    return result;
}

Vector128 NPUUnit::vmul(const Vector128& a, const Vector128& b) {
    Vector128 result;
    for (int i = 0; i < 4; ++i) {
        result[i] = a[i] * b[i];
    }
    return result;
}

Vector128 NPUUnit::vmac(const Vector128& acc, const Vector128& a, const Vector128& b) {
    Vector128 result;
    for (int i = 0; i < 4; ++i) {
        result[i] = acc[i] + (a[i] * b[i]);
    }
    return result;
}

Vector128 NPUUnit::vrelu(const Vector128& a) {
    Vector128 result;
    for (int i = 0; i < 4; ++i) {
        result[i] = (a[i] < 0) ? 0 : a[i];
    }
    return result;
}

Vector128 NPUUnit::vclp(const Vector128& a, SWord max) {
    Vector128 result;
    for (int i = 0; i < 4; ++i) {
        result[i] = (a[i] > max) ? max : a[i];
    }
    return result;
}

SWord NPUUnit::vredmax(const Vector128& a) {
    SWord maxVal = a[0];
    for (int i = 1; i < 4; ++i) {
        if (a[i] > maxVal) {
            maxVal = a[i];
        }
    }
    return maxVal;
}

SWord NPUUnit::vargmax(const Vector128& a) {
    SWord maxVal = a[0];
    SWord maxIdx = 0;
    for (int i = 1; i < 4; ++i) {
        if (a[i] > maxVal) {
            maxVal = a[i];
            maxIdx = i;
        }
    }
    return maxIdx;
}

Vector128 NPUUnit::vclr() {
    Vector128 result;
    result.fill(0);
    return result;
}

Vector128 NPUUnit::vbroadcast(SWord value) {
    Vector128 result;
    result.fill(value);
    return result;
}

} // namespace npu


