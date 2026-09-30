#ifndef HL7_PARSER_H
#define HL7_PARSER_H

#include <Arduino.h>

#define MAX_PARAM 50
#define MAX_STR 128  // Increased from 64
#define BUFFER_SIZE 2048

typedef struct {
    char name[MAX_STR];
    char value[MAX_STR];
} Parameter;

typedef struct {
    char alat[MAX_STR];
    char waktu[MAX_STR];
    char id[MAX_STR];
} Metadata;

// hl7_parser.h
//bool split_string(char *str, const char *delimiter, char fields[][MAX_STR], int *field_count);
bool parse_hl7_message(const char *message, int len, Metadata *meta, Parameter *params, int *param_count);

#endif