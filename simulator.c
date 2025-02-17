#include "assembler.h"  // Necessary imports
#include "cache.h"
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>

unsigned text_section[DATA_START / 4] = {0};
long long int data_section[STACK_START - DATA_START] = {0};

long long int registers[NUM_REGS] = {0};
unsigned pc = TEXT_START;
unsigned stack_pointer = STACK_START;

int instr_count = 0;    // Used to keep track of number of instructions in the input file

char instructions[MAX_LINES][MAX_LINE_LEN];  // 2D array to store instructions from the input file
int instruction_lines[MAX_LINES];   // Keeping track of line numbers of instructions

typedef struct call_frame{
    char* label;
    int line_num;
} call_frame;

call_frame call_stack[MAX_LINES];  // Call stack
int stack_top = -1;  // Points to the top of the stack

int break_points[MAX_LINES] = {0};

char filename[256];

FILE* cache_stat_ptr = NULL;

int cache_accesses = 0;
int cache_hits = 0;
int cache_misses = 0;
int cache_clock = 0;

CacheLine* select_eviction_line(CacheSet* set){
    CacheLine* eviction_line = NULL;
    int oldest_load_time = INT_MAX;
    int oldest_access_time = INT_MAX;
    int min_frequency = INT_MAX;

    // Choosing a random victim if the policy is random
    if(strcmp(replacement_policy, "RANDOM") == 0){
        int random_index = rand() % cache->lines_per_set;
        return &set->lines[random_index];
    }

    for(int i = 0; i < cache->lines_per_set; i++){
        CacheLine* line = &set->lines[i];
        if(!line->valid) return line;   // Pick an empty line if we can

        if(strcmp(replacement_policy, "LRU") == 0){
            if(line->last_access_time < oldest_access_time){
                oldest_access_time = line->last_access_time;
                eviction_line = line;
            }
        }
        else if(strcmp(replacement_policy, "FIFO") == 0){
            if(line->load_time < oldest_load_time){
                oldest_load_time = line->load_time;
                eviction_line = line;
            }
        }
        else if(strcmp(replacement_policy, "LFU") == 0){
            if(line->frequency < min_frequency ||
               (line->frequency == min_frequency && line->last_access_time < oldest_access_time)){
                min_frequency = line->frequency;
                oldest_access_time = line->last_access_time;
                eviction_line = line;
            }
        }
    }
    return eviction_line;
}

// Used to load one block of memory into chosen cache line
void load_block_from_memory(CacheLine* line, unsigned address){
    unsigned block_start = address - (address % block_size);
    memcpy(line->block, &data_section[block_start - DATA_START], block_size);
}

// Function to update cache line access times
void update_access_time(CacheLine* line){
    line->last_access_time = cache_clock;
    line->frequency++;
    cache_clock++;
}

// Function to directly write data into memory
void write_data_to_memory(unsigned address, long long data, int funct3){
    if(funct3 == 0x0){
        data_section[address - DATA_START] = data & 0xff;
    }
    else if (funct3 == 0x1){
        data_section[address - DATA_START] = data & 0xff;
        data_section[address + 1 - DATA_START] = (data >> 8) & 0xff;
    }
    else if (funct3 == 0x2){
        data_section[address - DATA_START] = data & 0xff;
        data_section[address + 1 - DATA_START] = (data >> 8) & 0xff;
        data_section[address + 2 - DATA_START] = (data >> 16) & 0xff;
        data_section[address + 3 - DATA_START] = (data >> 24) & 0xff;
    }
    else if (funct3 == 0x3){
        data_section[address - DATA_START] = data & 0xff;
        data_section[address + 1 - DATA_START] = (data >> 8) & 0xff;
        data_section[address + 2 - DATA_START] = (data >> 16) & 0xff;
        data_section[address + 3 - DATA_START] = (data >> 24) & 0xff;
        data_section[address + 4 - DATA_START] = (data >> 32) & 0xff;
        data_section[address + 5 - DATA_START] = (data >> 40) & 0xff;
        data_section[address + 6 - DATA_START] = (data >> 48) & 0xff;
        data_section[address + 7 - DATA_START] = (data >> 56) & 0xff;
    }
}

