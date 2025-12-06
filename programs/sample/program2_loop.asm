# ==========================================
# PROGRAM 2: Loop & Control Flow
# ==========================================
# Description: Implements a "Dot Product Loop," which is the fundamental
# operation of Neural Networks. It iterates through memory to process
# 3 separate input vectors using VMAC for multiply-accumulate.
#
# Memory Layout:
#   0x400 (1024): Input vectors (3 x 4 elements)
#   0x500 (1280): Weight vectors (3 x 4 elements)
#   0x600 (1536): Result (output)
#
# Expected Result:
#   V0 = sum of (Input[i] * Weight[i]) for i = 0,1,2
#      = [1,2,3,4]*[1,1,1,1] + [5,6,7,8]*[2,2,2,2] + [9,10,11,12]*[3,3,3,3]
#      = [1,2,3,4] + [10,12,14,16] + [27,30,33,36]
#      = [38, 44, 50, 56]

.text
.globl main

main:
    # --- 1. Initialization ---
    ADDI t0, x0, 3          # Set Loop Counter to 3 iterations
    ADDI x10, x0, 1024      # Base Address for Inputs (0x400)
    ADDI x11, x0, 1280      # Base Address for Weights (0x500)
    
    # Clear Accumulator V0 to ensure we start at 0
    VCLR V0

LOOP_START:
    # --- 2. NPU Operations ---
    VLOAD V1, 0(x10)        # Load Input Vector
    VLOAD V2, 0(x11)        # Load Weight Vector
    
    # Multiply V1*V2 and add to V0
    VMAC V0, V1, V2
    
    # --- 3. Pointer Updates ---
    # Move pointers forward by 16 bytes (size of 4 integers)
    ADDI x10, x10, 16
    ADDI x11, x11, 16
    
    # --- 4. Loop Control ---
    ADDI t0, t0, -1         # Decrement counter
    
    # If t0 is NOT equal to 0, jump back to LOOP_START
    BNE t0, x0, LOOP_START
    
    # --- 5. Final Store ---
    # Store total result to address 0x600 (1536)
    ADDI x12, x0, 1536
    VSTORE V0, 0(x12)
    
    # End of Program 2
    HALT


