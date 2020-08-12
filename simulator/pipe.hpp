#include <string>

#include "instructions.hpp"
#include "definitions.hpp"

// pipeStage is used to detect data hazard.
// it reflects actual instruction, i.e. taken branch.
// instructions are executed before being put in ’pipeline’
// (so pipeline implementation is much easier)
// nops are inserted according to hazard resolution but never executed
struct PipeStage {
    uint64_t pc;
    uint32_t inst;
    bool Rs1InUse, Rs2InUse;
    unsigned Rs1, Rs2, Rd;
    bool MemRead;
    bool isJump;
};

PipeStage nop = {
    0, NOP, false, false, 0, 0, 0, false, false
};

#define IF 0
#define ID 1 
#define EX 2
#define ME 3
#define WB 4

PipeStage pipeline[5];
extern uint32_t fetchedInst;
extern unsigned long long cycleCount;
extern uint64_t pc;

void showPipe()
{
    printf("Pipeline PC: ");
    for (int i = 0; i < 5; ++i)
        printf("%llX ", pipeline[i].pc);
    printf("\n");
}

void movePipe()
{
    for (int _i = 4; _i > 0; --_i)
        pipeline[_i] = pipeline[_i-1];
    pipeline[0].inst = fetchedInst;
}

// generate a small number of signals for hazard detection
void genSignal()
{
    uint32_t op = getBits(pipeline[IF].inst, 0, 6);
    pipeline[IF].pc = pc;
    
    if (op == LUI || op == AUIPC || op == JAL)
        pipeline[IF].Rs1InUse = false;
    else pipeline[IF].Rs1InUse = true;
    
    if (OP == LUI || op == AUIPC || op == JAL || op == JALR ||
        op == LOAD || op == IMM || op == IMM32)
        pipeline[IF].Rs2InUse = false;
    else pipeline[IF].Rs2InUse = true;
    
    pipeline[IF].Rs1 = getBits(pipeline[IF].inst, 15, 19);
    pipeline[IF].Rs2 = getBits(pipeline[IF].inst, 20, 24);
    pipeline[IF].Rd = getBits(pipeline[IF].inst, 7, 11);

    pipeline[IF].MemRead = op == LOAD;
    pipeline[IF].isJump = op == JAL || op == JALR;
}

void resolveHazard()
{
    // load-use hazard: stall one cycle between load and use
    // detection:
    //   ID/EX.MemRead && ( ID/EX.Rt == IF/ID.Rs || ID/EX.Rt == IF/ID.Rt)
    if (pipeline[ID].MemRead &&
        ((pipeline[IF].Rs1InUse && pipeline[ID].Rd == pipeline[IF].Rs1) ||
         (pipeline[IF].Rs2InUse && pipeline[ID].Rd == pipeline[IF].Rs2))) {
        std::cout << "Load/use hazard detected" << std::endl;
        pipeline[WB] = pipeline[ME];
        pipeline[ME] = pipeline[EX];
        pipeline[EX] = pipeline[ID];
        pipeline[ID] = nop;
        cycleCount++;
        showPipe();
    }

    // JALR/JAL: calculate jump target at execute stage with 2 stalls
    if (pipeline[IF].isJump) {
        std::cout << "Jump detected" << std::endl;
        pipeline[WB] = pipeline[EX];
        pipeline[ME] = pipeline[ID];
        pipeline[EX] = pipeline[IF];
        pipeline[ID] = pipeline[IF] = nop;
        cycleCount+=2;
        showPipe();
    }

}

// for simplicity, the simulator always predict branch not taken, so pipeline has
// to be flushed at a taken branch
void branchTaken()
{
    std::cout << "Branch mis-prediction" << std::endl;
    pipeline[WB] = pipeline[EX];
    pipeline[ME] = pipeline[ID];
    pipeline[EX] = pipeline[IF];
    pipeline[ID] = pipeline[IF] = nop;
    cycleCount+=2;

    showPipe();
}
