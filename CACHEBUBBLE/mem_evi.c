#define _GNU_SOURCE
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
#include <sched.h>
#include <inttypes.h>
#include "memory-utils.c"
#include "help.c"
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

#define BILLION 1E9
#define HUGEPAGE_NUM 1
#define NUM_WAYS 11
#define NUM_SLICES 8

#define NUM_SETS 2048 //max pollute size

int bits_s_1[2]={21,29};
int bits_s_0[2]={25,18};
int bits_s_01[4]={17,20,24,28};
int bits_s_02[1]={27};
int bits_s_12[2]={19,23};
int bits_s_012[2]={22,26};

//int stride=2048/NUM_SETS;

unsigned long toggle_n_bit(unsigned long number, int n)
{
	number ^= 1UL << n;
	return number;
}

//address: the original address... number_access: number of accesses this function will generate...
//set: the set to hold the generated addresses...
void gen_accesses_same_slice(unsigned long address,int num_access,unsigned long *set)
{
	unsigned long gen_address;
        int i=0,j=0,k=0;
        int counter=0;
	set[counter]=address;
        for (i=0; i<2; i++) {
		for (j=0;j<2;j++){
			for(k=0;k<4;k++){
				counter++;
                                if (counter>=num_access)
                                        return;
				gen_address=toggle_n_bit(address,bits_s_1[i]);
				gen_address=toggle_n_bit(gen_address,bits_s_0[j]);
				gen_address=toggle_n_bit(gen_address,bits_s_01[k]);
				set[counter]=gen_address;
			}
		//break;
		}
	//break;
	}
	return;	
}

//this function generates addresses that will access cache sets in different slices...
//set: the set to hold the generated addresses...
//suppose the original set slice(accessed by address) is represented by s_210
void gen_accesses_different_slices(unsigned long address,unsigned long *set)
{
	unsigned long gen_address;
	int counter =0;
	set[counter]=address;
	//generates address to access s_21not0
	//gen_address=address;
        gen_address=toggle_n_bit(address,26);
	gen_address=toggle_n_bit(gen_address,23);
	counter++;
        set[counter]=gen_address;

	//generate address to access s_2not10
	gen_address=toggle_n_bit(address,26);
        gen_address=toggle_n_bit(gen_address,27);
        counter++;
        set[counter]=gen_address;

        //generate address to access s_not210
	gen_address=toggle_n_bit(address,26);
	gen_address=toggle_n_bit(gen_address,27);
        gen_address=toggle_n_bit(gen_address,23);
	counter++;
	set[counter]=gen_address;

	//generate address to access s2not1not0
	gen_address=toggle_n_bit(address,23);
        gen_address=toggle_n_bit(gen_address,27);
	counter++;
	set[counter]=gen_address;

	//generate address to access snot21not0
        gen_address=toggle_n_bit(address,27);
        counter++;
        set[counter]=gen_address;

	//generate address to access snot2not10
        gen_address=toggle_n_bit(address,23);
	counter++;
        set[counter]=gen_address;

	//generate address to access snot2not1not0
        gen_address=toggle_n_bit(address,26);
	counter++;
	set[counter]=gen_address;
	return;
}

