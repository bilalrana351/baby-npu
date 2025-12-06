# ==========================================
# NEURAL NETWORK DEMO 3: Simple RNN
# ==========================================
# Description: Implements a simple Recurrent Neural Network cell
# h_new = ReLU(W_h * h_old + W_x * x_input)
#
# This demonstrates:
# - Hidden state persistence across timesteps
# - Recurrent computation pattern
# - State accumulation
#
# Architecture:
#   Hidden state: 4 elements
#   Input: 4 elements per timestep
#   Processing: 3 timesteps
#
# Memory Layout:
#   0x2100 (8448): W_h weights (hidden-to-hidden)
#   0x2200 (8704): W_x weights (input-to-hidden)
#   0x2300 (8960): Input sequence (3 timesteps x 4 elements)
#   0x2400 (9216): Final hidden state output
#
# Algorithm per timestep:
#   h_new = ReLU(W_h * h_old + W_x * x_input)

.text
.globl main

main:
    # === INITIALIZATION ===
    # Using LUI for addresses > 2047 (ADDI's 12-bit signed immediate limit)
    LUI x10, 2              # x10 = 0x2000 = 8192
    ADDI x10, x10, 256      # x10 = 0x2100 = 8448 (W_h address)
    LUI x11, 2              # x11 = 0x2000 = 8192
    ADDI x11, x11, 512      # x11 = 0x2200 = 8704 (W_x address)
    LUI x12, 2              # x12 = 0x2000 = 8192
    ADDI x12, x12, 768      # x12 = 0x2300 = 8960 (Input sequence address)
    LUI x13, 2              # x13 = 0x2000 = 8192
    ADDI x13, x13, 1024     # x13 = 0x2400 = 9216 (Output address)
    
    ADDI t0, x0, 3          # Number of timesteps
    
    # Initialize hidden state to zeros
    VCLR V0                  # V0 = h (hidden state)
    
# === RNN LOOP ===
RNN_TIMESTEP:
    # --- Load current input ---
    VLOAD V1, 0(x12)        # V1 = x_input for this timestep
    
    # --- Compute W_h * h_old using VMAC ---
    VCLR V7                 # V7 = accumulator for W_h * h_old
    
    VLOAD V2, 0(x10)        # W_h row 0
    VMAC V7, V2, V0         # V7 += V2 * V0
    
    VLOAD V2, 16(x10)       # W_h row 1
    VMAC V7, V2, V0         # V7 += V2 * V0
    
    VLOAD V2, 32(x10)       # W_h row 2
    VMAC V7, V2, V0         # V7 += V2 * V0
    
    VLOAD V2, 48(x10)       # W_h row 3
    VMAC V7, V2, V0         # V7 += V2 * V0
    # V7 now contains W_h * h_old
    
    # --- Compute W_x * x_input using VMAC ---
    # We can reuse V7 as accumulator since we'll add both terms
    VLOAD V2, 0(x11)        # W_x row 0
    VMAC V7, V2, V1         # V7 += V2 * V1 (accumulating into same register!)
    
    VLOAD V2, 16(x11)       # W_x row 1
    VMAC V7, V2, V1         # V7 += V2 * V1
    
    VLOAD V2, 32(x11)       # W_x row 2
    VMAC V7, V2, V1         # V7 += V2 * V1
    
    VLOAD V2, 48(x11)       # W_x row 3
    VMAC V7, V2, V1         # V7 += V2 * V1
    # V7 now contains W_h * h_old + W_x * x_input
    
    # --- Apply activation ---
    VRELU V0, V7            # h_new = ReLU(W_h * h_old + W_x * x_input)
    # Store result back in V0 for next timestep
    
    # --- Move to next timestep ---
    ADDI x12, x12, 16       # Next input vector
    ADDI t0, t0, -1         # Decrement counter
    BNE t0, x0, RNN_TIMESTEP
    
# === OUTPUT ===
    # Store final hidden state
    VSTORE V0, 0(x13)
    
    # Classification from final hidden state
    VREDMAX x20, V0          # Max activation
    VARGMAX x21, V0          # Most active unit
    
    HALT


