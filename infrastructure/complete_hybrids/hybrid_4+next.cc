#include "ooo_cpu.h"
#include "prefetch_buffer.h"
#include <list>
#include <map>

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

// Fourth Prefetcher: \#defines help create individually named 
// functions for each prefetcher
#define l1i_prefetcher_branch_operate l1i_prefetcher_branch_operate4
#define l1i_prefetcher_cache_fill l1i_prefetcher_cache_fill4
#define l1i_prefetcher_cache_operate l1i_prefetcher_cache_operate4
#define l1i_prefetcher_cycle_operate l1i_prefetcher_cycle_operate4
#define l1i_prefetcher_final_stats l1i_prefetcher_final_stats4
#define l1i_prefetcher_initialize l1i_prefetcher_initialize4
#define prefetch_code_line prefetch_code_line4

#include AAA

#undef l1i_prefetcher_branch_operate
#undef l1i_prefetcher_cache_fill
#undef l1i_prefetcher_cache_operate
#undef l1i_prefetcher_cycle_operate
#undef l1i_prefetcher_final_stats
#undef l1i_prefetcher_initialize
#undef prefetch_code_line

// Fifth Prefetcher: \#defines help create individually named 
// functions for each prefetcher
#define l1i_prefetcher_branch_operate l1i_prefetcher_branch_operate5
#define l1i_prefetcher_cache_fill l1i_prefetcher_cache_fill5
#define l1i_prefetcher_cache_operate l1i_prefetcher_cache_operate5
#define l1i_prefetcher_cycle_operate l1i_prefetcher_cycle_operate5
#define l1i_prefetcher_final_stats l1i_prefetcher_final_stats5
#define l1i_prefetcher_initialize l1i_prefetcher_initialize5
#define prefetch_code_line prefetch_code_line5

#include "next_line.cc"

#undef l1i_prefetcher_branch_operate
#undef l1i_prefetcher_cache_fill
#undef l1i_prefetcher_cache_operate
#undef l1i_prefetcher_cycle_operate
#undef l1i_prefetcher_final_stats
#undef l1i_prefetcher_initialize
#undef prefetch_code_line

// An array of five lists, one for each prefetcher
const uint32_t num_prefetchers = 5;
PREFETCH_BUFFER pfb(num_prefetchers);
list<uint64_t> my_prefetch_queue[num_prefetchers];


// ----------------------------------------------------------------------------
// Initialize the subprefetchers along with whatever the hybrid prefetcher
// needs, in particular a prefetch buffering system for each subprefetcher.
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_initialize() 
{
	l1i_prefetcher_initialize1();
	l1i_prefetcher_initialize2();
	l1i_prefetcher_initialize3();
	l1i_prefetcher_initialize4();
	l1i_prefetcher_initialize5();

  // For now, no debug comments
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
	l1i_prefetcher_branch_operate4(ip, branch_type, branch_target);
	l1i_prefetcher_branch_operate5(ip, branch_type, branch_target);
}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit)
{
	l1i_prefetcher_cache_operate1(v_addr, cache_hit, prefetch_hit);
	l1i_prefetcher_cache_operate2(v_addr, cache_hit, prefetch_hit);
	l1i_prefetcher_cache_operate3(v_addr, cache_hit, prefetch_hit);
	l1i_prefetcher_cache_operate4(v_addr, cache_hit, prefetch_hit);
	l1i_prefetcher_cache_operate5(v_addr, cache_hit, prefetch_hit);
}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_cycle_operate()
{
	l1i_prefetcher_cycle_operate1();
	l1i_prefetcher_cycle_operate2();
	l1i_prefetcher_cycle_operate3();
	l1i_prefetcher_cycle_operate4();
	l1i_prefetcher_cycle_operate5();

  
  // First, transfer/add contents from each of my_prefetch_queue's lists to
  // the pfb. Note: If the entry doesn't fit in the pfb, it will simply be 
  // dropped.
  for(uint32_t i = 0; i < num_prefetchers; i++) {
    while(my_prefetch_queue[i].size()) {
      uint64_t p_vaddr = my_prefetch_queue[i].front();
      pfb.add_pf_entry(0,0, p_vaddr, 0, 0, 1, 1, i);
      my_prefetch_queue[i].pop_front();
    }
  }
  
  // Next, call generate_prefetches on pfb to get the prefetches 
  // we inserted/transferred based on priority, order, etc. 
  // The generate_prefetches() function dictates how many addresses 
  // to prefetch!
  int num_to_fetch = L1I.get_size(3, 0) - L1I.get_occupancy(3, 0);
  deque<PF_BUFFER_ENTRY> cycle_prefetches = pfb.generate_prefetches(num_to_fetch, NULL);

  // Finally, for each of the prefetches generated, call 
  // prefetch_code_line() on the entry's address value
  for(uint32_t j = 0; j < cycle_prefetches.size(); j++) {
    prefetch_code_line(cycle_prefetches.at(j).pf_addr);
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
	l1i_prefetcher_cache_fill4(v_addr, set, way, prefetch, evicted_v_addr);
	l1i_prefetcher_cache_fill5(v_addr, set, way, prefetch, evicted_v_addr);
}

// ----------------------------------------------------------------------------
// 
// ----------------------------------------------------------------------------
void O3_CPU::l1i_prefetcher_final_stats()
{
	l1i_prefetcher_final_stats1();
	l1i_prefetcher_final_stats2();
	l1i_prefetcher_final_stats3();
	l1i_prefetcher_final_stats4();
	l1i_prefetcher_final_stats5();
}

// ----------------------------------------------------------------------------
// 1. Whenever a subprefetcher attempts to perform a prefetch via 
// prefetch_code_line(), we capture the information here without actually 
// submitting the prefetch.
// ----------------------------------------------------------------------------
int O3_CPU::prefetch_code_line1(uint64_t pf_v_addr) {
	
  my_prefetch_queue[0].push_back(pf_v_addr);
	
  return 1;
}

// ----------------------------------------------------------------------------
// 2. Whenever a subprefetcher attempts to perform a prefetch via 
// prefetch_code_line(), we capture the information here without actually 
// submitting the prefetch.
// ----------------------------------------------------------------------------
int O3_CPU::prefetch_code_line2(uint64_t pf_v_addr) {
	
	my_prefetch_queue[1].push_back(pf_v_addr);
	
  return 1;
}

// ----------------------------------------------------------------------------
// 3. Whenever a subprefetcher attempts to perform a prefetch via 
// prefetch_code_line(), we capture the information here without actually 
// submitting the prefetch.
// ----------------------------------------------------------------------------
int O3_CPU::prefetch_code_line3(uint64_t pf_v_addr) {
	
	my_prefetch_queue[2].push_back(pf_v_addr);
	
  return 1;
}

// ----------------------------------------------------------------------------
// 4. Whenever a subprefetcher attempts to perform a prefetch via 
// prefetch_code_line(), we capture the information here without actually 
// submitting the prefetch.
// ----------------------------------------------------------------------------
int O3_CPU::prefetch_code_line4(uint64_t pf_v_addr) {
	
	my_prefetch_queue[3].push_back(pf_v_addr);
	
  return 1;
}

// ----------------------------------------------------------------------------
// 5. Whenever a subprefetcher attempts to perform a prefetch via 
// prefetch_code_line(), we capture the information here without actually 
// submitting the prefetch.
// ----------------------------------------------------------------------------
int O3_CPU::prefetch_code_line5(uint64_t pf_v_addr) {
	
	my_prefetch_queue[4].push_back(pf_v_addr);
	
  return 1;
}
