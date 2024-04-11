/* 
 * This program will find cycles required to access a specific slice from one core
 *
 * Copyright (c) 2019, Alireza Farshin, KTH Royal Institute of Technology - All Rights Reserved
 */

#include "memory-utils.c"
#include "cache-utils.c"
#include <sched.h>
#include <inttypes.h>
#include <stdlib.h>

#define NUMBER_CORES 8
#define READ_TIMES 1
#define NUM_WAYS 11
#define NUM_SLICES 8
#define NUM_SETS 10


/* 
 *  * Function for XOR-ing all bits
 *   * ma: masked physical address
 *    
 */

#define bitNum 3
/* Bit0 hash */
#define hash_0 0x1B5F575440
/* Bit1 hash */
#define hash_1 0x2EB5FAA880
/* Bit2 hash */
#define hash_2 0x3CCCC93100


//uint64_t
//rte_xorall64(uint64_t ma) {
int bits_s_1[2]={21,29};
int bits_s_0[2]={25,18};
int bits_s_01[4]={17,20,24,28};
int bits_s_02[1]={27};
int bits_s_12[2]={19,23};
int bits_s_012[2]={22,26};



unsigned long toggle_n_bit(unsigned long number, int n)
{
        number ^= 1UL << n;
        return number;
}

       

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
                        break;
                }
                break;
        }
        return;
}
 /*
 * Pin program to the input core
 */

void CorePin(int coreID)
{
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(coreID,&set);
	if(sched_setaffinity(0, sizeof(cpu_set_t), &set) < 0) {
		printf("\nUnable to Set Affinity\n");
		exit(EXIT_FAILURE);
	}
}


int main(int argc, char **argv) {

	/*
	 * Check arguments: should contain coreID and slice number
	 */

        unsigned long sets[10]={30,246,910,480,705,1113,1348,1790,2007,1503};
        unsigned long accesses_diff_slices[10][8]={0};
        //access_same_slice[set_index][slice_index][way_index]
        unsigned long accesses_same_slice[10][8][11]={0};
         


	if(argc!=3){
		printf("Wrong Input! Enter desired slice and coreID!\n");
		printf("Enter: %s <coreID> <sliceNumber>\n", argv[0]);
		exit(1);
	}

	int coreID;
	sscanf (argv[1],"%d",&coreID);
	if(coreID > NUMBER_CORES*2-1 || coreID < 0){
		printf("Wrong Core! CoreID should be less than %d and more than 0!\n", NUMBER_CORES);
		exit(1);   
	}

	int desiredSlice;
	sscanf (argv[2],"%d",&desiredSlice);
	if(desiredSlice > NUMBER_SLICES-1 || desiredSlice < 0){
		printf("Wrong slice! Slice should be less than %d and more than 0!\n", NUMBER_SLICES);
		exit(1);   
	}

	/* 
	 * Ping program to core-0 for finding chunks
	 * Later the program will be pinned to the desired coreID
	 */
	CorePin(0);

	/* Get a 1GB-hugepage */
	void *buffer = create_buffer();

	/* Calculate the physical address of the buffer */
	uint64_t bufPhyAddr = read_pagemap("/proc/self/pagemap",(unsigned long) buffer);


	/* Physical Address of chunks */

	unsigned long long  i=0;
	int j=0,k=0;

	/* Find first chunk */
	//unsigned long long offset = sliceFinder_uncore(buffer,desiredSlice);

	//totalChunks[0]=buffer+offset;
	//totalChunksPhysical[0]= bufPhyAddr+offset;

	/* Find the Indexes (Set number in cache hierarychy) */
	//index3=indexCalculator(totalChunksPhysical[0],3);
	//index2=indexCalculator(totalChunksPhysical[0],2);
	//index1=indexCalculator(totalChunksPhysical[0],1);
	for(i=0;i<10;i++)
        {

                address=address+(sets[i]<<6);
		//address2=toggle_n_bit(address,16);
                printf("address is : %lx", address);
         	gen_accesses_different_slices(address,accesses_diff_slices[i]);
		for (k=0;k<NUM_SLICES;k++)
        	{
        	address=accesses_diff_slices[i][k];
        	gen_accesses_same_slice(address,11,accesses_same_slice[i][k]);
                }
	}
        
	/* Ping program to coreID */
    	CorePin(coreID);

	for(k=0;k<READ_TIMES;k++) {
		/* Fill Arrays */
		//for(i=0; i<;i++) {
		//	for(j=0;j<64;j++) {
		//		[j]=10+20;
		//	}
		//}

		/* Flush Array */
		for(i=0; i<10;i++) {
			for(j=0;j<NUM_SLICES;j++) {
				for(k=0;k<8;k++){
					_mm_clflush(accesses_same_slice[i][j][k]);
				}
			}
		}


		for (slice_index=0;slice_index<NUM_SLICES;slice_index++){
                	for (k=0;k<8;k++) {
				p=(char* )accesses_same_slice[0][slice_index][k];
				size_t time = rdtsc();
                       		maccess(p);
                        	size_t delta = rdtsc() - time;
				printf("mem: %u cycles \n",delta);
			}
		}

		

		for (slice_index=0;slice_index<NUM_SLICES;slice_index++){
                        for (k=0;k<8;k++) {
                                p=(char* )accesses_same_slice[0][slice_index][k];
                                size_t time = rdtsc();
                                maccess(p);
                                size_t delta = rdtsc() - time;
                                printf("llc: %u cycles \n",delta);
                        }
                }



	}

	/* Free the buffers */
	free_buffer(buffer);
	free(totalChunks);
	free(totalChunksPhysical);

	return 0;
}
