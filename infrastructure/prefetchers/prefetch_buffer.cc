#include "prefetch_buffer.h"

//#define EPOCH_DEBUG

const bool METRICS_ON = false;
const bool ALLOC_ON = false;

const float comparison_metric = 0.15;

// Needed to compare two buffer entries for iteration
bool operator== ( const PF_BUFFER_ENTRY &pfb1, const PF_BUFFER_ENTRY &pfb2) {

  uint64_t shifted_addr1 = pfb1.pf_addr << LOG2_BLOCK_SIZE;
  uint64_t shifted_addr2 = pfb2.pf_addr << LOG2_BLOCK_SIZE;
  
  return shifted_addr1 == shifted_addr2;
}

// ----------------------------------------------------------------------------
// Generates and returns a deque of (up to) num_to_fetch prefetches from the 
// subprefetchers' buffers. May or may 
// not use a Shadow Cache (via pointer) to check whether addresses 
// are already in the cache or not.
//
// ----------------------------------------------------------------------------
deque<PF_BUFFER_ENTRY> PREFETCH_BUFFER::generate_prefetches(int num_to_fetch, SHADOW_CACHE *sc){

  //allocate_prefetches(num_to_fetch);
  // The deque to return 
  deque<PF_BUFFER_ENTRY> prefetches;

  // Number of prefetches grabbed so far, 
  int num_prefetched = 0;

  // Check num_buff array for quick sweep of current deque sizes,
  // if all buffers are empty, stop and return empty deque
  uint32_t tot_entries = 0;
  for(uint32_t i = 0; i < num_subprefs; i++) {
    tot_entries += num_buff[i]; 
  }
  if(!tot_entries)
    return prefetches;

//else...
#ifdef ROUND_ROBIN

  // While we have prefetche slots to allocate, and prefetch
  // entries to choose from
  int attempt_pf = 0;
  vector<int>allocs = allocate_prefetches(num_to_fetch);
  vector<int> n_pf;

  for(uint32_t a = 0; a < num_subprefs; a++)
    n_pf.push_back(0);

  int num_with_pf = 0;
  uint8_t has_pf = 0;
  for(uint32_t a = 0; a < num_subprefs; a++){
    if(num_buff[a] != 0){
      has_pf |= (1 << a);
      num_with_pf++;
    }
  }

  pf_gen_scenario[has_pf]++;
  
  while(num_prefetched < num_to_fetch && tot_entries != 0){// && attempt_pf < (num_to_fetch * 2)) {

    // For each subprefetcher's deque...
    for(uint32_t i = 0; i < num_subprefs; i++) {
  
      // index value actually depends on subprefetcher
      // priority ordering (can be modified later)
      int b = subpref_order.at(i);
      attempt_pf++; 
      // if subprefetcher's deque has at least one entry... 
      if(!pf_buffer[b].empty() && (n_pf[b] < allocs[b] || !ALLOC_ON)) {
    
        // Check the entry is not already in the deque of prefetches
        if(find(prefetches.begin(), prefetches.end(), pf_buffer[b].front()) == prefetches.end()) {
        
          // Debug
          if(debug_mode) {
            cout << "Grabbing: ";
            pf_buffer[b].front().print_entry();
            cout << endl;
          }
          
          // Also check whether entry's address is in the shadow cache
          // if there is one
          bool hit = 0; 
          bool pre = 0;
          if(sc != NULL) {
            (*sc).access_cache(pf_buffer[b].front().pf_addr, &hit, &pre, NULL, b, NULL, 0, NULL, NULL, ACCESS_PROBE);

            //Add it to the history for confidence value
            if(!hit && !pre){
              uint64_t e_tag = 0;
              (*sc).access_cache(pf_buffer[b].front().pf_addr, &hit, &pre, NULL, b, NULL, 0, NULL, NULL, ACCESS_PROBE);
        
              if(METRICS_ON || ALLOC_ON){
                (*sc).access_cache(pf_buffer[b].front().pf_addr, NULL, NULL, NULL, NULL, &e_tag, 0, NULL, NULL, ACCESS_PREFETCH);

                pf_sent_count[i]++;
                pf_history[i].push_back(pf_buffer[b].front());

                pf_evict[i].push_back(e_tag);

                if(pf_evict[i].size() > MAX_ACC_VAL){
                  pf_evict[i].erase(pf_evict[i].begin());
                }

                if(pf_history[i].size() > MAX_ACC_VAL){
                  //if(!pf_history[i].front().hit && accuracy_count[i] != 0)
                  //  accuracy_count[i]--;
                  pf_history[i].erase(pf_history[i].begin());
                }
              }

              //assert(pf_history[i].size() <= MAX_ACC_VAL);
              n_pf[b]++;
            }
          }
          
          // If neither in the deque, nor in the shadow cache,
          // or an awaiting prefetch hit, add it to the prefetches deque.
          // Note: Condition of if-statement works even with no shadow cache
          //printf("%f gt %f = %f - %f\n", get_coverage(b), 
          //  (last_cov[b] - (last_cov[b] * 0.005)), last_cov[b], last_cov[b] * 0.05);
          if(!hit && !pre && ((get_accuracy(b)/get_harmful(b) > comparison_metric || !METRICS_ON))) {
          //(get_coverage(b) >= (last_cov[b] - (last_cov[b] * 0.05)) && get_coverage(b) > 0.01)
          //|| get_accuracy(b) > 0.1 || !METRICS_ON) && 
            // Pop out the front buffer entry and add it to deque 
            prefetches.push_back(pf_buffer[b].front());

            pf_buffer[b].pop_front();
            num_buff[b]--;

            // Got one! 
            num_prefetched++;
          }
          else { // hit in the cache! We can drop it...
            
            // Debug
            if(debug_mode) {
              cout << "Already in cache; dropping: ";
              pf_buffer[b].front().print_entry();
              cout << endl;
            }
            
            // It was in the shadow cache already. Simply discard it. 
            pf_buffer[b].pop_front();
            num_buff[b]--;
          }
        }
        else { // hit in the prefetch deque! We can drop it.
         
          deque<PF_BUFFER_ENTRY>::iterator it = find(prefetches.begin(), prefetches.end(), pf_buffer[b].front()); 

          if(it->pref_unit_id != b){
            it->pref_overlap_id |= b;
            assert(it->pref_unit_id != it->pref_overlap_id);
          }
          
          // Debug
          if(debug_mode) {
            cout << "Already in deque; dropping: ";
            pf_buffer[b].front().print_entry();
            cout << endl;
          }
          
          // It was in the deque already. Simply discard it. 
          pf_buffer[b].pop_front();
          num_buff[b]--;
        }
      }
    }

    // Check sizes of buffers again
    tot_entries = 0;
    for(uint32_t i = 0; i < num_subprefs; i++)
      tot_entries += num_buff[i];
  }

  // Debug 
  // Print out list of final prefetch 
  if(debug_mode) { 
    cout << "The cycle's final prefetch deque:";
    for (uint32_t i = 0; i < prefetches.size(); i++)
      prefetches.at(i).print_entry();
    cout << endl;
  }

  return prefetches;

//#elif defined(WEIGHTED_ORDER)
//
//  while(num_prefetched < PF_NUM){
//    for(int a = num_subprefs-1; a >= 0 && num_prefetched < PF_NUM; a--){
//      for(uint32_t b = 0; b < num_buff[a] && num_prefetched < PF_NUM; b++){
//
//      }
//    }
//  }
//  return prefetches;
//
//#else
//    assert(false);
#endif

#ifdef VOTERS
  //Find prefetches that are in 2 or more prefetch buffers
  map<uint64_t, int> pf_counts;
  vector<PF_BUFFER_ENTRY> pf;
  for(uint32_t a = 0; a < num_subprefs; a++){
    int b = subpref_order.at(a);
    if(!pf_buffer[b].empty()) {
      for(int c = 0; c < pf_buffer[b].size(); c++){
        if(pf_counts.find(pf_buffer[b][c].pf_addr) == pf_counts.end())
          pf_counts[pf_buffer[b][c].pf_addr] = 1;
        else
          pf_counts[pf_buffer[b][c].pf_addr]++;
      }
    } 
  } 
 
  vector<uint64_t> removal; 
  for(auto ent : pf_counts){
    if(prefetches.size() == num_to_fetch)
      break;
    if(ent.second >= 2){
      for(int a = 0; a < prefetches.size(); a++)
        if(prefetches[a].pf_addr == ent.first)
          continue;
      bool hit = 0; 
      bool pre = 0;
      if(sc != NULL) {
        (*sc).access_cache(ent.first, &hit, &pre, 0, NULL, NULL, ACCESS_PROBE);
      } 

      if(!hit && !pre) {
        bool found = false;
        for(uint32_t a = 0; a < num_subprefs; a++){
          for(int b = 0; b < pf_buffer[a].size(); b++){
            if(pf_buffer[a][b].pf_addr == ent.first){
              prefetches.push_back(pf_buffer[a][b]);
              //prefetches.push_back(ent.first);
              found = true;
              break;
            }
            if(found)
              break;
          }
          if(found)
            break;
        }

        // Pop out the front buffer entry and add it to deque 
        removal.push_back(ent.first);

        // Got one! 
        num_prefetched++;
      }
      else { // hit in the cache! We can drop it...

        // It was in the shadow cache already. Simply discard it. 
        removal.push_back(ent.first);
      }
    }
  }

  if(prefetches.size() < num_to_fetch){
    for(auto ent : pf_counts){
      if(prefetches.size() == num_to_fetch)
        break;
      if(ent.second >= 1){
        for(int a = 0; a < prefetches.size(); a++)
          if(prefetches[a].pf_addr == ent.first)
            continue;

        bool hit = 0; 
        bool pre = 0;
        if(sc != NULL) {
          (*sc).access_cache(ent.first, &hit, &pre, 0, NULL, NULL, ACCESS_PROBE);
        } 

        if(!hit && !pre) {

          // Pop out the front buffer entry and add it to deque 
          bool found = false;
          for(uint32_t a = 0; a < num_subprefs; a++){

            if(found)
              break;

            for(int b = 0; b < pf_buffer[a].size(); b++){
              if(pf_buffer[a][b].pf_addr == ent.first){
                prefetches.push_back(pf_buffer[a][b]);
                //prefetches.push_back(ent.first);
                found = true;
                break;
              }
              if(found)
                break;
            }
          } 
          removal.push_back(ent.first);

          // Got one! 
          num_prefetched++;
        }
        else { // hit in the cache! We can drop it...

          // It was in the shadow cache already. Simply discard it. 
          removal.push_back(ent.first);
        }
      }
    }
  }

  if(removal.size() == 0)
    return prefetches;

  for(uint32_t a = 0; a < num_subprefs; a++){
    int idx = 0;
    while(idx < pf_buffer[a].size()){
      if(find(removal.begin(), removal.end(), pf_buffer[a][idx].pf_addr) != removal.end()){
        pf_buffer[a].erase(pf_buffer[a].begin() + idx);
        num_buff[a]--;
      }else{
        idx++;
      }
    }
    //printf("%d\n", idx);
  }
  return prefetches; 
#endif

}

