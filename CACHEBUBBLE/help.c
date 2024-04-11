#include "help.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <assert.h>
#include <stdint.h>
#include <errno.h>

#include <signal.h>

#include <sys/mman.h>
#include <linux/perf_event.h>
#include <syscall.h>
#include "mb.h"


//***************** looking for phys address **************************************//
#define PAGEMAP_ENTRY 8
#define GET_BIT(X,Y) (X & ((unsigned long)1<<Y)) >> Y
#define GET_PFN(X) X & 0x7FFFFFFFFFFFFF

const int __endian_bit = 1;
#define is_bigendian() ( (*(char*)&__endian_bit) == 0 )
// get set index: (addr>>6)&0x7ff
//phy_addr=read_pagemap("/proc/self/pagemap", data_addr)
//return 0 if failed.....need to check
unsigned long read_pagemap(char *path_buf, unsigned long virt_addr){

   int i, c, status;
   unsigned long read_val, file_offset;
   FILE * f;
  
   
   f = fopen(path_buf, "rb");
   if(!f){
      printf("Error! Cannot open %s\n", path_buf);
      return 0;
   }

   file_offset = virt_addr / getpagesize() * PAGEMAP_ENTRY;
   //printf("Vaddr: 0x%lx, Page_size: %d, Entry_size: %d\n", virt_addr, getpagesize(), PAGEMAP_ENTRY);
   //printf("Reading %s at 0x%llx\n", path_buf, (unsigned long long) file_offset);
   status = fseek(f, file_offset, SEEK_SET);
   if(status){
      perror("Failed to do fseek!");
      return 0;
   }
   errno = 0;
   read_val = 0;
   unsigned char c_buf[PAGEMAP_ENTRY];
   for(i=0; i < PAGEMAP_ENTRY; i++){
      c = getc(f);
      if(c==EOF){
        // printf("\nReached end of the file\n");
         fclose(f);
         return 0;
      }
      if(is_bigendian())
           c_buf[i] = c;
      else
           c_buf[PAGEMAP_ENTRY - i - 1] = c;
      //printf("[%d]0x%x ", i, c);
   }
   for(i=0; i < PAGEMAP_ENTRY; i++){
      //printf("%d ",c_buf[i]);
      read_val = (read_val << 8) + c_buf[i];
   }
   //printf("\n");
   unsigned long phys = (GET_PFN(read_val))<<12;
   phys = phys | (virt_addr % getpagesize());
   //printf("Result: 0x%llx physical address: 0x%llx virtual Address: 0x%llx \n", (unsigned long long) read_val, (unsigned long long)phys, (unsigned long long)virt_addr);
   //if(GET_BIT(read_val, 63))
#if 0
   if(GET_BIT(read_val, 63))
      printf("PFN: 0x%llx\n",(unsigned long long) GET_PFN(read_val));
   else
      printf("Page not present\n");
   if(GET_BIT(read_val, 62))
      printf("Page swapped\n");
#endif
   fclose(f);
   return phys;
}


int handle_struct_read_format(unsigned char *sample,int read_format,long long timestamp) {

	int offset=0,i;
	static long long values[10];

	if (read_format & PERF_FORMAT_GROUP) {
		long long nr,time_enabled=0,time_running=0;

		memcpy(&nr,&sample[offset],sizeof(long long));
		offset+=8;

		if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) {
			memcpy(&time_enabled,&sample[offset],sizeof(long long));
			offset+=8;
		}
		if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) {
			memcpy(&time_running,&sample[offset],sizeof(long long));
			offset+=8;
		}

		if ((read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) &&
			(read_format & PERF_FORMAT_TOTAL_TIME_ENABLED)) {
			if (time_running!=time_enabled) {
				printf("ERROR! MULTIPLEXING\n");
			}
		}

		for(i=0;i<nr;i++) {
			long long value, id=0;

			memcpy(&value,&sample[offset],sizeof(long long));
			offset+=8;

			if (read_format & PERF_FORMAT_ID) {
				memcpy(&id,&sample[offset],sizeof(long long));
				offset+=8;
			}
			printf("%lld %lld (* %lld *)\n",timestamp,value-values[i],id);
			values[i]=value;

		}
	}
	else {

		long long value,time_enabled,time_running,id;

		memcpy(&value,&sample[offset],sizeof(long long));
		printf("\t\tValue: %lld ",value);
		offset+=8;

		if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) {
			memcpy(&time_enabled,&sample[offset],sizeof(long long));
			printf("enabled: %lld ",time_enabled);
			offset+=8;
		}
		if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) {
			memcpy(&time_running,&sample[offset],sizeof(long long));
			printf("running: %lld ",time_running);
			offset+=8;
		}
		if (read_format & PERF_FORMAT_ID) {
			memcpy(&id,&sample[offset],sizeof(long long));
			printf("id: %lld ",id);
			offset+=8;
		}
		printf("\n");
	}

	return offset;
}


