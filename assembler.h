#include <stdio.h>

#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#define MAX_LABEL_LEN 10
#define MAX_LINE_LEN 100
#define NUM_REGS 32
#define TEXT_START 0x0
#define DATA_START 0x10000
#define STACK_START 0x50000
#define MAX_LINES 64

typedef struct{
    char name[MAX_LABEL_LEN];
    int address;
} label_info;

extern label_info labels[MAX_LINES];

int r_cmds(char* inst_name, char* rd, char* rs1, char* rs2, FILE* optr, int line_num);
int i_cmds_1(char* instr_name, char* rd, char* rs1, char* imm, FILE* optr, int line_num);
int i_cmds_2(char* inst_name, char* rs2, char* rs1_or_offset, char* optional_rs1, FILE* optr, int line_num);
int s_cmds(char* inst_name, char* rs2, char* rs1_or_offset, char* optional_rs1, FILE* optr, int line_num);
int b_cmds(char* inst_name, char* rs1, char* rs2, char* offset, FILE* optr, int line_num);
int j_cmds(char* inst_name, char* rd, char* offset_str, FILE* optr, int line_num);
int u_cmds(char* rd, char* imm, char* opcode, FILE* optr, int line_num);
int parse_labels(FILE* fptr);
void process_instructions(FILE* fptr, FILE* optr);

#endif