// ----------------------------------------------------------------------------
// Creates a PF_BUFFER_ENTRY from information passed as parameters, and then
// adds it to the proper subprefetcher buffer deque based on puid
// ----------------------------------------------------------------------------
void PREFETCH_BUFFER::add_pf_entry(uint64_t ip, uint64_t base_addr, uint64_t pf_addr,
                                   int prefetch_fill_level, uint32_t pf_page, bool pf_valid,
                                   uint32_t age, uint32_t puid, uint64_t timestamp, long source_ent) {

  // Create the entry
  PF_BUFFER_ENTRY pfb_entry(ip, base_addr, pf_addr, 
                            prefetch_fill_level, pf_page, pf_valid, 
                            age, puid, timestamp, source_ent);

  // Add it to the right buffer based on puid value
  // while ensuring the deque isn't overfilled
  if(num_buff[puid] < PF_BUFF_SIZE) {
    pf_buffer[puid].push_back(pfb_entry);
    
    // Increment buffer size tracker
    num_buff[puid]++;
  }
  // Else, buffer was full...
  else { 
    // do nothing. It simply doesn't get added
  }
}

// ----------------------------------------------------------------------------
// Prints contents of all the subprefetcher buffers.
// ----------------------------------------------------------------------------
void PREFETCH_BUFFER::print_contents() {

  for(uint32_t i = 0; i < num_subprefs; i++) {
    cout << "Subprefetcher " << i << ": ";
    for(int j = 0; j < pf_buffer[i].size(); j++) {
      PF_BUFFER_ENTRY p = pf_buffer[i].at(j);
      cout << "[" << p.ip << " " << p.base_addr << " " 
           << p.pf_addr << " " << p.prefetch_fill_level << " "
           << p.page << " " << p.valid << " " 
           << p.age << " " << p.pref_unit_id << "]";
    }
    cout << endl;
  }
}

