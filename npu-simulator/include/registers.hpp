#ifndef REGISTERS_HPP
#define REGISTERS_HPP

#include "common.hpp"

namespace npu {

// =============================================================================
// Scalar Register File (x0-x31)
// =============================================================================

class ScalarRegisterFile {
private:
    std::array<SWord, NUM_SCALAR_REGS> regs_;
    
public:
    ScalarRegisterFile() {
        regs_.fill(0);
    }
    
    // Read register (x0 always returns 0)
    SWord read(uint8_t reg) const {
        if (reg >= NUM_SCALAR_REGS) {
            throw std::out_of_range("Invalid scalar register: " + std::to_string(reg));
        }
        return (reg == 0) ? 0 : regs_[reg];
    }
    
    // Write register (writes to x0 are ignored)
    void write(uint8_t reg, SWord value) {
        if (reg >= NUM_SCALAR_REGS) {
            throw std::out_of_range("Invalid scalar register: " + std::to_string(reg));
        }
        if (reg != 0) {
            regs_[reg] = value;
        }
    }
    
    // Get all registers for inspection
    const std::array<SWord, NUM_SCALAR_REGS>& getAll() const {
        return regs_;
    }
    
    // Clear all registers
    void clear() {
        regs_.fill(0);
    }
    
    // Dump registers for debugging
    void dump(std::ostream& os = std::cout) const {
        os << "Scalar Registers:\n";
        for (size_t i = 0; i < NUM_SCALAR_REGS; ++i) {
            if (regs_[i] != 0 || i == 0) {
                os << "  " << scalarRegName(i) << " (x" << i << "): " 
                   << regs_[i] << " (" << wordToHex(static_cast<Word>(regs_[i])) << ")\n";
            }
        }
    }
    
    // Dump only non-zero registers (compact)
    void dumpNonZero(std::ostream& os = std::cout) const {
        os << "Non-zero Scalar Registers:\n";
        for (size_t i = 1; i < NUM_SCALAR_REGS; ++i) {
            if (regs_[i] != 0) {
                os << "  " << scalarRegName(i) << ": " << regs_[i] << "\n";
            }
        }
    }
};

// =============================================================================
// Vector Register File (V0-V7)
// =============================================================================

class VectorRegisterFile {
private:
    std::array<Vector128, NUM_VECTOR_REGS> regs_;
    
public:
    VectorRegisterFile() {
        for (auto& reg : regs_) {
            reg.fill(0);
        }
    }
    
    // Read vector register
    const Vector128& read(uint8_t reg) const {
        if (reg >= NUM_VECTOR_REGS) {
            throw std::out_of_range("Invalid vector register: V" + std::to_string(reg));
        }
        return regs_[reg];
    }
    
    // Read into array
    void read(uint8_t reg, int32_t out[4]) const {
        const Vector128& v = read(reg);
        std::copy(v.begin(), v.end(), out);
    }
    
    // Write vector register
    void write(uint8_t reg, const Vector128& value) {
        if (reg >= NUM_VECTOR_REGS) {
            throw std::out_of_range("Invalid vector register: V" + std::to_string(reg));
        }
        regs_[reg] = value;
    }
    
    // Write from array
    void write(uint8_t reg, const int32_t in[4]) {
        if (reg >= NUM_VECTOR_REGS) {
            throw std::out_of_range("Invalid vector register: V" + std::to_string(reg));
        }
        std::copy(in, in + 4, regs_[reg].begin());
    }
    
    // Clear a vector register
    void clear(uint8_t reg) {
        if (reg >= NUM_VECTOR_REGS) {
            throw std::out_of_range("Invalid vector register: V" + std::to_string(reg));
        }
        regs_[reg].fill(0);
    }
    
    // Clear all vector registers
    void clearAll() {
        for (auto& reg : regs_) {
            reg.fill(0);
        }
    }
    
    // Get all registers for inspection
    const std::array<Vector128, NUM_VECTOR_REGS>& getAll() const {
        return regs_;
    }
    
    // Dump registers for debugging
    void dump(std::ostream& os = std::cout) const {
        os << "Vector Registers:\n";
        for (size_t i = 0; i < NUM_VECTOR_REGS; ++i) {
            os << "  V" << i << ": " << vectorToString(regs_[i]) << "\n";
        }
    }
    
    // Dump only non-zero registers
    void dumpNonZero(std::ostream& os = std::cout) const {
        os << "Non-zero Vector Registers:\n";
        for (size_t i = 0; i < NUM_VECTOR_REGS; ++i) {
            bool isZero = std::all_of(regs_[i].begin(), regs_[i].end(), 
                                      [](int32_t v) { return v == 0; });
            if (!isZero) {
                os << "  V" << i << ": " << vectorToString(regs_[i]) << "\n";
            }
        }
    }
};

// =============================================================================
// Combined Register File
// =============================================================================

class RegisterFile {
public:
    ScalarRegisterFile scalar;
    VectorRegisterFile vector;
    
    // Convenience methods
    SWord readScalar(uint8_t reg) const { return scalar.read(reg); }
    void writeScalar(uint8_t reg, SWord value) { scalar.write(reg, value); }
    
    const Vector128& readVector(uint8_t reg) const { return vector.read(reg); }
    void writeVector(uint8_t reg, const Vector128& value) { vector.write(reg, value); }
    
    void clear() {
        scalar.clear();
        vector.clearAll();
    }
    
    void dump(std::ostream& os = std::cout) const {
        scalar.dump(os);
        os << "\n";
        vector.dump(os);
    }
    
    void dumpNonZero(std::ostream& os = std::cout) const {
        scalar.dumpNonZero(os);
        vector.dumpNonZero(os);
    }
};

} // namespace npu

#endif // REGISTERS_HPP