// Function to write into a chosen cache line
void write_data_to_cache_line(unsigned address, long long data, int funct3){
    cache_accesses++;
    int bytes_to_write;
    switch(funct3){
        case 0x0: bytes_to_write = 1; break;
        case 0x1: bytes_to_write = 2; break;
        case 0x2: bytes_to_write = 4; break;
        case 0x3: bytes_to_write = 8; break;
        default: return;
    }

    int bytes_written = 0;
    while(bytes_written < bytes_to_write){
        unsigned current_address = address + bytes_written;
        unsigned tag;
        int set_index;
        calculate_cache_address(current_address, &tag, &set_index);

        CacheSet* set = &cache->sets[set_index];
        CacheLine* target_line = NULL;

        for(int i = 0; i < cache->lines_per_set; i++){
            if(set->lines[i].valid && set->lines[i].tag == tag){
                target_line = &set->lines[i];
                break;
            }
        }

        if(!target_line){
            cache_misses++;
            target_line = select_eviction_line(set);
            if(target_line->dirty && strcmp(write_back_policy, "WB") == 0){
                unsigned mem_address = (target_line->tag << (__builtin_ctz(block_size) + __builtin_ctz(cache->num_sets))) | (set_index << __builtin_ctz(block_size));
                write_data_to_memory(mem_address, *(long long*)(target_line->block), funct3);
            }
            load_block_from_memory(target_line, current_address);
            target_line->tag = tag;
            target_line->valid = 1;
            target_line->dirty = 0;
        }
        else{
            cache_hits++;
            update_access_time(target_line);
        }
        
        int offset = current_address % block_size;
        int bytes_in_line = block_size - offset;
        int bytes_this_write = (bytes_written + bytes_in_line > bytes_to_write) ? (bytes_to_write - bytes_written) : bytes_in_line;

        for(int i = 0; i < bytes_this_write; i++){
            target_line->block[offset + i] = (data >> (8 * bytes_written)) & 0xff;
        }

        target_line->dirty = 1;
        bytes_written += bytes_this_write;

        if(strcmp(write_back_policy, "WT") == 0) write_data_to_memory(current_address, data, funct3);
    }
}

// Function to handle misses while writing into cache
void handle_write_miss(unsigned address, long long data, int funct3){
    cache_misses++;
    unsigned tag;
    int set_index;
    calculate_cache_address(address, &tag, &set_index);
    CacheSet* set = &cache->sets[set_index];
    CacheLine* line = select_eviction_line(set);

    if(line->dirty && strcmp(write_back_policy, "WB") == 0){
        unsigned evict_address = (line->tag << (__builtin_ctz(block_size) + __builtin_ctz(cache->num_sets))) | (set_index << __builtin_ctz(block_size));
        write_data_to_memory(evict_address, *(int*)(line->block), funct3);
    }

    line->tag = tag;
    line->valid = 1;
    line->dirty = 0;

    memcpy(line->block, &data_section[address - (address % block_size) - DATA_START], block_size);
    write_data_to_cache_line(address, data, funct3);

    if(strcmp(write_back_policy, "WT") == 0) write_data_to_memory(address, data, funct3);
    else if(strcmp(write_back_policy, "WB") == 0) line->dirty = 1;
}

// Method to write into cache
void cache_write(unsigned address, long long data, int funct3){
    cache_accesses++;
    unsigned tag;
    int set_index;
    calculate_cache_address(address, &tag, &set_index);

    CacheSet* set = &(cache->sets[set_index]);
    for(int i = 0; i < cache->lines_per_set; i++){
        CacheLine* line = &(set->lines[i]);
        if(line->valid && line->tag == tag){
            cache_hits++;
            write_data_to_cache_line(address, data, funct3);
            if(strcmp(write_back_policy, "WB") == 0) line->dirty = 1;
            else if(strcmp(write_back_policy, "WT") == 0) write_data_to_memory(address, data, funct3);
            return;
        }
    }
    handle_write_miss(address, data, funct3);
}

int read_data_from_cache_line(CacheLine* line, unsigned address, int funct3){
    int offset = address % block_size;
    int data = 0;

    switch(funct3){
        case 0x0:
            data = line->block[offset];
            break;
        case 0x1:
            data = line->block[offset] | (line->block[offset + 1] << 8);
            break;
        case 0x2:
            data = line->block[offset] | (line->block[offset + 1] << 8) |
                   (line->block[offset + 2] << 16) | (line->block[offset + 3] << 24);
            break;
        case 0x3:
            data = *(long long int*)(line->block + offset);
            break;
    }
    return data;
}