// ----------------------------------------------------------------------------
// Prints size of each buffer deque. i.e. number of entries each has. 
// ----------------------------------------------------------------------------
void PREFETCH_BUFFER::print_counts() {
  
  cout << "Current buffer capacities: [";
  for(uint32_t i = 0; i < num_subprefs; i++) {
    cout << pf_buffer[i].size();
    if(i < num_subprefs-1)
      cout << ",";
    else
      cout << "]" << endl;
  }
}

// ----------------------------------------------------------------------------
// Prints the priority order of the subprefetchers for generating prefetches.
// ----------------------------------------------------------------------------
void PREFETCH_BUFFER::print_order() {
  
  cout << "Current Subprefetcher fetch order: [";
  for(uint32_t i = 0; i < MAX_NUM_SUBPREFS; i++) {
    cout << subpref_order.at(i);
    if(i < MAX_NUM_SUBPREFS-1)
      cout << ",";
    else
      cout << "]" << endl;
  }
}

// ----------------------------------------------------------------------------
// Upgrades position of a single prefetcher to the front of ordering.
// Currently not in use... but it can be! :D
// ----------------------------------------------------------------------------
void PREFETCH_BUFFER::upgrade_prefetcher(uint32_t puid) {

  // Find where puid is currently ranked, and remove
  deque<uint32_t>::iterator curr_puid_pos = find(subpref_order.begin(), subpref_order.end(), puid);
  subpref_order.erase(curr_puid_pos);
  
  // Now place it at the front 
  subpref_order.push_front(puid);

}

