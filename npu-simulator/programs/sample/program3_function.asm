# ==========================================
# PROGRAM 3: Reusable Function
# ==========================================
# Description: Implements a reusable function named FUNC_PROCESS that performs
# standard post-processing for AI Inference: ReLU activation followed by
# threshold/clamp operation.
#
# Calling Convention:
#   - Input: Data in V1
#   - Output: Processed data in V1
#   - Return Address: ra register
#
# Memory Layout:
#   0x100 (256): Input data (may contain negative values)
#   0x110 (272): Output result
#
# Test Data:
#   Input: [-5, 20, -10, 300] (contains negatives and value > 255)
#
# Expected Result:
#   After ReLU:  [0, 20, 0, 300]
#   After Clamp: [0, 20, 0, 255]

.text
.globl main

# --- Main Code (Caller) ---
main:
    # 1. Prepare test data with negatives
    # We'll manually load test values since memory is pre-initialized
    ADDI x10, x0, 256
    VLOAD V1, 0(x10)
    
    # 2. Call the Function
    # Jump to FUNC_PROCESS and save return address in 'ra'
    JAL ra, FUNC_PROCESS
    
    # 3. Store Result (V1 contains the processed data)
    ADDI x11, x0, 272
    VSTORE V1, 0(x11)
    
    # 4. Get max value and its index for classification
    VREDMAX x20, V1
    VARGMAX x21, V1
    
    # Stop
    J END_SIM

# --- The Reusable Function ---
# INPUT: V1 (Data vector)
# OUTPUT: V1 (Processed vector)
FUNC_PROCESS:
    # Step 1: ReLU Activation
    # If element < 0, set to 0
    VRELU V1, V1
    
    # Step 2: Clamping (Thresholding)
    # If element > 255, set to 255
    VCLP V1, V1, 255
    
    # Step 3: Return
    # Jump back to the address stored in 'ra'
    JALR x0, ra, 0

END_SIM:
    HALT
