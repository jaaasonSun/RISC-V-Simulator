#include "stdio.h"
#include "cache.h"
#include "memory.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, const char* argv[]) {

    std::ifstream config(argv[2]);
    std::string opt;
    std::ofstream log(argv[3]);

    getline(config, opt);
    
    while (!config.eof()) {
        
        std::vector<Cache*> cache;
        init(cache, opt);
        std::ifstream trace(argv[1]);

        unsigned long hitCount = 0, count = 0;
        
        std::string req;
        getline(trace, req);
        while(!trace.eof()) {
            std::stringstream request(req);
            char op;
            uint64_t addr;
            request >> std::hex >> op >> addr;
            int hit, time;
            if (op == 'r') cache[0]->HandleRequest(addr, 0, 1, NULL, hit, time);
            else cache[0]->HandleRequest(addr, 0, 0, NULL, hit, time);
            hitCount += hit;
            count++;
            getline(trace, req);
        }

        StorageStats ss1, ss2, ssm;
        cache[0]->GetStats(ss1);
        cache[1]->GetStats(ss2);
        cache[1]->lower_->GetStats(ssm);
        
        auto t =  ss1.access_time + ss2.access_time + ssm.access_time;
        log << (double)t/(double)count << " "
            << (double)ss1.miss_num/(double)ss1.access_counter << " "
            << (double)ss2.miss_num/(double)ss2.access_counter << " "
            << (double)ss2.miss_num/(double)count << std::endl;
        
        getline(config, opt);
    }
	
    return 0;
}