// ----------------------------------------------------------------------------
// Updates ordering of all prefetchers, needs a vector of the orderings.
// ----------------------------------------------------------------------------
void PREFETCH_BUFFER::upgrade_prefetchers(vector<uint32_t> new_order) {

  // Completely clear out deque
  subpref_order.clear();
  
  // Now put in the new order
  for(uint32_t i = 0; i < num_subprefs; i++)
    subpref_order.push_back(new_order.at(i));

}

//Allocate prefetches based on the number of prefetches in a buffer
//and the metric used to indicate a pfer is confident 
vector<int> PREFETCH_BUFFER::allocate_prefetches(int num_to_fetch){
  vector<int> alloc(num_subprefs, 0);
  //Find the total number of prefetches waiting
  int total_pf = 0;
  for(uint32_t a = 0; a < num_subprefs; a++){
    total_pf += num_buff[a];
  }

  if(total_pf == 0){
    return alloc;
  }

  vector<float> numpf_weight;

  int num_left = num_to_fetch;
  //printf("Total: %d NtP: %d\n", total_pf, num_to_fetch);
  for(uint32_t a = 0; a < num_subprefs; a++){
    numpf_weight.push_back(float(num_buff[a])/float(total_pf));

    //if(get_coverage(a) > comparison_metric)
    //  alloc[a] = (num_to_fetch/6); 
    //else 
    //  alloc[a] = num_to_fetch/3;
    //printf("%d : %f _ ", a, numpf_weight[a]);

  }

  //printf("\n");
 
  //Based on some metric, scale the number of prefetches that can be issued
  for(uint32_t a = 0; a < num_subprefs; a++){

    if(numpf_weight[a] == 0 || get_accuracy(a) == 0 || get_coverage(a) == 0){

      alloc[a] = 1;
      //printf("%d : %d NA _ ", a, alloc[a]);
    }else{
      //num_buff[a] 
      float factor = get_harmful(a) > 0 ? (get_coverage(a)/get_harmful(a)) * get_accuracy(a) : get_coverage(a) * get_accuracy(a); 
      //numpf_weight[a] * get_accuracy(a) * get_coverage(a);// * get_harmful(a);
      //factor *= numpf_weight[a];
      if(factor > 1.0)
        factor = 1.0;
      
      alloc[a] = (round(num_buff[a] * factor) + 1);//(round(num_to_fetch * factor) + 1);
      //printf("%d : %d %f _ ", a, alloc[a], factor);
    }

  }
  //printf("\n");

  //for(int a = 0; a < num_subprefs; a++){
  //  if(num_buff[a] < alloc[a])
  //    alloc[a] = num_buff[a];
  //}

  return alloc; 
}

