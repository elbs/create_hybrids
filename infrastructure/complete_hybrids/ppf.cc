#include "ppf.h"

uint64_t TRACKING_TABLE::get_hash(uint64_t feature, uint64_t limit){
  assert(feature >= 0);
  uint64_t hash = feature;
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  hash %= limit;
  //printf("%ld\n", hash);
  assert(hash < limit);
  return hash; 
}

vector<uint64_t> TRACKING_TABLE::get_feat(uint64_t addr){
  uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
  int way = get_hash(baddr, TRACKING_TABLE_SIZE);
  assert(way < TRACKING_TABLE_SIZE);

  //Find an entry for the address in the tracking table
  vector<uint64_t> ret_feat;
  if(entries[way].valid && ((entries[way].addr >> LOG2_BLOCK_SIZE) == baddr)){
    //Return the features in the valid entry
    ret_feat = entries[way].features;
  }

  //Return an empty vector if no entries are found
  return ret_feat; 
}

bool TRACKING_TABLE::check_entry(uint64_t addr){
  uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
  int way = get_hash(baddr, TRACKING_TABLE_SIZE);
  assert(way < TRACKING_TABLE_SIZE);

  //Check if there is a valid entry for the address in the tracking table
  if(entries[way].valid && ((entries[way].addr >> LOG2_BLOCK_SIZE) == baddr))
    return true;

  return false;
}

//Adds a new entry to ppf using the address to access the tables and a vector of features
pair<uint64_t, vector<uint64_t>> TRACKING_TABLE::add_entry(uint64_t addr, vector<uint64_t> features){
  uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
  int way = get_hash(baddr, TRACKING_TABLE_SIZE);
  
  vector<uint64_t> feat;
  uint64_t evict_addr;

  pair<uint64_t, vector<uint64_t>> evict_pair;

  //Something is already present
  if(entries[way].valid && (entries[way].addr >> LOG2_BLOCK_SIZE) != baddr){
    
    //Evict the entry and return its features
    feat = entries[way].features;
    evict_addr = entries[way].addr;
    evict_pair = make_pair(evict_addr, feat);

    entries[way].valid = true;
    entries[way].features = features;
    entries[way].addr = addr; 

  }else if(!entries[way].valid){

    //invalid entry found
    entries[way].valid = true;
    entries[way].features = features;
    entries[way].addr = addr;
    evict_pair = make_pair(0, feat);

  }else{
    //Sanity check, should not occur
    assert(false);
  }

  return evict_pair;
}

pair<uint64_t, vector<uint64_t>> TRACKING_TABLE::remove_entry(uint64_t addr){

  uint64_t baddr = addr >> LOG2_BLOCK_SIZE;
  int way = get_hash(baddr, TRACKING_TABLE_SIZE);
  vector<uint64_t> features;
  pair<uint64_t, vector<uint64_t>> evict_pair;

  if((entries[way].addr >> LOG2_BLOCK_SIZE) == baddr){
    features = entries[way].features;
    evict_pair = make_pair(entries[way].addr, features);
    entries[way].valid = false;
  }else{
    evict_pair = make_pair(0, features);
  }

  return evict_pair;
}

