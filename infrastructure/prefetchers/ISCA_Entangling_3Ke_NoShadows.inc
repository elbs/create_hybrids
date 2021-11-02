////////////////////////////////////////////////////////////////////////
//
//  Implementation for the Entangling Instruction Prefetcher 
//  presented at ISCA'21.
//
//  Authors: Alberto Ros (aros@ditec.um.es)
//           Alexandra Jimborean (alexandra.jimborean@um.es)
//
//  Cite: Alberto Ros and Alexandra Jimborean, "A Cost-Effective 
//        Entangling Prefetcher for Instructions," ISCA, june, 2021.
//
////////////////////////////////////////////////////////////////////////

#include "ooo_cpu.h"

extern uint64_t current_core_cycle[NUM_CPUS];
extern uint8_t  all_warmup_complete;
extern std::vector<O3_CPU> ooo_cpu;

// To access cpu in my functions
uint32_t l1i_cpu_id;

uint64_t l1i_last_basic_block;
uint32_t l1i_consecutive_count;
uint32_t l1i_basic_block_merge_diff;

bool debug = 0;
bool all_warmed_up = false;

#define L1I_HIST_TABLE_ENTRIES 16

// LINE AND MERGE BASIC BLOCK SIZE

#define L1I_MERGE_BBSIZE_BITS 6
#define L1I_MERGE_BBSIZE_MAX_VALUE ((1 << L1I_MERGE_BBSIZE_BITS) - 1)

// TIME AND OVERFLOWS

#define L1I_TIME_DIFF_BITS 20
#define L1I_TIME_DIFF_OVERFLOW ((uint64_t)1 << L1I_TIME_DIFF_BITS)
#define L1I_TIME_DIFF_MASK (L1I_TIME_DIFF_OVERFLOW - 1)

#define L1I_TIME_BITS 12
#define L1I_TIME_OVERFLOW ((uint64_t)1 << L1I_TIME_BITS)
#define L1I_TIME_MASK (L1I_TIME_OVERFLOW - 1)

uint64_t l1i_get_latency(uint64_t cycle, uint64_t cycle_prev) {
  uint64_t cycle_masked = cycle & L1I_TIME_MASK;
  uint64_t cycle_prev_masked = cycle_prev & L1I_TIME_MASK;
  if (cycle_prev_masked > cycle_masked) {
    return (cycle_masked + L1I_TIME_OVERFLOW) - cycle_prev_masked;
  }
  return cycle_masked - cycle_prev_masked;
}

// ENTANGLED COMPRESSION FORMAT

#define L1I_ENTANGLED_MAX_FORMATS 7

// STATS
#define L1I_STATS_TABLE_INDEX_BITS 16
#define L1I_STATS_TABLE_ENTRIES (1 << L1I_STATS_TABLE_INDEX_BITS)
#define L1I_STATS_TABLE_MASK (L1I_STATS_TABLE_ENTRIES - 1)

typedef struct __l1i_stats_entry {
  uint64_t accesses;
  uint64_t misses;
  uint64_t hits;
  uint64_t late;
  uint64_t wrong; // early
} l1i_stats_entry;

l1i_stats_entry l1i_stats_table[NUM_CPUS][L1I_STATS_TABLE_ENTRIES];
uint64_t l1i_stats_discarded_prefetches;
uint64_t l1i_stats_evict_entangled_j_table;
uint64_t l1i_stats_evict_entangled_k_table;
uint64_t l1i_stats_max_bb_size;
uint64_t l1i_stats_formats[L1I_ENTANGLED_MAX_FORMATS];
uint64_t l1i_stats_hist_lookups[L1I_HIST_TABLE_ENTRIES+2];
uint64_t l1i_stats_basic_blocks[L1I_MERGE_BBSIZE_MAX_VALUE+1];
uint64_t l1i_stats_entangled[L1I_ENTANGLED_MAX_FORMATS+1];
uint64_t l1i_stats_basic_blocks_ent[L1I_MERGE_BBSIZE_MAX_VALUE+1];

void l1i_init_stats_table() {
  for (int i = 0; i < L1I_STATS_TABLE_ENTRIES; i++) {
    l1i_stats_table[l1i_cpu_id][i].accesses = 0;
    l1i_stats_table[l1i_cpu_id][i].misses = 0;
    l1i_stats_table[l1i_cpu_id][i].hits = 0;
    l1i_stats_table[l1i_cpu_id][i].late = 0;
    l1i_stats_table[l1i_cpu_id][i].wrong = 0;
  }
  l1i_stats_discarded_prefetches = 0;
  l1i_stats_evict_entangled_j_table = 0;
  l1i_stats_evict_entangled_k_table = 0;
  l1i_stats_max_bb_size = 0;
  for (int i = 0; i < L1I_ENTANGLED_MAX_FORMATS; i++) {
    l1i_stats_formats[i] = 0;
  }
  for (int i = 0; i <= L1I_HIST_TABLE_ENTRIES; i++) {
    l1i_stats_hist_lookups[i] = 0;
  }
  for (int i = 0; i <= L1I_MERGE_BBSIZE_MAX_VALUE; i++) {
    l1i_stats_basic_blocks[i] = 0;
  }
  for (int i = 0; i <= L1I_ENTANGLED_MAX_FORMATS; i++) {
    l1i_stats_entangled[i] = 0;
  }
  for (int i = 0; i <= L1I_MERGE_BBSIZE_MAX_VALUE; i++) {
    l1i_stats_basic_blocks_ent[i] = 0;
  }
}

