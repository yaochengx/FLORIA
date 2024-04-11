#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "help.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
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

//use an array to store the result and print them at once
int main(int argc, char *argv[])
{

  int sum=0,pid=0,ret,same_set,mb_index;
  size_t size = 2*1024*1024,index=0;
  struct timespec start_nop,end_nop;
  unsigned long long int nop_duration=0,loop_index=0;
  void *mymemory[MEMBLOCKS];
  unsigned long phy_addr,set;
  char path_buf [0x100] = {};
  void *elements[30];
  int cache_index=0;
  //int set_max=400;
  //int set_min=300;
  srand(time(NULL));
  for (mb_index=0;mb_index<MEMBLOCKS;mb_index++) {
     ret = posix_memalign(&mymemory[mb_index], 2*1024*1024, size);
     if (ret!=0) {
         printf("\n posix_memalign error");
     }
     madvise(mymemory[mb_index], size, MADV_HUGEPAGE);
     for (index=0;index<size;index++)
         *((char *)mymemory[mb_index]+index)=rand()%100;
  
  }

  //pid=getpid();
  //eviction
   
  clock_gettime(CLOCK_MONOTONIC,&start_nop);
                             // same_set:8 loop_index:300000
  for (loop_index=0;loop_index<300000;loop_index++){
                     
      usleep(1);
      //sum=0;
      for (mb_index=0;mb_index<MEMBLOCKS;mb_index++) {
       for (same_set=0;same_set<2;same_set++) {
         for (cache_index=0;cache_index<500;cache_index=cache_index+2) {
              *((char *)mymemory[mb_index]+0x10000*2*same_set+0x40*(cache_index+2)) = *((char *)mymemory[mb_index]+0x10000*2*same_set+0x40*cache_index)+ *((char *)mymemory[mb_index]+0x10000*2*same_set+0x40*(cache_index+1));  
             }
         //printf("sum of a row is: %d.\n",sum);   
       }
      }
  }
  clock_gettime(CLOCK_MONOTONIC,&end_nop);
  nop_duration = (end_nop.tv_sec - start_nop.tv_sec) * 1e9 + (end_nop.tv_nsec - start_nop.tv_nsec);
  

  
  /*     
  sprintf(path_buf, "/proc/%u/pagemap", pid);
   for (mb_index=0;mb_index<MEMBLOCKS;mb_index++) {
       for (same_set=0;same_set<8;same_set++) {
           for (cache_index=0;cache_index<500;cache_index++) {
                elements[mb_index]=mymemory[mb_index]+0x10000*2*same_set+0x40*cache_index;
                phy_addr=read_pagemap(path_buf, (long unsigned ) elements[mb_index]);
                set = (phy_addr>>6)&0x7ff;
                //printf("\n array %d: index: %d: vir address: %llx, phy address: %llx, set: %ld \n, content: %d\n",mb_index,same_set,(long long) elements[mb_index],(long long) phy_addr,set,*((char *)elements[mb_index]));
           }

        }
   } 
   */ 
   //for (loop_index=0;loop_index<1500;loop_index++){
      //sum=0;
      for (mb_index=0;mb_index<MEMBLOCKS;mb_index++) {
       for (same_set=0;same_set<8;same_set++) {
         for (cache_index=0;cache_index<500;cache_index=cache_index+10) {
              printf("ele is: %d\n",*((char *)mymemory[mb_index]+0x10000*2*same_set+0x40*cache_index));  
             }
         //printf("sum of a row is: %d.\n",sum);   
       }
      }
  //}  
  for (mb_index=0;mb_index<MEMBLOCKS;mb_index++) {
     free(mymemory[mb_index]);
  }
  printf("\neviction: %llu ns\n",nop_duration);

  return 0; 
}