bool PPF::check_filter(uint64_t addr, vector<uint64_t> features){

  //printf("%d Incremented %ld Decremented %ld\n", ppf_id, increment_weight, decrement_weight);

  //Check if the address is present in either reject or prefetch table
  bool in_tables = reject_table.check_entry(addr) || prefetch_table.check_entry(addr);

  if(reject_table.check_entry(addr))
    reject_table_hit++;
  else if(prefetch_table.check_entry(addr))
    accept_table_hit++;

  //If addr has been recently requested as a prefetch
  //do not re-prefetch
  if(in_tables)
    return false;

  //Get the weights for the features
  vector<int> weights = get_weights(features);
  assert(weights.size() > 0); 

  //Get the sum of the features
  int sum = 0;
  for(int a : weights){
    sum += a;
  }

  pair<uint64_t, vector<uint64_t>> evict_pair;

  //printf("%d SUM: %d\n\n", ppf_id, sum);

  if(sum > sum_max)
    sum_max = sum;
  if(sum < sum_min)
    sum_min = sum;

  int weight_val = 0;

  //If the sum of the weights is over the acceptable threshold
  if(sum > FILTER_THRESHOLD){

    //Normalize to create an index for measuring PPF statistics
    int temp = 0;
    temp = sum + (MAX_FEAT * NUM_FEAT); 

    assert(temp > 0 && MAX_FEAT != 0);
    uint64_t index = temp/(MAX_FEAT);
    if(index == sum_distro.size())
      index = sum_distro.size() - 1;
    if(index >= sum_distro.size())
      printf("%d %d %d %ld\n", temp, index, sum, sum_distro.size());
    assert(index < sum_distro.size()); 

    sum_distro[index]++;

    //Add the entry to the prefetch table
    evict_pair = prefetch_table.add_entry(addr, features);

    //If no entry was evicted, allow the prefetch
    //to go through without updating the tables
    if(evict_pair.first == 0)
      return true;

    eviction_update++;

    vector<int> evict_weights = get_weights(evict_pair.second);

    int evict_sum = 0;
    for(auto e_weight : evict_weights)
      evict_sum += e_weight;

    //Below training threshold so do not updatec
    if(abs(evict_sum) > TRAINING_THRESH)
      return true;

    //If eviction candidate is returned, decrement to teach
    //that it was a useless prefetch    
    for(int a = 0; a < evict_pair.second.size(); a++){

      weight_val = ppf_table[a][get_hash(evict_pair.second[a], FEAT_TABLE_SIZE)];

      //Decrement the evicted prefetch table entry's weights since the 
      //prediction was wrong
      if(abs(weight_val) < MAX_FEAT){
        ppf_table[a][get_hash(evict_pair.second[a], FEAT_TABLE_SIZE)]--;
        decrement_weight++;
      }

      //if(abs(weight_val) >= MAX_FEAT)
      //  weight_val >>= 1;
    }

    return true;
  }else{
    evict_pair = reject_table.add_entry(addr, features);

    if(evict_pair.first == 0)
      return false;
    
    eviction_update++;

    //Get the sum to check if it was over the threshold
    int temp = 0;
    temp = sum + (MAX_FEAT * NUM_FEAT); 
    int index = temp/(MAX_FEAT);
    if(index == sum_distro.size())
      index = sum_distro.size() - 1;
    if(index >= sum_distro.size())
      printf("%d %d %d %ld\n", temp, index, sum, sum_distro.size());
    assert(index < sum_distro.size()); 
    sum_distro[index]++;
    
    vector<int> evict_weights = get_weights(evict_pair.second);

    int evict_sum = 0;
    for(auto e_weight : evict_weights)
      evict_sum += e_weight; 

    if(abs(evict_sum) > TRAINING_THRESH)
      return false;
 

    //If eviction candidate is returned, decrement to reinforce
    //that it was a useless prefetch, if the training rules are met
    for(int a = 0; a < evict_pair.second.size(); a++){

      weight_val = ppf_table[a][get_hash(evict_pair.second[a], FEAT_TABLE_SIZE)];

      if(abs(weight_val) <  MAX_FEAT){
        ppf_table[a][get_hash(evict_pair.second[a], FEAT_TABLE_SIZE)]--;
        decrement_weight++;
      }

      //if(abs(weight_val) >= MAX_FEAT)
      //  weight_val >>= 1;
    }

    return false;
  }
}

