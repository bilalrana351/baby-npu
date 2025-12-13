# ==========================================
# Mixed Hazards Test
# ==========================================
# Description: Combines multiple hazard types in a realistic
# scenario. Tests load-use stalls, data forwarding, and
# control hazards all together.
#
# Expected Behavior: Multiple stalls and flushes
# Expected CPI: Significantly >1.0

.text
.globl main

main:
    # Setup
    ADDI x10, x0, 256      # x10 = 0x100 (base address)
    
    # --- Scenario 1: Load-use hazard ---
    LW x11, 0(x10)         # Load from memory
    ADDI x12, x11, 5       # STALL! (load-use hazard)
    
    # Expected: 1 stall
    # x11 = value from memory[256]
    # x12 = x11 + 5
    
    # --- Scenario 2: RAW hazard with forwarding ---
    ADDI x13, x12, 1       # Forward from MEM stage (no stall)
    ADD x14, x13, x12      # Forward x13 from EX, x12 from MEM
    
    # Expected: 0 stalls (forwarding handles it)
    # x13 = x12 + 1
    # x14 = x13 + x12
    
    # --- Scenario 3: Branch with dependent computation ---
    # The branch condition depends on x14 which was just computed
    BEQ x14, x0, SKIP      # Branch depends on x14 (forwarding needed)
    
    # If x14 != 0, this executes:
    VLOAD V1, 0(x10)       # May be flushed if branch taken
    VADD V2, V1, V1        # May be flushed if branch taken
    
SKIP:
    # This executes regardless
    ADDI x15, x0, 100      # x15 = 100
    
    # Expected: Depends on x14 value
    # If branch taken: 2 instructions flushed
    # If not taken: 0 flushes, but might have vector load-use stall
    
    # --- Scenario 4: Vector computation chain ---
    VLOAD V3, 16(x10)      # Load vector
    VMAC V0, V3, V3        # STALL! (vector load-use)
    VADD V4, V0, V3        # Forward V0 from EX (no stall)
    
    # Expected: 1 stall
    # V3 = vector from memory[272]
    # V0 = accumulated V3 * V3
    # V4 = V0 + V3
    
    # --- Scenario 5: Load followed by branch ---
    LW x16, 4(x10)         # Load
    BNE x16, x0, TARGET    # Branch depends on loaded value
                           # Need to wait for load to complete!
    ADDI x17, x0, 1        # May execute or be flushed
    J END_TEST
    
TARGET:
    ADDI x17, x0, 2        # Alternative path
    
END_TEST:
    # --- Scenario 6: Multiple dependencies ---
    VLOAD V5, 32(x10)      # Load
    VLOAD V6, 48(x10)      # Load (independent)
    VMAC V7, V5, V6        # STALL on V5, but V6 should be ready
    
    # Expected: 1 stall (V5 load-use)
    # Both vectors loaded, but V5 causes stall
    
    HALT

# End of mixed_hazards.asm
#
# Summary:
# - Scenario 1: 1 stall (load-use)
# - Scenario 2: 0 stalls (forwarding)
# - Scenario 3: Variable (0-2 flushes depending on branch)
# - Scenario 4: 1 stall (vector load-use)
# - Scenario 5: Possibly 1 stall + 1 flush
# - Scenario 6: 1 stall (vector load-use)
#
# Total expected: 3-5 stalls/flushes depending on data values

