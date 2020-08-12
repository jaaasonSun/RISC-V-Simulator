#include <string>
#include <cstdio>
#include <vector>
#include <elfio/elfio.hpp>

#include "../cache/cache.h"

#ifndef DEFH
#define DEFH

using namespace std;

const long MEMSIZE = 1<<20;
const long STRBEGIN = MEMSIZE/2;
const long STACKBEGIN = MEMSIZE-16;
const long ENDADDR = MEMSIZE-8;
const string MAINSYM = "main";
const string GPSYM = "__global_pointer$";
const int cycleTime = 1;

enum Option {
    memInfoOpt = 'm',
    stepOpt = 's',
    helpOpt = 'h',
    verboseOpt = 'v',
    debugOpt = 'd',
    pipeOpt = 'p',
    cacheOpt = 'c'
};

ELFIO::elfio reader;

// global statistics
unsigned long long instCount = 0;
unsigned long long cycleCount = 0;

// global options
bool usePipe = 0;
bool manualF = 0;
bool stepF = 0;
bool HALT = 0;
bool verboseF = 0;
bool debugInsF = 0;
bool memInfoF = 0;
bool useCache = 0;

// command line arguments
const char** argVec = NULL;
int argCount = 1;

// registers
uint64_t pc;
uint64_t reg[32];

uint8_t memory[MEMSIZE];

std::vector<Cache*> cache;

void fetch();
bool decode();
void execute();
void mem();
void write();
void PCupdate();

string regName[32] = { "zr", "ra", "sp", "gp", "tp",
		       "t0", "t1", "t2", "s0", "s1",
		       "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
		       "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
		       "t3", "t4", "t5", "t6" };

// debugging functions
void ndb();
void printReg()
{
    for (int i = 0; i < 8; ++i) {
	for (int j = 0; j < 4; ++j)
	    printf("%s: %8llX\t", regName[i*4+j].c_str(), reg[i*4+j]);
	printf("\n");
    }
}


// utility functions
bool loadBinary();
bool parseOptions(int argc, const char* argv[]);

void printHelp()
{
    cout << "Usage: simulator [opt1 [opt2 [...]]] <binary> [arg1 [arg2 [...]]]" << endl;
    cout << "Options:" << endl;
    cout << "\t-h\tprint this message" << endl;
    cout << "\t-p\tuse pipeline" << endl;
    cout << "\t-v\tverbose mode" << endl;
    cout << "\t-s\tstep mode" << endl;
    cout << "\t-m\tdisplay memory access" << endl;
}

// inline uint32_t getBits(uint32_t value, int b, int e) // both end included
// {
//     return value << (31-e) >> (31-e+b);
// }

#define getBits(value, b, e) (value << (31-e) >> (31-e+b))

enum MemType {
    INST, DATA, INHIBIT
};

inline void setMem(long pos, uint64_t value, size_t length, MemType memtype = DATA, int *time = NULL)
{
    switch(length) {
    case 1: *(uint8_t  *)(memory+pos) = (uint8_t )value; break;
    case 2: *(uint16_t *)(memory+pos) = (uint16_t)value; break;
    case 4: *(uint32_t *)(memory+pos) = (uint32_t)value; break;
    case 8: *(uint64_t *)(memory+pos) = (uint64_t)value; break;
    }
    switch (memtype) {
    case DATA:
        if (memInfoF)
            printf("Write mem pos %lX, width %ld, value %llX\n",
                       pos, length, value);
        if (useCache) {
            int h, t;
            cache[0]->HandleRequest(pos, length, false, NULL, h, t);
            if (time) *time = t;
            if (memInfoF)
                printf("Cache %shit, time %d\n", (h?"":"not"), t);
        }
        break;
    case INST: case INHIBIT: break;
    default: break;
    }
}

inline uint64_t getMem(long pos, size_t length, MemType memtype = DATA, int *time = NULL)
{   
    uint64_t res;
    switch(length) {
    case 1: res = *(uint8_t  *)(memory+pos); break;
    case 2: res = *(uint16_t *)(memory+pos); break;
    case 4: res = *(uint32_t *)(memory+pos); break;
    case 8: res = *(uint64_t *)(memory+pos); break;
    default: res = 0;
    }
    switch (memtype) {
    case DATA:
        if (memInfoF)
            printf("Read mem pos %lX, width %ld, result %llX\n",
                      pos, length, res);
        if (useCache) {
            int h, t;
            cache[0]->HandleRequest(pos, length, true, NULL, h, t);
            if (time) *time = t;
            if (memInfoF)
                printf("Cache %shit, time %d\n", (h?"":"not"), t);
        }
        break;
    case INST: case INHIBIT: break;
    default: break;
    }

    return res;
}

inline uint64_t signedExt(uint64_t u, size_t length)
{
    int bits = 64-length;
    int64_t ext = (int64_t)u << bits >> bits;
    return (uint64_t)ext;
}

void startSimple();
void startPipe();

#endif