void PREFETCH_BUFFER::update_accuracy(uint64_t addr, bool pf_hit){
 
  //Check each prefetcher's prefetch history to see if the address is present
  for(uint32_t i = 0; i < num_subprefs; i++){
    for(auto pf : pf_history[i]){ 

      //If it is 
      if((pf.pf_addr >> LOG2_BLOCK_SIZE) == (addr >> LOG2_BLOCK_SIZE) && !pf.hit){
        //and its a prefetch hit, increment its accuracy
        if(pf_hit && accuracy_count[i] < MAX_ACC_VAL){
          accuracy_count[i]++;
          pf.hit = true;
        //If it is and its a prefetch miss, decrement its accuracy
        }else if(!pf_hit && accuracy_count[i] != 0){
          accuracy_count[i]--;
        }
        //Only one update per prefetcher allowed
        break;
      }
    }
  }

}

float PREFETCH_BUFFER::get_accuracy(int pf_id){
  if(pf_hit_count[pf_id] > pf_sent_count[pf_id])
    pf_hit_count[pf_id] = pf_sent_count[pf_id];
  float acc = pf_sent_count[pf_id] > 0 ? float(pf_hit_count[pf_id])/float(pf_sent_count[pf_id]) : 0;
  //float acc = float(accuracy_count[pf_id])/float(MAX_ACC_VAL);
  //printf("acc %d %d %f\n", pf_hit_count[pf_id], pf_sent_count[pf_id], acc);
  assert(acc <= 1.0);
  return acc;
}

void PREFETCH_BUFFER::update_pf_hits(uint64_t addr, bool cache_hit,  bool pf_hit){
  
  epoch_hits += cache_hit;
  //Check each prefetcher's prefetch history to see if the address is present
  for(uint32_t i = 0; i < num_subprefs; i++){
    for(auto pf : pf_history[i]){ 

      //If it is 
      if((pf.pf_addr >> LOG2_BLOCK_SIZE) == (addr >> LOG2_BLOCK_SIZE)){
        //and its a prefetch hit, increment its accuracy
        if(pf_hit && pf_hit_count[i] < MAX_PF_HITS){
          pf_hit_count[i]++;
        }
        break;
      }
    }
  }
} 

