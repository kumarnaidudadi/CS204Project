# RISC-V Assembler (Phase 1)

## Project Details

- **Course**: CS204 - Computer Architecture
- **Phase 1**: Assembly to Machine Code Conversion

### Submitted by:
1. **Ch.V.Rithish Reddy** (2023CSB1113)
2. **Dadi Kumar Naidu** (2023CSB1115)
3. **Polisetty Tharun Sai** (2023CSB1144)

## Overview

This project implements a **32-bit RISC-V assembler** that converts assembly language instructions into machine code. The assembler processes an `.asm` file and generates a corresponding `.mc` file with machine code output. 

### Features:
- Supports **31 RISC-V instructions** (R, I, S, SB, U, and UJ formats)
- Implements basic assembler directives: `.text`, `.data`, `.byte`, `.half`, `.word`, `.dword`, `.asciz`
- Two-pass assembler to handle labels and address calculations
- Outputs machine code in a format similar to the **Venus assembler**

## Input Format (`input.asm`)

The input is a RISC-V assembly file with one instruction per line. Example:

```assembly
.data
arr: .word 10 , 20 

.text
add x1, x2, x3
andi x5, x6, 10
beq x1, x5, label
label:
    addi x7, x0, 5
```

## Output Format (`output.mc`)

The output file contains machine code in the following format:

```
<address> <machine_code> , <assembly_instruction> # <binary_encoding_details>
```

Example:

```
0x10000000 0x0000000a
0x10000004 0x00000014
0x0 0x003100B3 , add x1,x2,x3 # 0110011-000-0000000-00001-00010-00011-NULL
0x4 0x00A37293 , andi x5,x6,10 # 0010011-111-NULL-00101-00110-000000001010
0x8 0x00508263 , beq x1,x5,label # 1100011-000-NULL-00001-00101-0000000000100
0xc 0x00500393 , addi x7,x0,5 # 0010011-000-NULL-00111-00000-000000000101
TERMINATED
```

## How to Run

### 1. Compile the Assembler
  **Note** - use Standard C++ 17 

```sh
g++ assembler.cpp -o assembler
```

### 2. Run the Assembler

```sh
./assembler
```

The assembler will process `input.asm` and generate `output.mc`.

## Supported Instructions

The assembler supports **31 RISC-V instructions**:

- **R-format**: `add`, `and`, `or`, `sll`, `slt`, `sra`, `srl`, `sub`, `xor`, `mul`, `div`, `rem`
- **I-format**: `addi`, `andi`, `ori`, `lb`, `ld`, `lh`, `lw`, `jalr`
- **S-format**: `sb`, `sw`, `sd`, `sh`
- **SB-format**: `beq`, `bne`, `bge`, `blt`
- **U-format**: `auipc`, `lui`
- **UJ-format**: `jal`

## Limitations and Constraints

- Labels are resolved in a **two-pass process**.
- **Pseudo-instructions** are **not** supported.
- **Floating-point operations** are **not** supported.