int main(int argc, char *argv[])
{

	int pid=0,ret,i=0;
        char* p;
	//size_t size = 80*1024*1024,;
	struct timespec start_loop,end_loop;
 	unsigned long long int nop_duration=0,loop_index=0;
	//void* mymemory;
	unsigned long address;
  //unsigned long phy_addr,set;
  //char path_buf [0x100] = {};
  //void *elements[30];
	//int cache_index=0;
	unsigned long set_index=110;
	unsigned long sets[64]={30,246,910,480,705,1113,1348,1790,2007,1503};
	unsigned long accesses_diff_slices[NUM_SETS][NUM_SLICES]={0};
	// access_same_slice[set_index][slice_index][way_index]
	unsigned long accesses_same_slice[NUM_SETS][NUM_SLICES][NUM_WAYS]={0};
  /* Pin to core 0 */
	int mask = 3;
	int slice_index=0;
	cpu_set_t my_set;
	CPU_ZERO(&my_set);
	CPU_SET(mask, &my_set);
	sched_setaffinity(0, sizeof(cpu_set_t), &my_set);
  
  int way_pollu=1;
  int num_mon_set=64;
  int mon_set0=13;

  int same_set_size=6;
  if(argc!=5){
    printf("usage: ./mem_evi number_of_way_topollute num_mon_set mon_set0 same_set_size\n");
    return 0;
  }
  way_pollu=atoi(argv[1]);
  num_mon_set=atoi(argv[2]);
  mon_set0=atoi(argv[3]);
  same_set_size=atoi(argv[4]);
  int tmp = num_mon_set -1;
  int stride =0;
  int _count = 0;
  while( tmp != 0){ 
    tmp >>= 1; 
    _count += 1; 
  } 

   stride = 2048 / (1 << _count);

/* this is the function to allocate buffer
  srand(time(NULL));
  ret = posix_memalign(&mymemory, 2*1024*1024, size);
  if (ret!=0) {
         printf("\n posix_memalign error");
  }
  madvise(mymemory, size, MADV_HUGEPAGE);
  for (index=0;index<size;index++)
      *((char *)mymemory+index)=rand()%100;

  pid=getpid();
*/
	/* Create a buffer with some data in it */
	void **bufferList=malloc(HUGEPAGE_NUM*sizeof(*bufferList));
	int k=0;
	for(k=0;k<HUGEPAGE_NUM;k++) {
		bufferList[k] = create_buffer();
	}

	address=(unsigned long)bufferList[0];
   	for(i=0;i<num_mon_set;i++)
	{
    address=(address & 0xfffffffffffe0000) +((i * stride + mon_set0)<<6);
		//printf("set is : %d ", i);
		gen_accesses_different_slices(address,accesses_diff_slices[i]);
		for (k=0;k<NUM_SLICES;k++)
		{
			address=accesses_diff_slices[i][k];
			gen_accesses_same_slice(address,NUM_WAYS,accesses_same_slice[i][k]);
		}

		//for(i=0;i<NUM_SLICES;i++)
		//{
		//printf("the slice address is: %lx \n", accesses_diff_slices[i]);
		//	for (k=0;k<NUM_WAYS;k++)
		//	{
                //printaccesses_diff_slices[k];
                //	printf("a:%lx  ",accesses_same_slice[i][k]);
                //		;
		//	}
		//printf("\n");
        	//}
	}

  //eviction
	int loops=0;
	long long int time_diff=0;
 //old code to access addresses  
	//int  _count=0;
  if(way_pollu==-1){
    //clock_gettime(CLOCK_MONOTONIC,&start_loop);
	  for (time_diff=0;time_diff<6000*BILLION;){
    for (loops=0;loops<20;loops++)
	  {
  
	    for(set_index=0;set_index<num_mon_set;set_index+=1){
	      for (slice_index=0;slice_index<NUM_SLICES;slice_index+=1){
		      for (k=0;k<(set_index %11  +1);k++) {
            p=(char* )accesses_same_slice[set_index][slice_index][k];
				    maccess(p);
            //printf("set index: %d. address is: %d.\n",set_index,k);
		    }
      }
    }
	  }
  //clock_gettime(CLOCK_MONOTONIC,&end_loop);
	//time_diff = (end_loop.tv_nsec - start_loop.tv_nsec)+ (end_loop.tv_sec-start_loop.tv_sec)* BILLION;
	  } 
  }  
  else{
     //clock_gettime(CLOCK_MONOTONIC,&start_loop);
	  for (time_diff=0;time_diff<6000*BILLION;){
    for (loops=0;loops<2000;loops++)
	  {
  
	    for(set_index=0;set_index<num_mon_set;set_index+=1){
	      for (slice_index=0;slice_index<NUM_SLICES;slice_index+=1){
		      for (k=0;k<way_pollu;k++) {
            p=(char* )accesses_same_slice[set_index][slice_index][k];
				    maccess(p); 
		    }
      }
    }
	  }
    //clock_gettime(CLOCK_MONOTONIC,&end_loop);
	  //time_diff = (end_loop.tv_nsec - start_loop.tv_nsec)+ (end_loop.tv_sec-start_loop.tv_sec)* BILLION;
	  } 
 
  } 

/*	
	for(k=0;k<HUGEPAGE_NUM;k++) {
		// Get physical address of the buffer 
		//uint64_t physical_address=get_physical_address(bufferList[k]);

		// Iterate through the 1GB-page 
		unsigned long buffer=(unsigned long)bufferList[k];
		uint64_t offset=1024*1024;
		uint64_t iteration=0;
		uint8_t slice_number=0;
		char bits[5+1];
		while(iteration < 10) {
			uint64_t physical_address=read_pagemap("/proc/self/pagemap",(unsigned long) buffer);
			// Print Physical Address 
			printf("virtual address: %lx ; phsical address:%lx\n",buffer,physical_address);

			// Print the slice number 
			//printf("%d\t",slice_number);
			//printf("%s\n",convert(slice_number,bits,5));
			buffer+=offset;
                        iteration=iteration+1;
		}
	}
*/

        for(k=0;k<HUGEPAGE_NUM;k++) {
              printf("free hugepage\n");
                free_buffer(bufferList[k]);
        }
	return 0;



}