long long perf_mmap_read( void *our_mmap, int mmap_size, long long prev_head, int sample_type, int read_format, FILE *sample, char *path_buf, FILE *pagemap) {

	struct perf_event_mmap_page *control_page = our_mmap;
	long long head,offset;
	int size;
	long long bytesize,prev_head_wrap;
	static long long begin_time;
                     //int myid;
	unsigned char *data;
                     static long long int index=0;
	void *data_mmap=our_mmap+getpagesize();
                     //printf("data_mmap: %p \n",data_mmap);
        int hit=-1;
	begin_time=0;
                     struct timespec curr_time;
                     clock_gettime(CLOCK_REALTIME, &curr_time);
                     fprintf(sample,"time:%lld index:%lld \n",(long long int)curr_time.tv_sec,index);
                     index++;
	if (mmap_size==0) return 0;

	if (control_page==NULL) {
		fprintf(stderr,"ERROR mmap page NULL\n");
		return -1;
	}
                     // Jun Xiao: head starts from 0 and it is increasing.
	head=control_page->data_head;
	rmb(); /* Must always follow read of data_head */

	size=head-prev_head;

	//printf("Head: %lld Prev_head=%lld\n",head,prev_head);
	//printf("%d new bytes\n",size);

	bytesize=mmap_size*getpagesize();

	if (size>bytesize) {
		printf("error!  we overflowed the mmap buffer %d>%lld bytes\n",
			size,bytesize);
	}

	data=malloc(bytesize);
	if (data==NULL) {
		return -1;
	}

	prev_head_wrap=prev_head%bytesize;

	//printf("Copying %lld bytes from %lld to %d\n",
	//	  bytesize-prev_head_wrap,prev_head_wrap,0);
	memcpy(data,(unsigned char*)data_mmap + prev_head_wrap,
		bytesize-prev_head_wrap);

	//printf("Copying %lld bytes from %d to %lld\n",
	//	  prev_head_wrap,0,bytesize-prev_head_wrap);

	memcpy(data+(bytesize-prev_head_wrap),(unsigned char *)data_mmap,
		prev_head_wrap);

	struct perf_event_header *event;


	offset=0;

	while(offset<size) {

		//printf("Offset %d Size %d\n",offset,size);
		event = ( struct perf_event_header * ) & data[offset];

		/********************/
		/* Print event Type */
		/********************/
                                          /*
		switch(event->type) {
				case PERF_RECORD_MMAP:
					printf("PERF_RECORD_MMAP"); break;
				case PERF_RECORD_LOST:
					printf("PERF_RECORD_LOST"); break;
				case PERF_RECORD_COMM:
					printf("PERF_RECORD_COMM"); break;
				case PERF_RECORD_EXIT:
					printf("PERF_RECORD_EXIT"); break;
				case PERF_RECORD_THROTTLE:
					printf("PERF_RECORD_THROTTLE"); break;
				case PERF_RECORD_UNTHROTTLE:
					printf("PERF_RECORD_UNTHROTTLE"); break;
				case PERF_RECORD_FORK:
					printf("PERF_RECORD_FORK"); break;
				case PERF_RECORD_READ:
					printf("PERF_RECORD_READ");
					break;
				case PERF_RECORD_SAMPLE:
					//printf("PERF_RECORD_SAMPLE [%x]",sample_type);
					break;
				case PERF_RECORD_MMAP2:
					printf("PERF_RECORD_MMAP2"); break;
				default: printf("UNKNOWN %d",event->type); break;
			}
*/


			/* Both have the same value */
			if (event->misc & PERF_RECORD_MISC_MMAP_DATA) {
				//printf(",PERF_RECORD_MISC_MMAP_DATA or PERF_RECORD_MISC_COMM_EXEC ");
			}

			if (event->misc & PERF_RECORD_MISC_EXACT_IP) {
				//printf(",PERF_RECORD_MISC_EXACT_IP ");
			}

			if (event->misc & PERF_RECORD_MISC_EXT_RESERVED) {
				//printf(",PERF_RECORD_MISC_EXT_RESERVED ");
			}

		offset+=8; /* skip header */

		/***********************/
		/* Print event Details */
		/***********************/

		switch(event->type) {

		/* Lost */
		case PERF_RECORD_LOST: {
			long long id,lost;
			memcpy(&id,&data[offset],sizeof(long long));
			//printf("\tID: %lld\n",id);
			offset+=8;
			memcpy(&lost,&data[offset],sizeof(long long));
			//printf("\tLOST: %lld\n",lost);
			offset+=8;
			}
			break;

		/* COMM */
		case PERF_RECORD_COMM: {
			int pid,tid,string_size;
			char *string;

			memcpy(&pid,&data[offset],sizeof(int));
			//printf("\tPID: %d\n",pid);
			offset+=4;
			memcpy(&tid,&data[offset],sizeof(int));
			//printf("\tTID: %d\n",tid);
			offset+=4;

			/* FIXME: sample_id handling? */

			/* two ints plus the 64-bit header */
			string_size=event->size-16;
			string=calloc(string_size,sizeof(char));
			memcpy(string,&data[offset],string_size);
			//printf("\tcomm: %s\n",string);
			offset+=string_size;
			if (string) free(string);
			}
			break;

		/* Fork */
		case PERF_RECORD_FORK: {
			}
			break;

		/* mmap */
		case PERF_RECORD_MMAP: {
			}
			break;

		/* mmap2 */
		case PERF_RECORD_MMAP2: {
			}
			break;

		/* Exit */
		case PERF_RECORD_EXIT: {
			}
			break;

		/* Throttle/Unthrottle */
		case PERF_RECORD_THROTTLE:
		case PERF_RECORD_UNTHROTTLE: {
			long long throttle_time,id,stream_id;

			memcpy(&throttle_time,&data[offset],sizeof(long long));
			//printf("\tTime: %lld\n",throttle_time);
			offset+=8;
			memcpy(&id,&data[offset],sizeof(long long));
			//printf("\tID: %lld\n",id);
			offset+=8;
			memcpy(&stream_id,&data[offset],sizeof(long long));
			//printf("\tStream ID: %lld\n",stream_id);
			offset+=8;

			}
			break;

		/* Sample */
		case PERF_RECORD_SAMPLE: {
                                                               long long time=0;
                                                               long long ip;
                                                               int pid, tid;
                                                               long long addr,phy_addr=0;
			//char path_buf [0x100] = {};
			if (sample_type & PERF_SAMPLE_IP) {
				memcpy(&ip,&data[offset],sizeof(long long));
				//printf("\tPERF_SAMPLE_IP, IP: %llx\n",ip);
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_TID) {
				memcpy(&pid,&data[offset],sizeof(int));
				memcpy(&tid,&data[offset+4],sizeof(int));
                                                                                    //myid=pid;
				//printf("\tPERF_SAMPLE_TID, pid: %d  tid %d\n",pid,tid);
				offset+=8;
			}

			if (sample_type & PERF_SAMPLE_TIME) {
				memcpy(&time,&data[offset],sizeof(long long));
				//printf("\tPERF_SAMPLE_TIME, time: %lld\n",time);
				//if (begin_time==0) begin_time=time;
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_ADDR) {
                                                                                    //long long set;
				memcpy(&addr,&data[offset],sizeof(long long));
                                                                                    //sprintf(path_buf, "/proc/%u/pagemap", pid);
                                phy_addr=read_pagemap(path_buf, addr);
                                offset+=8;
                                                                                    
				//printf("\tPERF_SAMPLE_ADDR, addr: %llx, physical address: %llx, set: %llu \n",addr,phy_addr,(phy_addr>>6)&0x7ff);
				
			}
			if (sample_type & PERF_SAMPLE_ID) {
				long long sample_id;
				memcpy(&sample_id,&data[offset],sizeof(long long));
				printf("\tPERF_SAMPLE_ID, sample_id: %lld\n",sample_id);
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_STREAM_ID) {
				long long sample_stream_id;
				memcpy(&sample_stream_id,&data[offset],sizeof(long long));
				printf("\tPERF_SAMPLE_STREAM_ID, sample_stream_id: %lld\n",sample_stream_id);
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_CPU) {
				int cpu, res;
				memcpy(&cpu,&data[offset],sizeof(int));
				memcpy(&res,&data[offset+4],sizeof(int));
				printf("\tPERF_SAMPLE_CPU, cpu: %d  res %d\n",cpu,res);
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_PERIOD) {
				long long period;
				memcpy(&period,&data[offset],sizeof(long long));
				printf("\tPERF_SAMPLE_PERIOD, period: %lld\n",period);
				offset+=8;
			}
			if (sample_type & PERF_SAMPLE_READ) {
				int length;

				length=handle_struct_read_format(&data[offset],
						read_format,time-begin_time);
				if (length>=0) offset+=length;
			}
			if (sample_type & PERF_SAMPLE_CALLCHAIN) {
			}
			if (sample_type & PERF_SAMPLE_RAW) {
			}

			if (sample_type & PERF_SAMPLE_BRANCH_STACK) {
	   		}
			if (sample_type & PERF_SAMPLE_REGS_USER) {
			}

			if (sample_type & PERF_SAMPLE_REGS_INTR) {
			}

			if (sample_type & PERF_SAMPLE_STACK_USER) {
			}

			if (sample_type & PERF_SAMPLE_WEIGHT) {
			}

			if (sample_type & PERF_SAMPLE_DATA_SRC) {
                          /*
                           long long src=0; 
		           memcpy(&src,&data[offset],sizeof(long long));
                           printf("\n %lld \n", src);
                           offset+=8;
                           if (src & (PERF_MEM_LVL_HIT<<PERF_MEM_LVL_SHIFT))
						hit=1;
					if (src & (PERF_MEM_LVL_MISS<<PERF_MEM_LVL_SHIFT))
						hit=0;
                           if (src & (PERF_MEM_LVL_L3<<PERF_MEM_LVL_SHIFT))
                               hit=2;
                           					if (src!=0) printf("\t\t");
					if (src & (PERF_MEM_OP_NA<<PERF_MEM_OP_SHIFT))
						printf("Op Not available ");
					if (src & (PERF_MEM_OP_LOAD<<PERF_MEM_OP_SHIFT))
						printf("Load ");
					if (src & (PERF_MEM_OP_STORE<<PERF_MEM_OP_SHIFT))
						printf("Store ");
					if (src & (PERF_MEM_OP_PFETCH<<PERF_MEM_OP_SHIFT))
						printf("Prefetch ");
					if (src & (PERF_MEM_OP_EXEC<<PERF_MEM_OP_SHIFT))
						printf("Executable code ");

					if (src & (PERF_MEM_LVL_NA<<PERF_MEM_LVL_SHIFT))
						printf("Level Not available ");
					if (src & (PERF_MEM_LVL_HIT<<PERF_MEM_LVL_SHIFT))
						printf("Hit ");
					if (src & (PERF_MEM_LVL_MISS<<PERF_MEM_LVL_SHIFT))
						printf("Miss ");
					if (src & (PERF_MEM_LVL_L1<<PERF_MEM_LVL_SHIFT))
						printf("L1 cache ");
					if (src & (PERF_MEM_LVL_LFB<<PERF_MEM_LVL_SHIFT))
						printf("Line fill buffer ");
					if (src & (PERF_MEM_LVL_L2<<PERF_MEM_LVL_SHIFT))
						printf("L2 cache ");
					if (src & (PERF_MEM_LVL_L3<<PERF_MEM_LVL_SHIFT))
						printf("L3 cache ");
					if (src & (PERF_MEM_LVL_LOC_RAM<<PERF_MEM_LVL_SHIFT))
						printf("Local DRAM ");
					if (src & (PERF_MEM_LVL_REM_RAM1<<PERF_MEM_LVL_SHIFT))
						printf("Remote DRAM 1 hop ");
					if (src & (PERF_MEM_LVL_REM_RAM2<<PERF_MEM_LVL_SHIFT))
						printf("Remote DRAM 2 hops ");
					if (src & (PERF_MEM_LVL_REM_CCE1<<PERF_MEM_LVL_SHIFT))
						printf("Remote cache 1 hop ");
					if (src & (PERF_MEM_LVL_REM_CCE2<<PERF_MEM_LVL_SHIFT))
						printf("Remote cache 2 hops ");
					if (src & (PERF_MEM_LVL_IO<<PERF_MEM_LVL_SHIFT))
						printf("I/O memory ");
					if (src & (PERF_MEM_LVL_UNC<<PERF_MEM_LVL_SHIFT))
						printf("Uncached memory ");

					if (src & (PERF_MEM_SNOOP_NA<<PERF_MEM_SNOOP_SHIFT))
						printf("Not available ");
					if (src & (PERF_MEM_SNOOP_NONE<<PERF_MEM_SNOOP_SHIFT))
						printf("No snoop ");
					if (src & (PERF_MEM_SNOOP_HIT<<PERF_MEM_SNOOP_SHIFT))
						printf("Snoop hit ");
					if (src & (PERF_MEM_SNOOP_MISS<<PERF_MEM_SNOOP_SHIFT))
						printf("Snoop miss ");
					if (src & (PERF_MEM_SNOOP_HITM<<PERF_MEM_SNOOP_SHIFT))
						printf("Snoop hit modified ");

					if (src & (PERF_MEM_LOCK_NA<<PERF_MEM_LOCK_SHIFT))
						printf("Not available ");
					if (src & (PERF_MEM_LOCK_LOCKED<<PERF_MEM_LOCK_SHIFT))
						printf("Locked transaction ");

					if (src & (PERF_MEM_TLB_NA<<PERF_MEM_TLB_SHIFT))
						printf("Not available ");
					if (src & (PERF_MEM_TLB_HIT<<PERF_MEM_TLB_SHIFT))
						printf("Hit ");
					if (src & (PERF_MEM_TLB_MISS<<PERF_MEM_TLB_SHIFT))
						printf("Miss ");
					if (src & (PERF_MEM_TLB_L1<<PERF_MEM_TLB_SHIFT))
						printf("Level 1 TLB ");
					if (src & (PERF_MEM_TLB_L2<<PERF_MEM_TLB_SHIFT))
						printf("Level 2 TLB ");
					if (src & (PERF_MEM_TLB_WK<<PERF_MEM_TLB_SHIFT))
						printf("Hardware walker ");
					if (src & ((long long)PERF_MEM_TLB_OS<<PERF_MEM_TLB_SHIFT))
						printf("OS fault handler ");
                                      */
			}

			if (sample_type & PERF_SAMPLE_IDENTIFIER) {
			}

			if (sample_type & PERF_SAMPLE_TRANSACTION) {
			}
                                                               //fprintf(sample,"%lld %d %llx %llx %llx %lld \n", time,pid,ip,addr,phy_addr,(phy_addr>>6)&0x7ff);
                        if (phy_addr!=0) {
                                                                   //add the slice selection algorithms
                                                                   // set_index = (phy_addr>>6)&0x7ff
                                                                   // p17= (phy_addr>>17)&1
                       int p1=0,p2=0,set_index=0;
                                                                      /*
                                                                      p1=((phy_addr>>17)&1)^ ((phy_addr>>18)&1)^
                                                                         ((phy_addr>>20)&1)^ ((phy_addr>>22)&1)^
                                                                         ((phy_addr>>24)&1)^ ((phy_addr>>25)&1)^
                                                                         ((phy_addr>>26)&1)^ ((phy_addr>>27)&1)^
                                                                         ((phy_addr>>28)&1)^ ((phy_addr>>30)&1)^
                                                                         ((phy_addr>>32)&1);
                                                                      p2=((phy_addr>>18)&1)^ ((phy_addr>>19)&1)^
                                                                         ((phy_addr>>21)&1)^ ((phy_addr>>23)&1)^
                                                                         ((phy_addr>>25)&1)^ ((phy_addr>>27)&1)^
                                                                         ((phy_addr>>29)&1)^ ((phy_addr>>30)&1)^
                                                                         ((phy_addr>>31)&1)^ ((phy_addr>>32)&1);
                                                                      */
                                                                 /*another possibility                                                                     
                                                                      p1=((phy_addr>>18)&1)^ ((phy_addr>>19)&1)^
                                                                         ((phy_addr>>21)&1)^ ((phy_addr>>23)&1)^
                                                                         ((phy_addr>>25)&1)^ ((phy_addr>>27)&1)^
                                                                         ((phy_addr>>29)&1)^ ((phy_addr>>30)&1)^
                                                                         ((phy_addr>>31)&1);
                                                                      p2=((phy_addr>>17)&1)^ ((phy_addr>>19)&1)^
                                                                         ((phy_addr>>20)&1)^ ((phy_addr>>21)&1)^
                                                                         ((phy_addr>>22)&1)^ ((phy_addr>>23)&1)^
                                                                         ((phy_addr>>24)&1)^ ((phy_addr>>26)&1)^
                                                                         ((phy_addr>>28)&1)^ ((phy_addr>>29)&1)^
                                                                         ((phy_addr>>31)&1) ;
                                                                       */
                                                                      //set_index=((p1*2)+p2)*2048+((phy_addr>>6)&0x7ff);            
                                                                      //set_index=((p1*2)+p2)*2048+((phy_addr>>6)&0x7ff);            
                                                                      set_index=(phy_addr>>6)&0x7ff;
			                                              fprintf(sample,"%lld %d \n",time,set_index);
                                                                      //fprintf(sample,"%d \n",set_index);
                                                               }
			}
			break;

			default: printf("\tUnknown type %d\n",event->type);
		}
	}

	control_page->data_tail=head;

	free(data);

	return head;

}
