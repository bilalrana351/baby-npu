# ==========================================
# Load-Use Hazard Test
# ==========================================
# Description: Tests load-use hazards that require pipeline stalls.
# A load instruction produces a result that is needed by the next
# instruction, creating a hazard that cannot be resolved by forwarding
# alone (data isn't ready until MEM stage).
#
# Expected Behavior: 2-3 stall cycles
# Expected CPI: >1.0 due to stalls

.text
.globl main

main:
    # Setup base address
    ADDI x10, x0, 256      # x10 = 0x100 = 256
    
    # --- Case 1: Scalar load-use (requires 1-cycle stall) ---
    # The load needs MEM stage to complete, but next instruction
    # needs it in EX stage - MUST STALL
    LW x11, 0(x10)         # Load from memory[256]
    ADDI x12, x11, 10      # Use immediately - STALL! 
                           # (x11 not ready until MEM/WB)
    
    # Expected: 1 stall cycle
    # x11 = (value from memory[256])
    # x12 = x11 + 10
    
    # --- Case 2: Vector load-use (requires 1-cycle stall) ---
    VLOAD V1, 0(x10)       # Load vector from memory
    VADD V2, V1, V1        # Use immediately - STALL!
                           # (V1 not ready until MEM/WB)
    
    # Expected: 1 stall cycle
    # V1 = vector from memory[256]
    # V2 = V1 + V1
    
    # --- Case 3: Load with non-dependent instruction (no stall) ---
    # This shows that stalls only happen when there's a dependency
    LW x13, 4(x10)         # Load
    ADDI x14, x0, 5        # Independent operation - NO STALL
    ADD x15, x13, x14      # Use x13 here (forwarding works, 2 cycles later)
    
    # Expected: NO stall
    # x13 = value from memory[260]
    # x14 = 5
    # x15 = x13 + 5
    
    # --- Case 4: Multiple loads in sequence ---
    LW x16, 8(x10)         # Load 1
    LW x17, 12(x10)        # Load 2 (no dependency on x16)
    ADD x18, x16, x17      # Use both (forwarding works)
    
    # Expected: NO stall (loads don't depend on each other)
    # x16 = value from memory[264]
    # x17 = value from memory[268]
    # x18 = x16 + x17
    
    # --- Case 5: Vector load followed by dependent operation ---
    VLOAD V3, 16(x10)      # Load vector
    VMAC V0, V3, V3        # Use immediately - STALL!
    
    # Expected: 1 stall cycle
    # V3 = vector from memory[272]
    # V0 = accumulate V3 * V3
    
    # --- Case 6: Load-store sequence (no stall) ---
    LW x19, 0(x10)         # Load
    SW x19, 100(x0)        # Store same value (forwarding in MEM stage)
    
    # Expected: NO stall (store uses forwarding in MEM stage)
    
    HALT

# End of load_use.asm
#
# Summary:
# - Case 1: Stall (scalar load-use)
# - Case 2: Stall (vector load-use)
# - Case 3: No stall (independent operation)
# - Case 4: No stall (loads independent)
# - Case 5: Stall (vector load-use with MAC)
# - Case 6: No stall (store can use MEM forwarding)
#
# Total expected stalls: 3