uint64_t PREFETCH_BUFFER::get_pf_hits(int pf_id){
  uint64_t hits = last_epoch_pf_hit[pf_id] > 0 ? last_epoch_pf_hit[pf_id] : pf_hit_count[pf_id];
  return hits;
}

float PREFETCH_BUFFER::get_coverage(int pf_id){

  //Find the coverage based on the number of hits originating from
  //prefetchers in the last epoch 
  //OR
  //If this is the first epoch, base it on the number of hits so far
  float cov = last_epoch_hits > 0 ? float(last_epoch_pf_hit[pf_id])/float(last_epoch_hits) : float(pf_hit_count[pf_id])/float(epoch_hits);

  last_epoch_num = epoch_num; 
  return cov;
}


void PREFETCH_BUFFER::update_harmful(uint64_t addr, SHADOW_CACHE pf_cache1, SHADOW_CACHE pf_cache2, SHADOW_CACHE pf_cache3, SHADOW_CACHE bc){
 
  uint64_t tag = 0;
  int idx = -1;

  //int pf_cache_way = pf_cache.get_way(addr);
  int bc_way = bc.get_way(addr);
 
  if(bc_way == -1)
    return; 
  //printf("PF %d BC %d\n", pf_cache_way, bc_way);

  if(pf_cache1.get_way(addr) == -1)
    harmful_count[0]++;
  if(pf_cache2.get_way(addr) == -1)
    harmful_count[1]++;
  if(pf_cache3.get_way(addr) == -1)
    harmful_count[2]++;

  //alternative way of doing harmfulness calculations
  //for(uint32_t i = 0; i < num_subprefs; i++){
  //  idx = -1;
  //  for(auto pf : pf_evict[i]){
  //    tag = ((addr/BLOCK_SIZE) / L1I_SET) & ((1ull<<CACHE_PARTIAL_TAG_BITS)-1);
  //    if(pf == tag){// && bc.get_way(addr) == -1 && pf_cache.get_way(addr) != -1){
  //      harmful_count[i]++;
  //      break;
  //    }
  //    idx++;
  //  }

  //  if(idx != -1){
  //    pf_evict[i].erase(pf_evict[i].begin() + idx);
  //  } 

  //} 
}

float PREFETCH_BUFFER::get_harmful(int pf_id){
  float perc_harm = pf_sent_count[pf_id] > 0 && (epoch - epoch_hits) > 0 ? float(harmful_count[pf_id])/float(epoch - epoch_hits) : 0;//float(pf_sent_count[pf_id]) 
  //printf("%ld %ld %d %d\n", harmful_count[pf_id], epoch, epoch_hits, epoch - epoch_hits);
  return perc_harm;
}

