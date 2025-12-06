# ==========================================
# PROGRAM 1: Basic Vector Arithmetic
# ==========================================
# Description: Demonstrates vector arithmetic and memory operations.
# Loads two vectors (A and B) from memory, computes V_result = (A + B) * A,
# and stores the final vector back to memory.
#
# Memory Layout:
#   0x100 (256):  Vector A = [10, 20, 30, 40]
#   0x200 (512):  Vector B = [1, 2, 3, 4]
#   0x300 (768):  Result (output)
#
# Expected Result:
#   V3 = (A + B) * A = ([10,20,30,40] + [1,2,3,4]) * [10,20,30,40]
#      = [11,22,33,44] * [10,20,30,40]
#      = [110, 440, 990, 1760]

.text
.globl main

main:
    # --- 1. Setup Memory Addresses ---
    # x10 = Address of Vector A (0x100 = 256)
    ADDI x10, x0, 256
    
    # x11 = Address of Vector B (0x200 = 512)
    ADDI x11, x0, 512
    
    # x12 = Address for Result (0x300 = 768)
    ADDI x12, x0, 768
    
    # --- 2. Load Data from Memory ---
    # Load 4 integers (128-bits) from Mem[x10] into V1
    VLOAD V1, 0(x10)
    
    # Load 4 integers from Mem[x11] into V2
    VLOAD V2, 0(x11)
    
    # --- 3. Perform Calculation ---
    # Step A: Add Vectors (V3 = V1 + V2)
    VADD V3, V1, V2
    
    # Step B: Multiply Result by Vector A (V3 = V3 * V1)
    VMUL V3, V3, V1
    
    # --- 4. Store Result ---
    # Store the final vector V3 back to memory at address 0x300
    VSTORE V3, 0(x12)
    
    # End of Program 1
    HALT


