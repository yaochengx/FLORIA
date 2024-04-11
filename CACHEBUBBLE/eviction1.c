#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
//#include "help.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include "cacheutils.h"
//number of sets: 64  associativity:8 line size:64
#define L1d_size pow(2,15)
//number of sets: 512  associativity:8 line size:64
#define L2_size pow(2,18)
//number of sets: 8192  associativity:12 line size:64
#define L3_size 6*pow(2,20)

//#define N 6*8192
#define N 4096/64
#define US 1000
#define MS 1000000
#define NS 1 
#define MEMBLOCKS 1
#define ONE asm("nop");
#define FIVE ONE ONE ONE ONE ONE
#define TEN FIVE FIVE
#define HUND TEN TEN TEN TEN TEN TEN TEN TEN TEN TEN
#define THOD HUND HUND HUND HUND HUND HUND HUND HUND HUND HUND



int main(int argc, char *argv[])
{

  int pid=0,ret,same_set;
  size_t size = 80*1024*1024,index=0;
  struct timespec start_nop,end_nop;
  unsigned long long int nop_duration=0,loop_index=0;
  void* mymemory;
  char* address;
  //unsigned long phy_addr,set;
  //char path_buf [0x100] = {};
  //void *elements[30];
  int cache_index=0;
  srand(time(NULL));
  ret = posix_memalign(&mymemory, 2*1024*1024, size);
  if (ret!=0) {
         printf("\n posix_memalign error");
  }
  madvise(mymemory, size, MADV_HUGEPAGE);
  for (index=0;index<size;index++)
      *((char *)mymemory+index)=rand()%100;

  pid=getpid();
  //eviction
   
  clock_gettime(CLOCK_MONOTONIC,&start_nop);
  for (loop_index=0;loop_index<1000000;loop_index++){
       for (same_set=0;same_set<11*10*2;same_set++) {
         for (cache_index=100;cache_index<101;cache_index++) {
              address=(char *)(mymemory+0x10000*2*same_set+0x40*cache_index);
              size_t time = rdtsc();
              maccess(address);
	      size_t delta = rdtsc() - time;
              // delta, miss ot hit?
              //printf(" %u ns ",delta);
             }   
       }      
  }
  clock_gettime(CLOCK_MONOTONIC,&end_nop);
  return 0; 
}