//Selects which level of the cache to send the prefetch to based on the features' weights
PF_LEVEL PPF::check_filter_level(uint64_t addr, vector<uint64_t> features){

  //printf("%d Incremented %ld Decremented %ld\n", ppf_id, increment_weight, decrement_weight);

  bool in_tables = reject_table.check_entry(addr) || prefetch_table.check_entry(addr);

  if(reject_table.check_entry(addr))
    reject_table_hit++;
  else if(prefetch_table.check_entry(addr))
    accept_table_hit++;

  //If its been recently requested as a prefetch
  //do not re-prefetch
  if(in_tables)
    return PF_REJECT;

  //Get the weights for the features
  vector<int> weights = get_weights(features);
  assert(weights.size() > 0); 

  //Get the sum of the features
  int sum = 0;
  for(int a : weights){
    sum += a;
  }

  pair<uint64_t, vector<uint64_t>> evict_pair;

  //printf("%d SUM: %d\n\n", ppf_id, sum);

  if(sum > sum_max)
    sum_max = sum;
  if(sum < sum_min)
    sum_min = sum;

  int weight_val = 0;

  //If the sum of the weights is over the acceptable threshold
  if(sum > FILTER_THRESHOLD){

    int temp = 0;
    temp = sum + (MAX_FEAT * NUM_FEAT); 
    int index = temp/(MAX_FEAT);
    if(index == sum_distro.size())
      index = sum_distro.size() - 1;
    if(index >= sum_distro.size())
      printf("%d %d %d %ld\n", temp, index, sum, sum_distro.size());
    assert(index < sum_distro.size()); 

    sum_distro[index]++;

    //Add the entry to the prefetch table
    evict_pair = prefetch_table.add_entry(addr, features);

    //If no entry was evicted, allow the prefetch
    //to go through without updating the tables
    if(evict_pair.first == 0)
      return PF_L1;//true;

    eviction_update++;

    vector<int> evict_weights = get_weights(evict_pair.second);

    int evict_sum = 0;
    for(auto e_weight : evict_weights)
      evict_sum += e_weight;

    //Below training threshold so do not updatec
    if(abs(evict_sum) > TRAINING_THRESH)
      return PF_L1; //true;

    //If eviction candidate is returned, decrement to teach
    //that it was a useless prefetch    
    for(int a = 0; a < evict_pair.second.size(); a++){

      weight_val = ppf_table[a][get_hash(evict_pair.second[a], FEAT_TABLE_SIZE)];

      //Decrement the evicted prefetch table entry's weights since the 
      //prediction was wrong
      if(abs(weight_val) < MAX_FEAT){
        ppf_table[a][get_hash(evict_pair.second[a], FEAT_TABLE_SIZE)]--;
        decrement_weight++;
      }
    }

    return PF_L1;
  }else{

    evict_pair = reject_table.add_entry(addr, features);

    if(evict_pair.first == 0)
      return sum > L2_THRESHOLD ? PF_L2 : PF_REJECT; //false;
    
    eviction_update++;

    //Get the sum to check if it was over the threshold
    int temp = 0;
    temp = sum + (MAX_FEAT * NUM_FEAT); 
    int index = temp/(MAX_FEAT);
    if(index == sum_distro.size())
      index = sum_distro.size() - 1;
    if(index >= sum_distro.size())
      printf("%d %d %d %ld\n", temp, index, sum, sum_distro.size());
    assert(index < sum_distro.size()); 
    sum_distro[index]++;
    
    vector<int> evict_weights = get_weights(evict_pair.second);

    int evict_sum = 0;
    for(auto e_weight : evict_weights)
      evict_sum += e_weight; 

    //No need to train, return whether to send to the L2 or reject the PF
    if(abs(evict_sum) > TRAINING_THRESH)
      return sum > L2_THRESHOLD ? PF_L2 : PF_REJECT;

    //If eviction candidate is returned, decrement to reinforce
    //that it was a useless prefetch, if the training rules are met
    for(int a = 0; a < evict_pair.second.size(); a++){

      weight_val = ppf_table[a][get_hash(evict_pair.second[a], FEAT_TABLE_SIZE)];

      if(abs(weight_val) <  MAX_FEAT){
        ppf_table[a][get_hash(evict_pair.second[a], FEAT_TABLE_SIZE)]--;
        decrement_weight++;
      }

      //if(abs(weight_val) >= MAX_FEAT)
      //  weight_val >>= 1;
    }

    return sum > L2_THRESHOLD ? PF_L2 : PF_REJECT; //false;
    //return false;
  }
}    