int cache_read(unsigned address, int funct3, long long* read_data){
    cache_accesses++;
    unsigned tag;
    int set_index;
    int offset = address % block_size;

    int bytes_to_read;
    switch(funct3){
        case 0x0:
            bytes_to_read = 1;
            break;
        case 0x1:
            bytes_to_read = 2;
            break;
        case 0x2:
            bytes_to_read = 4;
            break;
        case 0x3:
            bytes_to_read = 8;
            break;
        default:
            return -1;
    }

    int bytes_read = 0;
    while(bytes_read < bytes_to_read){
        unsigned current_address = address + bytes_read;
        calculate_cache_address(current_address, &tag, &set_index);

        CacheSet* set = &cache->sets[set_index];
        CacheLine* target_line = NULL;

        for(int i = 0; i < cache->lines_per_set; i++){
            if(set->lines[i].valid && set->lines[i].tag == tag){
                target_line = &set->lines[i];
                break;
            }
        }

        if(target_line == NULL){
            cache_misses++;
            target_line = select_eviction_line(set);
            if(target_line->dirty && strcmp(write_back_policy, "WB") == 0){
                unsigned mem_address = (target_line->tag << (__builtin_ctz(block_size) + __builtin_ctz(cache->num_sets))) |
                                       (set_index << __builtin_ctz(block_size));
                write_data_to_memory(mem_address, *(int*)(target_line->block), funct3);
            }

            load_block_from_memory(target_line, current_address);
            target_line->tag = tag;
            target_line->valid = 1;
            target_line->dirty = 0;
        }
        else{
            cache_hits++;
            update_access_time(target_line);
        }

        int bytes_in_line = block_size - offset;
        int bytes_this_read = (bytes_read + bytes_in_line > bytes_to_read) ? (bytes_to_read - bytes_read) : bytes_in_line;

        for(int i = 0; i < bytes_this_read; i++){
            *read_data |= ((long long)target_line->block[offset + i] << (8 * bytes_read));
            bytes_read++;
        }
    }
    return 0;
}

