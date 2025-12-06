# ==========================================
# NEURAL NETWORK DEMO 2: 2-Layer MLP
# ==========================================
# Description: Implements a 2-layer Multi-Layer Perceptron
# Layer 1: hidden = ReLU(W1 * input)
# Layer 2: output = W2 * hidden
# Final: class = argmax(output)
#
# Architecture:
#   Input:  4 elements
#   Hidden: 4 elements (with ReLU)
#   Output: 4 elements (classification scores)
#
# Memory Layout:
#   0x1000 (4096): W1 weights (4x4)
#   0x1100 (4352): Input vector
#   0x2000 (8192): W2 weights (4x4)
#   0x2100 (8448): Hidden layer output (intermediate)
#   0x2200 (8704): Final output
#
# Test Data:
#   W1 = diagonal [1,2,3,4] for easy verification
#   W2 = [[1,1,1,1], [-1,1,-1,1], [1,-1,1,-1], [-1,-1,1,1]]
#   input = [10, 20, 30, 40]

.text
.globl main

main:
    # === LAYER 1 ===
    # --- Setup Addresses ---
    # Using LUI for addresses > 2047 (ADDI's 12-bit signed immediate limit)
    LUI x10, 1              # x10 = 0x1000 = 4096 (W1 base address)
    LUI x11, 1              # x11 = 0x1000 = 4096
    ADDI x11, x11, 256      # x11 = 0x1100 = 4352 (Input address)
    
    # --- Load Input ---
    VLOAD V0, 0(x11)        # V0 = input = [10, 20, 30, 40]
    
    # --- Layer 1 Forward Pass using VMAC ---
    # Initialize accumulator to zero
    VCLR V6                 # V6 = accumulator for layer 1
    
    # Load W1 rows and accumulate
    VLOAD V1, 0(x10)        # W1 row 0
    VMAC V6, V1, V0         # V6 += V1 * V0
    
    VLOAD V1, 16(x10)       # W1 row 1
    VMAC V6, V1, V0         # V6 += V1 * V0
    
    VLOAD V1, 32(x10)       # W1 row 2
    VMAC V6, V1, V0         # V6 += V1 * V0
    
    VLOAD V1, 48(x10)       # W1 row 3
    VMAC V6, V1, V0         # V6 += V1 * V0
    # V6 now contains W1 * input
    
    # Apply ReLU to get hidden layer
    VRELU V6, V6            # V6 = hidden = ReLU(W1 * input)
    
    # Store hidden for reference
    LUI x12, 2              # x12 = 0x2000 = 8192
    ADDI x12, x12, 256      # x12 = 0x2100 = 8448 (Hidden layer storage)
    VSTORE V6, 0(x12)
    
    # === LAYER 2 ===
    # --- Setup W2 Address ---
    LUI x13, 2              # x13 = 0x2000 = 8192 (W2 base address)
    
    # --- Layer 2 Forward Pass using VMAC ---
    # V6 now contains hidden layer output
    # Initialize accumulator to zero
    VCLR V7                 # V7 = accumulator for layer 2
    
    VLOAD V1, 0(x13)        # W2 row 0 = [1,1,1,1]
    VMAC V7, V1, V6         # V7 += V1 * V6
    
    VLOAD V1, 16(x13)       # W2 row 1 = [-1,1,-1,1]
    VMAC V7, V1, V6         # V7 += V1 * V6
    
    VLOAD V1, 32(x13)       # W2 row 2 = [1,-1,1,-1]
    VMAC V7, V1, V6         # V7 += V1 * V6
    
    VLOAD V1, 48(x13)       # W2 row 3 = [-1,-1,1,1]
    VMAC V7, V1, V6         # V7 += V1 * V6
    # V7 now contains output scores
    
    # Store final output
    LUI x14, 2              # x14 = 0x2000 = 8192
    ADDI x14, x14, 512      # x14 = 0x2200 = 8704 (Final output storage)
    VSTORE V7, 0(x14)
    
    # === CLASSIFICATION ===
    # Get predicted class
    VREDMAX x20, V7          # Max score
    VARGMAX x21, V7          # Predicted class index
    
    HALT


