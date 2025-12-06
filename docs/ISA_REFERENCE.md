# NPU Instruction Set Architecture Reference

## Overview

This document describes the custom NPU (Neural Processing Unit) ISA extension for RISC-V. All NPU instructions use opcode `0x77` (binary: `1110111`).

## Register Files

### Scalar Registers (x0-x31)

| Register | ABI Name | Description |
|----------|----------|-------------|
| x0 | zero | Hardwired zero |
| x1 | ra | Return address |
| x2 | sp | Stack pointer |
| x3 | gp | Global pointer |
| x4 | tp | Thread pointer |
| x5-x7 | t0-t2 | Temporaries |
| x8 | s0/fp | Saved/Frame pointer |
| x9 | s1 | Saved register |
| x10-x11 | a0-a1 | Arguments/Return values |
| x12-x17 | a2-a7 | Arguments |
| x18-x27 | s2-s11 | Saved registers |
| x28-x31 | t3-t6 | Temporaries |

### Vector Registers (V0-V7)

Each vector register holds 128 bits (4 × 32-bit integers).

| Register | Typical Use |
|----------|-------------|
| V0 | Accumulator |
| V1-V2 | Operands |
| V3-V5 | Temporaries |
| V6-V7 | Additional storage |

## Instruction Encoding

### R-Type (Register-Register)

```
 31    25  24  20  19  15  14  12  11   7  6    0
┌────────┬───────┬───────┬───────┬───────┬───────┐
│ funct7 │  rs2  │  rs1  │funct3 │  rd   │opcode │
│  7 bit │ 5 bit │ 5 bit │ 3 bit │ 5 bit │ 7 bit │
└────────┴───────┴───────┴───────┴───────┴───────┘
```

**Used by:** VADD, VMUL, VMAC, VRELU, VREDMAX, VARGMAX, VCLR

### I-Type (Immediate)

```
 31          20  19  15  14  12  11   7  6    0
┌──────────────┬───────┬───────┬───────┬───────┐
│  immediate   │  rs1  │funct3 │  rd   │opcode │
│    12 bit    │ 5 bit │ 3 bit │ 5 bit │ 7 bit │
└──────────────┴───────┴───────┴───────┴───────┘
```

**Used by:** VLOAD, VLBC, VCLP

### S-Type (Store)

```
 31    25  24  20  19  15  14  12  11   7  6    0
┌────────┬───────┬───────┬───────┬───────┬───────┐
│imm[11:5]│ rs2  │  rs1  │funct3 │imm[4:0]│opcode│
│  7 bit │ 5 bit │ 5 bit │ 3 bit │ 5 bit │ 7 bit │
└────────┴───────┴───────┴───────┴───────┴───────┘
```

**Used by:** VSTORE

## NPU Instructions

### Memory Operations

#### VLOAD - Vector Load

Load 128 bits (4 integers) from memory into a vector register.

```
Syntax:  VLOAD Vd, offset(rs1)
Operation: Vd[0:3] = Mem[rs1 + offset]
Encoding: I-Type, funct3 = 111
```

**Example:**
```assembly
ADDI x10, x0, 256      # x10 = 256
VLOAD V1, 0(x10)       # V1 = Mem[256..271]
```

#### VSTORE - Vector Store

Store 128 bits from a vector register to memory.

```
Syntax:  VSTORE Vs, offset(rs1)
Operation: Mem[rs1 + offset] = Vs[0:3]
Encoding: S-Type, funct3 = 111
```

#### VLBC - Vector Load Broadcast

Load a single 32-bit value and broadcast to all 4 elements.

```
Syntax:  VLBC Vd, offset(rs1)
Operation: Vd[0:3] = Mem[rs1 + offset] (replicated)
Encoding: I-Type, funct3 = 111, imm[11] = 1
```

### Arithmetic Operations

#### VADD - Vector Add

Element-wise addition of two vectors.

```
Syntax:  VADD Vd, Vs1, Vs2
Operation: Vd[i] = Vs1[i] + Vs2[i] for i = 0..3
Encoding: R-Type, funct3 = 000, funct7 = 0000000
```

#### VMUL - Vector Multiply

Element-wise multiplication of two vectors.

```
Syntax:  VMUL Vd, Vs1, Vs2
Operation: Vd[i] = Vs1[i] × Vs2[i] for i = 0..3
Encoding: R-Type, funct3 = 010, funct7 = 0000000
```

#### VMAC - Vector Multiply-Accumulate

Multiply-accumulate: multiply two vectors and add to accumulator.

