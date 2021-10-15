# create_hybrids
Infrastructure to create composite prefetchers

Basically all you need to do is 

module load python3 for terra 

and then 

python3 create_hybrids.py

It's super well commented so you can get a good idea. Add prefetchers and supporting .cc files (e.g. ppf.cc etc.) into 
./infrastructure/prefetchers and then add the prefetcher name to that first list of create_hybrids.py 

Add your hybrid files into ./infrastructure/complete_hybrids. You need to make a few changes to it, mainly removing the #includes and instead keeping one #include XXX, for each prefetcher. 