void PPF::update_filter(uint64_t addr, bool cache_hit){

  //Check the tables
  vector<uint64_t> reject_entry = reject_table.get_feat(addr);
  vector<uint64_t> accept_entry = prefetch_table.get_feat(addr);

  //This was not recently filtered
  if(reject_entry.size() == 0 && accept_entry.size() == 0)
    return;

  //An address should be present in only one of the tracking
  //tables
  assert(reject_entry.size() == 0 ^ accept_entry.size() == 0);

  if(reject_entry.size() != 0){
    
    reject_trigger++;

    //Get the sum to check if it was over the threshold
    int sum = 0;

    assert(reject_entry.size() == ppf_table.size());

    vector<int> weights = get_weights(reject_entry);
    for(auto a : weights)
      sum += a;
 
    if(abs(sum) > TRAINING_THRESH)
      return;
 
    int weight_val = 0;
    for(int a = 0; a < reject_entry.size(); a++){
      weight_val = ppf_table[a][get_hash(reject_entry[a], FEAT_TABLE_SIZE)];

      //Increment, since it might have been a hit if the prefetch
      //had been accepted

      //Will decrement when the entry is evicted from reject table
      if(!cache_hit && abs(weight_val) < MAX_FEAT){
        ppf_table[a][get_hash(reject_entry[a], FEAT_TABLE_SIZE)]++;
        increment_weight++;
      }else if(abs(weight_val) < MAX_FEAT){
        ppf_table[a][get_hash(reject_entry[a], FEAT_TABLE_SIZE)]--;
        decrement_weight++;
      }
    }

    reject_table.remove_entry(addr);
    return;

  }else if(accept_entry.size() != 0){

    //Get the sum to check if it was over the threshold
    int sum = 0;
    accept_trigger++;

    assert(accept_entry.size() == ppf_table.size());
    vector<int> weights = get_weights(accept_entry);

    for(auto a : weights)
      sum += a;
    
    if(abs(sum) > TRAINING_THRESH)
      return;
  
    int weight_val = 0;
    for(int a = 0; a < accept_entry.size(); a++){

      weight_val = ppf_table[a][get_hash(accept_entry[a], FEAT_TABLE_SIZE)];

      if(cache_hit && abs(weight_val) < MAX_FEAT){
        ppf_table[a][get_hash(accept_entry[a], FEAT_TABLE_SIZE)]++;
        increment_weight++;
      }else if(!cache_hit && abs(weight_val) < MAX_FEAT){
        ppf_table[a][get_hash(accept_entry[a], FEAT_TABLE_SIZE)]--;
        decrement_weight++;
      }
    }

    prefetch_table.remove_entry(addr);

    return;
  }
  
  return;
}

//Extremely simple hash
uint64_t PPF::get_hash(uint64_t feature, int limit){
  assert(feature >= 0);
  size_t i = 0;
  uint64_t hash = feature;
  assert(limit != 0);
  //hash += hash << 3;
  //hash ^= hash >> 11;
  //hash += hash << 15;
  hash %= limit;
  assert(hash < limit);
  return hash; 
}

vector<int> PPF::get_weights(vector<uint64_t> features){

  assert(features.size() == ppf_table.size());
  vector<int> weights;
  int index = 0;
  for(int a = 0; a < features.size(); a++){

    //printf("id %d : %lu : %d : %d\n", 
    //  ppf_id, features[a], get_hash(features[a], FEAT_TABLE_SIZE), 
    //  ppf_table[a][get_hash(features[a], FEAT_TABLE_SIZE)]);

    index = get_hash(features[a], FEAT_TABLE_SIZE); 

    if(find(unique_indexes[a].begin(), unique_indexes[a].end(), index) == unique_indexes[a].end())
      unique_indexes[a].push_back(index); 

    weights.push_back(ppf_table[a][get_hash(features[a], FEAT_TABLE_SIZE)]);
  }

  return weights;
}


//Returns a distribution of PPF's features' weights
vector<uint64_t> PPF::get_feat_distro(int feat_num){

  vector<uint64_t> weight_count((MAX_FEAT * 2)/8, 0);
  int temp = 0;
  int index = 0;
  for(auto a : ppf_table[feat_num]){
    temp = a + MAX_FEAT;
    index = temp/8;
    if(index == weight_count.size())
      index = weight_count.size() - 1;
    assert(index >= 0 && index < weight_count.size());
    weight_count[index]++;
  }
  return weight_count; 
}

vector<uint64_t> PPF::get_sum_distro(){
  return sum_distro;
}