```
Syntax:  VMAC Vd, Vs1, Vs2
Operation: Vd[i] = Vd[i] + (Vs1[i] × Vs2[i]) for i = 0..3
Encoding: R-Type, funct3 = 011, funct7 = 0000000
```

**Note:** This is the core operation for neural network dot products.

### Activation Functions

#### VRELU - Vector ReLU

Apply ReLU activation: max(0, x) for each element.

```
Syntax:  VRELU Vd, Vs1
Operation: Vd[i] = (Vs1[i] < 0) ? 0 : Vs1[i]
Encoding: R-Type, funct3 = 100, funct7 = 0000000
```

#### VCLP - Vector Clamp

Clamp values to a maximum threshold.

```
Syntax:  VCLP Vd, Vs1, imm
Operation: Vd[i] = min(Vs1[i], imm)
Encoding: I-Type, funct3 = 101
```

### Reduction Operations

#### VREDMAX - Vector Reduce Max

Find the maximum value in a vector.

```
Syntax:  VREDMAX rd, Vs1
Operation: rd = max(Vs1[0], Vs1[1], Vs1[2], Vs1[3])
Encoding: R-Type, funct3 = 110, funct7 = 0000000
```

**Note:** Result is written to a scalar register.

#### VARGMAX - Vector Argmax

Find the index of the maximum value.

```
Syntax:  VARGMAX rd, Vs1
Operation: rd = argmax(Vs1[0:3])
Encoding: R-Type, funct3 = 110, funct7 = 0000001
```

**Note:** Returns index (0-3) to a scalar register. Used for classification.

### Utility Operations

#### VCLR - Vector Clear

Clear a vector register to all zeros.

```
Syntax:  VCLR Vd
Operation: Vd[0:3] = 0
Encoding: R-Type, funct3 = 000, funct7 = 0000001
```

## Encoding Summary

| Instruction | Opcode | funct3 | funct7 | Type |
|-------------|--------|--------|--------|------|
| VLOAD | 1110111 | 111 | - | I |
| VSTORE | 1110111 | 111 | - | S |
| VLBC | 1110111 | 111 | 01 | I |
| VADD | 1110111 | 000 | 0000000 | R |
| VMUL | 1110111 | 010 | 0000000 | R |
| VMAC | 1110111 | 011 | 0000000 | R |
| VRELU | 1110111 | 100 | 0000000 | R |
| VCLP | 1110111 | 101 | - | I |
| VREDMAX | 1110111 | 110 | 0000000 | R |
| VARGMAX | 1110111 | 110 | 0000001 | R |
| VCLR | 1110111 | 000 | 0000001 | R |

## Decoding Logic

The decoder uses a hierarchical check:

1. **Opcode Check**: If opcode = 1110111, route to NPU decoder
2. **funct3 Check**: Determines instruction category
   - 000: VADD or VCLR (distinguished by funct7)
   - 010: VMUL
   - 011: VMAC
   - 100: VRELU
   - 101: VCLP
   - 110: VREDMAX or VARGMAX (distinguished by funct7)
   - 111: Memory operations (VLOAD, VSTORE, VLBC)
3. **funct7 Check**: For instructions sharing funct3

## Example Programs

### Dot Product

```assembly
# Compute dot product of two 4-element vectors
    ADDI x10, x0, 256      # Input address
    ADDI x11, x0, 512      # Weight address
    VCLR V0                # Clear accumulator
    VLOAD V1, 0(x10)       # Load input
    VLOAD V2, 0(x11)       # Load weights
    VMAC V0, V1, V2        # Accumulate product
    VREDMAX x20, V0        # Get max (for pooling)
```

### ReLU Activation

```assembly
# Apply ReLU to a vector
    VLOAD V1, 0(x10)       # Load data
    VRELU V1, V1           # Apply ReLU in-place
    VSTORE V1, 0(x11)      # Store result
```

### Classification

```assembly
# Get predicted class from output scores
    VLOAD V1, 0(x10)       # Load output scores
    VARGMAX x20, V1        # Get index of max (predicted class)
```

## Performance Characteristics

| Instruction | Cycles | Operations/Cycle |
|-------------|--------|------------------|
| VLOAD | 1 | 4 loads |
| VSTORE | 1 | 4 stores |
| VADD | 1 | 4 additions |
| VMUL | 1 | 4 multiplications |
| VMAC | 1 | 4 MACs |
| VRELU | 1 | 4 comparisons |
| VREDMAX | 1 | 3 comparisons |
| VARGMAX | 1 | 3 comparisons |

**Speedup over scalar:** ~4x for vector operations, ~8x for MACs (due to reduced instruction count).
