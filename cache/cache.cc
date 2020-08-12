#include <vector>
#include <string>
#include <sstream>
#include <iostream>

#include "cache.h"
#include "def.h"
#include "memory.h"

void Cache::HandleRequest(uint64_t addr, int bytes, int read,
                          char *content, int &hit, int &time) {
    hit = 0;
    time = 0;
    auto &set = getSet(addr);
    tag_t tag = getTag(addr);
    int line = set.cacheQuery(tag);
    AccessThisLevel(time);
    stats_.access_counter++;
    
    if (line != -1) {
        hit = 1;
        CacheHit(time);
        set.access(line);
        if (read) return;
        if (!config_.write_through) {
            set.dirty(line);
            return;
        }
    } else {
        hit = 0;
        stats_.miss_num++;
        if (read || config_.write_allocate) {
            // need to evict and replace
            int line = set.getVictim();
            tag_t oldTag = set.getLine(line).tag;
            bool dirty = set.getLine(line).dirty;
            set.replace(tag, line);

            if (dirty) {
                uint64_t oldAddr = oldTag << (bits.setBits + bits.offsetBits) |
                    getBits(addr, 0, bits.offsetBits+bits.setBits);
                AccessNextLevel(oldAddr, 1<<bits.offsetBits, false, NULL, time);
                // write evicted block to lower level
            }
        
            if (!read) {
                // write && write_alloc
                // read block from lower level
                AccessNextLevel(addr, bytes, true, NULL, time);
                if (!config_.write_through) {
                    set.dirty(line);
                    return;
                }
            }
        }
    }

    // if by this time function has not returned, either
    //   1. write hit && write_through, need to write to lower
    //   2. read miss, need to read from lower
    //   3. write miss && write_no_alloc, need to write to lower
    //   4. write miss && write_alloc && write_through, need to write to lower
    AccessNextLevel(addr, bytes, read, content, time);
}

void Cache::AccessThisLevel(int &time) {
    time += latency_.bus_latency;
    stats_.access_time += latency_.bus_latency;
}

void Cache::CacheHit(int &time) {
    time += latency_.hit_latency;
    stats_.access_time += latency_.hit_latency;
}

void Cache::AccessNextLevel(uint64_t addr, int bytes, int read,
                           char *content, int &time) {
    int lower_time, lower_hit;
    lower_->HandleRequest(addr, bytes, read, content, lower_hit, lower_time);
    time += lower_time;
}

void Cache::SetConfig(CacheConfig cc) {
    config_ = cc;
    bits.setBits = powerOf2(cc.set_num);
    bits.offsetBits = powerOf2(cc.size/(cc.set_num*cc.associativity));
    sets.resize(cc.set_num);
    for (auto &set: sets) set.init(cc.associativity);
}

unsigned powerOf2(uint64_t n) {
	int p = 0;
	while(n>>=1) p++;
	return p;
}

void init(std::vector<Cache*>& cache, std::string opt)
{
	std::stringstream options(opt);
        std::cout << opt << std::endl;
	int level;
	options >> level;
	cache.resize(level);
	
	StorageStats ss;
	ss.access_time = 0;
        ss.access_counter = 0;
        ss.miss_num = 0;
	
	for (int i = 0; i < level; ++i) {
		// <SK> <AK> <NK> <WTK> <WAK> <BLK> <HLK>
		unsigned sizeBit, associativityBit, setNumBit;
		CacheConfig cc;
		StorageLatency sl;
		
		cache[i] = new Cache;
		
		cache[i]->SetStats(ss);
		
		options >> sizeBit >> associativityBit >> setNumBit >> cc.write_through
		>> cc.write_allocate >> sl.bus_latency >> sl.hit_latency;
		cc.size = 1<<sizeBit;
		cc.associativity = 1<<associativityBit;
		cc.set_num = 1<<setNumBit;
                
		cache[i]->SetConfig(cc);
		cache[i]->SetLatency(sl);
		if (i) cache[i-1]->SetLower(cache[i]);
	}
	
	auto m = new Memory;
	m->SetStats(ss);
	StorageLatency sl;
	options >> sl.bus_latency >> sl.hit_latency;
	m->SetLatency(sl);
	
	cache[level-1]->SetLower(m);
}
