#include "assembler.h"
#include "cache.h"

int cache_size, block_size, associativity;
char replacement_policy[8];
char write_back_policy[8];

int cache_enabled = 0;

Cache* cache;

void initialize_cache(){
    int num_sets = cache_size / (block_size * associativity);
    cache = (Cache*)malloc(sizeof(Cache));
    cache->num_sets = num_sets;
    cache->lines_per_set = associativity;
    cache->sets = (CacheSet*)malloc(num_sets * sizeof(CacheSet));

    for(int i = 0; i < num_sets; i++){
        cache->sets[i].lines = (CacheLine*)malloc(associativity * sizeof(CacheLine));
        for(int j = 0;j<associativity;j++){
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].block = (char*)malloc(block_size);
            cache->sets[i].lines[j].last_access_time = 0;  // Initialize for LRU policy
            cache->sets[i].lines[j].load_time = 0;         // Initialize for FIFO policy
        }
    }
}

void enable_cache(char* config_file){
    FILE* fptr = fopen(config_file, "r");
    if(fptr == NULL){
        perror("Error opening file");
        return;
    }

    char buffer[50];
    int line_num = 0;

    while(fgets(buffer, sizeof(buffer), fptr)){
        // Remove newline character if it exists
        buffer[strcspn(buffer, "\n")] = 0;

        switch(line_num){
            case 0:
                cache_size = atoi(buffer);
                break;
            case 1:
                block_size = atoi(buffer);
                break;
            case 2:
                associativity = atoi(buffer);
                break;
            case 3:
                strncpy(replacement_policy, buffer, sizeof(replacement_policy) - 1);
                replacement_policy[sizeof(replacement_policy) - 1] = '\0'; // Ensure null-termination
                break;
            case 4:
                strncpy(write_back_policy, buffer, sizeof(write_back_policy) - 1);
                write_back_policy[sizeof(write_back_policy) - 1] = '\0'; // Ensure null-termination
                break;
            case 5:
                break;
            default:
                printf("Unexpected line in configuration file\n");
                break;
        }
        line_num++;
    }
    fclose(fptr);
    initialize_cache();
    cache_enabled = 1;
}

void disable_cache(){
    cache_enabled = 0;
    printf("Cache simulation disabled.\n");
}

void print_cache_status(){
    if(cache_enabled){
        printf("Cache Status: Enabled\n");
        printf("Cache Size: %d bytes\n", cache_size);
        printf("Block Size: %d bytes\n", block_size);
        printf("Associativity: %d-way\n", associativity);
        printf("Replacement Policy: %s\n", replacement_policy);
        printf("Write-Back Policy: %s\n", write_back_policy);
    }
    else printf("Cache Status: Disabled\n");    
}

void calculate_cache_address(unsigned address, int* tag, int* set_index){
    int block_offset_bits = __builtin_ctz(block_size);  // Identifying the required number of bits to address all spots in the block
    int set_index_bits = __builtin_ctz(cache->num_sets);    // Similar to above
    *tag = (int)(address >> (set_index_bits + block_offset_bits));  // Calculating tag and set_index values
    *set_index = (int)((address >> block_offset_bits) & (cache->num_sets - 1));
}