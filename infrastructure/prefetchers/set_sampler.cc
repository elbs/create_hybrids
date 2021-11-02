#include "set_sampler.h"

bool SAMPLER::in_sampler(uint64_t addr){
    uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
    uint64_t set = (baddr & ((1 << (SET_BITS)) - 1));

    if(set % SET_SELECT != 0){
        return false;
    }
    return true;
}

bool SAMPLER::is_pf(uint64_t addr){
  int way = get_way(addr);
  uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
  uint64_t set = (baddr & ((1 << (SET_BITS)) - 1));
  set = set/SET_SELECT;

  if(way == -1)
    return false;
  else
    return sample_blocks[set][way].prefetch;

}
int SAMPLER::get_set(uint64_t addr){  
  uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
  uint64_t set = (baddr & ((1 << (SET_BITS)) - 1));
  return set;
}

int SAMPLER::get_way(uint64_t addr){
    uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
    uint64_t set = (baddr & ((1 << (SET_BITS)) - 1));
    set = set/SET_SELECT;
    
    //check if its a hit
    for(int a = 0; a < SAMPLE_WAY; a++){
        //Hit!
        if(sample_blocks[set][a].valid && sample_blocks[set][a].tag == baddr){
            return a;
        }
    }

    return -1;
}

bool SAMPLER::trigger_pf(uint64_t addr){
  uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
  uint64_t set = (baddr & ((1 << (SET_BITS)) - 1));
  int way = get_way(addr);

  if(way != -1 && !sample_blocks[set][way].prefetch)
    return false;
    
  return true;
    
}

uint64_t SAMPLER::update_sampler(uint64_t addr, bool prefetch){
    uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
    uint64_t set = (baddr & ((1 << (SET_BITS)) - 1));

    if(set % SET_SELECT != 0){
        return 0;
    }

    set = set/SET_SELECT;

    if(set >= SAMPLE_SET)
        printf("set %lu\n", set);
    assert(set < SAMPLE_SET);
    //check if its a hit
    int way = get_way(addr);

    //if its a hit, update LRU
    if(way != -1){

        int tgt_lru = sample_blocks[set][way].lru;

        for(int a = 0; a < SAMPLE_WAY; a++){
            if(sample_blocks[set][a].valid && sample_blocks[set][a].lru < tgt_lru){
                sample_blocks[set][a].lru++;
                assert(sample_blocks[set][a].lru < SAMPLE_WAY);
            }
        }

        sample_blocks[set][way].lru = 0;
        if(sample_blocks[set][way].prefetch){
          sample_blocks[set][way].used = true;
        }else{
          sample_blocks[set][way].prefetch = prefetch;
          sample_blocks[set][way].used = false;
        }
        return 0;
    }else{
        //Otherwise find a victim
        int vic = -1;

        for(int a = 0; a < SAMPLE_WAY; a++){
            if(!sample_blocks[set][a].valid || sample_blocks[set][a].lru == SAMPLE_WAY-1){
                vic = a;
                break;
            }
        }

        assert(vic != -1);

        for(int a = 0; a < SAMPLE_WAY; a++){
            if(sample_blocks[set][a].valid && a != vic){
                sample_blocks[set][a].lru++;
                if(sample_blocks[set][a].lru >= SAMPLE_WAY){
                    assert(sample_blocks[set][a].lru < SAMPLE_WAY);
                }
            }
        }

        uint64_t vic_tag = sample_blocks[set][vic].tag;
        bool was_v =  sample_blocks[set][vic].valid;
        //bool was_used = sample_blocks[set][vic].used;
        bool was_used = sample_blocks[set][vic].used;
        bool was_prefetch = sample_blocks[set][vic].prefetch;

        sample_blocks[set][vic].valid = 1;
        sample_blocks[set][vic].lru = 0;
        sample_blocks[set][vic].tag = baddr;
        sample_blocks[set][vic].prefetch = prefetch;
        sample_blocks[set][vic].used = false;
         
        if(was_v && was_prefetch && !was_used)
          return vic_tag;//sample_blocks[set][vic].tag;
        else
          return 0;
    }
}

