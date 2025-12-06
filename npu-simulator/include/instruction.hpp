#ifndef INSTRUCTION_HPP
#define INSTRUCTION_HPP

#include "common.hpp"

namespace npu {

// =============================================================================
// Instruction Encoding/Decoding Utilities
// =============================================================================

class Instruction {
public:
    // -------------------------------------------------------------------------
    // Field Extraction (Decoding)
    // -------------------------------------------------------------------------
    
    static Word getOpcode(Word instr) {
        return instr & 0x7F;  // bits [6:0]
    }
    
    static uint8_t getRd(Word instr) {
        return (instr >> 7) & 0x1F;  // bits [11:7]
    }
    
    static uint8_t getFunct3(Word instr) {
        return (instr >> 12) & 0x7;  // bits [14:12]
    }
    
    static uint8_t getRs1(Word instr) {
        return (instr >> 15) & 0x1F;  // bits [19:15]
    }
    
    static uint8_t getRs2(Word instr) {
        return (instr >> 20) & 0x1F;  // bits [24:20]
    }
    
    static uint8_t getFunct7(Word instr) {
        return (instr >> 25) & 0x7F;  // bits [31:25]
    }
    
    // -------------------------------------------------------------------------
    // Immediate Extraction (with sign extension)
    // -------------------------------------------------------------------------
    
    // I-type immediate: instr[31:20]
    static SWord getImmI(Word instr) {
        return signExtend(instr >> 20, 12);
    }
    
    // S-type immediate: instr[31:25] | instr[11:7]
    static SWord getImmS(Word instr) {
        Word imm = ((instr >> 25) << 5) | ((instr >> 7) & 0x1F);
        return signExtend(imm, 12);
    }
    
    // B-type immediate: instr[31] | instr[7] | instr[30:25] | instr[11:8] | 0
    static SWord getImmB(Word instr) {
        Word imm = ((instr >> 31) << 12) |
                   (((instr >> 7) & 0x1) << 11) |
                   (((instr >> 25) & 0x3F) << 5) |
                   (((instr >> 8) & 0xF) << 1);
        return signExtend(imm, 13);
    }
    
    // U-type immediate: instr[31:12] << 12
    static SWord getImmU(Word instr) {
        return static_cast<SWord>(instr & 0xFFFFF000);
    }
    
    // J-type immediate: instr[31] | instr[19:12] | instr[20] | instr[30:21] | 0
    static SWord getImmJ(Word instr) {
        Word imm = ((instr >> 31) << 20) |
                   (((instr >> 12) & 0xFF) << 12) |
                   (((instr >> 20) & 0x1) << 11) |
                   (((instr >> 21) & 0x3FF) << 1);
        return signExtend(imm, 21);
    }
    
    // -------------------------------------------------------------------------
    // Instruction Encoding
    // -------------------------------------------------------------------------
    
    // R-type: funct7 | rs2 | rs1 | funct3 | rd | opcode
    static Word encodeR(uint8_t opcode, uint8_t rd, uint8_t funct3, 
                        uint8_t rs1, uint8_t rs2, uint8_t funct7) {
        return (static_cast<Word>(funct7) << 25) |
               (static_cast<Word>(rs2) << 20) |
               (static_cast<Word>(rs1) << 15) |
               (static_cast<Word>(funct3) << 12) |
               (static_cast<Word>(rd) << 7) |
               opcode;
    }
    
    // I-type: imm[11:0] | rs1 | funct3 | rd | opcode
    static Word encodeI(uint8_t opcode, uint8_t rd, uint8_t funct3,
                        uint8_t rs1, SWord imm) {
        return (static_cast<Word>(imm & 0xFFF) << 20) |
               (static_cast<Word>(rs1) << 15) |
               (static_cast<Word>(funct3) << 12) |
               (static_cast<Word>(rd) << 7) |
               opcode;
    }
    
    // S-type: imm[11:5] | rs2 | rs1 | funct3 | imm[4:0] | opcode
    static Word encodeS(uint8_t opcode, uint8_t funct3, uint8_t rs1,
                        uint8_t rs2, SWord imm) {
        return (static_cast<Word>((imm >> 5) & 0x7F) << 25) |
               (static_cast<Word>(rs2) << 20) |
               (static_cast<Word>(rs1) << 15) |
               (static_cast<Word>(funct3) << 12) |
               (static_cast<Word>(imm & 0x1F) << 7) |
               opcode;
    }
    