void l1i_print_stats_table() {
  cout << "IP accesses: ";
  uint64_t max = 0;
  uint64_t max_addr = 0;
  uint64_t total_accesses = 0;
  for (uint32_t i = 0; i < L1I_STATS_TABLE_ENTRIES; i++) {
    if (l1i_stats_table[l1i_cpu_id][i].accesses > max) {
      max = l1i_stats_table[l1i_cpu_id][i].accesses;
      max_addr = i;
    }
    total_accesses += l1i_stats_table[l1i_cpu_id][i].accesses;
  }
  cout << hex << max_addr << " " << (max_addr << LOG2_BLOCK_SIZE) << dec << " " << max << " / " << total_accesses << endl;
  cout << "IP misses: ";
  max = 0;
  max_addr = 0;
  uint64_t total_misses = 0;
  for (uint32_t i = 0; i < L1I_STATS_TABLE_ENTRIES; i++) {
    if (l1i_stats_table[l1i_cpu_id][i].misses > max) {
      max = l1i_stats_table[l1i_cpu_id][i].misses;
      max_addr = i;
    }
    total_misses += l1i_stats_table[l1i_cpu_id][i].misses;
  }
  cout << hex << max_addr << " " << (max_addr << LOG2_BLOCK_SIZE) << dec << " " << max << " / " << total_misses << endl;
  cout << "IP hits: ";
  max = 0;
  max_addr = 0;
  uint64_t total_hits = 0;
  for (uint32_t i = 0; i < L1I_STATS_TABLE_ENTRIES; i++) {
    if (l1i_stats_table[l1i_cpu_id][i].hits > max) {
      max = l1i_stats_table[l1i_cpu_id][i].hits;
      max_addr = i;
    }
    total_hits += l1i_stats_table[l1i_cpu_id][i].hits;
  }
  cout << hex << max_addr << " " << (max_addr << LOG2_BLOCK_SIZE) << dec << " " << max << " / " << total_hits << endl;
  cout << "IP late: ";
  max = 0;
  max_addr = 0;
  uint64_t total_late = 0;
  for (uint32_t i = 0; i < L1I_STATS_TABLE_ENTRIES; i++) {
    if (l1i_stats_table[l1i_cpu_id][i].late > max) {
      max = l1i_stats_table[l1i_cpu_id][i].late;
      max_addr = i;
    }
    total_late += l1i_stats_table[l1i_cpu_id][i].late;
  }
  cout << hex << max_addr << " " << (max_addr << LOG2_BLOCK_SIZE) << dec << " " << max << " / " << total_late << endl;
  cout << "IP wrong: ";
  max = 0;
  max_addr = 0;
  uint64_t total_wrong = 0;
  for (uint32_t i = 0; i < L1I_STATS_TABLE_ENTRIES; i++) {
    if (l1i_stats_table[l1i_cpu_id][i].wrong > max) {
      max = l1i_stats_table[l1i_cpu_id][i].wrong;
      max_addr = i;
    }
    total_wrong += l1i_stats_table[l1i_cpu_id][i].wrong;
  }
  cout << hex << max_addr << " " << (max_addr << LOG2_BLOCK_SIZE) << dec << " " << max << " / " << total_wrong << endl;

  cout << "miss rate: " << ((double)total_misses / (double)total_accesses) << endl;
  cout << "coverage: " << ((double)total_hits / (double)(total_hits + total_misses)) << endl;
  cout << "coverage_late: " << ((double)(total_hits + total_late) / (double)(total_hits + total_misses)) << endl;
  cout << "accuracy: " << ((double)total_hits / (double)(total_hits + total_late + total_wrong)) << endl;
  cout << "accuracy_late: " << ((double)(total_hits + total_late) / (double)(total_hits + total_late + total_wrong)) << endl;
  cout << "discarded: " << l1i_stats_discarded_prefetches << endl;
  cout << "evicts entangled j table: " << l1i_stats_evict_entangled_j_table << endl;
  cout << "evicts entangled k table: " << l1i_stats_evict_entangled_k_table << endl;
  cout << "max bb size: " << l1i_stats_max_bb_size << endl;
  cout << "formats: ";
  for (uint32_t i = 0; i < L1I_ENTANGLED_MAX_FORMATS; i++) {
    cout << l1i_stats_formats[i] << " ";
  }
  cout << endl;
  cout << "hist_lookups: ";
  uint64_t total_hist_lookups = 0;
  for (uint32_t i = 0; i <= L1I_HIST_TABLE_ENTRIES+1; i++) {
    cout << l1i_stats_hist_lookups[i] << " ";
    total_hist_lookups += l1i_stats_hist_lookups[i];
  }
  cout << endl;
  cout << "hist_lookups_evict: " << (double)l1i_stats_hist_lookups[L1I_HIST_TABLE_ENTRIES] * 100 / (double)(total_hist_lookups) << " %" << endl;
  cout << "hist_lookups_shortlat: " << (double)l1i_stats_hist_lookups[L1I_HIST_TABLE_ENTRIES+1] * 100 / (double)(total_hist_lookups) << " %" << endl;

  cout << "bb_found_hist: ";
  uint64_t total_bb_found = 0;
  uint64_t total_bb_prefetches = 0;
  for (uint32_t i = 0; i <= L1I_MERGE_BBSIZE_MAX_VALUE; i++) {
    cout << l1i_stats_basic_blocks[i] << " ";
    total_bb_found += i * l1i_stats_basic_blocks[i];
    total_bb_prefetches += l1i_stats_basic_blocks[i];
  }
  cout << endl;
  cout << "bb_found_summary: " << total_bb_found << " " << total_bb_prefetches << " " << (double)total_bb_found / (double)total_bb_prefetches << endl;

  cout << "entangled_found_hist: ";
  uint64_t total_entangled_found = 0;
  uint64_t total_ent_prefetches = 0;
  for (uint32_t i = 0; i <= L1I_ENTANGLED_MAX_FORMATS; i++) {
    cout << l1i_stats_entangled[i] << " ";
    total_entangled_found += i * l1i_stats_entangled[i];
    total_ent_prefetches += l1i_stats_entangled[i];
  }
  cout << endl;
  cout << "entangled_found_summary: " << total_entangled_found << " " << total_ent_prefetches << " " << (double)total_entangled_found / (double)total_ent_prefetches << endl;

  cout << "bb_ent_found_hist: ";
  uint64_t total_bb_ent_found = 0;
  uint64_t total_bb_ent_prefetches = 0;
  for (uint32_t i = 0; i <= L1I_MERGE_BBSIZE_MAX_VALUE; i++) {
    cout << l1i_stats_basic_blocks_ent[i] << " ";
    total_bb_ent_found += i * l1i_stats_basic_blocks_ent[i];
    total_bb_ent_prefetches += l1i_stats_basic_blocks_ent[i];
  }
  cout << endl;
  cout << "bb_ent_found_summary: " << total_bb_ent_found << " " << total_bb_ent_prefetches << " " << (double)total_bb_ent_found / (double)total_bb_ent_prefetches << endl;
}

