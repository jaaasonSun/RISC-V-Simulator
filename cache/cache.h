#ifndef CACHE_CACHE_H_
#define CACHE_CACHE_H_

#include <stdint.h>
#include <vector>
#include <climits>
#include "storage.h"

#define getBits(bits, begin, end) \
    (begin==end)?0:(bits <<(sizeof(bits)*8-(end))>>(sizeof(bits)*8-(end)+(begin)))

typedef struct CacheConfig_ {
  int size;
  int associativity;
  int set_num; // Number of cache sets
  int write_through; // 0|1 for back|through
  int write_allocate; // 0|1 for no-alc|alc
} CacheConfig;

typedef uint64_t tag_t;

class Cache: public Storage {
public:
    Cache() {}
    ~Cache() {}

    // Sets & Gets
    void SetConfig(CacheConfig cc);
    void GetConfig(CacheConfig cc);
    void SetLower(Storage *ll) { lower_ = ll; }
    // Main access process
    void HandleRequest(uint64_t addr, int bytes, int read,
                       char *content, int &hit, int &time);

    struct Config {
        size_t offsetBits;
        size_t setBits;
    } bits;
    
    struct CacheLine {
        tag_t tag;
        unsigned lastUse = UINT_MAX;
        bool valid = false;
        bool dirty = false;
    };

    class CacheSet {
        std::vector<CacheLine> set;

    public:        
        int cacheQuery(tag_t tag) {
            for (size_t i = 0; i < set.size(); ++i)
                if (set[i].tag == tag && set[i].valid) return i;
            return -1;
        }

        int getVictim() {
            unsigned maxIndex = 0;
            unsigned maxUse = 0;
            for (unsigned i = 0; i < set.size(); ++i)
                if (!set[i].valid) return i;
                else if (set[i].lastUse > maxUse) maxIndex = i;
            return maxIndex;
        }
        
        bool replace(tag_t tag, size_t index) {
            access(index);
            auto &line = set[index];
            line.tag = tag;
            line.valid = true;
            bool dirty = line.dirty;
            line.dirty = false;
            return dirty;
        }

        void access(unsigned index) {
            for (unsigned i = 0; i < set.size(); ++i)
                if (set[i].lastUse < UINT_MAX) // prevent overflow
                    set[i].lastUse++;
            set[index].lastUse = 0;
        }

        void dirty(unsigned index) {
            set[index].dirty = true;
        }

        void init(size_t lineNum) {
            set.resize(lineNum);
            for (auto &line: set) {
                line.lastUse = UINT_MAX;
                line.valid = false;
                line.dirty = false;
            }
        }

        CacheLine& getLine(unsigned index) {
            return set[index];
        }
    };

    std::vector<CacheSet> sets;

    tag_t getTag(uint64_t mem) {
        return getBits(mem, bits.setBits+bits.offsetBits, 64);
    }
    CacheSet& getSet(uint64_t mem) {
        uint64_t set = getBits(mem, bits.offsetBits, bits.offsetBits+bits.setBits);
        return sets[set];
    }

    // Replacement
    int ReplaceDecision(bool, CacheSet&, tag_t);
    bool ReplaceAlgorithm(bool, CacheSet&, tag_t);

    void AccessThisLevel(int&);
    void CacheHit(int&);
    void AccessNextLevel(uint64_t, int, int, char*, int&);
    
    CacheConfig config_;
    Storage *lower_;
    DISALLOW_COPY_AND_ASSIGN(Cache);
};

unsigned powerOf2(uint64_t);
void init(std::vector<Cache*>&, std::string);

#endif //CACHE_CACHE_H_ 
