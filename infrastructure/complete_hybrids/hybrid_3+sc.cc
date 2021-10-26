#include "ooo_cpu.h"
#include "prefetch_buffer.h"
#include "shadow_cache.h"
#include "ppf.h"
#include <iostream>
#include <list>
#include <map>
#include <cstdlib>

#define HYBRID_BOP

//#define ENV_TUNING

//Turns on multiple shadow caches for the purpose of measuring prefetchers' overlapping behavior
//Must be enabled to allow PPF to use these statistics
#define MEASURE                                                      
//Enables PPF                                                  
// Elba: October 24, disable ppf here 
#define PPF_ENABLED 0
//Decides to prefetch based on the positive outcome of any ppf belonging to a merged request
#define PPF_MERGE 0 
//Allows PPF to prefetch directly to the L1, L2, or reject a prefetch completely
#define PPF_MULTI_LEVEL 1

//Size of the bit vectors containing branch behavior history 
#define B_HIST_LENGTH 32
#define B_TAKEN_LENGTH 32

//Turns on feedback metrics for PFB
//#define PFB_METRICS

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

#include "ISCA_Entangling_1Ke_NoShadows.inc"

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
list<long> my_prefetch_queue_source_ent[num_prefetchers];

PREFETCH_BUFFER pfb(num_prefetchers);

PPF ppf1;
PPF ppf2;
PPF ppf3;

uint64_t branch_history = 0;
uint64_t b_taken_hist = 0;
uint64_t b_type_hist = 0;
uint64_t last_b_target = 0;
uint64_t last_pf = 0;

// Elba: Made the shadow cache a class
SHADOW_CACHE sc;
SHADOW_CACHE base_sc;

#ifdef MEASURE

SHADOW_CACHE first_sc;
SHADOW_CACHE second_sc;
SHADOW_CACHE third_sc;

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

//const int EPOCH_SIZE = 100000;
int epoch = 0;
int sample_count = 0;

vector<float> avg_entries_1;
vector<float> avg_entries_2;
vector<float> avg_entries_3;
#endif

uint64_t num_acc = 0;

uint64_t filtered_1 = 0;
uint64_t filtered_2 = 0;
uint64_t filtered_3 = 0;

