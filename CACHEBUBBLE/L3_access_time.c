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
#define NUM_SETS 10

/* Number of hash functions/Bits for slices */
#define bitNum 3
/* Bit0 hash */
#define hash_0 0x1B5F575440
/* Bit1 hash */
#define hash_1 0x2EB5FAA880
/* Bit2 hash */
#define hash_2 0x3CCCC93100

#define L3_INDEX_PER_SLICE 0x1FFC0 /* 11 bits - [16-6] - 2048 sets per slice + 11 way for each slice (1.375MB) */
#define L2_INDEX 0xFFC0 /* 10 bits - [15-6] - 1024 sets + 16 way for each core  */
#define L1_INDEX 0xFC0 /* 6 bits - [11-6] - 64 sets + 8 way for each core  */



int bits_s_1[2]={21,29};
int bits_s_0[2]={25,18};
int bits_s_01[4]={17,20,24,28};
int bits_s_02[1]={27};
int bits_s_12[2]={19,23};
int bits_s_012[2]={22,26};




/* 
 *  * Function for XOR-ing all bits
 *   * ma: masked physical address
 */
uint64_t rte_xorall64(uint64_t ma) {
	return __builtin_parityll(ma);
}

/* Calculate slice based on the physical address - Haswell */

uint8_t calculateSlice(uint64_t pa) {
	uint8_t sliceNum=0;
	sliceNum= (sliceNum << 1) | (rte_xorall64(pa&hash_2));
	sliceNum= (sliceNum << 1) | (rte_xorall64(pa&hash_1));
	sliceNum= (sliceNum << 1) | (rte_xorall64(pa&hash_0));
	return sliceNum;
}



/* Calculate the index of set for a given physical address */

uint64_t calculateSet(uint64_t addr_in, int cacheLevel) {

	uint64_t index;

	if (cacheLevel==1)
	{
		index=(addr_in)&L1_INDEX;
		index = index >> 6;
	}
	else if (cacheLevel==2)
	{
		index=(addr_in)&L2_INDEX;
		index = index >> 6;
	}
	else if (cacheLevel==3)
	{
		index=(addr_in)&L3_INDEX_PER_SLICE;
		index = index >> 6;
	}
	else
	{
		exit(EXIT_FAILURE);
	}
	return index;
}


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

	int pid=0,ret=0,i=0,j=0;
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
	unsigned long sets[10]={30,246,910,480,705,1113,1348,1790,2007,1503};
	unsigned long accesses_diff_slices[10][8]={0};
	// access_same_slice[set_index][slice_index][way_index]
	unsigned long accesses_same_slice[10][8][11]={0};
  /* Pin to core 0 */
	int mask = 4;
	int slice_index=0;
	cpu_set_t my_set;
	CPU_ZERO(&my_set);
	CPU_SET(mask, &my_set);
	sched_setaffinity(0, sizeof(cpu_set_t), &my_set);

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
   	for(i=0;i<10;i++)
	{
		
		address=address+(sets[i]<<6);
	//printf("address is : %lx", address);
		gen_accesses_different_slices(address,accesses_diff_slices[i]);
		for (k=0;k<NUM_SLICES;k++)
		{
			address=accesses_diff_slices[i][k];
			gen_accesses_same_slice(address,11,accesses_same_slice[i][k]);
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
	//int loops=0;
	//long time_diff=0;
 //old code to access addresses  
	//clock_gettime(CLOCK_MONOTONIC,&start_loop);
	//for (time_diff=0;time_diff<0.1*BILLION;)
	//{
	//clock_gettime(CLOCK_MONOTONIC,&start_loop);
	//for (loops=0;loops<20;loops++)
	//{
	//
	//
	




                /* Flush Array */
                for(i=0; i<1;i++) {
                        for(j=0;j<NUM_SLICES;j++) {
                                for(k=0;k<8;k++){
					unsigned long physical_address=read_pagemap("/proc/self/pagemap",(unsigned long) accesses_same_slice[i][j][k]);
					uint8_t slice=calculateSlice( (uint64_t) physical_address);
					uint64_t l2_set=calculateSet((uint64_t) physical_address, 2);
					uint64_t l3_set=calculateSet((uint64_t) physical_address, 3); 

					printf("va: %ld, pa: %ld, slice: %d, l2set: %ld, l3set: %ld  \n",accesses_same_slice[i][j][k], physical_address, slice, l2_set, l3_set);
                                        flush((char*) accesses_same_slice[i][j][k]);
                                }
                        }
                }


                for (slice_index=0;slice_index<NUM_SLICES;slice_index++){
                        for (k=0;k<5;k++) {
                                size_t time = rdtsc();
                                maccess((char* )accesses_same_slice[0][slice_index][k]);
                                size_t delta = rdtsc() - time;
                                printf("mem: %u cycles \n",delta);
                        }
                }
                for(i=0;i<10;i++){
                for (slice_index=0;slice_index<NUM_SLICES;slice_index++){
                        for (k=0;k<5;k++) {
                                //p=(char* )accesses_same_slice[0][slice_index][k];
                                maccess((char* )accesses_same_slice[0][slice_index][k]);
                        }
                }
		}

                for (slice_index=0;slice_index<NUM_SLICES;slice_index++){
                        for (k=0;k<5;k++) {
                                p=(char* )accesses_same_slice[0][slice_index][k];
                                size_t time = rdtsc();
                                maccess(p);
                                size_t delta = rdtsc() - time;
                                printf("llc: %u cycles \n",delta);
                                maccess(p);
				maccess(p);
				time = rdtsc();
                                maccess(p);
                                delta = rdtsc() - time;
                                printf("l1: %u cycles \n",delta);


                        }
                }

                for (slice_index=0;slice_index<3;slice_index++){
                        for (k=0;k<3;k++) {
                                p=(char* )accesses_same_slice[3][slice_index][k];
                                size_t time = rdtsc();
                                maccess(p);
                                size_t delta = rdtsc() - time;
                                printf("llc: %u cycles \n",delta);
                        }
                }

                for (slice_index=0;slice_index<3;slice_index++){
                        for (k=0;k<3;k++) {
                                p=(char* )accesses_same_slice[3][slice_index][k];
                                size_t time = rdtsc();
                                maccess(p);
                                size_t delta = rdtsc() - time;
                                printf("l2: %u cycles \n",delta);
                        }
                }


	//}
	//clock_gettime(CLOCK_MONOTONIC,&end_loop);
	//time_diff = (end_loop.tv_nsec - start_loop.tv_nsec)+ (end_loop.tv_sec-start_loop.tv_sec ) * BILLION;
	//}
	//clock_gettime(CLOCK_MONOTONIC,&end_nop);
  //return 0; 
  

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
                free_buffer(bufferList[k]);
        }
	return 0;



}