void PREFETCH_BUFFER::inc_epoch(){
  //increment epoch
  epoch++;

 //if a new epoch
  if(epoch >= EPOCH_SIZE){

    //Set the last epoch's final value
    for(uint32_t a = 0; a < num_subprefs; a++){
      last_cov[a] = get_coverage(a);
      last_acc[a] = get_accuracy(a);
      last_harmful[a] = get_harmful(a);//harmful_count[a];
      last_epoch_pf_hit[a] = pf_hit_count[a];
      accuracy_count[a] >>= 1;

      pf_history[a].clear();
      pf_evict[a].clear();

      harmful_count[a] = 0;
      pf_sent_count[a] = 0;
      pf_hit_count[a] = 0;
    }
    
    last_epoch_hits = epoch_hits;
    epoch_hits = 0;
    epoch = 0;
    epoch_num++;

    #ifdef EPOCH_DEBUG
    if(last_epoch_num != epoch_num){
      printf(" HITRATE: %f EPOCH: %d ", float(last_epoch_hits)/float(EPOCH_SIZE), epoch_num);
      for(uint32_t pf_id = 0; pf_id < num_subprefs; pf_id++){ 
        float cov = last_epoch_hits > 0 ? float(last_epoch_pf_hit[pf_id])/float(last_epoch_hits) : float(pf_hit_count[pf_id])/float(epoch_hits);

        if(avg_cov[pf_id] == 0.0){
          avg_cov[pf_id] = cov;
        }else{
          avg_cov[pf_id] = float(avg_cov[pf_id] * (epoch_num - 1) + cov)/float(epoch_num);
        }

        if(avg_acc[pf_id] == 0.0){
          avg_acc[pf_id] = last_acc[pf_id];
        }else{
          avg_acc[pf_id] = float(avg_acc[pf_id] * (epoch_num - 1) + last_acc[pf_id])/float(epoch_num);
        }

        if(avg_harm[pf_id] == 0.0){
          //printf("last_harmful %f ", last_harmful[pf_id]);
          avg_harm[pf_id] = last_harmful[pf_id];
        }else{
          //printf("last_harmful %f ", last_harmful[pf_id]);
          avg_harm[pf_id] = float(avg_harm[pf_id] * (epoch_num - 1) + last_harmful[pf_id])/float(epoch_num);
        }
        float factor = avg_harm[pf_id] != 0 ? (avg_cov[pf_id]/avg_harm[pf_id]) * avg_acc[pf_id] : 1;
        if(factor > 1)
          factor = 1; 
        printf("cov %f acc %f harm %f F %f ", avg_cov[pf_id], avg_acc[pf_id], avg_harm[pf_id], factor);
      }
      printf("\n");
    }
    #endif
   } 
}
// ----------------------------------------------------------------------------
//update confidence on a cache access
//This can be called from the prefetcher_operate()
//and prefetcher_fill() function as it is overloaded below
// ----------------------------------------------------------------------------
//void PREFETCH_BUFFER::update_confidence(uint8_t cache_hit, uint8_t type, uint64_t addr){
//
//    uint64_t f_path_id = 0;
//    if(cache_hit == 0 && type == LOAD){
//        int temp_bucket = 0;
//        for(int a = 0; a < NUM_SUBPREFS; a++){
//            for(uint32_t b = 0; b < num_buff[a]; b++){
//                //If the miss address is in a prefetch buffer, get its path_id
//                if((pf_buffer[a][b].pf_addr >> LOG2_BLOCK_SIZE) == (addr >> LOG2_BLOCK_SIZE)){
//
//                    temp_bucket = int(pf_buffer[a][b].confidence) / NUM_SUBPREFS;//confidence < 10 ? 9 : confidence/NUM_SUBPREFS;
//                    temp_bucket =  temp_bucket < 10 ? temp_bucket : 9;
//
//                    assert(f_path_id == 0);
//                    f_path_id = pf_buffer[a][b].path_id;
//                    break;
//                }
//            }
//            if(f_path_id != 0)
//                break;
//        }
//        //total++;
//    }
//
//    if(f_path_id > 0){
//        //Upgrade all the prefetches on the path
//        for(int a = 0; a < NUM_SUBPREFS; a++){
//            if(num_buff[a] != 0){
//                for(uint32_t b = 0; b < num_buff[a]; b++){
//
//                    //If the current prefetch has the same id
//                    //as the path missed, increase its priority,
//                    //if not, then decrease it as an aging mechanism
//                    if(pf_buffer[a][b].path_id == f_path_id){
//
//                        int conf_bucket = int(pf_buffer[a][b].confidence / NUM_SUBPREFS);
//                        conf_bucket =  conf_bucket < 10 ? conf_bucket : 9;
//
//                        //int conf_bucket = (pf_buffer[a][b].confidence +
//                        //                    (100/NUM_SUBPREFS))/NUM_SUBPREFS;
//                        //conf_bucket =  conf_bucket < 10 ? 9 : conf_bucket/NUM_SUBPREFS;
//                        pf_buffer[a][b].confidence += (1 - CONF_ALPHA) *
//                                                      pf_buffer[a][b].confidence;
//
//                    }else{
//                        pf_buffer[a][b].confidence *= AGING_ALPHA;
//                    }
//                }
//            }
//        }
//    }else{
//        for(int a = 0; a < NUM_SUBPREFS; a++){
//            if(num_buff[a] != 0){
//                for(uint32_t b = 0; b < num_buff[a]; b++){
//                    //If the current prefetch has the same id as the path missed, increase its priority,
//                    //if not, then decrease it as an aging mechanism
//                    pf_buffer[a][b].confidence *= AGING_ALPHA;
//                }
//            }
//        }
//    }
//
//    for(int a = 0; a < NUM_SUBPREFS; a++){
//        for(uint32_t b = 0; b < num_buff[a]; b++){
//            int conf_bucket = int(pf_buffer[a][b].confidence / NUM_SUBPREFS);
//            conf_bucket =  conf_bucket < 10 ? conf_bucket : 9;
//            if(conf_bucket != a){
//                //move through each buffer and move any confidences in the correct buffer
//                move_buffer_entry(a, b, conf_bucket);
//            }
//        }
//    }
//
//
//    int t_total = 0;
//    int conf_perc[NUM_SUBPREFS];
//
//    for(int a = 0; a < NUM_SUBPREFS; a++){
//      t_total += useful_conf[a];
//    }
//
//    //Assign the percentage values
//    for(int a = 0; a < NUM_SUBPREFS; a++){
//      if(t_total == 0)
//        conf_perc[a] = 0;
//      else
//        conf_perc[a] = int(100 * float(useful_conf[a])/float(t_total));
//      conf_allocation[a] = 0;
//    }
//
//
//    conf_allocate(conf_allocation, conf_perc);
//}

