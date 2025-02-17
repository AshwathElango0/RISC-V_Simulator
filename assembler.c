#include<stdio.h>  // Necessary header files imported
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<math.h>
#include "assembler.h"

int line_num = 0;

label_info labels[MAX_LINES];
int label_count = 0; // Count of labels

typedef struct{
    char alias[5];  // Register alias (e.g., zero, ra)
    int num;        // Corresponding register number (e.g., 0 for zero, 1 for ra)
} reg_alias;

reg_alias reg_aliases[] = {
    {"zero", 0}, {"ra", 1}, {"sp", 2}, {"gp", 3}, {"tp", 4},
    {"t0", 5}, {"t1", 6}, {"t2", 7}, {"s0", 8}, {"fp", 8},
    {"s1", 9}, {"a0", 10}, {"a1", 11}, {"a2", 12}, {"a3", 13},
    {"a4", 14}, {"a5", 15}, {"a6", 16}, {"a7", 17}, {"s2", 18},
    {"s3", 19}, {"s4", 20}, {"s5", 21}, {"s6", 22}, {"s7", 23},
    {"s8", 24}, {"s9", 25}, {"s10", 26}, {"s11", 27}, {"t3", 28},
    {"t4", 29}, {"t5", 30}, {"t6", 31}
};

typedef struct{
    char name[4];      // Instruction name (e.g., add, sub)
    char funct3[4];    // funct3 field (3 bits)
    char funct7[8];    // funct7 field (7 bits)
} r_format;

r_format r_instructions[] = {   // Struct array for R format instructions
    {"add", "000", "0000000"},
    {"sub", "000", "0100000"},
    {"and", "111", "0000000"},
    {"or",  "110", "0000000"},
    {"xor", "100", "0000000"},
    {"sll", "001", "0000000"},
    {"srl", "101", "0000000"},
    {"sra", "101", "0100000"}
};
// Similarly, struct arrays for all formats of instructions
typedef struct{
    char name[5];
    char funct3[4];
    char opcode[8];
} i_format;

i_format i_instructions[] = {
    {"addi", "000", "0010011"},
    {"andi", "111", "0010011"},
    {"ori",  "110", "0010011"},
    {"srai", "101", "0010011"},
    {"slli", "001", "0010011"},
    {"srli", "101", "0010011"},
    {"xori", "100", "0010011"},
};

i_format i_part_2[] = {
    {"lb", "000", "0000011"},
    {"lh", "001", "0000011"},
    {"lw", "010", "0000011"},
    {"ld", "011", "0000011"},
    {"lbu", "100", "0000011"},
    {"lhu", "101", "0000011"},
    {"lwu", "110", "0000011"},
    {"jalr", "000", "1100111"}
};

typedef struct{
    char name[3];
    char funct3[4];
} s_format;

s_format s_instructions[] = {
    {"sb", "000"},
    {"sh", "001"},
    {"sw", "010"},
    {"sd", "011"}
};

typedef struct{
    char name[5];       // Instruction name
    char funct3[4];     // funct3 field for B-format instructions
} b_format;

// Initialize B-format instructions
b_format b_instructions[] = {
    {"beq", "000"},
    {"bne", "001"},
    {"blt", "100"},
    {"bge", "101"},
    {"bltu", "110"},
    {"bgeu", "111"}
};

typedef struct{
    char name[4];
    char funct3[4];
} j_format;

j_format j_instructions[] = {
    {"jal", "000"}
};

typedef struct{
    char name[4];
    char opcode[8];
} u_format;

u_format u_instructions[] = {
    {"lui", "0110111"}
};
// Function to convert registers to binary form
int reg_to_bin(char* reg, char* bin){
    int sen = 0;
    int reg_num = -1;

    // Check if reg is a numeric register (e.g., x0, x1)
    if(reg[0] == 'x'){
        int check = 1;
        for(int i=1; reg[i] != '\0'; i++){
            if(!isdigit(reg[i])){
                check = 0;
                break;
            }
        }
        if(check){
            reg_num = atoi(reg + 1);
            sen = 1;
        }  // Skip the 'x' and convert to integer
        
    }
    else{
        // Check if reg is an alias
        for(int i = 0; i < sizeof(reg_aliases) / sizeof(reg_alias); i++){
            if(strcmp(reg, reg_aliases[i].alias) == 0){
                reg_num = reg_aliases[i].num;
                sen = 1;
                break;
            }
        }
    }

    // Convert reg_num to binary
    if(reg_num != -1){
        for (int i = 4; i >= 0; i--) {
            bin[i] = (reg_num % 2) + '0';
            reg_num /= 2;
        }
        bin[5] = '\0';
        return sen;
    }
    else{
        // Handle error: unknown register
        return sen;
    }
}

