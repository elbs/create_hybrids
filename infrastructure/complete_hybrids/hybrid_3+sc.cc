#include "ooo_cpu.h"
#include "prefetch_buffer.h"
#include "shadow_cache.h"
#include <list>
#include <map>

/*
 ================================================================================
||  UNCOMMENT LINE BELOW TO MEASURE OVERLAPPING HITS FOR FNL, DJOLT, and BARCA  ||
||  ALSO MEASURE PRESSURE IN THE PREFETCH BUFFER BASED ON THE EPOCH AND SAMPLE  ||
||  VALUES BELOW DEF LINES                                                      ||
|| ---------------------------------------------------------------------------- ||
 ================================================================================
*/
//#define MEASURE                                                      

using namespace std;

// First Prefetcher: \#defines help create individually named 
// functions for each prefetcher
#define l1i_prefetcher_branch_operate l1i_prefetcher_branch_operate1
#define l1i_prefetcher_cache_fill l1i_prefetcher_cache_fill1
#define l1i_prefetcher_cache_operate l1i_prefetcher_cache_operate1
#define l1i_prefetcher_cycle_operate l1i_prefetcher_cycle_operate1
#define l1i_prefetcher_final_stats l1i_prefetcher_final_stats1
#define l1i_prefetcher_initialize l1i_prefetcher_initialize1
#define prefetch_code_line prefetch_code_line1

#include XXX

#undef l1i_prefetcher_branch_operate
#undef l1i_prefetcher_cache_fill
#undef l1i_prefetcher_cache_operate
#undef l1i_prefetcher_cycle_operate
#undef l1i_prefetcher_final_stats
#undef l1i_prefetcher_initialize
#undef prefetch_code_line

// Second Prefetcher: \#defines help create individually named 
// functions for each prefetcher
#define l1i_prefetcher_branch_operate l1i_prefetcher_branch_operate2
#define l1i_prefetcher_cache_fill l1i_prefetcher_cache_fill2
#define l1i_prefetcher_cache_operate l1i_prefetcher_cache_operate2
#define l1i_prefetcher_cycle_operate l1i_prefetcher_cycle_operate2
#define l1i_prefetcher_final_stats l1i_prefetcher_final_stats2
#define l1i_prefetcher_initialize l1i_prefetcher_initialize2
#define prefetch_code_line prefetch_code_line2

#include YYY

#undef l1i_prefetcher_branch_operate
#undef l1i_prefetcher_cache_fill
#undef l1i_prefetcher_cache_operate
#undef l1i_prefetcher_cycle_operate
#undef l1i_prefetcher_final_stats
#undef l1i_prefetcher_initialize
#undef prefetch_code_line

// Third Prefetcher: \#defines help create individually named 
// functions for each prefetcher
#define l1i_prefetcher_branch_operate l1i_prefetcher_branch_operate3
#define l1i_prefetcher_cache_fill l1i_prefetcher_cache_fill3
#define l1i_prefetcher_cache_operate l1i_prefetcher_cache_operate3
#define l1i_prefetcher_cycle_operate l1i_prefetcher_cycle_operate3
#define l1i_prefetcher_final_stats l1i_prefetcher_final_stats3
#define l1i_prefetcher_initialize l1i_prefetcher_initialize3
#define prefetch_code_line prefetch_code_line3

#include ZZZ

#undef l1i_prefetcher_branch_operate
#undef l1i_prefetcher_cache_fill
#undef l1i_prefetcher_cache_operate
#undef l1i_prefetcher_cycle_operate
#undef l1i_prefetcher_final_stats
#undef l1i_prefetcher_initialize
#undef prefetch_code_line

// Three prefetchers + shadow cache
const uint32_t num_prefetchers = 3;

// An array of three lists, one for each prefetcher
list<uint64_t> my_prefetch_queue[num_prefetchers];

PREFETCH_BUFFER pfb(num_prefetchers);

// Elba: Made the shadow cache a class
SHADOW_CACHE sc;

#ifdef MEASURE

SHADOW_CACHE fnl_sc;
SHADOW_CACHE djolt_sc;
SHADOW_CACHE barca_sc;
SHADOW_CACHE base_sc;

//number of possible hit scenarios between
//the 4 shadow caches
const int HIT_STATES = 16;

//Number of shadow caches measuring
const int NUM_MEASURE = 4;

//CONSTANTS FOR PRINTING
//Bit vectors are from 0:X meaning 
//leftmost is 0 but appears as 8

//FNL is  1000
//DJOLT   0100
//BARCA   0010
//BASE    0001
int scenarios[HIT_STATES] = {0,8,4,2,1,12,10,9,6,5,3,14,13,11,7,15};
uint64_t hit_stats[HIT_STATES];

const int EPOCH_SIZE = 1000;
int epoch = 0;
int sample_count = 0;

vector<float> avg_entries_1;
vector<float> avg_entries_2;
vector<float> avg_entries_3;
#endif

