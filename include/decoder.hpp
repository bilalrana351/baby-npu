#ifndef DECODER_HPP
#define DECODER_HPP

#include "common.hpp"
#include "instruction.hpp"

namespace npu {

// =============================================================================
// Instruction Decoder
// =============================================================================

class Decoder {
public:
    // Decode a 32-bit instruction
    DecodedInstr decode(Word instruction);
    
private:
    // Decode based on opcode
    void decodeNPU(Word instr, DecodedInstr& decoded);
    void decodeRType(Word instr, DecodedInstr& decoded);
    void decodeIType(Word instr, DecodedInstr& decoded);
    void decodeSType(Word instr, DecodedInstr& decoded);
    void decodeBType(Word instr, DecodedInstr& decoded);
    void decodeUType(Word instr, DecodedInstr& decoded);
    void decodeJType(Word instr, DecodedInstr& decoded);
    
    // Determine ALU operation from R-type
    ALUOp getALUOpR(uint8_t funct3, uint8_t funct7);
    
    // Determine ALU operation from I-type
    ALUOp getALUOpI(uint8_t funct3, SWord imm);
    
    // Determine branch operation
    BranchOp getBranchOp(uint8_t funct3);
    
    // Determine NPU operation
    NPUOp getNPUOp(uint8_t funct3, uint8_t funct7, bool isStore);
    
    // Build mnemonic string
    std::string buildMnemonic(const DecodedInstr& decoded);
};

} // namespace npu

#endif // DECODER_HPP