// HISTORY TABLE (BUFFER)

#define L1I_HIST_TABLE_MASK (L1I_HIST_TABLE_ENTRIES - 1)
#define L1I_BB_MERGE_ENTRIES 6
#define L1I_HIST_TAG_BITS 58
#define L1I_HIST_TAG_MASK (((uint64_t)1 << L1I_HIST_TAG_BITS) - 1)

typedef struct __l1i_hist_entry {
  uint64_t tag; // L1I_HIST_TAG_BITS bits
  uint64_t time_diff; // L1I_TIME_DIFF_BITS bits
  uint32_t bb_size; // L1I_MERGE_BBSIZE_BITS bits
} l1i_hist_entry;

l1i_hist_entry l1i_hist_table[NUM_CPUS][L1I_HIST_TABLE_ENTRIES];
uint64_t l1i_hist_table_head[NUM_CPUS]; // log_2 (L1I_HIST_TABLE_ENTRIES)
uint64_t l1i_hist_table_head_time[NUM_CPUS]; // 64 bits

void l1i_init_hist_table() {
  l1i_hist_table_head[l1i_cpu_id] = 0;
  l1i_hist_table_head_time[l1i_cpu_id] = current_core_cycle[l1i_cpu_id];
  for (uint32_t i = 0; i < L1I_HIST_TABLE_ENTRIES; i++) {
    l1i_hist_table[l1i_cpu_id][i].tag = 0;
    l1i_hist_table[l1i_cpu_id][i].time_diff = 0;
    l1i_hist_table[l1i_cpu_id][i].bb_size = 0;
  }
}