// ----------------------------------------------------------------------------
// Initialize the subprefetchers along with whatever the hybrid prefetcher
// needs, in particular a prefetch buffering system.
// Shadow cache constructor already called above.
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_initialize() 
{
  bool pass_taken = true;
  // Initialize each subprefetcher  
  l1i_prefetcher_initialize1();
  l1i_prefetcher_initialize2();
  l1i_prefetcher_initialize3();

//DEFAULT PPF PARAMETERS
  int PPF_1_MAX = 64;
  int PPF_2_MAX = 64;
  int PPF_3_MAX = 64;

  int PPF_1_FEAT_TABLE = 4096; 
  int PPF_2_FEAT_TABLE = 4096;
  int PPF_3_FEAT_TABLE = 4096;

  int PPF_1_TRAINING_T = 320;
  int PPF_2_TRAINING_T = 320;
  int PPF_3_TRAINING_T = 320;
 
  int PPF_1_THRESH = -512;
  int PPF_2_THRESH = -128;
  int PPF_3_THRESH = -256;

  int PPF_1_L2_THRESH = -576;
  int PPF_2_L2_THRESH = -256;
  int PPF_3_L2_THRESH = -576;

  printf("PPF_ENABLED %d\n", PPF_ENABLED);
  printf("PPF_MULTI_LEVEL PREFETCHING %d\n", PPF_MULTI_LEVEL);
#ifdef ENV_TUNING
  
  if(const char* ppf1_val = getenv("PPF1_THRESH"))
    PPF_1_THRESH = atoi(ppf1_val);
  else
    PPF_1_THRESH = -128;

  if(const char* ppf2_val = getenv("PPF2_THRESH"))
    PPF_2_THRESH = atoi(ppf2_val);
  else
    PPF_2_THRESH = -128;

  if(const char* ppf3_val = getenv("PPF3_THRESH"))
    PPF_3_THRESH = atoi(ppf3_val);
  else
    PPF_3_THRESH = -128;

  if(const char* ppf1_val = getenv("PPF1_L2_THRESH"))
    PPF_1_L2_THRESH = atoi(ppf1_val);
  else
    PPF_1_L2_THRESH = -128;

  if(const char* ppf2_val = getenv("PPF2_L2_THRESH"))
    PPF_2_L2_THRESH = atoi(ppf2_val);
  else
    PPF_2_L2_THRESH = -128;

  if(const char* ppf3_val = getenv("PPF3_L2_THRESH"))
    PPF_3_L2_THRESH = atoi(ppf3_val);
  else
    PPF_3_L2_THRESH = -128;

  ppf1.initialize(PPF_1_MAX, PPF_1_FEAT_TABLE, PPF_1_TRAINING_T, PPF_1_THRESH, PPF_1_L2_THRESH);
  ppf2.initialize(PPF_2_MAX, PPF_2_FEAT_TABLE, PPF_2_TRAINING_T, PPF_2_THRESH, PPF_2_L2_THRESH);
  ppf3.initialize(PPF_3_MAX, PPF_3_FEAT_TABLE, PPF_3_TRAINING_T, PPF_3_THRESH, PPF_3_L2_THRESH);

//PPF(uint64_t max_feat, uint64_t feat_table_s, uint64_t training_threshold, int filter_threshold){
#else
  
  ppf1.initialize(PPF_1_MAX, PPF_1_FEAT_TABLE, PPF_1_TRAINING_T, PPF_1_THRESH, PPF_1_L2_THRESH);
  ppf2.initialize(PPF_2_MAX, PPF_2_FEAT_TABLE, PPF_2_TRAINING_T, PPF_2_THRESH, PPF_2_L2_THRESH);
  ppf3.initialize(PPF_3_MAX, PPF_3_FEAT_TABLE, PPF_3_TRAINING_T, PPF_3_THRESH, PPF_3_L2_THRESH);

#endif

  printf("Setting PPF1 Threshold to: %d\n", PPF_1_THRESH);
  printf("Setting PPF2 Threshold to: %d\n", PPF_2_THRESH);
  printf("Setting PPF3 Threshold to: %d\n", PPF_3_THRESH);
  printf("Setting PPF1 L2 Threshold to: %d\n", PPF_1_L2_THRESH);
  printf("Setting PPF2 L2 Threshold to: %d\n", PPF_2_L2_THRESH);
  printf("Setting PPF3 L2 Threshold to: %d\n", PPF_3_L2_THRESH);

  #ifdef MEASURE
  for(int a = 0; a < HIT_STATES; a++)
    hit_stats[a] = 0;
  #endif

  // For now, prefetch buffer has no debug comments
  pfb.set_debug_mode(false);

  ppf1.ppf_id = 0;
  ppf2.ppf_id = 1;
  ppf3.ppf_id = 2;
}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)
{
  l1i_prefetcher_branch_operate1(ip, branch_type, branch_target);
  l1i_prefetcher_branch_operate2(ip, branch_type, branch_target);
  l1i_prefetcher_branch_operate3(ip, branch_type, branch_target);
 
  //PPF Features 
  branch_history <<= 1;
  branch_history |= (branch_target != 0);
  branch_history &= (1 << B_HIST_LENGTH) - 1;
  last_b_target = branch_target;

  //b_taken_hist <<= 1; 
  //b_taken_hist |= taken;
  //b_taken_hist &= (1 << B_TAKEN_LENGTH) - 1;
  
  b_type_hist <<= 3; 
  b_type_hist |= branch_type;
  b_type_hist &= (1 << B_TAKEN_LENGTH) - 1;

  ////////////////
  
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

  ppf1.update_filter(v_addr, cache_hit);
  ppf2.update_filter(v_addr, cache_hit);
  ppf3.update_filter(v_addr, cache_hit);

  ppf1.last_ip = v_addr >> LOG2_BLOCK_SIZE;

  //pfb.get_accuracy(0); 
  //pfb.get_pf_hits(0); 
  //pfb.get_coverage(0); 

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
  //for (auto p=prefetch_queue.begin(); p!=prefetch_queue.end(); p++)
  //  if ((v_addr&~(BLOCK_SIZE-1)) == (*p).pf_addr)
  //    p = prefetch_queue.erase (p);

  bool  sc_hit = false, 
        pf_hit = false;

  // make this demand access to the shadow cache
  sc.access_cache (v_addr, &sc_hit, &pf_hit, 0, NULL, NULL, ACCESS_DEMAND);

  #ifdef PFB_METRICS
  pfb.update_pf_hits(v_addr, sc_hit, pf_hit);

  if(!cache_hit)
    pfb.update_harmful(v_addr, first_sc, second_sc, third_sc, base_sc);
  pfb.update_accuracy(v_addr, pf_hit);
  pfb.inc_epoch(); 
  #endif


#ifdef MEASURE

  bool first_hit = false;
  bool second_hit = false;
  bool third_hit = false;
  bool base_hit = false;

  //First prefetcher
  first_sc.access_cache(v_addr, &first_hit, NULL, 0, NULL, NULL, ACCESS_DEMAND);

  //Second prefetcher
  second_sc.access_cache(v_addr, &second_hit, NULL, 0, NULL, NULL, ACCESS_DEMAND);

  //Third prefetcher
  third_sc.access_cache(v_addr, &third_hit, NULL, 0, NULL, NULL, ACCESS_DEMAND);

  base_sc.access_cache(v_addr, &base_hit, NULL, 0, NULL, NULL, ACCESS_DEMAND);

  bool hit_vector[NUM_MEASURE] = {first_hit, second_hit, third_hit, base_hit};
  int bit_hit = 0;

  for(int a = 0; a < NUM_MEASURE; a++){
    bit_hit |= (hit_vector[a] & 1) << ((NUM_MEASURE - 1) - a);
  }

  hit_stats[bit_hit] += 1;

  
#endif
#ifndef MEASURE
  base_sc.access_cache(v_addr, NULL, NULL, 0, NULL, NULL, ACCESS_DEMAND);
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

  //#ifdef MEASURE
  //int curr_entries[3] = {0,0,0};

  //if(sample_count == EPOCH_SIZE){
  //  epoch += 1;
  //  sample_count = 0;
  //}

  //for(uint32_t i = 0;i < num_prefetchers; i++)
  //  curr_entries[i] = my_prefetch_queue[i].size();

  //if(avg_entries_1.size() < (epoch + 1))
  //  avg_entries_1.push_back(curr_entries[0]);
  //else if(sample_count == 0 || avg_entries_1[epoch] < curr_entries[0])
  //  avg_entries_1[epoch] = curr_entries[0];
  ////else 
  //  //avg_entries_1[epoch] = float(curr_entries[0] + (sample_count*avg_entries_1[epoch]))/float(sample_count + 1);
 
  //if(avg_entries_2.size() < (epoch + 1))
  //  avg_entries_2.push_back(curr_entries[1]);
  //else if(sample_count == 0 || avg_entries_2[epoch] < curr_entries[1])
  //  avg_entries_2[epoch] = curr_entries[1];
  ////else
  ////  avg_entries_2[epoch] = float(curr_entries[1] + (sample_count*avg_entries_2[epoch]))/float(sample_count + 1);

  //if(avg_entries_3.size() < (epoch + 1))
  //  avg_entries_3.push_back(curr_entries[2]);
  //else if(sample_count == 0 || (avg_entries_3[epoch] < curr_entries[2]))
  //  avg_entries_3[epoch] = curr_entries[2];
  ////else
  ////  avg_entries_3[epoch] = float(curr_entries[2] + (sample_count*avg_entries_3[epoch]))/float(sample_count + 1);
 
  //sample_count += 1;

  //#endif

  // First, transfer/add contents from each of my_prefetch_queue's lists to
  // the pfb. Note: If the entry doesn't fit in the pfb, it will simply be 
  // dropped.
  for(uint32_t i = 0; i < num_prefetchers; i++) {
    while(my_prefetch_queue[i].size()) {
      
      uint64_t p_vaddr = my_prefetch_queue[i].front();
      long ent = my_prefetch_queue_source_ent[i].front();

      #ifdef MEASURE
      switch(i){
        //FNL
        case 0:
          first_sc.access_cache(p_vaddr, NULL, NULL, 0, NULL, NULL, ACCESS_PREFETCH);
          break;
        //DJOLT
        case 1:
          second_sc.access_cache(p_vaddr, NULL, NULL, 0, NULL, NULL, ACCESS_PREFETCH);
          break;
        //BARCA 
        case 2:
          third_sc.access_cache(p_vaddr, NULL, NULL, 0, NULL, NULL, ACCESS_PREFETCH);
          break;
      }
      #endif

      pfb.add_pf_entry(0,0, p_vaddr, 0, 0, 1, 1, i, current_core_cycle[cpu], ent);
      my_prefetch_queue[i].pop_front();
      my_prefetch_queue_source_ent[i].pop_front();
    }
  }
  
  // Next, call generate_prefetches on pfb to get the prefetches 
  // we inserted/transferred based on priority, order, etc. 
  // The generate_prefetches() function dictates how many addresses 
  // to prefetch!
  int num_to_fetch = L1I.get_size(3, 0) - L1I.get_occupancy(3, 0);
  // Elba: October 24, delete sc here
  deque<PF_BUFFER_ENTRY> cycle_prefetches = pfb.generate_prefetches(num_to_fetch, NULL);

  bool allow = true;

  // Finally, for each of the prefetches generated, call 
  // the ChampSim prefetch_code_line() on the entry's 
  // address value and update the shadow cache
  for(uint32_t j = 0; j < cycle_prefetches.size(); j++) {
   
    vector<uint64_t> features = {cycle_prefetches.at(j).pf_addr >> LOG2_BLOCK_SIZE, 
        (cycle_prefetches.at(j).pf_addr >> LOG2_BLOCK_SIZE) & 0xffffff,
        (ppf1.last_ip >> LOG2_BLOCK_SIZE) & 0xffffff,
        ppf1.last_ip >> LOG2_BLOCK_SIZE,
        branch_history,
        last_b_target >> LOG2_BLOCK_SIZE,
        //b_taken_hist,
        last_pf,
        b_type_hist
        };

    //printf("Get acc %f\n", //get_cov %f get_harm %f\n",
    //    pfb.get_accuracy(j)); 
    //vector<uint64_t> features = {cycle_prefetches.at(j).pf_addr, 
    //  cycle_prefetches.at(j).base_addr & ((1 << LOG2_BLOCK_SIZE)-1), 
    //  cycle_prefetches.at(j).base_addr >> LOG2_BLOCK_SIZE};

    //printf("feat1 %lx feat2 %lx %lx feat3 %lx\n", cycle_prefetches.at(j).pf_addr >> LOG2_BLOCK_SIZE, 
    //    (cycle_prefetches.at(j).pf_addr >> LOG2_BLOCK_SIZE) & 0xffff,
    //    ((1 << LOG2_BLOCK_SIZE)-1), 
    //    ppf1.last_ip >> LOG2_BLOCK_SIZE);
 
 
    if(PPF_ENABLED){ 
      //Checks if this was requested by multiple prefetchers and then makes a decision based on 
      //the results of 2 or more PFF units
      if((1 << cycle_prefetches.at(j).pref_unit_id) != cycle_prefetches.at(j).pref_overlap_id && PPF_MERGE){
        //printf("%d %d %d\n", 1 << cycle_prefetches.at(j).pref_unit_id, cycle_prefetches.at(j).pref_overlap_id,
        //    cycle_prefetches.at(j).pref_overlap_id & ((1 << 1) >> 1) );
        int allow_vect = 0;
        for(uint32_t a = 0; a < num_prefetchers; a++){
          if(cycle_prefetches.at(j).pref_overlap_id & ((1 << a) >> a) == 1){
            switch(a){
              case 0: 
                allow_vect &= ppf1.check_filter(cycle_prefetches.at(j).pf_addr, features);// << a;
                //allow_vect |= ppf1.check_filter(cycle_prefetches.at(j).pf_addr, features) << a;
                filtered_1 += allow;
                break;
              case 1: 
                allow_vect &= ppf2.check_filter(cycle_prefetches.at(j).pf_addr, features);// << a;
                //allow_vect |= ppf2.check_filter(cycle_prefetches.at(j).pf_addr, features) << a;
                filtered_2 += allow;
                break;
              case 2: 
                allow_vect &= ppf3.check_filter(cycle_prefetches.at(j).pf_addr, features);// << a;
                //allow_vect |= ppf3.check_filter(cycle_prefetches.at(j).pf_addr, features) << a;
                filtered_3 += allow;
                break;
            }
          }
        }
        if(allow_vect > 0)
          allow = true;
        else
          allow = false; 
      
      //Check what level, if any, PPF will allow the prefetch to be sent to 
      }else if(PPF_MULTI_LEVEL){
        PF_LEVEL pf_level = PF_REJECT;
        switch(cycle_prefetches.at(j).pref_unit_id){
          case 0: 
            pf_level = ppf1.check_filter_level(cycle_prefetches.at(j).pf_addr, features);
            filtered_1 += pf_level;
            break;
          case 1: 
            pf_level = ppf2.check_filter_level(cycle_prefetches.at(j).pf_addr, features);
            filtered_2 += pf_level;
            break;
          case 2: 
            pf_level = ppf3.check_filter_level(cycle_prefetches.at(j).pf_addr, features);
            filtered_3 += pf_level;
            break;
        }
        
        last_pf = cycle_prefetches.at(j).pf_addr >> LOG2_BLOCK_SIZE;
        if(pf_level != PF_REJECT)
          prefetch_code_line(cycle_prefetches.at(j).pf_addr, cycle_prefetches.at(j).pref_unit_id, (int)pf_level, cycle_prefetches.at(j).timestamp, cycle_prefetches.at(j).source_ent);

        // !!! shadow cache code !!!
        // update the shadow cache with this prefetch
        if(pf_level == PF_L1)
          sc.access_cache (cycle_prefetches.at(j).pf_addr, NULL, NULL, 0, NULL, NULL, ACCESS_PREFETCH);

      //Base PPF configuration that gives a ACCEPT/REJECT response
      }else{
        switch(cycle_prefetches.at(j).pref_unit_id){
          case 0: 
            allow = ppf1.check_filter(cycle_prefetches.at(j).pf_addr, features);
            filtered_1 += allow;
            break;
          case 1: 
            allow = ppf2.check_filter(cycle_prefetches.at(j).pf_addr, features);
            filtered_2 += allow;
            break;
          case 2: 
            allow = ppf3.check_filter(cycle_prefetches.at(j).pf_addr, features);
            filtered_3 += allow;
            break;
        }
      }
    }

    //Only used if PPF is disabled or its enabled and the multilevel prefetching is not turned on
    if((allow && !PPF_MULTI_LEVEL) || !PPF_ENABLED){
      last_pf = cycle_prefetches.at(j).pf_addr >> LOG2_BLOCK_SIZE;
      prefetch_code_line(cycle_prefetches.at(j).pf_addr, cycle_prefetches.at(j).pref_unit_id, cycle_prefetches.at(j).timestamp, cycle_prefetches.at(j).source_ent);
     
      // !!! shadow cache code !!!
      // update the shadow cache with this prefetch
      sc.access_cache (cycle_prefetches.at(j).pf_addr, NULL, NULL, 0, NULL, NULL, ACCESS_PREFETCH);
    }
    allow = false;
    // !!! end shadow cache code !!!
  }

}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr, PACKET &filling_entry, BLOCK &evicting_entry)
{
  l1i_prefetcher_cache_fill1(v_addr, set, way, prefetch, evicted_v_addr);
  l1i_prefetcher_cache_fill2(v_addr, set, way, prefetch, evicted_v_addr);
  l1i_prefetcher_cache_fill3(v_addr, set, way, prefetch, evicted_v_addr, filling_entry, evicting_entry);
  
  // !!! shadow cache code !!!
  if (!prefetch) {
    // if this isn't a prefetch, fill the shadow cache and...
    // Elba: and nothing else
    sc.access_cache (v_addr, NULL, NULL, evicted_v_addr, NULL, NULL, ACCESS_DEMAND);
  }

   //ppf1.update_filter(evicted_v_addr, 0); 
   //ppf2.update_filter(evicted_v_addr, 0);
   //ppf3.update_filter(evicted_v_addr, 0);

   //else {
   //  // if it is a prefetch, just fill the shadow cache
   //  sc.access_cache (v_addr, NULL, NULL, evicted_v_addr, NULL, NULL, ACCESS_PREFETCH);
   //}
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

  for(int i = 0; i < MAX_NUM_SUBPREFS; i++){
    printf("Avg Cov %d: %f\n", i, pfb.avg_cov[i]);
  }

  for(uint32_t a = 0; a < 8; a++){
    printf("Gen Scenario %d : %d\n", a, pfb.pf_gen_scenario[a]);
  }
  
#ifdef MEASURE
  //Shows the number of prefetches generated per prefetcher per access
  //printf("Prefetcher 1 Pressure\n");
  //for(uint32_t a = 0; a < avg_entries_1.size(); a++){
  //  if(avg_entries_1[a] < 0)
  //    break; 
  //  printf("%d %.3f\n", a, avg_entries_1[a]);
  //}
  //printf("\n");
  //printf("Prefetcher 2 Pressure\n");
  //for(uint32_t a = 0; a < avg_entries_2.size(); a++){
  //  if(avg_entries_2[a] < 0)
  //    break; 
  //  printf("%d %.3f\n", a, avg_entries_2[a]);
  //}
  //printf("\n");
  //printf("Prefetcher 3 Pressure\n");
  //for(uint32_t a = 0; a < avg_entries_3.size(); a++){
  //  if(avg_entries_3[a] < 0)
  //    break; 
  //  printf("%d %.3f\n", a, avg_entries_3[a]);
  //}
  //printf("\n");

  printf("Number of hit pre scenario\n");
  for(uint32_t a = 0; a < HIT_STATES; a++)
    printf("%ld ", hit_stats[scenarios[a]]);
  printf("\n");

#endif

  printf("PPF1 Accept %d PPF1 Reject %d\n", 
    ppf1.accept_table_hit, ppf1.reject_table_hit);
  printf("PPF1 Inc %d PPF1 Dec %d\n", 
    ppf1.increment_weight, ppf1.decrement_weight);
  printf("PPF1 Accept Trig %d Rej Trig %d\n", 
    ppf1.accept_trigger, ppf1.reject_trigger);
  printf("Eviction update %d\n", ppf1.eviction_update);
  for(int a = 0; a < ppf1.NUM_FEAT; a++)
    printf("PPF1 Unique Indexes %d: %ld\n", a, ppf1.unique_indexes[a].size());
  printf("PPF1 Maximum sum seen: %d Minimum sum seen: %d\n",
    ppf1.sum_max, ppf1.sum_min);

  printf("PPF2 Accept %d PPF2 Reject %d\n", 
      ppf2.accept_table_hit, ppf2.reject_table_hit);
  printf("PPF2 Inc %d PPF2 Dec %d\n", 
    ppf2.increment_weight, ppf2.decrement_weight);
  printf("PPF2 Accept Trig %d Rej Trig %d\n", 
    ppf2.accept_trigger, ppf2.reject_trigger);
  printf("Eviction update %d\n", ppf2.eviction_update);
  for(int a = 0; a < ppf2.NUM_FEAT; a++)
    printf("PPF2 Unique Indexes %d: %ld\n", a, ppf2.unique_indexes[a].size());
  printf("PPF2 Maximum sum seen: %d Minimum sum seen: %d\n",
    ppf2.sum_max, ppf2.sum_min);
  
  printf("PPF3 Accept %d PPF3 Reject %d\n", 
    ppf3.accept_table_hit, ppf3.reject_table_hit);
  printf("PPF3 Inc %d PPF3 Dec %d\n", 
    ppf3.increment_weight, ppf3.decrement_weight);
  printf("PPF3 Accept Trig %d Rej Trig %d\n", 
    ppf3.accept_trigger, ppf3.reject_trigger);
  printf("Eviction update %d\n", ppf3.eviction_update);
  for(int a = 0; a < ppf2.NUM_FEAT; a++)
    printf("PPF3 Unique Indexes %d: %ld\n", a, ppf3.unique_indexes[a].size());
  printf("PPF3 Maximum sum seen: %d Minimum sum seen: %d\n",
    ppf3.sum_max, ppf3.sum_min);

  printf("PPF1 Weight Distributions\n");
  for(int a = 0; a < (ppf1.MAX_FEAT * 2)/8; a++)
    printf("%d:%d ", (-1 * ppf1.MAX_FEAT) + (a * 8), (-1 * ppf1.MAX_FEAT) + (a * 8) + 7);
  printf("\n");
  vector<uint64_t> weight_count;
  for(int a = 0; a < ppf1.NUM_FEAT; a++){
    weight_count = ppf1.get_feat_distro(a);
    for(auto w : weight_count)
      printf("%ld ", w);
    printf("\n");
  }

  printf("PPF1 Sum Distribution\n");
  for(int a = 0; a < (ppf1.MAX_FEAT * ppf1.NUM_FEAT * 2)/ppf1.MAX_FEAT; a++){
    printf("%d:%d ", (-1 * ppf1.MAX_FEAT * ppf1.NUM_FEAT) + (a * ppf1.MAX_FEAT), (-1 * ppf1.MAX_FEAT * ppf1.NUM_FEAT) + (a * ppf1.MAX_FEAT) + ppf1.MAX_FEAT - 1);
  }
  printf("\n");
  vector<uint64_t> s_distro = ppf1.get_sum_distro();
  for(int a = 0; a < (ppf1.MAX_FEAT * ppf1.NUM_FEAT * 2)/ppf1.MAX_FEAT; a++){
    printf("%ld ", s_distro[a]);
  }
  printf("\n");
  
  printf("PPF1 Filtered: %ld\n", filtered_1);

  printf("PPF2 Weight Distributions\n");
  for(int a = 0; a < (ppf2.MAX_FEAT * 2)/8; a++)
    printf("%d:%d ", (-1 * ppf1.MAX_FEAT) + (a * 8), (-1 * ppf1.MAX_FEAT) + (a * 8) + 7);
  printf("\n");
  for(int a = 0; a < ppf2.NUM_FEAT; a++){
    weight_count = ppf2.get_feat_distro(a);
    for(auto w : weight_count)
      printf("%ld ", w);
    printf("\n");
  }

  printf("PPF2 Sum Distribution\n");
  for(int a = 0; a < (ppf2.MAX_FEAT * ppf2.NUM_FEAT * 2)/ppf2.MAX_FEAT; a++){
    printf("%d:%d ", (-1 * ppf2.MAX_FEAT * ppf2.NUM_FEAT) + (a * ppf2.MAX_FEAT), (-1 * ppf2.MAX_FEAT * ppf2.NUM_FEAT) + (a * ppf2.MAX_FEAT) + ppf2.MAX_FEAT - 1);
  } 
  printf("\n");
  s_distro = ppf2.get_sum_distro();
  for(int a = 0; a < (ppf2.MAX_FEAT * ppf2.NUM_FEAT * 2)/ppf2.MAX_FEAT; a++){
    printf("%ld ", s_distro[a]);
  }
  printf("\n");
  printf("PPF2 Filtered: %ld\n", filtered_2);

  printf("PPF3 Weight Distributions\n");
  for(int a = 0; a < (ppf3.MAX_FEAT * 2)/8; a++)
    printf("%d:%d ", (-1 * ppf3.MAX_FEAT) + (a * 8), (-1 * ppf3.MAX_FEAT) + (a * 8) + 7);
  printf("\n");
  for(int a = 0; a < ppf3.NUM_FEAT; a++){
    weight_count = ppf3.get_feat_distro(a);
    for(auto w : weight_count)
      printf("%ld ", w);
    printf("\n");
  } 
  
  printf("PPF3 Sum Distribution\n");
  for(int a = 0; a < (ppf3.MAX_FEAT * ppf3.NUM_FEAT * 2)/ppf3.MAX_FEAT; a++){
    printf("%d:%d ", (-1 * ppf3.MAX_FEAT * ppf3.NUM_FEAT) + (a * ppf3.MAX_FEAT), (-1 * ppf3.MAX_FEAT * ppf3.NUM_FEAT) + (a * ppf3.MAX_FEAT) + ppf3.MAX_FEAT - 1);
  } 
  printf("\n");
  s_distro = ppf3.get_sum_distro();
  for(int a = 0; a < (ppf3.MAX_FEAT * ppf3.NUM_FEAT * 2)/ppf3.MAX_FEAT; a++){
    printf("%ld ", s_distro[a]);
  }
  printf("\n");
  printf("PPF3 Filtered: %ld\n", filtered_3);
}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
int O3_CPU::prefetch_code_line1(uint64_t pf_v_addr) {
  
  my_prefetch_queue[0].push_back(pf_v_addr);
  my_prefetch_queue_source_ent[0].push_back(-1);
  return 1;
}

int O3_CPU::prefetch_code_line2(uint64_t pf_v_addr) {
  
  my_prefetch_queue[1].push_back(pf_v_addr);
  my_prefetch_queue_source_ent[1].push_back(-1);
  return 1;
}

int O3_CPU::prefetch_code_line3(uint64_t pf_v_addr, long source_ent) {
  
  my_prefetch_queue[2].push_back(pf_v_addr);
  my_prefetch_queue_source_ent[2].push_back(source_ent);
  return 1;
}
