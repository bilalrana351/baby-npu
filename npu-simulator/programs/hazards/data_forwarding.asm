# ==========================================
# RAW Hazard Test - Data Forwarding
# ==========================================
# Description: Tests EX-to-EX and MEM-to-EX forwarding scenarios.
# This program creates back-to-back dependencies that should be
# resolved by forwarding without stalling the pipeline.
#
# Expected Behavior: No stalls, all hazards resolved by forwarding.
# Expected CPI: ~1.0 (pipelined) vs 1.0 (single-cycle)

.text
.globl main

main:
    # --- Case 1: EX-to-EX forwarding (no stall needed) ---
    # Instruction 1 writes x10, instruction 2 reads x10
    ADDI x10, x0, 5        # x10 = 5
    ADDI x11, x10, 3       # x11 = x10 + 3 = 8 (needs x10 from prev instr)
    ADD x12, x10, x11      # x12 = x10 + x11 = 5 + 8 = 13 (needs both)
    
    # Expected Results:
    # x10 = 5
    # x11 = 8
    # x12 = 13
    
    # --- Case 2: MEM-to-EX forwarding ---
    ADDI x13, x0, 100      # x13 = 100
    SW x13, 0(x0)          # Store x13 to memory[0]
    ADDI x14, x13, 1       # x14 = x13 + 1 = 101 (forward from MEM stage)
    
    # Expected Results:
    # x13 = 100
    # x14 = 101
    # memory[0] = 100
    
    # --- Case 3: Longer dependency chain ---
    ADDI x15, x0, 10       # x15 = 10
    ADD x16, x15, x15      # x16 = 10 + 10 = 20 (forward from EX)
    ADD x17, x16, x15      # x17 = 20 + 10 = 30 (forward x16 from EX, x15 from MEM)
    SUB x18, x17, x16      # x18 = 30 - 20 = 10 (forward both)
    
    # Expected Results:
    # x15 = 10
    # x16 = 20
    # x17 = 30
    # x18 = 10
    
    # --- Case 4: Vector forwarding ---
    # Load test vector at 0x100
    ADDI x20, x0, 256      # x20 = 256 (0x100)
    
    VLOAD V1, 0(x20)       # Load vector from memory
    VADD V2, V1, V1        # Double V1 (forward from EX/MEM)
    VMUL V3, V2, V1        # Uses both V2 and V1 (multiple forwards)
    
    # Expected Results:
    # V1 = [10, 20, 30, 40] (from memory at 0x100)
    # V2 = [20, 40, 60, 80]
    # V3 = [200, 800, 1800, 3200]
    
    # --- Case 5: Mixed scalar and vector ---
    VREDMAX x21, V3        # Get max from V3 (forward V3)
    ADDI x22, x21, 1       # Use scalar result (forward from EX)
    
    # Expected Results:
    # x21 = 3200
    # x22 = 3201
    
    HALT

# End of data_forwarding.asm