// ----------------------------------------------------------------------------
//Update confidence for a fill in the cache
// ----------------------------------------------------------------------------
//void PREFETCH_BUFFER::update_confidence(bool val, bool prefetch, uint32_t confidence){
//
//  if(valid && prefetch){
//    int temp_bucket = confidence < 10 ? confidence/NUM_SUBPREFS : 9;
//    if(useful_conf[temp_bucket] > 0)
//      useful_conf[temp_bucket]--;
//  }
//
//  int t_total = 0;
//  int conf_perc[NUM_SUBPREFS];
//
//  for(int a = 0; a < NUM_SUBPREFS; a++){
//    t_total += useful_conf[a];
//  }
//
//  //Assign the useful percentage values
//  for(int a = 0; a < NUM_SUBPREFS; a++){
//    conf_perc[a] = 100 * float(useful_conf[a])/float(t_total);
//    conf_allocation[a] = 0;
//  }
//  conf_allocate(conf_allocation, conf_perc);
//
//}


bool PREFETCH_BUFFER::ongoing_request_vaddr_different_ent(uint64_t v_addr, int puid, long source_ent) {
  for(int j = 0; j < pf_buffer[puid].size(); j++) {
    PF_BUFFER_ENTRY p = pf_buffer[puid].at(j);
    if (p.pf_addr == v_addr && p.source_ent != source_ent) return true;
  }
  return false;
}

bool PREFETCH_BUFFER::ongoing_request_vaddr_different_puid(uint64_t v_addr, int puid) {
  for(uint32_t i = 0; i < num_subprefs; i++) {
    if (i != puid) {
      for(int j = 0; j < pf_buffer[i].size(); j++) {
	PF_BUFFER_ENTRY p = pf_buffer[i].at(j);
	if (p.pf_addr == v_addr) return true;
      }
    }
  }
  return false;
}