// Function to convert a binary string to a hexadecimal string
void bin_to_hex(const char* bin, char* hex){
    int value = 0;
    for(int i = 0; i < 32; i++){
        value = (value << 1) + (bin[i] - '0');
        if((i + 1) % 4 == 0){
            sprintf(hex + (i / 4), "%x", value);
            value = 0;
        }
    }
    hex[8] = '\0';  // Ensure the hex string is null-terminated
}

// Function to handle R format instructions
int r_cmds(char* inst_name, char* rd, char* rs1, char* rs2, FILE* optr, int line_num){
    const char* r_opcode = "0110011";  // Common opcode for all R-format instructions

    for(int i = 0; i < sizeof(r_instructions) / sizeof(r_format); i++){
        if (strcmp(inst_name, r_instructions[i].name) == 0) {
            char rd_bin[6];
            char rs1_bin[6];
            char rs2_bin[6];
            int c1 = reg_to_bin(rd, rd_bin);
            int c2 = reg_to_bin(rs1, rs1_bin);
            int c3 = reg_to_bin(rs2, rs2_bin);

            if(c1 && c2 && c3){
                char machine_code_bin[33];
                snprintf(machine_code_bin, sizeof(machine_code_bin), "%s%s%s%s%s%s", r_instructions[i].funct7, rs2_bin, rs1_bin, r_instructions[i].funct3, rd_bin, r_opcode);

                char machine_code_hex[9];
                bin_to_hex(machine_code_bin, machine_code_hex);
                fprintf(optr, "%s\n", machine_code_hex);    // Writing assembled output onto output file
                return 1;
            }
            else{
                fprintf(stderr, "Error at line %d\nUnknown register encountered", line_num); // Error handling
                return 0;
            }
        }
    }
}
// Function to handle first type of I format instructions
int i_cmds_1(char* instr_name, char* rd, char* rs1, char* imm, FILE* optr, int line_num){
    for(int i = 0; i < sizeof(i_instructions) / sizeof(i_format); i++){
        if(strcmp(instr_name, i_instructions[i].name) == 0){
            char rd_bin[6];
            char rs1_bin[6];
            int c3 = reg_to_bin(rd, rd_bin);
            int c1 = reg_to_bin(rs1, rs1_bin);
            int imm_val = -1;
            // Convert immediate value to binary (12 bits)
            if(imm_val == -1){
                imm_val = atoi(imm);
                if(imm_val < - 2048 || imm_val > 2047){
                    fprintf(stderr, "Error at line %d\nImmediate value needs to lie between -2048 and 2047", line_num);
                    return 0;
                }
                if(imm_val < 0){
                    imm_val = (1 << 12) + imm_val;  // Convert to 2's complement for 12-bit value if negative
                }                    
            }
            
            char imm_bin[13];   // Converting immediate to binary string
            for(int j = 11; j >= 0; j--){
                imm_bin[j] = (imm_val % 2) + '0';
                imm_val /= 2;
            }
            imm_bin[12] = '\0';
            // Handling special cases and encoding accordingly
            if(c1 && c3){
                const char* funct6 = "000000";
                char machine_code_bin[33];
                if(strcmp(instr_name, "srai") == 0){
                    funct6 = "010000";
                    snprintf(machine_code_bin, sizeof(machine_code_bin), "%s%s%s%s%s%s", 
                        funct6, imm_bin + 6, rs1_bin, i_instructions[i].funct3, rd_bin, i_instructions[i].opcode);
                }
                else if(strcmp(instr_name, "srli") == 0 || strcmp(instr_name, "slli") == 0){
                    snprintf(machine_code_bin, sizeof(machine_code_bin), "%s%s%s%s%s%s", 
                        funct6, imm_bin + 6, rs1_bin, i_instructions[i].funct3, rd_bin, i_instructions[i].opcode);
                }
                else{
                    snprintf(machine_code_bin, sizeof(machine_code_bin), "%s%s%s%s%s", 
                        imm_bin, rs1_bin, i_instructions[i].funct3, rd_bin, i_instructions[i].opcode);
                }
                
                // Converting the binary machine code to hexadecimal
                char machine_code_hex[9];
                bin_to_hex(machine_code_bin, machine_code_hex);
                fprintf(optr, "%s\n", machine_code_hex);
                return 1;
            }
            else{
                fprintf(stderr, "Error at line %d\nUnknown register encountered", line_num); // Error handling
                return 0;
            }
        }
    }
}
// Second type of I format instructions
int i_cmds_2(char* inst_name, char* rs2, char* rs1_or_offset, char* optional_rs1, FILE* optr, int line_num){
    for(int i = 0; i < sizeof(i_part_2) / sizeof(i_format); i++){
        if(strcmp(inst_name, i_part_2[i].name) == 0){
            char* i_opcode = i_part_2[i].opcode;
            char rs1_bin[6];
            char rs2_bin[6];
            int c2 = reg_to_bin(rs2, rs2_bin);

            // Handle "offset(register)" format
            if(strchr(rs1_or_offset, '(')){
                char* offset = strtok(rs1_or_offset, "(");
                if(isdigit(offset[0]) || offset[0] == '-'){
                    char* rs1 = strtok(NULL, ")");
                    if(rs1 != NULL){
                        int c1 = reg_to_bin(rs1, rs1_bin);
                        if(c1 && c2){
                            int imm_val = atoi(offset);
                            if(imm_val >= -2048 && imm_val <= 2047){ // Valid 12-bit signed range
                                char imm_bin[13];
                                if(imm_val < 0){
                                    imm_val = (1 << 12) + imm_val;  // Convert to 2's complement for 12-bit value
                                }
                                for(int j = 11; j >= 0; j--){
                                    imm_bin[j] = (imm_val % 2) + '0';
                                    imm_val /= 2;
                                }
                                imm_bin[12] = '\0';

                                // Build the machine code in binary
                                char machine_code_bin[33];
                                snprintf(machine_code_bin, sizeof(machine_code_bin), "%s%s%s%s%s", imm_bin, rs1_bin, i_part_2[i].funct3, rs2_bin, i_part_2[i].opcode);

                                // Convert the binary machine code to hexadecimal
                                char machine_code_hex[9];
                                bin_to_hex(machine_code_bin, machine_code_hex);
                                fprintf(optr, "%s\n", machine_code_hex);
                                return 1;
                            }
                            else{   //Error handling
                                fprintf(stderr, "Error at line %d\nOffset out of valid range (-2048 to 2047)", line_num);
                                return 0;
                            }
                        }
                        else{
                            fprintf(stderr, "Error at line %d\nUnknown register encountered", line_num);
                            return 0;
                        }
                    } 
                    else{
                        fprintf(stderr, "Error at line %d\nMissing register", line_num);
                        return 0;
                    }
                }
                else{
                    fprintf(stderr, "Error at line %d\nInvalid offset: expected a numeric value", line_num);
                    return 0;
                }
            }

            // Handle "register offset register" format
            else if(optional_rs1 != NULL && (isdigit(rs1_or_offset[0]) || rs1_or_offset[0] == '-')){
                char* rs1 = optional_rs1;
                int c1 = reg_to_bin(rs1, rs1_bin);
                int imm_val = atoi(rs1_or_offset);
                if(c1 && c2 && imm_val >= -2048 && imm_val <= 2047){
                    // Convert imm_val to binary, handling negative values using 2's complement
                    if (imm_val < 0) {
                        imm_val = (1 << 12) + imm_val;  // Convert to 2's complement for 12-bit value
                    }

                    char imm_bin[13];
                    for(int j = 11; j >= 0; j--){
                        imm_bin[j] = (imm_val % 2) + '0';
                        imm_val /= 2;
                    }
                    imm_bin[12] = '\0';

                    // Build the machine code in binary
                    char machine_code_bin[33];
                    snprintf(machine_code_bin, sizeof(machine_code_bin), "%s%s%s%s%s", imm_bin, rs1_bin, i_part_2[i].funct3, rs2_bin, i_opcode);

                    // Convert binary to hexadecimal
                    char machine_code_hex[9];
                    bin_to_hex(machine_code_bin, machine_code_hex);
                    fprintf(optr, "%s\n", machine_code_hex);
                    return 1;
                }
                else{
                    fprintf(stderr, "Error at line %d\nInvalid offset or register encountered", line_num);
                    return 0;
                }
            }

            // If neither format matches
            else{
                fprintf(stderr, "Error at line %d\nInvalid format: expected 'offset(register)' or 'register offset register'", line_num);
                return 0;
            }
        }
    }

    // Instruction not found
    fprintf(stderr, "Error at line %d\nUnknown instruction encountered", line_num);
    return 0;
}
// Function to handle S format instructions
int s_cmds(char* inst_name, char* rs2, char* rs1_or_offset, char* optional_rs1, FILE* optr, int line_num){
    const char* s_opcode = "0100011";

    for(int i = 0; i < sizeof(s_instructions) / sizeof(s_format); i++){
        if(strcmp(inst_name, s_instructions[i].name) == 0){
            char rs1_bin[6] = {0};
            char rs2_bin[6] = {0};
            int c2 = reg_to_bin(rs2, rs2_bin);

            // Handle "offset(register)" format
            if(strchr(rs1_or_offset, '(')){
                char* offset = strtok(rs1_or_offset, "(");
                if(isdigit(offset[0]) || offset[0] == '-'){
                    char* rs1 = strtok(NULL, ")");
                    if(rs1 != NULL){
                        int c1 = reg_to_bin(rs1, rs1_bin);
                        if(c1 && c2){
                            int imm_val = atoi(offset);
                            if(imm_val >= -2048 && imm_val <= 2047){ // Valid 12-bit signed range
                                char imm_bin[13];
                                if(imm_val < 0){
                                    imm_val = (1 << 12) + imm_val;  // Convert to 2's complement for 12-bit value
                                }
                                for(int j = 11; j >= 0; j--){
                                    imm_bin[j] = (imm_val % 2) + '0';
                                    imm_val /= 2;
                                }
                                imm_bin[12] = '\0';

                                // Split imm_bin into imm[11:5] and imm[4:0]
                                char imm_high[8], imm_low[6];
                                strncpy(imm_high, imm_bin, 7);  // imm[11:5]
                                imm_high[7] = '\0';
                                strncpy(imm_low, imm_bin + 7, 5);  // imm[4:0]
                                imm_low[5] = '\0';

                                // Build the machine code in binary
                                char machine_code_bin[33];
                                snprintf(machine_code_bin, sizeof(machine_code_bin), "%s%s%s%s%s%s", imm_high, rs2_bin, rs1_bin, s_instructions[i].funct3, imm_low, s_opcode);

                                // Convert binary to hexadecimal
                                char machine_code_hex[9];
                                bin_to_hex(machine_code_bin, machine_code_hex);
                                fprintf(optr, "%s\n", machine_code_hex);
                                return 1;
                            }
                            else{
                                fprintf(stderr, "Error at line %d\nOffset out of valid range (-2048 to 2047)", line_num);
                                return 0;
                            }
                        }
                        else{
                            fprintf(stderr, "Error at line %d\nUnknown register encountered", line_num);
                            return 0;
                        }
                    }
                    else{
                        fprintf(stderr, "Error at line %d\nMissing register", line_num);
                        return 0;
                    }
                }
                else{
                    fprintf(stderr, "Error at line %d\nInvalid offset: expected a numeric value", line_num);
                    return 0;
                }
            }

            // Handle "register offset register" format
            else if(optional_rs1 != NULL && (isdigit(rs1_or_offset[0]) || rs1_or_offset[0] == '-')){
                char* rs1 = optional_rs1;
                int c1 = reg_to_bin(rs1, rs1_bin);
                int imm_val = atoi(rs1_or_offset);
                
                if(c1 && c2 && imm_val >= -2048 && imm_val <= 2047){
                    // Convert imm_val to binary, handling negative values using 2's complement
                    if (imm_val < 0) {
                        imm_val = (1 << 12) + imm_val;  // Convert to 2's complement for 12-bit value
                    }

                    char imm_bin[13];
                    for (int j = 11; j >= 0; j--) {
                        imm_bin[j] = (imm_val % 2) + '0';
                        imm_val /= 2;
                    }
                    imm_bin[12] = '\0';
                    // Split imm_bin into imm[11:5] and imm[4:0]
                    char imm_high[8], imm_low[6];
                    strncpy(imm_high, imm_bin, 7);  // imm[11:5]
                    imm_high[7] = '\0';
                    strncpy(imm_low, imm_bin + 7, 5);  // imm[4:0]
                    imm_low[5] = '\0';

                    // Build the machine code in binary
                    char machine_code_bin[33];
                    snprintf(machine_code_bin, sizeof(machine_code_bin), "%s%s%s%s%s%s",
                            imm_high, rs2_bin, rs1_bin, s_instructions[i].funct3, imm_low, s_opcode);
                
                    // Convert binary to hexadecimal
                    char machine_code_hex[9];
                    bin_to_hex(machine_code_bin, machine_code_hex);
                    fprintf(optr, "%s\n", machine_code_hex);
                    return 1;
                }
                else{
                    fprintf(stderr, "Error at line %d\nInvalid offset or register encountered", line_num);
                    return 0;
                }
            }

            // If neither format matches
            else{
                fprintf(stderr, "Error at line %d\nInvalid format: expected 'offset(register)' or 'register offset register'", line_num);
                return 0;
            }
        }
    }

    // Instruction not found
    fprintf(stderr, "Error at line %d\nUnknown instruction %s encountered", line_num, inst_name);
    return 0;
}
// Function to handle B format instructions
int b_cmds(char* inst_name, char* rs1, char* rs2, char* offset, FILE* optr, int line_num){
    const char* b_opcode = "1100011";

    for(int i = 0; i < sizeof(b_instructions) / sizeof(b_format); i++){
        if(strcmp(inst_name, b_instructions[i].name) == 0){
            char rs1_bin[6];
            char rs2_bin[6];
            int c1 = reg_to_bin(rs1, rs1_bin);
            int c2 = reg_to_bin(rs2, rs2_bin);
            if(c1 && c2){
                int offset_val = atoi(offset) >> 1; // Right shift by 1
                // Generate binary representation of the offset
                char offset_bin[13];
                if(offset_val < 0){
                    offset_val = (1 << 12) + offset_val; // Handle negative offset
                }
                for(int j = 11; j >= 0; j--){
                    offset_bin[j] = (offset_val % 2) + '0';
                    offset_val /= 2;
                }
                offset_bin[12] = '\0';
                char machine_code_bin[33];
                // Split offset_bin into components
                char imm_11 = offset_bin[1]; // imm[11] -> bit 31
                char imm_4_1[5];
                strncpy(imm_4_1, offset_bin + 8, 4);
                imm_4_1[4] = '\0'; // imm[4:1] -> bits 11:8
                char imm_10_5[7];
                strncpy(imm_10_5, offset_bin + 2, 7);
                imm_10_5[6] = '\0'; // imm[10:5] -> bits 30:25
                char imm_12 = offset_bin[0];

                // Assemble the final machine code
                sprintf(machine_code_bin, "%c%s%s%s%s%s%c%s", imm_12, imm_10_5, rs2_bin, rs1_bin, b_instructions[i].funct3, imm_4_1, imm_11, b_opcode);
                // Convert to hex
                char machine_code_hex[9];
                bin_to_hex(machine_code_bin, machine_code_hex);
                fprintf(optr, "%s\n", machine_code_hex);
                return 1;
                break;
            }
            else{   // Error handling
                fprintf(stderr, "Unknown register found\n in line %d", line_num);
                return 0;
                break;
            }
            
        }
    }
}
// Function to handle J format instructions
int j_cmds(char* inst_name, char* rd, char* offset_str, FILE* optr, int line_num){
    const char* j_opcode = "1101111";
    char rd_bin[6];
    int c1 = reg_to_bin(rd, rd_bin);
    if(c1){
        int offset_val = atoi(offset_str) >> 1; // Right shift by 1
        // Generate binary representation of the offset
        char offset_bin[21];
        if(offset_val < 0){
            offset_val = (1 << 20) + offset_val; // Handle negative offset
        }
        for(int j = 19; j >= 0; j--){
            offset_bin[j] = (offset_val % 2) + '0';
            offset_val /= 2;
        }
        offset_bin[20] = '\0';
        char machine_code_bin[33];
        char imm_20 = offset_bin[0];
        char imm_19_12[9];
        strncpy(imm_19_12, offset_bin + 1, 8);
        imm_19_12[8] = '\0';
        char imm_11 = offset_bin[8];
        char imm_10_1[11];
        strncpy(imm_10_1, offset_bin + 10, 10);
        imm_10_1[10] = '\0';
        // Assembling the machine code with the encoding rule for J format instructions
        snprintf(machine_code_bin, 33, "%c%s%c%s%s%s", imm_20, imm_10_1, imm_11, imm_19_12, rd_bin, j_opcode);
        char machine_code_hex[9];
        bin_to_hex(machine_code_bin, machine_code_hex);
        fprintf(optr, "%s\n", machine_code_hex);    // Writing onto output file
        return 1;
    }
    else{   // Error handling
        fprintf(stderr, "Error in line %d\nUnknown register found\n", line_num);
        return 0;
    }
    
}
// Function to handle U format instructions
int u_cmds(char* rd, char* imm, char* opcode, FILE* optr, int line_num){
    int imm_val = atoi(imm);    // Obtaining numeric immediate
    char imm_bin[21];
    char rd_bin[6];  
    int cd = reg_to_bin(rd, rd_bin);
    if(imm_val >= 0 && imm_val < 1048576){
        if(cd){
            for(int j = 19; j >= 0; j--){   // Converting the immediate to bit string
                imm_bin[j] = (imm_val % 2) + '0';
                imm_val /= 2;
            }
            imm_bin[20] = '\0';
            char machine_code_bin[33];
            strncpy(machine_code_bin, imm_bin, 20);
            strncpy(machine_code_bin + 20, rd_bin, 5);
            strncpy(machine_code_bin + 25, opcode, 7);
            machine_code_bin[32] = '\0';    // Completing assembly of machine code in binary form
            char machine_code_hex[9];
            bin_to_hex(machine_code_bin, machine_code_hex);     // Writing machine code on output file
            fprintf(optr, "%s\n", machine_code_hex);
            return 1;
        }
        else{
            fprintf(stderr, "Error in line%d\nUnknown register found\n", line_num);    // Error handling
            return 0;
        }
    }
    else{
        fprintf(stderr, "Error in line %d\nImmediate value has to be >=0 and < 2^ 20", line_num);
        return 0;
    }
}
// Function to read the input file and identify labels and their positions
int parse_labels(FILE* fptr){
    /*
    This function reads the entire input file and stores information about all labelled lines
    Invalid labels are identified and reported, execution is ended gracefully
    */
    char line[MAX_LINE_LEN];
    int address = 0;
    int line_num = 0;
    while(fgets(line, sizeof(line), fptr)){
        line_num++;
        if(line[0] == '\n' || line[0] == ';' || line[0] == '.' || line[0] == '#') continue;   // Accounting for empty lines and comments
        char* label_end = strchr(line, ':');
        if(label_end){
            *label_end = '\0';  // Terminate the label part
            char* label = line;
            // Remove any extra whitespace from the label
            while(*label == ' ' || *label == '\t'){
                label++;
            }
            // Remove any trailing newline or whitespace from the label
            char* end = label + strlen(label) - 1;
            while(end > label && (*end == ' ' || *end == '\t' || *end == '\n')){
                end--;
            }
            *(end + 1) = '\0';

            // Store the label
            strcpy(labels[label_count].name, label);
            // Invalid labels are identified
            for(int i=0;i<strlen(labels[label_count].name);i++){
                if(labels[label_count].name[i] == '(' || labels[label_count].name[i] == ')' || labels[label_count].name[i] == ','){
                    printf("Invalid label %s in line %d\n", labels[label_count].name, line_num);
                    return 0;
                }
            }
            labels[label_count].address = address;
            label_count++;
        }
        address += 4; // Increment address by 4 bytes for each instruction
    }
    return 1;
}
// Function to read the input file again and encode instructions one by one
void process_instructions(FILE* fptr, FILE* optr){
    /*
    If a line has excess tokens, they are ignored and the code tries to make use of the required number of tokens and generate machine code
    Example: add x0 x0 x0 74....the 74 is ignored, and the add x0 x0 x0 is encoded
    If an instruction is missing arguments, it will be identified and reported as an error
    */

    int current_address = 0; // Track the current address while processing
    char line[MAX_LINE_LEN];
    int sen = 1;
    while(fgets(line, sizeof(line), fptr) && sen){
        // Accounting for empty lines and comments
        line_num++; // Keeping track of line numbers for error handling
        if(line[0] == '\n') continue;
        if(line[0] == ';' || line[0] == '#' || line[0] == '.') continue;
        // Checking for and removing the labels (if any)
        int handled = 0;
        char* label_end = strchr(line, ':');
        char* command_part = line;

        if(label_end){
            *label_end = '\0';  // Terminate the label part
            command_part = label_end + 1; // Command part starts after the colon
        }

        // Remove any extra whitespaces from the command part
        while(*command_part == ' ' || *command_part == '\t'){
            command_part++;
        }

        // Tokenize the command part
        char* instr_name = strtok(command_part, ", \t\n");
        if(instr_name == NULL) continue;    // Ignoring lines with only labels and no instructions
        if(strcmp(instr_name, "jal") == 0){ //Identifying jal instruction
            char* rd = strtok(NULL, ", \t\n");
            if(rd == NULL){
                fprintf(stderr, "Error in line %d\nMissing argument(s)\n", line_num);
                break;
            }
            char* linked_label = strtok(NULL, ", \t\n");
            if(linked_label == NULL){
                fprintf(stderr, "Error in line %d\nMissing argument(s)\n", line_num);
                break;
            }
            char* dummy = strtok(NULL, ", \t\n");
            if(dummy != NULL) printf("Warning: Excess tokens detected in line %d...ignoring and proceeding with execution\n", line_num);
            if(rd != NULL && linked_label != NULL){
                if(rd[strlen(rd) - 1] == ')' && rd[0] == '('){
                    rd[strlen(rd) - 1] = '\0';
                    rd++;
                }
                if(linked_label[strlen(linked_label) - 1] == ')' && linked_label[0] == '('){
                    linked_label[strlen(linked_label) - 1] = '\0';
                    linked_label++;
                }
            }
            int offset_val = 0;
            int label_found = 0;
            // Identifying byte offset using label address
            for(int j = 0; j < label_count; j++){
                if(strcmp(linked_label, labels[j].name) == 0){
                    offset_val = labels[j].address - current_address;
                    label_found = 1;
                    break;
                }
            }
            // Handling missing labels
            if(!label_found){
                fprintf(stderr, "Error in line %d: Label '%s' not found.\n", line_num, linked_label);
                break;
            }

            char offset_str[12];
            sprintf(offset_str, "%d", offset_val);
            
            int check = j_cmds(instr_name, rd, offset_str, optr, current_address);
            if(!check) break;
            current_address += 4;
            continue;
        }
        // Handling U format instructions
        for(int i = 0; i < sizeof(u_instructions) / sizeof(u_format); i++){
            if(strcmp(instr_name, u_instructions[i].name) == 0){
                char* rd = strtok(NULL, ", \t\n");
                if(rd == NULL){
                    fprintf(stderr, "Error in line %d\nMissing argument(s)\n", line_num);
                    handled = 1;
                    sen = 0;
                    break;
                }
                
                char* imm_str = strtok(NULL, ", \t\n");
                if(imm_str == NULL){
                    fprintf(stderr, "Error in line %d\nMissing argument(s)\n", line_num);
                    handled = 1;
                    sen = 0;
                    break;
                }
                // Handling cases where immediate is in parentheses
                if(imm_str[0] == '(' && imm_str[strlen(imm_str) - 1] == ')'){
                    imm_str[strlen(imm_str) - 1] = '\0';
                    imm_str++;
                }

                // Check for excess tokens
                char* dummy = strtok(NULL, ", \t\n");
                if(dummy != NULL) printf("Warning: Excess tokens detected in line %d...ignoring and proceeding with execution\n", line_num);
                
                // Convert the immediate value (hex or decimal)
                long imm_value;
                if(strncasecmp(imm_str, "0x", 2) == 0){
                    imm_value = strtol(imm_str, NULL, 16);  // Converting hexadecimals to long ints
                }
                else{
                    imm_value = strtol(imm_str, NULL, 10);  // Converting decimals to long ints
                }

                // Handle rd similarly if it's surrounded by parentheses
                if(rd[strlen(rd) - 1] == ')' && rd[0] == '('){
                    rd[strlen(rd) - 1] = '\0';
                    rd++;
                }

                char imm_buffer[21];
                sprintf(imm_buffer, "%ld", imm_value);  // Converting immediate back to string
                
                int check = u_cmds(rd, imm_buffer, u_instructions[i].opcode, optr, line_num);
                if(!check){
                    sen = 0;
                    break;
                }
                else{
                    handled = 1;
                }
            }
        }
        if(sen == 0) break;
        if(handled){
            current_address += 4;
            continue;
        }
        // Proceeding with line if it is not J format or U format
        char* rd_or_rs2 = strtok(NULL, ", \t\n");
        if(rd_or_rs2 == NULL){
            fprintf(stderr, "Error in line %d\nMissing argument(s)\n", line_num);
            break;
        }
        if(rd_or_rs2[strlen(rd_or_rs2) - 1] == ')' && rd_or_rs2[0] == '('){
            rd_or_rs2[strlen(rd_or_rs2) - 1] = '\0';
            rd_or_rs2++;
        }
        

        char* rs1_or_offset = strtok(NULL, ", \t\n");
        if(rs1_or_offset == NULL){
            fprintf(stderr, "Error in line %d\nMissing argument(s)\n", line_num);
            break;
        }
        if(rs1_or_offset[strlen(rs1_or_offset) - 1] == ')' && rs1_or_offset[0] == '('){
            rs1_or_offset[strlen(rs1_or_offset) - 1] = '\0';
            rs1_or_offset++;
        }
        
        char* rs2_or_offset = strtok(NULL, ", \t\n");
        if(rs2_or_offset != NULL){
            if(rs2_or_offset[strlen(rs2_or_offset) - 1] == ')' && rs2_or_offset[0] == '('){
                rs2_or_offset[strlen(rs2_or_offset) - 1] = '\0';
                rs2_or_offset++;
            }
        }

        long imm_value;
        if(strncasecmp(rs2_or_offset, "0x", 2) == 0){
            imm_value = strtol(rs2_or_offset, NULL, 16);  // Converting hexadecimals to long ints
        }
        else{
            imm_value = strtol(rs2_or_offset, NULL, 10);  // Converting decimals to long ints
        }

        char imm_buffer[13];
        sprintf(imm_buffer, "%ld", imm_value);  // Converting immediate back to string

        char* dummy = strtok(NULL, ", \t\n");
        if(dummy != NULL) printf("Warning: Excess tokens detected in line %d...ignoring and proceeding with execution\n", line_num);
        // Handling S format instructions of the form instr name rs2 offset instr 1
        if(!handled && rs2_or_offset != NULL && isdigit(rs1_or_offset[0])){
            for(int i = 0; i < sizeof(s_instructions) / sizeof(s_format); i++){
                if(strcmp(instr_name, s_instructions[i].name) == 0){
                    // Pass rs2, offset (as rs1_or_offset), and rs1 to s_cmds
                    int check = s_cmds(instr_name, rd_or_rs2, rs1_or_offset, imm_buffer, optr, current_address);
                    if(!check){
                        sen = 0;
                    }
                    handled = 1;
                    break;
                }
            }
        }

        // Process R-format instructions
        if(!handled){
            for (int i = 0; i < sizeof(r_instructions) / sizeof(r_format); i++) {
                if(strcmp(instr_name, r_instructions[i].name) == 0){
                    if(rs2_or_offset == NULL){
                        fprintf(stderr, "Error in line %d\nMissing argument(s)\n", line_num);
                        sen = 0;
                        handled = 1;
                        break;
                    }
                    int check = r_cmds(instr_name, rd_or_rs2, rs1_or_offset, rs2_or_offset, optr, line_num);
                    if(!check){
                        sen = 0;
                    }
                    handled = 1;
                    break;
                }
            }
        }
        
        // Process I-format instructions(both types of i commands)
        if(!handled){
            for(int i = 0; i < sizeof(i_instructions) / sizeof(i_format); i++){
                if(strcmp(instr_name, i_instructions[i].name) == 0){
                    if(rs2_or_offset == NULL){
                        fprintf(stderr, "Error in line %d\nMissing argument(s)\n", line_num);
                        sen = 0;
                        handled = 1;
                        break;
                    }
                    int check = i_cmds_1(instr_name, rd_or_rs2, rs1_or_offset, imm_buffer, optr, line_num);
                    if(!check){
                        sen = 0;
                    }
                    handled = 1;
                    break;
                }
            }
        }
        if(!handled){
            for(int i = 0; i < sizeof(i_part_2) / sizeof(i_format); i++){
                if(strcmp(instr_name, i_part_2[i].name) == 0){
                    int check = i_cmds_2(instr_name, rd_or_rs2, rs1_or_offset, imm_buffer, optr, current_address);
                    if(!check){
                        sen = 0;
                    }
                    handled = 1;
                    break;
                    
                }
            }
        }
        
        // Process S-format instructions if it is of the form instr name rs2 offset(rs1)
        if(!handled){
            for(int i = 0; i < sizeof(s_instructions) / sizeof(s_format); i++){
                if(strcmp(instr_name, s_instructions[i].name) == 0){
                    int check = s_cmds(instr_name, rd_or_rs2, rs1_or_offset, imm_buffer, optr, current_address);
                    if(!check){
                        sen = 0;
                    }
                    handled = 1;
                    break;
                    
                }
            }
        }

        // Process B-format instructions
        if(!handled){
            for (int i = 0; i < sizeof(b_instructions) / sizeof(b_format); i++) {
                if (strcmp(instr_name, b_instructions[i].name) == 0) {
                    int offset_val = 0;
                    int label_found = 0;
                    if(rs2_or_offset == NULL){
                        fprintf(stderr, "Error in line %d\nMissing argument(s)\n", line_num);
                        handled = 1;
                        sen = 0;
                        break;
                    }
                    for(int j = 0; j < label_count; j++){
                        if (strcmp(rs2_or_offset, labels[j].name) == 0) {
                            offset_val = labels[j].address - current_address;
                            label_found = 1;
                            break;
                        }
                    }

                    if(!label_found){
                        fprintf(stderr, "Error in line %d: Label '%s' not found.\n", line_num, rs2_or_offset);
                        handled = 1;
                        sen = 0;
                        break;
                    }

                    char offset_str[12];
                    sprintf(offset_str, "%d", offset_val);
                    int check = b_cmds(instr_name, rd_or_rs2, rs1_or_offset, offset_str, optr, line_num);
                    if(!check){
                        sen = 0;
                    }
                    handled = 1;
                    break;
                    
                }
            }
        }
        // If no instruction was handled, report an error
        if(!handled){
            fprintf(stderr, "Error in line %d\nUnknown instruction: %s\n", line_num, instr_name);
            break;
        }
        current_address += 4; // Increment address by 4 bytes for each instruction
    }
}