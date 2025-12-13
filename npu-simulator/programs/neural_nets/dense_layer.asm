# ==========================================
# NEURAL NETWORK DEMO 1: Single Dense Layer
# ==========================================
# Description: Implements a single fully-connected (dense) layer
# Output = ReLU(W * input + bias)
#
# Architecture:
#   Input:  4 elements
#   Output: 4 elements
#   Weights: 4x4 matrix (row-major)
#
# Memory Layout:
#   0x1000 (4096): Weight matrix W (4 rows x 4 cols = 16 values)
#   0x1100 (4352): Input vector
#   0x1200 (4608): Bias vector
#   0x1300 (4864): Output result
#
# Algorithm:
#   For each output neuron i:
#     output[i] = sum(W[i][j] * input[j]) + bias[i]
#   Then apply ReLU activation
#
# Test Data (diagonal matrix for easy verification):
#   W = [[1,0,0,0], [0,2,0,0], [0,0,3,0], [0,0,0,4]]
#   input = [10, 20, 30, 40]
#   bias = [1, 2, 3, 4]
#   Expected output = ReLU([11, 42, 93, 164])

.text
.globl main

main:
    # --- Setup Addresses ---
    # Using LUI for addresses > 2047 (ADDI's 12-bit signed immediate limit)
    LUI x10, 1              # x10 = 0x1000 = 4096 (Weight base address)
    LUI x11, 1              # x11 = 0x1000 = 4096
    ADDI x11, x11, 256      # x11 = 0x1100 = 4352 (Input address)
    LUI x12, 1              # x12 = 0x1000 = 4096
    ADDI x12, x12, 512      # x12 = 0x1200 = 4608 (Bias address)
    LUI x13, 1              # x13 = 0x1000 = 4096
    ADDI x13, x13, 768      # x13 = 0x1300 = 4864 (Output address)
    
    # --- Load Input Vector ---
    VLOAD V0, 0(x11)        # V0 = input = [10, 20, 30, 40]
    
    # --- Initialize Output with Bias ---
    VLOAD V6, 0(x12)        # V6 = bias = [1, 2, 3, 4] (accumulator)
    
    # --- Matrix-Vector Multiplication using VMAC ---
    # For a proper matrix multiply, we need to compute dot products
    # Since we have a 4x4 matrix and 4-element vectors, we do:
    # output[i] = sum_j(W[i,j] * input[j])
    # Using VMAC (Multiply-Accumulate) to combine multiply and add in one instruction
    
    # Row 0: W[0] = [1,0,0,0]
    VLOAD V1, 0(x10)         # V1 = W row 0
    VMAC V6, V1, V0          # V6 += V1 * V0 (multiply-accumulate)
    
    # Row 1: W[1] = [0,2,0,0]
    VLOAD V1, 16(x10)        # V1 = W row 1
    VMAC V6, V1, V0          # V6 += V1 * V0
    
    # Row 2: W[2] = [0,0,3,0]
    VLOAD V1, 32(x10)        # V1 = W row 2
    VMAC V6, V1, V0          # V6 += V1 * V0
    
    # Row 3: W[3] = [0,0,0,4]
    VLOAD V1, 48(x10)        # V1 = W row 3
    VMAC V6, V1, V0          # V6 += V1 * V0
    
    # V6 now contains W*input + bias (VMAC accumulated everything)
    
    # --- Apply ReLU Activation ---
    VRELU V6, V6
    
    # --- Store Output ---
    VSTORE V6, 0(x13)
    
    # --- Classification ---
    VREDMAX x20, V6          # Max value
    VARGMAX x21, V6          # Index of max (predicted class)
    
    HALT