// Function to push labels onto stack(for jal)
void push_stack(const char* label, int start){
    if(!start){
        if(stack_top < MAX_LINES){
            stack_top += 1;
            call_stack[stack_top].label = strdup(label);
            call_stack[stack_top].line_num = instruction_lines[pc / 4];
        }
        else printf("Error: Stack overflow.\n");
    }
    else{
        if(stack_top < MAX_LINES){
            stack_top += 1;
            call_stack[stack_top].label = "main";
            call_stack[stack_top].line_num = pc / 4;
        }
        else printf("Error: Stack overflow.\n");
    }
}
// Function to pop labels from stack(for jalr)
void pop_stack(){
    if(stack_top > -1){  // Ensure 'main' remains at the bottom
        free(call_stack[stack_top].label);  // Free the top entry
        call_stack[stack_top].line_num = 0;
        stack_top -= 1;
    }
}
// Function to display the call stack
void show_stack(){
    printf("Call Stack:\n");
    if(stack_top == -1) printf("Empty Call Stack: Execution complete");
    for(int i = 0; i <= stack_top; i++){
        printf("%s:%d\n", call_stack[i].label, call_stack[i].line_num);
    }
}
// Function to reset the values of all memory locations, registers, etc
void reset(){
    memset(text_section, 0, DATA_START / 4);
    memset(data_section, 0, STACK_START - DATA_START);
    memset(registers, 0, NUM_REGS);
    memset(break_points, 0, 64);
    memset(instruction_lines, 0, MAX_LINES);

    instr_count = 0;
    pc = TEXT_START;
    stack_pointer = STACK_START;

    cache_accesses = 0;
    cache_hits = 0;
    cache_misses = 0;
    cache_clock = 0;

    for(int i=0;i<MAX_LINES;i++){
        memset(instructions[i], 0, MAX_LINE_LEN);
    }
    pop_stack();
}
// Loads a file into instruction memory, performs necessary implementations
void load(char *filename){
    reset();    // Resetting registers, instruction memory, etc
    // Opening file pointers
    FILE* fptr = fopen(filename, "r");
    if(!fptr){
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
  
    FILE* optr = fopen("temp.hex", "w+");   // Creating a .hex file to store generated machine code, for user's benefit
    if(!optr){
        printf("Error: Cannot create temp file for machine code\n");
        fclose(fptr);
        return;
    }

    int in_data_section = 0;
    int in_text_section = 0;
    int has_data_section = 0;
    int has_text_section = 0;
    unsigned int memory_address = DATA_START; // Starting address for the .data section
    long long int value;
    char line[100];
    int error = 0;
    unsigned line_number = 0;

    // Scanning for the presence of sections
    rewind(fptr);
    while(fgets(line, sizeof(line), fptr)){
        if(line[0] == '\n' || line[0] == ';' || line[0] == '#') continue;   // Accounting for empty lines and comments
        char *token = strtok(line, " \t\n");

        if(strcmp(token, ".data") == 0){
            if(has_text_section == 1){
                printf(".text is defined before data! Please rectify error and try again\n");
                return;
            }
            has_data_section = 1;
        }
        else if(strcmp(token, ".text") == 0) has_text_section = 1;
        
    }
    rewind(fptr);  // Rewinding the file pointer for actual processing
    // Checking for rule violations
    if(has_data_section && !has_text_section){
        printf("Error: .text section is missing, but .data section is present.\n");
        fclose(fptr);
        fclose(optr);
        return;
    }
    while(fgets(line, sizeof(line), fptr)){
        line_number++;

        if(line[0] == '\n' || line[0] == ';' || line[0] == '#') continue;   // Accounting for empty lines and comments
        char *token = strtok(line, " \t\n");

        if(strcmp(token, ".data") == 0){
            in_data_section = 1;
            in_text_section = 0;
            continue;
        }

        if(strcmp(token, ".text") == 0){
            in_text_section = 1;
            in_data_section = 0;
            continue;
        }
        // Handling the .data section content
        if(in_data_section){
            if(strcmp(token, ".dword") == 0){
                while((token = strtok(NULL, " ,\t\n")) != NULL){
                    // Checking if the token starts with "0x" for hexadecimal, otherwise use decimal
                    if(token[0] == '0' && token[1] == 'x') value = strtoll(token, NULL, 16); // Hexadecimal   
                    else value = strtoll(token, NULL, 10); // Decimal
                        
                    if(memory_address + 8 >= STACK_START){
                        printf("Insufficient space in memory! Please modify code and try again\n");
                        fclose(fptr);
                        fclose(optr);
                        return;
                    }
                    for(int i = 0; i < 8; i++){
                        data_section[memory_address++ - DATA_START] = (value >> (i * 8)) & 0xff;
                    }
                }
            }
            else if (strcmp(token, ".word") == 0){
                while((token = strtok(NULL, " ,\t\n")) != NULL) {
                    if (token[0] == '0' && token[1] == 'x') value = strtoll(token, NULL, 16); // Hexadecimal
                    else value = strtoll(token, NULL, 10); // Decimal

                    if(memory_address + 4 >= STACK_START){
                        printf("Insufficient space in memory! Please modify code and try again\n");
                        fclose(fptr);
                        fclose(optr);
                        return;
                    }
                    for(int i = 0; i < 4; i++){
                        data_section[memory_address++ - DATA_START] = (value >> (i * 8)) & 0xff;
                    }
                }
            }
            else if(strcmp(token, ".half") == 0){
                while((token = strtok(NULL, " ,\t\n")) != NULL){
                    if(token[0] == '0' && token[1] == 'x') value = strtoll(token, NULL, 16); // Hexadecimal
                    else value = strtol(token, NULL, 10); // Decimal

                    if(memory_address + 2 >= STACK_START){
                        printf("Insufficient space in memory! Please modify code and try again\n");
                        fclose(fptr);
                        fclose(optr);
                        return;
                    }
                    for(int i = 0; i < 2; i++){
                        data_section[memory_address++ - DATA_START] = (value >> (i * 8)) & 0xff;
                    }
                }
            }
            else if(strcmp(token, ".byte") == 0){
                while((token = strtok(NULL, " ,\t\n")) != NULL){
                    if(token[0] == '0' && token[1] == 'x') value = strtoll(token, NULL, 16); // Hexadecimal
                    else value = strtoll(token, NULL, 10); // Decimal

                    // Store 1 byte directly
                    if(memory_address + 1 >= STACK_START){
                        printf("Insufficient space in memory! Please modify code and try again\n");
                        fclose(fptr);
                        fclose(optr);
                        return;
                    }
                    data_section[memory_address++ - DATA_START] = value & 0xff;
                }
            }
            else{
                printf("Error: Unsupported directive in .data section: %s\n", token);   // Handling unidentified commands
                fclose(fptr);
                fclose(optr);
                return;
            }
        }

        // Handling assembly instructions in the .text section
        if(in_text_section || (!has_text_section && !has_data_section)){
            rewind(fptr);
            if(parse_labels(fptr)){
                rewind(fptr);
                process_instructions(fptr, optr);
                push_stack("main", 1);
            }
            else{
                printf("Please rectify error and load the file once more\n");
                fclose(fptr);
                fclose(optr);
                return;
            }

            // Store instructions in a 2D array
            rewind(fptr);
            char buffer[MAX_LINE_LEN];
            int total_lines = 0;
            while(fgets(buffer, sizeof(buffer), fptr) != NULL){
                if(buffer[0] == '\n' || buffer[0] == ';' || buffer[0] == '#' || buffer[0] == '.') continue;  // Skip comments and blank lines

                // Handle labels and instructions
                char *label_end = strchr(buffer, ':');
                char *command_part = buffer;

                if(label_end){
                    *label_end = '\0';  // Terminate the label part
                    command_part = label_end + 1;  // Move to the command part
                }

                // Remove extra whitespace
                while(*command_part == ' ' || *command_part == '\t'){
                    command_part++;
                }
                char* newline = strchr(command_part, '\n');
                if(newline != NULL) *newline = '\0';

                strcpy(instructions[total_lines], command_part);  // Store the command part
                instruction_lines[total_lines] = line_number;
                total_lines++;
                line_number++;
            }

            // Load instructions into text section
            unsigned addr = 0;
            unsigned instruction;
            rewind(fptr);
            rewind(optr);
            while(fscanf(optr, "%x", &instruction) != EOF && addr < (0x10000 / 4)){
                text_section[addr++] = instruction;
                instr_count++;
            }
            pc = TEXT_START;
            fclose(fptr);
            fclose(optr);
            printf("Loaded %d instructions into text section from %s.\n", addr, filename);
        }
    }
}
// Function used to execute any given instruction
void execute_instruction(unsigned instruction){
    unsigned opcode = instruction & 0x7f;
    if(opcode == 0x33){   // R-type  
        unsigned int funct3 = (instruction >> 12) & 0x7;
        unsigned int rs1 = (instruction >> 15) & 0x1f;
        unsigned int rs2 = (instruction >> 20) & 0x1f;
        unsigned int rd = (instruction >> 7) & 0x1f;
        unsigned int funct7 = (instruction >> 25) & 0x7f;
        if(rd != 0){
            if(funct3 == 0x0 && funct7 == 0x00){
                registers[rd] = registers[rs1] + registers[rs2];
            }
            else if(funct3 == 0x0 && funct7 == 0x20){
                registers[rd] = registers[rs1] - registers[rs2];
            }
            else if(funct3 == 0x4 && funct7 == 0x00){
                registers[rd] = registers[rs1] ^ registers[rs2];
            }
            else if(funct3 == 0x6 && funct7 == 0x00){
                registers[rd] = registers[rs1] | registers[rs2];
            }
            else if(funct3 == 0x7 && funct7 == 0x00){
                registers[rd] = registers[rs1] & registers[rs2];
            }
            else if(funct3 == 0x1 && funct7 == 0x00){
                registers[rd] = registers[rs1] << (registers[rs2] & 0x1F);
            }
            else if(funct3 == 0x5 && funct7 == 0x00){
                registers[rd] = registers[rs1] >> (registers[rs2] & 0x1F);
            }
            else if(funct3 == 0x5 && funct7 == 0x20){
                registers[rd] = (int)registers[rs1] >> (registers[rs2] & 0x1F);
            }
        }
        pc += 4;     
    }
    else if(opcode == 0x13){  // I-type part 1
        unsigned int funct3 = (instruction >> 12) & 0x7;
        unsigned int rs1 = (instruction >> 15) & 0x1F;
        unsigned int rd = (instruction >> 7) & 0x1f;
        int imm = (instruction >> 20);

        if(imm & 0x800){
            imm |= 0xfffff000;
        }
        if(rd != 0){
            if(funct3 == 0x0){
                registers[rd] = registers[rs1] + imm;
            }
            else if(funct3 == 0x4){
                registers[rd] = registers[rs1] ^ imm;
            }
            else if(funct3 == 0x6){
                registers[rd] = registers[rs1] | imm;
            }
            else if(funct3 == 0x7){
                registers[rd] = registers[rs1] & imm;
            }
            else if(funct3 == 0x1 && (imm & 0xfe0) == 0){
                registers[rd] = registers[rs1] << (imm & 0x1f);
            }
            else if(funct3 == 0x5 && (imm & 0xfe0) == 0){
                registers[rd] = (unsigned int)registers[rs1] >> (imm & 0x1f);
            }
            else if(funct3 == 0x5 && (imm & 0xfe0) == 0x400){
                registers[rd] = (int)registers[rs1] >> (imm & 0x1f);
            }
            else if(funct3 == 0x2){
                registers[rd] = (int)registers[rs1] < imm ? 1 : 0;
            }
            else if(funct3 == 0x3){
                registers[rd] = (unsigned int)registers[rs1] < (unsigned int)imm ? 1 : 0;
            }
        }
        pc += 4;
    }
    else if(opcode == 0x3){ // I-type part 2
        unsigned int funct3 = (instruction >> 12) & 0x7;
        unsigned int rs1 = (instruction >> 15) & 0x1f;
        unsigned int rd = (instruction >> 7) & 0x1f;
        long long int imm = (instruction >> 20);

        if(imm & 0x800) imm |= 0xfffff000;
        
        unsigned address = registers[rs1] + imm;
        
        if(rd!=0){
            int cache_result;
            if(cache_enabled){
                long long read_data;
                cache_result = cache_read(address, funct3, &read_data);  // Call cache_read() if cache is enabled
                registers[rd] = read_data;
            }
            else{
                if(funct3 == 0x0){
                    registers[rd] = data_section[imm + registers[rs1] - DATA_START];
                    registers[rd] = registers[rd] << (64 - 8);
                    registers[rd] = registers[rd] >> (64 - 8);
                }
                else if(funct3 == 0x1){
                    registers[rd] = data_section[imm + registers[rs1] - DATA_START] + (data_section[imm + 1 + registers[rs1] - DATA_START] << 8);
                    registers[rd] = registers[rd] << (64 - 16);
                    registers[rd] = registers[rd] >> (64 - 16);
                }
                else if(funct3 == 0x2){
                    registers[rd] = data_section[imm + registers[rs1] - DATA_START] + (data_section[imm + 1 + registers[rs1] - DATA_START] << 8);
                    registers[rd] += (data_section[imm + 2 + registers[rs1] - DATA_START] << 16) + (data_section[imm + 3 + registers[rs1] - DATA_START] << 24);
                    registers[rd] = registers[rd] << (64 - 32);
                    registers[rd] = registers[rd] >> (64 - 32);
                }
                else if(funct3 == 0x3){
                    registers[rd] = data_section[imm + registers[rs1] - DATA_START] + (data_section[imm + 1 + registers[rs1] - DATA_START] << 8);
                    registers[rd] += (data_section[imm + 2 + registers[rs1] - DATA_START] << 16) + (data_section[imm + 3 + registers[rs1] - DATA_START] << 24);
                    registers[rd] += (data_section[imm + 4 + registers[rs1] - DATA_START] << 32) + (data_section[imm + 5 + registers[rs1] - DATA_START] << 40);
                    registers[rd] += (data_section[imm + 6 + registers[rs1] - DATA_START] << 48) + (data_section[imm + 7 + registers[rs1] - DATA_START] << 56);
                }
                else if(funct3 == 0x4){
                    registers[rd] = data_section[imm + registers[rs1] - DATA_START];
                    registers[rd] = registers[rd] & 0xff;
                }
                else if(funct3 == 0x5){
                    registers[rd] = data_section[imm + registers[rs1] - DATA_START] + (data_section[imm + 1 + registers[rs1] - DATA_START] << 8);
                    registers[rd] = registers[rd] & 0xffff;
                }
                else if(funct3 == 0x6){
                    registers[rd] = data_section[imm + registers[rs1] - DATA_START] + (data_section[imm + 1 + registers[rs1] - DATA_START] << 8);
                    registers[rd] += (data_section[imm + 2 + registers[rs1] - DATA_START] << 16) + (data_section[imm + 3 + registers[rs1] - DATA_START] << 24);
                    registers[rd] = registers[rd] & 0xffffffff;
                }
            }
        }
        pc += 4;
    }
    else if(opcode == 0x67){    // jalr
        unsigned int funct3 = (instruction >> 12) & 0x7;
        unsigned int rs1 = (instruction >> 15) & 0x1f;
        unsigned int rd = (instruction >> 7) & 0x1f;
        long long int imm = (instruction >> 20);

        if(imm & 0x800){
            imm |= 0xfffff000;
        }

        if(rd != 0){
            registers[rd] = pc + 4;
        }

        pc = registers[rs1] + imm;
        pop_stack();
    }
    else if(opcode == 0x23){  // S-type
        unsigned funct3 = (instruction >> 12) & 0x7;
        unsigned rs1 = (instruction >> 15) & 0x1f;
        unsigned rs2 = (instruction >> 20) & 0x1f;
        unsigned imm_11_5 = (instruction >> 25) & 0x7f;  // 7 bits (imm[11:5])

        // Extract imm[4:0] from bits [11:7]
        int imm_4_0 = (instruction >> 7) & 0x1f;   // 5 bits (imm[4:0])
        // Combine imm[11:5] and imm[4:0] to form the 12-bit immediate
        long long imm = (imm_11_5 << 5) | imm_4_0;

        // Sign-extend the immediate (12 bits to 32 bits)
        if(imm & 0x800){  // Check if the sign bit (bit 11) is set
            imm |= 0xfffff000;  // Sign-extend by filling the upper 20 bits with 1s
        }
        long long address = registers[rs1] + imm;
        long long data = registers[rs2];
        if(cache_enabled){
            cache_accesses++;
            cache_write(address, data, funct3);  // Attempt cache write
        }
        else{
            if(funct3 == 0x0){  // SB (Store Byte)
                data_section[address - DATA_START] = registers[rs2] & 0xff;
            }
            else if(funct3 == 0x1){  // SH (Store Halfword)
                data_section[address - DATA_START] = registers[rs2] & 0xff;  // Store the least significant byte
                data_section[address + 1 - DATA_START] = (registers[rs2] >> 8) & 0xff;  // Store the next byte
            }
            else if(funct3 == 0x2){  // SW (Store Word)
                data_section[address - DATA_START] = registers[rs2] & 0xff;  // Store the least significant byte
                data_section[address + 1 - DATA_START] = (registers[rs2] >> 8) & 0xff;
                data_section[address + 2 - DATA_START] = (registers[rs2] >> 16) & 0xff;
                data_section[address + 3 - DATA_START] = (registers[rs2] >> 24) & 0xff;
            }
            else if(funct3 == 0x3){  // SD (Store Doubleword)
                data_section[address - DATA_START] = registers[rs2] & 0xff;  // Store the least significant byte
                data_section[address + 1 - DATA_START] = (registers[rs2] >> 8) & 0xff;
                data_section[address + 2 - DATA_START] = (registers[rs2] >> 16) & 0xff;
                data_section[address + 3 - DATA_START] = (registers[rs2] >> 24) & 0xff;
                data_section[address + 4 - DATA_START] = (registers[rs2] >> 32) & 0xff;
                data_section[address + 5 - DATA_START] = (registers[rs2] >> 40) & 0xff;
                data_section[address + 6 - DATA_START] = (registers[rs2] >> 48) & 0xff;
                data_section[address + 7 - DATA_START] = (registers[rs2] >> 56) & 0xff;
            }
        }
        pc += 4;  // Move to the next instruction
    }
    else if(opcode == 0x37){  // LUI instruction
        unsigned rd = (instruction >> 7) & 0x1f;
        int imm = instruction & 0xfffff000;  // Immediate is the upper 20 bits
        if(rd != 0) registers[rd] = imm;  // Load upper immediate value into the register
        pc += 4;
    }
    else if(opcode == 0x63){  // B-type (branch) instructions
        unsigned funct3 = (instruction >> 12) & 0x7;
        unsigned rs1 = (instruction >> 15) & 0x1f;
        unsigned rs2 = (instruction >> 20) & 0x1f;

        // Immediate is a combination of different instruction fields
        int imm = ((instruction >> 7) & 0x1E) | ((instruction >> 25) << 5) | ((instruction >> 8) & 0x1) | ((instruction & 0x80000000) ? 0xfffff000 : 0);

        if(funct3 == 0x0){  // BEQ
            if(registers[rs1] == registers[rs2]) pc += imm;    // Take the branch
            else pc += 4;  // Move to the next instruction
        } 
        else if(funct3 == 0x1){  // BNE
            if(registers[rs1] != registers[rs2]) pc += imm;  // Take the branch
            else pc += 4;
        }
        else if(funct3 == 0x4){  // BLT
            if(registers[rs1] < registers[rs2]) pc += imm;
            else pc += 4;
        }
        else if(funct3 == 0x5){  // BGE
            if(registers[rs1] >= registers[rs2]) pc += imm;
            else pc += 4;
        }
        else if(funct3 == 0x6){  // BLTU (unsigned comparison)
            if((unsigned int)registers[rs1] < (unsigned int)registers[rs2]) pc += imm;
            else pc += 4;
        }
        else if(funct3 == 0x7){  // BGEU (unsigned comparison)
            if((unsigned int)registers[rs1] >= (unsigned int)registers[rs2]) pc += imm;
            else pc += 4;
        }
    }
    else if(opcode == 0x6f){  // J-format: JAL (Jump and Link)
        unsigned rd = (instruction >> 7) & 0x1f;
        
        // Immediate encoding in JAL (31-12 bits for immediate)
        int imm = ((instruction >> 12) & 0xff) << 12;  // bits [19:12]
        imm |= ((instruction >> 20) & 0x1) << 11;      // bit [11]
        imm |= ((instruction >> 21) & 0x3ff) << 1;     // bits [10:1]
        imm |= ((instruction & 0x80000000) ? 0xfff00000 : 0);  // Sign extension

        // Store return address (pc + 4) into rd (if rd != 0)
        if(rd != 0){
            registers[rd] = pc + 4;
        }
        // Update pc with the jump target address
        pc += imm;

        for(int i=0;i<64;i++){
            if(labels[i].address == pc){
                push_stack(labels[i].name, 0);
                break;
            }
        }
    }
}
// Function to execute all pending instructions
void run(){
    printf("Running program...\n");
    int sen = 0;
    int break_pt = 0;
    while(pc < instr_count * 4){
        unsigned current_pc = pc;
        if(break_points[current_pc / 4 - 1]){
            break_pt = 1;
            break;
        }
        else{
            call_stack[stack_top].line_num = instruction_lines[current_pc / 4];
            execute_instruction(text_section[pc / 4]);
            printf("Executed instruction: %s PC = 0x%016lx\n", instructions[current_pc / 4], (long unsigned)current_pc);
        }
    }
    if(break_pt) printf("Execution stopped at break point\n");
    if((pc - TEXT_START) / 4 >= instr_count) sen = 1;
    if(sen){
        pop_stack();
        printf("%d %d %d\n", cache_accesses, cache_hits, cache_misses);
    }
    else if(!break_pt) printf("No more instructions left to execute\n");
}
// Function to execute current instruction
void step(){
    if(pc < 0x10000 && (pc - TEXT_START) / 4 < instr_count){
        unsigned current_pc = pc;
        call_stack[stack_top].line_num = instruction_lines[current_pc / 4];
        execute_instruction(text_section[pc / 4]);
        printf("Executed instruction: %s PC = 0x%016lx\n", instructions[current_pc / 4], (long unsigned)current_pc);
        if((pc - TEXT_START) / 4 >= instr_count){
            pop_stack();
        } 
    }
    else printf("Nothing to step.\n");
}
// Function to dsiplay values of all registers
void print_registers(){
    printf("Register values:\n");
    for(int i = 0; i < NUM_REGS; i++){
        printf("x%d: 0x%016llx\n", i, registers[i]);
    }
}
// Function to end program
void exit_simulator(){
    printf("Exiting simulator.\n");
    exit(0);
}
// Function to print memory values in byte format
void print_mem_at_address(unsigned start_address, unsigned count){
    if(start_address < DATA_START || start_address >= stack_pointer){
        printf("Error: Start address out of bounds\n");
        return;
    }

    unsigned int offset = start_address - DATA_START;
    if(offset + count > STACK_START - DATA_START){
        printf("Error: Requested memory range out of bounds\n");
        return;
    }

    // Print count number of bytes starting from the start_address
    printf("Memory content from address 0x%08x:\n", start_address);
    for(int i = 0; i < count; i++){
        printf("0x%02llx\n", data_section[offset + i]);
    }
}
// Function to handle all input commands from user
void handle_command(char *command){
    char cmd[strlen(command)];
    char command_copy[strlen(command)];
    strcpy(command_copy, command);
    sscanf(command, "%s", cmd);
    if(strcmp(cmd, "load") == 0){
        sscanf(command + strlen(cmd), "%s", filename);
        load(filename);
    }
    else if(strcmp(cmd, "run") == 0) run();
    else if(strcmp(cmd, "step") == 0) step();
    else if(strcmp(cmd, "regs") == 0) print_registers();
    else if(strcmp(cmd, "exit") == 0) exit_simulator();
    else if(strcmp(cmd, "show-stack") == 0) show_stack();
    else if(strcmp(cmd, "mem") == 0){
        char* util = strtok(command_copy, " ,");
        char* address_str = strtok(NULL, " ,");
        char* count_str = strtok(NULL, " ,\n");
        if(address_str == NULL || count_str == NULL) printf("Missing arguments\n");
        else{
            unsigned int address, count;
            sscanf(command + strlen(cmd), "%x %u", &address, &count);
            print_mem_at_address(address, count);
            free(address_str);
            free(count_str);
        }
    }
    else if(strcmp(cmd, "break") == 0){
        char* if_break = strtok(command_copy, " ");
        char* break_line = strtok(NULL, " \n");
        if(break_line == NULL) printf("Missing arguments\n");
        else{
            int break_line_num = atoi(break_line);
            if(break_line_num > 0) break_points[break_line_num - 1] = 1;
            fprintf(stdout, "Break point set at line %d\n", break_line_num);
        }
    }
    else if(strcmp(cmd, "delete") == 0 || strcmp(cmd, "del") == 0){
        char* to_delete = strtok(command_copy, " ");
        char* if_break = strtok(NULL, " ");
        char* break_line = strtok(NULL, " \n");
        if(break_line == NULL || if_break == NULL) printf("Missing arguments\n");
        else{
            int break_line_num = atoi(break_line);
            if(break_points[break_line_num - 1]){
                if(break_line_num > 0) break_points[break_line_num - 1] = 0;
            }
            else fprintf(stdout, "No break point found at line %d\n", break_line_num);
        }
    }
    else if(strcmp(cmd, "cache_sim") == 0){
        char operation[10], filename[256];
        int parsed_items = sscanf(command + strlen(cmd), "%s %s", operation, filename);

        if(parsed_items >= 1 && strcmp(operation, "enable") == 0){
            if (parsed_items < 2) printf("Usage: cache_sim enable <file_name>\n");
            else enable_cache(filename);

            char *filename_without_ext;

            // Find the position of the last dot
            char *dot = strrchr(filename, '.');

            if(dot){
                // Null-terminate the string at the dot to isolate the filename
                *dot = '\0';
                filename_without_ext = filename;
            }
            else filename_without_ext = filename;

            // Create the new filename with the ".output" extension
            char new_filename[100];
            sprintf(new_filename, "%s.output", filename_without_ext);

            // You can now use new_filename to create the file
            cache_stat_ptr = fopen(new_filename, "w");
        }
        else if(parsed_items >= 1 && strcmp(operation, "disable") == 0) disable_cache();
        else if (parsed_items >= 1 && strcmp(operation, "status") == 0) print_cache_status();
        else fprintf(stdout, "Usage: cache_sim <enable/disable/status> [file_name]\n");
    }
    else fprintf(stdout, "Unknown command: %s\n", command);
}
// Main function
int main(){ 
    char command[256];

    printf("RISC-V Simulator\n");
    while(1){
        printf("> ");
        fgets(command, sizeof(command), stdin);
        handle_command(command);
    }
    return 0;
}