// ----------------------------------------------------------------------------
// Initialize the subprefetchers along with whatever the hybrid prefetcher
// needs, in particular a prefetch buffering system.
// Shadow cache constructor already called above.
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_initialize() 
{
  // Initialize each subprefetcher  
  l1i_prefetcher_initialize1();
  l1i_prefetcher_initialize2();
  l1i_prefetcher_initialize3();

  #ifdef MEASURE
  for(int a = 0; a < HIT_STATES; a++)
    hit_stats[a] = 0;
  #endif

  // For now, prefetch buffer has no debug comments
  pfb.set_debug_mode(false);
}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)
{
  l1i_prefetcher_branch_operate1(ip, branch_type, branch_target);
  l1i_prefetcher_branch_operate2(ip, branch_type, branch_target);
  l1i_prefetcher_branch_operate3(ip, branch_type, branch_target);

  // !!! shadow cache code !!!
  // if this is a return, we'll do some stuff
  if (branch_type == BRANCH_RETURN) {
  //  // turns out generating prefetch candidates from here can also 
  //  // help. (we access the cache here because frickin' ChampSim 
  //  // seems to call this function and the cache operate function 
  //  // out of order, probably because of all those prefetches we're issuing)
    sc.access_cache (ip, NULL, NULL, 0, NULL, NULL, ACCESS_DEMAND);
  }
  // !!! end shadow cache code !!!

}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit)
{
  l1i_prefetcher_cache_operate1(v_addr, cache_hit, prefetch_hit);
  l1i_prefetcher_cache_operate2(v_addr, cache_hit, prefetch_hit);
  l1i_prefetcher_cache_operate3(v_addr, cache_hit, prefetch_hit);
  
  // !!! shadow cache code !!!
  // Elba: This removes late-arriving prefetches from the prefetch queue.
  // This is not the place for us to do this, I think, if at all. 
  //bool hit, pre;
  //sc.access_cache (addr, &hit, &pre, 0, NULL, &edge, ACCESS_PROBE);
  // if (!cache_hit && pre) {

  //  // we have a late prefetch. strength this connection to bump 
  //  //it up in the queue next time.
  //  if (edge) for (int i=0; i<inc_late; i++) countup (edge);
  // }

  // Elba: Should we do this?
  // get rid of this demand fetch from our prefetch queue
  for (auto p=prefetch_queue.begin(); p!=prefetch_queue.end(); p++)
    if ((v_addr&~(BLOCK_SIZE-1)) == (*p).pf_addr)
      p = prefetch_queue.erase (p);

  // make this demand access to the shadow cache
  sc.access_cache (v_addr, NULL, NULL, 0, NULL, NULL, ACCESS_DEMAND);


#ifdef MEASURE

  bool fnl_hit = false;
  bool djolt_hit = false;
  bool barca_hit = false;
  bool base_hit = false;

  //FNL
  fnl_sc.access_cache(v_addr, &fnl_hit, NULL, 0, NULL, NULL, ACCESS_DEMAND);

  //DJOLT
  djolt_sc.access_cache(v_addr, &djolt_hit, NULL, 0, NULL, NULL, ACCESS_DEMAND);

  //BARCA 
  barca_sc.access_cache(v_addr, &barca_hit, NULL, 0, NULL, NULL, ACCESS_DEMAND);

  base_sc.access_cache(v_addr, &base_hit, NULL, 0, NULL, NULL, ACCESS_DEMAND);

  bool hit_vector[NUM_MEASURE] = {fnl_hit, djolt_hit, barca_hit, base_hit};
  int bit_hit = 0;

  for(int a = 0; a < NUM_MEASURE; a++){
    bit_hit |= (hit_vector[a] & 1) << ((NUM_MEASURE - 1) - a);
  }

  hit_stats[bit_hit] += 1;

  
#endif

  // !!! end shadow cache code !!!

}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_cycle_operate()
{
  l1i_prefetcher_cycle_operate1();
  l1i_prefetcher_cycle_operate2();
  l1i_prefetcher_cycle_operate3();

  #ifdef MEASURE
  int curr_entries[3] = {0,0,0};

  if(sample_count == EPOCH_SIZE){
    epoch += 1;
    sample_count = 0;
  }

  for(uint32_t i = 0;i < num_prefetchers; i++)
    curr_entries[i] = my_prefetch_queue[i].size();

  if(avg_entries_1.size() < (epoch + 1))
    avg_entries_1.push_back(curr_entries[0]);
  else if(sample_count == 0 || avg_entries_1[epoch] < curr_entries[0])
    avg_entries_1[epoch] = curr_entries[0];
  //else 
    //avg_entries_1[epoch] = float(curr_entries[0] + (sample_count*avg_entries_1[epoch]))/float(sample_count + 1);
 
  if(avg_entries_2.size() < (epoch + 1))
    avg_entries_2.push_back(curr_entries[1]);
  else if(sample_count == 0 || avg_entries_2[epoch] < curr_entries[1])
    avg_entries_2[epoch] = curr_entries[1];
  //else
  //  avg_entries_2[epoch] = float(curr_entries[1] + (sample_count*avg_entries_2[epoch]))/float(sample_count + 1);

  if(avg_entries_3.size() < (epoch + 1))
    avg_entries_3.push_back(curr_entries[2]);
  else if(sample_count == 0 || (avg_entries_3[epoch] < curr_entries[2]))
    avg_entries_3[epoch] = curr_entries[2];
  //else
  //  avg_entries_3[epoch] = float(curr_entries[2] + (sample_count*avg_entries_3[epoch]))/float(sample_count + 1);
 
  sample_count += 1;

  #endif

  // First, transfer/add contents from each of my_prefetch_queue's lists to
  // the pfb. Note: If the entry doesn't fit in the pfb, it will simply be 
  // dropped.
  for(uint32_t i = 0; i < num_prefetchers; i++) {
    while(my_prefetch_queue[i].size()) {
      
      uint64_t p_vaddr = my_prefetch_queue[i].front();

      #ifdef MEASURE
      switch(i){
        //FNL
        case 0:
          fnl_sc.access_cache(p_vaddr, NULL, NULL, 0, NULL, NULL, ACCESS_PREFETCH);
          break;
        //DJOLT
        case 1:
          djolt_sc.access_cache(p_vaddr, NULL, NULL, 0, NULL, NULL, ACCESS_PREFETCH);
          break;
        //BARCA 
        case 2:
          barca_sc.access_cache(p_vaddr, NULL, NULL, 0, NULL, NULL, ACCESS_PREFETCH);
          break;
      }
      #endif

      pfb.add_pf_entry(0,0, p_vaddr, 0, 0, 1, 1, i);
      my_prefetch_queue[i].pop_front();
    }
  }
  
  // Next, call generate_prefetches on pfb to get the prefetches 
  // we inserted/transferred based on priority, order, etc. 
  // The generate_prefetches() function dictates how many addresses 
  // to prefetch!
  int num_to_fetch = L1I.get_size(3, 0) - L1I.get_occupancy(3, 0);
  deque<PF_BUFFER_ENTRY> cycle_prefetches = pfb.generate_prefetches(num_to_fetch, &sc);

  // Finally, for each of the prefetches generated, call 
  // the ChampSim prefetch_code_line() on the entry's 
  // address value and update the shadow cache
  for(uint32_t j = 0; j < cycle_prefetches.size(); j++) {
    prefetch_code_line(cycle_prefetches.at(j).pf_addr);
   
    // !!! shadow cache code !!!
    // update the shadow cache with this prefetch
    sc.access_cache (cycle_prefetches.at(j).pf_addr, NULL, NULL, 0, NULL, NULL, ACCESS_PREFETCH);
    // !!! end shadow cache code !!!
  }

}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr)
{
  l1i_prefetcher_cache_fill1(v_addr, set, way, prefetch, evicted_v_addr);
  l1i_prefetcher_cache_fill2(v_addr, set, way, prefetch, evicted_v_addr);
  l1i_prefetcher_cache_fill3(v_addr, set, way, prefetch, evicted_v_addr);
  
  // !!! shadow cache code !!!
   if (!prefetch) {
     // if this isn't a prefetch, fill the shadow cache and...
     // Elba: and nothing else
     sc.access_cache (v_addr, NULL, NULL, evicted_v_addr, NULL, NULL, ACCESS_DEMAND);
   } else {
     // if it is a prefetch, just fill the shadow cache
     sc.access_cache (v_addr, NULL, NULL, evicted_v_addr, NULL, NULL, ACCESS_PREFETCH);
   }
  // !!! end shadow cache code !!!
}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_final_stats()
{
  l1i_prefetcher_final_stats1();
  l1i_prefetcher_final_stats2();
  l1i_prefetcher_final_stats3();

#ifdef MEASURE

  printf("Prefetcher 1 Pressure\n");
  for(int a = 0; a < avg_entries_1.size(); a++){
    if(avg_entries_1[a] < 0)
      break; 
    printf("%d %.3f\n", a, avg_entries_1[a]);
  }
  printf("\n");
  printf("Prefetcher 2 Pressure\n");
  for(int a = 0; a < avg_entries_2.size(); a++){
    if(avg_entries_2[a] < 0)
      break; 
    printf("%d %.3f\n", a, avg_entries_2[a]);
  }
  printf("\n");
  printf("Prefetcher 3 Pressure\n");
  for(int a = 0; a < avg_entries_3.size(); a++){
    if(avg_entries_3[a] < 0)
      break; 
    printf("%d %.3f\n", a, avg_entries_3[a]);
  }
  printf("\n");

  printf("Number of hit pre scenario\n");
  for(int a = 0; a < HIT_STATES; a++)
    printf("%ld ", hit_stats[scenarios[a]]);
  printf("\n");

#endif
}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
int O3_CPU::prefetch_code_line1(uint64_t pf_v_addr) {
  
  my_prefetch_queue[0].push_back(pf_v_addr);
  
  return 1;
}

int O3_CPU::prefetch_code_line2(uint64_t pf_v_addr) {
  
  my_prefetch_queue[1].push_back(pf_v_addr);
  
  return 1;
}

int O3_CPU::prefetch_code_line3(uint64_t pf_v_addr) {
  
  my_prefetch_queue[2].push_back(pf_v_addr);
  
  return 1;
}
