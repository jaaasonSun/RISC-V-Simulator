#include <iostream>
#include <cstdio>
#include <string>
#include <vector>

#include "definitions.hpp"
#include "instructions.hpp"
#include "simple.hpp"
#include "pipe.hpp"

#include "../cache/cache.h"

using namespace std;

int main(int argc, const char* argv[])
{   
    if (!parseOptions(argc, argv)) {
	printHelp();
	return -1;
    }

    if (!loadBinary()) return -1;

    cout << "done" << std::endl;
    
    if (useCache) {
        ifstream cacheConfig("./cache.config");
        std::string opt;
        getline(cacheConfig, opt);
        cout << "done" << std::endl;
        init(cache, opt);
    }
    
    if (usePipe) startPipe();
    else startSimple();

    cout << "Simulation complete. " << endl
         << "Cycle count: " << cycleCount << endl
         << "Instruction count: " << instCount << endl
         << "CPI: " << (double)cycleCount / (double)instCount << endl;
        
    ndb();

    return 0; 
}

void startSimple()
{
    while (1) {
	fetch();
        inst = fetchedInst;
	printf("PC: %llX\tInstruction: %08X\n", pc, inst);
	if (verboseF) printReg();
	if (stepF || manualF) ndb();
	if (HALT) break;
	if (decode()) break;
        
	execute();
	mem();
	write();
	PCupdate();
        cycleCount+=CYCLE_DEFAULT;
        instCount++;
    }
}

void startPipe()
{
    while (1) {
        // fetch an instruction normally
        fetch();
        // move pipeline one stage and push fetch inst in it
        movePipe();
        // generate signal needed to detect hazard
        genSignal();

        inst = pipeline[0].inst;

        showPipe();

        // detect hazard
        resolveHazard();
        
	if (verboseF) printReg();
	if (stepF || manualF) ndb();
	if (HALT) break;
	if (decode()) break;

        // execute inst as in simple processor
	execute();
	mem();
	write();
	PCupdate();
        cycleCount+=CYCLE_DEFAULT;
        instCount++;
    }
}

void ndb()
{
    char a;
    uint64_t memAddr;
    size_t length;
    unsigned int repeat;

    while(1) {
	cout << "(ndb) ";
	       cin >> a;
	switch(a) {
	case 'q': HALT = 1; return;
	case 'c': stepF = false; return;
        case 's': return;
	case 'r': printReg(); break;
	case 'm':
	    cin >> hex >> memAddr;
	    cin >> length >> dec >> repeat;
            for (int i = 0; i < repeat; ++i)
                printf("%llX\n", getMem(memAddr+i*length, length, INHIBIT));
	    break;
	case 'd': debugInsF = true; break;
	default: break;
	}
    }
}

void fetch()
{
    if (manualF) cin >> hex >> fetchedInst;
    else fetchedInst = getMem(pc, 4, INST);
}

bool decode()
// return true on EBREAK or unknown instruction
{
    op = getBits(inst, 0, 6);
    f3 = getBits(inst, 12, 14);
    uint32_t f7 = getBits(inst, 25, 31);
    uint32_t rd = getBits(inst, 7, 11);
    uint32_t rs1 = getBits(inst, 15, 19);
    uint32_t rs2 = getBits(inst, 20, 24);

    uint64_t immI = signedExt(getBits(inst, 20, 31), 12);
    uint64_t immS = signedExt((rd | f7<<5), 12);
    uint32_t immBU = getBits(inst, 8, 11)<<1 | getBits(inst, 25, 30)<<5 |
	getBits(inst, 7, 7)<<11 | getBits(inst, 31, 31)<<12;
    uint64_t immB = signedExt(immBU, 13);
    uint32_t immU = signedExt(getBits(inst, 12, 31)<<12, 32);
    uint32_t immJU = getBits(inst, 21, 30)<<1 | getBits(inst, 20, 20)<<11 |
	getBits(inst, 12, 19)<<12 | getBits(inst, 31, 31)<<20;
    uint64_t immJ = signedExt(immJU, 21);
    
    // verify the instruction before access registers
    // to prevent segmentation fault. this is ugly
    switch(op) {
    case SYSTEM: case OP: case IMM: case OP32: case IMM32: case BRANCH:
    case LOAD: case STORE: case LUI: case AUIPC: case JAL: case JALR: break;
    default:
	printf("Unknown instruction: %08X\n", inst);
	return true;
    }
    
    // default case
    op1 = reg[rs1];
    op2 = reg[rs2];
    src2 = reg[rs2];
    operation = ADD;
    modifier = 0;
    regWr = true;
    dst = rd;
    memWr = false;
    memRd = false;
    branch = (op & 0xf0) == 0x60;
    cond = false;

    switch(op) {
    case SYSTEM:
	if (inst == EBREAK) return true;
	if (inst == ECALL) {
	    switch (reg[10]) {
            case 1:
                std::cout << "Value of register a1: " << std::hex << reg[11] << std::endl;
                break;
            case 10:
                return true;
            default:
                std::cout << "Unknown ecall parameter: " << std::hex << reg[10] << std::endl;
                return true;
            }
            regWr = false;
            inst = NOP;
	}
	break;
    case OP: case IMM: case OP32: case IMM32:
	if ((op & OP) != OP) op2 = immI;
	if ((op & IMM32) == IMM32) {
	    op1 = signedExt(op1, 32);
	    op2 = signedExt(op2, 32);
	}
	operation = f3;
	modifier = f7;
	break;
    case BRANCH:
	jmpDes = pc + immB;
        operation = f3;
	regWr = false;
	break;
    case LOAD:
	memRd = true;
	op2 = immI;
	break;
    case STORE:
	regWr = false;
	memWr = true;
	op2 = immS;
	break;
    case LUI: case AUIPC:
	if (op == LUI) op1 = 0;
	else op1 = pc;
	op2 = immU;
	break;
    case JAL: case JALR:
	op1 = pc;
	op2 = 4;
	if (op == JAL) jmpDes = pc + immJ;
	else jmpDes = reg[rs1] + immI;
	cond = true;
	break;
    default:
	cout << "Unknown instruction: " << hex << inst << endl;
	return true;
    }
    
    return false;
}

