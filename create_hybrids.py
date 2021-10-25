import os
import sys
import shutil
import itertools as it
import re

# These two lists are the only things you must add to, or modify
# TODO - JIP isn't playing nicely currently. 
prefetchers = ['Barca_10C', 'D-JOLT_10J', 'FNL-MMA_12E', 'PIPS_10F', 'TAP_10E', 'mana_10F', 'JIP_13N']
# TODO - make it so we can add up the total size of the combination easily
#p_sizes = [32.0, 30.3699951, 30.583984, 30.4755859, 31.953125, 31.48828, 31.0358887]

# Change these to where your prefetchers and hybrid prefetchers are
home = os.getcwd()
hybrids_dir = '/infrastructure/complete_hybrids/'
prefs_dir =  '/infrastructure/prefetchers/'
json_config_file = '/infrastructure/json_config_file/ipc_base.json'

# First, get the hybrid prefetchers' file names 
# from 'complete_hybrids' directory
# Parse the file names so that we know what to 
# use to name each resulting file
hybrid_prefetchers = os.listdir(home + hybrids_dir)

#For each hybrid prefetcher file...
# 1. Parse to create the future directory name
# 2. Look at file name to find choose-n number 
# 3. Iterate through the prefetchers-choose-n number 
#    of combinations of prefetchers
#    a. create directory with combination numbers
#    b. substitute 'XXX' etc. in files with n prefetchers
#    c. copy over the n prefetchers into directory
for h in hybrid_prefetchers:

  # Debug
  print('Original File Name: ' + h)

  # Use to name current configuration being made
  curr_combination = 1
  
  # Directory creation: Lose the '.cc' 
  hybrid_base = h.split('.')[0]
  
  # Debug
  print('Hybrid Base Name: ' + hybrid_base)
  
  # Find the integer number in the hybrid base's name
  # tells us how many prefetchers we need to fetch, 
  # minus one because EIP is eternal <3
  combination_amt = int(re.findall(r'\d+', hybrid_base)[0]) - 1
  
  # Debug
  print('Combination Count: ' + str(combination_amt))
  
  print('-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-**-*-')
  
  for c in it.combinations(prefetchers, combination_amt):
   
    # This combination's directory name
    comb_name = hybrid_base + '_' + str(curr_combination)
    comb_dir_name = hybrid_base + '_' + str(curr_combination) + '_l1i'
    
    # Debug
    print('Directory Name: ' + comb_dir_name)
  
    # Create a directory named comb_dir_name
    os.mkdir(home + '/' + comb_dir_name)

    # Copy over the hybrid prefetcher base
    shutil.copy2(home + hybrids_dir + h, home + '/' + comb_dir_name) 
    
    # Debug
    print('Combinations: ' + str(curr_combination) + ', ' + str(c) + '\n')
   
    # Convert tuple of prefetchers to array
    comb_prefs = list(c)

    # Copy over the individual combination prefetchers
    for p in comb_prefs:
      shutil.copy2(home + prefs_dir + p + '.inc', home + '/' + comb_dir_name)
    
    # Copy over ppf.cc, which all need
    shutil.copy2(home + prefs_dir + 'ppf.cc', home + '/' + comb_dir_name)
    
    # Copy over guaranteed EIP, which all combinations need
    shutil.copy2(home + prefs_dir + 'ISCA_Entangling_2Ke_NoShadows.inc', home + '/' + comb_dir_name)

    # And finally move over prefetch_buffer.cc, which all need
    shutil.copy2(home + prefs_dir + 'prefetch_buffer.cc', home + '/' + comb_dir_name)

    # Now change directory into the new subdir
    os.chdir(home + '/' + comb_dir_name)

    # Debug
    print('Now in directory: '+os.getcwd())

    # Strings we will be substituting in the hybrid file(s)
    # based on combination_amt
    # e.g. XXX -> Barca_A, YYY -> JIP_10E, ZZZ -> mana_10F
    str_subs = ['XXX','YYY','ZZZ','AAA']

    # Open up hybrid prefetcher file and substitute line by line
    for s in range(combination_amt):
      fin = open(h, 'r') 
      fout = open(h+'_new', 'w')
      for line in fin:
        fout.write(line.replace(str_subs[s],'"' + comb_prefs[s] + '.inc"'))
      fin.close()
      fout.close()
      shutil.move(h+'_new', h)

    # Move back to the original directory 
    os.chdir(home)

    # Debug
    print('Moved back to directory: '+os.getcwd())
    
    # Now, create json configuration file
    # new json's name
    comb_json_name = comb_name + '.json'
   
    # copy over ipc_baseline json to the new json file
    shutil.copy2(home + json_config_file, home + '/' + comb_json_name)

    # the info replacing XXX & YYY in configuration file
    new_info = [comb_name, comb_dir_name]
    
    # Open up configuration json file and substitute two lines
    for s in range(2):
      fin = open(comb_json_name, 'r') 
      fout = open(comb_name + '_new', 'w')
      for line in fin:
        fout.write(line.replace(str_subs[s], new_info[s]))
      fin.close()
      fout.close()
      shutil.move(comb_name + '_new', comb_json_name)
    
    # The absolute final step 
    curr_combination = curr_combination + 1

  print('-------------------------------------------------------------')
