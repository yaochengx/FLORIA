#ifndef PERF_HELP_H
#define PERF_HELP_H
#include <stdio.h>
#include <stdlib.h>
int handle_struct_read_format(unsigned char *sample, int read_format, long long timestamp);
long long perf_mmap_read( void *our_mmap, int mmap_size, long long prev_head, int sample_type, int read_format, FILE *sample,char *path_buf,FILE *pagemap);
//unsigned long read_pagemap(char *path_buf, FILE *pagemap, unsigned long virt_addr);
unsigned long read_pagemap(char *path_buf, unsigned long virt_addr);
#endif
