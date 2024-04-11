all: eviction1 eviction2 mem_evi L3_access_time

eviction1: eviction1.c cacheutils.h
	gcc -std=gnu11 -O2 -o $@ eviction1.c 

eviction2: eviction2.c cacheutils.h
	gcc -std=gnu11 -O2 -o $@ $@.c

mem_evi: mem_evi.c memory-utils.c help.c
	gcc -std=gnu11 -O0 -o $@ $@.c

L3_access_time: L3_access_time.c memory-utils.c help.c
	gcc -std=gnu11 -O3 -o $@ $@.c

clean: 
	rm -rf mem_evi L3_access_time