uint64_t l1i_find_hist_entry(uint64_t line_addr) {
  uint64_t tag = line_addr & L1I_HIST_TAG_MASK; 
  for (uint32_t count = 0, i = (l1i_hist_table_head[l1i_cpu_id] + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES; count < L1I_HIST_TABLE_ENTRIES; count++, i = (i + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES) {
    if (l1i_hist_table[l1i_cpu_id][i].tag == tag) return i;
  }
  return L1I_HIST_TABLE_ENTRIES;
}

// It can have duplicated entries if the line was evicted in between
void l1i_add_hist_table(uint64_t line_addr) {
  // Insert empty addresses in hist not to have timediff overflows
  while(current_core_cycle[l1i_cpu_id] - l1i_hist_table_head_time[l1i_cpu_id] >= L1I_TIME_DIFF_OVERFLOW) {
    l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].tag = 0;
    l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].time_diff = L1I_TIME_DIFF_MASK;
    l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].bb_size = 0;
    l1i_hist_table_head[l1i_cpu_id] = (l1i_hist_table_head[l1i_cpu_id] + 1) % L1I_HIST_TABLE_ENTRIES;
    l1i_hist_table_head_time[l1i_cpu_id] += L1I_TIME_DIFF_MASK;
  }
  
  // Allocate a new entry (evict old one if necessary)
  l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].tag = line_addr & L1I_HIST_TAG_MASK;
  l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].time_diff = (current_core_cycle[l1i_cpu_id] - l1i_hist_table_head_time[l1i_cpu_id]) & L1I_TIME_DIFF_MASK;
  l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].bb_size = 0;
  l1i_hist_table_head[l1i_cpu_id] = (l1i_hist_table_head[l1i_cpu_id] + 1) % L1I_HIST_TABLE_ENTRIES;
  l1i_hist_table_head_time[l1i_cpu_id] = current_core_cycle[l1i_cpu_id];
}

void l1i_add_bb_size_hist_table(uint64_t line_addr, uint32_t bb_size) {
  uint64_t index = l1i_find_hist_entry(line_addr);
  l1i_hist_table[l1i_cpu_id][index].bb_size = bb_size & L1I_MERGE_BBSIZE_MAX_VALUE;
}