    // B-type encoding
    static Word encodeB(uint8_t opcode, uint8_t funct3, uint8_t rs1,
                        uint8_t rs2, SWord imm) {
        return (static_cast<Word>((imm >> 12) & 0x1) << 31) |
               (static_cast<Word>((imm >> 5) & 0x3F) << 25) |
               (static_cast<Word>(rs2) << 20) |
               (static_cast<Word>(rs1) << 15) |
               (static_cast<Word>(funct3) << 12) |
               (static_cast<Word>((imm >> 1) & 0xF) << 8) |
               (static_cast<Word>((imm >> 11) & 0x1) << 7) |
               opcode;
    }
    
    // U-type: imm[31:12] | rd | opcode
    static Word encodeU(uint8_t opcode, uint8_t rd, SWord imm) {
        return (static_cast<Word>(imm) & 0xFFFFF000) |
               (static_cast<Word>(rd) << 7) |
               opcode;
    }
    
    // J-type encoding
    static Word encodeJ(uint8_t opcode, uint8_t rd, SWord imm) {
        return (static_cast<Word>((imm >> 20) & 0x1) << 31) |
               (static_cast<Word>((imm >> 1) & 0x3FF) << 21) |
               (static_cast<Word>((imm >> 11) & 0x1) << 20) |
               (static_cast<Word>((imm >> 12) & 0xFF) << 12) |
               (static_cast<Word>(rd) << 7) |
               opcode;
    }
    
    // -------------------------------------------------------------------------
    // NPU-specific encoding helpers
    // -------------------------------------------------------------------------
    
    // Encode NPU R-type instruction
    static Word encodeNPU_R(uint8_t funct3, uint8_t funct7, 
                            uint8_t vd, uint8_t vs1, uint8_t vs2) {
        return encodeR(OPCODE_NPU, vd, funct3, vs1, vs2, funct7);
    }
    
    // Encode NPU I-type instruction (VLOAD, VLBC, VCLP)
    static Word encodeNPU_I(uint8_t funct3, uint8_t vd, uint8_t rs1, SWord imm) {
        return encodeI(OPCODE_NPU, vd, funct3, rs1, imm);
    }
    
    // Encode NPU S-type instruction (VSTORE)
    static Word encodeNPU_S(uint8_t funct3, uint8_t vs, uint8_t rs1, SWord imm) {
        return encodeS(OPCODE_NPU, funct3, rs1, vs, imm);
    }
};

// =============================================================================
// NPU Instruction Constants
// =============================================================================

namespace NPUFunct3 {
    constexpr uint8_t ARITH     = 0b000;  // VADD, VCLR
    constexpr uint8_t MUL       = 0b010;  // VMUL
    constexpr uint8_t MAC       = 0b011;  // VMAC
    constexpr uint8_t RELU      = 0b100;  // VRELU
    constexpr uint8_t CLP       = 0b101;  // VCLP
    constexpr uint8_t REDUCE    = 0b110;  // VREDMAX, VARGMAX
    constexpr uint8_t MEM       = 0b111;  // VLOAD, VSTORE, VLBC
}

namespace NPUFunct7 {
    constexpr uint8_t DEFAULT   = 0b0000000;
    constexpr uint8_t VCLR      = 0b0000001;
    constexpr uint8_t VARGMAX   = 0b0000001;
    constexpr uint8_t VLBC      = 0b0000001;  // Broadcast load
    constexpr uint8_t VSTORE    = 0b0100000;  // Distinguish from VLOAD
}

// =============================================================================
// RV32I Instruction Constants
// =============================================================================

namespace RV32IFunct3 {
    // Arithmetic
    constexpr uint8_t ADD_SUB   = 0b000;
    constexpr uint8_t SLL       = 0b001;
    constexpr uint8_t SLT       = 0b010;
    constexpr uint8_t SLTU      = 0b011;
    constexpr uint8_t XOR       = 0b100;
    constexpr uint8_t SRL_SRA   = 0b101;
    constexpr uint8_t OR        = 0b110;
    constexpr uint8_t AND       = 0b111;
    
    // Branches
    constexpr uint8_t BEQ       = 0b000;
    constexpr uint8_t BNE       = 0b001;
    constexpr uint8_t BLT       = 0b100;
    constexpr uint8_t BGE       = 0b101;
    constexpr uint8_t BLTU      = 0b110;
    constexpr uint8_t BGEU      = 0b111;
    
    // Load/Store
    constexpr uint8_t LW        = 0b010;
    constexpr uint8_t SW        = 0b010;
    
    // JALR
    constexpr uint8_t JALR      = 0b000;
}

namespace RV32IFunct7 {
    constexpr uint8_t DEFAULT   = 0b0000000;
    constexpr uint8_t ALT       = 0b0100000;  // SUB, SRA
}

} // namespace npu

#endif // INSTRUCTION_HPP


