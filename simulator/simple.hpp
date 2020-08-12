#include <string>

// fetch
uint32_t fetchedInst;
uint32_t inst;
// decode
uint64_t src2;
uint32_t f3;
unsigned op;
unsigned dst;
bool cond;
bool branch;
// alu
uint64_t op1, op2;
uint32_t operation, modifier;
// mem
bool memWr, memRd;
uint64_t aluRes;
// write
bool regWr;
uint64_t memRes;
// update
uint64_t jmpDes;