uint32_t l1i_find_bb_merge_hist_table(uint64_t line_addr) {
  uint64_t tag = line_addr & L1I_HIST_TAG_MASK; 
  for (uint32_t count = 0, i = (l1i_hist_table_head[l1i_cpu_id] + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES; count < L1I_HIST_TABLE_ENTRIES; count++, i = (i + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES) {
    if (count >= L1I_BB_MERGE_ENTRIES) {
      return 0;
    }
    if (tag > l1i_hist_table[l1i_cpu_id][i].tag
	&& (tag - l1i_hist_table[l1i_cpu_id][i].tag) <= l1i_hist_table[l1i_cpu_id][i].bb_size) {
      //&& (tag - l1i_hist_table[l1i_cpu_id][i].tag) == l1i_hist_table[l1i_cpu_id][i].bb_size) {
      return tag - l1i_hist_table[l1i_cpu_id][i].tag;
    }
  }
  assert(false);
}

// return src-entangled pair
uint64_t l1i_get_src_entangled_hist_table(uint64_t line_addr, uint64_t latency, uint32_t skip = 0) {
  uint64_t tag = line_addr & L1I_HIST_TAG_MASK; 
  assert(tag);
  uint32_t first = (l1i_hist_table_head[l1i_cpu_id] + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES;
  uint64_t time_i = l1i_hist_table_head_time[l1i_cpu_id];
  uint64_t req_time = 0;
  uint32_t num_skipped = 0;
  for (uint32_t count = 0, i = first; count < L1I_HIST_TABLE_ENTRIES; count++, i = (i + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES) {
    // Against the time overflow
    if (req_time == 0
	&& l1i_hist_table[l1i_cpu_id][i].tag == tag
	&& time_i + latency >= current_core_cycle[l1i_cpu_id]) { // Its me (miss or late prefetcher)
      req_time = time_i;
    } else if (req_time) { // Not me (check only older than me)
      if (l1i_hist_table[l1i_cpu_id][i].tag == tag) {
	return 0; // Second time it appeared (it was evicted in between) or many for the same set. No entangle
      }
      if (time_i + latency <= req_time && l1i_hist_table[l1i_cpu_id][i].tag) {
	if (skip == num_skipped) {
	  return l1i_hist_table[l1i_cpu_id][i].tag;
	} else {
	  num_skipped++;
	}
      }
    }
    time_i -= l1i_hist_table[l1i_cpu_id][i].time_diff;  
  }
  return 0;
}

// ENTANGLED TABLE

uint32_t L1I_ENTANGLED_FORMATS[L1I_ENTANGLED_MAX_FORMATS] = {58, 28, 18, 13, 10, 8, 6};
#define L1I_ENTANGLED_NUM_FORMATS 6

uint32_t l1i_get_format_entangled(uint64_t line_addr, uint64_t entangled_addr) {
  for (uint32_t i = L1I_ENTANGLED_NUM_FORMATS; i != 0; i--) {
    if ((line_addr >> L1I_ENTANGLED_FORMATS[i-1]) == (entangled_addr >> L1I_ENTANGLED_FORMATS[i-1])) {
      return i;
    }
  }
  assert(false);
}

uint64_t l1i_extend_format_entangled(uint64_t line_addr, uint64_t entangled_addr, uint32_t format) {
  return ((line_addr >> L1I_ENTANGLED_FORMATS[format-1]) << L1I_ENTANGLED_FORMATS[format-1])
    | (entangled_addr & (((uint64_t)1 << L1I_ENTANGLED_FORMATS[format-1]) - 1));
}

uint64_t l1i_compress_format_entangled(uint64_t entangled_addr, uint32_t format) {
  return entangled_addr & (((uint64_t)1 << L1I_ENTANGLED_FORMATS[format-1]) - 1);
}

#define L1I_ENTANGLED_TABLE_INDEX_BITS 8
#define L1I_ENTANGLED_TABLE_SETS (1 << L1I_ENTANGLED_TABLE_INDEX_BITS)
#define L1I_ENTANGLED_TABLE_WAYS 12
#define L1I_MAX_ENTANGLED_PER_LINE L1I_ENTANGLED_NUM_FORMATS
#define L1I_TAG_BITS (18 - L1I_ENTANGLED_TABLE_INDEX_BITS)
#define L1I_TAG_MASK (((uint64_t)1 << L1I_TAG_BITS) - 1)
#define L1I_CONFIDENCE_COUNTER_BITS 2
#define L1I_CONFIDENCE_COUNTER_MAX_VALUE ((1 << L1I_CONFIDENCE_COUNTER_BITS) - 1)
#define L1I_CONFIDENCE_COUNTER_THRESHOLD 1
#define L1I_TRIES_AVAIL_ENTANGLED 2

typedef struct __l1i_entangled_entry {
  uint64_t tag; // L1I_TAG_BITS bits
  uint32_t format; // log2(L1I_ENTANGLED_NUM_FORMATS) bits
  uint64_t entangled_addr[L1I_MAX_ENTANGLED_PER_LINE]; // JUST DIFF
  uint32_t entangled_conf[L1I_MAX_ENTANGLED_PER_LINE]; // L1I_CONFIDENCE_COUNTER_BITS bits
  uint32_t bb_size; // L1I_MERGE_BBSIZE_BITS bits
} l1i_entangled_entry;

l1i_entangled_entry l1i_entangled_table[NUM_CPUS][L1I_ENTANGLED_TABLE_SETS][L1I_ENTANGLED_TABLE_WAYS];
uint32_t l1i_entangled_fifo[NUM_CPUS][L1I_ENTANGLED_TABLE_SETS]; // log2(L1I_ENTANGLED_TABLE_WAYS) * L1I_ENTANGLED_TABLE_SETS bits

uint64_t l1i_hash(uint64_t line_addr) {
  return line_addr ^ (line_addr >> 2) ^ (line_addr >> 5);
}

void l1i_init_entangled_table() {
  for (uint32_t i = 0; i < L1I_ENTANGLED_TABLE_SETS; i++) {
    for (uint32_t j = 0; j < L1I_ENTANGLED_TABLE_WAYS; j++) {
      l1i_entangled_table[l1i_cpu_id][i][j].tag = 0;
      l1i_entangled_table[l1i_cpu_id][i][j].format = 1;
      for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
	l1i_entangled_table[l1i_cpu_id][i][j].entangled_addr[k] = 0;
	l1i_entangled_table[l1i_cpu_id][i][j].entangled_conf[k] = 0;
      }
      l1i_entangled_table[l1i_cpu_id][i][j].bb_size = 0;
    }
    l1i_entangled_fifo[l1i_cpu_id][i] = 0;
  }
}

uint32_t l1i_get_way_entangled_table(uint64_t line_addr) {
  uint64_t tag = (l1i_hash(line_addr) >> L1I_ENTANGLED_TABLE_INDEX_BITS) & L1I_TAG_MASK; 
  uint32_t set = l1i_hash(line_addr) % L1I_ENTANGLED_TABLE_SETS;
  for (uint32_t i = 0; i < L1I_ENTANGLED_TABLE_WAYS; i++) {
    if (l1i_entangled_table[l1i_cpu_id][set][i].tag == tag) { // Found
      return i;
    }
  }
  return L1I_ENTANGLED_TABLE_WAYS;
}

void l1i_try_realocate_evicted_in_available_entangled_table(uint32_t set) {
  uint64_t way = l1i_entangled_fifo[l1i_cpu_id][set];
  bool dest_free_way = true;
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
      dest_free_way = false;
      break;
    }
  }
  if (dest_free_way && l1i_entangled_table[l1i_cpu_id][set][way].bb_size == 0) return;
  uint32_t free_way = way;
  bool free_with_size = false;
  for (uint32_t i = (way + 1) % L1I_ENTANGLED_TABLE_WAYS; i != way; i = (i + 1) % L1I_ENTANGLED_TABLE_WAYS) {
    bool dest_free = true;
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      if (l1i_entangled_table[l1i_cpu_id][set][i].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
	dest_free = false;
	break;
      }
    }
    if (dest_free) {
      if (free_way == way) {
	free_way = i;
	free_with_size = (l1i_entangled_table[l1i_cpu_id][set][i].bb_size != 0);
      } else if (free_with_size && l1i_entangled_table[l1i_cpu_id][set][i].bb_size == 0) {
	free_way = i;
	free_with_size = false;
	break;
      }
    }
  }
  if (free_way != way && ((!free_with_size) || (free_with_size && !dest_free_way))) { // Only evict if it has more information 
    l1i_entangled_table[l1i_cpu_id][set][free_way].tag = l1i_entangled_table[l1i_cpu_id][set][way].tag;
    l1i_entangled_table[l1i_cpu_id][set][free_way].format = l1i_entangled_table[l1i_cpu_id][set][way].format;
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      l1i_entangled_table[l1i_cpu_id][set][free_way].entangled_addr[k] = l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k];
      l1i_entangled_table[l1i_cpu_id][set][free_way].entangled_conf[k] = l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k];
    }
    l1i_entangled_table[l1i_cpu_id][set][free_way].bb_size = l1i_entangled_table[l1i_cpu_id][set][way].bb_size;
  }
}

