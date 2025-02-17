#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <limits.h>
#ifndef CACHE_H
#define CACHE_H

typedef struct {
    int valid;
    int dirty;              // For write-back policy
    unsigned tag;
    char* block;            // Pointer to data block
    int last_access_time;   // For LRU policy
    int load_time;          // For FIFO policy
    int frequency;          // For LFU policy
} CacheLine;

typedef struct{
    CacheLine* lines;        // Array of cache lines for set-associativity
} CacheSet;

typedef struct{
    int num_sets;
    int lines_per_set;
    CacheSet* sets;          // Array of cache sets
} Cache;

extern int cache_enabled;
extern Cache* cache;
extern int cache_size, block_size, associativity;
extern char replacement_policy[8];
extern char write_back_policy[8];

void enable_cache(char* config_file);
void disable_cache();
void print_cache_status();
void calculate_cache_address(unsigned address, int* tag, int* set_index);

#endif