void execute()
{
    uint64_t res;
    bool is32 = (op & IMM32) == IMM32;
    bool isMul = (op & OP) == OP && modifier == MD;
    bool isBranch = op == BRANCH;
    int shamt = op2 & 0x2f;

    if (debugInsF)
	printf("Execute: op1 %lld, op2 %lld, operation %d, modifier %d\n",
	       op1, op2, operation, modifier);

    
    if (isBranch) {
	res = op1 - op2;
	switch(operation) {
	case BEQ: cond = res == 0; break;
	case BNE: cond = res != 0; break;
	case BLT: cond = (int64_t)res < 0; break;
	case BGE: cond = (int64_t)res >= 0; break;
	case BLTU: cond = op1 < op2; break;
	case BGEU: cond = op1 >= op2; break;
        default: cout << "simulator has a bug" << endl;
	}
        if (usePipe && cond) branchTaken();
    } else if (isMul) {
	switch(operation) {
	case MUL: res = op1 * op2; break;
	case MULH: res = ((__int128_t)op1 * (__int128_t)op2)>>64; break;
	case MULHSU: res = ((__int128_t)op1 * (__uint128_t)op2)>>64; break;
	case MULHU: res = ((__uint128_t)op1 * (__uint128_t)op2)>>64; break;
	case DIV: res = (int64_t)op1 / (int64_t)op2; break;
	case DIVU:
	    if (is32) res = signedExt((uint32_t)op1 / (uint32_t)op2, 32);
	    else res = op1 / op2;
	    break;
	case REM: res = (int64_t)op1 % (int64_t)op2; break;
	case REMU:
	    if (is32) res = signedExt((uint32_t)op1 % (uint32_t)op2, 32);
	    else res = op1 % op2;
	    break;
	default: res = 0; break;
	}
        switch(operation) {
	case MUL: case MULH: case MULHSU: case MULHU:
            if (is32) cycleCount += CYCLE_MUL32 - CYCLE_DEFAULT;
            else cycleCount += CYCLE_MUL64 - CYCLE_DEFAULT;
            if (verboseF) std::cout << "multiplication instruction" << std::endl;
            break;
	case DIV: case DIVU: case REM: case REMU:
            if (is32) cycleCount += CYCLE_DIV32 - CYCLE_DEFAULT;
            else cycleCount += CYCLE_DIV64 - CYCLE_DEFAULT;
            if (verboseF) std::cout << "division instruction" << std::endl;
	}
    } else {
	switch(operation) {
	case ADD:
	    if (modifier == ALT) res = op1-op2;
	    else res = op1+op2;
	    break;
	case SHL: res = op1 << shamt; break;
	case STL: res = (int64_t)op1 < (int64_t)op2; break;
	case STLU: res = op1 < op2; break;
	case XOR: res = op1 ^ op2; break;
	case SHR:
	    if (modifier == ALT) res = (int64_t)op1 >> shamt; // arithmetic
	    else if (is32) res = (op1 & 0xffff) >> shamt; // 32 logical
	    else res = op1 >> shamt; // 64 logical
	    break;
	case OR: res = op1 | op2; break;
	case AND: res = op1 & op2; break;
	default: res = 0; break;
	}
    }

    if (debugInsF)
	printf("Execute: result %lld\n", res);
    
    aluRes = res;
}