void l1i_add_entangled_table(uint64_t line_addr, uint64_t entangled_addr) {
  uint64_t tag = (l1i_hash(line_addr) >> L1I_ENTANGLED_TABLE_INDEX_BITS) & L1I_TAG_MASK; 
  uint32_t set = l1i_hash(line_addr) % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way == L1I_ENTANGLED_TABLE_WAYS) {
    l1i_try_realocate_evicted_in_available_entangled_table(set);
    way = l1i_entangled_fifo[l1i_cpu_id][set];
    l1i_entangled_table[l1i_cpu_id][set][way].tag = tag;
    l1i_entangled_table[l1i_cpu_id][set][way].format = 1;
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k] = 0;
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] = 0;
    }
    l1i_entangled_table[l1i_cpu_id][set][way].bb_size = 0;
    l1i_entangled_fifo[l1i_cpu_id][set] = (l1i_entangled_fifo[l1i_cpu_id][set] + 1) % L1I_ENTANGLED_TABLE_WAYS;
  }
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD
	&& l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k], l1i_entangled_table[l1i_cpu_id][set][way].format) == entangled_addr) {
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] = L1I_CONFIDENCE_COUNTER_MAX_VALUE;
      return;
    }
  }

  // Adding a new entangled
  uint32_t format_new = l1i_get_format_entangled(line_addr, entangled_addr);
  l1i_stats_formats[format_new-1]++;
  
  // Check for evictions
  while(true) {
    uint32_t min_format = format_new;
    uint32_t num_valid = 1;
    uint32_t min_value = L1I_CONFIDENCE_COUNTER_MAX_VALUE + 1;
    uint32_t min_pos = 0;
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
	num_valid++;
	uint32_t format_k = l1i_get_format_entangled(line_addr, l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k], l1i_entangled_table[l1i_cpu_id][set][way].format));
	if (format_k < min_format) {
	  min_format = format_k;
	}
	if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] < min_value) {
	  min_value = l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k];
	  min_pos = k;
	}
      }
    }
    if (num_valid > min_format) { // Eviction is necessary. We chose the lower confidence one 
      l1i_stats_evict_entangled_k_table++;
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[min_pos] = 0;
    } else {
      // Reformat
      for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
	if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
	  l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k] = l1i_compress_format_entangled(l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k], l1i_entangled_table[l1i_cpu_id][set][way].format), min_format);
	}
      }
      l1i_entangled_table[l1i_cpu_id][set][way].format = min_format;
      break;
    }
  }
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] < L1I_CONFIDENCE_COUNTER_THRESHOLD) {
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k] = l1i_compress_format_entangled(entangled_addr, l1i_entangled_table[l1i_cpu_id][set][way].format);
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] = L1I_CONFIDENCE_COUNTER_MAX_VALUE;
      return;
    }
  }
}

bool l1i_avail_entangled_table(uint64_t line_addr, uint64_t entangled_addr, bool insert_not_present) {
  uint32_t set = l1i_hash(line_addr) % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way == L1I_ENTANGLED_TABLE_WAYS) return insert_not_present;
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD
	&& l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k], l1i_entangled_table[l1i_cpu_id][set][way].format) == entangled_addr) {
      return true;
    }
  }
  // Check for availability
  uint32_t min_format = l1i_get_format_entangled(line_addr, entangled_addr);
  uint32_t num_valid = 1;
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
      num_valid++;
      uint32_t format_k = l1i_get_format_entangled(line_addr, l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k], l1i_entangled_table[l1i_cpu_id][set][way].format));
      if (format_k < min_format) {
	min_format = format_k;
      }
    }
  }
  if (num_valid > min_format) { // Eviction is necessary
    return false;
  } else {
    return true;
  }
}

