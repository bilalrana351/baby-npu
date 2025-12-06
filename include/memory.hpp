#ifndef MEMORY_HPP
#define MEMORY_HPP

#include "common.hpp"

namespace npu {

// =============================================================================
// Memory Subsystem
// =============================================================================

class Memory {
private:
    std::vector<Byte> data_;
    size_t size_;
    
public:
    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------
    
    explicit Memory(size_t size = DEFAULT_MEMORY_SIZE) 
        : data_(size, 0), size_(size) {}
    
    // -------------------------------------------------------------------------
    // Basic Access
    // -------------------------------------------------------------------------
    
    size_t size() const { return size_; }
    
    // Read a single byte
    Byte readByte(Address addr) const {
        checkAddress(addr);
        return data_[addr];
    }
    
    // Write a single byte
    void writeByte(Address addr, Byte value) {
        checkAddress(addr);
        data_[addr] = value;
    }
    
    // -------------------------------------------------------------------------
    // Word Access (32-bit, little-endian)
    // -------------------------------------------------------------------------
    
    // Read a 32-bit word
    Word readWord(Address addr) const {
        checkAddress(addr + 3);
        return static_cast<Word>(data_[addr]) |
               (static_cast<Word>(data_[addr + 1]) << 8) |
               (static_cast<Word>(data_[addr + 2]) << 16) |
               (static_cast<Word>(data_[addr + 3]) << 24);
    }
    
    // Write a 32-bit word
    void writeWord(Address addr, Word value) {
        checkAddress(addr + 3);
        data_[addr]     = static_cast<Byte>(value & 0xFF);
        data_[addr + 1] = static_cast<Byte>((value >> 8) & 0xFF);
        data_[addr + 2] = static_cast<Byte>((value >> 16) & 0xFF);
        data_[addr + 3] = static_cast<Byte>((value >> 24) & 0xFF);
    }
    
    // -------------------------------------------------------------------------
    // Vector Access (128-bit = 4 x 32-bit)
    // -------------------------------------------------------------------------
    
    // Read 128 bits (4 integers) for VLOAD
    void read128(Address addr, int32_t out[4]) const {
        checkAddress(addr + 15);
        for (int i = 0; i < 4; ++i) {
            out[i] = static_cast<int32_t>(readWord(addr + i * 4));
        }
    }
    
    // Read 128 bits into Vector128
    Vector128 readVector(Address addr) const {
        Vector128 vec;
        read128(addr, vec.data());
        return vec;
    }
    
    // Write 128 bits (4 integers) for VSTORE
    void write128(Address addr, const int32_t in[4]) {
        checkAddress(addr + 15);
        for (int i = 0; i < 4; ++i) {
            writeWord(addr + i * 4, static_cast<Word>(in[i]));
        }
    }
    
    // Write Vector128
    void writeVector(Address addr, const Vector128& vec) {
        write128(addr, vec.data());
    }
    
    // -------------------------------------------------------------------------
    // Bulk Operations
    // -------------------------------------------------------------------------
    
    // Load program into memory at specified address
    void loadProgram(const std::vector<Word>& program, Address startAddr = TEXT_SEGMENT_START) {
        for (size_t i = 0; i < program.size(); ++i) {
            writeWord(startAddr + i * 4, program[i]);
        }
    }
    
    // Initialize data section
    void loadData(const std::vector<int32_t>& values, Address startAddr) {
        for (size_t i = 0; i < values.size(); ++i) {
            writeWord(startAddr + i * 4, static_cast<Word>(values[i]));
        }
    }
    
    // Clear all memory
    void clear() {
        std::fill(data_.begin(), data_.end(), 0);
    }
    
    // -------------------------------------------------------------------------
    // Debug/Inspection
    // -------------------------------------------------------------------------
    
    // Dump memory region for debugging
    void dump(Address start, size_t numWords, std::ostream& os = std::cout) const {
        os << "Memory dump from " << addrToHex(start) << ":\n";
        for (size_t i = 0; i < numWords; ++i) {
            Address addr = start + i * 4;
            if (addr + 3 < size_) {
                os << "  " << addrToHex(addr) << ": " << wordToHex(readWord(addr)) << "\n";
            }
        }
    }
    
    // Get raw pointer for advanced operations (use carefully)
    const Byte* getRawPointer() const { return data_.data(); }
    Byte* getRawPointer() { return data_.data(); }
    
private:
    void checkAddress(Address addr) const {
        if (addr >= size_) {
            throw std::out_of_range("Memory access out of bounds: " + addrToHex(addr));
        }
    }
};

} // namespace npu

#endif // MEMORY_HPP