void mem()
{
    size_t size = 1 << (f3 & 0x3);
    int time;
    if (memRd) {
	memRes = getMem(aluRes, size, DATA, &time);
	if (!(operation & 0x4)) //signed extension
	    memRes = signedExt(memRes, size*8);
    } else if (memWr) setMem(aluRes, src2, size, DATA, &time);
    else memRes = aluRes; // pass alu result so write stage can be simpler
    if (useCache)
        cycleCount += time - CYCLE_DEFAULT;
}

void write()
{
    if (regWr && dst) reg[dst] = memRes;
}

void PCupdate()
{
    if (branch && cond) pc = jmpDes;
    else pc += 4;
}

bool loadBinary()
{
    if (reader.get_class() != ELFCLASS64 ||
	reader.get_type() != ET_EXEC ||
	reader.get_machine() != 0xF3) {
	cout << "This simulator only supports 64bit RISC-V executable" << endl;
	return false;
    }

    FILE* fp = fopen(argVec[0], "r");

    for (const auto& seg: reader.segments) {
	if (seg->get_type() != PT_LOAD) continue;
	fseek(fp, seg->get_offset(), SEEK_SET);
	fread(memory+seg->get_virtual_address(), 1, seg->get_file_size(), fp);
    }

    for (const auto& sec: reader.sections) {
	if (sec->get_type() == SHT_SYMTAB) {
	    // set pc and gp
	    const ELFIO::symbol_section_accessor symtab(reader, sec);
	    ELFIO::Elf64_Sym s;
	    string name;
	    bool pcSet = false, gpSet = false;

	    for (unsigned long long j = 0; j < symtab.get_symbols_num(); ++j ) {
		symtab.get_symbol(j, name, s.st_value, s.st_size, s.st_info,
				  s.st_info, s.st_shndx, s.st_other);
	        if (!pcSet && name == MAINSYM) {
		    pc = s.st_value;
		    pcSet = true;
		} else if (!gpSet && name == GPSYM) {
		    reg[3] = s.st_value;
		    gpSet = true;
		}
		if (pcSet && gpSet) break;
	    }

	    if (!pcSet) {
		cout << "cannot find \"main\"" << endl;
		return false;
	    }
	    if (!gpSet) {
		cout << "cannot find global pointer" << endl;
		return false;
	    }
	    
	    break;
	}
    }

    fclose(fp);

    // set up command line arguments, put pointers in the middle of memory
    // and strings right behind pointers. hopefully text segment, arguments
    // and user stack will not clash
    reg[10] = argCount; // fucntion argument register
    reg[11] = STRBEGIN;
    long strPos = STRBEGIN + 8*argCount;
    for (int i = 0; i < argCount; ++i) {
	setMem(STRBEGIN+i*8, strPos, 8, INHIBIT);
	strcpy((char*)memory+strPos, argVec[i]);
	strPos += strlen(argVec[i]);
    }
    
    // set up stack, set return address to a special address (with EBREAK inst)
    // so we can detect when main funciton end
    setMem(ENDADDR, EBREAK, 4, INHIBIT);
    reg[1] = ENDADDR;
    reg[2] = STACKBEGIN;
    
    return true;
}


bool parseOptions(int argc, const char* argv[])
{
    if (argc < 2) return false;
    for (int i = 1; i < argc; ++i) {
	if ( argv[i][0] != '-') { // is not an option, assume it’s the binary
	    if (!reader.load(argv[i])) {
		cout << "binary not found: " << argv[i] << endl;
		return false;
	    }
	    argVec = argv+i;
	    argCount = argc-i;
	    return true;
	}
	
	if (strlen(argv[i]) < 2) return false;
	switch(argv[i][1]) {
	case helpOpt: return false;
	case memInfoOpt: memInfoF = true; break;
	case stepOpt: stepF = true; break;
	case verboseOpt: verboseF = true; break;
	case debugOpt: debugInsF = true; break;
        case pipeOpt: usePipe = true; break;
        case cacheOpt: useCache = true; break;
	default:
	    cout << "Unrecgnized option: " << char(argv[i][1]) << endl;
	    return false;
	}
    }
    return false;
}