void l1i_add_bbsize_table(uint64_t line_addr, uint32_t bb_size) {
  uint64_t tag = (l1i_hash(line_addr) >> L1I_ENTANGLED_TABLE_INDEX_BITS) & L1I_TAG_MASK; 
  uint32_t set = l1i_hash(line_addr) % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way == L1I_ENTANGLED_TABLE_WAYS) {
    l1i_try_realocate_evicted_in_available_entangled_table(set);
    way = l1i_entangled_fifo[l1i_cpu_id][set];
    l1i_entangled_table[l1i_cpu_id][set][way].tag = tag;
    l1i_entangled_table[l1i_cpu_id][set][way].format = 1;
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k] = 0;
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] = 0;
    }
    l1i_entangled_table[l1i_cpu_id][set][way].bb_size = 0;
    l1i_entangled_fifo[l1i_cpu_id][set] = (l1i_entangled_fifo[l1i_cpu_id][set] + 1) % L1I_ENTANGLED_TABLE_WAYS;
  }
  if (bb_size > l1i_entangled_table[l1i_cpu_id][set][way].bb_size) {
    l1i_entangled_table[l1i_cpu_id][set][way].bb_size = bb_size & L1I_MERGE_BBSIZE_MAX_VALUE;
  }
  if (bb_size > l1i_stats_max_bb_size) {
    l1i_stats_max_bb_size = bb_size;
  }
}

uint64_t l1i_get_entangled_addr_entangled_table(uint64_t line_addr, uint32_t index_k, long &ent) {
  uint32_t set = l1i_hash(line_addr) % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way < L1I_ENTANGLED_TABLE_WAYS) {
    ent = set * L1I_ENTANGLED_TABLE_WAYS + way;
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[index_k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
      return l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[index_k], l1i_entangled_table[l1i_cpu_id][set][way].format);
    }
  }
  return 0;
}

uint32_t l1i_get_bbsize_entangled_table(uint64_t line_addr) {
  uint32_t set = l1i_hash(line_addr) % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way < L1I_ENTANGLED_TABLE_WAYS) {
    return l1i_entangled_table[l1i_cpu_id][set][way].bb_size;
  }
  return 0;
}

void l1i_update_confidence_entangled_table(long ent, uint64_t entangled_addr, bool accessed) {
  assert(ent >= 0);
  uint32_t set = ent / L1I_ENTANGLED_TABLE_WAYS;
  uint32_t way = ent % L1I_ENTANGLED_TABLE_WAYS;
  if (way < L1I_ENTANGLED_TABLE_WAYS) {
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD
	  && l1i_compress_format_entangled(l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k], l1i_entangled_table[l1i_cpu_id][set][way].format) == l1i_compress_format_entangled(entangled_addr, l1i_entangled_table[l1i_cpu_id][set][way].format)) {
	if (accessed && l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] < L1I_CONFIDENCE_COUNTER_MAX_VALUE) {
	  l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k]++;
	}
	if (!accessed && l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] > 0) {
	  l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k]--;
	}
      }
    }
  }
}

// INTERFACE

void O3_CPU::l1i_prefetcher_initialize() 
{
  cout << "CPU " << cpu << " Entangling prefetcher" << endl;

  l1i_cpu_id = cpu;
  l1i_init_stats_table();
  l1i_last_basic_block = 0;
  l1i_consecutive_count = 0;
  l1i_basic_block_merge_diff = 0;

  l1i_init_hist_table();
  l1i_init_entangled_table();
}

void O3_CPU::l1i_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)
{
}

