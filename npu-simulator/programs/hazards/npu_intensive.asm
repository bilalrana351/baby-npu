# ==========================================
# NPU Pipeline Stress Test
# ==========================================
# Description: NPU-specific hazard scenarios including vector
# register chains, vector-to-scalar operations, and heavy
# forwarding requirements.
#
# Expected Behavior: Heavy forwarding, minimal stalls
# Expected CPI: Close to 1.0 if forwarding works correctly

.text
.globl main

main:
    # Setup
    ADDI x10, x0, 256      # x10 = 0x100
    
    # --- Test 1: Vector register chain (forwarding) ---
    # Each operation depends on the previous one
    VLOAD V1, 0(x10)       # Load base vector
    VADD V2, V1, V1        # V2 = V1 + V1 (forward V1)
    VMUL V3, V2, V1        # V3 = V2 * V1 (forward V2 and V1)
    VMAC V4, V3, V2        # V4 = V4 + V3 * V2 (forward V3, V2, and V4)
    
    # Expected: 1 initial stall (V1 load-use), then all forwarding
    # V1 = [10, 20, 30, 40] (from memory)
    # V2 = [20, 40, 60, 80]
    # V3 = [200, 800, 1800, 3200]
    # V4 = [4000, 32000, 108000, 256000] (accumulated)
    
    # --- Test 2: Vector-to-scalar (reduction) ---
    # Tests vector result forwarding to scalar operation
    VREDMAX x11, V4        # Get max from V4 (forward V4)
    ADDI x12, x11, 1       # Use scalar result (forward from EX)
    
    # Expected: No stalls (forwarding handles it)
    # x11 = 256000
    # x12 = 256001
    
    # --- Test 3: Scalar-to-vector (broadcast) ---
    # Tests scalar forwarding into vector operation
    ADDI x13, x0, 5        # x13 = 5
    VLBC V5, 0(x13)        # Broadcast load (uses x13 as address)
    VADD V6, V5, V4        # Combine (forward V5 and V4)
    
    # Expected: 1 stall (V5 broadcast load-use)
    # V5 = broadcast of value at memory[5]
    # V6 = V5 + V4
    
    # --- Test 4: Multiple parallel vector operations ---
    # Tests pipeline utilization with independent operations
    VLOAD V7, 16(x10)      # Load new vector
    VLOAD V0, 32(x10)      # Load another (independent) - reuse V0
    
    # Now use them
    VADD V5, V7, V0        # Will stall on V7, but V0 ready via forwarding
    VADD V6, V7, V1        # Uses V7 (forward) and V1 (long ago)
    
    # Expected: 1 stall (V7 load-use), V8 forwarded
    
    # --- Test 5: Vector MAC with self-accumulation ---
    # Tests forwarding of destination register
    VCLR V0                # Clear accumulator (reuse V0)
    VMAC V0, V1, V1        # V0 += V1 * V1 (forward V0)
    VMAC V0, V2, V2        # V0 += V2 * V2 (forward V0 again)
    VMAC V0, V3, V3        # V0 += V3 * V3 (forward V0 again)
    
    # Expected: No stalls (all forwarding)
    # V11 accumulates three dot products
    
    # --- Test 6: ReLU activation chain ---
    # Tests NPU-specific operations in sequence
    VLOAD V7, 48(x10)      # Load raw values (reuse V7)
    VRELU V7, V7           # Apply ReLU in-place (forward)
    VCLP V7, V7, 255       # Clamp to 255 (forward)
    
    # Expected: 1 stall (V12 load-use), then forwarding
    
    # --- Test 7: Vector store-load dependency ---
    # Tests memory dependency (WAR - Write After Read)
    VSTORE V6, 0(x10)      # Store V6
    VLOAD V1, 0(x10)       # Load from same location (reuse V1)
    
    # Expected: No stall (WAR is safe in our pipeline)
    # V1 should get the newly stored value
    
    # --- Test 8: Complex dependency graph ---
    # Multiple operations with various dependencies
    VLOAD V2, 64(x10)      # Load (reuse V2)
    VADD V3, V2, V1        # Depends on V2 (stall) and V1 (old)
    VMUL V4, V3, V2        # Depends on V3 (forward) and V2 (forward)
    VMAC V0, V4, V2        # Depends on V4 (forward), V2 (forward), V0 (forward)
    
    # Expected: 1 stall (V14 load-use), then all forwarding
    
    # --- Test 9: Reduction followed by vector operation ---
    VARGMAX x14, V0        # Find index of max (forward V0)
    ADDI x15, x14, 0       # Copy index (forward x14)
    # Use x15 somehow...
    ADDI x16, x15, 10      # x16 = x15 + 10 (forward x15)
    
    # Expected: No stalls (all scalar forwarding)
    
    # --- Test 10: Final accumulation ---
    VREDMAX x17, V0        # Get final max value
    VREDMAX x18, V1        # Get another max
    ADD x19, x17, x18      # Sum the maxes (forward both)
    
    # Expected: No stalls (all forwarding)
    
    HALT

# End of npu_intensive.asm
#
# Summary:
# This program heavily exercises:
# - Vector register forwarding
# - Vector-to-scalar forwarding
# - Scalar-to-vector forwarding
# - Self-accumulation (MAC with forward of destination)
# - Load-use hazards (should have ~4-5 stalls total)
# - Complex dependency chains
#
# Expected total stalls: 4-6 (from load-use hazards)
# Expected total flushes: 0 (no branches)
# Expected CPI: ~1.1-1.15 (very efficient due to good forwarding)

