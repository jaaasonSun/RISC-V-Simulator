#ifndef INSTH
#define INSTH

// opcode
#define LUI    0x37
#define AUIPC  0x17
#define JAL    0x6F
#define JALR   0x67

#define OP     0x33
#define OP32   0x3B
#define IMM    0x13
#define IMM32  0x1B

#define BRANCH 0x63
#define LOAD   0x03
#define STORE  0x23

#define SYSTEM 0x73

// funct3
/// OP/OP32/IMM/IMM32
#define ADD  0
#define SHL  1
#define STL  2
#define STLU 3
#define XOR  4
#define SHR  5
#define OR   6
#define AND  7
#define MUL    0
#define MULH   1
#define MULHSU 2
#define MULHU  3
#define DIV    4
#define DIVU   5
#define REM    6
#define REMU   7
/// BRANCH
#define BEQ  0
#define BNE  1
#define BLT  4
#define BGE  5
#define BLTU 6
#define BGEU 7
/// LOAD/STORE
#define BYTE 0
#define HALF 1
#define WORD 2
#define DWRD 3
#define USGN 4

// funct7
#define ALT 0x20
#define MD  0x01 

// misc
#define EBREAK 0x00100073
#define ECALL  0x00000073
#define NOP    0x00000013

// execution cycle
#define CYCLE_DEFAULT 1
#define CYCLE_MUL32 3
#define CYCLE_MUL64 4
#define CYCLE_DIV32 4
#define CYCLE_DIV64 6

#endif