void O3_CPU::l1i_prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit)
{
  l1i_cpu_id = cpu;
  uint64_t line_addr = v_addr >> LOG2_BLOCK_SIZE;

  if (!cache_hit) assert(!prefetch_hit);
  
  l1i_stats_table[cpu][(line_addr & L1I_STATS_TABLE_MASK)].accesses++;
  if (!cache_hit) {
    l1i_stats_table[cpu][(line_addr & L1I_STATS_TABLE_MASK)].misses++;
  }
  if (prefetch_hit) {
    l1i_stats_table[cpu][(line_addr & L1I_STATS_TABLE_MASK)].hits++;
  }

  bool consecutive = false;
  
  if (l1i_last_basic_block + l1i_consecutive_count == line_addr) { // Same
    return;
  } else if (l1i_last_basic_block + l1i_consecutive_count + 1 == line_addr) { // Consecutive
    l1i_consecutive_count++;
    consecutive = true;
  }
      
  // Queue basic block prefetches
  uint32_t bb_size = l1i_get_bbsize_entangled_table(line_addr);
  if (bb_size) l1i_stats_basic_blocks[bb_size]++;
  for (uint32_t i = 1; i <= bb_size; i++) {
    uint64_t pf_addr = v_addr + i * (1<<LOG2_BLOCK_SIZE);
    if (!L1I.ongoing_request_vaddr(pf_addr)) {
      prefetch_code_line(pf_addr, (long)-1);
    }
  }
  
  // Queue entangled and basic block of entangled prefetches
  uint32_t num_entangled = 0;
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    long source_ent = -1;
    uint64_t entangled_line_addr = l1i_get_entangled_addr_entangled_table(line_addr, k, source_ent);
    if (entangled_line_addr && (entangled_line_addr != line_addr)) {
      num_entangled++;
      uint32_t bb_size = l1i_get_bbsize_entangled_table(entangled_line_addr);
      if (bb_size) l1i_stats_basic_blocks_ent[bb_size]++;
      for (uint32_t i = 0; i <= bb_size; i++) {
	uint64_t pf_line_addr = entangled_line_addr + i;
	if (!L1I.ongoing_request_vaddr(pf_line_addr << LOG2_BLOCK_SIZE)) {
	  prefetch_code_line(pf_line_addr << LOG2_BLOCK_SIZE, (i == 0) ? source_ent : (long)-1);
	}
      }
    }
  }
  if (num_entangled) l1i_stats_entangled[num_entangled]++; 

  if (!consecutive) { // New basic block found
    uint32_t max_bb_size = l1i_get_bbsize_entangled_table(l1i_last_basic_block);

    // Check for merging bb opportunities
    if (l1i_consecutive_count) { // single blocks no need to merge and are not inserted in the entangled table
      if (l1i_basic_block_merge_diff > 0) {
	l1i_add_bbsize_table(l1i_last_basic_block - l1i_basic_block_merge_diff, l1i_consecutive_count + l1i_basic_block_merge_diff);
	l1i_add_bb_size_hist_table(l1i_last_basic_block - l1i_basic_block_merge_diff, l1i_consecutive_count + l1i_basic_block_merge_diff);
      } else {
	l1i_add_bbsize_table(l1i_last_basic_block, max(max_bb_size, l1i_consecutive_count));
   	l1i_add_bb_size_hist_table(l1i_last_basic_block, max(max_bb_size, l1i_consecutive_count));
      }
    }
  }
  
  if (!consecutive) { // New basic block found
    l1i_consecutive_count = 0;
    l1i_last_basic_block = line_addr;
  }  

  if (!consecutive) {
    l1i_basic_block_merge_diff = l1i_find_bb_merge_hist_table(l1i_last_basic_block);
  }
  
  // Add the request in the history buffer
  if (!consecutive && l1i_basic_block_merge_diff == 0) {
    if ((l1i_find_hist_entry(line_addr) == L1I_HIST_TABLE_ENTRIES)) {
      l1i_add_hist_table(line_addr);
    } // else {
      // if (!cache_hit && !L1I.ongoing_demanded_request(line_addr)) {
      // 	l1i_add_hist_table(line_addr);      
      // }
    // }
  }

}

void O3_CPU::l1i_prefetcher_cycle_operate()
{
  if (!all_warmed_up && all_warmup_complete > NUM_CPUS) {
    l1i_init_stats_table();
    all_warmed_up = true;
  }
}

void O3_CPU::l1i_prefetcher_cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr, PACKET &filling_entry, BLOCK &evicting_entry)
{
  l1i_cpu_id = cpu;
  uint64_t line_addr = (v_addr >> LOG2_BLOCK_SIZE);
  uint64_t evicted_line_addr = (evicted_v_addr >> LOG2_BLOCK_SIZE);

  // Update confidence if late
  if (filling_entry.demanded && filling_entry.source_ent >= 0) {
    l1i_update_confidence_entangled_table(filling_entry.source_ent, line_addr, false);
    l1i_stats_table[cpu][(line_addr & L1I_STATS_TABLE_MASK)].late++;
  }
  
  if (evicted_v_addr) { // If there is an eviction
    if (evicting_entry.prefetch) {
      l1i_stats_table[cpu][(evicted_line_addr & L1I_STATS_TABLE_MASK)].wrong++;
    }
    if (evicting_entry.source_ent >= 0) {
      // If demanded hit, but if not wrong
      l1i_update_confidence_entangled_table(evicting_entry.source_ent, evicted_line_addr, !evicting_entry.prefetch);
    }
  }


  uint64_t latency = 0;
  if (filling_entry.demanded) latency = current_core_cycle[cpu] - filling_entry.timestamp;
  
  // Get and update entangled
  if (latency) {
    bool inserted = false;
    for (uint32_t i = 0; i < L1I_TRIES_AVAIL_ENTANGLED; i++) {
      uint64_t src_entangled = l1i_get_src_entangled_hist_table(line_addr, latency, i);
      if (src_entangled && line_addr != src_entangled) {
	if (l1i_avail_entangled_table(src_entangled, line_addr, false)) {
	  l1i_add_entangled_table(src_entangled, line_addr);
	  inserted = true;
	  break;
	}
      }
    }
    if (!inserted) {
      uint64_t src_entangled = l1i_get_src_entangled_hist_table(line_addr, latency);
      if (src_entangled && line_addr != src_entangled) {
	l1i_add_entangled_table(src_entangled, line_addr);
      }
    }
  }
}

void O3_CPU::l1i_prefetcher_final_stats()
{
  cout << "CPU " << cpu << " L1I Entangling prefetcher final stats" << endl;
  l1i_print_stats_table();
}
